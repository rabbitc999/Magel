/**
 * @file native_format.cpp
 * @brief 原生参数读取与格式化输出。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 取一个实参的整数值（整数槽直接取，浮点槽取整）。
 * @param slot 待转换或传递的虚拟机值槽。
 * @retval 取一个实参的整数值（整数槽直接取，浮点槽取整）。
 */
std::int64_t nativeInt(const Slot &slot)
{
    if (!slot.isFloat) return slot.integer;
    // 浮点槽转整数是未定义行为；越界时按方向取饱和值，不让宿主拿到无效值。
    TypeInfo type;
    type.bits = 64;
    if (floatFitsInteger(slot.real, type)) return static_cast<std::int64_t>(slot.real);
    if (!std::isfinite(slot.real)) return 0;
    return slot.real > 0.0 ? std::numeric_limits<std::int64_t>::max() : std::numeric_limits<std::int64_t>::min();
}

/**
 * @brief 按格式串把实参拼成文本。
 *
 * 这是 `printf` 与 `format` **共用的一份实现**（语言侧不写格式化逻辑）。占位符：
 *
 * | 写法 | 说明 |
 * | --- | --- |
 * | `%d` / `%i` | 有符号整数 |
 * | `%u` | 无符号整数 | `%x` / `%X` | 十六进制（小写 / 大写） |
 * | `%f` | 浮点（**默认 3 位小数**，`.N` 给定就按 N） |
 * | `%s` | 字符串（`.N` = 最多取 N 个字符） |
 * | `%c` | 一个字符（整数按低 8 位当字符码） |
 * | `%%` | 一个百分号 |
 *
 * 每个占位符前面可以写旗标 / 宽度 / 精度：`%-8d`（左对齐）、`%08.2f`（补零）、
 * `%.3s`（截断）。数字部分直接交给 `snprintf`，所以语义就是 C 的那一套。
 *
 * @param ctx 环境。
 * @param format 格式串（已经过语言的转义处理）。
 * @param args 实参（下标 0 起）。
 * @param argCount 实参个数。
 * @param out 拼出来的文本。
 * @return 成功返回 true；`ctx.error` 里是出错原因。
 */
bool nativeFormatInto(NativeContext &ctx, const Memory::String &format, const Memory::Vector<Slot> &args,
                      std::size_t argCount, Memory::String *out)
{
    out->clear();
    /* `args` 里**头一个**是格式串自己（原生函数的第一个实参），真正的实参从
     * `args.size() - argCount` 开始 —— 之前从这里写错成 0，`%s` 就把格式串自己当成了
     * 第一个实参（打出来的东西自然全是错位的）。 */
    std::size_t next = args.size() - argCount;
    for (std::size_t index = 0; index < format.size(); ++index)
    {
        if (format[index] != '%')
        {
            out->push_back(format[index]);
            continue;
        }
        if (index + 1u >= format.size())
        {
            ctx.error = "格式串以 % 结尾（要打百分号请写 `%%`）";
            return false;
        }
        const char spec = format[++index];
        if (spec == '%')
        {
            out->push_back('%');
            continue;
        }

        /* 1. 收集旗标 / 宽度 / 精度（与 C 的 printf 同形）。 */
        Memory::String flags;
        Memory::String width;
        Memory::String precision;
        for (; index < format.size(); ++index)
        {
            const char ch = format[index];
            if (ch == '-' || ch == '+' || ch == ' ' || ch == '#')
                flags.push_back(ch);
            else if (ch == '0' && width.empty())
                width.push_back(ch);
            else
                break;
        }
        for (; index < format.size() && std::isdigit(static_cast<unsigned char>(format[index]));
             ++index)
        {
            width.push_back(format[index]);
        }
        if (index < format.size() && format[index] == '.')
        {
            precision.push_back('.');
            ++index;
            while (index < format.size() && std::isdigit(static_cast<unsigned char>(format[index])))
            {
                precision.push_back(format[index]);
                ++index;
            }
        }
        if (index >= format.size())
        {
            ctx.error = "格式串里 `%` 后面缺转换字符（`%d` / `%f` / `%s` 这种）";
            return false;
        }
        const char conversion = format[index];
        const bool isString = (conversion == 's');
        const bool isChar = (conversion == 'c');
        if (next >= args.size()) /* 真正的实参在 `args` 里只到末尾 */
        {
            ctx.error = Memory::String("格式串里的占位符比实参多（%") + conversion + " 没东西可填)";
            return false;
        }
        const Slot &argument = args[next++];

        /* 2. `%s` / `%c` 不走 snprintf：字符串要先把地址读成文本。 */
        if (isString)
        {
            Memory::String text;
            if (argument.isText)
                text = argument.text;
            else if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(argument)), &text))
            {
                ctx.error = "格式化 %s 时读不到字符串（地址 " + Memory::toString(nativeInt(argument)) + " 不在任何一块已分配内存里）";
                return false;
            }
            /* 精度：最多取前 N 个字符（C 的 `%.3s` 就是这个意思）。 */
            if (precision.size() > 1u)
            {
                const std::size_t limit = static_cast<std::size_t>(
                    std::strtoull(precision.c_str() + 1, nullptr, 10));
                if (text.size() > limit) text.resize(limit);
            }
            const std::size_t pad = (width.empty() || text.size() >= std::strtoull(width.c_str(), nullptr, 10)) ? 0u : std::strtoull(width.c_str(), nullptr, 10) - text.size();
            if (pad != 0u && flags.find('-') == Memory::String::npos) out->append(pad, ' ');
            out->append(text);
            if (pad != 0u && flags.find('-') != Memory::String::npos) out->append(pad, ' ');
            continue;
        }
        if (isChar)
        {
            const char ch = static_cast<char>(nativeInt(argument) & 0xFF);
            out->push_back(ch);
            continue;
        }

        /* `%b` 不是 C++17 snprintf 的可移植转换，按 uint64 原始位模式输出。 */
        if (conversion == 'b')
        {
            std::uint64_t bits = static_cast<std::uint64_t>(nativeInt(argument));
            Memory::String digits;
            do
            {
                digits.insert(digits.begin(), (bits & 1u) != 0u ? '1' : '0');
                bits >>= 1u;
            } while (bits != 0u);
            if (precision.size() > 1u)
            {
                const std::size_t minimum = std::min<std::size_t>(
                    std::strtoull(precision.c_str() + 1, nullptr, 10), 10000u);
                if (digits.size() < minimum) digits.insert(0u, minimum - digits.size(), '0');
            }
            const Memory::String prefix = flags.find('#') != Memory::String::npos ? "0b" : "";
            const std::size_t requested = std::min<std::size_t>(
                width.empty() ? 0u : std::strtoull(width.c_str(), nullptr, 10), 10000u);
            const std::size_t pad = requested > prefix.size() + digits.size() ? requested - prefix.size() - digits.size() : 0u;
            if (flags.find('-') != Memory::String::npos)
            {
                *out += prefix + digits;
                out->append(pad, ' ');
            }
            else if (width.find('0') == 0u)
            {
                *out += prefix;
                out->append(pad, '0');
                *out += digits;
            }
            else
            {
                out->append(pad, ' ');
                *out += prefix + digits;
            }
            continue;
        }

        /* 3. 其余数字：旗标 / 宽度 / 精度交给 snprintf。 */
        const bool isFloatConversion =
            (conversion == 'f' || conversion == 'F' || conversion == 'e' || conversion == 'E' || conversion == 'g' || conversion == 'G');
        Memory::String spec2 = "%";
        spec2 += flags;
        spec2 += width;
        if (!precision.empty())
            spec2 += precision;
        else if (isFloatConversion)
            spec2 += ".3"; /* 语言的默认：三位小数 */
        /* 整数一律按 64 位传（`long long` / `unsigned long long`）：`%d` 只吃 32 位，
         * 不补 `ll` 的话 `%d` 拿到 2^32 以上的值就会被截成低 32 位。 */
        if (!isFloatConversion) spec2 += "ll";
        spec2.push_back(conversion);

        char buffer[512];
        int written = 0;
        if (isFloatConversion)
        {
            const double value = argument.isFloat ? argument.real : static_cast<double>(argument.integer);
            written = std::snprintf(buffer, sizeof(buffer), spec2.c_str(), value);
        }
        else if (conversion == 'x' || conversion == 'X' || conversion == 'u')
        {
            written = std::snprintf(buffer, sizeof(buffer), spec2.c_str(),
                                    static_cast<unsigned long long>(nativeInt(argument)));
        }
        else
        {
            written = std::snprintf(buffer, sizeof(buffer), spec2.c_str(),
                                    static_cast<long long>(nativeInt(argument)));
        }
        if (written < 0)
        {
            ctx.error = "格式化失败（转换 `%" + Memory::String(1, conversion) + "`）";
            return false;
        }
        out->append(buffer, std::min<std::size_t>(static_cast<std::size_t>(written),
                                                  sizeof(buffer) - 1u));
    }
    if (next != args.size())
    {
        ctx.error = "格式化时实参比格式串里的占位符多（多给了 " + Memory::toString(args.size() - next) + " 个）";
        return false;
    }
    return true;
}
}  // namespace runtimeDetail
}  // namespace stackvm
