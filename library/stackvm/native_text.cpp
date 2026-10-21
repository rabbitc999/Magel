/**
 * @file native_text.cpp
 * @brief 字节文本操作与内存复制服务。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief `__std_str_len(地址) -> 长度`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strLen(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::size_t address = static_cast<std::size_t>(nativeInt(args[0]));
    std::int64_t length = 0;
    if (ctx.heap != nullptr && ctx.heap->contains(address) && ctx.heap->readInt(address, length) && length >= 0)
        return Slot::fromInt(length);
    Memory::String text;
    if (!nativeReadCString(ctx, address, &text))
    {
        ctx.error = "取字符串长度时读不到它（地址 " + Memory::toString(nativeInt(args[0])) + " 不在任何一块已分配内存里，或者没有结尾的 \\0）";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<std::int64_t>(text.size()));
}

/**
 * `__std_str_sub(串, 起, 止) -> 新串地址`：半开区间 `[起, 止)`，**越界自动夹到范围内**；
 * `止` 传负数表示"到末尾"。
 */
/**
 * @brief 这个字节位置是不是 UTF-8 字符的边界（`String` 的下标与切片都按字节算，但端点落在
 * 多字节字符中间就会切出半个字符——那时报错，而不是静默产出坏字节）。
 * @param text 字符串。
 * @param index 字节位置（等于长度也算合法边界）。
 * @return 是字符边界（或正好在末尾）返回 true。
 */
bool utf8Boundary(const Memory::String &text, std::size_t index)
{
    if (index >= text.size())
    {
        return true;
    }
    return (static_cast<unsigned char>(text[index]) & 0xc0u) != 0x80u; /* 10xxxxxx 是续字节 */
}

/**
 * @brief `__std_str_fmt(格式串, 实参…) -> 新串地址`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strFormat(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (args.empty())
    {
        ctx.error = "format 至少要有格式串";
        return Slot::fromInt(0);
    }
    Memory::String format;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &format))
    {
        ctx.error = "format 的格式串读不到（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Slot::fromInt(0);
    }
    Memory::String text;
    if (!nativeFormatInto(ctx, format, args, args.size() - 1u, &text)) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, text)));
}

/**
 * @brief 输出一段文本：挂了下沉就先实时推给宿主，`keepText=false` 时不再进文本桶。
 * @param ctx 环境。
 * @param bytes 字节。
 * @param count 字节数。
 */
void nativeEmitText(NativeContext &ctx, const char *bytes, std::size_t count)
{
    if (bytes == nullptr || count == 0u) return;
    if (ctx.textSink != nullptr) ctx.textSink(ctx.textSinkUser, bytes, count);
    if (ctx.text != nullptr && (ctx.textKeep == nullptr || *ctx.textKeep))
        ctx.text->append(bytes, count);
}

/**
 * @brief `__std_print(格式串, 实参…) -> 0`：把格式化结果追加到文本输出桶（`printf` 用它）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strPrint(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (args.empty())
    {
        ctx.error = "printf 至少要有格式串";
        return Slot::fromInt(0);
    }
    Memory::String format;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &format))
    {
        ctx.error = "printf 的格式串读不到（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Slot::fromInt(0);
    }
    Memory::String text;
    if (!nativeFormatInto(ctx, format, args, args.size() - 1u, &text)) return Slot::fromInt(0);
    nativeEmitText(ctx, text.data(), text.size());
    return Slot::fromInt(0);
}

/**
 * @brief 输出一个字符。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot putChar(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const char byte = static_cast<char>(nativeInt(args[0]) & 0xff);
    nativeEmitText(ctx, &byte, 1u);
    return Slot::fromInt(0);
}

/**
 * @brief `__std_str_set(串, 下标, 字符码) -> 0`：往字符串的第 `下标` 个字节写一个字符。
 *
 * 字符串在堆上是**可写**的（字面量当值用时已经复制成自己的那块），越界就报错 ——
 * 和数组下标一个脾气。写进去的是低 8 位。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strSet(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::size_t address = static_cast<std::size_t>(nativeInt(args[0]));
    Memory::String text;
    if (!nativeReadCString(ctx, address, &text))
    {
        ctx.error = "改字符时读不到字符串（地址 " + Memory::toString(address) + "）";
        return Slot::fromInt(0);
    }
    const std::int64_t index = nativeInt(args[1]);
    if (index < 0 || static_cast<std::size_t>(index) >= text.size())
    {
        ctx.error = "字符串下标越界：" + Memory::toString(index) + "（长度 " + Memory::toString(text.size()) + "）";
        return Slot::fromInt(0);
    }
    const char code = static_cast<char>(nativeInt(args[2]) & 0xFF);
    if (!nativeWrite(ctx, address + 8u + static_cast<std::size_t>(index), &code, 1u))
    {
        ctx.error = "往字符串里写字符失败（地址 " + Memory::toString(address) + "）";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(0);
}

/**
 * @brief `__std_str_at(串, 下标) -> 字符码 / -1`：按下标取一个字符。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strAt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "取字符时读不到字符串（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Slot::fromInt(-1);
    }
    /* 长度是显式元数据，下标必须严格落在 [0, length) 内。 */
    const std::int64_t index = nativeInt(args[1]);
    if (index < 0 || static_cast<std::size_t>(index) >= text.size())
    {
        ctx.error = "字符串下标越界：" + Memory::toString(index) + "（长度 " + Memory::toString(text.size()) + "）";
        return Slot::fromInt(0);
    }
    if (!utf8Boundary(text, static_cast<std::size_t>(index)))
    {
        ctx.error = "字符串下标落在多字节字符中间（String 按字节算）";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<unsigned char>(text[static_cast<std::size_t>(index)]));
}

/**
 * @brief 验证下标后取得字符串字符地址，范围与普通字符串下标一致。
 * @param ctx 提供堆块和数据区地址范围的运行环境。
 * @param args 字符串地址和按 UTF-8 字节计数的下标。
 * @return 对应字节地址；读取失败或越界时返回零并登记运行时错误。
 * @note 堆串跳过长度头；数据区字面量直接从字节起始地址取值。
 */
Slot strRef(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    const std::int64_t address = nativeInt(args[0]);
    const std::int64_t index = nativeInt(args[1]);
    if (!nativeReadCString(ctx, static_cast<std::size_t>(address), &text))
    {
        ctx.error = "取字符引用时读不到字符串";
        return Slot::fromInt(0);
    }
    if (index < 0 || static_cast<std::size_t>(index) >= text.size())
    {
        ctx.error = "字符串下标越界：" + Memory::toString(index) + "（长度 " + Memory::toString(text.size()) + "）";
        return Slot::fromInt(0);
    }
    const std::int64_t prefix = ctx.heap != nullptr && ctx.heap->contains(static_cast<std::size_t>(address)) ? 8 : 0;
    return Slot::fromInt(address + prefix + index);
}

/**
 * @brief `__std_memcpy(目标, 源, 字节数) -> 0`：逐字节搬一段（C++ 里就是 `memcpy`）。
 *
 * 源与目标整段必须**各自落在一块已分配内存里**（堆块 / 数据区符号），否则报错 ——
 * 以前语言侧是"在目标地址上读 8 字节 → 只改最低那个字节 → 写回"，那么读堆串最后一
 * 个字节时那 8 字节会越过块尾（现在字符串在堆上，块是紧的）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot memCopy(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::size_t target = static_cast<std::size_t>(nativeInt(args[0]));
    const std::size_t source = static_cast<std::size_t>(nativeInt(args[1]));
    const std::int64_t count = nativeInt(args[2]);
    if (count < 0)
    {
        ctx.error = "memcpy 的字节数不能是负数：" + Memory::toString(count);
        return Slot::fromInt(0);
    }
    if (count == 0) return Slot::fromInt(0);
    Memory::String buffer(static_cast<std::size_t>(count), '\0');
    if (!nativeRead(ctx, source, &buffer[0], static_cast<std::size_t>(count)))
    {
        ctx.error = "memcpy 读不到源地址 " + Memory::toString(source) + " 处的 " + Memory::toString(count) + " 个字节（不在任何一块已分配内存里）";
        return Slot::fromInt(0);
    }
    if (!nativeWrite(ctx, target, buffer.data(), static_cast<std::size_t>(count)))
    {
        ctx.error = "memcpy 写不进目标地址 " + Memory::toString(target) + " 处的 " + Memory::toString(count) + " 个字节（不在任何一块已分配内存里）";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(0);
}

/**
 * @brief `__std_memset(地址, 字节值, 字节数) -> 0`：把一段内存填成同一个字节。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot memSet(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::size_t address = static_cast<std::size_t>(nativeInt(args[0]));
    const char value = static_cast<char>(nativeInt(args[1]) & 0xFF);
    const std::int64_t count = nativeInt(args[2]);
    if (count < 0)
    {
        ctx.error = "memset 的字节数不能是负数：" + Memory::toString(count);
        return Slot::fromInt(0);
    }
    if (count == 0) return Slot::fromInt(0);
    const Memory::String buffer(static_cast<std::size_t>(count), value);
    if (!nativeWrite(ctx, address, buffer.data(), static_cast<std::size_t>(count)))
    {
        ctx.error = "memset 写不进地址 " + Memory::toString(address) + " 处的 " + Memory::toString(count) + " 个字节（不在任何一块已分配内存里）";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(0);
}

/**
 * `__std_read_text() -> 堆上的新串地址`：把**下一项输入原文**读成一个字符串。
 *
 * `input_number(x)` 走的是 `(read, _, _, x)`（按变量类型解析一个数），而字符串没法
 * "解析"成一个数，所以单独给一个原生函数：输入项本身就是文本，直接复制到堆上。
 * 注意取完就往前走一项，两种读法**共用**同一个输入序号。
 */
}  // namespace runtimeDetail
}  // namespace stackvm
