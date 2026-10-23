/**
 * @file native_closure.cpp
 * @brief 闭包环境、对象复制与资源释放。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 解析闭包或旧式无环境函数地址。
 * @param objects 对象表。
 * @param pointer 函数值。
 * @param environment 返回需要隐式传入的环境句柄。
 * @return 标签地址，非法闭包返回零。
 */
std::int64_t closureTarget(ObjectStore &objects, std::int64_t pointer, std::int64_t &environment)
{
    environment = 0;
    if (pointer < kObjectHandleBase) return pointer;
    const auto object = objects.find(pointer);
    if (!object || object->kind != ObjectKind::Closure) return 0;
    if (object->closureEnvironment) environment = pointer;
    return object->closureTarget;
}

/**
 * @brief 建立函数值；捕获环境由后续操作逐项登记借用地址。
 */
Slot closureNew(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto value = Memory::makeShared<RuntimeObject>();
    value->kind = ObjectKind::Closure;
    value->closureTarget = nativeInt(args[0]);
    value->closureEnvironment = nativeInt(args[1]) != 0;
    return Slot::fromInt(ctx.objects->add(std::move(value)));
}

/**
 * @brief 登记已校验的捕获项；借用捕获以非拥有整数地址保存。
 */
Slot closureCapture(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto target = nativeObject(ctx, args[0], ObjectKind::Closure, "lambda 捕获");
    if (!target) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), true, &value))
        return Slot::fromInt(0);
    target->items.push_back(std::move(value));
    return args[0];
}

/**
 * @brief 登记独立的值捕获。
 * @param ctx 原生上下文。
 * @param args 环境、值与存储布局。
 * @return 环境句柄。
 */
Slot closureCaptureCopy(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const Slot result = closureCapture(ctx, args);
    auto target = ctx.objects->find(nativeInt(args[0]));
    if (target)
    {
        target->closureCopy = true;
        target->iteratorContext = ctx;
    }
    return result;
}

/**
 * @brief 转移源存储到闭包，清空源槽而不复制资源。
 * @param ctx 上下文。
 * @param args 环境、地址、布局、浮点标志和字节数。
 * @return 环境句柄。
 */
Slot closureCaptureMove(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto object = nativeObject(ctx, args[0], ObjectKind::Closure, "lambda move 捕获");
    if (!object) return Slot::fromInt(0);
    const auto bytes = static_cast<std::size_t>(nativeInt(args[5]));
    RuntimeObject::ElementView view;
    view.isText = nativeInt(args[2]) != 0;
    view.objectFlag = nativeInt(args[3]);
    view.isFloat = nativeInt(args[4]) != 0;
    view.address = nativeAllocate(ctx, std::max<std::size_t>(8, bytes));
    if (!view.address)
    {
        ctx.error = "lambda move 捕获内存不足";
        return Slot::fromInt(0);
    }
    Memory::Vector<char> data(std::max<std::size_t>(8, bytes), 0);
    if (!nativeRead(ctx, static_cast<std::size_t>(nativeInt(args[1])), data.data(), bytes) || !nativeWrite(ctx, view.address, data.data(), data.size()))
    {
        ctx.heap->release(view.address);
        return Slot::fromInt(0);
    }
    std::fill(data.begin(), data.end(), 0);
    if (!nativeWrite(ctx, static_cast<std::size_t>(nativeInt(args[1])), data.data(), bytes))
    {
        ctx.heap->release(view.address);
        return Slot::fromInt(0);
    }
    const auto index = object->items.size();
    object->items.emplace_back();
    object->elementViews[index] = view;
    object->closureCopy = true;
    object->iteratorContext = ctx;
    return args[0];
}

/**
 * @brief 登记聚合捕获的用户析构入口。
 * @param ctx 上下文。
 * @param args 环境、索引和函数地址。
 * @return 环境句柄。
 */
Slot closureDestructor(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto object = nativeObject(ctx, args[0], ObjectKind::Closure, "lambda 捕获析构");
    if (!object) return Slot::fromInt(0);
    object->captureDestructors[static_cast<std::size_t>(nativeInt(args[1]))] = nativeInt(args[2]);
    return args[0];
}

/**
 * @brief 取得 copy 捕获的可写稳定单元。
 * @param ctx 原生上下文。
 * @param args 环境、索引及布局。
 * @return 捕获副本的地址。
 */
Slot closureRef(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto object = nativeObject(ctx, args[0], ObjectKind::Closure, "lambda copy 环境");
    if (!object) return Slot::fromInt(0);
    const auto raw = nativeInt(args[1]);
    if (raw < 0 || static_cast<std::size_t>(raw) >= object->items.size())
    {
        ctx.error = "lambda 捕获索引越界";
        return Slot::fromInt(0);
    }
    const auto index = static_cast<std::size_t>(raw);
    const auto found = object->elementViews.find(index);
    if (found != object->elementViews.end()) return Slot::fromInt(found->second.address);
    RuntimeObject::ElementView view;
    view.isText = nativeInt(args[2]) != 0;
    view.objectFlag = nativeInt(args[3]);
    view.isFloat = nativeInt(args[4]) != 0;
    view.address = nativeAllocate(ctx, std::max<std::size_t>(8u,
                                                             object->items[index].isStruct ? object->items[index].fields.size() * 8u : 8u));
    if (!view.address)
    {
        ctx.error = "lambda copy 捕获内存不足";
        return Slot::fromInt(0);
    }
    if (!nativeWriteView(ctx, view, object->items[index]))
    {
        nativeReleaseViewContents(ctx, view);
        ctx.heap->release(view.address);
        return Slot::fromInt(0);
    }
    object->elementViews[index] = view;
    return Slot::fromInt(view.address);
}

/**
 * @brief 取出捕获项；借用地址由来源所有者管理，不在调用块释放。
 */
Slot closureGet(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto target = nativeObject(ctx, args[0], ObjectKind::Closure, "lambda 环境");
    const auto index = nativeInt(args[1]);
    if (!target) return Slot::fromInt(0);
    if (index < 0 || static_cast<std::size_t>(index) >= target->items.size())
    {
        ctx.error = "lambda 捕获索引越界";
        return Slot::fromInt(0);
    }
    Slot value;
    const auto heapCount = ctx.objects->materializedHeap.size();
    const auto objectCount = ctx.objects->materializedObjects.size();
    if (!nativeSlot(ctx, target->items[static_cast<std::size_t>(index)], &value)) return Slot::fromInt(0);
    /* 调用方按普通参数所有权接管聚合及叶子；不再留在容器取值的停机兜底名单中。 */
    ctx.objects->materializedHeap.resize(heapCount);
    ctx.objects->materializedObjects.resize(objectCount);
    return value;
}

/**
 * @brief 为运行时对象创建独立副本。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objectCopy(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::int64_t handle = nativeInt(args[0]);
    if (ctx.objects == nullptr)
    {
        ctx.error = "不是一个能复制的对象句柄：" + Memory::toString(handle);
        return Slot::fromInt(0);
    }
    Memory::String error;
    RuntimeObject *source = ctx.objects->find(handle);
    if (source != nullptr && !nativeRefreshViews(ctx, *source)) return Slot::fromInt(0);
    const std::int64_t copy = ctx.objects->copyHandle(handle, &error);
    if (copy == 0)
    {
        ctx.error = error;
        return Slot::fromInt(0);
    }
    return Slot::fromInt(copy);
}

/**
 * @brief 静态函数地址无需复制；带捕获函数值深拷贝环境。
 */
Slot functionCopy(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (!isObjectHandle(nativeInt(args[0]))) return args[0];
    const auto value = ctx.objects->find(nativeInt(args[0]));
    if (!value || value->kind != ObjectKind::Closure)
    {
        ctx.error = "函数值已经释放或不是闭包";
        return Slot::fromInt(0);
    }
    return objectCopy(ctx, args);
}

/**
 * @brief `__std_object_free(句柄) -> 0`：把对象还回去（`delete` 用；不是句柄就报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objectFree(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::int64_t handle = nativeInt(args[0]);
    if (ctx.objects == nullptr || !isObjectHandle(handle))
    {
        ctx.error = "不是一个能释放的对象句柄：" + Memory::toString(handle);
        return Slot::fromInt(0);
    }
    if (!ctx.objects->release(handle))
    {
        ctx.error = "这个对象已经释放过了（句柄 " + Memory::toString(handle) + "）";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(0);
}
}  // namespace runtimeDetail
}  // namespace stackvm
