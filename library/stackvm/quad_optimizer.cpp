/**
 * @file quad_optimizer.cpp
 * @brief 四元式优化流程与标准迭代预算。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
// ---------------------------------------------------------------------------
//  执行
// ---------------------------------------------------------------------------

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
            // 函数参数恰好位于前一函数清理之后；污染 local 会导致 参数清理遗漏。
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
                        remember(quad);
                        folded = true;
                    }
                    else if (!leftIsFloat || floatFitsInteger(leftDouble, type))
                    {
                        const std::int64_t value = leftIsFloat ? static_cast<std::int64_t>(leftDouble) : leftInt;
                        quad.op = ":=";
                        quad.args = { Memory::toString(normalizeInt(
                                          value, type)),
                                      "_", quad.arg(2) };
                        remember(quad);
                        folded = true;
                    }
                    /* 浮点转整数越界：C++ 的 static_cast 是未定义行为，这里不折叠，
                     * 保留 cvt 交给运行期的范围检查报错。 */
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
}  // namespace stackvm
