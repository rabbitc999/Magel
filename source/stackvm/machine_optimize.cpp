/**
 * @file machine_optimize.cpp
 * @brief 机器级优化遍：强度削减、常量折叠与传播、公共子表达式、死代码消除、循环不变量外提。
 */
#include "machine_optimize.hpp"
#include "runtime_internal.hpp"
#include <algorithm>
#include <cstdio>

namespace stackvm
{
namespace machine
{
using namespace runtimeDetail;
namespace
{
constexpr std::size_t absent = static_cast<std::size_t>(-1);

/**
 * @brief 进程内累计统计，只用于 `--stats`。
 * @return 统计引用。
 */
Stats &accumulated()
{
    static Stats stats;
    return stats;
}

/**
 * @brief 是否是"算得出结果、不带副作用"的二元运算。
 * @param base 去掉类型后缀的操作符。
 * @return 是返回 true。
 */
bool pureBinary(const Memory::String &base)
{
    return base == "+" || base == "-" || base == "*" || base == "&" || base == "|" || base == "^" || base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=";
}

/**
 * @brief 是否会在运行期失败（除零、除法溢出、移位越界），这类运算不折叠也不外提。
 * @param base 基础操作符。
 * @return 会失败返回 true。
 */
bool trapping(const Memory::String &base)
{
    return base == "/" || base == "%" || base == "<<" || base == ">>";
}

/**
 * @brief 是否有结果写回、需要计入"定义次数"。
 * @param base 基础操作符。
 * @return 有结果返回 true。
 */
bool writesResult(const Memory::String &base)
{
    static const Memory::Map<Memory::String, bool> kTable = {
        { ":=", true },
        { "neg", true },
        { "~", true },
        { "cvt", true },
        { "+", true },
        { "-", true },
        { "*", true },
        { "/", true },
        { "%", true },
        { "&", true },
        { "|", true },
        { "^", true },
        { "<<", true },
        { ">>", true },
        { "==", true },
        { "!=", true },
        { "<", true },
        { "<=", true },
        { ">", true },
        { ">=", true },
        { "mul", true },
        { "div", true },
        { "addr", true },
        { "alloc", true },
        { "load", true },
        { "call", true },
        { "callptr", true },
        { "read", true },
        { "getc", true },
        { "decl", true },
    };
    return kTable.find(base) != kTable.end();
}

/**
 * @brief 解析字面量操作数。
 * @param text 操作数原文。
 * @param value 输出的整数值。
 * @return 是整数（或可精确表示的浮点）字面量返回 true。
 */
bool integerLiteral(const Memory::String &text, std::int64_t &value)
{
    bool isFloat = false;
    std::int64_t integer = 0;
    double real = 0.0;
    if (text.empty() || !parseNumber(text, isFloat, integer, real)) return false;
    if (isFloat) return false;
    value = integer;
    return true;
}

/**
 * @brief 按操作符与类型折叠两个整数字面量。
 * @param base 基础操作符。
 * @param type 运算类型。
 * @param a 左值。
 * @param b 右值。
 * @param out 结果。
 * @return 可以折叠（且不会改变陷阱行为）返回 true。
 */
bool foldBinary(const Memory::String &base, const TypeInfo &type, std::int64_t a, std::int64_t b, std::int64_t &out)
{
    if (type.isFloat) return false;
    if (base == "+")
    {
        out = normalizeInt(static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b)), type);
        return true;
    }
    if (base == "-")
    {
        out = normalizeInt(static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)), type);
        return true;
    }
    if (base == "*")
    {
        out = normalizeInt(static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b)), type);
        return true;
    }
    if (base == "&" || base == "|" || base == "^")
    {
        const std::int64_t value = base == "&" ? (a & b) : base == "|" ? (a | b) :
                                                                         (a ^ b);
        out = normalizeInt(value, type);
        return true;
    }
    if (base == "/" || base == "%")
    {
        if (b == 0) return false;  // 折叠掉就把除零错误也折掉了。
        if (type.isUnsigned)
        {
            const std::uint64_t ua = static_cast<std::uint64_t>(a), ub = static_cast<std::uint64_t>(b);
            out = normalizeInt(static_cast<std::int64_t>(base == "/" ? ua / ub : ua % ub), type);
            return true;
        }
        if (base == "/" && a == INT64_MIN && b == -1) return false;  // 运行期会报溢出。
        out = normalizeInt(base == "/" ? a / b : (a == INT64_MIN && b == -1 ? 0 : a % b), type);
        return true;
    }
    if (base == "<<" || base == ">>")
    {
        if (b < 0 || b >= 64) return false;  // 运行期会报移位越界。
        const auto bits = static_cast<std::uint64_t>(a);
        const int shift = static_cast<int>(b);
        const std::int64_t value = base == "<<" ? static_cast<std::int64_t>(bits << shift) :
                                                  (type.isUnsigned ? static_cast<std::int64_t>(bits >> shift) : (a >> shift));
        out = normalizeInt(value, type);
        return true;
    }
    if (base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=")
    {
        bool value = false;
        if (base == "==")
            value = a == b;
        else if (base == "!=")
            value = a != b;
        else if (type.isUnsigned)
        {
            const std::uint64_t ua = static_cast<std::uint64_t>(a), ub = static_cast<std::uint64_t>(b);
            value = base == "<" ? ua < ub : base == "<=" ? ua <= ub :
                                        base == ">"      ? ua > ub :
                                                           ua >= ub;
        }
        else
        {
            value = base == "<" ? a < b : base == "<=" ? a <= b :
                                      base == ">"      ? a > b :
                                                         a >= b;
        }
        out = value ? 1 : 0;
        return true;
    }
    return false;
}

/**
 * @brief 是否是 2 的幂。
 * @param value 正整数。
 * @return 是返回真。
 */
bool powerOfTwo(std::int64_t value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

/**
 * @brief 只保留基础操作符的类型后缀。
 * @param type 类型名。
 * @return 形如 `:i64` 的后缀；无类型返回空串。
 */
Memory::String suffixOf(const Memory::String &type)
{
    return type.empty() ? Memory::String() : ":" + type;
}

/**
 * @brief 造一条 `:=(源, _, 目标)` 拷贝。
 * @param like 位置模板（继承源码位置）。
 * @param source 源操作数。
 * @param destination 目标变量。
 * @return 新四元式。
 */
Quad makeCopy(const Quad &like, const Memory::String &source, const Memory::String &destination)
{
    Quad quad = like;
    quad.op = ":=";
    quad.args.clear();
    quad.args.push_back(source);
    quad.args.push_back("_");
    quad.args.push_back(destination);
    return quad;
}

/**
 * @brief 强度削减：乘 2^k 换成左移，无符号除 / 模 2^k 换成右移 / 按位与。
 * @param quads 四元式。
 * @param stats 统计。
 */
void strengthReduce(Memory::Vector<Quad> &quads, Stats &stats)
{
    for (Quad &quad : quads)
    {
        Memory::String type;
        const Memory::String base = splitType(quad.op, type);
        if (base != "*" && base != "/" && base != "%") continue;
        std::int64_t factor = 0;
        if (!integerLiteral(quad.arg(1), factor) || !powerOfTwo(factor)) continue;
        const TypeInfo info = type.empty() ? typeOf("") : typeOf(type);
        if (info.isFloat) continue;
        int shift = 0;
        while ((std::int64_t(1) << shift) < factor) ++shift;
        if (base == "*")
        {
            quad.op = "<<" + suffixOf(type);
            quad.args[1] = Memory::toString(shift);
            ++stats.reduced;
        }
        else if (info.isUnsigned)
        {
            quad.op = (base == "/" ? ">>" : "&") + suffixOf(type);
            quad.args[1] = Memory::toString(base == "/" ? shift : factor - 1);
            ++stats.reduced;
        }
    }
}

/**
 * @brief 常量传播与折叠：只对"整个程序里只定义一次"的局部 / 临时量传播。
 * @param quads 四元式。
 * @param stats 统计。
 */
void foldAndPropagate(Memory::Vector<Quad> &quads, Stats &stats)
{
    Memory::Map<Memory::String, std::size_t> defCount;
    Memory::Set<Memory::String> locals;
    for (const Quad &quad : quads)
    {
        Memory::String type;
        const Memory::String base = splitType(quad.op, type);
        if (base == "decl" && quad.arg(1) == "local") locals.insert(quad.arg(2));
        if (!writesResult(base)) continue;
        if (!quad.arg(2).empty() && quad.arg(2) != "_") ++defCount[quad.arg(2)];
        if ((base == "mul" || base == "div") && !quad.arg(3).empty() && quad.arg(3) != "_") ++defCount[quad.arg(3)];
    }
    // 值得传播的名字：声明过的局部，或编译器临时量——都要求全程序只有一处定义。
    const auto propagable = [&](const Memory::String &name) {
        if (name.empty() || name == "_" || defCount[name] != 1u) return false;
        return locals.find(name) != locals.end() || name.rfind("__temp_", 0) == 0;
    };
    Memory::Map<Memory::String, std::int64_t> constants;
    for (Quad &quad : quads)
    {
        Memory::String type;
        const Memory::String base = splitType(quad.op, type);
        const TypeInfo info = type.empty() ? typeOf("") : typeOf(type);
        const bool computing = pureBinary(base) || trapping(base);
        if (computing)
        {
            for (std::size_t slot = 0; slot < 2u && slot < quad.args.size(); ++slot)
            {
                const auto found = constants.find(quad.args[slot]);
                if (found == constants.end()) continue;
                quad.args[slot] = Memory::toString(found->second);
                ++stats.propagated;
            }
        }
        std::int64_t a = 0, b = 0, folded = 0;
        if (computing && integerLiteral(quad.arg(0), a) && integerLiteral(quad.arg(1), b) && foldBinary(base, info, a, b, folded))
        {
            const Memory::String result = quad.arg(2);
            quad.op = ":=";
            quad.args.clear();
            quad.args.push_back(Memory::toString(folded));
            quad.args.push_back("_");
            quad.args.push_back(result);
            ++stats.folded;
            if (propagable(result)) constants[result] = folded;
            continue;
        }
        if (base == ":=" && quad.args.size() >= 3u && integerLiteral(quad.arg(0), a) && propagable(quad.arg(2)))
            constants[quad.arg(2)] = a;
    }
}

/**
 * @brief 局部公共子表达式消除：同一基本块里重复的纯运算改写成一次拷贝。
 * @param quads 四元式。
 * @param stats 统计。
 */
void commonSubexpressions(Memory::Vector<Quad> &quads, Stats &stats)
{
    struct Entry
    {
        Memory::String result;
        std::size_t version = 0;  ///< 记录时 result 的版本，用来判"这条结果还作数吗"。
    };
    /* 失效不做遍历：给每个名字一个版本号，键里带上"当时操作数的版本"，再给整张表一个
     * epoch（块边界 / 调用时 +1）。写过谁就 ++版本，旧条目自然再也匹配不上，代价 O(1)。
     * 之前每遇到一条纯运算就线性扫一遍表，是 `n × 表长`，一万多条四元式上会明显拖慢编译。 */
    Memory::Map<Memory::String, std::size_t> versions;
    Memory::Map<Memory::String, Entry> table;
    std::size_t epoch = 0;
    const auto versionOf = [&](const Memory::String &name) {
        const auto found = versions.find(name);
        return found == versions.end() ? std::size_t{ 0 } : found->second;
    };
    const auto touch = [&](const Memory::String &name) {
        if (name.empty() || name == "_") return;
        ++versions[name];
    };
    // 会跳走（块边界）、会改别人的存储、或调用之后任何符号都可能变：换一个 epoch。
    static const Memory::Set<Memory::String> kBarrier = { "label", "j", "jz", "jnz", "call", "callptr",
                                                          "ret", "halt", "store", "free", "drop", "read", "getc" };
    for (Quad &quad : quads)
    {
        Memory::String type;
        const Memory::String base = splitType(quad.op, type);
        if (kBarrier.find(base) != kBarrier.end())
        {
            ++epoch;
            continue;
        }
        if (!pureBinary(base))
        {
            touch(quad.arg(2));
            continue;
        }
        const Memory::String left = quad.arg(0), right = quad.arg(1);
        const Memory::String result = quad.arg(2);
        const Memory::String key = base + "|" + type + "|" + Memory::toString(epoch) + "|" + Memory::toString(versionOf(left)) + "|" + left + "|" + Memory::toString(versionOf(right)) + "|" + right;
        const auto found = table.find(key);
        if (found != table.end() && found->second.version == versionOf(found->second.result) && found->second.result != result && !found->second.result.empty())
        {
            quad = makeCopy(quad, found->second.result, result);
            ++stats.merged;
            touch(result);
            continue;
        }
        touch(result);  // 本运算重写了 result，依赖它的旧条目随之失效（版本已经变了）。
        table[key] = { result, versionOf(result) };
    }
}

/**
 * @brief 死代码消除：纯运算的结果没人读就整条删掉。
 * @param quads 四元式。
 * @param stats 统计。
 */
void deadCode(Memory::Vector<Quad> &quads, Stats &stats)
{
    Memory::Map<Memory::String, std::size_t> reads;
    Memory::Set<Memory::String> locals;
    for (const Quad &quad : quads)
    {
        Memory::String type;
        const Memory::String base = splitType(quad.op, type);
        if (base == "decl" && quad.arg(1) == "local") locals.insert(quad.arg(2));
        const bool definesSecond = base == "mul" || base == "div";
        for (std::size_t slot = 0; slot < quad.args.size(); ++slot)
        {
            if (slot == 2u) continue;                   // 结果位置：不算读。
            if (definesSecond && slot == 3u) continue;  // 双结果的第二半：不算读。
            const Memory::String &name = quad.args[slot];
            if (name.empty() || name == "_") continue;
            ++reads[name];
        }
    }
    Memory::Vector<Quad> kept;
    kept.reserve(quads.size());
    /* 临时量的纯拷贝也归 DCE 管：`:=` 本身没有副作用，写到编译器临时量上的拷贝
     * 如果没人读，删掉它和删掉一次算术没有区别。声明过的局部不能删——它能被调试器
     * 或反射按名字看到。 */
    const auto disposable = [&](const Memory::String &base, const Memory::String &result) {
        if (result.empty() || result == "_") return false;
        if (locals.find(result) != locals.end()) return false;
        if (reads.find(result) != reads.end()) return false;
        return pureBinary(base) || (base == ":=" && result.rfind("__temp_", 0) == 0);
    };
    for (Quad &quad : quads)
    {
        Memory::String type;
        const Memory::String base = splitType(quad.op, type);
        const Memory::String result = quad.arg(2);
        /* 只删"纯且不会失败"的运算：会陷阱的必须留下，否则报错就消失了。 */
        if (disposable(base, result))
        {
            ++stats.removed;
            continue;
        }
        kept.push_back(std::move(quad));
    }
    quads = std::move(kept);
}

/**
 * @brief 循环不变量外提：把循环里每轮都算、且每轮结果一样的纯运算提到前置头。
 * @param quads 四元式。
 * @param stats 统计。
 */
void hoistInvariants(Memory::Vector<Quad> &quads, Stats &stats)
{
    struct Loop
    {
        std::size_t head = 0, tail = 0;
        Memory::String label;
    };
    Memory::Map<Memory::String, std::size_t> labelAt;
    Memory::Map<Memory::String, Memory::Vector<std::size_t>> definitions;
    Memory::Set<Memory::String> entries;
    for (std::size_t index = 0; index < quads.size(); ++index)
    {
        Memory::String type;
        const Memory::String base = splitType(quads[index].op, type);
        if (base == "label")
            labelAt.emplace(quads[index].arg(2), index);
        else if (base == "call")
            entries.insert(quads[index].arg(0));
        if (!writesResult(base) || base == "decl") continue;
        if (!quads[index].arg(2).empty() && quads[index].arg(2) != "_") definitions[quads[index].arg(2)].push_back(index);
    }
    Memory::Vector<Loop> loops;
    Memory::Vector<std::size_t> jumpTargets;  // 每条跳转的目标下标，判断"有没有从外面跳进循环中间"。
    for (std::size_t index = 0; index < quads.size(); ++index)
    {
        Memory::String type;
        const Memory::String base = splitType(quads[index].op, type);
        if (base != "j" && base != "jz" && base != "jnz") continue;
        const auto found = labelAt.find(quads[index].arg(2));
        if (found == labelAt.end()) continue;
        jumpTargets.push_back(found->second);
        // 回边：跳转目标在它前面，就构成一个循环，标签处是循环头。
        if (found->second < index) loops.push_back({ found->second, index, quads[index].arg(2) });
    }
    /* 同一个循环头可能有多条回边（`while` 的 break 收尾常这么出），那是同一个循环：
     * 只保留范围最大的那一条。否则会给同一个头插两个前置头，而且第二次插入时下标已经
     * 被第一次插歪了。 */
    {
        Memory::Map<std::size_t, std::size_t> widest;  // 头 → 在 loops 里的下标
        for (std::size_t index = 0; index < loops.size(); ++index)
        {
            const auto found = widest.find(loops[index].head);
            if (found == widest.end())
                widest.emplace(loops[index].head, index);
            else if (loops[found->second].tail < loops[index].tail)
                found->second = index;
        }
        Memory::Vector<Loop> unique;
        for (std::size_t index = 0; index < loops.size(); ++index)
            if (widest[loops[index].head] == index) unique.push_back(loops[index]);
        loops = std::move(unique);
    }
    // 从后往前处理：插入点在前面的循环之前，前面循环的下标不受影响。
    for (std::size_t cursor = loops.size(); cursor > 0u; --cursor)
    {
        const Loop loop = loops[cursor - 1u];
        if (entries.find(loop.label) != entries.end()) continue;  // 函数入口不能插前置头。
        bool skip = false;
        for (std::size_t other = 0; other < loops.size(); ++other)
        {
            if (other == cursor - 1u) continue;
            if (loops[other].head > loop.head && loops[other].tail < loop.tail) skip = true;  // 有内层循环，留给它自己。
        }
        /* 前置头只能从循环头进：如果有跳转从外面直接落到循环中间，说明这个"循环"其实
         * 有多个入口，外提的值在那个入口上是没有算过的，绝不能提。 */
        for (std::size_t target = 0; target < jumpTargets.size() && !skip; ++target)
            if (jumpTargets[target] > loop.head && jumpTargets[target] <= loop.tail) skip = true;
        if (skip) continue;
        /* 循环体里有没有"能从别处改到名字值"的操作：调用能改全局，`store` 能通过指针
         * 改内存，`read`/`getc` 改自己的目标，`alloc` 改堆。这些操作不影响"操作数查得到
         * 唯一定义"的情形，只影响"操作数在本程序里根本没有定义"的情形（形参 / 全局），
         * 所以只在缺失定义时用它兜底，而不是一见到调用就整段放弃。 */
        bool opaque = false;
        for (std::size_t index = loop.head; index <= loop.tail && !opaque; ++index)
        {
            Memory::String type;
            const Memory::String base = splitType(quads[index].op, type);
            if (base == "call" || base == "callptr" || base == "store" || base == "read" || base == "getc" || base == "alloc")
                opaque = true;
        }

        Memory::Vector<std::size_t> candidates;
        for (std::size_t index = loop.head; index <= loop.tail; ++index)
        {
            Memory::String type;
            const Memory::String base = splitType(quads[index].op, type);
            if (!pureBinary(base)) continue;
            const Memory::String result = quads[index].arg(2);
            if (result.empty() || result == "_" || definitions[result].size() != 1u) continue;
            bool invariant = true;
            for (std::size_t slot = 0; slot < 2u && slot < quads[index].args.size() && invariant; ++slot)
            {
                const Memory::String &operand = quads[index].args[slot];
                if (operand.empty() || operand == "_") continue;
                std::int64_t ignored = 0;
                if (integerLiteral(operand, ignored)) continue;
                const auto found = definitions.find(operand);
                if (found == definitions.end())
                {
                    /* 全程序里没人写过它：形参或全局。只要循环里没有能绕过去改它的操作，
                     * 它就是不变的。（形参由调用方用 `param` 填，不算本函数里的定义。） */
                    if (opaque) invariant = false;
                    continue;
                }
                if (found->second.size() != 1u || found->second.front() >= loop.head) invariant = false;
            }
            if (invariant) candidates.push_back(index);
        }
        if (candidates.empty()) continue;

        const Memory::String preheader = "__licm_pre_" + Memory::toString(cursor);
        Memory::Map<Memory::String, Memory::String> renamed;
        Memory::Vector<Quad> prologue;
        for (std::size_t order = 0; order < candidates.size(); ++order)
        {
            Quad hoisted = quads[candidates[order]];
            const Memory::String name = "__licm_" + Memory::toString(cursor) + "_" + Memory::toString(order);
            renamed.emplace(quads[candidates[order]].arg(2), name);
            hoisted.args[2] = name;
            prologue.push_back(std::move(hoisted));
        }

        Memory::Vector<Quad> result;
        result.reserve(quads.size() + prologue.size() + 2u);
        const Quad &at = quads[loop.head];
        for (std::size_t index = 0; index < quads.size(); ++index)
        {
            if (index == loop.head)
            {
                Quad prelabel = at;
                prelabel.op = "label";
                prelabel.args.clear();
                prelabel.args.push_back("_");
                prelabel.args.push_back("_");
                prelabel.args.push_back(preheader);
                result.push_back(prelabel);
                for (const Quad &item : prologue) result.push_back(item);
                Quad back = at;
                back.op = "j";
                back.args.clear();
                back.args.push_back("_");
                back.args.push_back("_");
                back.args.push_back(loop.label);
                result.push_back(back);
            }
            const bool inside = index >= loop.head && index <= loop.tail;
            bool hoisted = false;
            for (const std::size_t candidate : candidates)
                if (candidate == index) hoisted = true;
            if (hoisted)
            {
                ++stats.hoisted;
                continue;
            }
            Quad quad = quads[index];
            // 循环内使用被外提的结果 → 直接改成外提后的名字。
            for (std::size_t slot = 0; slot < quad.args.size(); ++slot)
            {
                if (slot == 2u) continue;
                const auto found = renamed.find(quad.args[slot]);
                if (found != renamed.end()) quad.args[slot] = found->second;
            }
            // 循环外的跳转要先进前置头（循环体里的回边保持指向原标签）。
            if (!inside)
            {
                Memory::String type;
                const Memory::String base = splitType(quad.op, type);
                if ((base == "j" || base == "jz" || base == "jnz") && quad.arg(2) == loop.label)
                {
                    quad.args[2] = preheader;
                }
            }
            result.push_back(std::move(quad));
        }
        quads = std::move(result);
    }
}
/**
 * @brief 内联展开时给复制过来的符号改名。
 * @param name 原名。
 * @param site 展开点编号。
 * @return 新名：编译器临时量保住 `__temp_` 前缀（展开点编号保证各调用点的复制品互不同名，
 *         每份都在调用者激活里拿自己的帧槽），其余名字换成 `__inl<编号>_<原名>`（`$`、`.`、`<`、`>`、`:` 换成 `_`）。
 */
Memory::String inlineName(const Memory::String &name, std::size_t site)
{
    Memory::String result = name.rfind("__temp_", 0) == 0 ? "__temp_inl" : "__inl";
    result += Memory::toString(site);
    result += '_';
    for (const char letter : name)
    {
        const bool plain = (letter >= '0' && letter <= '9') || (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') || letter == '_';
        result += plain ? letter : '_';
    }
    return result;
}

/**
 * @brief 小函数内联：把热调用点上的小函数就地展开，省掉调用帧与参数绑定。
 *
 * 只挑三类**一定安全**的调用点：
 *  - 被调函数在本程序里有函数体（有入口标签、有 `ret`），且不是递归的（直接或间接）；
 *  - 函数体（含入口标签前的形参声明）不超过 `kInlineLimit` 条四元式；
 *  - 调用点落在某个循环区间里——展开会复制指令，只给热调用点做，代码膨胀才有界。
 *
 * 展开口径与调用口径逐条对齐：`param` 变成一次拷贝，`call` 的结果临时量由 `ret` 位置的
 * 拷贝写入，多个 `ret` 汇到同一个出口标签。复制过来的符号一律改名：形参与局部换成
 * `__inl<点>_<原名>`（仍然声明成局部，于是在调用者帧里拿到槽位），编译器临时量与
 * `__chali_ret_v$*` 换成 `__temp_inl<点>_<原名>`（保住临时量前缀，展开点编号保证互不同名，
 * 每份在调用者激活里拿自己的帧槽）。
 *
 * @param quads 四元式。
 * @param stats 统计。
 */
void inlineSmallFunctions(Memory::Vector<Quad> &quads, Stats &stats)
{
    /* 函数体规模上限与展开点上限：两个数一起决定"最坏膨胀多少条"。基准里的热点函数
     * 都在十条上下，`24` 足够覆盖它们，又不会把大函数复制进循环。 */
    constexpr std::size_t kInlineLimit = 24;
    constexpr std::size_t kInlineSites = 32;

    Memory::Map<Memory::String, std::size_t> labelAt;
    Memory::Set<Memory::String> entries;
    Memory::Set<Memory::String> addressed;
    Memory::Vector<std::size_t> calls;
    for (std::size_t index = 0; index < quads.size(); ++index)
    {
        Memory::String type;
        const Memory::String base = splitType(quads[index].op, type);
        if (base == "label")
        {
            labelAt.emplace(quads[index].arg(2), index);
        }
        else if (base == "call")
        {
            entries.insert(quads[index].arg(0));
            calls.push_back(index);
        }
        else if (base == "addr" || base == "callptr")
        {
            addressed.insert(quads[index].arg(0));  // 取过地址的函数可能被间接调用，不动它。
        }
    }
    if (entries.empty() || calls.empty()) return;

    /* 循环区间：一条跳回前面的跳转就是一个循环 `[目标, 跳转位置]`。 */
    Memory::Vector<std::pair<std::size_t, std::size_t>> loops;
    for (std::size_t index = 0; index < quads.size(); ++index)
    {
        Memory::String type;
        const Memory::String base = splitType(quads[index].op, type);
        if (base != "j" && base != "jz" && base != "jnz") continue;
        const auto found = labelAt.find(quads[index].arg(2));
        if (found != labelAt.end() && found->second < index) loops.push_back({ found->second, index });
    }
    if (loops.empty()) return;

    // 入口标签按下标排好，用来切"一个函数的区域到下一个入口标签为止"。
    Memory::Vector<std::pair<std::size_t, Memory::String>> ordered;
    for (const Memory::String &name : entries)
    {
        const auto found = labelAt.find(name);
        if (found != labelAt.end() && addressed.find(name) == addressed.end()) ordered.push_back({ found->second, name });
    }
    std::sort(ordered.begin(), ordered.end());

    struct Callee
    {
        std::size_t first = 0;                  ///< 区域起点（入口标签前的形参声明）。
        std::size_t label = 0;                  ///< 入口标签下标。
        std::size_t last = 0;                   ///< 最后一条 `ret` 的下标。
        Memory::Vector<Memory::String> params;  ///< 形参名，按声明顺序。
        bool usable = false;                    ///< 能不能内联。
    };
    Memory::Map<Memory::String, Callee> table;
    Memory::Map<Memory::String, Memory::Vector<Memory::String>> callsOf;
    for (std::size_t order = 0; order < ordered.size(); ++order)
    {
        Callee callee;
        callee.label = ordered[order].first;
        const std::size_t regionEnd = order + 1u < ordered.size() ? ordered[order + 1u].first : quads.size();
        callee.first = callee.label;
        while (callee.first > 0u)
        {
            Memory::String type;
            const Memory::String base = splitType(quads[callee.first - 1u].op, type);
            const Memory::String kind = quads[callee.first - 1u].arg(1);
            if (base != "decl" || (kind != "local" && kind != "param")) break;
            --callee.first;
        }
        for (std::size_t index = callee.first; index < callee.label; ++index) callee.params.push_back(quads[index].arg(2));

        /* 函数体到哪儿结束：从入口往后找第一个"内部跳转都不越界"的 `ret`。提前返回的
         * `ret` 前面总有跳转要越过它，所以那个前缀不成立；成立的那个就是完整函数体。
         * 只有窗口 `kInlineLimit * 2` 以内才找——反正超过上限也不展开。 */
        bool found = false;
        for (std::size_t index = callee.label + 1u; index < regionEnd && index < quads.size() && index <= callee.label + kInlineLimit * 2u + 1u && !found; ++index)
        {
            Memory::String type;
            const Memory::String base = splitType(quads[index].op, type);
            if (base != "ret") continue;
            bool inside = true;
            for (std::size_t scan = callee.label; scan <= index && inside; ++scan)
            {
                Memory::String scanType;
                const Memory::String scanBase = splitType(quads[scan].op, scanType);
                if (scanBase != "j" && scanBase != "jz" && scanBase != "jnz") continue;
                const auto target = labelAt.find(quads[scan].arg(2));
                if (target == labelAt.end() || target->second < callee.first || target->second > index) inside = false;
            }
            if (!inside) continue;
            callee.last = index;
            found = true;
        }
        if (!found) continue;  // 没有函数体（外部函数 / 只声明没定义），原样调用。
        bool usable = callee.last - callee.first + 1u <= kInlineLimit;
        /* 函数体里不能有陷阱式的收尾，也不展开"取过地址 / 间接调用"的东西。 */
        for (std::size_t index = callee.first; index <= callee.last; ++index)
        {
            Memory::String type;
            const Memory::String base = splitType(quads[index].op, type);
            if (base == "halt" || base == "callptr") usable = false;
            if (base != "call") continue;
            callsOf[ordered[order].second].push_back(quads[index].arg(0));
        }
        callee.usable = usable;
        table[ordered[order].second] = callee;
    }
    if (table.empty()) return;

    /* 递归判定：从被调函数能走回自己就不展开（展开递归会无限膨胀）。 */
    const auto recursive = [&](const Memory::String &start) {
        Memory::Set<Memory::String> seen;
        Memory::Vector<Memory::String> pending;
        pending.push_back(start);
        while (!pending.empty())
        {
            const Memory::String current = pending.back();
            pending.pop_back();
            if (!seen.insert(current).second) continue;
            const auto found = callsOf.find(current);
            if (found == callsOf.end()) continue;
            for (const Memory::String &next : found->second)
            {
                if (next == start) return true;
                pending.push_back(next);
            }
        }
        return false;
    };

    /* 选站点与展开分两步：先按原坐标挑站点，再线性重建一遍。展开会插入四元式，
     * 一边挑一边改会把"被调函数体"的下标全部冲掉（函数体通常排在调用点后面），
     * 所以先定后改。 */
    struct Site
    {
        std::size_t begin = 0;           ///< 这组 `param` 的起点。
        std::size_t at = 0;              ///< `call` 的位置。
        const Callee *callee = nullptr;  ///< 被调函数（指向 `table` 里的稳定结点）。
        std::size_t id = 0;              ///< 展开编号，改名时用它区分不同站点。
    };
    Memory::Vector<Site> plans;
    for (std::size_t cursor = 0; cursor < calls.size() && plans.size() < kInlineSites; ++cursor)
    {
        const std::size_t at = calls[cursor];
        Memory::String type;
        const Memory::String base = splitType(quads[at].op, type);
        if (base != "call") continue;
        const auto found = table.find(quads[at].arg(0));
        if (found == table.end() || !found->second.usable) continue;
        const Callee &callee = found->second;
        if (recursive(quads[at].arg(0))) continue;

        /* 调用点前面必须正好是按形参顺序摆好的一组 `param`；再往前一条还是 `param`
         * 就说明这一组不止这些实参（可变参数 / 原生函数），口径对不上，不展开。 */
        const std::size_t count = callee.params.size();
        if (at < count) continue;
        const std::size_t begin = at - count;
        bool matched = true;
        for (std::size_t slot = 0; slot < count && matched; ++slot)
        {
            Memory::String slotType;
            const Memory::String slotBase = splitType(quads[begin + slot].op, slotType);
            if (slotBase != "param" || quads[begin + slot].arg(2) != callee.params[slot]) matched = false;
        }
        if (!matched) continue;
        if (begin > 0u)
        {
            Memory::String beforeType;
            const Memory::String beforeBase = splitType(quads[begin - 1u].op, beforeType);
            if (beforeBase == "param") continue;
        }
        bool hot = false;
        for (const auto &range : loops)
            if (range.first <= begin && at <= range.second) hot = true;
        if (!hot) continue;
        /* 和已选站点重叠就放弃：重叠说明这处调用的实参里还有另一处要展开的调用，
         * 外层展开会把内层的整个调用点覆盖掉。 */
        bool overlap = false;
        for (const Site &plan : plans)
            if (!(at < plan.begin || begin > plan.at)) overlap = true;
        if (overlap) continue;
        plans.push_back({ begin, at, &callee, plans.size() });
    }
    if (plans.empty()) return;

    /* 把一处调用展开成一段四元式：形参声明 → 传参拷贝 → 函数体 → 出口标签。 */
    const auto expand = [&](const Site &plan) {
        const Callee &callee = *plan.callee;
        const std::size_t count = callee.params.size();
        const Memory::String join = "__inl" + Memory::toString(plan.id) + "_join";
        const Memory::String destination = quads[plan.at].arg(2);
        /* 复制品里的符号改名表：形参、声明过的局部、编译器临时量。 */
        Memory::Map<Memory::String, Memory::String> renamed;
        Memory::Map<Memory::String, Memory::String> labels;
        for (const Memory::String &param : callee.params) renamed.emplace(param, inlineName(param, plan.id));
        for (std::size_t index = callee.first; index <= callee.last; ++index)
        {
            Memory::String bodyType;
            const Memory::String bodyBase = splitType(quads[index].op, bodyType);
            for (std::size_t slot = 0; slot < quads[index].args.size(); ++slot)
            {
                const Memory::String &argument = quads[index].args[slot];
                if (argument.empty() || argument == "_") continue;
                const bool temporary = argument.rfind("__temp_", 0) == 0 || argument.rfind("__chali_ret_v$", 0) == 0;
                if (!temporary && !(bodyBase == "decl" && slot == 2u)) continue;
                renamed.emplace(argument, inlineName(argument, plan.id));
            }
            if (bodyBase == "label") labels.emplace(quads[index].arg(2), inlineName(quads[index].arg(2), plan.id));
        }
        const auto renameOf = [&](const Memory::String &name) {
            const auto hit = renamed.find(name);
            return hit == renamed.end() ? name : hit->second;
        };
        const auto labelOf = [&](const Memory::String &name) {
            const auto hit = labels.find(name);
            return hit == labels.end() ? name : hit->second;
        };

        Memory::Vector<Quad> copy;
        copy.reserve(callee.last - callee.first + count + 4u);
        // ① 形参声明：先落进调用者的帧，参数才有地方放。
        for (std::size_t index = callee.first; index < callee.label; ++index)
        {
            Quad quad = quads[index];
            quad.args[2] = renameOf(quad.arg(2));
            copy.push_back(std::move(quad));
        }
        // ② 传参：`param` 变成一次拷贝（声明之后再写，免得多值被 `decl` 重置掉）。
        for (std::size_t slot = 0; slot < count; ++slot)
        {
            Quad quad = quads[plan.begin + slot];
            quad.op = ":=";
            quad.args[2] = renameOf(callee.params[slot]);
            copy.push_back(std::move(quad));
        }
        // ③ 函数体：`ret` 换成"写结果 + 跳到出口"，其余指令改名。
        for (std::size_t index = callee.label; index <= callee.last; ++index)
        {
            Quad quad = quads[index];
            Memory::String bodyType;
            const Memory::String bodyBase = splitType(quad.op, bodyType);
            if (bodyBase == "ret")
            {
                const Memory::String source = quad.arg(0);
                if (!source.empty() && source != "_" && !destination.empty() && destination != "_")
                {
                    quad.op = ":=";
                    quad.args.clear();
                    quad.args.push_back(renameOf(source));
                    quad.args.push_back("_");
                    quad.args.push_back(destination);
                    copy.push_back(std::move(quad));
                }
                Quad jump = quads[index];
                jump.op = "j";
                jump.args.clear();
                jump.args.push_back("_");
                jump.args.push_back("_");
                jump.args.push_back(join);
                copy.push_back(std::move(jump));
                continue;
            }
            if (bodyBase == "label")
            {
                quad.args[2] = labelOf(quad.arg(2));
            }
            else if (bodyBase == "j" || bodyBase == "jz" || bodyBase == "jnz")
            {
                quad.args[0] = renameOf(quad.arg(0));
                quad.args[2] = labelOf(quad.arg(2));
            }
            else if (bodyBase == "param")
            {
                quad.args[0] = renameOf(quad.arg(0));  // 第三位是别人家的形参名，不能改。
            }
            else if (bodyBase == "call")
            {
                quad.args[2] = renameOf(quad.arg(2));
            }
            else
            {
                for (std::size_t slot = 0; slot < quad.args.size(); ++slot) quad.args[slot] = renameOf(quad.args[slot]);
            }
            copy.push_back(std::move(quad));
        }
        // ④ 出口：所有 `ret` 汇到这里，调用之后的第一条指令就从这儿接着走。
        Quad exit = quads[plan.at];
        exit.op = "label";
        exit.args.clear();
        exit.args.push_back("_");
        exit.args.push_back("_");
        exit.args.push_back(join);
        copy.push_back(std::move(exit));
        return copy;
    };

    Memory::Vector<Quad> rebuilt;
    rebuilt.reserve(quads.size() + kInlineLimit * plans.size());
    std::size_t cursor = 0;
    for (const Site &plan : plans)  // `plans` 按 `begin` 升序
    {
        /* 只能拷贝不能移动：被调函数的函数体可能排在调用点前面，后面展开时还要读它。 */
        while (cursor < plan.begin) rebuilt.push_back(quads[cursor++]);
        Memory::Vector<Quad> copy = expand(plan);
        const std::size_t taken = plan.at - plan.begin + 1u;
        ++stats.inlined;
        if (copy.size() > taken) stats.inlinedQuads += copy.size() - taken;
        rebuilt.insert(rebuilt.end(), copy.begin(), copy.end());
        cursor = plan.at + 1u;
    }
    while (cursor < quads.size()) rebuilt.push_back(quads[cursor++]);
    quads = std::move(rebuilt);
}
}  // namespace

QuadFile optimize(const QuadFile &file, Stats *stats)
{
    QuadFile result = file;
    Stats local;
    local.before = file.quads.size();
    if (file.ok() && enabled())
    {
        /* 每一遍都可以单独关掉：`CHALI_NO_OPT_<遍>=1`。定位"某一遍改了语义"时
         * 不用改代码重编，逐遍二分即可。 */
        const auto passOn = [](const char *name) {
            const char *flag = std::getenv(name);
            return flag == nullptr || *flag == '\0';
        };
        // 内联先跑：后面的折叠 / 外提都作用在展开后的代码上，收益才叠加得起来。
        if (passOn("CHALI_NO_OPT_INLINE")) inlineSmallFunctions(result.quads, local);
        if (passOn("CHALI_NO_OPT_STRENGTH")) strengthReduce(result.quads, local);
        if (passOn("CHALI_NO_OPT_FOLD")) foldAndPropagate(result.quads, local);
        if (passOn("CHALI_NO_OPT_GVN")) commonSubexpressions(result.quads, local);
        if (passOn("CHALI_NO_OPT_DCE")) deadCode(result.quads, local);
        if (passOn("CHALI_NO_OPT_LICM")) hoistInvariants(result.quads, local);
    }
    local.after = result.quads.size();
    if (std::getenv("CHALI_DUMP_MACHINE") != nullptr)
    {
        std::fputs("=== 机器级优化后 ===\n", stderr);
        for (const Quad &quad : result.quads)
        {
            std::fprintf(stderr, "%s(", quad.op.c_str());
            for (std::size_t index = 0; index < quad.args.size(); ++index) std::fprintf(stderr, "%s%s", index == 0u ? "" : ", ", quad.args[index].c_str());
            std::fputs(")\n", stderr);
        }
    }
    Stats &total = accumulated();
    total.before += local.before;
    total.after += local.after;
    total.folded += local.folded;
    total.propagated += local.propagated;
    total.removed += local.removed;
    total.merged += local.merged;
    total.reduced += local.reduced;
    total.hoisted += local.hoisted;
    total.inlined += local.inlined;
    total.inlinedQuads += local.inlinedQuads;
    if (stats != nullptr) *stats = local;
    return result;
}

bool enabled()
{
    const char *flag = std::getenv("CHALI_NO_MACHINE_OPT");
    return flag == nullptr || *flag == '\0';
}

Stats lastStats() { return accumulated(); }

void resetStats() { accumulated() = Stats{}; }

Memory::String describe(const Stats &stats)
{
    Memory::String text = "机器级优化：四元式 " + Memory::toString(stats.before) + " → " + Memory::toString(stats.after) + "（折叠 " + Memory::toString(stats.folded) + "，传播 " + Memory::toString(stats.propagated) + "，合并 " + Memory::toString(stats.merged) + "，削减 " + Memory::toString(stats.reduced) + "，外提 " + Memory::toString(stats.hoisted) + "，内联 " + Memory::toString(stats.inlined) + " 处（+" + Memory::toString(stats.inlinedQuads) + " 条），删除 " + Memory::toString(stats.removed) + "）";
    return text;
}
}  // namespace machine
}  // namespace stackvm
