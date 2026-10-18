// ---------------------------------------------------------------------------
//  执行
// ---------------------------------------------------------------------------

namespace
{

/**
 * @brief 读一个字面量。
 * @param token 原文。
 * @param isNumber 输出：是不是数字。
 * @param isFloat 输出：是不是浮点。
 * @param intValue 输出：整数值。
 * @param doubleValue 输出：浮点值。
 * @param text 输出：字符串内容。
 * @return 是字面量返回 true。
 */
bool readLiteral(const Memory::String &token, bool &isNumber, bool &isFloat,
                 std::int64_t &intValue, double &doubleValue, Memory::String &text)
{
    if (token.empty() || token == "_") return false;
    if (token.front() == '"' || token.front() == '\'')
    {
        isNumber = false;
        text = unquote(token);
        return true;
    }
    return parseNumber(token, isFloat, intValue, doubleValue) ? (isNumber = true, true) : false;
}

/**
 * @brief 一条四元式读到的变量（用于死代码分析）。
 * @param quad 四元式。
 * @return 变量名列表（不含字面量）。
 */
Memory::Vector<Memory::String> readOperands(const Quad &quad)
{
    Memory::String typeName;
    const Memory::String base = splitType(quad.op, typeName);
    Memory::Vector<Memory::String> names;

    /**
     * 收一个可能读到的操作数。
     * @param token 原文。
     */
    auto take = [&](const Memory::String &token) {
        if (token.empty()) return;
        bool isNumber = false, isFloat = false;
        std::int64_t intValue = 0;
        double doubleValue = 0.0;
        Memory::String text;
        if (readLiteral(token, isNumber, isFloat, intValue, doubleValue, text)) return;
        names.push_back(token);
    };

    if (base == "decl" || base == "label" || base == "call" || base == "j" || base == "halt" || base == "addr" || base == "print") return names;
    if (base == ":=" || base == "neg" || base == "~" || base == "cvt" || base == "write" || base == "putc" || base == "param" || base == "ret" || base == "jz" || base == "jnz")
    {
        take(quad.arg(0));
        return names;
    }
    if (base == "mul" || base == "div")
    {
        take(quad.arg(0));
        take(quad.arg(1));
        return names;
    }
    // 二元运算与比较：前两个操作数。
    take(quad.arg(0));
    take(quad.arg(1));
    return names;
}

/**
 * @brief 这条四元式会不会写变量（写的是第几个参数）。
 * @param base 基础操作符。
 * @return 写入的参数下标；不写返回 -1。
 */
int writesTo(const Memory::String &base)
{
    if (base == ":=" || base == "neg" || base == "~" || base == "cvt" || base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=" || base == "mul" || base == "div" || base == "read" || base == "getc")
    {
        return 2;
    }
    if (base == "+" || base == "-" || base == "*" || base == "/" || base == "%" || base == "&" || base == "|" || base == "^" || base == "<<" || base == ">>")
    {
        return 2;
    }
    return -1;
}

/**
 * @brief 返回真正读取值的操作数下标，元数据及标签不参与传播。
 * @param base 基础操作码。
 * @return 读操作数下标。
 */
std::array<std::size_t, 2> optimizationReads(const Memory::String &base)
{
    if (base == "decl" || base == "label" || base == "call" || base == "j" || base == "halt" || base == "addr" || base == "print") return { static_cast<std::size_t>(-1), static_cast<std::size_t>(-1) };
    if (base == ":=" || base == "cvt" || base == "neg" || base == "~" || base == "write" || base == "putc" || base == "param" || base == "ret" || base == "jz" || base == "jnz" || base == "free" || base == "drop" || base == "alloc" || base == "load" || base == "callptr") return { 0, static_cast<std::size_t>(-1) };
    if (writesTo(base) >= 0 && base != "read" && base != "getc") return { 0, 1 };
    if (base == "store" || base == "check" || base == "checkptr") return { 0, 1 };
    return { static_cast<std::size_t>(-1), static_cast<std::size_t>(-1) };
}
/**
 * @brief 识别可消除复制的无声明编译器临时量。
 * @param name 名字。
 * @return 是否 t 加数字。
 */
bool optimizationTemporary(const Memory::String &name)
{
    if (name.size() < 2 || name[0] != 't') return false;
    for (std::size_t i = 1; i < name.size(); ++i)
        if (name[i] < '0' || name[i] > '9') return false;
    return true;
}
/**
 * @brief 基本块内传播同类型复制与消除同类型转换，不跨过调用或控制流边界。
 * @param code 四元式。
 * @param declared 声明类型。
 * @param report 优化统计。
 * @return 是否改变。
 */
bool optimizeLocalCopies(Memory::Vector<Quad> &code, const Memory::Map<Memory::String, TypeInfo> &declared,
                         OptimizationReport &report)
{
    bool changed = false;
    Memory::Map<Memory::String, Memory::String> copies;
    Memory::Map<Memory::String, TypeInfo> values;
    const auto same = [](const TypeInfo &a, const TypeInfo &b) {
        return a.bits == b.bits && a.isFloat == b.isFloat && a.isUnsigned == b.isUnsigned;
    };
    const auto lookup = [&](const Memory::String &name, TypeInfo &out) {
        const auto known = values.find(name);
        if (known != values.end())
        {
            out = known->second;
            return true;
        }
        const auto type = declared.find(name);
        if (type != declared.end())
        {
            out = type->second;
            return true;
        }
        return false;
    };
    for (auto &quad : code)
    {
        Memory::String suffix;
        auto base = splitType(quad.op, suffix);
        for (const auto index : optimizationReads(base))
        {
            if (index >= quad.args.size()) continue;
            if (base == "write" && suffix.empty()) continue;
            const auto found = copies.find(quad.arg(index));
            if (found != copies.end())
            {
                quad.args[index] = found->second;
                ++report.copiesPropagated;
                changed = true;
            }
        }
        TypeInfo source;
        const bool known = lookup(quad.arg(0), source);
        if (base == "cvt" && known && same(source, typeOf(suffix)))
        {
            quad.op = ":=";
            base = ":=";
            ++report.conversionsRemoved;
            changed = true;
        }
        const int written = writesTo(base);
        if (written >= 0)
        {
            const auto destination = quad.arg(static_cast<std::size_t>(written));
            for (auto it = copies.begin(); it != copies.end();)
            {
                if (it->first == destination || it->second == destination)
                    it = copies.erase(it);
                else
                    ++it;
            }
            values.erase(destination);
            TypeInfo result;
            bool resultKnown = false;
            if (base == ":=" && known)
            {
                result = source;
                resultKnown = true;
            }
            else if (base == "cvt" || base == "+" || base == "-" || base == "*" || base == "/" || base == "%" || base == "neg")
            {
                result = typeOf(suffix);
                resultKnown = base == "cvt" || base == "neg";
                if (!resultKnown)
                {
                    TypeInfo right;
                    if (known && lookup(quad.arg(1), right))
                    {
                        result.isFloat = source.isFloat || right.isFloat;
                        resultKnown = true;
                    }
                    else if (known && !source.isFloat && !result.isFloat)
                    {
                        bool number = false, floating = false;
                        std::int64_t integer = 0;
                        double real = 0;
                        Memory::String text;
                        resultKnown = readLiteral(quad.arg(1), number, floating, integer, real, text) && number && !floating;
                    }
                }
            }
            else if (base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=")
            {
                result = typeOf("i64");
                resultKnown = true;
            }
            const auto target = declared.find(destination);
            if (target != declared.end())
            {
                result = target->second;
                resultKnown = true;
            }
            if (resultKnown) values[destination] = result;
            // 原复制保留到死临时量分析，避免提前删除可能报告未定义源的读取。
            if (base == ":=" && known && resultKnown && same(source, result) && destination != quad.arg(0))
                copies[destination] = quad.arg(0);
        }
        const bool pure = written >= 0 && base != "read" && base != "getc" && base != "mul" && base != "div";
        if (!pure && base != "param" && base != "write" && base != "putc")
        {
            copies.clear();
            values.clear();
        }
    }
    Memory::Map<Memory::String, std::size_t> reads;
    for (const auto &quad : code)
        for (const auto &name : readOperands(quad)) ++reads[name];
    std::size_t output = 0;
    for (std::size_t i = 0; i < code.size(); ++i)
    {
        Memory::String suffix;
        const auto base = splitType(code[i].op, suffix);
        const auto temporary = code[i].arg(2);
        const bool pure = base == "+" || base == "-" || base == "*" || base == "/" || base == "%" || base == "cvt" || base == "neg" || base == "~" || base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=";
        if (i + 1 < code.size() && pure && optimizationTemporary(temporary) && !declared.count(temporary) && reads[temporary] == 1 && code[i + 1].op == ":=" && code[i + 1].arg(0) == temporary && !code[i + 1].arg(2).empty())
        {
            code[i].args[2] = code[i + 1].arg(2);
            ++i;
            if (output != i - 1) code[output] = std::move(code[i - 1]);
            ++output;
            ++report.copiesPropagated;
            changed = true;
            continue;
        }
        if (output != i) code[output] = std::move(code[i]);
        ++output;
    }
    code.resize(output);
    return changed;
}

#include "optimization_flow.inc"

}  // namespace

/**
 * @brief 化简四元式（**就地改写** `file.quads`）。
 *
 * 在基本块内传播常量与同类型复制、折叠常量运算、消除同类型转换，
 * 合并只使用一次的临时结果与紧随其后的复制，化简保留类型语义的整数恒等式；
 * 保留动态算术的实际数值类型、窄类型存储和可能失败的读取。
 * 折叠常量条件与跳转链，删除不可达代码和安全的无用临时复制。
 * 控制流分析在合流点与循环回边求整数常量不动点，并消除块内重复表达式。
 * 调用、内存操作及未知副作用构成传播边界，不改变声明元数据及隐式输出格式。
 * @param file 四元式文件。
 * @param maxRounds 最多迭代几轮（缺省 4）。
 * @param collectTrace 是否收集优化说明。
 * @return 化简统计。
 */
OptimizationReport optimize(QuadFile &file, std::size_t maxRounds, bool collectTrace)
{
    OptimizationReport report;

    // 0. 先收集变量类型，折叠时按类型回绕。
    Memory::Map<Memory::String, TypeInfo> types;
    for (const Quad &quad : file.quads)
    {
        Memory::String typeName;
        if (splitType(quad.op, typeName) == "decl") types[quad.arg(2)] = typeOf(quad.arg(0));
    }

    for (std::size_t round = 0; round < maxRounds; ++round)
    {
        ++report.rounds;
        bool changed = optimizeControlFlow(file.quads, types, report);
        Memory::Map<Memory::String, Memory::String> constants;  // 变量 → 已知字面量原文
        const auto remember = [&](const Quad &quad) {
            const auto destination = quad.arg(2);
            auto literal = quad.arg(0);
            const auto declared = types.find(destination);
            bool numeric = false, floating = false;
            std::int64_t integer = 0;
            double real = 0;
            Memory::String text;
            if (declared != types.end() && readLiteral(literal, numeric, floating, integer, real, text) && numeric)
            {
                if (declared->second.isFloat)
                    literal = formatFloatLiteral(normalizeFloat(floating ? real : static_cast<double>(integer), declared->second.bits));
                else if (!floating || (real >= static_cast<double>(INT64_MIN) && real < 9223372036854775808.0))
                    literal = Memory::toString(normalizeInt(floating ? static_cast<std::int64_t>(real) : integer, declared->second));
                else
                {
                    constants.erase(destination);
                    return;
                }
            }
            constants[destination] = literal;
        };
        Memory::Vector<Quad> passOne;
        passOne.reserve(file.quads.size());

        for (Quad quad : file.quads)
        {
            Memory::String typeName;
            const Memory::String base = splitType(quad.op, typeName);
            const TypeInfo type = typeOf(typeName);

            /**
             * 把第 index 个操作数换成已知常量。
             */
            auto substitute = [&](std::size_t index) {
                if (index >= quad.args.size()) return;
                const Memory::String token = quad.args[index];
                if (token.empty() || token == "_") return;
                if (token.front() == '"' || token.front() == '\'') return;
                const auto found = constants.find(token);
                if (found != constants.end()) quad.args[index] = found->second;
            };

            // 1. 标签 / 调用会打断常量传播；写入变量的指令清掉该变量的常量。
            if (base == "label" || base == "call" || base == "callptr" || base == "halt" || base == "ret" || base == "j")
            {
                constants.clear();
                passOne.push_back(std::move(quad));
                continue;
            }
            if (base == "read" || base == "getc")
            {
                constants.erase(quad.arg(2));
                passOne.push_back(std::move(quad));
                continue;
            }
            // 声明的类型和存储类别是元数据，不能把名为 local 的变量常量代入其中。
            // 函数参数恰好位于前一函数清理之后；污染 local 会导致 AOT 漏收参数。
            if (base == "mul" || base == "div")
            {
                // 双结果操作同时覆盖商/余数或低/高位，不能保留任一旧值。
                constants.clear();
                passOne.push_back(std::move(quad));
                continue;
            }
            if (base == "decl")
            {
                constants.erase(quad.arg(2));
                passOne.push_back(std::move(quad));
                continue;
            }

            // 2. 代入已知常量。
            if (base != "write" || !typeName.empty())
                for (const auto index : optimizationReads(base)) substitute(index);

            // 3. 常量折叠：两个操作数（或一元的一个）都是字面量。
            bool folded = false;
            {
                bool leftIsNumber = false, leftIsFloat = false;
                std::int64_t leftInt = 0;
                double leftDouble = 0.0;
                Memory::String leftText;
                bool rightIsNumber = false, rightIsFloat = false;
                std::int64_t rightInt = 0;
                double rightDouble = 0.0;
                Memory::String rightText;

                const bool hasLeft = readLiteral(quad.arg(0), leftIsNumber, leftIsFloat,
                                                 leftInt, leftDouble, leftText);
                const bool hasRight = readLiteral(quad.arg(1), rightIsNumber, rightIsFloat,
                                                  rightInt, rightDouble, rightText);

                static const Memory::Map<Memory::String, char> kArithmetic = {
                    { "+", '+' },
                    { "-", '-' },
                    { "*", '*' },
                    { "/", '/' },
                    { "%", '%' },
                };
                const auto arithmetic = kArithmetic.find(base);
                const bool isCompare = (base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=");
                const bool isBitwise = (base == "&" || base == "|" || base == "^" || base == "<<" || base == ">>");

                if (arithmetic != kArithmetic.end() && hasLeft && hasRight && leftIsNumber && rightIsNumber)
                {
                    const bool useFloat = leftIsFloat || rightIsFloat;
                    const double a = leftIsFloat ? leftDouble : static_cast<double>(leftInt);
                    const double b = rightIsFloat ? rightDouble : static_cast<double>(rightInt);
                    if (useFloat)
                    {
                        if ((arithmetic->second != '/' && arithmetic->second != '%') || b != 0.0)
                        {
                            double value = 0.0;
                            switch (arithmetic->second)
                            {
                            case '+': value = a + b; break;
                            case '-': value = a - b; break;
                            case '*': value = a * b; break;
                            case '/': value = a / b; break;
                            default: value = std::fmod(a, b); break;
                            }
                            value = normalizeFloat(value, type.bits);
                            quad.op = ":=";
                            quad.args = { formatFloatLiteral(value), "_", quad.arg(2) };
                            remember(quad);
                            folded = true;
                        }
                    }
                    else if ((arithmetic->second != '/' && arithmetic->second != '%') || (rightInt != 0 && !(leftInt == INT64_MIN && rightInt == -1)))
                    {
                        std::int64_t value = 0;
                        if (type.isUnsigned && (arithmetic->second == '/' || arithmetic->second == '%'))
                        {
                            const std::uint64_t left = static_cast<std::uint64_t>(leftInt);
                            const std::uint64_t right = static_cast<std::uint64_t>(rightInt);
                            value = static_cast<std::int64_t>(arithmetic->second == '/' ? left / right : left % right);
                        }
                        else
                        {
                            switch (arithmetic->second)
                            {
                            case '+':
                                value = static_cast<std::int64_t>(
                                    static_cast<std::uint64_t>(leftInt) + static_cast<std::uint64_t>(rightInt));
                                break;
                            case '-':
                                value = static_cast<std::int64_t>(
                                    static_cast<std::uint64_t>(leftInt) - static_cast<std::uint64_t>(rightInt));
                                break;
                            case '*':
                                value = static_cast<std::int64_t>(
                                    static_cast<std::uint64_t>(leftInt) * static_cast<std::uint64_t>(rightInt));
                                break;
                            case '/': value = leftInt / rightInt; break;
                            default: value = leftInt % rightInt; break;
                            }
                        }
                        value = normalizeInt(value, type);
                        quad.op = ":=";
                        quad.args = { Memory::toString(value), "_", quad.arg(2) };
                        remember(quad);
                        folded = true;
                    }
                }
                else if (isCompare && hasLeft && hasRight && leftIsNumber && rightIsNumber)
                {
                    const double a = leftIsFloat ? leftDouble : static_cast<double>(leftInt);
                    const double b = rightIsFloat ? rightDouble : static_cast<double>(rightInt);
                    bool value = false;
                    if (type.isUnsigned && !leftIsFloat && !rightIsFloat)
                    {
                        const std::uint64_t ua = static_cast<std::uint64_t>(leftInt);
                        const std::uint64_t ub = static_cast<std::uint64_t>(rightInt);
                        if (base == "==")
                            value = (ua == ub);
                        else if (base == "!=")
                            value = (ua != ub);
                        else if (base == "<")
                            value = (ua < ub);
                        else if (base == "<=")
                            value = (ua <= ub);
                        else if (base == ">")
                            value = (ua > ub);
                        else
                            value = (ua >= ub);
                    }
                    else if (!leftIsFloat && !rightIsFloat)
                    {
                        if (base == "==")
                            value = leftInt == rightInt;
                        else if (base == "!=")
                            value = leftInt != rightInt;
                        else if (base == "<")
                            value = leftInt < rightInt;
                        else if (base == "<=")
                            value = leftInt <= rightInt;
                        else if (base == ">")
                            value = leftInt > rightInt;
                        else
                            value = leftInt >= rightInt;
                    }
                    else
                    {
                        if (base == "==")
                            value = (a == b);
                        else if (base == "!=")
                            value = (a != b);
                        else if (base == "<")
                            value = (a < b);
                        else if (base == "<=")
                            value = (a <= b);
                        else if (base == ">")
                            value = (a > b);
                        else
                            value = (a >= b);
                    }
                    quad.op = ":=";
                    quad.args = { value ? "1" : "0", "_", quad.arg(2) };
                    remember(quad);
                    folded = true;
                }
                else if (isBitwise && hasLeft && hasRight && leftIsNumber && rightIsNumber && !leftIsFloat && !rightIsFloat)
                {
                    std::int64_t value = 0;
                    if (base == "&")
                        value = leftInt & rightInt;
                    else if (base == "|")
                        value = leftInt | rightInt;
                    else if (base == "^")
                        value = leftInt ^ rightInt;
                    else if (base == "<<")
                        value = static_cast<std::int64_t>(static_cast<std::uint64_t>(leftInt) << (rightInt & 63));
                    else
                        value = type.isUnsigned ? static_cast<std::int64_t>(
                                                      static_cast<std::uint64_t>(leftInt) >> (rightInt & 63)) :
                                                  (leftInt >> (rightInt & 63));
                    value = normalizeInt(value, type);
                    quad.op = ":=";
                    quad.args = { Memory::toString(value), "_", quad.arg(2) };
                    remember(quad);
                    folded = true;
                }
                else if (base == "neg" && hasLeft && leftIsNumber && (leftIsFloat || type.isFloat))
                {
                    // 浮点取负：必须按 double 折，`std::atoll("2.5")` 会先截成 2 再取负。
                    quad.op = ":=";
                    quad.args = { formatFloatLiteral(normalizeFloat(-(leftIsFloat ? leftDouble : static_cast<double>(leftInt)), type.bits)),
                                  "_", quad.arg(2) };
                    remember(quad);
                    folded = true;
                }
                else if ((base == "neg" || base == "~") && hasLeft && leftIsNumber && !leftIsFloat)
                {
                    std::int64_t value = (base == "neg") ? static_cast<std::int64_t>(std::uint64_t(0) - static_cast<std::uint64_t>(leftInt)) : ~leftInt;
                    value = normalizeInt(value, type);
                    quad.op = ":=";
                    quad.args = { Memory::toString(value), "_", quad.arg(2) };
                    remember(quad);
                    folded = true;
                }
                else if (base == "cvt" && hasLeft && leftIsNumber)
                {
                    if (type.isFloat)
                    {
                        const double value = normalizeFloat(
                            leftIsFloat ? leftDouble : typeOf(quad.arg(1)).isUnsigned ? static_cast<double>(static_cast<std::uint64_t>(leftInt)) :
                                                                                        static_cast<double>(leftInt),
                            type.bits);
                        quad.op = ":=";
                        quad.args = { formatFloatLiteral(value), "_", quad.arg(2) };
                    }
                    else
                    {
                        const std::int64_t value = leftIsFloat ? static_cast<std::int64_t>(leftDouble) : leftInt;
                        quad.op = ":=";
                        quad.args = { Memory::toString(normalizeInt(
                                          value, type)),
                                      "_", quad.arg(2) };
                    }
                    remember(quad);
                    folded = true;
                }
            }
            if (folded)
            {
                ++report.constantsFolded;
                changed = true;
                if (collectTrace) report.trace.push_back("常量折叠：" + quad.args[2] + " = " + quad.args[0]);
                passOne.push_back(std::move(quad));
                continue;
            }

            // 4. 代数化简：`x+0`、`x*1`、`x<<0` 这类直接变成一次赋值。
            {
                const Memory::String left = quad.arg(0);
                const Memory::String right = quad.arg(1);
                Memory::String keep;
                bool simplified = false;

                const auto isInteger = [&](const Memory::String &value) {
                    if (value.empty()) return true;
                    bool number = false, floating = false;
                    std::int64_t integer = 0;
                    double real = 0;
                    Memory::String text;
                    if (readLiteral(value, number, floating, integer, real, text)) return number && !floating;
                    const auto found = types.find(value);
                    return found != types.end() && !found->second.isFloat;
                };
                const bool integerIdentity = !type.isFloat && isInteger(left) && isInteger(right);
                if (integerIdentity && (base == "+" || base == "-" || base == "|" || base == "^") && right == "0")
                {
                    keep = left;
                    simplified = true;
                }
                else if (integerIdentity && base == "+" && left == "0")
                {
                    keep = right;
                    simplified = true;
                }
                else if (integerIdentity && (base == "*" || base == "/") && right == "1")
                {
                    keep = left;
                    simplified = true;
                }
                else if (integerIdentity && base == "*" && left == "1")
                {
                    keep = right;
                    simplified = true;
                }
                else if (integerIdentity && (base == "<<" || base == ">>") && right == "0")
                {
                    keep = left;
                    simplified = true;
                }

                if (simplified)
                {
                    ++report.algebraicSimplified;
                    changed = true;
                    if (collectTrace) report.trace.push_back("代数化简：" + quad.op + " " + left + ", " + right + " → " + keep);
                    quad.op = (base == "+" || base == "-" || base == "*" || base == "/") ?
                                  "cvt:" + (typeName.empty() ? Memory::String("i32") : typeName) :
                                  Memory::String(":=");
                    quad.args = { keep, "_", quad.arg(2) };
                    constants.erase(quad.arg(2));
                    passOne.push_back(std::move(quad));
                    continue;
                }
            }

            // 5. 常量条件的跳转：`jz` / `jnz` 直接折成 `j` 或删掉。
            if (base == "jz" || base == "jnz")
            {
                bool isNumber = false, isFloat = false;
                std::int64_t intValue = 0;
                double doubleValue = 0.0;
                Memory::String text;
                if (readLiteral(quad.arg(0), isNumber, isFloat, intValue, doubleValue, text) && isNumber)
                {
                    const bool zero = (isFloat ? doubleValue == 0.0 : intValue == 0);
                    const bool take = (base == "jz") ? zero : !zero;
                    ++report.deadBranches;
                    changed = true;
                    if (take)
                    {
                        if (collectTrace) report.trace.push_back("死分支：" + quad.op + " 条件恒真 → j " + quad.arg(2));
                        quad.op = "j";
                        quad.args = { "_", "_", quad.arg(2) };
                    }
                    else
                    {
                        if (collectTrace) report.trace.push_back("死分支：" + quad.op + " 条件恒假 → 删除");
                        continue;  // 整条丢掉
                    }
                }
            }

            // 6. 其它写入：更新常量表（`:=` 记录，其它类型的写入清掉）。
            const int written = writesTo(base);
            if (written >= 0)
            {
                const Memory::String name = quad.arg(static_cast<std::size_t>(written));
                if (!name.empty())
                {
                    if (base == ":=")
                    {
                        bool isNumber = false, isFloat = false;
                        std::int64_t intValue = 0;
                        double doubleValue = 0.0;
                        Memory::String text;
                        if (readLiteral(quad.arg(0), isNumber, isFloat, intValue, doubleValue, text))
                        {
                            remember(quad);
                        }
                        else
                        {
                            constants.erase(name);
                        }
                    }
                    else
                    {
                        constants.erase(name);
                    }
                }
            }
            if (base == "jz" || base == "jnz" || (writesTo(base) < 0 && base != "param" && base != "write" && base != "putc")) constants.clear();
            passOne.push_back(std::move(quad));
        }
        changed = optimizeLocalCopies(passOne, types, report) || changed;

        Memory::Map<Memory::String, std::size_t> labels;
        for (std::size_t i = 0; i < passOne.size(); ++i)
            if (passOne[i].op == "label") labels[passOne[i].arg(2)] = i;
        for (auto &quad : passOne)
            if (quad.op == "j" || quad.op == "jz" || quad.op == "jnz")
            {
                auto target = quad.arg(2);
                Memory::Set<Memory::String> visited;
                bool cycle = false;
                for (unsigned depth = 0; depth < 32; ++depth)
                {
                    if (!visited.insert(target).second)
                    {
                        cycle = true;
                        break;
                    }
                    const auto label = labels.find(target);
                    if (label == labels.end()) break;
                    auto next = label->second + 1;
                    while (next < passOne.size() && passOne[next].op == "label") ++next;
                    if (next >= passOne.size() || passOne[next].op != "j") break;
                    target = passOne[next].arg(2);
                }
                if (!cycle && target != quad.arg(2))
                {
                    quad.args[2] = target;
                    ++report.deadBranches;
                    changed = true;
                }
            }
        // 7. 不可达代码：`j` / `halt` / `ret` 之后到下一个标签之间删掉；紧跟 `j` 的标签
        //    让这条 `j` 变成多余的。
        Memory::Vector<Quad> reachable;
        reachable.reserve(passOne.size());
        for (std::size_t index = 0; index < passOne.size(); ++index)
        {
            Memory::String typeName;
            const Memory::String base = splitType(passOne[index].op, typeName);

            // 7.1 跳到紧邻的下一条标签 → 这条跳转没用。
            if (base == "j" && (index + 1u < passOne.size()))
            {
                Memory::String nextType;
                // 注意要比较名字：`j loop` 后面跟着 `label done` 时这条跳转是有用的（回边）。
                if ((splitType(passOne[index + 1u].op, nextType) == "label") && (passOne[index + 1u].arg(2) == passOne[index].arg(2)))
                {
                    ++report.deadCode;
                    changed = true;
                    continue;
                }
            }
            reachable.push_back(std::move(passOne[index]));

            // 7.2 无条件跳转 / 停机 / 返回之后，直到下一个标签都不可达。
            if (base == "j" || base == "halt" || base == "ret")
            {
                while ((index + 1u < passOne.size()))
                {
                    Memory::String nextType;
                    if (splitType(passOne[index + 1u].op, nextType) == "label") break;
                    ++index;
                    // `decl` 是**类型元信息**，不是可执行指令：它常常排在函数标签之前
                    // （`simple_ast` 生成的形参声明就是 `(decl …)` 在 `(label …)` 前面，
                    // 前面还隔着引导用的 `(halt …)`）。删了它，`program.types` 就少了
                    // 形参的类型，调用时的实参归一化与 `write` 的格式都会跟着错。
                    if (splitType(passOne[index].op, nextType) == "decl")
                    {
                        reachable.push_back(std::move(passOne[index]));
                        continue;
                    }
                    ++report.deadCode;
                    changed = true;
                }
            }
        }

        // 8. 死代码：结果变量从没被读过的 `:=` 删掉。
        Memory::Map<Memory::String, std::size_t> readCount;
        for (const Quad &quad : reachable)
        {
            for (const Memory::String &name : readOperands(quad)) ++readCount[name];
        }
        Memory::Vector<Quad> final;
        final.reserve(reachable.size());
        Memory::Set<Memory::String> initialized;
        for (Quad &quad : reachable)
        {
            Memory::String typeName;
            const Memory::String base = splitType(quad.op, typeName);
            bool numeric = false, floating = false;
            std::int64_t integer = 0;
            double real = 0;
            Memory::String text;
            const bool safeSource = readLiteral(quad.arg(0), numeric, floating, integer, real, text) || initialized.count(quad.arg(0));
            if (base == "label" || base == "call" || base == "callptr" || base == "j" || base == "jz" || base == "jnz" || base == "ret" || base == "halt") initialized.clear();
            if (base == "decl") initialized.insert(quad.arg(2));
            const auto written = writesTo(base);
            if (written >= 0) initialized.insert(quad.arg(static_cast<std::size_t>(written)));
            if (base == ":=")
            {
                const Memory::String result = quad.arg(2);
                if (!result.empty() && readCount[result] == 0u && !types.count(result) && safeSource)
                {
                    ++report.deadCode;
                    changed = true;
                    continue;
                }
            }
            final.push_back(std::move(quad));
        }

        file.quads = std::move(final);
        if (!changed) break;
    }

    return report;
}

#if STACKVM_WITH_THREADS
/**
 * @brief 宿主投递的中断邮箱（`Machine::raiseInterrupt`）。
 *
 * `posted` 是**无锁快路径**计数：没有宿主投递时，VM 每条指令只读一个原子量，
 * 不加锁；有投递时取锁把队列整体搬走。**跨线程才需要这把锁**，所以板端
 * （`STACKVM_WITH_THREADS=0`）整段裁掉，只剩 VM 自己的软件定时器。
 */
struct InterruptMailbox
{
    std::mutex mutex;
    std::atomic<std::size_t> posted{ 0u };
    Memory::Vector<InterruptEvent> queue;
};

/**
 * @brief 向**当前虚拟机**投递一次中断事件（交绑定在该定时器上的回调）。
 *
 * 这是胶水层触发 MAGEL 中断事件函数的入口，对应 `std::event::Timer` 的 `handle`。
 * **异步 + 安全点交付**：宿主只把事件挂进邮箱就返回，回调由 VM 在**下一条指令
 * 边界**用执行线程调用 —— 不会重入正在跑的宿主回调，也不用自己加锁保护 VM 状态。
 * 句柄无效 / 定时器已 `destroy` / 还没绑回调时事件被丢弃（空操作）。
 * @param timer `std::event::Timer` 的句柄。
 * @param source 原样交给回调的来源值。
 */
void Machine::raiseInterrupt(std::int64_t timer, std::int64_t source)
{
    if (interruptMailbox_ == nullptr) interruptMailbox_ = std::make_shared<InterruptMailbox>();
    std::lock_guard<std::mutex> guard(interruptMailbox_->mutex);
    interruptMailbox_->queue.push_back(InterruptEvent{ timer, source, 0, threadId_ });
    interruptMailbox_->posted.fetch_add(1u, std::memory_order_relaxed);
}

/**
 * @brief 按中断句柄投递：函数指针 + 定时器 + 归属线程。
 *
 * `handle.thread` 不是本 VM 的线程时**忽略**（调用方不用自己判断，投错线程是空操作）；
 * `handle.callback` 非 0 时直接跑它，所以宿主不用先让 MAGEL 侧 `onInterrupt` 绑回调；
 * `handle.timer` 仍然参与"句柄无效 / 定时器已经 destroy"的判定（那种情况照样丢弃）。
 * @param handle 中断句柄（见 @ref InterruptHandle）。
 * @param source 交给回调的来源值。
 */
void Machine::raiseInterrupt(const InterruptHandle &handle, std::int64_t source)
{
    /* 只投给归属线程的 VM：别的线程的同名句柄不会误触发。 */
    if (handle.thread != threadId_) return;
    if (handle.timer == 0 && handle.callback == 0) return;
    if (interruptMailbox_ == nullptr) interruptMailbox_ = std::make_shared<InterruptMailbox>();
    std::lock_guard<std::mutex> guard(interruptMailbox_->mutex);
    interruptMailbox_->queue.push_back(
        InterruptEvent{ handle.timer, source, handle.callback, handle.thread });
    interruptMailbox_->posted.fetch_add(1u, std::memory_order_relaxed);
}

/**
 * @brief 清除虚拟机待处理的中断。
 */
void Machine::clearInterrupts()
{
    if (interruptMailbox_ == nullptr) return;
    std::lock_guard<std::mutex> guard(interruptMailbox_->mutex);
    interruptMailbox_->queue.clear();
    interruptMailbox_->posted.store(0u, std::memory_order_relaxed);
}

/**
 * @brief 线程运行时：每个隔离宿主线程一个 VM，消息在它们之间复制传递。
 *
 * 板端（`STACKVM_WITH_THREADS=0`）整段裁掉：不编 `std::thread` /
 * `std::mutex` / `std::condition_variable`，`std::thread::*` 与
 * `std::concurrency::Shared` 走"函数入口不存在"的普通错误。
 */
struct ThreadRuntime
{
    struct Worker
    {
        std::thread host;
        Memory::Deque<std::shared_ptr<TransferPacket>> inbox;
        RunResult result;
        bool done = false;
        bool joined = false;
        bool reported = false;
        bool receiving = false;
        bool joining = false;
        std::size_t joinTarget = 0u;
    };

    std::mutex mutex;
    std::condition_variable changed;
    Memory::Map<std::size_t, std::shared_ptr<Worker>> workers;
    std::size_t nextId = 1u;
    bool closing = false;
    std::atomic<bool> panicked{ false };  ///< 发布快照后置位，所有 VM/AOT 安全点检查。
    Memory::String panicError;
    std::size_t panicThread = 0;
#if STACKVM_WITH_PANIC_TRACE
    Memory::Vector<DebugFrame> panicTrace;
    SourceLocation panicLocation;
#endif

    /**
     * @brief 调用方持有 mutex；全部活线程只在等待，且没有待收消息时形成死锁。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool deadlocked() const
    {
        bool live = false;
        for (const auto &item : workers)
        {
            const Worker &worker = *item.second;
            if (worker.done) continue;
            live = true;
            if (!worker.inbox.empty()) return false;
            if (worker.receiving) continue;
            if (!worker.joining) return false;
            const auto target = workers.find(worker.joinTarget);
            if (target != workers.end() && target->second->done) return false;
        }
        return live;
    }
};

struct SharedRuntime
{
    std::mutex mutex;
    std::int64_t next = 1;
    Memory::Map<std::int64_t, std::shared_ptr<TransferPacket>> values;
};
#endif /* STACKVM_WITH_THREADS */

/**
 * @brief 设置断点集合（指令下标）。
 * @param instructions 断点位置；重复项会被忽略，空表等于清空。
 */
void Machine::setBreakpoints(Memory::Vector<std::size_t> instructions)
{
    breakpoints_.clear();
    for (const std::size_t instruction : instructions) breakpoints_.insert(instruction);
}

/**
 * @brief 执行一个组装好的程序。
 * @param program 程序。
 * @param input `read` / `getc` 的输入项（按顺序取）。
 * @param stepLimit 指令条数上限，防止死循环；传 `0` 表示不设上限（缺省为 0）。
 * @return 执行结果。
 */
RunResult Machine::run(const Program &program, const Memory::Vector<Memory::String> &input,
                       std::size_t stepLimit)
{
    RunResult result;
    Memory::Vector<Memory::String> featureErrors;
    if (!validateProgramMetadata(program, &featureErrors))
    {
        result.error = featureErrors.front();
        return result;
    }
    // 本次运行生效的调试表：调用方临时挂的优先，其次是程序自带的那张。
    debugInfo_ = debugOverride_ != nullptr ? debugOverride_ : (program.debug.empty() ? nullptr : &program.debug);
    currentLocation_ = SourceLocation{};
    debugFrames_.clear();
#if STACKVM_WITH_THREADS
    if (sharedRuntime_ == nullptr) sharedRuntime_ = Memory::makeShared<SharedRuntime>();
    if (threadsEnabled_ && threadRuntime_ == nullptr)
    {
        threadRuntime_ = Memory::makeShared<ThreadRuntime>();
        threadRuntime_->workers.emplace(0u, Memory::makeShared<ThreadRuntime::Worker>());
    }
#endif
    if (timeCallback_)
    {
        result.timingAvailable = true;
        result.startedNanoseconds = timeCallback_();
    }
    Memory::Vector<Slot> stack;
    Memory::Map<Memory::String, Slot> globals;
    using execution_detail::absent;
    execution_detail::ExecutionPlan plan(program);
    register_core::Engine registerEngine(program, plan, registerJumpFolding_, registerSpecialization_, registerServices_);
    Memory::Vector<std::int64_t *> registerSlots;
    execution_detail::DataCapacityIndex dataCapacity(data_);
    const register_core::Region *boundRegion = nullptr;
    if (Memory::restartSignal().pending())
    {
        result.recoverable = true;
        result.recoverableReason = static_cast<int>(Memory::restartSignal().reason());
        return result;
    }
    Memory::Vector<Slot> globalSlots(plan.types.size());
    Memory::Vector<bool> globalBound(plan.types.size(), false);
    // 只对内部固定入口预绑定；宿主入口额外校验注册表版本。
    Memory::Vector<const SlotNativeEntry *> internalEntries(plan.types.size(), nullptr);
    Memory::Vector<const NativeRegistry::Entry *> externalEntries(plan.types.size(), nullptr);
    Memory::Vector<std::size_t> externalRevisions(plan.types.size(), absent);
    /* 中断事件 / 软件定时器：表按 run() 一份，句柄随 run() 失效（`.svmb` 重新运行时重新建）。 */
    TimerRuntime timers;
    // 调用帧栈：每个活动调用独占一帧，返回后清空值并可限量缓存存储，
    // 递归的每层因此各有各的局部变量存储。
    Memory::Vector<Memory::UniquePtr<Frame>> frames;
    // 只缓存已返回的小帧；活动递归帧和协程帧始终独占存储。
    Memory::Vector<Memory::UniquePtr<Frame>> returnedFrames;
    Memory::Vector<PendingArg> pending;
    // 堆：紧跟在数据区后面长（没有数据区时从地址 1 开始，0 留给空指针）。
    const std::size_t heapBase = (data_ == nullptr || data_->size() == 0u) ? kRuntimeCellBytes : data_->size();
#if STACKVM_WITH_DEFAULT_HEAP
    Heap defaultHeap;
    HeapAllocator &heap = heapAllocator_ == nullptr ? static_cast<HeapAllocator &>(defaultHeap) : *heapAllocator_;
#else
    /* 板端不编默认字节堆（`STACKVM_WITH_DEFAULT_HEAP=0`）：必须由宿主注入字节池
     *（`mcuArenas::SetBytePool`）。没注入时用空堆，全部分配明确失败。 */
    NoHeapAllocator noHeap;
    HeapAllocator &heap = heapAllocator_ == nullptr ? static_cast<HeapAllocator &>(noHeap) : *heapAllocator_;
#endif
    heap.reset(heapBase);
    std::size_t inputIndex = 0;
    int bigFloatPrecision = 34;
    bool exitRequested = false;
    int exitCode = 0;
    // 运行期对象表：`bint`（大整数）与 vec / set / map 三种容器的"值"就是这里的句柄。
    // 跟着这次 run 走，一条 `(drop, 句柄, _, _)` 或 `(free, 句柄, _, _)` 就能还回去。
    ObjectStore objects;
    std::size_t pc = 0;
    // 内建符号在 VM 端按同一 FNV 算法绑定，不把名字写进文件。
    Memory::Map<Memory::String, Memory::String> nativeNames;
    Memory::Map<Memory::String, std::size_t> dataSymbols;
    if (program.compactSymbols)
    {
        for (const auto &[name, ignored] : kSlotNativeEntries)
        {
            (void)ignored;
            nativeNames.emplace(program.symbolKey(name), name);
        }
        for (const char *name : { "__vm_message_receive_coroutine", "__vm_message_receive_thread", "__vm_message_send_coroutine", "__vm_message_send_thread", "__vm_shared_create", "__vm_shared_get", "__vm_shared_set", "__vm_shared_swap", "main", "std::coroutine::create", "std::coroutine::current", "std::coroutine::parent", "std::coroutine::receive", "std::coroutine::send", "std::coroutine::wait", "std::coroutine::yelid", "std::coroutine::yield", "std::thread::create", "std::thread::current", "std::thread::parent", "std::thread::receive", "std::thread::send", "std::thread::wait" })
            nativeNames.emplace(program.symbolKey(Memory::String(name)), name);
        if (data_)
            for (const auto &[name, offset] : data_->symbols)
                dataSymbols.emplace(program.symbolKey(name), offset);
    }
    Memory::Vector<const Memory::String *> nativeAliases(plan.types.size(), nullptr);
    Memory::Vector<bool> specialEntries(plan.types.size(), false);
    if (optimizeExecution_)
        for (const auto &item : plan.symbols)
        {
            const auto symbol = item.second;
            const auto alias = nativeNames.find(item.first);
            const auto &name = alias == nativeNames.end() ? item.first : alias->second;
            specialEntries[symbol] = name.compare(0, 15, "std::coroutine::") == 0 || name.compare(0, 13, "std::thread::") == 0 || name.compare(0, 13, "__vm_message_") == 0 || name.compare(0, 12, "__vm_shared_") == 0;
            internalEntries[symbol] = kSlotNatives.find(name);
            if (alias != nativeNames.end()) nativeAliases[symbol] = &alias->second;
        }
    /**
     * @brief 仅在宿主注册表版本变化时重新查找入口，包括此前未注册的名称。
     */
    const NativeRegistry *cachedRegistry = nativeRegistry_;
    const auto resolveExternal = [&](std::size_t symbol, const Memory::String &name) -> const NativeRegistry::Entry * {
        if (cachedRegistry != nativeRegistry_)
        {
            std::fill(externalRevisions.begin(), externalRevisions.end(), absent);
            cachedRegistry = nativeRegistry_;
        }
        if (!nativeRegistry_) return nullptr;
        if (!optimizeExecution_ || symbol == absent) return nativeRegistry_->find(name);
        if (externalRevisions[symbol] != nativeRegistry_->revision())
        {
            externalEntries[symbol] = nativeRegistry_->find(name);
            externalRevisions[symbol] = nativeRegistry_->revision();
        }
        return externalEntries[symbol];
    };
    bool entryPending = !entryLabel_.empty();
    bool jumped = false;                     ///< 本步是否改过 pc（跳转 / 调用 / 返回）
    Memory::Vector<Memory::String> numbers;  // write 的数值：多个值之间一个空格
    Memory::Vector<CoroutineContext> coroutines(1u);
    std::size_t currentCoroutine = 0u;
    coroutines[0].state = CoroutineContext::State::Running;

    auto saveCoroutine = [&](std::size_t resumePc, CoroutineContext::State state) {
        CoroutineContext &context = coroutines[currentCoroutine];
        context.stack = std::move(stack);
        context.frames = std::move(frames);
        context.pending = std::move(pending);
        context.pc = resumePc;
        context.state = state;
    };
    auto loadCoroutine = [&](std::size_t index) {
        currentCoroutine = index;
        CoroutineContext &context = coroutines[index];
        stack = std::move(context.stack);
        frames = std::move(context.frames);
        pending = std::move(context.pending);
        pc = context.pc;
        context.state = CoroutineContext::State::Running;
        jumped = true;
    };
    auto nextRunnable = [&](std::size_t after) -> std::size_t {
        if (coroutines.size() <= 1u) return coroutines.size();
        for (std::size_t offset = 1u; offset <= coroutines.size(); ++offset)
        {
            const std::size_t index = (after + offset) % coroutines.size();
            const CoroutineContext::State state = coroutines[index].state;
            if (state == CoroutineContext::State::Ready || state == CoroutineContext::State::Suspended)
                return index;
        }
        return coroutines.size();
    };

    /**
     * @brief 地址空间：低地址是数据区（编译期布局），再往后是堆。
     *
     * `(load, 地址, …)` / `(store, 地址, …)` 不区分对象在哪一段，按地址落位。
     * @param address 地址。
     * @return 落在堆区返回 true，否则当数据区。
     */
    auto inHeap = [&](std::size_t address) { return heap.contains(address); };

    /**
     * @brief 按地址读 8 个字节。
     * @param address 地址。
     * @param isFloat 是否按 IEEE-754 双精度解释。
     * @param out 结果。
     * @return 成功返回 true。
     */
    auto loadMemory = [&](std::size_t address, bool isFloat, Slot &out) -> bool {
        if (inHeap(address))
        {
            if (isFloat)
            {
                double real = 0.0;
                if (!heap.readDouble(address, real)) return false;
                out = Slot::fromDouble(real);
                return true;
            }
            std::int64_t integer = 0;
            if (!heap.readInt(address, integer)) return false;
            out = Slot::fromInt(integer);
            return true;
        }
        if (data_ == nullptr) return false;
        if (isFloat)
        {
            double real = 0.0;
            if (!data_->readDouble(address, real)) return false;
            out = Slot::fromDouble(real);
            return true;
        }
        std::int64_t integer = 0;
        if (!data_->readInt(address, integer)) return false;
        out = Slot::fromInt(integer);
        return true;
    };

    /**
     * @brief 按地址写 8 个字节。
     * @param address 地址。
     * @param isFloat 是否按 IEEE-754 双精度写。
     * @param value 值。
     * @return 成功返回 true。
     */
    auto storeMemory = [&](std::size_t address, bool isFloat, const Slot &value) -> bool {
        if (inHeap(address))
        {
            return isFloat ? heap.writeDouble(address, value.number()) : heap.writeInt(address, (value.isFloat ? static_cast<std::int64_t>(nativeInt(value)) : value.integer));
        }
        if (data_ == nullptr) return false;
        return isFloat ? data_->writeDouble(address, value.number()) : data_->writeInt(address, (value.isFloat ? static_cast<std::int64_t>(nativeInt(value)) : value.integer));
    };

    /**
     * 追加一个数值输出（与 transfer 的示例套件口径一致）。
     */
    auto emitNumber = [&](Memory::String text) {
        if (!numbers.empty()) result.output += ' ';
        numbers.push_back(std::move(text));
        result.output += numbers.back();
    };

    /**
     * 追加一段**文本**输出：挂了输出下沉就先实时推给宿主（板端串口 / OLED / RTT），
     * `keepText=false` 时不再累积进 `RunResult::text`。`print` / `println` / `putChar`
     * 的指令与原生函数路径都走它，保证下沉看到的字节与累积文本完全一致。
     */
    auto emitText = [&](const char *bytes, std::size_t count) {
        if (bytes == nullptr || count == 0u) return;
        if (outputSink_ != nullptr) outputSink_(outputUser_, bytes, count);
        if (outputKeepText_) result.text.append(bytes, count);
    };

    /* ---- 可恢复终止（软重启的两个触发共用一条通道） ---------------------------
     * ① 分配失败：在**失败点只写标志**（`Memory::restartSignal()`，见
     *    `allocator/magel_allocator.hpp`），当前这次申请由 arena 的应急预留兜住，于是当前
     *    指令照常跑完、容器保持自洽；
     * ② 显式请求：`std::watchdog::requestRestart()` → `__vm_request_restart()` 写同一份标志。
     * VM 在**下一条指令边界（安全点）**看到标志就停在这里，把本轮标成 `recoverable`。
     * **不抛异常、不 longjmp、不在失败点跳走**。
     * panic 的判定永远排在前面：panic 不是可恢复终止。 */
    /**
     * @brief 有没有待处理的软重启触发（分配失败**或**显式请求）。
     */
    auto restartPending = [&]() { return Memory::restartSignal().pending(); };
    /**
     * @brief 在安全点以"可恢复终止"结束本次 run()。
     *
     * 这里**只写标量**、不拼错误串：池可能刚耗尽（应急预留有限），一格式化就可能再撞一次。
     * 结构化原因（类别 / 申请字节 / 池内快照）由宿主从 `Memory::restartSignal()`
     * 读；`result.error` 里如果有内容，那是失败点自己的普通错误文案，一并留给宿主。
     */
    auto stopForRestart = [&]() {
        result.ok = false;
        result.recoverable = true;
        result.recoverableReason = static_cast<int>(Memory::restartSignal().reason());
#if STACKVM_WITH_HOST_EXCEPTION_GUARD
        if (!result.error.empty()) return;
        /* 桌面口径可以安全地补一句可读文案（结构化原因仍在信号位里）。池可能刚耗尽，
         * 所以拼字符串本身也要兜住异常 —— 拼不出来就只留结构化标记，宿主照旧能判可恢复。 */
        try
        {
            result.error = Memory::restartSignal().reason() == Memory::WatchdogReason::ExplicitRequest ? Memory::String("软重启：程序显式请求重启（可恢复终止）") : Memory::String("软重启：Magel 运行内存池用尽（可恢复终止）");
        } catch (...)
        {
        }
#endif
    };

    /**
     * @brief 按数字身份找变量；共享汇编入口与动态回调保留名称回退。
     */
    auto findDense = [&](std::size_t symbol) -> Slot * {
        if (symbol == absent) return nullptr;
        if (!frames.empty() && frames.back()->layout)
        {
            auto &frame = *frames.back();
            const auto local = frame.layout->slots.find(symbol);
            if (local != frame.layout->slots.end() && frame.bound[local->second]) return &frame.slots[local->second];
        }
        return globalBound[symbol] ? &globalSlots[symbol] : nullptr;
    };
    /**
     * @brief 找变量槽，保留局部优先、全局回退及动态回调临时变量。
     */
    auto findSlot = [&](const Memory::String &name) -> Slot * {
        if (!frames.empty())
        {
            const auto local = frames.back()->locals.find(name);
            if (local != frames.back()->locals.end()) return &local->second;
        }
        if (optimizeExecution_)
        {
            if (auto *slot = findDense(plan.find(name))) return slot;
        }
        const auto global = globals.find(name);
        return global == globals.end() ? nullptr : &global->second;
    };
    /**
     * @brief 按已解析类型归一化写入值，未声明临时量保持原始形态。
     */
    const auto normalizeStored = [&](std::size_t symbol, Slot value) {
        const auto *type = symbol == absent ? nullptr : plan.types[symbol].type;
        if (type && !value.isText)
        {
            if (type->isFloat)
                value = Slot::fromDouble(normalizeFloat(value.number(), type->bits));
            else
                value = Slot::fromInt(normalizeInt(value.isFloat ? static_cast<std::int64_t>(nativeInt(value)) : value.integer, *type));
        }
        return value;
    };
    /**
     * @brief 对预绑定指令直接读取局部槽，全局与共享汇编路径保持原有可见性。
     */
    const auto readPrepared = [&](std::size_t index) -> Slot * {
        const auto &prepared = plan.code[index];
        if (!optimizeExecution_ || prepared.shared || (!frames.empty() && !frames.back()->locals.empty()))
            return findSlot(program.code[index].name);
        if (!frames.empty() && prepared.owner == frames.back()->layout && prepared.local != execution_detail::absentRegister && frames.back()->bound[prepared.local])
            return &frames.back()->slots[prepared.local];
        if (!frames.empty() && prepared.owner != frames.back()->layout) return findSlot(program.code[index].name);
        return prepared.symbol != absent && globalBound[prepared.symbol] ? &globalSlots[prepared.symbol] : nullptr;
    };
    /**
     * @brief 写变量，普通指令走稠密存储，未登记回调目的地走原有动态表。
     */
    auto assign = [&](const Memory::String &name, const Slot &value) {
        const auto symbol = plan.find(name);
        const auto stored = normalizeStored(symbol, value);
        if (!frames.empty())
        {
            const auto local = frames.back()->locals.find(name);
            if (local != frames.back()->locals.end())
            {
                local->second = stored;
                return;
            }
        }
        if (optimizeExecution_ && symbol != absent)
        {
            if (auto *slot = findDense(symbol))
            {
                *slot = stored;
                return;
            }
            globalSlots[symbol] = stored;
            globalBound[symbol] = true;
            return;
        }
        globals[name] = stored;
    };
    /**
     * @brief 按预解析局部槽直接写入，未执行声明时仍写全局。
     */
    const auto storePrepared = [&](std::size_t index, const Slot &value) {
        const auto &prepared = plan.code[index];
        if (!optimizeExecution_ || prepared.shared || (!frames.empty() && (prepared.owner != frames.back()->layout || !frames.back()->locals.empty())))
        {
            assign(program.code[index].name, value);
            return;
        }
        const auto stored = normalizeStored(prepared.symbol, value);
        if (!frames.empty() && prepared.local != execution_detail::absentRegister && frames.back()->bound[prepared.local])
            frames.back()->slots[prepared.local] = stored;
        else
        {
            globalSlots[prepared.symbol] = stored;
            globalBound[prepared.symbol] = true;
        }
    };
    /**
     * @brief 为调用帧分配该函数实际需要的槽位，不按整个程序的符号数扩容。
     */
    const auto prepareFrame = [&](Frame &frame, std::size_t entry) {
        if (!optimizeExecution_) return;
        frame.layout = plan.layout(entry);
        frame.slots.resize(frame.layout->slots.size());
        frame.bound.resize(frame.slots.size(), false);
    };
    /**
     * @brief 绑定参数到稠密槽，动态汇编参数名保留回退。
     */
    const auto bindParameter = [&](Frame &frame, const Memory::String &name, const Slot &value) {
        if (frame.layout)
        {
            const auto slot = frame.layout->slots.find(plan.find(name));
            if (slot != frame.layout->slots.end())
            {
                frame.slots[slot->second] = value;
                frame.bound[slot->second] = true;
                return;
            }
        }
        frame.locals[name] = value;
    };

    /**
     * @brief 把 VM 的调用帧栈翻译成"源码级调用栈"。
     *
     * 调用者的位置取该帧的**返回地址**（就是调用点后面那条指令），当前帧用 `at`。
     * 无调试表时可用可选的函数符号和指令位置；宏关闭后只支持显式调试。
     */
    auto debugFramesOf = [&](std::size_t at) {
        Memory::Vector<DebugFrame> snapshot;
#if !STACKVM_WITH_PANIC_TRACE
        if (debugInfo_ == nullptr) return snapshot;
#endif
        snapshot.reserve(frames.size() + 1u);
        std::size_t callerIndex = 0u;
        for (const auto &frame : frames)
        {
            DebugFrame entry;
            entry.threadId = threadId_;
            entry.coroutineId = currentCoroutine;
            if (frame)
            {
                /* 帧里存的是**返回地址**（调用指令的下一条），往回退一条才是调用点本身。 */
                const std::size_t callSite = frame->returnIp > 0u ? frame->returnIp - 1u : 0u;
                entry.instruction = callSite;
                if (debugInfo_ != nullptr)
                    if (const SourceLocation *caller = debugInfo_->lookup(callSite))
                    {
                        entry.function = caller->function;
                        entry.location = *caller;
                    }
            }
#if STACKVM_WITH_PANIC_TRACE
            if (entry.function.empty()) entry.function = callerIndex == 0u ? (entryLabel_.empty() ? Memory::String("(入口)") : program.symbolKey(entryLabel_)) : frames[callerIndex - 1u]->function;
#endif
            ++callerIndex;
            snapshot.push_back(std::move(entry));
        }
        DebugFrame current;
        current.threadId = threadId_;
        current.coroutineId = currentCoroutine;
        current.instruction = at;
        if (debugInfo_ != nullptr)
            if (const SourceLocation *where = debugInfo_->lookup(at)) current.location = *where;
        current.function = current.location.function;
#if STACKVM_WITH_PANIC_TRACE
        if (current.function.empty()) current.function = frames.empty() ? (entryLabel_.empty() ? Memory::String("(入口)") : program.symbolKey(entryLabel_)) : frames.back()->function;
#endif
        snapshot.push_back(std::move(current));
        return snapshot;
    };

    auto fail = [&](const Memory::String &message) {
        result.ok = false;
        result.error = message;
        if (program.compactSymbols || !panicDetailsEnabled_)
        {
            result.failureLocation = SourceLocation{};
            result.sourceTrace.clear();
            currentLocation_ = SourceLocation{};
            debugFrames_.clear();
        }
        else
        {
            if (debugInfo_ != nullptr)
                if (const SourceLocation *where = debugInfo_->lookup(pc)) result.failureLocation = *where;
            currentLocation_ = result.failureLocation;
#if STACKVM_WITH_PANIC_TRACE
            if (result.sourceTrace.empty()) result.sourceTrace = debugFramesOf(pc);
            debugFrames_ = result.sourceTrace;
#else
            debugFrames_.clear();
#endif
        }
#if STACKVM_WITH_THREADS
        if (message.rfind("panic", 0u) == 0u && threadRuntime_)
        {
            std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
            if (!threadRuntime_->panicked.load(std::memory_order_relaxed))
            {
                threadRuntime_->panicError = message;
                threadRuntime_->panicThread = threadId_;
#if STACKVM_WITH_PANIC_TRACE
                threadRuntime_->panicTrace = result.sourceTrace;
                threadRuntime_->panicLocation = result.failureLocation;
#endif
                threadRuntime_->closing = true;
                threadRuntime_->panicked.store(true, std::memory_order_release);
                threadRuntime_->changed.notify_all();
            }
        }
#endif /* STACKVM_WITH_THREADS */
    };

#if STACKVM_WITH_THREADS
    /**
     * @brief 子线程错误传播保留子线程的真实帧，不以父线程等待点替代。
     */
    auto failFromThread = [&](const Memory::String &message, const RunResult &child) {
        fail(message);
#if STACKVM_WITH_PANIC_TRACE
        if (!program.compactSymbols && panicDetailsEnabled_ && !child.sourceTrace.empty())
        {
            result.sourceTrace = child.sourceTrace;
            result.failureLocation = child.failureLocation;
            debugFrames_ = result.sourceTrace;
            currentLocation_ = result.failureLocation;
        }
#else
        (void)child;
#endif
    };

    /**
     * @brief 在安全点接受全程序 panic，以首个发布者的错误与栈为准。
     */
    auto stopForPanic = [&]() -> bool {
        if (!threadRuntime_ || !threadRuntime_->panicked.load(std::memory_order_acquire)) return false;
        std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
        result.ok = false;
        result.error = threadRuntime_->panicThread == 0u ? threadRuntime_->panicError :
                                                           "线程 " + Memory::toString(threadRuntime_->panicThread) + " 失败：" + threadRuntime_->panicError;
        result.sourceTrace.clear();
        result.failureLocation = SourceLocation{};
#if STACKVM_WITH_PANIC_TRACE
        if (!program.compactSymbols && panicDetailsEnabled_)
        {
            result.sourceTrace = threadRuntime_->panicTrace;
            result.failureLocation = threadRuntime_->panicLocation;
        }
#endif
        debugFrames_ = result.sourceTrace;
        currentLocation_ = result.failureLocation;
        return true;
    };
#else
    /**
     * @brief 板端没有线程：没有跨线程 panic 要在这里收，安全点检查恒为"没停"。
     */
    auto stopForPanic = [&]() -> bool { return false; };
#endif /* STACKVM_WITH_THREADS */

    /**
     * @brief 进一层函数：绑实参 → 在堆上开一帧 → 跳过去。直接调用与间接调用共用。
     *
     * `positional = false`（直接调用）：实参按**调用点写着的形参名**绑进新帧
     * （前端知道 callee 的形参名，逐个写出来了）。
     * `positional = true`（函数指针）：调用点不知道会调到谁，所以按**位置**绑 ——
     * 表来自 `program.parameters`（组装时从"函数标签前面那段 decl"扫出来的）。
     *
     * @param label 目标函数标签。
     * @param returnDest 返回值写回哪个变量（可空）。
     * @param positional 是否按位置绑实参。
     * @return 成功返回 true。
     */
    auto enterFunction = [&](const Memory::String &label, const Memory::String &returnDest,
                             bool positional) -> bool {
        std::size_t entry = absent;
        if (optimizeExecution_ && pc < program.code.size() && program.code[pc].op == Op::Call && program.code[pc].name == label)
            entry = plan.code[pc].target;
        else
        {
            const auto target = program.labels.find(program.symbolKey(label));
            if (target != program.labels.end()) entry = target->second;
        }
        if (entry == absent)
        {
            fail("函数入口不存在（也不在原生函数表里）：" + label);
            pending.clear();
            return false;
        }
        // 每层调用取得独占帧；优先复用已清空的小帧，缓存为空时分配。
        Memory::UniquePtr<Frame> frame;
        if (optimizeExecution_ && registerExecution_ && registerServices_ && !returnedFrames.empty())
        {
            frame = std::move(returnedFrames.back());
            returnedFrames.pop_back();
        }
        else
            frame = Memory::makeUnique<Frame>();
        prepareFrame(*frame, entry);
        frame->returnIp = pc + 1u;
        frame->returnDest = returnDest;
#if STACKVM_WITH_PANIC_TRACE
        if (!program.compactSymbols && panicDetailsEnabled_) frame->function = program.symbolKey(label);
#endif

        if (positional)
        {
            const auto names = program.parameters.find(program.symbolKey(label));
            // 零参数函数不产生 decl，旧字节码的形参表因此没有该入口。
            const std::size_t arity = names == program.parameters.end() ? 0u : names->second.size();
            if (arity != pending.size())
            {
                fail("通过函数指针调用 " + label + " 需要 " + Memory::toString(arity) + " 个参数，实到 " + Memory::toString(pending.size()) + " 个");
                pending.clear();
                return false;
            }
            for (std::size_t slot = 0; slot < pending.size(); ++slot)
            {
                bindParameter(*frame, names->second[slot], pending[slot].value);
            }
            pending.clear();
        }
        else
        {
            // 形参按名字绑定进新帧（类型在 callee 的 decl 里声明过）。
            for (const PendingArg &argument : pending)
            {
                const auto type = program.types.find(argument.name);
                Slot value = argument.value;
                if (type != program.types.end() && !value.isText)
                {
                    if (type->second.isFloat)
                        value = Slot::fromDouble(normalizeFloat(value.number(), type->second.bits));
                    else
                        value = Slot::fromInt(normalizeInt(
                            value.isFloat ? (value.isFloat ? static_cast<std::int64_t>(nativeInt(value)) : value.integer) : value.integer, type->second));
                }
                bindParameter(*frame, argument.name, value);
            }
            pending.clear();
        }
        frames.push_back(std::move(frame));
        pc = entry;
        jumped = true;
        return true;
    };

    Memory::Function<bool()> executeOne;
    Memory::Function<bool(const Memory::String &, const Memory::String &)> callNative;
    /* 中断交付（定义在 executeOne 之前，见下面的注释）：线程等待 / 收消息的循环也要用它。 */
    Memory::Function<void()> deliverInterrupts;

    /*
     * 等线程 / 收消息时**中断优先**：不能"睡到条件成立才醒"，否则定时器和宿主投递的
     * 中断会被线程等待卡住。改成小步等待，每步先交付一轮中断。
     *
     * 交付时**必须释放线程锁**：处理器里可能再调线程 / 共享值函数（它们要同一把锁），
     * 持锁交付就是自锁。谓词在放锁前后各查一次，避免丢掉唤醒。
     *
     * 只有线程等待才需要它（板端 `STACKVM_WITH_THREADS=0` 时整段裁掉）。
     */
#if STACKVM_WITH_THREADS
    const auto waitWithInterrupts = [&](std::unique_lock<std::mutex> &guard,
                                        const Memory::Function<bool()> &ready) {
        if (ready()) return;
        for (;;)
        {
            guard.unlock();
            deliverInterrupts();
            guard.lock();
            if (ready() || !result.error.empty()) return;
            threadRuntime_->changed.wait_for(guard, std::chrono::milliseconds(1));
        }
    };
#endif /* STACKVM_WITH_THREADS */
    NativeCallContextImpl::Invoker invokeFunction;
    std::size_t nativeCallbackDepth = 0u;
    /* 中断处理器运行中：放宽消息 / 协程 / 共享值限制（见 deliverInterrupts）。 */
    bool interruptDelivery = false;

    invokeFunction = [&](std::int64_t functionPointer,
                         const Memory::Vector<NativeValue> &arguments,
                         NativeValue &returnValue) -> bool {
        if (functionPointer == 0 || data_ == nullptr)
        {
            fail(functionPointer == 0 ? "胶水层调用了空函数指针" : "没有加载数据区，胶水层无法解析函数指针");
            return false;
        }
        std::int64_t environment = 0;
        const auto target = closureTarget(objects, functionPointer, environment);
        const Memory::String label = target ? data_->readCString(static_cast<std::size_t>(target)) : Memory::String();
        if (label.empty())
        {
            fail("胶水层函数指针没有指向有效函数名：" + Memory::toString(functionPointer));
            return false;
        }

        const std::size_t callerPc = pc;
        const bool callerJumped = jumped;
        const std::size_t callerFrames = frames.size();
        const std::size_t callerStack = stack.size();
        Memory::Vector<PendingArg> callerPending = std::move(pending);
        pending.clear();
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const NativeValue &argument = arguments[index];
            if (argument.isText())
            {
                fail("胶水层调用 MAGEL 函数时，文本参数必须先由 Codec<string> 编码");
                break;
            }
            pending.push_back(PendingArg{ "__magel_p" + Memory::toString(index),
                                          argument.isFloat() ? Slot::fromDouble(argument.floating) : Slot::fromInt(argument.integer) });
        }

        if (environment) pending.push_back(PendingArg{ "__magel_environment", Slot::fromInt(environment) });
        const Memory::String destination = "__magel_native_callback_" + Memory::toString(result.steps) + "_" + Memory::toString(callerFrames);
        ++nativeCallbackDepth;
        bool entered = result.error.empty();
        if (entered && callNative(label, destination))
        {
            entered = result.error.empty();
        }
        else if (entered)
        {
            entered = enterFunction(label, destination, true);
            while (entered && result.error.empty() && frames.size() > callerFrames)
            {
                if (!executeOne()) break;
            }
            if (entered && result.error.empty() && frames.size() != callerFrames)
            {
                fail("胶水层调用的 MAGEL 函数没有正常 return");
                entered = false;
            }
        }

        const auto returned = globals.find(destination);
        if (entered && result.error.empty() && returned != globals.end())
        {
            returnValue = returned->second.isFloat ? NativeValue::fromFloat(returned->second.real) : NativeValue::fromInt(returned->second.integer);
        }
        else if (entered && result.error.empty())
        {
            returnValue = NativeValue::fromInt(0);
        }
        globals.erase(destination);
        if (stack.size() > callerStack) stack.resize(callerStack);
        pending = std::move(callerPending);
        pc = callerPc;
        jumped = callerJumped;
        --nativeCallbackDepth;
        return entered && result.error.empty();
    };

    /**
     * 调用 VM 内部运行时或宿主注册表；不建普通 MAGEL 调用帧。
     */
    Memory::Vector<Memory::UniquePtr<Memory::Vector<Slot>>> internalArgumentBuffers;
    std::size_t internalArgumentDepth = 0;
    /**
     * @brief 执行已绑定内部入口；同步回调递归使用独立参数缓冲，普通调用复用容量。
     */
    const auto applyInternal = [&](const SlotNativeEntry *internal, const Memory::String &name, const Memory::String &returnDest) -> bool {
        if (internal->second.arity >= 0 && pending.size() != static_cast<std::size_t>(internal->second.arity))
        {
            fail("原生函数 " + name + " 需要 " + Memory::toString(internal->second.arity) + " 个参数，实到 " + Memory::toString(pending.size()) + " 个");
            pending.clear();
            return true;  // 已经报错了，算"处理过"
        }
        if (internalArgumentDepth == internalArgumentBuffers.size())
            internalArgumentBuffers.push_back(Memory::makeUnique<Memory::Vector<Slot>>());
        auto &arguments = *internalArgumentBuffers[internalArgumentDepth++];
        struct ArgumentDepthGuard
        {
            std::size_t &depth;
            /**
             * @brief 恢复原生回调调用前的参数栈深度。
             */
            ~ArgumentDepthGuard() { --depth; }
        } argumentGuard{ internalArgumentDepth };
        arguments.clear();
        arguments.reserve(pending.size());
        for (const PendingArg &argument : pending) arguments.push_back(argument.value);
        pending.clear();

        NativeContext context;
        context.heap = &heap;
        context.data = data_;
        context.text = &result.text; /* printf 和 putc / print 共用这条文本流 */
        context.textSink = outputSink_;
        context.textSinkUser = outputUser_;
        context.textKeep = &outputKeepText_;
        context.input = &input;
        context.inputIndex = &inputIndex;
        context.programArguments = &programArguments_;
        context.invoker = &invokeFunction;
        context.objects = &objects;
        context.timers = &timers;
        context.machine = this;
        context.threadId = threadId_;
        context.bigFloatPrecision = &bigFloatPrecision;
        context.exitRequested = &exitRequested;
        context.exitCode = &exitCode;
        context.fileApi = fileApi_;
        const Slot value = internal->second.apply(context, arguments);
        if (!context.memoryFault.empty()) context.error = context.memoryFault;
        if (!context.error.empty())
        {
            fail(context.error);
            return true;
        }
        if (exitRequested)
        {
            result.ok = true;
            result.exitCode = exitCode;
            pc = program.code.size();
            jumped = true;
            return true;
        }
        if (trace_ && !value.isFloat && isObjectHandle(value.integer))
            result.trace.push_back("    => object " + Memory::toString(value.integer));
        if (!returnDest.empty()) assign(returnDest, value);
        return true;
    };
    callNative = [&](const Memory::String &rawName, const Memory::String &returnDest) -> bool {
        const auto symbol = pc < program.code.size() && &rawName == &program.code[pc].name ? plan.code[pc].symbol : plan.find(rawName);
        if (optimizeExecution_ && symbol != absent && internalEntries[symbol])
            return applyInternal(internalEntries[symbol], nativeAliases[symbol] ? *nativeAliases[symbol] : rawName, returnDest);

        if (optimizeExecution_ && symbol != absent && !specialEntries[symbol] && pc < program.code.size() && &rawName == &program.code[pc].name && plan.code[pc].target != absent && !resolveExternal(symbol, rawName)) return false;
        const auto alias = nativeNames.find(rawName);
        const Memory::String &name = alias == nativeNames.end() ? rawName : alias->second;
        const bool messageSendThread = name == "__vm_message_send_thread";
        const bool messageReceiveThread = name == "__vm_message_receive_thread";
        const bool messageSendCoroutine = name == "__vm_message_send_coroutine";
        const bool messageReceiveCoroutine = name == "__vm_message_receive_coroutine";
        const bool sharedCreate = name == "__vm_shared_create";
        const bool sharedGet = name == "__vm_shared_get";
        const bool sharedSet = name == "__vm_shared_set";
        const bool sharedSwap = name == "__vm_shared_swap";
        if (messageSendThread || messageReceiveThread || messageSendCoroutine || messageReceiveCoroutine || sharedCreate || sharedGet || sharedSet || sharedSwap)
        {
            const bool send = messageSendThread || messageSendCoroutine;
            const bool receive = messageReceiveThread || messageReceiveCoroutine;
            const bool withTarget = send || sharedGet || sharedSet || sharedSwap;
            const bool withValue = send || sharedCreate || sharedSet || sharedSwap;
            const std::size_t wanted = (withTarget ? 1u : 0u) + 3u + (withValue ? 1u : 0u);
            if (pending.size() != wanted)
            {
                fail("消息或共享值调用参数个数错误：" + name);
                pending.clear();
                return true;
            }
#if STACKVM_WITH_THREADS
            if ((messageSendThread || messageReceiveThread) && (!threadsEnabled_ || threadRuntime_ == nullptr))
            {
                fail("多线程未启用；解释运行或 C++ 导出时传 --threads");
                pending.clear();
                return true;
            }
#else
            /* 板端不编线程运行时与线程共享值（`STACKVM_WITH_THREADS=0`）：这几个名字明确报
             * "这个构建没有"，而不是掉到"函数入口不存在"。协程消息继续往下走。 */
            if (messageSendThread || messageReceiveThread || sharedCreate || sharedGet || sharedSet || sharedSwap)
            {
                fail("这个构建没有多线程 / 线程共享值（STACKVM_WITH_THREADS=0）：" + name);
                pending.clear();
                return true;
            }
#endif
            if (nativeCallbackDepth != 0u && !interruptDelivery && (send || receive))
            {
                fail("同步胶水回调中不能执行消息操作");
                pending.clear();
                return true;
            }
            const std::size_t base = withTarget ? 1u : 0u;
            const Memory::Vector<PendingArg> suspendedArgs = messageReceiveCoroutine ? pending :
                                                                                       Memory::Vector<PendingArg>{};
            const std::int64_t target = withTarget ? pending[0].value.integer : 0;
            const int kind = static_cast<int>(pending[base].value.integer);
            const std::int64_t layout = pending[base + 1u].value.integer;
            NativeContext context;
            context.heap = &heap;
            context.data = data_;
            context.objects = &objects;
            Memory::String type;
            const bool validType = nativeReadCString(context,
                                                     static_cast<std::size_t>(pending[base + 2u].value.integer), &type);
            if (!validType || kind < 0 || kind > 4)
            {
                fail("消息或共享值的类型描述无效");
                pending.clear();
                return true;
            }
            std::shared_ptr<TransferPacket> packet;
            if (withValue)
            {
                packet = Memory::makeShared<TransferPacket>();
                if (!captureTransfer(context, pending.back().value, kind, layout, type,
                                     packet.get()))
                {
                    fail(context.error);
                    pending.clear();
                    return true;
                }
            }
            pending.clear();
#if STACKVM_WITH_THREADS
            if (sharedCreate || sharedGet || sharedSet || sharedSwap)
            {
                std::shared_ptr<TransferPacket> previous;
                std::int64_t handle = target;
                {
                    std::lock_guard<std::mutex> guard(sharedRuntime_->mutex);
                    if (sharedCreate)
                    {
                        handle = sharedRuntime_->next++;
                        sharedRuntime_->values.emplace(handle, std::move(packet));
                    }
                    else
                    {
                        const auto found = sharedRuntime_->values.find(handle);
                        if (found != sharedRuntime_->values.end() && found->second->type == type)
                        {
                            previous = found->second;
                            if (sharedSet || sharedSwap) found->second = std::move(packet);
                        }
                    }
                }
                if (!sharedCreate && previous == nullptr)
                {
                    fail("std::lang::Shared 类型不匹配或句柄无效：" + type);
                    return true;
                }
                if (sharedCreate)
                {
                    if (!returnDest.empty()) assign(returnDest, Slot::fromInt(handle));
                    return true;
                }
                if (sharedGet || sharedSwap)
                {
                    Slot value;
                    if (!restoreTransfer(context, *previous, &value))
                        fail(context.error);
                    else if (!returnDest.empty())
                        assign(returnDest, value);
                }
                return true;
            }
            if (messageSendThread)
            {
                {
                    std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
                    const auto found = target < 0 ? threadRuntime_->workers.end() :
                                                    threadRuntime_->workers.find(static_cast<std::size_t>(target));
                    if (found == threadRuntime_->workers.end() || found->second->done || threadRuntime_->closing)
                    {
                        fail("发送消息的线程句柄无效或已结束");
                        return true;
                    }
                    found->second->inbox.push_back(std::move(packet));
                }
                threadRuntime_->changed.notify_all();
                return true;
            }
            if (messageReceiveThread)
            {
                {
                    std::unique_lock<std::mutex> guard(threadRuntime_->mutex);
                    const auto self = threadRuntime_->workers.at(threadId_);
                    self->receiving = true;
                    threadRuntime_->changed.notify_all();
                    waitWithInterrupts(guard, [&] {
                        return !self->inbox.empty() || threadRuntime_->closing || threadRuntime_->deadlocked();
                    });
                    self->receiving = false;
                    threadRuntime_->changed.notify_all();
                    if (!result.error.empty()) return true;
                    if (self->inbox.empty())
                    {
                        Memory::String error = threadRuntime_->closing ? "线程收件箱已关闭" :
                                                                         "线程接收消息形成死锁";
                        for (const auto &worker : threadRuntime_->workers)
                            if (worker.second->done && !worker.second->result.ok && !worker.second->result.error.empty())
                            {
                                error = "线程 " + Memory::toString(worker.first) + " 失败：" + worker.second->result.error;
                                failFromThread(error, worker.second->result);
                                return true;
                            }
                        fail(error);
                        return true;
                    }
                    packet = self->inbox.front();
                    if (packet->type != type)
                    {
                        fail("线程消息类型不匹配：期望 " + type + "，收到 " + packet->type);
                        return true;
                    }
                    self->inbox.pop_front();
                }
            }
#endif /* STACKVM_WITH_THREADS */
            if (messageSendCoroutine)
            {
                if (target < 0 || static_cast<std::size_t>(target) >= coroutines.size() || coroutines[static_cast<std::size_t>(target)].state == CoroutineContext::State::Done)
                {
                    fail("发送消息的协程句柄无效或已结束");
                    return true;
                }
                CoroutineContext &receiver = coroutines[static_cast<std::size_t>(target)];
                receiver.inbox.push_back(std::move(packet));
                if (receiver.state == CoroutineContext::State::Receiving)
                    receiver.state = CoroutineContext::State::Ready;
                return true;
            }
            if (messageReceiveCoroutine)
            {
                CoroutineContext &self = coroutines[currentCoroutine];
                if (self.inbox.empty())
                {
                    const std::size_t next = nextRunnable(currentCoroutine);
                    if (next == coroutines.size())
                        fail("协程接收消息形成死锁");
                    else
                    {
                        pending = suspendedArgs;
                        saveCoroutine(pc, CoroutineContext::State::Receiving);
                        loadCoroutine(next);
                    }
                    return true;
                }
                packet = self.inbox.front();
                if (packet->type != type)
                {
                    fail("协程消息类型不匹配：期望 " + type + "，收到 " + packet->type);
                    return true;
                }
                self.inbox.erase(self.inbox.begin());
            }
            if (receive)
            {
                Slot value;
                if (!restoreTransfer(context, *packet, &value))
                    fail(context.error);
                else if (!returnDest.empty())
                    assign(returnDest, value);
                return true;
            }
        }
#if STACKVM_WITH_THREADS
        if (name.rfind("std::thread::", 0u) == 0u)
        {
            const std::size_t wanted = name == "std::thread::create" || name == "std::thread::wait" ? 1u : name == "std::thread::send" ? 2u :
                                                                                                                                         0u;
            if (name != "std::thread::create" && name != "std::thread::wait" && name != "std::thread::send" && name != "std::thread::receive" && name != "std::thread::current" && name != "std::thread::parent")
                return false;
            if (!threadsEnabled_ || threadRuntime_ == nullptr)
            {
                fail("多线程未启用；解释运行或 C++ 导出时传 --threads");
                pending.clear();
                return true;
            }
            if (nativeCallbackDepth != 0u)
            {
                fail("同步胶水回调中不能执行线程操作");
                pending.clear();
                return true;
            }
            if (pending.size() != wanted)
            {
                fail("线程函数 " + name + " 需要 " + Memory::toString(wanted) + " 个参数");
                pending.clear();
                return true;
            }
            if (name == "std::thread::current" || name == "std::thread::parent")
            {
                pending.clear();
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(
                                                                static_cast<std::int64_t>(name == "std::thread::current" ? threadId_ : parentThreadId_)));
                return true;
            }
            if (name == "std::thread::create")
            {
                if (heapAllocator_ != nullptr && !threadHeapFactory_)
                {
                    fail("自定义 VM 字节堆需要为子线程设置独立堆工厂");
                    pending.clear();
                    return true;
                }
                const std::size_t address = static_cast<std::size_t>(pending[0].value.integer);
                pending.clear();
                if (isObjectHandle(static_cast<std::int64_t>(address)))
                {
                    fail("线程异步入口不支持带捕获的 lambda");
                    return true;
                }
                const Memory::String label = data_ == nullptr ? Memory::String() : program.symbolKey(data_->readCString(address));
                if (address == 0u || program.labels.find(label) == program.labels.end())
                {
                    fail("std::thread::create 收到了无效任务函数指针");
                    return true;
                }
                if (data_ == nullptr)
                {
                    fail("std::thread::create 需要数据区");
                    return true;
                }
                DataArea snapshot = *data_;
                const auto worker = Memory::makeShared<ThreadRuntime::Worker>();
                std::size_t id = 0u;
                std::unique_lock<std::mutex> creationGuard(threadRuntime_->mutex);
                if (threadRuntime_->closing)
                {
                    fail("主线程已经结束，不能再创建线程");
                    return true;
                }
                id = threadRuntime_->nextId++;
                threadRuntime_->workers.emplace(id, worker);
                const auto group = threadRuntime_;
                const auto shared = sharedRuntime_;
                const auto registry = nativeRegistry_;
                const auto heapFactory = threadHeapFactory_;
                auto *const workerAllocator = Memory::currentRuntimeAllocator();
                const std::size_t parentId = threadId_;
                const std::size_t slots = stackLimit_;
                const bool childPanicDetails = panicDetailsEnabled_;
                try
                {
                    worker->host = std::thread([worker, group, shared, registry, heapFactory, &program, label,
                                                snapshot = std::move(snapshot), parentId, id,
                                                slots, stepLimit, workerAllocator, childPanicDetails]() mutable {
                        Memory::AllocationScope allocationScope(workerAllocator);
                        RunResult childResult;
                        try
                        {
                            Machine child;
                            child.setPanicDetailsEnabled(childPanicDetails);
                            Memory::UniquePtr<HeapAllocator> workerHeap = heapFactory ? heapFactory() : nullptr;
                            child.setHeapAllocator(workerHeap.get());
                            child.setThreadHeapFactory(heapFactory);
                            child.setData(&snapshot);
                            child.setNativeRegistry(registry);
                            child.setStackLimit(slots);
                            child.setThreadsEnabled(true);
                            child.threadRuntime_ = group;
                            child.sharedRuntime_ = shared;
                            child.threadId_ = id;
                            child.parentThreadId_ = parentId;
                            child.entryLabel_ = label;
                            childResult = child.run(program, {}, stepLimit);
                        } catch (const std::exception &error)
                        {
                            childResult.error = error.what();
                        } catch (...)
                        {
                            childResult.error = "线程运行时发生未知宿主异常";
                        }
                        {
                            std::lock_guard<std::mutex> guard(group->mutex);
                            worker->result = std::move(childResult);
                            worker->done = true;
                        }
                        group->changed.notify_all();
                    });
                } catch (const std::exception &error)
                {
                    threadRuntime_->workers.erase(id);
                    fail(Memory::String("创建宿主线程失败：") + error.what());
                    return true;
                }
                creationGuard.unlock();
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(id)));
                return true;
            }
            if (name == "std::thread::send")
            {
                const std::int64_t target = pending[0].value.integer;
                NativeContext context;
                context.heap = &heap;
                context.data = data_;
                Memory::String message;
                const bool validText = nativeReadCString(context,
                                                         static_cast<std::size_t>(pending[1].value.integer), &message);
                pending.clear();
                if (!validText)
                {
                    fail("std::thread::send 收到了无效字符串");
                    return true;
                }
                {
                    std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
                    const auto found = target < 0 ? threadRuntime_->workers.end() :
                                                    threadRuntime_->workers.find(static_cast<std::size_t>(target));
                    if (found == threadRuntime_->workers.end() || found->second->done || threadRuntime_->closing)
                    {
                        fail("std::thread::send 收到了无效或已结束的线程句柄");
                        return true;
                    }
                    auto packet = Memory::makeShared<TransferPacket>();
                    packet->type = "string";
                    packet->value.isText = true;
                    packet->value.text = std::move(message);
                    found->second->inbox.push_back(std::move(packet));
                }
                threadRuntime_->changed.notify_all();
                return true;
            }
            if (name == "std::thread::receive")
            {
                pending.clear();
                Memory::String message;
                {
                    std::unique_lock<std::mutex> guard(threadRuntime_->mutex);
                    const auto self = threadRuntime_->workers.at(threadId_);
                    self->receiving = true;
                    threadRuntime_->changed.notify_all();
                    /* 收消息也按"中断优先"：小步等待，每步先交付一轮中断。 */
                    waitWithInterrupts(guard, [&] {
                        return !self->inbox.empty() || threadRuntime_->closing || threadRuntime_->deadlocked();
                    });
                    self->receiving = false;
                    threadRuntime_->changed.notify_all();
                    if (!result.error.empty()) return true;
                    if (self->inbox.empty())
                    {
                        if (threadRuntime_->closing)
                        {
                            fail("线程收件箱已关闭");
                            return true;
                        }
                        /* 死锁只是"谁都等不到"的表象：真有线程先失败，报它的错误，
                         * 否则真正的原因（比如线程里的运行期错误）会被死锁盖掉。 */
                        for (const auto &item : threadRuntime_->workers)
                        {
                            if (item.first == 0u || item.second->reported) continue;
                            ThreadRuntime::Worker &worker = *item.second;
                            if (worker.done && !worker.result.ok && !worker.result.error.empty())
                            {
                                worker.reported = true;
                                failFromThread("线程 " + Memory::toString(item.first) + " 失败：" + worker.result.error, worker.result);
                                return true;
                            }
                        }
                        fail("线程接收消息形成死锁");
                        return true;
                    }
                    if (self->inbox.front()->type != "string")
                    {
                        fail("线程消息类型不匹配：期望 string，收到 " + self->inbox.front()->type);
                        return true;
                    }
                    message = self->inbox.front()->value.text;
                    self->inbox.pop_front();
                }
                NativeContext context;
                context.heap = &heap;
                const std::size_t address = nativeAllocString(context, message);
                if (address == 0u)
                {
                    fail(context.error);
                    return true;
                }
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(address)));
                return true;
            }
            const std::int64_t raw = pending[0].value.integer;
            pending.clear();
            std::shared_ptr<ThreadRuntime::Worker> worker;
            {
                std::unique_lock<std::mutex> guard(threadRuntime_->mutex);
                const auto found = raw <= 0 ? threadRuntime_->workers.end() :
                                              threadRuntime_->workers.find(static_cast<std::size_t>(raw));
                if (found == threadRuntime_->workers.end() || static_cast<std::size_t>(raw) == threadId_ || found->second->joined)
                {
                    fail("std::thread::wait 收到了无效、自身或已等待的线程句柄");
                    return true;
                }
                worker = found->second;
                worker->joined = true;
                const auto self = threadRuntime_->workers.at(threadId_);
                self->joining = true;
                self->joinTarget = static_cast<std::size_t>(raw);
                threadRuntime_->changed.notify_all();
                waitWithInterrupts(guard, [&] {
                    return worker->done || threadRuntime_->closing || (threadRuntime_->deadlocked() && !worker->receiving);
                });
                self->joining = false;
                self->joinTarget = 0u;
                threadRuntime_->changed.notify_all();
                if (!result.error.empty()) return true;
                if (!worker->done)
                {
                    worker->joined = false;
                    fail(threadRuntime_->closing ? "线程等待被主程序结束中断" : "线程等待形成死锁");
                    return true;
                }
            }
            if (worker->host.joinable()) worker->host.join();
            emitText(worker->result.text.data(), worker->result.text.size());
            if (!worker->result.output.empty())
            {
                if (!result.output.empty()) result.output += ' ';
                result.output += worker->result.output;
            }
            worker->reported = true;
            if (!worker->result.ok) failFromThread("线程 " + Memory::toString(raw) + " 失败：" + worker->result.error, worker->result);
            return true;
        }
#endif /* STACKVM_WITH_THREADS */
        const bool coroutineCreate = name == "std::coroutine::create";
        const bool coroutineWait = name == "std::coroutine::wait";
        const bool coroutineYield = name == "std::coroutine::yield" || name == "std::coroutine::yelid";
        const bool coroutineSend = name == "std::coroutine::send";
        const bool coroutineReceive = name == "std::coroutine::receive";
        const bool coroutineIdentity = name == "std::coroutine::current" || name == "std::coroutine::parent";
        if (coroutineCreate || coroutineWait || coroutineYield || coroutineSend || coroutineReceive || coroutineIdentity)
        {
            if (nativeCallbackDepth != 0u && !interruptDelivery)
            {
                fail("同步胶水回调中不能创建、等待或切换协程");
                pending.clear();
                return true;
            }
            const std::size_t wanted = coroutineSend ? 2u :
                                                       (coroutineYield || coroutineReceive || coroutineIdentity ? 0u : 1u);
            if (pending.size() != wanted)
            {
                fail("协程函数 " + name + " 需要 " + Memory::toString(wanted) + " 个参数，实到 " + Memory::toString(pending.size()) + " 个");
                pending.clear();
                return true;
            }
            if (coroutineCreate)
            {
                const std::size_t address = static_cast<std::size_t>(nativeInt(pending[0].value));
                pending.clear();
                if (isObjectHandle(static_cast<std::int64_t>(address)))
                {
                    fail("协程异步入口不支持带捕获的 lambda");
                    return true;
                }
                if (address == 0u || data_ == nullptr)
                {
                    fail("std::coroutine::create 收到了空函数指针");
                    return true;
                }
                const Memory::String label = program.symbolKey(data_->readCString(address));
                const auto target = program.labels.find(program.symbolKey(label));
                if (label.empty() || target == program.labels.end())
                {
                    fail("std::coroutine::create 找不到协程入口：" + label);
                    return true;
                }
                CoroutineContext context;
                context.pc = target->second;
                context.state = CoroutineContext::State::Ready;
                context.parent = currentCoroutine;
                context.frames.push_back(Memory::makeUnique<Frame>());
                prepareFrame(*context.frames.back(), context.pc);
                coroutines.push_back(std::move(context));
                if (!returnDest.empty())
                    assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(coroutines.size() - 1u)));
                return true;
            }

            if (coroutineIdentity)
            {
                pending.clear();
                const std::size_t id = name == "std::coroutine::current" ? currentCoroutine : coroutines[currentCoroutine].parent;
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(id)));
                return true;
            }

            if (coroutineSend)
            {
                const std::int64_t raw = pending[0].value.integer;
                NativeContext context;
                context.heap = &heap;
                context.data = data_;
                Memory::String message;
                const bool validText = nativeReadCString(context,
                                                         static_cast<std::size_t>(pending[1].value.integer), &message);
                pending.clear();
                if (raw < 0 || static_cast<std::size_t>(raw) >= coroutines.size() || coroutines[static_cast<std::size_t>(raw)].state == CoroutineContext::State::Done || !validText)
                {
                    fail("std::coroutine::send 收到了无效句柄或字符串");
                    return true;
                }
                CoroutineContext &target = coroutines[static_cast<std::size_t>(raw)];
                auto packet = Memory::makeShared<TransferPacket>();
                packet->type = "string";
                packet->value.isText = true;
                packet->value.text = std::move(message);
                target.inbox.push_back(std::move(packet));
                if (target.state == CoroutineContext::State::Receiving)
                    target.state = CoroutineContext::State::Ready;
                return true;
            }

            if (coroutineReceive)
            {
                pending.clear();
                CoroutineContext &self = coroutines[currentCoroutine];
                if (self.inbox.empty())
                {
                    const std::size_t next = nextRunnable(currentCoroutine);
                    if (next == coroutines.size())
                    {
                        fail("协程接收消息形成死锁");
                        return true;
                    }
                    saveCoroutine(pc, CoroutineContext::State::Receiving);
                    loadCoroutine(next);
                    return true;
                }
                if (self.inbox.front()->type != "string")
                {
                    fail("协程消息类型不匹配：期望 string，收到 " + self.inbox.front()->type);
                    return true;
                }
                Memory::String message = self.inbox.front()->value.text;
                self.inbox.erase(self.inbox.begin());
                NativeContext context;
                context.heap = &heap;
                const std::size_t address = nativeAllocString(context, message);
                if (address == 0u)
                {
                    fail(context.error);
                    return true;
                }
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(address)));
                return true;
            }

            if (coroutineWait)
            {
                const std::int64_t raw = pending[0].value.integer;
                pending.clear();
                if (raw <= 0 || static_cast<std::size_t>(raw) >= coroutines.size())
                {
                    fail("std::coroutine::wait 收到了无效句柄：" + Memory::toString(raw));
                    return true;
                }
                const std::size_t target = static_cast<std::size_t>(raw);
                if (target == currentCoroutine)
                {
                    fail("协程不能等待自己");
                    return true;
                }
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(0));
                if (coroutines[target].state == CoroutineContext::State::Done) return true;
                coroutines[currentCoroutine].waitingFor = target;
                saveCoroutine(pc + 1u, CoroutineContext::State::Waiting);
                const CoroutineContext::State targetState = coroutines[target].state;
                const std::size_t next =
                    (targetState == CoroutineContext::State::Ready || targetState == CoroutineContext::State::Suspended) ? target : nextRunnable(currentCoroutine);
                if (next == coroutines.size())
                {
                    fail("协程等待形成死锁");
                    loadCoroutine(currentCoroutine);
                    return true;
                }
                loadCoroutine(next);
                return true;
            }

            pending.clear();
            const std::size_t next = nextRunnable(currentCoroutine);
            if (next == coroutines.size())
            {
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(0));
                return true;
            }
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(0));
            saveCoroutine(pc + 1u, CoroutineContext::State::Suspended);
            loadCoroutine(next);
            return true;
        }
        /* 内部 C++ 运行时（字符串 / 容器）：参数与结果都按 Slot 走。 */
        const auto internal = optimizeExecution_ && symbol != absent ? internalEntries[symbol] : kSlotNatives.find(name);
        if (internal == kSlotNatives.end())
        {
            const NativeRegistry::Entry *external = resolveExternal(symbol, name);
            if (external == nullptr) return false;
            if (pending.size() != external->arity)
            {
                fail("宿主函数 " + name + " 需要 " + Memory::toString(external->arity) + " 个参数，实到 " + Memory::toString(pending.size()) + " 个");
                pending.clear();
                return true;
            }
            Memory::Vector<NativeValue> arguments;
            arguments.reserve(pending.size());
            for (const PendingArg &argument : pending)
                arguments.push_back(argument.value.isFloat ? NativeValue::fromFloat(argument.value.real) : NativeValue::fromInt(argument.value.integer));
            pending.clear();

            NativeContext runtime;
            runtime.heap = &heap;
            runtime.data = data_;
            runtime.text = &result.text;
            runtime.textSink = outputSink_;
            runtime.textSinkUser = outputUser_;
            runtime.textKeep = &outputKeepText_;
            runtime.input = &input;
            runtime.inputIndex = &inputIndex;
            runtime.programArguments = &programArguments_;
            runtime.objects = &objects;
            runtime.timers = &timers;
            runtime.machine = this;
            runtime.threadId = threadId_;
            runtime.bigFloatPrecision = &bigFloatPrecision;
            runtime.exitRequested = &exitRequested;
            runtime.exitCode = &exitCode;
            runtime.fileApi = fileApi_;
            NativeCallContextImpl context(runtime, invokeFunction, &result.error);
            const auto callback = external->callback;
            NativeValue value;
#if STACKVM_WITH_HOST_EXCEPTION_GUARD
            try
            {
                value = callback(context, arguments);
            } catch (const std::exception &exception)
            {
                runtime.error = "宿主函数 " + name + " 抛出异常：" + exception.what();
            } catch (...) { runtime.error = "宿主函数 " + name + " 抛出未知异常"; }
#else
            value = callback(context, arguments);
#endif
            if (!runtime.memoryFault.empty()) runtime.error = runtime.memoryFault;
            if (!runtime.error.empty())
            {
                fail(runtime.error);
                return true;
            }
            if (!returnDest.empty())
            {
                if (value.isText())
                {
                    const std::size_t address = nativeAllocString(runtime, value.text);
                    if (!runtime.error.empty())
                    {
                        fail(runtime.error);
                        return true;
                    }
                    assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(address)));
                }
                else
                    assign(returnDest, value.isFloat() ? Slot::fromDouble(value.floating) : Slot::fromInt(value.integer));
            }
            return true;
        }
        return applyInternal(internal, name, returnDest);
    };

    /*
     * 中断交付：宿主投递（`Machine::raiseInterrupt`，可能来自别的线程）与到期的软件定时器
     * 都在**指令边界**调绑定的回调 —— 像硬件 IRQ 那样"异步发生、在安全点交付"。
     *
     * 交付是**队列**语义：一次边界把队列排空，先入先出。同一次触发里排在前面的
     * （先 `raise` 的、先创建的定时器）先执行；处理器运行期间新 `raise` 的追加到队尾，
     * 这一轮继续按顺序执行（不会套娃进处理器，但也不会被推到下一条指令）。
     *
     * 交付用 `invokeFunction`（和胶水层同步回调同一套机制），所以回调里容器、大数、
     * 宿主胶水都能正常用。`interruptDelivery` 让处理器里也能做消息 / 协程 / 共享值
     * 操作：普通胶水同步回调挡这些是因为当时 VM 正停在胶水栈上；中断交付发生在指令
     * 边界、胶水栈是干净的，没有这个限制。
     *
     * 软件定时器的时间源：宿主注入过时钟就用它；没有就用**虚拟时钟**（1 条指令 = 1 个
     * 时间单位），所以解释器和导出程序不接时钟也能得到确定的定时行为。
     */
    std::size_t boundaryCount = 0u;
    bool interruptActive = false;  ///< 队列排空中：不再重入。
    deliverInterrupts = [&]() {
        if (interruptActive) return;
#if STACKVM_WITH_THREADS
        const bool mailboxPosted = interruptMailbox_ != nullptr && interruptMailbox_->posted.load(std::memory_order_relaxed) != 0u;
#else
        /* 板端不编宿主中断邮箱（跨线程才需要）：只剩 VM 自己的软件定时器。 */
        const bool mailboxPosted = false;
#endif
        /* 快路径：没有投递、没有在跑的定时器、队列也空 → 什么都不做。 */
        if (!mailboxPosted && timers.running == 0u && timers.pending.empty()) return;
        ++boundaryCount;
        /* 定时器按 16 条指令一次节流，省掉每条指令读时钟的开销。 */
        const bool checkTimers = timers.running != 0u && (boundaryCount & 0xfu) == 0u;
        const std::uint64_t now = checkTimers ? (timeCallback_ ? timeCallback_() : static_cast<std::uint64_t>(result.steps)) : 0u;
        interruptActive = true;
        std::size_t head = 0u;
        for (;;)
        {
#if STACKVM_WITH_THREADS
            /* 宿主投递：按投递顺序接到队尾（可能边处理边有新投递）。 */
            if (interruptMailbox_ != nullptr && interruptMailbox_->posted.load(std::memory_order_relaxed) != 0u)
            {
                std::lock_guard<std::mutex> guard(interruptMailbox_->mutex);
                timers.pending.insert(timers.pending.end(), interruptMailbox_->queue.begin(),
                                      interruptMailbox_->queue.end());
                interruptMailbox_->queue.clear();
                interruptMailbox_->posted.store(0u, std::memory_order_relaxed);
            }
#endif
            /* 软件定时器到期：一个边界只查一次，按句柄（创建）顺序入队。 */
            if (checkTimers)
            {
                for (std::size_t index = 0; index < timers.slots.size(); ++index)
                {
                    TimerSlot &slot = timers.slots[index];
                    if (!slot.alive || !slot.running || slot.period <= 0) continue;
                    if (slot.thread != threadId_) continue; /* 只跑本线程的定时器 */
                    if (slot.deadline < 0)
                        slot.deadline = static_cast<std::int64_t>(now) + slot.period;
                    if (static_cast<std::uint64_t>(slot.deadline) > now) continue;
                    /* 一次边界至多补一次：错过的周期直接跳过，不让回调一下补跑一串。 */
                    slot.deadline = static_cast<std::int64_t>(now) + slot.period;
                    timers.pending.push_back(
                        InterruptEvent{ static_cast<std::int64_t>(index + 1u), 0, 0, slot.thread });
                }
            }
            if (head >= timers.pending.size()) break;
            const InterruptEvent event = timers.pending[head++];
            /* 归属线程校验：投错线程的中断（宿主句柄 / 别的线程的表）在这里丢掉。 */
            if (event.thread != threadId_) continue;
            /* 函数指针优先取事件自带的（句柄里带的那个），没有才用定时器槽绑定的。 */
            std::int64_t callback = event.callback;
            if (event.timer > 0)
            {
                const std::size_t index = static_cast<std::size_t>(event.timer - 1);
                if (index >= timers.slots.size()) continue; /* 未知句柄 */
                TimerSlot &slot = timers.slots[index];
                if (!slot.alive) continue;                   /* destroy 之后是空操作 */
                if (slot.thread != threadId_) continue;      /* 只跑本线程的定时器 */
                if (callback == 0) callback = slot.callback; /* 句柄没带就用绑定的 */
                if (callback == 0) continue;                 /* 没绑回调 */
                ++slot.ticks;
            }
            else if (callback == 0)
            {
                continue; /* 既没有定时器也没有函数指针：丢弃 */
            }
            NativeValue ignored;
            interruptDelivery = true;
            const bool delivered =
                invokeFunction(callback, { NativeValue::fromInt(event.source) }, ignored);
            interruptDelivery = false;
            /* 处理器里 push 进来的新事件在队列后面；这一轮按顺序继续跑。 */
            if (!delivered || !result.error.empty()) break;
        }
        timers.pending.clear();
        interruptActive = false;
    };

    executeOne = [&]() -> bool {
        if (stopForPanic()) return false;
        if (pc >= program.code.size()) return false;
        /* 下一条指令边界：分配失败在这里被消费成"可恢复终止"。
         * panic 判定（上面那行）与它自己的错误串都排在前面，所以 panic 永远优先。 */
        if (restartPending() && result.error.empty())
        {
            stopForRestart();
            return false;
        }
        deliverInterrupts();
        if (!result.error.empty()) return false;
        if (stepLimit != 0u && result.steps >= stepLimit)
        {
            fail("指令条数超过上限（" + Memory::toString(stepLimit) + "），可能是死循环");
            return false;
        }
        const Instruction &instruction = program.code[pc];
        const std::size_t executedPc = pc;
        const std::size_t executedCoroutine = currentCoroutine;

        // 调试：断点命中或单步模式下，**先回调再执行**（所以断点看到的是"即将执行的那条"）。
        // 没挂调试回调时"停下"没有意义，直接跳过，普通运行零开销。
        if (debugHook_ != nullptr && (singleStep_ || breakpoints_.count(pc) != 0u))
        {
            DebugEvent event;
            event.instruction = pc;
            event.steps = result.steps;
            event.coroutine = currentCoroutine;
            event.opcode = instruction.toString();
            if (debugInfo_ != nullptr)
            {
                if (const SourceLocation *where = debugInfo_->lookup(pc)) event.location = *where;
            }
            event.breakpoint = !singleStep_ || breakpoints_.count(pc) != 0u;
            event.frames = debugFramesOf(pc);
            currentLocation_ = event.location;
            debugFrames_ = event.frames;
            const DebugAction action = debugHook_(event);
            if (action == DebugAction::Abort)
            {
                fail("调试器中止在 " + event.location.toString() + "（第 " + Memory::toString(pc) + " 条指令）");
                return false;
            }
            singleStep_ = action == DebugAction::StepInto;
        }
#include "register_execution.inc"
        boundRegion = nullptr;
#include "execution_fusion.inc"
        ++result.steps;
        result.maxStackDepth = std::max(result.maxStackDepth, stack.size());
        if (trace_)
        {
            result.trace.push_back("[" + Memory::toString(pc) + "] " + instruction.toString());
        }

        // 所有压栈统一经过容量检查；上限按协程分别计算。
        auto push = [&](Slot value) -> bool {
            if (stackLimit_ != 0u && stack.size() >= stackLimit_)
            {
                fail("求值栈超过上限（" + Memory::toString(stackLimit_) + " 个槽，第 " + Memory::toString(pc) + " 条指令 " + instruction.toString() + "）");
                return false;
            }
            stack.push_back(std::move(value));
            result.maxStackDepth = std::max(result.maxStackDepth, stack.size());
            return true;
        };

        // 需要弹栈的指令统一在这里取。
        auto pop = [&](Slot &out) -> bool {
            if (stack.empty())
            {
                fail("求值栈为空（第 " + Memory::toString(pc) + " 条指令 " + instruction.toString() + "）");
                return false;
            }
            out = stack.back();
            stack.pop_back();
            return true;
        };
        jumped = false;
        switch (instruction.op)
        {
        case Op::PushInt: push(Slot::fromInt(instruction.intValue)); break;
        case Op::PushDouble: push(Slot::fromDouble(instruction.doubleValue)); break;
        case Op::PushText: push(Slot::fromText(instruction.textValue)); break;
        case Op::Load:
        {
            const Slot *slot = readPrepared(pc);
            if (slot == nullptr)
            {
                fail("读未定义的变量：" + instruction.name);
                break;
            }
            push(*slot);
            break;
        }
        case Op::Store:
        {
            Slot slot;
            if (!pop(slot)) break;
            storePrepared(pc, slot);
            break;
        }
        case Op::Declare:
        {
            if (optimizeExecution_)
            {
                const auto &prepared = plan.code[pc];
                const auto symbol = prepared.symbol;
                const auto zero = plan.types[symbol].type && plan.types[symbol].type->isFloat ? Slot::fromDouble(0) : Slot::fromInt(0);
                if (!frames.empty())
                {
                    auto &frame = *frames.back();
                    const auto slot = frame.layout->slots.find(symbol);
                    if (slot != frame.layout->slots.end() && !frame.bound[slot->second])
                    {
                        frame.slots[slot->second] = zero;
                        frame.bound[slot->second] = true;
                    }
                }
                else if (!globalBound[symbol])
                {
                    globalSlots[symbol] = zero;
                    globalBound[symbol] = true;
                }
            }
            else
            {
                auto &scope = frames.empty() ? globals : frames.back()->locals;
                if (scope.find(instruction.name) == scope.end())
                {
                    const auto type = program.types.find(instruction.name);
                    scope[instruction.name] = type != program.types.end() && type->second.isFloat ? Slot::fromDouble(0) : Slot::fromInt(0);
                }
            }
            break;
        }
        case Op::Neg:
        {
            Slot slot;
            if (!pop(slot)) break;
            if (slot.isFloat || instruction.isFloat)
                push(Slot::fromDouble(
                    normalizeFloat(-slot.number(), static_cast<int>(instruction.intValue))));
            else
            {
                TypeInfo type;
                type.bits = static_cast<int>(instruction.intValue);
                type.isUnsigned = instruction.unsignedOp;
                push(Slot::fromInt(normalizeInt(-slot.integer, type)));
            }
            break;
        }
        case Op::Not:
        {
            Slot slot;
            if (!pop(slot)) break;
            TypeInfo type;
            type.bits = static_cast<int>(instruction.intValue);
            type.isUnsigned = instruction.unsignedOp;
            push(Slot::fromInt(normalizeInt(~slot.integer, type)));
            break;
        }
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Mod:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            const bool useFloat = left.isFloat || right.isFloat;
            if (useFloat)
            {
                const double a = left.number();
                const double b = right.number();
                if ((instruction.op == Op::Div || instruction.op == Op::Mod) && b == 0.0)
                {
                    fail("浮点除零");
                    break;
                }
                double value = 0.0;
                switch (instruction.op)
                {
                case Op::Add: value = a + b; break;
                case Op::Sub: value = a - b; break;
                case Op::Mul: value = a * b; break;
                case Op::Div: value = a / b; break;
                default: value = std::fmod(a, b); break;
                }
                push(Slot::fromDouble(
                    normalizeFloat(value, static_cast<int>(instruction.intValue))));
                break;
            }
            const std::int64_t a = left.integer;
            const std::int64_t b = right.integer;
            if ((instruction.op == Op::Div || instruction.op == Op::Mod) && b == 0)
            {
                fail("整数除零");
                break;
            }
            std::int64_t value = 0;
            if (instruction.unsignedOp && (instruction.op == Op::Div || instruction.op == Op::Mod))
            {
                const std::uint64_t ua = static_cast<std::uint64_t>(a);
                const std::uint64_t ub = static_cast<std::uint64_t>(b);
                value = static_cast<std::int64_t>(instruction.op == Op::Div ? ua / ub : ua % ub);
            }
            else
            {
                switch (instruction.op)
                {
                case Op::Add: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b)); break;
                case Op::Sub: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)); break;
                case Op::Mul: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b)); break;
                case Op::Div: value = (a == INT64_MIN && b == -1) ? a : a / b; break;
                default: value = (a == INT64_MIN && b == -1) ? 0 : a % b; break;
                }
            }
            TypeInfo type;
            type.bits = static_cast<int>(instruction.intValue);
            type.isUnsigned = instruction.unsignedOp;
            push(Slot::fromInt(normalizeInt(value, type)));
            break;
        }
        case Op::Shl:
        case Op::Shr:
        case Op::ShrU:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            const int shift = static_cast<int>(right.integer & 63);
            const auto bits = static_cast<std::uint64_t>(left.integer);
            const auto value = instruction.op == Op::Shl ? static_cast<std::int64_t>(bits << shift) :
                               instruction.op == Op::Shr ? (left.integer >> shift) :
                                                           static_cast<std::int64_t>(bits >> shift);
            TypeInfo type;
            type.bits = static_cast<int>(instruction.intValue);
            type.isUnsigned = instruction.unsignedOp;
            push(Slot::fromInt(normalizeInt(value, type)));
            break;
        }
        case Op::BitAnd:
        case Op::BitOr:
        case Op::BitXor:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            std::int64_t value = 0;
            if (instruction.op == Op::BitAnd)
                value = left.integer & right.integer;
            else if (instruction.op == Op::BitOr)
                value = left.integer | right.integer;
            else
                value = left.integer ^ right.integer;
            push(Slot::fromInt(value));
            break;
        }
        case Op::Eq:
        case Op::Ne:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge:
        case Op::LtU:
        case Op::LeU:
        case Op::GtU:
        case Op::GeU:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            bool value = false;
            if (instruction.op == Op::Eq || instruction.op == Op::Ne)
            {
                const bool equal = (left.isFloat || right.isFloat) ? (left.number() == right.number()) : (left.integer == right.integer);
                value = (instruction.op == Op::Eq) ? equal : !equal;
            }
            else
            {
                const bool unsignedCompare = (instruction.op == Op::LtU) || (instruction.op == Op::LeU) || (instruction.op == Op::GtU) || (instruction.op == Op::GeU);
                const double a = left.number();
                const double b = right.number();
                if (unsignedCompare)
                {
                    const std::uint64_t ua = static_cast<std::uint64_t>(left.integer);
                    const std::uint64_t ub = static_cast<std::uint64_t>(right.integer);
                    if (instruction.op == Op::LtU)
                        value = ua < ub;
                    else if (instruction.op == Op::LeU)
                        value = ua <= ub;
                    else if (instruction.op == Op::GtU)
                        value = ua > ub;
                    else
                        value = ua >= ub;
                }
                else if (!left.isFloat && !right.isFloat)
                {
                    if (instruction.op == Op::Lt)
                        value = left.integer < right.integer;
                    else if (instruction.op == Op::Le)
                        value = left.integer <= right.integer;
                    else if (instruction.op == Op::Gt)
                        value = left.integer > right.integer;
                    else
                        value = left.integer >= right.integer;
                }
                else
                {
                    if (instruction.op == Op::Lt)
                        value = a < b;
                    else if (instruction.op == Op::Le)
                        value = a <= b;
                    else if (instruction.op == Op::Gt)
                        value = a > b;
                    else
                        value = a >= b;
                }
            }
            push(Slot::fromInt(value ? 1 : 0));
            break;
        }
        case Op::ToDouble:
        {
            Slot slot;
            if (!pop(slot)) break;
            push(Slot::fromDouble(
                normalizeFloat(!slot.isFloat && instruction.unsignedOp ? static_cast<double>(static_cast<std::uint64_t>(slot.integer)) : slot.number(), static_cast<int>(instruction.intValue))));
            break;
        }
        case Op::ToInt:
        {
            Slot slot;
            if (!pop(slot)) break;
            TypeInfo type;
            type.bits = static_cast<int>(instruction.intValue);
            type.isUnsigned = instruction.unsignedOp;
            push(Slot::fromInt(
                normalizeInt(slot.isFloat ? static_cast<std::int64_t>(nativeInt(slot)) : slot.integer, type)));
            break;
        }
        case Op::MulWide:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            const int bits = (instruction.intValue > 0 && instruction.intValue <= 64) ? static_cast<int>(instruction.intValue) : 32;
            const std::uint64_t mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
            const std::uint64_t a = static_cast<std::uint64_t>(left.integer) & mask;
            const std::uint64_t b = static_cast<std::uint64_t>(right.integer) & mask;
            const std::uint64_t product = a * b;
            push(Slot::fromInt(static_cast<std::int64_t>(product & mask)));            // lo
            push(Slot::fromInt(static_cast<std::int64_t>((product >> bits) & mask)));  // hi
            break;
        }
        case Op::DivWide:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            const int bits = (instruction.intValue > 0 && instruction.intValue <= 64) ? static_cast<int>(instruction.intValue) : 32;
            const std::uint64_t mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
            const std::uint64_t ua = static_cast<std::uint64_t>(left.integer) & mask;
            const std::uint64_t ub = static_cast<std::uint64_t>(right.integer) & mask;

            std::int64_t quotient = 0;
            std::int64_t remainder = 0;
            if (instruction.unsignedOp)
            {
                if (ub == 0u)
                {
                    fail("双字除法除零");
                    break;
                }
                quotient = static_cast<std::int64_t>(ua / ub);
                remainder = static_cast<std::int64_t>(ua % ub);
            }
            else
            {
                TypeInfo signedType = typeOf("i32");
                signedType.bits = bits;
                signedType.isUnsigned = false;
                signedType.isFloat = false;
                const std::int64_t sa = normalizeInt(static_cast<std::int64_t>(ua), signedType);
                const std::int64_t sb = normalizeInt(static_cast<std::int64_t>(ub), signedType);
                if (sb == 0)
                {
                    fail("双字除法除零");
                    break;
                }
                quotient = sa / sb;
                remainder = sa % sb;
            }
            push(Slot::fromInt(quotient));   // q
            push(Slot::fromInt(remainder));  // r
            break;
        }
        case Op::Jmp:
        {
            const auto target = plan.code[pc].target;
            if (target == absent)
            {
                fail("跳转目标不存在：" + instruction.name);
                break;
            }
            pc = target;
            jumped = true;
            break;
        }
        case Op::Jz:
        case Op::Jnz:
        {
            Slot slot;
            if (!pop(slot)) break;
            const bool zero = (slot.number() == 0.0);
            const bool take = (instruction.op == Op::Jz) ? zero : !zero;
            if (take)
            {
                const auto target = plan.code[pc].target;
                if (target == absent)
                {
                    fail("跳转目标不存在：" + instruction.name);
                    break;
                }
                pc = target;
                jumped = true;
            }
            break;
        }
        case Op::Print:
        {
            Slot slot;
            if (!pop(slot)) break;
            switch (instruction.format)
            {
            case PrintFormat::Unsigned:
                emitNumber(Memory::toString(static_cast<std::uint64_t>(slot.integer)));
                break;
            case PrintFormat::Float:
                emitNumber(formatDouble(slot.number()));
                break;
            case PrintFormat::Text:
                emitText(slot.text.data(), slot.text.size());
                break;
            default:
                emitNumber(Memory::toString(static_cast<std::int64_t>(nativeInt(slot))));
                break;
            }
            break;
        }
        case Op::PrintText:
            emitText(instruction.textValue.data(), instruction.textValue.size());
            break;
        case Op::Putc:
        {
            Slot slot;
            if (!pop(slot)) break;
            const char byte = static_cast<char>(static_cast<int64_t>(nativeInt(slot)) & 0xFF);
            emitText(&byte, 1u);
            break;
        }
        case Op::Read:
        case Op::Getc:
        {
            if (inputIndex >= input.size())
            {
                fail("输入不够（第 " + Memory::toString(inputIndex + 1u) + " 项缺失）");
                break;
            }
            const Memory::String &item = input[inputIndex++];
            if (instruction.op == Op::Getc)
            {
                const int code = item.empty() ? 0 : static_cast<unsigned char>(item[0]);
                assign(instruction.name, Slot::fromInt(code));
                break;
            }
            const TypeInfo info = [&] {
                const auto found = program.types.find(instruction.name);
                return (found == program.types.end()) ? typeOf("") : found->second;
            }();
            bool isFloat = false;
            std::int64_t intValue = 0;
            double doubleValue = 0.0;
            if (!parseNumber(item, isFloat, intValue, doubleValue))
            {
                fail("输入 '" + item + "' 不是数字（变量 " + instruction.name + "）");
                break;
            }
            if (info.isFloat)
                assign(instruction.name, Slot::fromDouble(normalizeFloat(
                                             isFloat ? doubleValue : static_cast<double>(intValue), info.bits)));
            else
                assign(instruction.name, Slot::fromInt(isFloat ? static_cast<std::int64_t>(doubleValue) : intValue));
            break;
        }
        case Op::Param:
        {
            Slot slot;
            if (!pop(slot)) break;
            pending.push_back(PendingArg{ instruction.name, slot });
            break;
        }
        case Op::Alloc:
        {
            Slot size;
            if (!pop(size)) break;
            const std::int64_t bytes = size.isFloat ? 0 : size.integer;
            if (bytes <= 0)
            {
                fail("alloc 的字节数必须是正数（拿到 " + Memory::toString(bytes) + "）");
                break;
            }
            if (static_cast<std::uint64_t>(bytes) > std::numeric_limits<std::size_t>::max())
            {
                fail("alloc 的字节数超出地址空间");
                break;
            }
            const std::size_t address = heap.allocate(static_cast<std::size_t>(bytes));
            if (address == 0u)
            {
                fail("堆内存不足：申请 " + Memory::toString(bytes) + " 字节，当前在用 " + Memory::toString(heap.liveBytes()) + " 字节");
                break;
            }
            objects.beginStorage(address, static_cast<std::size_t>(bytes), true);
            push(Slot::fromInt(static_cast<std::int64_t>(address)));
            break;
        }
        case Op::Free:
        {
            Slot address;
            if (!pop(address)) break;
            /* 句柄区间（`bint` / 容器）走对象表；其余按堆地址严格释放。 */
            const std::int64_t handle = static_cast<std::int64_t>(nativeInt(address));
            if (trace_) result.trace.push_back("    release " + Memory::toString(handle));
            if (isObjectHandle(handle))
            {
                if (!objects.find(handle) && handle < objects.next) break;
                if (!objects.release(handle))
                {
                    fail("free 失败：这个对象已经释放过了（句柄 " + Memory::toString(handle) + "）");
                }
                break;
            }
            Memory::String reason;
            std::size_t capacity = 0;
            const auto target = static_cast<std::size_t>(nativeInt(address));
            if (heap.capacityAt(target, capacity)) objects.beginStorage(target, capacity, true);
            if (!heap.release(target, &reason))
            {
                fail("free 失败：" + reason);
                break;
            }
            break;
        }
        case Op::Drop:
        {
            /* **软释放**（编译器在语句块出口插的那一条）：只有"地址确实是一块活跃的
             * 堆内存"才还回去。0（空指针）、数据区地址（字符串字面量）、以及**已经还过**
             * 的地址都当没这回事 —— 这就是"作用域所有"能安全重入的原因：
             * 显式 `delete` 之后变量被置 0，块出口再 drop 一次是空操作；
             * 两个变量指向同一块堆内存时（别名），多出来的那次 drop 也不会炸。 */
            Slot address;
            if (!pop(address)) break;
            if (trace_) result.trace.push_back("    drop " + Memory::toString(static_cast<std::int64_t>(nativeInt(address))));
            /* 对象句柄（`bint` / 容器）也认：还在就还回去，已经还过就当没这回事
             * （和后面堆那条一个脾气）。 */
            const std::int64_t handle = static_cast<std::int64_t>(nativeInt(address));
            if (isObjectHandle(handle))
            {
                objects.release(handle);
                break;
            }
            const std::size_t target = static_cast<std::size_t>(nativeInt(address));
            if (target == 0u || !inHeap(target)) break;
            Memory::String reason;
            std::size_t capacity = 0;
            if (heap.capacityAt(target, capacity)) objects.beginStorage(target, capacity, true);
            heap.release(target, &reason); /* 已释放分配再次回收无效果。 */
            break;
        }
        case Op::Check:
        {
            // `(check, 下标, 上限, 标签)`：弹栈取上限与下标，越界就终止运行。
            Slot limit;
            Slot index;
            if (!pop(limit) || !pop(index)) break;
            const std::int64_t position = static_cast<std::int64_t>(nativeInt(index));
            const std::int64_t length = static_cast<std::int64_t>(nativeInt(limit));
            if (position < 0 || position >= length)
            {
                fail("数组下标越界：" + (instruction.name.empty() ? Memory::String("数组") : instruction.name) + " 的下标 " + Memory::toString(position) + "，长度 " + Memory::toString(length));
                break;
            }
            break;
        }
        case Op::CheckPointer:
        {
            // `(checkptr, 下标, 基址, 步长, 标签)`：按"基址所在那一块"的容量查这个元素。
            Slot base;
            Slot index;
            if (!pop(base) || !pop(index)) break;
            const std::int64_t position = static_cast<std::int64_t>(nativeInt(index));
            const std::int64_t stride =
                (instruction.intValue > 0) ? instruction.intValue : 8;
            const std::size_t address = static_cast<std::size_t>(nativeInt(base));
            const Memory::String label =
                instruction.name.empty() ? Memory::String("这一块") : instruction.name;

            // 1. 下标不能为负。
            if (position < 0)
            {
                fail("数组下标越界：" + label + " 的下标 " + Memory::toString(position) + "（不能为负）");
                break;
            }
            // 2. 找出基址落在哪一块里（堆块按分配大小；数据区按符号 size=）。
            std::size_t capacity = 0;
            bool found = false;
            if (inHeap(address))
            {
                found = heap.capacityAt(address, capacity);  // 没分配 / 已经 free → false
            }
            else if (data_ != nullptr)
            {
                found = dataCapacity.capacityAt(address, capacity);
            }
            if (!found || capacity == 0u)
            {
                fail("panic：数组下标检查失败：" + label + " 的地址 " + Memory::toString(address) + " 不在任何一块已分配内存里（没分配、已经 free，或者不是数组的起始地址）");
                break;
            }
            // 3. 这个元素（基址 + 下标 × 步长，共 stride 字节）必须整块落在容量里。
            if (capacity < static_cast<std::size_t>(stride) || static_cast<std::uint64_t>(position) >= capacity / static_cast<std::size_t>(stride))
            {
                fail("数组下标越界：" + label + " 的下标 " + Memory::toString(position) + "（这一块只有 " + Memory::toString(capacity) + " 字节，元素 " + Memory::toString(stride) + " 字节）");
                break;
            }
            break;
        }
        case Op::Call:
        {
            if (entryPending && frames.empty() && instruction.name == program.symbolKey("main"))
            {
                entryPending = false;
                if (!enterFunction(entryLabel_, instruction.name2, false)) break;
                break;
            }
            if (callNative(instruction.name, instruction.name2)) break;
            if (!enterFunction(instruction.name, instruction.name2, false)) break;
            break;
        }
        case Op::CallIndirect:
        {
            /* 函数指针：值就是"数据区里存着函数标签的那串字节"的地址（0 = 空指针）。 */
            Slot pointer;
            if (!pop(pointer)) break;
            const std::size_t address = static_cast<std::size_t>(nativeInt(pointer));
            if (address == 0u)
            {
                fail("调用了空函数指针（null）");
                pending.clear();
                break;
            }
            if (data_ == nullptr)
            {
                fail("没有加载数据区，无法通过函数指针取函数名");
                pending.clear();
                break;
            }
            std::int64_t environment = 0;
            const auto target = closureTarget(objects, static_cast<std::int64_t>(address), environment);
            const Memory::String label = target ? program.symbolKey(data_->readCString(static_cast<std::size_t>(target))) : Memory::String();
            if (label.empty())
            {
                fail("函数指针指向的地址 " + Memory::toString(address) + " 上没有函数名");
                pending.clear();
                break;
            }
            if (environment) pending.push_back(PendingArg{ "__magel_environment", Slot::fromInt(environment) });
            if (callNative(label, instruction.name2)) break;
            if (!enterFunction(label, instruction.name2, true)) break;
            break;
        }
        case Op::Ret:
        {
            Slot value;
            if (!pop(value)) break;
            if (currentCoroutine != 0u && frames.size() == 1u)
            {
                frames.clear();
                stack.clear();
                pending.clear();
                coroutines[currentCoroutine].state = CoroutineContext::State::Done;
                std::size_t next = coroutines.size();
                for (std::size_t index = 0; index < coroutines.size(); ++index)
                {
                    CoroutineContext &candidate = coroutines[index];
                    if (candidate.state == CoroutineContext::State::Waiting && candidate.waitingFor == currentCoroutine)
                    {
                        candidate.state = CoroutineContext::State::Ready;
                        candidate.waitingFor = 0u;
                        if (next == coroutines.size()) next = index;
                    }
                }
                if (next == coroutines.size()) next = nextRunnable(currentCoroutine);
                if (next == coroutines.size())
                {
                    result.ok = true;
                    pc = program.code.size();
                    jumped = true;
                    break;
                }
                loadCoroutine(next);
                break;
            }
            if (frames.empty())
            {
                pc = program.code.size();
                jumped = true;
                break;
            }
            // 返回时先摘下本帧并写回返回值，再销毁局部值；小帧仅缓存空存储。
            // 活动递归层始终拥有各自的槽位，不会被缓存复用。
            Memory::UniquePtr<Frame> frame = std::move(frames.back());
            frames.pop_back();
            if (!frame->returnDest.empty()) assign(frame->returnDest, value);
            pc = frame->returnIp;
            if (optimizeExecution_ && registerExecution_ && registerServices_ && returnedFrames.size() < 8 && frame->slots.size() <= 256)
            {
                // 清空所有值及声明状态，保留容量；文本等宿主资源仍在函数返回时释放。
                frame->locals.clear();
                frame->slots.clear();
                frame->bound.clear();
                frame->returnDest.clear();
#if STACKVM_WITH_PANIC_TRACE
                frame->function.clear();
#endif
                frame->layout = nullptr;
                returnedFrames.push_back(std::move(frame));
            }
            jumped = true;
            break;
        }
        case Op::Halt:
            result.ok = true;
            pc = program.code.size();
            jumped = true;
            break;
        case Op::AddrOf:
        {
            if (data_ == nullptr)
            {
                fail("没有加载数据区，无法取符号地址：" + instruction.name);
                break;
            }
            const auto &symbolTable = program.compactSymbols ? dataSymbols : data_->symbols;
            const auto found = symbolTable.find(instruction.name);
            if (found == symbolTable.end())
            {
                fail("数据区里没有符号：" + instruction.name);
                break;
            }
            push(Slot::fromInt(static_cast<std::int64_t>(found->second)));
            break;
        }
        case Op::LoadAt:
        {
            Slot address;
            if (!pop(address)) break;
            const std::size_t target = static_cast<std::size_t>(nativeInt(address));
            if (data_ == nullptr && !inHeap(target))
            {
                fail("panic：没有加载数据区，无法按地址取值：地址 " + Memory::toString(target));
                break;
            }
            Slot value;
            if (!loadMemory(target, instruction.isFloat, value))
            {
                const Memory::String where = inHeap(target) ? "堆" : "数据区";
                fail("panic：" + where + "越界读：地址 " + Memory::toString(target) + (inHeap(target) ? "（没分配或已经 free 过）" : ""));
                break;
            }
            push(value);
            break;
        }
        case Op::StoreAt:
        {
            Slot value;
            Slot address;
            if (!pop(value) || !pop(address)) break;
            const std::size_t target = static_cast<std::size_t>(nativeInt(address));
            if (data_ == nullptr && !inHeap(target))
            {
                fail("panic：没有加载数据区，无法按地址写值：地址 " + Memory::toString(target));
                break;
            }
            if (!storeMemory(target, instruction.isFloat, value))
            {
                const Memory::String where = inHeap(target) ? "堆" : "数据区";
                fail("panic：" + where + "越界写：地址 " + Memory::toString(target) + (inHeap(target) ? "（没分配或已经 free 过）" : ""));
                break;
            }
            break;
        }
        }

        /* 交付中断时不跑指令钩子：钩子里的宿主泵（Qt 事件循环）可能再 raise 中断，
         * 那会在"处理器 → 钩子 → 新中断 → 处理器"之间无限套娃。ISR 里只跑回调本身。 */
        if (hookInterval_ != 0u && instructionHook_ && !interruptDelivery && result.steps % hookInterval_ == 0u)
        {
            const Memory::String text = instruction.toString();
            const std::size_t separator = text.find(' ');
            instructionHook_(InstructionHookInfo{
                result.steps, executedPc, executedCoroutine, text.substr(0u, separator) });
        }
        if (!result.error.empty()) return false;
        if (!jumped) ++pc;
        return pc < program.code.size();
    };

    while (executeOne()) {}

#if STACKVM_WITH_THREADS
    if (threadId_ == 0u && threadRuntime_ != nullptr)
    {
        {
            std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
            threadRuntime_->closing = true;
        }
        threadRuntime_->changed.notify_all();
        for (;;)
        {
            std::shared_ptr<ThreadRuntime::Worker> worker;
            std::size_t id = 0u;
            {
                std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
                for (const auto &item : threadRuntime_->workers)
                {
                    if (item.first == 0u || item.second->joined) continue;
                    id = item.first;
                    worker = item.second;
                    worker->joined = true;
                    break;
                }
            }
            if (worker == nullptr) break;
            if (worker->host.joinable()) worker->host.join();
            if (!worker->reported)
            {
                emitText(worker->result.text.data(), worker->result.text.size());
                if (!worker->result.output.empty())
                {
                    if (!result.output.empty()) result.output += ' ';
                    result.output += worker->result.output;
                }
                if (!worker->result.ok && result.error.empty())
                    failFromThread("线程 " + Memory::toString(id) + " 失败：" + worker->result.error, worker->result);
            }
        }
        stopForPanic();
        threadRuntime_.reset();
    }
#else
    /* 板端没有线程运行时：线程收尾（关闭 / join / 合并子线程输出）整段不存在。 */
#endif /* STACKVM_WITH_THREADS */

    stopForPanic();
    /* 兜住"失败发生在最后一条指令之后 / 循环外"的场合：失败点自己 fail() 过（error 非空）
     * 也算可恢复终止，除非那是 panic —— panic 绝不能走软重启。 */
    if (restartPending() && !result.recoverable && result.error.rfind("panic", 0u) != 0u)
        stopForRestart();
    if (result.error.empty() && !result.ok && !result.recoverable)
    {
        // 掉出程序尾也算正常结束（组装时补过 Halt，这里兜底）。
        result.ok = true;
    }
    for (const std::int64_t handle : objects.materializedObjects) objects.release(handle);
    for (const std::size_t address : objects.materializedHeap)
    {
        Memory::String ignored;
        heap.release(address, &ignored);
    }
    // 堆统计：停机时还有多少字节没 free —— 非 0 就是漏了，交给调用方决定报不报。
    result.heapPeakBytes = heap.peakBytes();
    result.heapLiveBytes = heap.liveBytes();
    result.heapLiveBlocks = heap.liveBlocks();
    result.liveObjects = objects.liveCount();
    if (result.timingAvailable)
    {
        result.finishedNanoseconds = timeCallback_();
        result.elapsedNanoseconds = result.finishedNanoseconds >= result.startedNanoseconds ? result.finishedNanoseconds - result.startedNanoseconds : 0u;
    }
    coroutines[currentCoroutine].pc = pc;
    // 收尾：把最后的位置与调用栈留成快照（出错时 fail() 已经填过，这里补成功停机的场合）。
    if (debugInfo_ != nullptr && result.ok && result.sourceTrace.empty())
    {
        if (const SourceLocation *where = debugInfo_->lookup(pc)) result.failureLocation = *where;
        result.sourceTrace = debugFramesOf(pc);
        currentLocation_ = result.failureLocation;
        debugFrames_ = result.sourceTrace;
    }
    coroutines[currentCoroutine].state = result.ok ? CoroutineContext::State::Done : coroutines[currentCoroutine].state;
    coroutines[currentCoroutine].stack = std::move(stack);
    coroutines[currentCoroutine].frames = std::move(frames);
    coroutines[currentCoroutine].pending = std::move(pending);
    const auto stateName = [](CoroutineContext::State state) {
        switch (state)
        {
        case CoroutineContext::State::Running: return "running";
        case CoroutineContext::State::Ready: return "ready";
        case CoroutineContext::State::Suspended: return "suspended";
        case CoroutineContext::State::Waiting: return "waiting";
        case CoroutineContext::State::Receiving: return "receiving";
        case CoroutineContext::State::Done: return "done";
        }
        return "unknown";
    };
    for (std::size_t index = 0; index < coroutines.size(); ++index)
    {
        const CoroutineContext &context = coroutines[index];
        result.coroutines.push_back(CoroutineInfo{ index, stateName(context.state), context.pc,
                                                   context.frames.size(), context.stack.size(),
                                                   context.waitingFor });
    }
    // 本轮全部执行上下文已停下；重启前撤销释放身份，下一轮 reset 再清空物理槽。
    if (result.recoverable) heap.sweep();
    return result;
}
