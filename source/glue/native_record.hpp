#ifndef MAGEL_GLUE_NATIVE_RECORD_HPP
#define MAGEL_GLUE_NATIVE_RECORD_HPP

#include "stackvm.hpp"

/**
 * @brief 基于公开运行时接口访问不透明宿主记录的胶水辅助层。
 */
namespace native_record_glue
{
using Value = stackvm::NativeValue;

/**
 * @brief 在一次胶水操作内累积诊断，绑定入口统一交回运行时。
 */
struct State
{
    stackvm::NativeCallContext &host;  ///< 仅在本次调用内有效的宿主上下文。
    Memory::String error;              ///< 当前操作的原始诊断。
};

/**
 * @brief 按通用记录协议复制对象，供 opcode 的 copy 方法绑定。
 * @param state 当前胶水调用状态。
 * @param arguments 包含待复制句柄。
 * @return 独立副本句柄。
 */
inline Value copy(State &state, const Memory::Vector<Value> &arguments)
{
    return Value::fromInt(state.host.cloneNativeRecord(arguments[0].asInt()));
}

/**
 * @brief 幂等释放通用记录，供 opcode 的 drop 方法绑定。
 * @param state 当前胶水调用状态。
 * @param arguments 包含待释放句柄，零或已释放句柄不重复销毁。
 * @return 零占位值。
 */
inline Value drop(State &state, const Memory::Vector<Value> &arguments)
{
    state.host.releaseNativeRecord(arguments[0].asInt());
    return Value::fromInt(0);
}

/**
 * @brief 为 String 的 new 绑定创建独立空值。
 * @param state 当前运行时调用状态。
 * @param arguments 无参数创建调用的参数列表。
 * @return 新分配的空字符串地址。
 */
inline Value stringNew(State &state, const Memory::Vector<Value> &arguments)
{
    (void)arguments;
    return Value::fromInt(state.host.makeString({}));
}

/**
 * @brief 为 String 的 copy 绑定按长度复制文本。
 * @param state 当前运行时调用状态。
 * @param arguments 包含被复制的 String 值。
 * @return 与源值独立的字符串地址。
 */
inline Value stringCopy(State &state, const Memory::Vector<Value> &arguments)
{
    Memory::String text;
    if (!state.host.readString(arguments[0].asInt(), text))
    {
        state.error = "String copy 读取源值失败";
        return Value::fromInt(0);
    }
    return Value::fromInt(state.host.makeString(text));
}

/**
 * @brief 为 String 的 drop 绑定释放自己拥有的原始字节块。
 * @param state 当前运行时调用状态。
 * @param arguments 包含待释放的 String 值。
 * @return 零占位值。
 */
inline Value stringDrop(State &state, const Memory::Vector<Value> &arguments)
{
    state.host.releaseMemory(arguments[0].asInt(), true);
    return Value::fromInt(0);
}

/**
 * @brief 读取原生参数的整数位模式。
 * @param value 原生参数。
 * @return 整数值或浮点向零截断结果。
 */
inline std::int64_t nativeInt(const Value &value) { return value.asInt(); }

/**
 * @brief 从当前运行时按其文本协议读取字符串。
 * @param state 胶水状态。
 * @param address 文本地址。
 * @param out 接收读取结果。
 * @return 读取成功时为 true。
 */
inline bool nativeReadCString(State &state, std::size_t address, Memory::String *out)
{
    return state.host.readString(static_cast<std::int64_t>(address), *out);
}

/**
 * @brief 创建交给语言调用方拥有的字符串副本。
 * @param state 胶水状态。
 * @param text 要复制的文本。
 * @return 当前运行时中的文本句柄。
 */
inline std::int64_t nativeAllocString(State &state, const Memory::String &text)
{
    return state.host.makeString(text);
}

/**
 * @brief 校验记录类型并借用宿主记录。
 * @tparam T 胶水自己的记录布局。
 * @param state 胶水状态。
 * @param handle 原生参数中的对象句柄。
 * @param type 预期宿主描述符。
 * @param who 诊断中的操作名称。
 * @return 有效记录，失败返回空并保存诊断。
 */
template <class T>
T *record(State &state, const Value &handle, const stackvm::NativeRecordType &type, const char *who)
{
    auto *result = static_cast<T *>(state.host.nativeRecord(handle.asInt(), type));
    if (!result)
        state.error = Memory::String(who) + " 用的 " + type.name + " 已经释放过了（句柄 " + Memory::toString(handle.asInt()) + "）";
    return result;
}

/**
 * @brief 从 opcode 接收者或语言引用槽取得宿主记录句柄。
 * @param context 当前胶水调用上下文。
 * @param reference 直接传递的 opcode 接收者，或保存该值的语言引用槽地址。
 * @param type 目标记录的生命周期描述符。
 * @return 记录句柄；引用不可读时报告错误并返回零。
 * @note opcode 方法的 self 使用直接句柄，普通字段引用使用内存槽；两者都保留前端的可写性检查。
 */
inline std::int64_t referenceHandle(stackvm::NativeCallContext &context, std::int64_t reference,
                                    const stackvm::NativeRecordType &type)
{
    if (context.nativeRecord(reference, type)) return reference;
    char bytes[8];
    if (!context.readBytes(reference, bytes, sizeof(bytes)))
    {
        context.fail("opcode 引用槽不可读");
        return 0;
    }
    std::uint64_t handle = 0;
    for (unsigned index = 0; index < 8; ++index)
        handle |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[index])) << (index * 8);
    return static_cast<std::int64_t>(handle);
}

/**
 * @brief 注册只通过公开上下文访问宿主记录的原生操作。
 * @param registry 目标胶水注册表。
 * @param name 语言侧绑定名称。
 * @param arity 实参数量。
 * @param operation 操作实现。
 * @param error 注册失败的诊断输出。
 * @param referenceFirst 首个参数是否为保存对象句柄的语言引用槽。
 * @return 注册成功时为 true。
 */
inline bool bind(stackvm::NativeRegistry &registry, const char *name, std::size_t arity,
                 Value (*operation)(State &, const Memory::Vector<Value> &), Memory::String *error, bool referenceFirst = false)
{
    return registry.add(name, arity, [operation, referenceFirst](stackvm::NativeCallContext &context, const Memory::Vector<Value> &arguments) {
        State state{ context, {} };
        Memory::Vector<Value> values;
        const auto *input = &arguments;
        if (referenceFirst)
        {
            values = arguments;
            input = &values;
            char bytes[8];
            if (values.empty() || !context.readBytes(values[0].asInt(), bytes, sizeof(bytes)))
            {
                context.fail("胶水对象引用指向的句柄槽无效");
                return Value::fromInt(0);
            }
            std::uint64_t handle = 0;
            for (unsigned index = 0; index < 8; ++index)
                handle |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[index])) << (index * 8);
            values[0] = Value::fromInt(static_cast<std::int64_t>(handle));
        }
        auto result = operation(state, *input);
        if (!state.error.empty()) context.fail(state.error);
        return result; }, error);
}

/**
 * @brief 整数 → 十进制文本（`int` 是 64 位，所以按 `long long` 打）。
 * @param value 值。
 * @return 文本。
 */
inline Memory::String nativeIntText(std::int64_t value) { return Memory::toString(value); }

/**
 * @brief 浮点 → 文本：默认保留 3 位小数（语言的默认），去掉尾随的 0。
 *
 * 与 `%f` 的默认精度保持一致：`3.5` 打成 `3.5`（不是 `3.500`），`2.0` 打成 `2.0`。
 * @param value 值。
 * @param precision 小数位数。
 * @return 文本。
 */
inline Memory::String nativeFloatText(double value, int precision)
{
    if (precision < 0) precision = 3;
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    Memory::String text = buffer;
    if (precision > 0 && text.find('.') != Memory::String::npos)
    {
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
    }
    return text;
}

/**
 * @brief 这个字节位置是不是 UTF-8 字符的边界（`String` 的下标与切片都按字节算，但端点落在
 * 多字节字符中间就会切出半个字符——那时报错，而不是静默产出坏字节）。
 * @param text 字符串。
 * @param index 字节位置（等于长度也算合法边界）。
 * @return 是字符边界（或正好在末尾）返回 true。
 */
inline bool utf8Boundary(const Memory::String &text, std::size_t index)
{
    if (index >= text.size())
    {
        return true;
    }
    return (static_cast<unsigned char>(text[index]) & 0xc0u) != 0x80u; /* 10xxxxxx 是续字节 */
}

/**
 * @brief `__std_str_lit(地址) -> 堆上的新串地址`：把只读的字面量字节复制成一份**可写**的堆串。
 *
 * 字符串的"值"永远是堆地址（`\0` 结尾的字节串），数据区里只留只读的字面量字节当
 * 复制源 —— 于是每个 `string` 变量手里那块是自己能写的，`s[i] = 字符` 不会改到
 * 共享的字面量；所有权也就统一了：**变量即那块堆串的主人**。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value strLit(State &ctx, const Memory::Vector<Value> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "复制字符串字面量时读不到它（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Value::fromInt(0);
    }
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, text)));
}

/**
 * @brief `__std_str_join(串…) -> 新串地址`：把**一串**字符串接成一个（变参）。
 *
 * 之所以一次接一整条链（而不是两个两个接）：`a + b + c` 那样两两接会留下一个中间串，
 * 而中间串没有哪个变量"拥有"它 —— 没人释放（漏）。摊平成一次调用就只分配一个结果，
 * 它正好归被赋值的那个变量所有。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value strJoin(State &ctx, const Memory::Vector<Value> &args)
{
    Memory::String text;
    for (const Value &argument : args)
    {
        Memory::String part;
        if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(argument)), &part))
        {
            ctx.error = "拼接字符串时读不到操作数（地址 " + Memory::toString(nativeInt(argument)) + "）";
            return Value::fromInt(0);
        }
        text += part;
    }
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, text)));
}

/**
 * @brief 按字节区间截取字符串，并检查 UTF-8 端点边界。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value strSub(State &ctx, const Memory::Vector<Value> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "切片时读不到字符串（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Value::fromInt(0);
    }
    const std::int64_t length = static_cast<std::int64_t>(text.size());
    std::int64_t begin = nativeInt(args[1]);
    std::int64_t end = nativeInt(args[2]);
    if (end < 0) end = length;
    if (begin < 0) begin = 0;
    if (begin > length) begin = length;
    if (end < begin) end = begin;
    if (end > length) end = length;
    /* 端点落在多字节字符中间就报错：切出来的半个字符既不是合法 UTF-8，也没人会想用。 */
    if (!utf8Boundary(text, static_cast<std::size_t>(begin)) || !utf8Boundary(text, static_cast<std::size_t>(end)))
    {
        ctx.error = "切片端点落在多字节字符中间（String 的切片按字节算）";
        return Value::fromInt(0);
    }
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(
        ctx, text.substr(static_cast<std::size_t>(begin),
                         static_cast<std::size_t>(end - begin)))));
}

/**
 * @brief `__std_str_of_int(值) -> 新串地址`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value strOfInt(State &ctx, const Memory::Vector<Value> &args)
{
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, nativeIntText(nativeInt(args[0])))));
}

/**
 * @brief `__std_str_of_flt(值) -> 新串地址`（默认 3 位小数，`精度` 可覆盖）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value strOfFlt(State &ctx, const Memory::Vector<Value> &args)
{
    const double value = args[0].asFloat();
    const int precision = (args.size() > 1u) ? static_cast<int>(nativeInt(args[1])) : 3;
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, nativeFloatText(value, precision))));
}

/**
 * @brief `__std_str_cmp(左, 右) -> -1 / 0 / 1`（字典序；空指针当空串）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value strCmp(State &ctx, const Memory::Vector<Value> &args)
{
    Memory::String left, right;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &left) || !nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &right))
    {
        ctx.error = "比较字符串时读不到操作数（地址 " + Memory::toString(nativeInt(args[0])) + " / " + Memory::toString(nativeInt(args[1])) + "）";
        return Value::fromInt(0);
    }
    const int order = left.compare(right);
    return Value::fromInt(order < 0 ? -1 : (order > 0 ? 1 : 0));
}

/**
 * @brief `__std_str_pos(串, 子串) -> 位置 / -1`：子串第一次出现的位置。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 子串首次出现的字节位置，未找到时为 -1。
 */
inline Value strPos(State &ctx, const Memory::Vector<Value> &args)
{
    Memory::String text, needle;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text) || !nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &needle))
    {
        ctx.error = "查找子串时读不到操作数（地址 " + Memory::toString(nativeInt(args[0])) + " / " + Memory::toString(nativeInt(args[1])) + "）";
        return Value::fromInt(-1);
    }
    const std::size_t found = text.find(needle);
    return Value::fromInt(found == Memory::String::npos ? -1 : static_cast<std::int64_t>(found));
}

/**
 * @brief 注册 String opcode 的创建、复制和销毁协议。
 * @param registry 目标原生函数表。
 * @param error 注册失败的原因。
 * @return 三个生命周期函数均绑定成功时返回 true。
 */
inline bool bindString(stackvm::NativeRegistry &registry, Memory::String *error = nullptr)
{
    return native_record_glue::bind(registry, "__std_str_lit", 1, strLit, error) && native_record_glue::bind(registry, "__std_str_join", static_cast<std::size_t>(-1), strJoin, error) && native_record_glue::bind(registry, "__std_str_sub", 3, strSub, error) && native_record_glue::bind(registry, "__std_str_of_int", 1, strOfInt, error) && native_record_glue::bind(registry, "__std_str_of_flt", 2, strOfFlt, error) && native_record_glue::bind(registry, "__std_str_cmp", 2, strCmp, error) && native_record_glue::bind(registry, "__std_str_pos", 2, strPos, error) && native_record_glue::bind(registry, "std::lang::__stringNew", 0, stringNew, error) && native_record_glue::bind(registry, "std::lang::__stringCopy", 1, stringCopy, error) && native_record_glue::bind(registry, "std::lang::__stringDrop", 1, stringDrop, error);
}
}  // namespace native_record_glue

#endif
