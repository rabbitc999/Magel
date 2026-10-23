/**
 * @file native_input.cpp
 * @brief 宿主标准输入与逐行读取。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief `CHALI_NO_STDIN_INPUT=1` 时不做标准输入兜底。
 *
 * 门禁（`check.sh` 等）靠它保证确定性：样例缺输入时必须照旧报"输入不够"，
 * 而不是停下来等标准输入。
 * @return 关掉兜底返回 true。
 */
bool stdinInputDisabled()
{
    const char *flag = std::getenv("CHALI_NO_STDIN_INPUT");
    return flag != nullptr && flag[0] != '\0' && flag[0] != '0';
}

/**
 * 取下一项输入：先用 `--input "a b c"` 给的项，用完了就**从标准输入读一项**。
 *
 * 于是调试时可以直接敲，也可以 `echo "abc 3" | chali run x.chl` 喂进去；两项之间按空白
 * 切分，跟 `--input` 一个口径。读到 EOF 返回 false，调用方照旧报"输入不够"。
 *
 * @param ctx 原生调用上下文。
 * @param out 输出：读到的这一项。
 * @return 取到返回 true。
 */
/**
 * @brief 从当前平台输入源取一个字节。
 * @return 字节或 EOF。
 */
int nativeInputByte()
{
    return std::getchar();
}

/**
 * @brief 从输入流提取下一个文本字段。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool nextInputText(NativeContext &ctx, Memory::String *out)
{
    if (ctx.input == nullptr || ctx.inputIndex == nullptr)
    {
        return false;
    }
    if (*ctx.inputIndex < ctx.input->size())
    {
        *out = (*ctx.input)[(*ctx.inputIndex)++];
        return true;
    }
    if (stdinInputDisabled())
    {
        return false;
    }
    int character = 0;
    while ((character = nativeInputByte()) != EOF && std::isspace(static_cast<unsigned char>(character)))
    {
    }
    if (character == EOF)
    {
        return false;
    }
    Memory::String item;
    while (character != EOF && !std::isspace(static_cast<unsigned char>(character)))
    {
        item += static_cast<char>(character);
        character = nativeInputByte();
    }
    if (item.empty())
    {
        return false;
    }
    *out = item;
    ++*ctx.inputIndex; /* 与 --input 共用同一个序号 */
    return true;
}

/**
 * @brief 读取原生输入所需的文本数据。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot readText(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    if (ctx.input == nullptr || ctx.inputIndex == nullptr)
    {
        ctx.error = "这个程序没有接输入，取不到字符串";
        return Slot::fromInt(0);
    }
    Memory::String text;
    if (!nextInputText(ctx, &text))
    {
        ctx.error = "输入不够（第 " + Memory::toString(*ctx.inputIndex + 1u) + " 项缺失）";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, text)));
}

/**
 * @brief 从宿主输入读取完整行，保留空格；显式输入项各视作一行。
 * @param ctx 调用上下文。
 * @param line 输出行，不含 LF/CRLF。
 * @return 是否读到一行，EOF 返回 false。
 */
bool nextInputLine(NativeContext &ctx, Memory::String &line)
{
    line.clear();
    if (ctx.input && ctx.inputIndex && *ctx.inputIndex < ctx.input->size())
    {
        line = (*ctx.input)[(*ctx.inputIndex)++];
        return true;
    }
    if (stdinInputDisabled()) return false;
    bool any = false, terminated = false;
    for (int byte = nativeInputByte(); byte != EOF; byte = nativeInputByte())
    {
        any = true;
        if (byte == '\n')
        {
            terminated = true;
            break;
        }
        if (byte == 0)
        {
            ctx.error = "readLine 不接受零字节，String 不能包含内嵌零字节";
            return false;
        }
        line += static_cast<char>(byte);
    }
    if (terminated && !line.empty() && line.back() == '\r') line.pop_back();
    if (any && ctx.inputIndex) ++*ctx.inputIndex;
    return any;
}
/**
 * @brief 读取一行标准输入，EOF 返回空串。
 * @param ctx 调用上下文。
 * @return 新字符串槽。
 */
Slot inputReadLine(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    Memory::String line;
    nextInputLine(ctx, line);
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, line)));
}
/**
 * @brief 最多读取指定行数，以 LF 连接，EOF 提前结束。
 * @param ctx 上下文。
 * @param args 非负行数。
 * @return 新字符串槽；负数或零字节报告运行错误。
 */
Slot inputReadLines(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const auto count = nativeInt(args[0]);
    if (count < 0)
    {
        ctx.error = "readLines 的行数不能为负数";
        return Slot::fromInt(0);
    }
    Memory::String text, line;
    for (std::int64_t i = 0; i < count; ++i)
    {
        if (!nextInputLine(ctx, line)) break;
        if (i) text += '\n';
        text += line;
    }
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, text)));
}

/**
 * @brief 标准输入是不是交互式终端——决定 `__std_pause` 要不要真的停下来等。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool standardInputIsInteractive()
{
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

/**
 * @brief `__std_pause() -> 0`：调试用"临时停一下"，等到一行输入再继续（`std::lang::pause()` 降到它）。
 *
 * **只在交互式终端里真的停**：标准输入不是终端（重定向、管道、CI、嵌入式宿主）时立刻返回，
 * 所以回归样例和自动化跑批不会被它挂住。停下时提示走 stderr，不污染程序的 stdout；
 * 读到的内容直接丢弃（它是"按回车继续"，不是数据输入——要数据请用 `std::lang::inputLine`）。
 * @retval 封装为原生值的运行时结果。
 */
Slot pauseForDebug(NativeContext &, const Memory::Vector<Slot> &)
{
    if (standardInputIsInteractive())
    {
        std::fflush(stdout);
        std::fputs("stackvm：按回车继续…", stderr);
        std::fflush(stderr);
        int character = 0;
        while ((character = nativeInputByte()) != '\n' && character != EOF) {}
    }
    return Slot::fromInt(0);
}

/**
 * @brief 取得下一段可供类型转换的输入文本。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param name 名称文本。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool takeInput(NativeContext &ctx, const char *name, Memory::String *value)
{
    if (ctx.input == nullptr || ctx.inputIndex == nullptr)
    {
        ctx.error = Memory::String("这个程序没有接输入，无法调用 std::lang::") + name;
        return false;
    }
    if (!nextInputText(ctx, value))
    {
        ctx.error = "输入不够（第 " + Memory::toString(*ctx.inputIndex + 1u) + " 项缺失）";
        return false;
    }
    return true;
}

/**
 * @brief 读取输入并转换为有符号整数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputInt(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    Memory::String text;
    if (!takeInput(ctx, "inputInt", &text)) return Slot::fromInt(0);
    std::int64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
    {
        ctx.error = "std::lang::inputInt 读到的不是合法 int：\"" + text + "\"";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(value);
}

/**
 * @brief 读取输入并转换为无符号整数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputUInt(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    Memory::String text;
    if (!takeInput(ctx, "inputUInt", &text)) return Slot::fromInt(0);
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
    {
        ctx.error = "std::lang::inputUInt 读到的不是合法 uint：\"" + text + "\"";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<std::int64_t>(value));
}

/**
 * @brief 读取输入并转换为浮点数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputFloat(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    Memory::String text;
    if (!takeInput(ctx, "inputFloat", &text)) return Slot::fromDouble(0.0);
    char *end = nullptr;
    errno = 0;
    const double value = std::strtod(text.c_str(), &end);
    if (errno == ERANGE || end != text.c_str() + text.size() || !std::isfinite(value))
    {
        ctx.error = "std::lang::inputFloat 读到的不是有限浮点数：\"" + text + "\"";
        return Slot::fromDouble(0.0);
    }
    return Slot::fromDouble(value);
}

/**
 * @brief 读取输入并转换为布尔值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputBool(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    Memory::String text;
    if (!takeInput(ctx, "inputBool", &text)) return Slot::fromInt(0);
    if (text == "true") return Slot::fromInt(1);
    if (text == "false") return Slot::fromInt(0);
    ctx.error = "std::lang::inputBool 只接受 true 或 false，实际为：\"" + text + "\"";
    return Slot::fromInt(0);
}

/**
 * @brief 从输入中读取一个字符。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputChar(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    Memory::String text;
    if (!takeInput(ctx, "inputChar", &text)) return Slot::fromInt(0);
    return Slot::fromInt(text.empty() ? 0 : static_cast<unsigned char>(text.front()));
}

/* ---------------------------------------------------------------------------
 *  运行期对象：不透明宿主记录与三个容器（vec / set / map）
 * ---------------------------------------------------------------------------
 *
 * 这些东西的"值"**不是**内存地址，而是**对象表里的句柄**：表在 C++ 侧，一个
 * `std::shared_ptr<RuntimeObject>` 装下一种对象。宿主语言的类型只决定"句柄怎么用"：
 *
 *   * `vec<T>`   —— 有序表（`std::vector`）：按下标增删查改；
 *   * `set<T>`   —— 有序集合（`std::map` 的键）：元素不重复、按升序遍历；
 *   * `map<K,V>` —— 映射（`std::map`）；
 *   * `obj` —— **装箱格子**（一个值 + 它的类型号）：`set` 装进去、`get` 取出来。
 *
 * 容器里的字符串按**副本**存（`Memory::String`），取出来时在堆上复制一份新的 —— 这样
 * "变量出了作用域被释放"不会让容器里的元素悬空，容器也就活得比某根字符串久。
 *
 * 句柄取一个**很高的区间**（`kObjectHandleBase` 起），和编码堆地址、数据区地址
 * 分开：`(free, …)` / `(drop, …)` 一眼就能分出"这是对象句柄还是内存地址"。
 */
}  // namespace runtimeDetail
}  // namespace stackvm
