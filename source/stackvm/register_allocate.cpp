/**
 * @file register_allocate.cpp
 * @brief 活跃性分析与线性扫描槽位分配的实现。
 */
#include "register_allocate.hpp"
#include "runtime_internal.hpp"

namespace stackvm
{
namespace allocation
{
namespace
{
constexpr std::size_t absent = static_cast<std::size_t>(-1);

/**
 * @brief 一个局部符号的存活区间。
 */
struct Interval
{
    std::size_t symbol = absent;  ///< 程序符号下标。
    std::size_t begin = 0;        ///< 首次出现的位置。
    std::size_t end = 0;          ///< 最后出现的位置。
    std::size_t slot = absent;    ///< 分配到的槽位。
    bool pinned = false;          ///< 形参：固定槽。
};

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
 * @brief 自检发现冲突时记一笔，便于用 `CHALI_DUMP_FRAME` 追。
 * @param first 先占用槽位的符号。
 * @param second 后到的符号。
 * @param pc 冲突发生的 PC。
 */
void noteConflict(std::size_t first, std::size_t second, std::size_t pc)
{
    static bool reported = false;
    if (reported) return;
    reported = true;
    if (const char *dump = std::getenv("CHALI_DUMP_FRAME"); dump == nullptr || *dump == '\0') return;
    std::fprintf(stderr, "分配自检：符号 %zu 与 %zu 在 PC %zu 同时活着却共用了槽位\n", first, second, pc);
}
}  // namespace

Layout assign(const Body &body, Stats *stats)
{
    Layout layout;
    const std::size_t n = body.order.size();
    if (n == 0u) return layout;

    // 1. 只给"本函数自己的符号"编号：形参与声明过的局部。其余符号走全局路径，不占帧槽。
    Memory::Map<std::size_t, std::size_t> localOf;
    Memory::Vector<std::size_t> symbolOf;
    Memory::Map<std::size_t, bool> eligible;
    for (const std::size_t symbol : body.parameters)
        if (symbol != absent) eligible.emplace(symbol, true);
    for (const std::size_t symbol : body.declared)
        if (symbol != absent) eligible.emplace(symbol, true);
    const auto local = [&](std::size_t symbol) -> std::size_t {
        if (symbol == absent || eligible.find(symbol) == eligible.end()) return absent;
        const auto found = localOf.find(symbol);
        if (found != localOf.end()) return found->second;
        const std::size_t index = symbolOf.size();
        localOf.emplace(symbol, index);
        symbolOf.push_back(symbol);
        return index;
    };
    Memory::Vector<std::size_t> parameters;
    for (const std::size_t symbol : body.parameters)
    {
        const std::size_t index = local(symbol);
        if (index != absent) parameters.push_back(index);
    }
    /* 形态：1 = 浮点，0 = 其余。浮点槽只复用给浮点，避免一个槽在两种形态之间跳。 */
    Memory::Map<std::size_t, bool> realSymbol;
    for (const std::size_t symbol : body.reals)
        if (symbol != absent) realSymbol.emplace(symbol, true);
    const auto kindOf = [&](std::size_t symbol) { return symbol != absent && realSymbol.find(symbol) != realSymbol.end() ? 1 : 0; };
    Memory::Vector<int> slotReal;  ///< 每个槽当前主人的形态；-1 表示还没人用过。
    /* 2. 数据流数组按位置建（`body` 已经是位置下标），规模就是本函数的大小。 */
    Memory::Vector<std::size_t> defines(n, absent), uses(n, absent);
    for (std::size_t at = 0; at < n; ++at)
    {
        defines[at] = local(body.defines[at]);
        uses[at] = local(body.uses[at]);
    }
    const std::size_t count = symbolOf.size();
    if (count == 0u) return layout;

    // 3. 活跃性：以符号位集做后向固定点迭代。函数体通常很小，位集比集合更快也更省。
    const std::size_t words = (count + 63u) / 64u;
    Memory::Vector<std::uint64_t> liveIn(n * words, 0), liveOut(n * words, 0);
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (std::size_t step = 0; step < n; ++step)
        {
            const std::size_t at = n - 1u - step;
            std::uint64_t *out = &liveOut[at * words];
            for (std::size_t edge = 0; edge < 2u; ++edge)
            {
                const std::size_t successor = edge == 0u ? body.successors[at].first : body.successors[at].second;
                if (successor == absent) continue;
                const std::uint64_t *in = &liveIn[successor * words];
                for (std::size_t word = 0; word < words; ++word)
                {
                    const std::uint64_t merged = out[word] | in[word];
                    if (merged != out[word])
                    {
                        out[word] = merged;
                        changed = true;
                    }
                }
            }
            std::uint64_t *in = &liveIn[at * words];
            for (std::size_t word = 0; word < words; ++word)
            {
                std::uint64_t value = out[word];
                if (defines[at] != absent && defines[at] / 64u == word) value &= ~(std::uint64_t(1) << (defines[at] % 64u));
                if (uses[at] != absent && uses[at] / 64u == word) value |= std::uint64_t(1) << (uses[at] % 64u);
                if (value != in[word])
                {
                    in[word] = value;
                    changed = true;
                }
            }
        }
    }

    // 4. 存活区间：从活跃点推出每个符号的首末位置（只取出现过的符号）。
    Memory::Vector<Interval> intervals(count);
    Memory::Vector<bool> seen(count, false);
    /* 区间端点取 min/max（而不是"最后访问到的位置"）：区间取宽是安全的，取窄会把还活着的
     * 槽位提前分给别人。 */
    const auto touch = [&](std::size_t slot, std::size_t at) {
        Interval &interval = intervals[slot];
        if (!seen[slot])
        {
            seen[slot] = true;
            interval.symbol = symbolOf[slot];
            interval.begin = at;
            interval.end = at;
            return;
        }
        if (at < interval.begin) interval.begin = at;
        if (at > interval.end) interval.end = at;
    };
    for (std::size_t at = 0; at < n; ++at)
    {
        if (defines[at] != absent) touch(defines[at], at);
        if (uses[at] != absent) touch(uses[at], at);
        for (std::size_t word = 0; word < words; ++word)
        {
            std::uint64_t bits = liveIn[at * words + word] | liveOut[at * words + word];
            while (bits != 0u)
            {
                std::size_t index = 0;
                while ((bits & (std::uint64_t(1) << index)) == 0u) ++index;
                touch(word * 64u + index, at);
                bits &= ~(std::uint64_t(1) << index);
            }
        }
    }

    // 5. 形参先钉在 0..n-1（固定 ABI），其余按 beginning 排序后线性扫描。
    Memory::Vector<std::size_t> ordered;  // 指向 intervals 的下标，不能存副本。
    ordered.reserve(count);
    Memory::Vector<bool> taken(count, false);
    std::size_t nextSlot = 0;
    for (const std::size_t parameter : parameters)
    {
        // 形参即使没被用到也要占住自己的位置：调用方是按顺序绑定的。
        if (!seen[parameter])
        {
            seen[parameter] = true;
            intervals[parameter].symbol = symbolOf[parameter];
        }
        intervals[parameter].pinned = true;
        intervals[parameter].slot = nextSlot;
        if (taken.size() <= nextSlot) taken.resize(nextSlot + 1u, false);
        if (slotReal.size() <= nextSlot) slotReal.resize(nextSlot + 1u, -1);
        taken[nextSlot] = true;
        slotReal[nextSlot] = kindOf(symbolOf[parameter]);
        ++nextSlot;
    }
    for (std::size_t index = 0; index < count; ++index)
        if (seen[index] && !intervals[index].pinned) ordered.push_back(index);
    std::sort(ordered.begin(), ordered.end(), [&](std::size_t left, std::size_t right) {
        return intervals[left].begin != intervals[right].begin ? intervals[left].begin < intervals[right].begin : intervals[left].symbol < intervals[right].symbol;
    });
    Memory::Vector<std::pair<std::size_t, std::size_t>> active;  ///< (结束位置, 槽位)。
    for (const std::size_t cursor : ordered)
    {
        Interval &interval = intervals[cursor];
        for (std::size_t index = 0; index < active.size();)
        {
            if (active[index].first < interval.begin)
            {
                taken[active[index].second] = false;
                active[index] = active.back();
                active.pop_back();
                continue;
            }
            ++index;
        }
        const int kind = kindOf(interval.symbol);
        std::size_t slot = absent;
        for (std::size_t candidate = 0; candidate < nextSlot; ++candidate)
        {
            if (taken[candidate]) continue;
            // 形态不同的槽不能接手：接了就是「同一个槽一会儿装整数一会儿装双精度」。
            if (candidate < slotReal.size() && slotReal[candidate] != -1 && slotReal[candidate] != kind) continue;
            slot = candidate;
            break;
        }
        if (slot == absent)
        {
            slot = nextSlot++;
            if (taken.size() <= slot) taken.resize(slot + 1u, false);
        }
        if (slotReal.size() <= slot) slotReal.resize(slot + 1u, -1);
        taken[slot] = true;
        slotReal[slot] = kind;
        interval.slot = slot;
        active.push_back({ interval.end, slot });
    }

    // 6. 汇总：按符号升序给出映射。任何符号都不许留着 absent（那会被当成"共享/全局"路径）。
    const auto onePerSymbol = [&]() {
        layout.slots.clear();
        layout.count = 0;
        /* 编号顺序照抄"形参 → 声明顺序"，这样对照口径与复用前的帧布局完全一致，
         * 唯一变量就只剩"是否复用"。 */
        Memory::Map<std::size_t, bool> bound;
        const auto take = [&](std::size_t symbol) {
            const std::size_t index = local(symbol);
            if (index == absent || bound.find(symbol) != bound.end()) return;
            bound.emplace(symbol, true);
            layout.slots.push_back({ symbol, layout.count++ });
        };
        for (const std::size_t symbol : body.parameters) take(symbol);
        for (std::size_t at = 0; at < n; ++at)
        {
            const std::size_t symbol = body.defines[at];
            if (symbol != absent) take(symbol);
        }
        std::sort(layout.slots.begin(), layout.slots.end(), [](const auto &left, const auto &right) { return left.first < right.first; });
    };
    // 对照开关：强制"一人一槽"，用来判定问题出在别名还是槽号编排上。
    if (const char *force = std::getenv("CHALI_ONE_SLOT_PER_SYMBOL"); force != nullptr && *force != '\0')
    {
        onePerSymbol();
        return layout;
    }
    bool unassigned = false;
    layout.slots.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!seen[index]) continue;
        if (intervals[index].slot == absent)
        {
            unassigned = true;
            break;
        }
        layout.slots.push_back({ symbolOf[index], intervals[index].slot });
    }
    if (unassigned)
    {
        onePerSymbol();
        return layout;
    }
    std::sort(layout.slots.begin(), layout.slots.end(), [](const auto &left, const auto &right) { return left.first < right.first; });
    layout.count = nextSlot;

    /* 自检：任何一个位置上同时活着的两个符号都不能共用槽位。这是分配器的硬不变量，
     * 破了就是语义错误（两个变量互相踩），所以要当场发现、当场退回。
     * 只扫"这个位置上真的在意"的符号（活跃位 + 定义 + 使用），不要每个位置都过一遍
     * 全部符号——那是位置数 × 符号数，大函数上会很慢。 */
    Memory::Vector<std::size_t> slotOf(count, absent);
    for (std::size_t index = 0; index < count; ++index)
        if (seen[index]) slotOf[index] = intervals[index].slot;
    Memory::Vector<std::size_t> ownerOf(nextSlot + 1u, absent);
    Memory::Vector<std::size_t> claimed;  ///< 本位置占过的槽位，换位置时只清这些。
    bool conflicting = false;
    const auto claim = [&](std::size_t index, std::size_t at) {
        if (conflicting || index == absent || index >= count || slotOf[index] == absent) return;
        const std::size_t slot = slotOf[index];
        std::size_t &owner = ownerOf[slot];
        if (owner == absent)
        {
            owner = index;
            claimed.push_back(slot);
            return;
        }
        if (owner == index) return;
        conflicting = true;
        noteConflict(symbolOf[owner], symbolOf[index], body.order[at]);
    };
    for (std::size_t at = 0; at < n && !conflicting; ++at)
    {
        claim(defines[at], at);
        claim(uses[at], at);
        for (std::size_t word = 0; word < words && !conflicting; ++word)
        {
            std::uint64_t bits = liveIn[at * words + word] | liveOut[at * words + word];
            while (bits != 0u)
            {
                std::size_t bit = 0;
                while ((bits & (std::uint64_t(1) << bit)) == 0u) ++bit;
                claim(word * 64u + bit, at);
                bits &= ~(std::uint64_t(1) << bit);
                if (conflicting) break;
            }
        }
        // 换到下一个位置之前，只清掉本位置占过的格子。
        for (const std::size_t slot : claimed) ownerOf[slot] = absent;
        claimed.clear();
    }
    if (conflicting) onePerSymbol();

    const std::size_t used = layout.count;
    Stats &total = accumulated();
    ++total.functions;
    total.symbols += count;
    total.slots += used;
    if (stats != nullptr)
    {
        ++stats->functions;
        stats->symbols += count;
        stats->slots += used;
    }
    return layout;
}

Stats lastStats() { return accumulated(); }

void resetStats() { accumulated() = Stats{}; }
}  // namespace allocation
}  // namespace stackvm
