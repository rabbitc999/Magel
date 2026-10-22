#include "constant_expression.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace chali_syntax
{
/**
 * @brief 编译期常量表达式解析器；不执行语言运行时表达式。
 */
struct ConstantParser
{
    const Memory::String &source;                               ///< 条件文本。
    const Memory::Map<Memory::String, Memory::String> &values;  ///< 本次调用的文本绑定。
    Memory::String &error;                                      ///< 编译期诊断。
    std::size_t at = 0;                                         ///< 当前字符。
    unsigned depth = 0;                                         ///< 绑定求值深度。
    /**
     * @brief 读取一个常量原子。
     * @param enabled 是否实际求值。
     * @return 常量。
     */
    CompileTimeConstant atom(bool enabled)
    {
        spaces(source, at);
        if (at == source.size())
        {
            error = "常量表达式缺少操作数";
            return {};
        }
        if (source[at] == '(')
        {
            ++at;
            auto value = expression(1, enabled);
            spaces(source, at);
            if (at == source.size() || source[at++] != ')') error = "常量表达式缺少 `)`";
            return value;
        }
        if (source[at] == '!' || source[at] == '-' || source[at] == '+' || source[at] == '~')
        {
            if (source[at] == '-' && at + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[at + 1])))
            {
                char *end = nullptr;
                errno = 0;
                const bool hex = source.compare(at + 1, 2, "0x") == 0 || source.compare(at + 1, 2, "0X") == 0;
                const auto number = std::strtoll(source.c_str() + at, &end, hex ? 16 : 10);
                if (errno == ERANGE && enabled) error = "编译期整数超出 int64 范围";
                at = static_cast<std::size_t>(end - source.c_str());
                return { number, {}, false };
            }
            const auto op = source[at++];
            auto value = atom(enabled);
            if (!enabled) return {};
            if (op == '!') return { value.truth() ? 0 : 1, {}, false };
            if (value.string)
            {
                error = "常量表达式的一元算术需要整数";
                return {};
            }
            if (op == '-') value.number = static_cast<long long>(0ULL - static_cast<unsigned long long>(value.number));
            if (op == '~') value.number = ~value.number;
            return value;
        }
        if (source[at] == '"' || source[at] == '\'')
        {
            const auto quote = source[at++];
            Memory::String text;
            while (at < source.size() && source[at] != quote)
            {
                auto ch = source[at++];
                if (ch == '\\' && at < source.size())
                {
                    ch = source[at++];
                    if (ch == 'n')
                        ch = '\n';
                    else if (ch == 't')
                        ch = '\t';
                }
                text += ch;
            }
            if (at == source.size())
                error = "常量表达式字符串缺少引号";
            else
                ++at;
            return { 0, text, true };
        }
        if (std::isdigit(static_cast<unsigned char>(source[at])))
        {
            char *end = nullptr;
            errno = 0;
            const bool hex = source.compare(at, 2, "0x") == 0 || source.compare(at, 2, "0X") == 0;
            const auto value = std::strtoll(source.c_str() + at, &end, hex ? 16 : 10);
            if (errno == ERANGE && enabled) error = "编译期整数超出 int64 范围";
            if (end == source.c_str() + at)
            {
                error = "常量表达式整数不合法";
                return {};
            }
            at = static_cast<std::size_t>(end - source.c_str());
            return { value, {}, false };
        }
        const auto start = at;
        while (at < source.size() && identPart(source[at])) ++at;
        while (source.compare(at, 2, "::") == 0)
        {
            at += 2;
            while (at < source.size() && identPart(source[at])) ++at;
        }
        if (at == start)
        {
            error = "常量表达式只能使用常量、形参和局部绑定";
            return {};
        }
        const auto name = source.substr(start, at - start);
        if (name == "true" || name == "false") return { name == "true" ? 1 : 0, {}, false };
        if (!enabled) return {};
        const auto found = values.find(name);
        if (found == values.end())
        {
            error = "常量表达式不是编译期常量：" + name;
            return {};
        }
        if (depth >= 64)
        {
            error = "常量绑定求值超过 64 层";
            return {};
        }
        const auto &text = found->second;
        ConstantParser nested{ text, values, error, 0, depth + 1 };
        return nested.run();
    }
    /**
     * @brief 读取运算符优先级。
     * @param op 输出运算符。
     * @return 优先级，零表示结束。
     */
    int operation(Memory::String &op)
    {
        spaces(source, at);
        for (const auto item : { "||", "&&", "==", "!=", "<=", ">=", "<<", ">>", "|", "^", "&", "<", ">", "+", "-", "*", "/", "%" })
        {
            const Memory::String token = item;
            if (source.compare(at, token.size(), token)) continue;
            op = token;
            if (op == "||") return 1;
            if (op == "&&") return 2;
            if (op == "|") return 3;
            if (op == "^") return 4;
            if (op == "&") return 5;
            if (op == "==" || op == "!=") return 6;
            if (op == "<" || op == ">" || op == "<=" || op == ">=") return 7;
            if (op == "<<" || op == ">>") return 8;
            if (op == "+" || op == "-") return 9;
            return 10;
        }
        return 0;
    }
    /**
     * @brief 按优先级求值，逻辑运算保持短路。
     * @param minimum 最低优先级。
     * @param enabled 是否求值。
     * @return 结果。
     */
    CompileTimeConstant expression(int minimum, bool enabled)
    {
        auto left = atom(enabled);
        while (error.empty())
        {
            Memory::String op;
            const int priority = operation(op);
            if (priority < minimum) break;
            at += op.size();
            const bool rightEnabled = enabled && !(op == "&&" && !left.truth()) && !(op == "||" && left.truth());
            const auto right = expression(priority + 1, rightEnabled);
            if (!enabled) continue;
            const auto a = left.number, b = right.number;
            if (op == "&&")
                left = { left.truth() && right.truth(), {}, false };
            else if (op == "||")
                left = { left.truth() || right.truth(), {}, false };
            else if (left.string || right.string)
            {
                if (left.string != right.string || (op != "==" && op != "!="))
                {
                    error = "常量表达式字符串只支持同类型的 == / !=";
                    break;
                }
                left = { op == "==" ? left.text == right.text : left.text != right.text, {}, false };
            }
            else if (op == "==")
                left.number = a == b;
            else if (op == "!=")
                left.number = a != b;
            else if (op == "<")
                left.number = a < b;
            else if (op == ">")
                left.number = a > b;
            else if (op == "<=")
                left.number = a <= b;
            else if (op == ">=")
                left.number = a >= b;
            else if (op == "+")
                left.number = static_cast<long long>(static_cast<unsigned long long>(a) + static_cast<unsigned long long>(b));
            else if (op == "-")
                left.number = static_cast<long long>(static_cast<unsigned long long>(a) - static_cast<unsigned long long>(b));
            else if (op == "*")
                left.number = static_cast<long long>(static_cast<unsigned long long>(a) * static_cast<unsigned long long>(b));
            else if (op == "/" || op == "%")
            {
                if (!b)
                {
                    error = "常量表达式除数不能为零";
                    break;
                }
                if (a == (-9223372036854775807LL - 1) && b == -1)
                    left.number = op == "/" ? a : 0;
                else
                    left.number = op == "/" ? a / b : a % b;
            }
            else if (op == "&")
                left.number = a & b;
            else if (op == "|")
                left.number = a | b;
            else if (op == "^")
                left.number = a ^ b;
            else
            {
                if (b < 0 || b >= 64)
                {
                    error = "常量表达式移位次数必须在 0..63";
                    break;
                }
                left.number = op == "<<" ? static_cast<long long>(static_cast<unsigned long long>(a) << b) : a >> b;
            }
        }
        return left;
    }
    /**
     * @brief 求值整个条件并检查多余文本。
     * @return 编译期值。
     */
    CompileTimeConstant run()
    {
        auto value = expression(1, true);
        spaces(source, at);
        if (at != source.size() && error.empty()) error = "常量表达式中有多余内容：" + source.substr(at);
        return value;
    }
};

/**
 * @brief 求值const 泛型使用的编译期常量表达式。
 * @param source 需要完整消费的常量表达式。
 * @param values 当前可用的文本常量绑定。
 * @param error 失败时接收解析或运算诊断。
 * @return 表达式的整数或字符串常量；失败时由 error 指示原因。
 */
CompileTimeConstant evaluateConstant(const Memory::String &source, const Memory::Map<Memory::String, Memory::String> &values, Memory::String &error)
{
    ConstantParser evaluator{ source, values, error };
    return evaluator.run();
}

}  // namespace chali_syntax
