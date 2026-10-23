/**
 * @file native_memory.cpp
 * @brief 字节堆、字符访问、迭代与生存期迁移。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 造一个对象并返回句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param kind 待处理对象的类别。
 * @retval 封装为原生值的运行时结果。
 */
Slot nativeNewObject(NativeContext &ctx, ObjectKind kind)
{
    if (ctx.objects == nullptr)
    {
        ctx.error = Memory::String("没有接上对象表，建不了 ") + objectKindName(kind);
        return Slot::fromInt(0);
    }
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = kind;
    return Slot::fromInt(ctx.objects->add(std::move(object)));
}

/* ---- 容器（vec / set / map）---- */

/**
 * @brief 按一个字节读取 char 引用，适用于字符串字节及八字节标量槽。
 */
Slot charLoad(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    unsigned char byte = 0;
    if (!nativeRead(ctx, static_cast<std::size_t>(nativeInt(args[0])), reinterpret_cast<char *>(&byte), 1u))
        ctx.error = "字符引用读取地址无效";
    return Slot::fromInt(byte);
}
/**
 * @brief 按一个字节写入 char 引用，避免覆盖相邻字符。
 */
Slot charStore(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const unsigned char byte = static_cast<unsigned char>(nativeInt(args[1]));
    if (!nativeWrite(ctx, static_cast<std::size_t>(nativeInt(args[0])), reinterpret_cast<const char *>(&byte), 1u))
        ctx.error = "字符引用写入地址无效";
    return Slot::fromInt(0);
}

/**
 * @brief 复制结构体状态到拥有型箱，接口借用共享稳定单元，值复制另建状态。
 * @param ctx 运行上下文。
 * @param args 状态地址、叶子布局、引用类型号和类型名。
 * @return 新状态箱句柄；失败记录运行错误。
 */
Slot iterBox(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    ObjectValue value;
    if (!nativeValue(ctx, args[0], false, nativeInt(args[1]), true, &value)) return Slot::fromInt(0);
    auto box = Memory::makeShared<RuntimeObject>();
    box->kind = ObjectKind::Obj;
    box->iteratorStateBox = true;
    box->iteratorLayout = nativeInt(args[1]);
    box->objTag = nativeInt(args[2]);
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[3])), &box->objTypeName))
    {
        ctx.objects->releaseValue(value);
        ctx.error = "迭代器状态类型名无效";
        return Slot::fromInt(0);
    }
    box->items.push_back(std::move(value));
    box->iteratorContext.heap = ctx.heap;
    box->iteratorContext.data = ctx.data;
    box->iteratorContext.objects = ctx.objects;
    return Slot::fromInt(ctx.objects->add(std::move(box)));
}

/**
 * @brief 分配内存后复位地址复用的生命周期标记。
 * @param ctx 环境。
 * @param bytes 字节数。
 * @return 分配地址。
 */
std::size_t nativeAllocate(NativeContext &ctx, std::size_t bytes)
{
    const auto address = ctx.heap->allocate(bytes);
    if (!address) Memory::restartSignal().recordFailure(Memory::WatchdogReason::PoolExhausted, bytes,
                                                        ctx.heap->liveBytes(), ctx.heap->liveBlocks(), 0u);
    if (address && ctx.objects) ctx.objects->beginStorage(address, bytes, true);
    return address;
}

/**
 * @brief 查询逻辑分配是否有效，供编译器清理守卫避免读取失效载荷。
 * @param ctx 当前堆。
 * @param args 块首地址。
 * @return 活跃分配返回一。
 */
Slot heapLive(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    std::size_t capacity = 0;
    return Slot::fromInt(ctx.heap->capacityAt(static_cast<std::size_t>(nativeInt(args[0])), capacity));
}

/**
 * @brief 显式删除前严格检查块首，阻止对失效身份再次析构。
 * @param ctx 当前堆。
 * @param args 块首。
 * @return 活跃时为一；失效时记录诊断并返回零。
 */
Slot heapDropLive(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const auto address = static_cast<std::size_t>(nativeInt(args[0]));
    std::size_t capacity = 0;
    if (ctx.heap->capacityAt(address, capacity)) return Slot::fromInt(1);
    ctx.error = "panic：[CHL1205] drop 的来源已经释放、为空或不是有效块首";
    return Slot::fromInt(0);
}
/**
 * @brief 查询、结束或重新开始 move 管理的存储生命周期。
 * @param ctx 本次运行的对象表；标记不跨虚拟机实例共享。
 * @param args 存储地址、字节数和子对象类型号（开始生命周期只需前两项）。
 * @return 查询返回存活标志；重复移动报告运行错误。
 */
template <int Operation>
Slot moveLifetime(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const auto address = static_cast<std::size_t>(nativeInt(args[0]));
    const auto bytes = static_cast<std::size_t>(nativeInt(args[1]));
    auto &ranges = ctx.objects->movedStorage;
    const auto type = Operation == 3 ? 0 : nativeInt(args[2]);
    const auto key = std::make_pair(address, type);
    std::size_t capacity = 0;
    const bool allocationLive = !ctx.heap->contains(address) || ctx.heap->capacityAt(address, capacity);
    const bool live = !bytes || (allocationLive && ranges.find(key) == ranges.end());
    if (Operation == 0) return Slot::fromInt(live);
    if (Operation == 1 && !live) ctx.error = "move 的源对象已经销毁，必须先重新赋值";
    if (Operation == 2) ranges[key] = bytes;
    if (Operation == 3) ctx.objects->beginStorage(address, bytes);
    return Slot::fromInt(0);
}
template Slot moveLifetime<0>(NativeContext &, const Memory::Vector<Slot> &);

template Slot moveLifetime<1>(NativeContext &, const Memory::Vector<Slot> &);

template Slot moveLifetime<2>(NativeContext &, const Memory::Vector<Slot> &);

template Slot moveLifetime<3>(NativeContext &, const Memory::Vector<Slot> &);

}  // namespace runtimeDetail
}  // namespace stackvm
