namespace
{

/**
 * @brief 去掉首尾空白。
 * @param text 原文。
 * @return 去空白后的文本。
 */
Memory::String trim(const Memory::String &text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == Memory::String::npos) return Memory::String();
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1u);
}

/**
 * @brief 去掉首尾引号。
 * @param text 原文。
 * @return 去引号后的文本。
 */
Memory::String unquote(const Memory::String &text)
{
    if (text.size() >= 2u && (text.front() == '"' || text.front() == '\'') && text.back() == text.front())
    {
        return text.substr(1u, text.size() - 2u);
    }
    return text;
}

/**
 * @brief 按逗号切分四元式参数：引号里的逗号不算分隔符。
 * @param body 括号里的内容。
 * @return 参数列表（已去空白）。
 */
Memory::Vector<Memory::String> splitArguments(const Memory::String &body)
{
    Memory::Vector<Memory::String> parts;
    Memory::String current;
    char quote = '\0';
    for (const char ch : body)
    {
        if (quote != '\0')
        {
            current += ch;
            if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'')
        {
            quote = ch;
            current += ch;
            continue;
        }
        if (ch == ',')
        {
            parts.push_back(trim(current));
            current.clear();
            continue;
        }
        current += ch;
    }
    if (!current.empty() || !parts.empty()) parts.push_back(trim(current));
    return parts;
}

/**
 * @brief 把文本解析成整数 / 浮点。
 * @param text 文本。
 * @param isFloat 输出：是不是浮点。
 * @param intValue 输出：整数。
 * @param doubleValue 输出：浮点。
 * @return 是数字返回 true。
 */
bool parseNumber(const Memory::String &text, bool &isFloat, std::int64_t &intValue,
                 double &doubleValue)
{
    if (text.empty()) return false;

    // 1. 先按整数试（含负号）。
    std::size_t index = 0;
    if (text[index] == '+' || text[index] == '-') ++index;
    bool digits = false;
    bool isPlainInt = index < text.size();
    for (; index < text.size(); ++index)
    {
        if (text[index] >= '0' && text[index] <= '9')
        {
            digits = true;
            continue;
        }
        isPlainInt = false;
        break;
    }
    if (isPlainInt && digits)
    {
        char *end = nullptr;
        const long long value = std::strtoll(text.c_str(), &end, 10);
        if (end != nullptr && *end == '\0')
        {
            isFloat = false;
            intValue = static_cast<std::int64_t>(value);
            return true;
        }
    }

    // 2. 再按浮点试。
    char *end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if ((end != nullptr) && (*end == '\0') && (end != text.c_str()))
    {
        isFloat = true;
        doubleValue = value;
        intValue = static_cast<std::int64_t>(value);
        return true;
    }
    return false;
}

/**
 * @brief 最短往返的浮点文本（与 library/json 同一套路）。
 * @param value 浮点。
 * @return 文本。
 */
Memory::String formatDouble(double value)
{
    char buffer[64];
    Memory::String text;
    for (int precision = 17; precision >= 1; --precision)
    {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        text = buffer;
        char *end = nullptr;
        if (std::strtod(text.c_str(), &end) == value && end != nullptr && *end == '\0') break;
    }
    return text;
}

/**
 * @brief 浮点**字面量**的四元式文本：一定带小数点（`2` → `2.0`）。
 *
 * 四元式是文本形态，后端按"有没有小数点"区分整数槽位与浮点槽位；折叠出来的
 * 浮点常量必须写成 `2.0`，否则 `(:=, 2, _, t)` 之后按浮点用时会被当成整数。
 * @param value 浮点。
 * @return 文本。
 */
Memory::String formatFloatLiteral(double value)
{
    Memory::String text = formatDouble(value);
    if (text.find_first_of(".eE") == Memory::String::npos) text += ".0";
    return text;
}

/**
 * @brief 按位宽回绕并做符号扩展。
 * @param value 原值。
 * @param type 类型。
 * @return 归一化后的值。
 */
std::int64_t normalizeInt(std::int64_t value, const TypeInfo &type)
{
    if (type.bits <= 0 || type.bits >= 64) return value;
    const std::int64_t mask = (static_cast<std::int64_t>(1) << type.bits) - 1;
    value &= mask;
    if (!type.isUnsigned && (value & (static_cast<std::int64_t>(1) << (type.bits - 1))))
    {
        value -= (static_cast<std::int64_t>(1) << type.bits);
    }
    return value;
}

/**
 * @brief f32 在每次带类型的运算或转换后按 IEEE-754 单精度舍入，f64 保持 double。
 * @param value 待转换、保存或参与计算的值。
 * @param bits 数值类型的位宽。
 * @retval 保存f32 在每次带类型的运算或转换后按 IEEE-754 单精度舍入，f64 保持 double结果的对象。
 */
double normalizeFloat(double value, int bits)
{
    return bits == 32 ? static_cast<double>(static_cast<float>(value)) : value;
}

/**
 * 栈里的一个值。
 */
struct Slot
{
    bool isFloat = false;  ///< 是否浮点。
    bool isText = false;   ///< 是否字符串。
    std::int64_t integer = 0;
    double real = 0.0;
    Memory::String text;

    /**
     * @brief 造一个整数。
     * @param value 值。
     * @return 槽位。
     */
    static Slot fromInt(std::int64_t value)
    {
        Slot slot;
        slot.integer = value;
        return slot;
    }

    /**
     * @brief 造一个浮点。
     * @param value 值。
     * @return 槽位。
     */
    static Slot fromDouble(double value)
    {
        Slot slot;
        slot.isFloat = true;
        slot.real = value;
        return slot;
    }

    /**
     * @brief 造一个字符串。
     * @param value 文本。
     * @return 槽位。
     */
    static Slot fromText(Memory::String value)
    {
        Slot slot;
        slot.isText = true;
        slot.text = std::move(value);
        return slot;
    }

    /**
     * @brief 将当前值槽中的整数或浮点数读取为 double。
     *
     * 取出数值（整数或浮点都行）。
     * @retval 值槽中的整数经 double 转换后的值，或原有浮点值。
     */
    [[nodiscard]] double number() const { return isFloat ? real : static_cast<double>(integer); }
};

/**
 * @brief 虚拟机自己的堆：一块连续的字节区 + 空闲块表。
 *
 * **为什么不放在数据区里**：数据区（`.data.bin`）是编译期定好符号与槽位的静态内存，
 * 程序跑起来只有那一片；而 `new` / `malloc` 要多少字节是运行期才知道的，也不该跟
 * 静态数据抢地址。所以堆单独长，只和数据区**共用一套地址空间**：地址从 `base()`
 * 起算（紧跟在数据区后面），`(load, 地址, …)` / `(store, 地址, …)` 两边通用 ——
 * 对象既不在数据区里，也不在求值栈 / 调用帧里，什么时候还回来由程序自己 `free`。
 *
 * 分配策略：**按 1 字节单元** + 首次适配 + 相邻空闲块
 * 合并——**释放过的块会被后续分配复用**，块表也不会随"反复申请 / 释放"越滚越大，
 * 物理块继续复用，逻辑地址则单调分配；旧引用不会命中新对象。
 * 已释放块首保留墓碑，重复 release 成功且无效果，内部地址及未知地址仍报错。
 * 读写通过逻辑地址映射到物理偏移，仅允许访问当前活跃分配。
 *
 * 线程安全：`allocate` / `release` 由一把互斥锁串行化（宿主回调 / 多线程场景下
 * 同一个堆可能被并发触发分配与释放）。只读查询不加锁——堆按 `run()` / 线程独占，
 * 契约是"读的时候没有并发写"。
 */
#if STACKVM_WITH_DEFAULT_HEAP
class Heap final : public HeapAllocator
{
public:
    /**
     * @brief 一块内存。
     */
    struct Block
    {
        std::size_t address = 0;  ///< 本次分配的逻辑地址（不会复用）。
        std::size_t offset = 0;   ///< 可复用的物理字节区偏移。
        std::size_t size = 0;     ///< 字节数。
        bool free = true;         ///< 是否空闲。
    };

    /**
     * @brief 默认构造后由 Machine 在每次 run() 前 reset。
     */
    Heap() = default;

    /**
     * @brief 重置整个运行的槽与活跃分配。
     * @param base 数据区之后的堆基址。
     * @return 无。
     */
    void reset(std::size_t base) override
    {
        base_ = base < kRuntimeCellBytes ? kRuntimeCellBytes : base;
        bytes_.clear();
        blocks_.clear();
        peak_ = 0u;
        index_.reset(base_);
    }

    /**
     * @brief 取得底层内存区域的起始地址。
     *
     * 堆区起始地址。
     * @retval 对应运行时内存区域的地址。
     */
    [[nodiscard]] std::size_t base() const override { return base_; }

    /**
     * @brief 分配一块字节寻址的内存。
     * @param bytes 字节数。
     * @return 地址；参数为 0 或没内存了返回 0。
     */
    std::size_t allocate(std::size_t bytes) override
    {
        if (bytes == 0u) return 0u;
        std::lock_guard<std::mutex> guard(mutex_);
        if (!index_.available(bytes)) return 0u;

        /* 先在**已释放**的块里首次适配：够大就切下一段复用，剩下的留成新的空闲块。
         * 这就是"链表式"分配器：空闲块在块表里串着，重复申请 / 释放不会让高水位一直涨。 */
        for (std::size_t index = 0; index < blocks_.size(); ++index)
        {
            if (!blocks_[index].free || blocks_[index].size < bytes) continue;
            std::size_t offset = 0;
            if (!index_.placement(blocks_[index].offset, blocks_[index].size, bytes, offset)) continue;
            if (offset != blocks_[index].offset)
            {
                const Block original = blocks_[index];
                Block tail;
                tail.offset = offset;
                tail.size = original.size - (offset - original.offset);
                blocks_[index].size = offset - original.offset;
                try
                {
                    blocks_.insert(blocks_.begin() + static_cast<std::ptrdiff_t>(index + 1), tail);
                } catch (const std::bad_alloc &)
                {
                    blocks_[index] = original;
                    return 0u;
                } catch (const std::length_error &)
                {
                    blocks_[index] = original;
                    return 0u;
                }
                ++index;
            }
            Block &block = blocks_[index];
            const std::size_t address = index_.add(offset, bytes);
            if (!address) return 0u;
            block.address = address;
            const std::size_t leftover = block.size - bytes;
            block.size = bytes;
            block.free = false;
            if (leftover > 0u)
            {
                Block rest;
                rest.offset = offset + bytes;
                rest.size = leftover;
                rest.free = true;
                try
                {
                    blocks_.insert(
                        blocks_.begin() + static_cast<std::ptrdiff_t>(index + 1u), rest);
                } catch (const std::bad_alloc &)
                {
                    index_.release(address);
                    blocks_[index].size = bytes + leftover;
                    blocks_[index].free = true;
                    return 0u;
                } catch (const std::length_error &)
                {
                    index_.release(address);
                    blocks_[index].size = bytes + leftover;
                    blocks_[index].free = true;
                    return 0u;
                }
            }
            std::memset(bytes_.data() + offset, 0, bytes);
            refreshPeak();
            return address;
        }

        /* 没有可复用的空闲块：往高地址再长一块。 */
        if (bytes_.size() > std::numeric_limits<std::size_t>::max() - base_ || bytes > std::numeric_limits<std::size_t>::max() - base_ - bytes_.size() || bytes > bytes_.max_size() - bytes_.size()) return 0u;
        Block block;
        block.offset = bytes_.size();
        block.address = 0;
        block.size = bytes;
        block.free = false;
        try
        {
            bytes_.resize(bytes_.size() + bytes, 0u);
        } catch (const std::bad_alloc &) { return 0u; } catch (const std::length_error &)
        {
            return 0u;
        }
        try
        {
            blocks_.push_back(block);
        } catch (const std::bad_alloc &)
        {
            bytes_.resize(bytes_.size() - bytes);
            return 0u;
        } catch (const std::length_error &)
        {
            bytes_.resize(bytes_.size() - bytes);
            return 0u;
        }
        blocks_.back().address = block.address = index_.add(block.offset, bytes);
        if (!block.address)
        {
            blocks_.pop_back();
            bytes_.resize(block.offset);
            return 0u;
        }
        refreshPeak();
        return block.address;
    }

    /**
     * @brief 释放一块内存（地址 0 当空指针放过；相邻空闲块合并）。
     * @param address 地址。
     * @param reason 失败原因（可选）。
     * @return 成功（或空指针）返回 true。
     */
    bool release(std::size_t address, Memory::String *reason = nullptr) override
    {
        if (address == 0u) return true;  // free(NULL) 是空操作
        std::lock_guard<std::mutex> guard(mutex_);
        if (index_.retired(address)) return true;
        for (std::size_t index = 0; index < blocks_.size(); ++index)
        {
            Block &block = blocks_[index];
            if (block.free || block.address != address) continue;
            index_.release(address);
            block.free = true;
            /* 和左右相邻的空闲块合并：块表不会因为"反复申请 / 释放"越来越多。 */
            if (index + 1u < blocks_.size() && blocks_[index + 1u].free)
            {
                blocks_[index].size += blocks_[index + 1u].size;
                blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(index + 1u));
            }
            if (index > 0u && blocks_[index - 1u].free)
            {
                blocks_[index - 1u].size += blocks_[index].size;
                blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(index));
            }
            return true;
        }
        if (reason != nullptr)
        {
            *reason = "[MGL1205] 无效或已清扫的释放身份：地址 " + Memory::toString(address) + " 不在堆里（堆区从 " + Memory::toString(base_) + " 起）";
        }
        return false;
    }

    /**
     * @brief 清扫释放身份，保留活跃分配。
     * @return 被清扫的身份数。
     */
    std::size_t sweep() override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return index_.sweep();
    }
    /**
     * @brief 查询幂等释放身份。
     * @param address 块首。
     * @return 未清扫的墓碑为真。
     */
    bool retired(std::size_t address) const override { return index_.retired(address); }

    /**
     * @brief 判断是否属于编码堆空间，包括已清扫身份。
     * @param address 地址。
     * @return 属于堆空间为真。
     */
    [[nodiscard]] bool contains(std::size_t address) const override
    {
        return index_.contains(address);
    }

    /**
     * @brief 地址所在的**已分配块**从这里起还能用多少字节。
     * @param address 地址。
     * @param capacity 输出：从 address 到块尾的字节数。
     * @return 地址不在任何已分配块里（没分配 / 已经 free）返回 false。
     */
    bool capacityAt(std::size_t address, std::size_t &capacity) const override
    {
        const auto *block = index_.find(address);
        if (!block) return false;
        capacity = block->size - (address - block->address);
        return true;
    }

    /**
     * @brief 查询活跃字节数。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t liveBytes() const override
    {
        return index_.liveBytes();
    }
    /**
     * @brief 查询活跃块数。
     * @return 块数。
     */
    [[nodiscard]] std::size_t liveBlocks() const override { return index_.liveBlocks(); }

    /**
     * @brief 查询活跃字节峰值。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t peakBytes() const override { return peak_; }

    /**
     * @brief 读 8 字节（只允许读**已分配**的块）。
     * @param address 地址。
     * @param out 结果。
     * @return 越界 / 已释放返回 false。
     */
    bool readInt(std::size_t address, std::int64_t &out) const
    {
        if (!usable(address)) return false;
        std::memcpy(&out, bytes_.data() + physicalOffset(address), sizeof(out));
        return true;
    }

    /**
     * @brief 写 8 字节。
     * @param address 地址。
     * @param value 值。
     * @return 越界 / 已释放返回 false。
     */
    bool writeInt(std::size_t address, std::int64_t value)
    {
        if (!usable(address)) return false;
        std::memcpy(bytes_.data() + physicalOffset(address), &value, sizeof(value));
        return true;
    }

    /**
     * @brief 读 8 字节，按 IEEE-754 双精度解释。
     * @param address 地址。
     * @param out 结果。
     * @return 越界 / 已释放返回 false。
     */
    bool readDouble(std::size_t address, double &out) const
    {
        if (!usable(address)) return false;
        std::memcpy(&out, bytes_.data() + physicalOffset(address), sizeof(out));
        return true;
    }

    /**
     * @brief 把双精度写成 8 字节。
     * @param address 地址。
     * @param value 值。
     * @return 越界 / 已释放返回 false。
     */
    bool writeDouble(std::size_t address, double value)
    {
        if (!usable(address)) return false;
        std::memcpy(bytes_.data() + physicalOffset(address), &value, sizeof(value));
        return true;
    }

    /**
     * @brief 按字节读一段（字符串跨 8 字节槽，不能按槽读）。
     * @param address 地址。
     * @param out 输出缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 这 count 个字节全在**同一块已分配内存**里返回 true。
     */
    bool readBytes(std::size_t address, char *out, std::size_t count) const override
    {
        if (count == 0u) return true;
        if (!usableRange(address, count)) return false;
        std::memcpy(out, bytes_.data() + physicalOffset(address), count);
        return true;
    }

    /**
     * @brief 按字节写一段。
     * @param address 地址。
     * @param in 输入缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 这 count 个字节全在**同一块已分配内存**里返回 true。
     */
    bool writeBytes(std::size_t address, const char *in, std::size_t count) override
    {
        if (count == 0u) return true;
        if (!usableRange(address, count)) return false;
        std::memcpy(bytes_.data() + physicalOffset(address), in, count);
        return true;
    }

private:
    /**
     * @brief 将有效逻辑地址转换为物理偏移。
     * @param address 已验证的逻辑地址。
     * @return 字节区偏移。
     */
    std::size_t physicalOffset(std::size_t address) const
    {
        const auto *block = index_.find(address);
        return block ? block->offset + address - block->address : bytes_.size();
    }
    /**
     * @brief 整段范围必须落在同一活跃分配内。
     * @param address 地址。
     * @param count 长度。
     * @return 可访问时为 true。
     */
    [[nodiscard]] bool usableRange(std::size_t address, std::size_t count) const
    {
        const auto *block = index_.find(address);
        return block && count <= block->size - (address - block->address);
    }

    /**
     * @brief 地址是否可读写：落在某个**已分配**块里，且 8 个字节都在块内。
     * @param address 运行时内存地址。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    [[nodiscard]] bool usable(std::size_t address) const { return usableRange(address, 8u); }

    /**
     * @brief 更新峰值。
     */
    void refreshPeak()
    {
        const std::size_t live = liveBytes();
        if (live > peak_) peak_ = live;
    }

    execution_detail::AllocationIndex index_;  ///< 两端共用的逻辑区间索引与缓存。
    std::size_t base_ = 8u;                    ///< 堆区起始地址。
    Memory::Vector<std::uint8_t> bytes_;       ///< 字节区（从 base_ 开始）。
    Memory::Vector<Block> blocks_;             ///< 块表，按物理偏移升序。
    std::size_t peak_ = 0u;                    ///< 峰值。
    mutable std::mutex mutex_;                 ///< 串行化 allocate / release。
};
#else  /* !STACKVM_WITH_DEFAULT_HEAP */
/**
 * @brief 板端"不编默认字节堆"时的空堆：**所有分配都失败**，统计恒为 0。
 *
 * 板端必须由宿主注入字节池（`mcuArenas::SetBytePool` → `Machine::setHeapAllocator`）。
 * 万一没注入，这里让每次分配明确失败（`alloc` 报"堆内存不足" → 记分配失败标志 →
 * 下一个安全点可恢复终止），而不是悄悄退化成宿主堆 —— 板端根本没有宿主堆可用。
 */
class NoHeapAllocator final : public HeapAllocator
{
public:
    /**
     * @brief 将内存分配器恢复到初始状态。
     */
    void reset(std::size_t) override {}
    /**
     * @brief 取得底层内存区域的起始地址。
     * @retval 对应运行时内存区域的地址。
     */
    [[nodiscard]] std::size_t base() const override { return kRuntimeCellBytes; }
    /**
     * @brief 分配指定字节数的运行时内存。
     * @retval 对应运行时内存区域的地址。
     */
    [[nodiscard]] std::size_t allocate(std::size_t) override { return 0u; }
    /**
     * @brief 释放指定地址对应的运行时资源。
     * @param reason 接收释放失败原因或停止原因的对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool release(std::size_t, Memory::String *reason = nullptr) override
    {
        if (reason != nullptr) *reason = "这个构建没有默认字节堆（必须由宿主注入字节池）";
        return false;
    }
    /**
     * @brief 判断地址是否属于当前内存区域。
     * @retval 地址属于当前内存区域时为 true，否则为 false。
     */
    [[nodiscard]] bool contains(std::size_t) const override { return false; }
    /**
     * @brief 查询指定地址所在分配块的容量。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    [[nodiscard]] bool capacityAt(std::size_t, std::size_t &) const override { return false; }
    /**
     * @brief 查询活跃字节数。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t liveBytes() const override { return 0u; }
    /**
     * @brief 查询活跃块数。
     * @return 块数。
     */
    [[nodiscard]] std::size_t liveBlocks() const override { return 0u; }
    /**
     * @brief 查询活跃字节峰值。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t peakBytes() const override { return 0u; }
    /**
     * @brief 从指定地址读取一段字节数据。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    [[nodiscard]] bool readBytes(std::size_t, char *, std::size_t) const override { return false; }
    /**
     * @brief 将字节数据写入指定地址。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool writeBytes(std::size_t, const char *, std::size_t) override { return false; }
};
#endif /* STACKVM_WITH_DEFAULT_HEAP */

/**
 * @brief 调用帧：一层调用的局部变量存储 + 返回信息。
 *
 * 每个活动调用独占局部槽和返回信息，递归与协程的活动帧互不覆盖。
 * 返回时先写回结果并销毁局部值；执行优化可限量缓存已清空的小帧及其向量容量，
 * 下次调用重新准备布局、声明状态和参数。未缓存的帧由 UniquePtr 自动销毁。
 * 缓存只属于本次 Machine::run，不保留活动帧指针，也不延迟局部值的释放。
 */
struct Frame
{
    const execution_detail::FrameLayout *layout = nullptr;  ///< 本函数的稠密槽布局。
    Memory::Vector<Slot> slots;                             ///< 不按名称查询的局部存储。
    Memory::Vector<bool> bound;                             ///< 保留声明执行前的未定义与全局回退语义。
    Memory::Map<Memory::String, Slot> locals;               ///< 本帧的局部变量（形参 + 函数体里声明的）。
    std::size_t returnIp = 0;                               ///< 返回地址。
#if STACKVM_WITH_PANIC_TRACE
    Memory::String function;  ///< 实际被调函数的符号；压缩产物中为逻辑编号。
#endif
    Memory::String returnDest;  ///< 返回值写回调用方的哪个变量（可空）。
};

/**
 * 待绑定的实参。
 */
struct PendingArg
{
    Memory::String name;  ///< 形参名。
    Slot value;           ///< 值。
};

struct TransferPacket;

/**
 * 一个可暂停、可恢复的 VM 执行上下文。全局区、堆、数据区和对象表由所有协程共享。
 */
struct CoroutineContext
{
    enum class State
    {
        Running,
        Ready,
        Suspended,
        Waiting,
        Receiving,
        Done
    };

    Memory::Vector<Slot> stack;
    Memory::Vector<Memory::UniquePtr<Frame>> frames;
    Memory::Vector<PendingArg> pending;
    std::size_t pc = 0;
    State state = State::Ready;
    std::size_t waitingFor = 0;
    std::size_t parent = 0;
    Memory::Vector<std::shared_ptr<TransferPacket>> inbox;
};

/* ---------------------------------------------------------------------------
 *  内部 C++ 运行时：字符串 / 格式化
 * ---------------------------------------------------------------------------
 *
 * 运行时字符串是“8 字节长度 + UTF-8 字节数据”，不附加结尾 NUL；数据区字面量只是
 * 复制源，仍用生成期哨兵界定。**读字节 / 拼接 / 切片 / 格式化这些活儿全在这层 C++ 里干**，宿主语言
 * （magel）只发一次调用 —— 不用在四元式里手写逐字节的循环，也就能顺手用上
 * `Memory::String` / `snprintf`。
 *
 * 与数学原生函数（`kNatives`）的两点不同：
 * 1. 参数与结果都是 **Slot**：地址 / 长度必须落在**整数槽**里（`(load, 地址, …)`
 *    认的是 `Slot.integer`，浮点槽的 `integer` 是 0，塞进去就成空指针了）；
 * 2. 要能用堆（造新串）与数据区（读字面量），还要能报错 —— 所以带一个 NativeContext。
 */

using NativeInvoker = Memory::Function<bool(std::int64_t, const Memory::Vector<NativeValue> &,
                                            NativeValue &)>;

/**
 * 一次 run() 里的软件定时器表（`std::event::Timer` 的后端）。
 */
struct TimerRuntime;

/**
 * 原生函数跑起来时的环境。
 */
struct NativeContext
{
    HeapAllocator *heap = nullptr;                                               ///< 堆：造出来的字符串放这儿。
    const DataArea *data = nullptr;                                              ///< 数据区：字面量所在的那片静态内存。
    Memory::String *text = nullptr;                                              ///< 文本输出桶（`printf` 往里追加）。
    OutputSink textSink = nullptr;                                               ///< 文本输出下沉（`Machine::setOutputSink`）；空 = 只进桶。
    void *textSinkUser = nullptr;                                                ///< 下沉上下文。
    const bool *textKeep = nullptr;                                              ///< 有下沉时是否仍累积进文本桶；空 = 累积。
    const Memory::Vector<Memory::String> *input = nullptr;                       ///< 输入项（按顺序取）。
    std::size_t *inputIndex = nullptr;                                           ///< 已经取到第几项。
    const Memory::Vector<Memory::String> *programArguments = nullptr;            ///< 程序入口参数。
    NativeInvoker *invoker = nullptr;                                            ///< 同步调用 MAGEL 函数指针。
    struct ObjectStore *objects = nullptr;                                       ///< 运行期对象表（bint / 容器）。
    TimerRuntime *timers = nullptr;                                              ///< 软件定时器表（中断事件）。
    Machine *machine = nullptr;                                                  ///< 正在执行 run() 的虚拟机（胶水投递中断用）。
    Memory::Function<void(std::size_t, InstructionHook)> instructionHookSetter;  ///< AOT 执行边界钩子入口。
    Memory::Function<void(std::int64_t, std::int64_t)> interruptPosterFn;        ///< AOT 中断投递入口。
    std::size_t threadId = 0u;                                                   ///< 当前线程 id（定时器归属线程）。
    int *bigFloatPrecision = nullptr;                                            ///< bfloat 除法的有效位数（本次 run 独立）。
    bool *exitRequested = nullptr;                                               ///< `exit(n)` 请求提前正常停机。
    int *exitCode = nullptr;                                                     ///< `exit(n)` 提供的宿主状态码。
    FileApi *fileApi = nullptr;                                                  ///< 文件系统适配器。
    Memory::String error;                                                        ///< 非空 = 出错，调用方负责终止运行。
    mutable Memory::String memoryFault;                                          ///< 首次非法访问，优先于回调随后生成的普通错误。
};

/**
 * 扫数据区字面量时最多看多少字节（运行时堆字符串直接读取长度）。
 */
constexpr std::size_t kStringScanLimit = 1u << 20;

/**
 * @brief 从堆或数据区里读一段字节。
 * @param ctx 环境。
 * @param address 地址。
 * @param out 输出缓冲。
 * @param count 字节数。
 * @return 读得到返回 true（已释放 / 越界返回 false）。
 */
bool nativeRead(const NativeContext &ctx, std::size_t address, char *out, std::size_t count)
{
    if (count == 0) return true;
#if STACKVM_WITH_ALLOCATION_GUARDS
    const bool heap = ctx.heap != nullptr && ctx.heap->contains(address);
    const bool valid = heap ? allocatedHeapRange(*ctx.heap, address, count) :
                              ctx.data != nullptr && address <= ctx.data->size() && count <= ctx.data->size() - address;
    if (!valid)
    {
        if (ctx.memoryFault.empty()) ctx.memoryFault = MAGEL_TEXT("panic：访问未分配内存（读）：地址 ") + Memory::toString(address) + MAGEL_TEXT("，字节数 ") + Memory::toString(count);
        return false;
    }
#endif
    if (ctx.heap != nullptr && ctx.heap->contains(address))
    {
        return ctx.heap->readBytes(address, out, count);
    }
    return (ctx.data != nullptr) && ctx.data->readBytes(address, out, count);
}

/**
 * @brief 往堆或数据区按字节写一段（改字符串里的某个字节用它）。
 * @param ctx 环境。
 * @param address 地址。
 * @param in 输入缓冲。
 * @param count 字节数。
 * @return 成功返回 true。
 */
bool nativeWrite(NativeContext &ctx, std::size_t address, const char *in, std::size_t count)
{
    if (count == 0) return true;
#if STACKVM_WITH_ALLOCATION_GUARDS
    const bool heap = ctx.heap != nullptr && ctx.heap->contains(address);
    const bool valid = heap ? allocatedHeapRange(*ctx.heap, address, count) :
                              ctx.data != nullptr && address <= ctx.data->size() && count <= ctx.data->size() - address;
    if (!valid)
    {
        if (ctx.memoryFault.empty()) ctx.memoryFault = MAGEL_TEXT("panic：访问未分配内存（写）：地址 ") + Memory::toString(address) + MAGEL_TEXT("，字节数 ") + Memory::toString(count);
        return false;
    }
#endif
    if (ctx.heap != nullptr && ctx.heap->contains(address))
    {
        return ctx.heap->writeBytes(address, in, count);
    }
    return (ctx.data != nullptr) && const_cast<DataArea *>(ctx.data)->writeBytes(address, in, count);
}

/**
 * @brief 读取显式长度的堆字符串，或读取生成期带哨兵的数据区字面量。
 * @param ctx 环境。
 * @param address 地址（0 = 空指针）。
 * @param out 结果。
 * @return 成功返回 true；长度非法、地址不可读或数据区哨兵缺失返回 false。
 */
bool nativeReadCString(const NativeContext &ctx, std::size_t address, Memory::String *out)
{
    out->clear();
    if (address == 0u) return true;  ///< 空指针当空串
    if (ctx.heap != nullptr && ctx.heap->contains(address))
    {
        std::int64_t length = 0;
        std::size_t capacity = 0u;
        if (!nativeRead(ctx, address, reinterpret_cast<char *>(&length), sizeof(length)) || length < 0 || !ctx.heap->capacityAt(address, capacity) || static_cast<std::uint64_t>(length) > capacity - std::min<std::size_t>(capacity, 8u))
            return false;
        out->resize(static_cast<std::size_t>(length));
        return length == 0 || nativeRead(ctx, address + 8u, out->data(), out->size());
    }
    for (std::size_t index = 0; index < kStringScanLimit; ++index)
    {
        char ch = 0;
        if (!nativeRead(ctx, address + index, &ch, 1u)) return false;
        if (ch == '\0') return true;
        out->push_back(ch);
    }
    return false;
}

/**
 * @brief 分配新存储并清除旧地址的销毁标记。
 * @param ctx 运行环境。
 * @param bytes 字节数。
 * @return 新地址，失败返回零。
 */
std::size_t nativeAllocate(NativeContext &ctx, std::size_t bytes);

/**
 * @brief 造一个显式长度的堆字符串，返回长度字段的地址。
 * @param ctx 环境。
 * @param text UTF-8 字节内容。
 * @return 地址；0 = 失败（`ctx.error` 里是原因）。
 */
std::size_t nativeAllocString(NativeContext &ctx, const Memory::String &text)
{
    if (ctx.heap == nullptr) return 0u;
    const std::size_t bytes = text.size() + 8u;
    const std::size_t address = nativeAllocate(ctx, bytes);
    if (address == 0u)
    {
        ctx.error = "堆内存不足：拼接 / 格式化需要 " + Memory::toString(bytes) + " 字节";
        return 0u;
    }
    if (!ctx.heap->writeInt(address, static_cast<std::int64_t>(text.size())) || (!text.empty() && !ctx.heap->writeBytes(address + 8u, text.data(), text.size())))
    {
        ctx.error = "往堆上写字符串失败（地址 " + Memory::toString(address) + "）";
        return 0u;
    }
    return address;
}

/**
 * 把私有运行时环境收窄成公开胶水接口。
 */
class NativeCallContextImpl final : public NativeCallContext
{
public:
    using Invoker = NativeInvoker;

    /**
     * @brief 为原生函数调用关联虚拟机与临时对象集合。
     * @param context 当前处理或执行上下文。
     * @param invoker 调用运行时函数的宿主回调。
     * @param invocationError 运行时回调失败的原始诊断，可为空。
     */
    explicit NativeCallContextImpl(NativeContext &context, Invoker invoker = {}, const Memory::String *invocationError = nullptr) : context_(context), invoker_(std::move(invoker)), invocationError_(invocationError) {}

    /**
     * @brief 按字节读一段（字符串是"以 `\0` 结尾的字节串"，跨 8 字节槽）。
     * @param address 地址（数据区内的偏移）。
     * @param out 输出缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 越界返回 false。
     */
    bool readBytes(std::int64_t address, char *out, std::size_t count) const override
    {
        return address >= 0 && nativeRead(context_, static_cast<std::size_t>(address), out, count);
    }

    /**
     * @brief 将字节数据写入指定地址。
     * @param address 运行时内存地址。
     * @param data 输入或待写入的字节数据。
     * @param count 元素数量或操作次数。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool writeBytes(std::int64_t address, const char *data, std::size_t count) override
    {
        return address >= 0 && nativeWrite(context_, static_cast<std::size_t>(address), data, count);
    }

    /**
     * @brief 从原生值对应的地址读取字符串。
     * @param address 运行时内存地址。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool readString(std::int64_t address, Memory::String &out) const override
    {
        return address >= 0 && nativeReadCString(context_, static_cast<std::size_t>(address), &out);
    }

    bool readList(std::int64_t handle, Memory::Vector<NativeValue> &out) const override;

    std::int64_t makeList() override;
    bool listPush(std::int64_t listHandle, const NativeValue &value) override;
    bool listPushStruct(std::int64_t listHandle, std::int64_t structAddress,
                        std::size_t slotCount) override;
    bool readListStruct(std::int64_t handle, std::size_t slotCount,
                        Memory::Vector<double> &out) const override;
    std::int64_t makeMap() override;
    std::int64_t makeSet() override;
    bool mapSet(std::int64_t mapHandle, const NativeValue &key, const NativeValue &value) override;
    bool setInsert(std::int64_t setHandle, const NativeValue &value) override;
    bool readMap(std::int64_t handle, Memory::Vector<NativeValue> &keys,
                 Memory::Vector<NativeValue> &values) const override;
    bool readSet(std::int64_t handle, Memory::Vector<NativeValue> &elements) const override;

    /**
     * @brief 通过当前虚拟机调用指定函数。
     * @param functionPointer 被调用函数的运行时地址。
     * @param arguments 按调用顺序保存的实参列表。
     * @param result 接收处理结果的对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool invoke(std::int64_t functionPointer, const Memory::Vector<NativeValue> &arguments,
                NativeValue &result) override
    {
        if (invoker_ && invoker_(functionPointer, arguments, result)) return true;
        if (context_.error.empty())
            context_.error = invocationError_ && !invocationError_->empty() ? *invocationError_ : "当前运行时无法调用指定函数指针";
        return false;
    }

    /**
     * @brief 在运行时内存中创建字符串并返回其原生值。
     * @param text 待处理的文本。
     * @retval 在运行时内存中创建字符串并返回其原生值。
     */
    std::int64_t makeString(const Memory::String &text) override
    {
        return static_cast<std::int64_t>(nativeAllocString(context_, text));
    }

    std::int64_t makeTemporary(std::size_t bytes) override;

    /**
     * @brief 报告错误；panic 会请求整个线程组停止。
     * @param message 错误信息。
     */
    void fail(const Memory::String &message) override { context_.error = message; }

#if STACKVM_WITH_THREADS
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
    void raiseInterrupt(std::int64_t timer, std::int64_t source) override
    {
        auto post = interruptPoster();
        if (post) post(timer, source);
    }

    /**
     * @brief 生成借用运行时的投递器，不捕获短命的宿主调用上下文。
     * @return 投递函数或空函数。
     */
    Memory::Function<void(std::int64_t, std::int64_t)> interruptPoster() override
    {
        if (auto *machine = context_.machine)
            return [machine](std::int64_t timer, std::int64_t source) { machine->raiseInterrupt(timer, source); };
        return context_.interruptPosterFn;
    }

#endif

    /**
     * @brief 为 VM 或 AOT 安装执行钩子。
     * @param interval 间隔。
     * @param hook 钩子。
     * @return 是否支持。
     */
    bool setInstructionHook(std::size_t interval, InstructionHook hook) override
    {
        if (context_.machine)
        {
            context_.machine->setInstructionHook(interval, std::move(hook));
            return true;
        }
        if (!context_.instructionHookSetter) return false;
        context_.instructionHookSetter(interval, std::move(hook));
        return true;
    }

    /**
     * @brief 当前正在执行 `run()` 的虚拟机；宿主回调之外（指令钩子 / Qt 事件循环）也能用。
     *
     * 胶水拿到它以后可以在**宿主回调返回之后**继续工作：投递中断
     * （`Machine::raiseInterrupt`）或挂每 N 条指令一次的钩子
     * （`Machine::setInstructionHook`）。只在 VM 执行线程上使用；指针在 `Machine`
     * 生命周期内有效，`run()` 结束后不要再碰。
     * @return 正在运行的虚拟机；没有（例如离线编解码）返回 nullptr。
     */
    Machine *machine() override { return context_.machine; }

private:
    NativeContext &context_;
    Invoker invoker_;
    const Memory::String *invocationError_ = nullptr;
};

/**
 * @brief 取一个实参的整数值（整数槽直接取，浮点槽取整）。
 * @param slot 待转换或传递的虚拟机值槽。
 * @retval 取一个实参的整数值（整数槽直接取，浮点槽取整）。
 */
std::int64_t nativeInt(const Slot &slot)
{
    return slot.isFloat ? static_cast<std::int64_t>(slot.real) : slot.integer;
}

/**
 * @brief 整数 → 十进制文本（`int` 是 64 位，所以按 `long long` 打）。
 * @param value 值。
 * @return 文本。
 */
Memory::String nativeIntText(std::int64_t value) { return Memory::toString(value); }

/**
 * @brief 浮点 → 文本：默认保留 3 位小数（语言的默认），去掉尾随的 0。
 *
 * 与 `%f` 的默认精度保持一致：`3.5` 打成 `3.5`（不是 `3.500`），`2.0` 打成 `2.0`。
 * @param value 值。
 * @param precision 小数位数。
 * @return 文本。
 */
Memory::String nativeFloatText(double value, int precision)
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

/**
 * 一个"内部 C++"原生函数（字符串 / 格式化那一套）。
 */
struct SlotNative
{
    /**
     * 参数个数；**负数 = 变参**（吃光这次攒下的所有实参：`printf` / `format` 用它，
     * 第一个实参是格式串，其余按顺序对应占位符）。
     */
    int arity = 0;
    /// 实现。
    Slot (*apply)(NativeContext &, const Memory::Vector<Slot> &) = nullptr;
};

/**
 * @brief `__vm_str_lit(地址) -> 堆上的新串地址`：把只读的字面量字节复制成一份**可写**的堆串。
 *
 * 字符串的"值"永远是堆地址（`\0` 结尾的字节串），数据区里只留只读的字面量字节当
 * 复制源 —— 于是每个 `string` 变量手里那块是自己能写的，`s[i] = 字符` 不会改到
 * 共享的字面量；所有权也就统一了：**变量即那块堆串的主人**。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strLit(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "复制字符串字面量时读不到它（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, text)));
}

/**
 * @brief `__vm_str_len(地址) -> 长度`。
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
 * @brief `__vm_str_join(串…) -> 新串地址`：把**一串**字符串接成一个（变参）。
 *
 * 之所以一次接一整条链（而不是两个两个接）：`a + b + c` 那样两两接会留下一个中间串，
 * 而中间串没有哪个变量"拥有"它 —— 没人释放（漏）。摊平成一次调用就只分配一个结果，
 * 它正好归被赋值的那个变量所有。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strJoin(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    for (const Slot &argument : args)
    {
        Memory::String part;
        if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(argument)), &part))
        {
            ctx.error = "拼接字符串时读不到操作数（地址 " + Memory::toString(nativeInt(argument)) + "）";
            return Slot::fromInt(0);
        }
        text += part;
    }
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, text)));
}

/**
 * `__vm_str_sub(串, 起, 止) -> 新串地址`：半开区间 `[起, 止)`，**越界自动夹到范围内**；
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
 * @brief 按字节区间截取字符串，并检查 UTF-8 端点边界。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strSub(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "切片时读不到字符串（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Slot::fromInt(0);
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
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(
        ctx, text.substr(static_cast<std::size_t>(begin),
                         static_cast<std::size_t>(end - begin)))));
}

/**
 * @brief `__vm_str_of_int(值) -> 新串地址`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strOfInt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, nativeIntText(nativeInt(args[0])))));
}

/**
 * @brief `__vm_str_of_flt(值) -> 新串地址`（默认 3 位小数，`精度` 可覆盖）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strOfFlt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const double value = args[0].isFloat ? args[0].real : static_cast<double>(args[0].integer);
    const int precision = (args.size() > 1u) ? static_cast<int>(nativeInt(args[1])) : 3;
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, nativeFloatText(value, precision))));
}

/**
 * @brief `__vm_str_fmt(格式串, 实参…) -> 新串地址`。
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
 * @brief `__vm_print(格式串, 实参…) -> 0`：把格式化结果追加到文本输出桶（`printf` 用它）。
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
 * @brief `__vm_str_cmp(左, 右) -> -1 / 0 / 1`（字典序；空指针当空串）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strCmp(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String left, right;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &left) || !nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &right))
    {
        ctx.error = "比较字符串时读不到操作数（地址 " + Memory::toString(nativeInt(args[0])) + " / " + Memory::toString(nativeInt(args[1])) + "）";
        return Slot::fromInt(0);
    }
    const int order = left.compare(right);
    return Slot::fromInt(order < 0 ? -1 : (order > 0 ? 1 : 0));
}

/**
 * @brief `__vm_str_pos(串, 子串) -> 位置 / -1`：子串第一次出现的位置。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 子串首次出现的字节位置，未找到时为 -1。
 */
Slot strPos(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text, needle;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text) || !nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &needle))
    {
        ctx.error = "查找子串时读不到操作数（地址 " + Memory::toString(nativeInt(args[0])) + " / " + Memory::toString(nativeInt(args[1])) + "）";
        return Slot::fromInt(-1);
    }
    const std::size_t found = text.find(needle);
    return Slot::fromInt(found == Memory::String::npos ? -1 : static_cast<std::int64_t>(found));
}

/**
 * @brief `__vm_str_set(串, 下标, 字符码) -> 0`：往字符串的第 `下标` 个字节写一个字符。
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
 * @brief `__vm_str_at(串, 下标) -> 字符码 / -1`：按下标取一个字符。
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
    return Slot::fromInt(address + 8 + index);
}

/**
 * @brief `__vm_memcpy(目标, 源, 字节数) -> 0`：逐字节搬一段（C++ 里就是 `memcpy`）。
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
 * @brief `__vm_memset(地址, 字节值, 字节数) -> 0`：把一段内存填成同一个字节。
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
 * `__vm_read_text() -> 堆上的新串地址`：把**下一项输入原文**读成一个字符串。
 *
 * `input_number(x)` 走的是 `(read, _, _, x)`（按变量类型解析一个数），而字符串没法
 * "解析"成一个数，所以单独给一个原生函数：输入项本身就是文本，直接复制到堆上。
 * 注意取完就往前走一项，两种读法**共用**同一个输入序号。
 */
#include <cctype>
#include <cstdio>
#include <cstdlib>

/**
 * @brief `MAGEL_NO_STDIN_INPUT=1` 时不做标准输入兜底。
 *
 * 门禁（`check.sh` 等）靠它保证确定性：样例缺输入时必须照旧报"输入不够"，
 * 而不是停下来等标准输入。
 * @return 关掉兜底返回 true。
 */
bool stdinInputDisabled()
{
#if STACKVM_EMBEDDED
    return !mcuConsole::HasInput();
#else
    const char *flag = std::getenv("MAGEL_NO_STDIN_INPUT");
    return flag != nullptr && flag[0] != '\0' && flag[0] != '0';
#endif
}

/**
 * 取下一项输入：先用 `--input "a b c"` 给的项，用完了就**从标准输入读一项**。
 *
 * 于是调试时可以直接敲，也可以 `echo "abc 3" | magel run x.mgl` 喂进去；两项之间按空白
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
#if STACKVM_EMBEDDED
    return mcuConsole::ReadByte();
#else
    return std::getchar();
#endif
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
 * @brief 标准输入是不是交互式终端——决定 `__vm_pause` 要不要真的停下来等。
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
 * @brief `__vm_pause() -> 0`：调试用"临时停一下"，等到一行输入再继续（`std::lang::pause()` 降到它）。
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
 *  运行期对象：大整数（bint）与三个容器（vec / set / map）
 * ---------------------------------------------------------------------------
 *
 * 这些东西的"值"**不是**内存地址，而是**对象表里的句柄**：表在 C++ 侧，一个
 * `std::shared_ptr<RuntimeObject>` 装下一种对象。宿主语言的类型只决定"句柄怎么用"：
 *
 *   * `bint`     —— 任意精度整数，后端是 `library/big_number` 的 `bigint::BigInt`；
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

/**
 * 对象句柄的起点（2^62）：编码堆地址与数据区地址均低于此边界。
 */
constexpr std::int64_t kObjectHandleBase = 4611686018427387904LL;

/**
 * 对象种类。
 */
enum class ObjectKind
{
    BigInt,         ///< 任意精度整数（bint）
    BigFloat,       ///< 任意精度十进制浮点（bfloat）
    Vector,         ///< 有序表（vec<T>）
    Set,            ///< 有序集合（set<T>）
    Map,            ///< 映射（map<K,V>）
    Obj,            ///< 装箱格子（obj）：一个值 + 它的类型号
    File,           ///< 宿主文件句柄（file）
    Process,        ///< 宿主进程管道（proc）
    IteratorLease,  ///< 引用迭代器对列表元素单元的共享租约
    Arena,          ///< 批量持有字节堆分配的标准库 Arena
    Closure,        ///< 类型化函数入口和可写借用的捕获环境
};

/**
 * @brief 种类名（诊断文案里用）。
 * @param kind 待处理对象的类别。
 * @retval 对应对象的指针。
 */
[[nodiscard]] const char *objectKindName(ObjectKind kind)
{
    switch (kind)
    {
    case ObjectKind::BigInt: return "BigInteger";
    case ObjectKind::Closure: return "lambda";
    case ObjectKind::BigFloat: return "BigDecimal";
    case ObjectKind::Vector: return "vec";
    case ObjectKind::Set: return "set";
    case ObjectKind::Map: return "map";
    case ObjectKind::Obj: return "obj";
    case ObjectKind::File: return "file";
    case ObjectKind::Process: return "proc";
    case ObjectKind::Arena: return "arena";
    case ObjectKind::IteratorLease: return "iterator lease";
    }
    return "?";
}

/**
 * 集合 / 映射的键：对象按可跨副本保留的身份排序，其余按值排序。
 */
struct ObjectKey
{
    bool isStruct = false;
    bool isObject = false;
    bool isText = false;
    bool isFloat = false;
    bool ownsObject = false;
    std::int64_t integer = 0;
    std::int64_t ownedInteger = 0;
    double real = 0.0;
    Memory::String text;
    Memory::Vector<ObjectKey> fields;
    bool hashed = false;    ///< 自定义 Map 键是否已缓存哈希。
    std::int64_t hash = 0;  ///< Hash<T> 返回值按位保存，用于同哈希范围查找。

    /**
     * @brief 判断左值是否小于右值。
     *
     * 类型不同时按"数值 < 字符串"排（保证严格弱序）；同类型按值比。
     * @param other 参与当前运算或比较的另一对象。
     * @retval 左值小于右值时为 true，否则为 false。
     */
    bool operator<(const ObjectKey &other) const
    {
        if (hashed != other.hashed) return !hashed;
        if (hashed && hash != other.hash) return hash < other.hash;
        if (isStruct != other.isStruct) return !isStruct;
        if (isStruct) return fields < other.fields;
        if (isText != other.isText) return !isText;
        if (isText) return text < other.text;
        if (isFloat != other.isFloat)
        {
            const double left = isFloat ? real : static_cast<double>(integer);
            const double right = other.isFloat ? other.real : static_cast<double>(other.integer);
            return left < right;
        }
        return isFloat ? (real < other.real) : (integer < other.integer);
    }
};

/**
 * 容器里的一个值：数值（整数 / 浮点）或字符串（**存副本**）。
 */
struct ObjectValue
{
    bool isStruct = false;
    bool isText = false;
    bool isFloat = false;
    bool ownsObject = false;
    std::int64_t integer = 0;
    std::int64_t objectIdentity = 0;
    double real = 0.0;
    Memory::String text;
    Memory::Vector<ObjectValue> fields;

    /**
     * @brief 当键用（集合元素、映射的键）。
     * @param keepOwnership 是否保留输入对象的所有权。
     * @retval 保存当键用（集合元素、映射的键）结果的对象。
     */
    [[nodiscard]] ObjectKey key(bool keepOwnership = false) const
    {
        ObjectKey out;
        out.isStruct = isStruct;
        out.isObject = objectIdentity != 0;
        out.isText = isText;
        out.isFloat = isFloat;
        out.ownsObject = keepOwnership && ownsObject;
        out.integer = objectIdentity != 0 ? objectIdentity : integer;
        out.ownedInteger = integer;
        out.real = real;
        out.text = text;
        for (const ObjectValue &field : fields) out.fields.push_back(field.key(keepOwnership));
        return out;
    }
};

/**
 * 一个运行期对象（几种里的一种，用同一个结构装下，省一套 union 的麻烦）。
 */
/**
 * @brief 共享借用租约；最后一个游标副本释放时写回并回收引用单元。
 */
struct IteratorLeaseState
{
    NativeContext context;
    std::int64_t source = 0;
    Memory::Map<std::size_t, std::size_t> cells;
    ~IteratorLeaseState();
};

struct RuntimeObject
{
    std::int64_t closureTarget = 0;                             ///< 数据区函数标签地址，不保存宿主函数地址。
    bool closureEnvironment = false;                            ///< 调用时是否附加环境句柄。
    bool closureCopy = false;                                   ///< 拥有捕获使用稳定单元，复制闭包时复制当前状态。
    Memory::Map<std::size_t, std::int64_t> captureDestructors;  ///< 聚合捕获的用户析构函数。
    ObjectKind kind = ObjectKind::BigInt;
    std::int64_t identity = 0;  ///< 值副本共享的键身份；新对象由 ObjectStore::add 分配。
#if STACKVM_WITH_BIGINT
    bigint::BigInt number;  ///< BigInt：值本身（关闭大整数时这个字段不在）
#endif
#if STACKVM_WITH_BIGFLOAT
    bigfloat::BigFloat decimal;  ///< BigFloat：任意精度十进制浮点
    int decimalPrecision = 0;    ///< 0 使用运行期默认值；正数是该值的除法有效位数。
#endif
    Memory::Vector<ObjectValue> items;  ///< Vector：有序元素
    /**
     * @brief 活跃 foreach 元素引用的可寻址单元，由最外层借用负责回收。
     */
    struct ElementView
    {
        std::size_t address = 0;
        std::size_t users = 1;
        bool isText = false;
        bool isFloat = false;
        std::int64_t objectFlag = 0;
    };
    Memory::Map<std::size_t, ElementView> elementViews;
    bool refreshingViews = false;
    std::size_t iteratorLeases = 0;  ///< 活跃引用游标租约数量。
    bool pendingRelease = false;     ///< 持有元素引用时延后释放源列表。
    std::shared_ptr<IteratorLeaseState> iteratorLease;
    std::int64_t mapEqual = 0;                    ///< Equal<bool,T> 回调；0 表示内建键比较。
    std::int64_t mapHash = 0;                     ///< 可选 Hash<T> 回调；0 表示直接 Equal 查找。
    bool mapComparing = false;                    ///< 比较期间禁止重入同一个 Map。
    Memory::Map<ObjectKey, ObjectValue> entries;  ///< Set / Map：键 → 值（Set 的值不用）
    /**
     * Obj：**类型号**（由前端在编译期算好，`nameof(类型)` 给的就是它；0 = 还没 set 过）。
     * 有了类型号，`obj.name()` 才能和 `nameof(...)` 比对 —— 泛型就是靠这一条分支的。
     */
    std::int64_t objTag = 0;
    /**
     * Obj：类型名（**只为诊断文案**：`obj.get` 取错类型时要说清里面装的是什么）。
     */
    Memory::String objTypeName;
    Memory::Vector<std::int64_t> objTextCallbacks;  ///< 文本覆盖方法及其字段存储布局。
    Memory::String objTextShape;                    ///< 默认值文本布局；复制 Any 时一并保存。
    /// Obj：装着的值（数值 / 字符串副本；装指针时就是那个地址）。
    ObjectValue objValue;
    /// Obj：objValue.integer 是不是一份由这个盒子持有的运行期对象句柄。
    bool objOwnsObject = false;
    std::int64_t iteratorLayout = 0;  ///< 状态叶子布局字符串地址。
    bool iteratorStateBox = false;    ///< 专用迭代状态箱拥有单元素存储。
    NativeContext iteratorContext;    ///< 迭代状态或 copy 捕获单元的堆与对象表，供复制和清理使用。
    /// File：宿主文件流。
    std::shared_ptr<FileHandle> file;
    /// File：诊断用路径。
    Memory::String filePath;
    /// File：标准库 Result 包装层最近一次操作的错误。
    Memory::String fileError;
    /// File：最近一次无陷阱操作是否成功。
    bool fileSucceeded = true;
    /// Process：`popen` 返回的宿主管道。
    std::FILE *process = nullptr;
    /// Process：诊断用命令。
    Memory::String command;
    /// Arena：由该 arena 持有、reset/drop 时成批释放的字节堆地址。
    Memory::Vector<std::size_t> arenaBlocks;
};

bool nativeRefreshViews(NativeContext &ctx, RuntimeObject &object);
bool nativeWriteView(NativeContext &ctx, const RuntimeObject::ElementView &view, const ObjectValue &value);

#if !STACKVM_WITH_BIGINT
/**
 * @brief 没把大整数编进来时的一句人话（比"函数入口不存在"强）。只有"轻量构建"里才有。
 * @param ctx 环境。
 * @return 永远返回 0（调用方直接返回它）。
 */
Slot bigIntDisabled(NativeContext &ctx)
{
    ctx.error = "这个构建没有把大整数编进来（附加功能）：用 "
                "`make -C library/stackvm STACKVM_WITH_BIGINT=1` 重建虚拟机，"
                "宿主再链上 build/big_number/libbig_number.a";
    return Slot::fromInt(0);
}
#endif

void nativeReleaseViewContents(NativeContext &ctx, const RuntimeObject::ElementView &view);

/**
 * 对象表：句柄 → 对象。
 */
struct ObjectStore
{
    Memory::Map<std::pair<std::size_t, std::int64_t>, std::size_t> movedStorage;  ///< 已销毁子对象的（地址、类型号）及存储大小。
    Memory::Map<std::int64_t, std::shared_ptr<RuntimeObject>> objects;            ///< 还活着的对象
    /**
     * @brief 新分配或重新赋值的存储开始生命周期。
     * @param address 地址。
     * @param bytes 字节数。
     * @param fresh 是否为新分配块，需清除所有重叠的旧标记。
     */
    void beginStorage(std::size_t address, std::size_t bytes, bool fresh = false)
    {
        for (auto it = movedStorage.begin(); it != movedStorage.end();)
        {
            const auto start = it->first.first;
            const bool overlap = start >= address ? start - address < bytes : address - start < it->second;
            const bool contained = start >= address && start - address < bytes && it->second <= bytes - (start - address);
            if (fresh ? overlap : contained)
                it = movedStorage.erase(it);
            else
                ++it;
        }
    }
    bool shuttingDown = false;
    /**
     * @brief 释放对象存储中仍持有的全部资源。
     */
    ~ObjectStore()
    {
        shuttingDown = true;
        objects.clear();
    }
    std::int64_t next = kObjectHandleBase;  ///< 下一个句柄
    Memory::Vector<std::size_t> materializedHeap;
    Memory::Vector<std::int64_t> materializedObjects;

    /**
     * @brief 收下一个对象，返回它的句柄。
     * @param object 待访问的对象。
     * @retval 收下一个对象，返回它的句柄。
     */
    std::int64_t add(std::shared_ptr<RuntimeObject> object)
    {
        const std::int64_t handle = next++;
        if (object->identity == 0) object->identity = handle;
        objects[handle] = std::move(object);
        return handle;
    }

    /**
     * @brief 根据名称或键查找匹配项。
     *
     * 按句柄找对象；已经释放 / 不认识返回 nullptr。
     * @param handle 待操作的运行时句柄。
     * @retval 匹配项的指针，未找到时为空指针。
     */
    RuntimeObject *find(std::int64_t handle)
    {
        const auto found = objects.find(handle);
        return (found == objects.end()) ? nullptr : found->second.get();
    }

    /**
     * @brief 复制对象值并维护其引用或所有权。
     * @param value 待转换、保存或参与计算的值。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool copyValue(ObjectValue *value, Memory::String *error)
    {
        if (value == nullptr) return true;
        for (ObjectValue &field : value->fields)
            if (!copyValue(&field, error)) return false;
        if (!value->ownsObject) return true;
        const std::int64_t nestedCopy = copyHandle(value->integer, error);
        if (nestedCopy == 0) return false;
        value->integer = nestedCopy;
        return true;
    }

    /**
     * @brief 复制映射键及其持有的数据。
     * @param key 查找或写入使用的键。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool copyKey(ObjectKey *key, Memory::String *error)
    {
        if (key == nullptr) return true;
        for (ObjectKey &field : key->fields)
            if (!copyKey(&field, error)) return false;
        if (!key->ownsObject) return true;
        const std::int64_t nestedCopy = copyHandle(key->ownedInteger, error);
        if (nestedCopy == 0) return false;
        key->ownedInteger = nestedCopy;
        return true;
    }

    /**
     * @brief 为映射键建立独立持有的数据。
     * @param key 查找或写入使用的键。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool ownKey(ObjectKey *key, Memory::String *error)
    {
        if (key == nullptr) return true;
        for (ObjectKey &field : key->fields)
            if (!ownKey(&field, error)) return false;
        if (!key->isObject || key->ownsObject) return true;
        const std::int64_t nestedCopy = copyHandle(key->ownedInteger, error);
        if (nestedCopy == 0) return false;
        key->ownedInteger = nestedCopy;
        key->ownsObject = true;
        return true;
    }

    /**
     * @brief 释放对象值持有的资源。
     * @param value 待转换、保存或参与计算的值。
     */
    void releaseValue(const ObjectValue &value)
    {
        for (const ObjectValue &field : value.fields) releaseValue(field);
        if (value.ownsObject) release(value.integer);
    }

    /**
     * @brief 释放映射键持有的资源。
     * @param key 查找或写入使用的键。
     */
    void releaseKey(const ObjectKey &key)
    {
        for (const ObjectKey &field : key.fields) releaseKey(field);
        if (key.ownsObject) release(key.ownedInteger);
    }

    /**
     * @brief 释放容器中全部键值持有的资源。
     * @param object 待访问的对象。
     */
    void releaseContents(const RuntimeObject &object)
    {
        if (object.iteratorStateBox || object.closureCopy)
        {
            auto context = object.iteratorContext;
            for (const auto &entry : object.elementViews)
            {
                const auto destructor = object.captureDestructors.find(entry.first);
                if (destructor != object.captureDestructors.end() && context.invoker)
                {
                    NativeValue result;
                    (*context.invoker)(destructor->second, { NativeValue::fromInt(entry.second.address) }, result);
                }
                nativeReleaseViewContents(context, entry.second);
                context.heap->release(entry.second.address);
            }
        }
        for (const ObjectValue &value : object.items) releaseValue(value);
        for (const auto &entry : object.entries)
        {
            releaseKey(entry.first);
            releaseValue(entry.second);
        }
        if (object.kind == ObjectKind::Obj && object.objOwnsObject)
        {
            release(object.objValue.integer);
        }
        if (object.kind == ObjectKind::File && object.file)
        {
            object.file->close();
        }
#if STACKVM_WITH_HOST_IO
        if (object.kind == ObjectKind::Process && object.process != nullptr)
        {
#ifdef _WIN32
            _pclose(object.process);
#else
            pclose(object.process);
#endif
        }
#endif
    }

    /**
     * @brief 复制运行时对象及其容器内容。
     * @param object 待访问的对象。
     * @param error 接收失败原因的错误信息。
     * @retval 对应对象的指针。
     */
    std::shared_ptr<RuntimeObject> copyObject(const RuntimeObject &object, Memory::String *error)
    {
        if (object.kind == ObjectKind::File || object.kind == ObjectKind::Process || object.kind == ObjectKind::Arena)
        {
            if (error != nullptr)
            {
                *error = Memory::String(objectKindName(object.kind)) + " 句柄不能复制；请把它当作资源句柄传递并显式 close/delete";
            }
            return nullptr;
        }
        auto copy = Memory::makeShared<RuntimeObject>(object);
        copy->elementViews.clear();
        copy->refreshingViews = false;
        copy->pendingRelease = false;
        copy->iteratorLeases = 0;
        copy->mapComparing = false;
        for (ObjectValue &value : copy->items)
        {
            if (!copyValue(&value, error)) return nullptr;
        }
        if (!copy->entries.empty())
        {
            Memory::Map<ObjectKey, ObjectValue> copiedEntries;
            for (auto entry : copy->entries)
            {
                ObjectKey key = entry.first;
                if (!copyKey(&key, error)) return nullptr;
                if (!copyValue(&entry.second, error)) return nullptr;
                copiedEntries.emplace(std::move(key), std::move(entry.second));
            }
            copy->entries = std::move(copiedEntries);
        }
        if (copy->kind == ObjectKind::Obj && copy->objOwnsObject)
        {
            if (!copyValue(&copy->objValue, error)) return nullptr;
        }
        if (object.closureCopy)
        {
            for (const auto &entry : object.elementViews)
            {
                auto view = entry.second;
                view.address = nativeAllocate(copy->iteratorContext, std::max<std::size_t>(8,
                                                                                           copy->items[entry.first].isStruct ? copy->items[entry.first].fields.size() * 8 : 8));
                if (!view.address || !nativeWriteView(copy->iteratorContext, view, copy->items[entry.first]))
                {
                    if (error) *error = "闭包副本捕获存储分配失败";
                    return nullptr;
                }
                copy->elementViews[entry.first] = view;
            }
        }
        return copy;
    }

    /**
     * @brief 复制运行时句柄并维护其对象引用。
     * @param handle 待操作的运行时句柄。
     * @param error 接收失败原因的错误信息。
     * @retval 复制运行时句柄并维护其对象引用。
     */
    std::int64_t copyHandle(std::int64_t handle, Memory::String *error)
    {
        if (handle < kObjectHandleBase)
        {
            if (error != nullptr) *error = "不是一个能复制的对象句柄：" + Memory::toString(handle);
            return 0;
        }
        RuntimeObject *object = find(handle);
        if (object == nullptr)
        {
            if (error != nullptr) *error = "这个对象已经释放过了（句柄 " + Memory::toString(handle) + "）";
            return 0;
        }
        if ((object->iteratorStateBox || object->closureCopy) && !nativeRefreshViews(object->iteratorContext, *object))
        {
            if (error) *error = object->iteratorContext.error;
            return 0;
        }
        std::shared_ptr<RuntimeObject> copy = copyObject(*object, error);
        if (!copy) return 0;
        return add(std::move(copy));
    }

    /**
     * @brief 释放指定地址对应的运行时资源。
     *
     * 释放一个对象；确实释放了返回 true。
     * @param handle 待操作的运行时句柄。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool release(std::int64_t handle)
    {
        const auto found = objects.find(handle);
        if (found == objects.end()) return false;
        if (found->second->mapComparing) return false;
        if (found->second->iteratorLeases || (!found->second->iteratorStateBox && !found->second->closureCopy && !found->second->elementViews.empty()))
        {
            found->second->pendingRelease = true;
            return true;
        }
        std::shared_ptr<RuntimeObject> object = std::move(found->second);
        objects.erase(found);
        releaseContents(*object);
        return true;
    }

    /**
     * @brief 还活着的对象个数（停机时 != 0 就是漏了）。
     * @retval 还活着的对象个数（停机时 != 0 就是漏了）。
     */
    [[nodiscard]] std::size_t liveCount() const { return objects.size(); }
};

/**
 * @brief 创建由本次原生调用管理的临时对象。
 * @param bytes 内存区域或数据的字节数。
 * @retval 创建由本次原生调用管理的临时对象。
 */
std::int64_t NativeCallContextImpl::makeTemporary(std::size_t bytes)
{
    if (!context_.heap || !context_.objects)
    {
        fail("temporary storage unavailable");
        return 0;
    }
    const auto address = nativeAllocate(context_, std::max<std::size_t>(8, bytes));
    if (!address)
    {
        fail("ABI temporary allocation failed");
        return 0;
    }
    context_.objects->materializedHeap.push_back(address);
    return static_cast<std::int64_t>(address);
}

/**
 * @brief 创建原生调用可用的运行时列表。
 * @retval 创建原生调用可用的运行时列表。
 */
std::int64_t NativeCallContextImpl::makeList()
{
    if (context_.objects == nullptr)
    {
        fail("没有接上对象表，建不了列表");
        return 0;
    }
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = ObjectKind::Vector;
    return static_cast<std::int64_t>(context_.objects->add(std::move(object)));
}

/**
 * @brief 将一个原生值追加到运行时列表。
 * @param listHandle 目标运行时列表的句柄。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::listPush(std::int64_t listHandle, const NativeValue &value)
{
    if (context_.objects == nullptr)
    {
        fail("没有接上对象表");
        return false;
    }
    RuntimeObject *object = context_.objects->find(listHandle);
    if (object == nullptr || object->kind != ObjectKind::Vector)
    {
        fail("listPush 的句柄不是 List");
        return false;
    }
    ObjectValue out;
    if (value.isText())
    {
        out.isText = true;
        out.text = value.text;
    }
    else if (value.isFloat())
    {
        out.isFloat = true;
        out.real = value.asFloat();
    }
    else
    {
        out.integer = value.asInt();
    }
    object->items.push_back(std::move(out));
    return true;
}

/**
 * @brief 将结构体数据追加到运行时列表。
 * @param listHandle 目标运行时列表的句柄。
 * @param structAddress 结构体数据的运行时地址。
 * @param slotCount 结构体数据包含的值槽数量。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::listPushStruct(std::int64_t listHandle, std::int64_t structAddress,
                                           std::size_t slotCount)
{
    if (context_.objects == nullptr)
    {
        fail("没有接上对象表");
        return false;
    }
    RuntimeObject *object = context_.objects->find(listHandle);
    if (object == nullptr || object->kind != ObjectKind::Vector)
    {
        fail("listPushStruct 的句柄不是 List");
        return false;
    }
    ObjectValue out;
    out.isStruct = true;
    out.fields.reserve(slotCount);
    for (std::size_t i = 0; i < slotCount; ++i)
    {
        /* 每个槽按 double 的位模式拷贝：MAGEL 结构体字段都是 8 字节对齐，
         * 数值型结构体（如 Complex 的 re/im）这样拷最省事。 */
        double raw = 0.0;
        if (!readBytes(structAddress + static_cast<std::int64_t>(i * 8u),
                       reinterpret_cast<char *>(&raw), sizeof(raw)))
        {
            fail("listPushStruct 读结构体槽失败");
            return false;
        }
        ObjectValue field;
        field.isFloat = true;
        field.real = raw;
        out.fields.push_back(std::move(field));
    }
    object->items.push_back(std::move(out));
    return true;
}

/**
 * @brief 从列表项读取结构体数据。
 * @param handle 待操作的运行时句柄。
 * @param slotCount 结构体数据包含的值槽数量。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::readListStruct(std::int64_t handle, std::size_t slotCount,
                                           Memory::Vector<double> &out) const
{
    out.clear();
    if (context_.objects == nullptr) return false;
    RuntimeObject *object = context_.objects->find(handle);
    if (object == nullptr || object->kind != ObjectKind::Vector) return false;
    if (!nativeRefreshViews(context_, *object)) return false;
    for (const ObjectValue &item : object->items)
    {
        if (!item.isStruct) return false;
        for (std::size_t i = 0; i < slotCount; ++i)
        {
            if (i < item.fields.size())
            {
                const ObjectValue &field = item.fields[i];
                out.push_back(field.isFloat ? field.real : static_cast<double>(field.integer));
            }
            else
            {
                out.push_back(0.0);
            }
        }
    }
    return true;
}

/**
 * @brief 将运行时列表转换为原生值数组。
 * @param handle 待操作的运行时句柄。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::readList(std::int64_t handle,
                                     Memory::Vector<NativeValue> &out) const
{
    out.clear();
    if (context_.objects == nullptr) return false;
    RuntimeObject *object = context_.objects->find(handle);
    if (object == nullptr || object->kind != ObjectKind::Vector) return false;
    if (!nativeRefreshViews(context_, *object)) return false;
    out.reserve(object->items.size());
    for (const ObjectValue &item : object->items)
    {
        if (item.isText)
            out.push_back(NativeValue::fromText(item.text));
        else if (item.isFloat)
            out.push_back(NativeValue::fromFloat(item.real));
        else
            out.push_back(NativeValue::fromInt(item.integer));
    }
    return true;
}

/**
 * @brief 创建原生调用可用的运行时映射。
 * @retval 创建原生调用可用的运行时映射。
 */
std::int64_t NativeCallContextImpl::makeMap()
{
    if (context_.objects == nullptr)
    {
        fail("没有接上对象表，建不了映射");
        return 0;
    }
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = ObjectKind::Map;
    return static_cast<std::int64_t>(context_.objects->add(std::move(object)));
}

/**
 * @brief 创建原生调用可用的运行时集合。
 * @retval 创建原生调用可用的运行时集合。
 */
std::int64_t NativeCallContextImpl::makeSet()
{
    if (context_.objects == nullptr)
    {
        fail("没有接上对象表，建不了集合");
        return 0;
    }
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = ObjectKind::Set;
    return static_cast<std::int64_t>(context_.objects->add(std::move(object)));
}

namespace
{
/**
 * @brief 将运行时值转换为容器键。
 * @param value 待转换、保存或参与计算的值。
 * @retval 保存将运行时值转换为容器键结果的对象。
 */
ObjectKey keyFromValue(const NativeValue &value)
{
    ObjectKey key;
    if (value.isText())
    {
        key.isText = true;
        key.text = value.text;
    }
    else if (value.isFloat())
    {
        key.isFloat = true;
        key.real = value.asFloat();
    }
    else
    {
        key.integer = value.asInt();
    }
    return key;
}
/**
 * @brief 将宿主原生值转换为对象存储值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 保存将宿主原生值转换为对象存储值结果的对象。
 */
ObjectValue valueFromNative(const NativeValue &value)
{
    ObjectValue out;
    if (value.isText())
    {
        out.isText = true;
        out.text = value.text;
    }
    else if (value.isFloat())
    {
        out.isFloat = true;
        out.real = value.asFloat();
    }
    else
    {
        out.integer = value.asInt();
    }
    return out;
}
}  // namespace

/**
 * @brief 向运行时映射写入一个键值对。
 * @param mapHandle 目标运行时映射的句柄。
 * @param key 查找或写入使用的键。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::mapSet(std::int64_t mapHandle, const NativeValue &key,
                                   const NativeValue &value)
{
    if (context_.objects == nullptr)
    {
        fail("没有接上对象表");
        return false;
    }
    RuntimeObject *object = context_.objects->find(mapHandle);
    if (object == nullptr || object->kind != ObjectKind::Map)
    {
        fail("mapSet 的句柄不是 Map");
        return false;
    }
    object->entries[keyFromValue(key)] = valueFromNative(value);
    return true;
}

/**
 * @brief 将原生值插入运行时集合。
 * @param setHandle 目标运行时集合的句柄。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::setInsert(std::int64_t setHandle, const NativeValue &value)
{
    if (context_.objects == nullptr)
    {
        fail("没有接上对象表");
        return false;
    }
    RuntimeObject *object = context_.objects->find(setHandle);
    if (object == nullptr || object->kind != ObjectKind::Set)
    {
        fail("setInsert 的句柄不是 Set");
        return false;
    }
    /* Set 的值不用，键存元素本身。 */
    object->entries[keyFromValue(value)] = valueFromNative(value);
    return true;
}

namespace
{
/**
 * @brief 将容器键转换为宿主原生值。
 * @param key 查找或写入使用的键。
 * @retval 封装为原生值的运行时结果。
 */
NativeValue nativeFromKey(const ObjectKey &key)
{
    if (key.isText) return NativeValue::fromText(key.text);
    if (key.isFloat) return NativeValue::fromFloat(key.real);
    return NativeValue::fromInt(key.integer);
}
/**
 * @brief 将对象存储值转换为宿主原生值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
NativeValue nativeFromValue(const ObjectValue &value)
{
    if (value.isText) return NativeValue::fromText(value.text);
    if (value.isFloat) return NativeValue::fromFloat(value.real);
    return NativeValue::fromInt(value.integer);
}
}  // namespace

/**
 * @brief 读取运行时映射的全部键值。
 * @param handle 待操作的运行时句柄。
 * @param keys 接收映射键的输出列表。
 * @param values 输入值列表或接收读取值的输出列表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::readMap(std::int64_t handle, Memory::Vector<NativeValue> &keys,
                                    Memory::Vector<NativeValue> &values) const
{
    keys.clear();
    values.clear();
    if (context_.objects == nullptr) return false;
    RuntimeObject *object = context_.objects->find(handle);
    if (object == nullptr || object->kind != ObjectKind::Map) return false;
    for (const auto &entry : object->entries)
    {
        keys.push_back(nativeFromKey(entry.first));
        values.push_back(nativeFromValue(entry.second));
    }
    return true;
}

/**
 * @brief 读取运行时集合中的原生值。
 * @param handle 待操作的运行时句柄。
 * @param elements 元素类型或元素值列表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::readSet(std::int64_t handle, Memory::Vector<NativeValue> &elements) const
{
    elements.clear();
    if (context_.objects == nullptr) return false;
    RuntimeObject *object = context_.objects->find(handle);
    if (object == nullptr || object->kind != ObjectKind::Set) return false;
    for (const auto &entry : object->entries) elements.push_back(nativeFromKey(entry.first));
    return true;
}

/**
 * @brief 句柄是不是"对象表里的句柄"（不是堆地址 / 数据区地址 / 0）。
 * @param handle 待操作的运行时句柄。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
[[nodiscard]] bool isObjectHandle(std::int64_t handle) { return handle >= kObjectHandleBase; }

/**
 * @brief 把一个实参当句柄取对象。
 * @param ctx 环境。
 * @param handle 句柄（`Slot`）。
 * @param kind 期望的种类。
 * @param who 谁的参数（诊断文案）。
 * @return 对象；不是句柄 / 已经释放 / 种类不对时写 `ctx.error` 并返回 nullptr。
 */
bool nativeRefreshViews(NativeContext &ctx, RuntimeObject &object);
/**
 * @brief 根据原生调用参数查找运行时对象。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @param kind 待处理对象的类别。
 * @param who 诊断信息中使用的操作名称。
 * @retval 匹配项的指针，未找到时为空指针。
 */
RuntimeObject *nativeObject(NativeContext &ctx, const Slot &handle, ObjectKind kind,
                            const char *who)
{
    if (ctx.objects == nullptr || !isObjectHandle(handle.integer))
    {
        ctx.error = Memory::String(who) + " 需要一个 " + objectKindName(kind) + " 值，拿到的是 " + Memory::toString(handle.integer) + "（不是对象句柄）";
        return nullptr;
    }
    RuntimeObject *object = ctx.objects->find(handle.integer);
    if (object == nullptr)
    {
        ctx.error = Memory::String(who) + " 用的 " + objectKindName(kind) + " 已经释放过了（句柄 " + Memory::toString(handle.integer) + "）";
        return nullptr;
    }
    if (object->kind != kind)
    {
        ctx.error = Memory::String(who) + " 要的是 " + objectKindName(kind) + "，拿到的是 " + objectKindName(object->kind);
        return nullptr;
    }
    if (object->mapComparing)
    {
        ctx.error = Memory::String("Equal / Hash 回调期间不能重入同一个 ") + (kind == ObjectKind::Set ? "Set" : "Map");
        return nullptr;
    }
    if (object->kind != ObjectKind::Closure && !nativeRefreshViews(ctx, *object)) return nullptr;
    return object;
}

/**
 * @brief 一条实参 → 容器里的值 / 键。
 * @param ctx 环境。
 * @param value 值（数值就是 Slot 本身；字符串是它的地址）。
 * @param isText true = 按字符串读（**读成副本**）。
 * @param isObject true = 这个整数是一份由调用者持有的对象句柄。
 * @param copyObject true = 存入容器 / obj 时复制一份，false = 只用于查找比较。
 * @param out 结果。
 * @return 成功返回 true；字符串读不到时写 `ctx.error` 并返回 false。
 */
bool nativeValue(NativeContext &ctx, const Slot &value, bool isText, std::int64_t objectFlag,
                 bool copyObject, ObjectValue *out)
{
    if (objectFlag > 1)
    {
        Memory::String descriptor;
        if (!nativeReadCString(ctx, static_cast<std::size_t>(objectFlag), &descriptor) || descriptor.empty() || descriptor[0] != '@')
        {
            ctx.error = "容器结构体布局描述无效";
            return false;
        }
        out->isStruct = true;
        for (std::size_t index = 1; index < descriptor.size(); ++index)
        {
            std::int64_t bits = 0;
            const std::size_t address = static_cast<std::size_t>(value.integer) + (index - 1u) * 8u;
            if (!nativeRead(ctx, address, reinterpret_cast<char *>(&bits), sizeof(bits)))
            {
                ctx.error = "容器结构体字段地址无效";
                if (ctx.objects != nullptr) ctx.objects->releaseValue(*out);
                out->fields.clear();
                return false;
            }
            ObjectValue field;
            const char kind = descriptor[index];
            if (kind != 'i' && kind != 's' && kind != 'o' && kind != 'f' && kind != 'c')
            {
                ctx.error = "容器结构体字段布局无效";
                if (ctx.objects != nullptr) ctx.objects->releaseValue(*out);
                out->fields.clear();
                return false;
            }
            Slot fieldValue = Slot::fromInt(bits);
            if (kind == 'f')
            {
                double real = 0.0;
                std::memcpy(&real, &bits, sizeof(real));
                fieldValue = Slot::fromDouble(real);
            }
            /* 未激活枚举载荷和移动后的拥有槽为零；保留空槽，不能把它实体化为空字符串。 */
            if (bits == 0 && (kind == 's' || kind == 'o' || kind == 'c'))
            {
                out->fields.push_back(std::move(field));
                continue;
            }
            if (!nativeValue(ctx, fieldValue, kind == 's', kind == 'c' ? -1 : kind == 'o' ? 1 :
                                                                                            0,
                             copyObject, &field))
            {
                if (ctx.objects != nullptr) ctx.objects->releaseValue(*out);
                out->fields.clear();
                return false;
            }
            out->fields.push_back(std::move(field));
        }
        return true;
    }
    out->isText = isText;
    out->ownsObject = false;
    if (!isText)
    {
        out->isFloat = value.isFloat;
        out->integer = value.integer;
        out->objectIdentity = 0;
        out->real = value.real;
        if (objectFlag == 1 || (objectFlag == -1 && isObjectHandle(value.integer)))
        {
            RuntimeObject *object = ctx.objects->find(value.integer);
            if (object == nullptr)
            {
                ctx.error = "对象键/值已经释放或不是有效句柄：" + Memory::toString(value.integer);
                return false;
            }
            if (!nativeRefreshViews(ctx, *object)) return false;
            out->objectIdentity = object->identity;
            out->isFloat = false;
            out->real = 0.0;
            if (copyObject)
            {
                Memory::String error;
                const std::int64_t copy = ctx.objects->copyHandle(value.integer, &error);
                if (copy == 0)
                {
                    ctx.error = error;
                    return false;
                }
                out->integer = copy;
                out->ownsObject = true;
            }
        }
        return true;
    }
    if (!nativeReadCString(ctx, static_cast<std::size_t>(value.integer), &out->text))
    {
        ctx.error = "读容器元素里的字符串失败（地址 " + Memory::toString(value.integer) + "）";
        return false;
    }
    return true;
}

/**
 * @brief 容器里的值 / 键 → 一个 Slot（字符串会在堆上复制一份新的）。
 * @param ctx 环境。
 * @param value 值。
 * @param out 结果。
 * @return 成功返回 true。
 */
bool nativeSlot(NativeContext &ctx, const ObjectValue &value, Slot *out)
{
    if (value.isStruct)
    {
        if (ctx.heap == nullptr || ctx.objects == nullptr) return false;
        const std::size_t bytes = std::max<std::size_t>(8u, value.fields.size() * 8u);
        const std::size_t address = nativeAllocate(ctx, bytes);
        if (address == 0u)
        {
            ctx.error = "容器结构体取值时堆内存不足";
            return false;
        }
        ctx.objects->materializedHeap.push_back(address);
        for (std::size_t index = 0; index < value.fields.size(); ++index)
        {
            Slot field;
            if (!nativeSlot(ctx, value.fields[index], &field)) return false;
            if (value.fields[index].isText)
                ctx.objects->materializedHeap.push_back(static_cast<std::size_t>(field.integer));
            if (value.fields[index].ownsObject || value.fields[index].objectIdentity != 0)
                ctx.objects->materializedObjects.push_back(field.integer);
            const std::int64_t bits = field.isFloat ? [&]() {
                std::int64_t raw = 0;
                std::memcpy(&raw, &field.real, sizeof(raw));
                return raw;
            }() :
                                                      field.integer;
            if (!ctx.heap->writeBytes(address + index * 8u,
                                      reinterpret_cast<const char *>(&bits), sizeof(bits)))
            {
                ctx.error = "容器结构体取值时写入字段失败";
                return false;
            }
        }
        *out = Slot::fromInt(static_cast<std::int64_t>(address));
        return true;
    }
    if (!value.isText)
    {
        if (value.ownsObject || value.objectIdentity != 0)
        {
            Memory::String error;
            const std::int64_t copy = ctx.objects->copyHandle(value.integer, &error);
            if (copy == 0)
            {
                ctx.error = error;
                return false;
            }
            *out = Slot::fromInt(copy);
            return true;
        }
        *out = value.isFloat ? Slot::fromDouble(value.real) : Slot::fromInt(value.integer);
        return true;
    }
    const std::size_t address = nativeAllocString(ctx, value.text);
    if (address == 0u) return false;
    *out = Slot::fromInt(static_cast<std::int64_t>(address));
    return true;
}

/**
 * @brief 从活跃元素单元读取当前值；聚合单元直接保存摊平后的字段。
 */
bool nativeViewValue(NativeContext &ctx, const RuntimeObject::ElementView &view, ObjectValue *out)
{
    if (view.objectFlag > 1)
        return nativeValue(ctx, Slot::fromInt(static_cast<std::int64_t>(view.address)), false,
                           view.objectFlag, true, out);
    std::int64_t bits = 0;
    if (!nativeRead(ctx, view.address, reinterpret_cast<char *>(&bits), sizeof(bits)))
    {
        ctx.error = "列表迭代引用读取失败";
        return false;
    }
    Slot slot = Slot::fromInt(bits);
    if (view.isFloat)
    {
        double real = 0;
        std::memcpy(&real, &bits, sizeof(real));
        slot = Slot::fromDouble(real);
    }
    return nativeValue(ctx, slot, view.isText, view.objectFlag, true, out);
}
/**
 * @brief 将活跃引用中的当前值同步到列表快照，所有列表读取与值复制共用该入口。
 */
bool nativeRefreshViews(NativeContext &ctx, RuntimeObject &object)
{
    if (object.refreshingViews || object.elementViews.empty()) return true;
    object.refreshingViews = true;
    for (const auto &entry : object.elementViews)
    {
        if (entry.first >= object.items.size())
        {
            ctx.error = "迭代期间列表结构已改变";
            object.refreshingViews = false;
            return false;
        }
        ObjectValue value;
        if (!nativeViewValue(ctx, entry.second, &value))
        {
            object.refreshingViews = false;
            return false;
        }
        ctx.objects->releaseValue(object.items[entry.first]);
        object.items[entry.first] = std::move(value);
    }
    object.refreshingViews = false;
    return true;
}
/**
 * @brief 释放元素单元中当前拥有的字符串与对象叶子，不释放单元本身。
 */
void nativeReleaseViewContents(NativeContext &ctx, const RuntimeObject::ElementView &view)
{
    Memory::String descriptor;
    if (view.objectFlag > 1)
    {
        if (!nativeReadCString(ctx, static_cast<std::size_t>(view.objectFlag), &descriptor)) return;
    }
    else
        descriptor = Memory::String("@") + (view.isText ? 's' : view.objectFlag == -1 ? 'c' :
                                                            view.objectFlag == 1      ? 'o' :
                                                                                        'i');
    for (std::size_t i = 1; i < descriptor.size(); ++i)
    {
        std::int64_t bits = 0;
        if (!nativeRead(ctx, view.address + (i - 1u) * 8u, reinterpret_cast<char *>(&bits), sizeof(bits))) continue;
        if (descriptor[i] == 's' && bits != 0 && ctx.heap != nullptr) ctx.heap->release(static_cast<std::size_t>(bits));
        if ((descriptor[i] == 'o' || descriptor[i] == 'c') && isObjectHandle(bits)) ctx.objects->release(bits);
    }
}
/**
 * @brief 将容器快照复制到引用单元；字符串与对象叶子由单元独占。
 */
bool nativeWriteView(NativeContext &ctx, const RuntimeObject::ElementView &view, const ObjectValue &value)
{
    const std::size_t count = value.isStruct ? value.fields.size() : 1u;
    for (std::size_t i = 0; i < count; ++i)
    {
        Slot slot;
        if (!nativeSlot(ctx, value.isStruct ? value.fields[i] : value, &slot)) return false;
        std::int64_t bits = slot.integer;
        if (slot.isFloat) std::memcpy(&bits, &slot.real, sizeof(bits));
        if (!nativeWrite(ctx, view.address + i * 8u, reinterpret_cast<const char *>(&bits), sizeof(bits)))
        {
            ctx.error = "列表迭代引用写入失败";
            return false;
        }
    }
    return true;
}

/**
 * 脱离发送方堆和对象表的消息快照；对象句柄仅作为快照内部的索引。
 */
struct TransferPacket
{
    Memory::String type;
    ObjectValue value;
    Memory::Map<std::int64_t, RuntimeObject> objects;
};

bool captureTransferObject(NativeContext &ctx, std::int64_t handle, TransferPacket &packet,
                           Memory::Set<std::int64_t> &active);

/**
 * @brief 捕获可跨运行时传递的对象值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param value 待转换、保存或参与计算的值。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param active 当前正在处理的节点集合。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool captureTransferValue(NativeContext &ctx, const ObjectValue &value,
                          TransferPacket &packet, Memory::Set<std::int64_t> &active)
{
    for (const ObjectValue &field : value.fields)
        if (!captureTransferValue(ctx, field, packet, active)) return false;
    return value.objectIdentity == 0 || captureTransferObject(ctx, value.integer, packet, active);
}

/**
 * @brief 捕获可跨运行时传递的容器键。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param key 查找或写入使用的键。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param active 当前正在处理的节点集合。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool captureTransferKey(NativeContext &ctx, const ObjectKey &key,
                        TransferPacket &packet, Memory::Set<std::int64_t> &active)
{
    for (const ObjectKey &field : key.fields)
        if (!captureTransferKey(ctx, field, packet, active)) return false;
    return !key.isObject || captureTransferObject(ctx, key.ownedInteger, packet, active);
}

/**
 * @brief 捕获运行时对象的可传输快照。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param active 当前正在处理的节点集合。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool captureTransferObject(NativeContext &ctx, std::int64_t handle, TransferPacket &packet,
                           Memory::Set<std::int64_t> &active)
{
    if (active.count(handle) != 0u)
    {
        ctx.error = "消息中的容器对象形成循环；请先拆开循环引用";
        return false;
    }
    if (packet.objects.count(handle) != 0u) return true;
    RuntimeObject *source = ctx.objects == nullptr ? nullptr : ctx.objects->find(handle);
    if (source == nullptr)
    {
        ctx.error = "消息包含已释放的对象句柄：" + Memory::toString(handle);
        return false;
    }
    if (source->kind == ObjectKind::File || source->kind == ObjectKind::Process || source->kind == ObjectKind::Arena || source->kind == ObjectKind::Obj)
    {
        ctx.error = Memory::String("消息不能复制运行期资源或 std::lang::Any：") + objectKindName(source->kind);
        return false;
    }
    active.insert(handle);
    packet.objects.emplace(handle, *source);
    for (const ObjectValue &item : source->items)
        if (!captureTransferValue(ctx, item, packet, active)) return false;
    for (const auto &entry : source->entries)
        if (!captureTransferKey(ctx, entry.first, packet, active) || !captureTransferValue(ctx, entry.second, packet, active)) return false;
    active.erase(handle);
    return true;
}

/**
 * @brief 将原生参数捕获为可传输的值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param slot 待转换或传递的虚拟机值槽。
 * @param kind 待处理对象的类别。
 * @param layout 传输值的布局描述。
 * @param type 类型描述。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool captureTransfer(NativeContext &ctx, const Slot &slot, int kind,
                     std::int64_t layout, const Memory::String &type, TransferPacket *out)
{
    out->type = type;
    const std::int64_t objectFlag = kind == 4 ? layout : kind == 3 ? 1 :
                                                                     0;
    if (!nativeValue(ctx, slot, kind == 2, objectFlag, false, &out->value)) return false;
    Memory::Set<std::int64_t> active;
    return captureTransferValue(ctx, out->value, *out, active);
}

bool restoreTransferObject(NativeContext &ctx, const TransferPacket &packet,
                           std::int64_t oldHandle, Memory::Set<std::int64_t> &active,
                           Memory::Vector<std::int64_t> &created, std::int64_t *newHandle);

/**
 * @brief 在目标运行时恢复传输的对象值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param value 待转换、保存或参与计算的值。
 * @param active 当前正在处理的节点集合。
 * @param created 已恢复对象的句柄映射，避免重复恢复。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool restoreTransferValue(NativeContext &ctx, const TransferPacket &packet,
                          ObjectValue *value, Memory::Set<std::int64_t> &active,
                          Memory::Vector<std::int64_t> &created)
{
    for (ObjectValue &field : value->fields)
        if (!restoreTransferValue(ctx, packet, &field, active, created)) return false;
    if (value->objectIdentity == 0) return true;
    std::int64_t handle = 0;
    if (!restoreTransferObject(ctx, packet, value->integer, active, created, &handle)) return false;
    value->integer = handle;
    value->objectIdentity = ctx.objects->find(handle)->identity;
    value->ownsObject = true;
    return true;
}

/**
 * @brief 在目标运行时恢复传输的容器键。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param key 查找或写入使用的键。
 * @param active 当前正在处理的节点集合。
 * @param created 已恢复对象的句柄映射，避免重复恢复。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool restoreTransferKey(NativeContext &ctx, const TransferPacket &packet,
                        ObjectKey *key, Memory::Set<std::int64_t> &active,
                        Memory::Vector<std::int64_t> &created)
{
    for (ObjectKey &field : key->fields)
        if (!restoreTransferKey(ctx, packet, &field, active, created)) return false;
    if (!key->isObject) return true;
    std::int64_t handle = 0;
    if (!restoreTransferObject(ctx, packet, key->ownedInteger, active, created, &handle)) return false;
    key->ownedInteger = handle;
    key->integer = ctx.objects->find(handle)->identity;
    key->ownsObject = true;
    return true;
}

/**
 * @brief 在目标运行时恢复传输的对象快照。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param oldHandle 原运行时中的对象句柄。
 * @param active 当前正在处理的节点集合。
 * @param created 已恢复对象的句柄映射，避免重复恢复。
 * @param newHandle 目标运行时中恢复的对象句柄。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool restoreTransferObject(NativeContext &ctx, const TransferPacket &packet,
                           std::int64_t oldHandle, Memory::Set<std::int64_t> &active,
                           Memory::Vector<std::int64_t> &created, std::int64_t *newHandle)
{
    const auto found = packet.objects.find(oldHandle);
    if (found == packet.objects.end() || active.count(oldHandle) != 0u)
    {
        ctx.error = "消息对象快照无效或包含循环";
        return false;
    }
    active.insert(oldHandle);
    auto object = Memory::makeShared<RuntimeObject>(found->second);
    object->identity = 0;
    for (ObjectValue &item : object->items)
        if (!restoreTransferValue(ctx, packet, &item, active, created)) return false;
    if (!object->entries.empty())
    {
        Memory::Map<ObjectKey, ObjectValue> entries;
        for (const auto &entry : object->entries)
        {
            ObjectKey key = entry.first;
            ObjectValue value = entry.second;
            if (!restoreTransferKey(ctx, packet, &key, active, created) || !restoreTransferValue(ctx, packet, &value, active, created)) return false;
            entries.emplace(std::move(key), std::move(value));
        }
        object->entries = std::move(entries);
    }
    *newHandle = ctx.objects->add(std::move(object));
    created.push_back(*newHandle);
    active.erase(oldHandle);
    return true;
}

/**
 * @brief 将传输值恢复为目标运行时的原生值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool restoreTransfer(NativeContext &ctx, const TransferPacket &packet, Slot *out)
{
    ObjectValue value = packet.value;
    Memory::Set<std::int64_t> active;
    Memory::Vector<std::int64_t> created;
    if (!restoreTransferValue(ctx, packet, &value, active, created))
    {
        for (auto it = created.rbegin(); it != created.rend(); ++it) ctx.objects->release(*it);
        return false;
    }
    if (value.objectIdentity != 0 && !value.isStruct)
    {
        *out = Slot::fromInt(value.integer);
        return true;
    }
    const bool okay = nativeSlot(ctx, value, out);
    ctx.objects->releaseValue(value);
    return okay;
}

/**
 * @brief 造一个对象并返回句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param kind 待处理对象的类别。
 * @retval 封装为原生值的运行时结果。
 */
Slot nativeNewObject(NativeContext &ctx, ObjectKind kind)
{
    if (ctx.objects == nullptr)
    {
        ctx.error = Memory::String("没有接上对象表，建不了 ") + objectKindName(kind);
        return Slot::fromInt(0);
    }
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = kind;
    return Slot::fromInt(ctx.objects->add(std::move(object)));
}

#if STACKVM_WITH_BIGINT

/**
 * @brief 收下一个算好的大整数（并返回它的句柄）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
Slot nativeNewBigInt(NativeContext &ctx, bigint::BigInt value)
{
    const Slot handle = nativeNewObject(ctx, ObjectKind::BigInt);
    RuntimeObject *object = ctx.objects->find(handle.integer);
    if (object == nullptr) return Slot::fromInt(0);
    object->number = std::move(value);
    return handle;
}

/* ---- 大整数（bint）---- */

/**
 * @brief 执行大整数从整数构造并封装运行时结果。
 *
 * `__vm_bint_from_int(值) -> bint`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintFromInt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return nativeNewBigInt(ctx, bigint::BigInt(args[0].isFloat ? static_cast<std::int64_t>(args[0].real) : args[0].integer));
}

/**
 * @brief 从无符号整数完整构造大数，保留 uint64 的高位。
 * @param ctx 原生调用上下文。
 * @param args 包含无符号整数位模式的参数列表。
 * @retval 新建的大数对象句柄。
 */
Slot bintFromUInt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return nativeNewBigInt(ctx, bigint::BigInt::fromU64(static_cast<std::uint64_t>(args[0].integer)));
}

/**
 * @brief 执行大整数从文本构造并封装运行时结果。
 *
 * `__vm_bint_from_str(十进制文本地址) -> bint`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintFromStr(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "构造大整数时读不到文本（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Slot::fromInt(0);
    }
    bool ok = false;
    const bigint::BigInt value = bigint::BigInt::fromString(text, &ok);
    if (!ok)
    {
        ctx.error = "不是合法的大整数文本：\"" + text + "\"（只认可选的 +/- 号加十进制数字）";
        return Slot::fromInt(0);
    }
    return nativeNewBigInt(ctx, value);
}

/**
 * @brief 执行大整数从浮点数构造并封装运行时结果。
 *
 * `__vm_bint_from_flt(值) -> bint`（按向零截断）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintFromFloat(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const double value = args[0].isFloat ? args[0].real : static_cast<double>(args[0].integer);
    return nativeNewBigInt(ctx, bigint::BigInt(static_cast<std::int64_t>(value)));
}

/**
 * @brief 执行大整数算术运算并封装运行时结果。
 *
 * 大整数的 `+ - * / %`：取两个句柄 → 算 → 把结果装进**新的**对象。
 * @param op `+` / `-` / `*` / `/` / `%`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintArith(NativeContext &ctx, const Memory::Vector<Slot> &args, char op)
{
    RuntimeObject *left = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数运算");
    if (left == nullptr) return Slot::fromInt(0);
    RuntimeObject *right = nativeObject(ctx, args[1], ObjectKind::BigInt, "大整数运算");
    if (right == nullptr) return Slot::fromInt(0);
    if ((op == '/' || op == '%') && right->number.isZero())
    {
        ctx.error = Memory::String("大整数 ") + op + " 0：除数是零";
        return Slot::fromInt(0);
    }
    switch (op)
    {
    case '+': return nativeNewBigInt(ctx, left->number + right->number);
    case '-': return nativeNewBigInt(ctx, left->number - right->number);
    case '*': return nativeNewBigInt(ctx, left->number * right->number);
    case '/': return nativeNewBigInt(ctx, left->number / right->number);
    default: return nativeNewBigInt(ctx, left->number % right->number);
    }
}

/**
 * @brief 执行大整数加法并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintAdd(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bintArith(ctx, args, '+'); }
/**
 * @brief 执行大整数减法并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintSub(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bintArith(ctx, args, '-'); }
/**
 * @brief 执行大整数乘法并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintMul(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bintArith(ctx, args, '*'); }
/**
 * @brief 执行大整数除法并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintDiv(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bintArith(ctx, args, '/'); }
/**
 * @brief 执行大整数取余并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintMod(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bintArith(ctx, args, '%'); }

/**
 * @brief 执行大整数取反并封装运行时结果。
 *
 * `__vm_bint_neg(值) -> bint`：取负。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintNeg(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数取负");
    if (value == nullptr) return Slot::fromInt(0);
    return nativeNewBigInt(ctx, -value->number);
}

/**
 * @brief 执行大整数比较并封装运行时结果。
 *
 * `__vm_bint_cmp(左, 右) -> -1 / 0 / 1`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintCmp(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *left = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数比较");
    if (left == nullptr) return Slot::fromInt(0);
    RuntimeObject *right = nativeObject(ctx, args[1], ObjectKind::BigInt, "大整数比较");
    if (right == nullptr) return Slot::fromInt(0);
    if (left->number < right->number) return Slot::fromInt(-1);
    if (right->number < left->number) return Slot::fromInt(1);
    return Slot::fromInt(0);
}

/**
 * @brief 执行大整数转换为文本并封装运行时结果。
 *
 * `__vm_bint_str(值) -> 新串地址`：转成十进制文本（`.str()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintToStr(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数转字符串");
    if (value == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, value->number.toString())));
}

/**
 * @brief 大整数 → 64 位整数：取**低 64 位**（二进制补码截断，和 C 的整数转换一个脾气）。
 *
 * 不能借 `toDouble()` 绕（`double` 只有 53 位有效位，`(int)9223372036854775807` 会
 * 因为四舍五入溢出成 0）；这里按 2^32 进制两位两位地取余数拼出来。
 * @param value 大整数。
 * @return 值。
 */
std::int64_t bigIntToInt64(const bigint::BigInt &value)
{
    const bigint::BigInt base(4294967296LL); /* 2^32 */
    bigint::BigInt current = value.abs();
    std::uint64_t low = 0u;
    for (int index = 0; index < 2; ++index)
    {
        bigint::BigInt quotient;
        bigint::BigInt remainder;
        if (!current.divmod(base, &quotient, &remainder)) break;
        low |= static_cast<std::uint64_t>(remainder.toDouble()) << (32 * index);
        current = quotient;
    }
    const std::int64_t bits = static_cast<std::int64_t>(low);
    return (value.sign() < 0) ? -bits : bits;
}

/**
 * @brief 执行大整数转换为整数并封装运行时结果。
 *
 * `__vm_bint_to_int(值) -> 整数`（超出 64 位取低 64 位）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintToInt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数转 int");
    if (value == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(bigIntToInt64(value->number));
}

/**
 * @brief 执行大整数转换为浮点数并封装运行时结果。
 *
 * `__vm_bint_to_flt(值) -> 浮点`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintToFloat(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数转 double");
    if (value == nullptr) return Slot::fromDouble(0.0);
    return Slot::fromDouble(value->number.toDouble());
}

/**
 * @brief 跑一段会抛异常的大数运算，把异常翻译成 VM 错误。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param what 运算名称（拼进错误信息）。
 * @param operation 待执行的运算（返回结果槽）。
 * @retval 封装为原生值的运行时结果。
 */
template <typename Operation>
Slot nativeGuardedBig(NativeContext &ctx, const char *what, Operation operation)
{
    try
    {
        return operation();
    }
    catch (const std::exception &error)
    {
        ctx.error = Memory::String(what) + "：" + error.what();
        return Slot::fromInt(0);
    }
}

/* ---- 大整数扩展方法：幂 / 数论 / 素性 / 位运算 / 进制 / 开方 ---- */

/**
 * @brief `__vm_bint_pow(值, 指数) -> bint`（`.pow(e)`，指数为非负 int64）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintPow(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数幂运算");
    if (value == nullptr) return Slot::fromInt(0);
    const std::int64_t exponent = nativeInt(args[1]);
    if (exponent < 0)
    {
        ctx.error = "BigInteger.pow 的指数不能是负数";
        return Slot::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大整数幂运算", [&] {
        return nativeNewBigInt(ctx, value->number.pow(static_cast<std::uint64_t>(exponent)));
    });
}

/**
 * @brief `__vm_bint_powmod(值, 指数, 模) -> bint`（`.powmod(e, m)`，指数是大整数）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintPowMod(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return nativeGuardedBig(ctx, "大整数模幂", [&] {
        RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数模幂");
        if (value == nullptr) return Slot::fromInt(0);
        RuntimeObject *exponent = nativeObject(ctx, args[1], ObjectKind::BigInt, "大整数模幂");
        if (exponent == nullptr) return Slot::fromInt(0);
        RuntimeObject *modulus = nativeObject(ctx, args[2], ObjectKind::BigInt, "大整数模幂");
        if (modulus == nullptr) return Slot::fromInt(0);
        return nativeNewBigInt(ctx, value->number.powmod(exponent->number, modulus->number));
    });
}

/**
 * @brief `__vm_bint_gcd(左, 右) -> bint`（`.gcd(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintGcd(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *left = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数 gcd");
    if (left == nullptr) return Slot::fromInt(0);
    RuntimeObject *right = nativeObject(ctx, args[1], ObjectKind::BigInt, "大整数 gcd");
    if (right == nullptr) return Slot::fromInt(0);
    return nativeNewBigInt(ctx, bigint::BigInt::gcd(left->number, right->number));
}

/**
 * @brief `__vm_bint_lcm(左, 右) -> bint`（`.lcm(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintLcm(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *left = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数 lcm");
    if (left == nullptr) return Slot::fromInt(0);
    RuntimeObject *right = nativeObject(ctx, args[1], ObjectKind::BigInt, "大整数 lcm");
    if (right == nullptr) return Slot::fromInt(0);
    return nativeNewBigInt(ctx, bigint::BigInt::lcm(left->number, right->number));
}

/**
 * @brief `__vm_bint_mod_nonneg(值, 模) -> bint`（`.mod(m)`，结果落在 `[0, |m|)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintModNonNeg(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return nativeGuardedBig(ctx, "大整数非负取模", [&] {
        RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数非负取模");
        if (value == nullptr) return Slot::fromInt(0);
        RuntimeObject *modulus = nativeObject(ctx, args[1], ObjectKind::BigInt, "大整数非负取模");
        if (modulus == nullptr) return Slot::fromInt(0);
        return nativeNewBigInt(ctx, value->number.modNonNegative(modulus->number));
    });
}

/**
 * @brief `__vm_bint_mod_inverse(值, 模) -> bint`（`.modInverse(m)`，不互素报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintModInverse(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return nativeGuardedBig(ctx, "大整数模逆元", [&] {
        RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数模逆元");
        if (value == nullptr) return Slot::fromInt(0);
        RuntimeObject *modulus = nativeObject(ctx, args[1], ObjectKind::BigInt, "大整数模逆元");
        if (modulus == nullptr) return Slot::fromInt(0);
        return nativeNewBigInt(ctx, bigint::BigInt::modInverse(value->number, modulus->number));
    });
}

/**
 * @brief `__vm_bint_is_probable_prime(值, 轮数) -> bool`（`.isProbablePrime(rounds)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintIsProbablePrime(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数素性测试");
    if (value == nullptr) return Slot::fromInt(0);
    const std::int64_t rounds = nativeInt(args[1]);
    if (rounds < 0)
    {
        ctx.error = "素性测试的轮数不能是负数";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(value->number.isProbablePrime(static_cast<std::size_t>(rounds)) ? 1 : 0);
}

/**
 * @brief `__vm_bint_next_probable_prime(值) -> bint`（`.nextProbablePrime()`，25 轮）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintNextProbablePrime(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "找下一个素数");
    if (value == nullptr) return Slot::fromInt(0);
    return nativeGuardedBig(ctx, "找下一个素数", [&] {
        return nativeNewBigInt(ctx, value->number.nextProbablePrime());
    });
}

/**
 * @brief `__vm_bint_bit_length(值) -> 整数`（`.bitLength()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintBitLength(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数位长");
    if (value == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(value->number.bitLength()));
}

/**
 * @brief 大整数按位运算（`&` / `|` / `^`，只支持非负数）。
 * @param op 运算字符。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintBitwise(NativeContext &ctx, const Memory::Vector<Slot> &args, char op)
{
    return nativeGuardedBig(ctx, "大整数位运算", [&] {
        RuntimeObject *left = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数位运算");
        if (left == nullptr) return Slot::fromInt(0);
        RuntimeObject *right = nativeObject(ctx, args[1], ObjectKind::BigInt, "大整数位运算");
        if (right == nullptr) return Slot::fromInt(0);
        switch (op)
        {
        case '&': return nativeNewBigInt(ctx, left->number & right->number);
        case '|': return nativeNewBigInt(ctx, left->number | right->number);
        default: return nativeNewBigInt(ctx, left->number ^ right->number);
        }
    });
}

/**
 * @brief `__vm_bint_bit_and(左, 右) -> bint`（`.bitAnd(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintBitAnd(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bintBitwise(ctx, args, '&'); }
/**
 * @brief `__vm_bint_bit_or(左, 右) -> bint`（`.bitOr(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintBitOr(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bintBitwise(ctx, args, '|'); }
/**
 * @brief `__vm_bint_bit_xor(左, 右) -> bint`（`.bitXor(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintBitXor(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bintBitwise(ctx, args, '^'); }

/**
 * @brief `__vm_bint_str_radix(值, 进制) -> 新串地址`（`.strRadix(base)`，2..36 进制）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintToStrRadix(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数转进制文本");
    if (value == nullptr) return Slot::fromInt(0);
    const std::int64_t base = nativeInt(args[1]);
    if (base < 2 || base > 36)
    {
        ctx.error = "进制必须在 2..36 之间，现在是 " + Memory::toString(base);
        return Slot::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大整数转进制文本", [&] {
        return Slot::fromInt(static_cast<std::int64_t>(
            nativeAllocString(ctx, value->number.toString(static_cast<unsigned int>(base)))));
    });
}

/**
 * @brief `__vm_bint_iroot(值, k) -> bint`（`.iroot(k)`，整数 k 次方根向下取整）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bintIRoot(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数开方");
    if (value == nullptr) return Slot::fromInt(0);
    const std::int64_t k = nativeInt(args[1]);
    if (k <= 0)
    {
        ctx.error = "根次必须是正整数";
        return Slot::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大整数开方", [&] {
        return nativeNewBigInt(ctx, bigint::integerRoot(value->number, static_cast<std::uint64_t>(k)));
    });
}

/**
 * @brief `__vm_read_bint() -> bint`：把下一项输入按十进制读成一个大整数（`input_bstr`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot readBigInt(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    if (ctx.input == nullptr || ctx.inputIndex == nullptr)
    {
        ctx.error = "这个程序没有接输入，取不到大整数";
        return Slot::fromInt(0);
    }
    Memory::String text;
    if (!nextInputText(ctx, &text))
    {
        ctx.error = "输入不够（第 " + Memory::toString(*ctx.inputIndex + 1u) + " 项缺失）";
        return Slot::fromInt(0);
    }
    bool ok = false;
    const bigint::BigInt value = bigint::BigInt::fromString(text, &ok);
    if (!ok)
    {
        ctx.error = "输入项不是合法的大整数：\"" + text + "\"";
        return Slot::fromInt(0);
    }
    return nativeNewBigInt(ctx, value);
}

#else /* !STACKVM_WITH_BIGINT：大整数没编进来，这几个名字都指向"一句人话" */

/* 名字照样在表里（调用点不用改），但一调就报"这个构建没有把大整数编进来" ——
 * 比"函数入口不存在"清楚得多。 */
#define STACKVM_BINT_STUB(name) \
    Slot name(NativeContext &cx, const Memory::Vector<Slot> &) { return bigIntDisabled(cx); }

STACKVM_BINT_STUB(bintFromInt)
STACKVM_BINT_STUB(bintFromUInt)
STACKVM_BINT_STUB(bintFromStr)
STACKVM_BINT_STUB(bintFromFloat)
STACKVM_BINT_STUB(bintAdd)
STACKVM_BINT_STUB(bintSub)
STACKVM_BINT_STUB(bintMul)
STACKVM_BINT_STUB(bintDiv)
STACKVM_BINT_STUB(bintMod)
STACKVM_BINT_STUB(bintNeg)
STACKVM_BINT_STUB(bintCmp)
STACKVM_BINT_STUB(bintToStr)
STACKVM_BINT_STUB(bintToInt)
STACKVM_BINT_STUB(bintToFloat)
STACKVM_BINT_STUB(bintPow)
STACKVM_BINT_STUB(bintPowMod)
STACKVM_BINT_STUB(bintGcd)
STACKVM_BINT_STUB(bintLcm)
STACKVM_BINT_STUB(bintModNonNeg)
STACKVM_BINT_STUB(bintModInverse)
STACKVM_BINT_STUB(bintIsProbablePrime)
STACKVM_BINT_STUB(bintNextProbablePrime)
STACKVM_BINT_STUB(bintBitLength)
STACKVM_BINT_STUB(bintBitAnd)
STACKVM_BINT_STUB(bintBitOr)
STACKVM_BINT_STUB(bintBitXor)
STACKVM_BINT_STUB(bintToStrRadix)
STACKVM_BINT_STUB(bintIRoot)
STACKVM_BINT_STUB(readBigInt)

#undef STACKVM_BINT_STUB

#endif /* STACKVM_WITH_BIGINT */

#if STACKVM_WITH_BIGFLOAT

/**
 * @brief 在对象存储中创建大浮点数对象。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param value 待转换、保存或参与计算的值。
 * @param precision 大浮点数的计算精度。
 * @retval 封装为原生值的运行时结果。
 */
Slot nativeNewBigFloat(NativeContext &ctx, bigfloat::BigFloat value, int precision = 0)
{
    const Slot handle = nativeNewObject(ctx, ObjectKind::BigFloat);
    RuntimeObject *object = ctx.objects->find(handle.integer);
    if (object == nullptr) return Slot::fromInt(0);
    object->decimal = std::move(value);
    object->decimalPrecision = precision;
    return handle;
}

/**
 * @brief 执行大浮点数从整数构造并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatFromInt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return nativeNewBigFloat(ctx, bigfloat::BigFloat::fromInt(
                                      bigint::BigInt(args[0].isFloat ? static_cast<std::int64_t>(args[0].real) : args[0].integer)));
}

/**
 * @brief 从无符号整数完整构造大数，保留 uint64 的高位。
 * @param ctx 原生调用上下文。
 * @param args 包含无符号整数位模式的参数列表。
 * @retval 新建的大数对象句柄。
 */
Slot bfloatFromUInt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return nativeNewBigFloat(ctx, bigfloat::BigFloat::fromInt(
                                      bigint::BigInt::fromU64(static_cast<std::uint64_t>(args[0].integer))));
}

/**
 * @brief 执行大浮点数从浮点数构造并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatFromFloat(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return nativeNewBigFloat(ctx, bigfloat::BigFloat::fromDouble(
                                      args[0].isFloat ? args[0].real : static_cast<double>(args[0].integer)));
}

/**
 * @brief 执行大浮点数从文本构造并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatFromStr(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "构造大浮点数时读不到文本";
        return Slot::fromInt(0);
    }
    bool ok = false;
    const bigfloat::BigFloat value = bigfloat::BigFloat::fromString(text, &ok);
    if (!ok)
    {
        ctx.error = "不是合法的大浮点数文本：\"" + text + "\"";
        return Slot::fromInt(0);
    }
    return nativeNewBigFloat(ctx, value);
}

/**
 * @brief 执行大浮点数从大整数构造并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatFromBigInt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigInt, "大整数转大浮点数");
    if (value == nullptr) return Slot::fromInt(0);
    return nativeNewBigFloat(ctx, bigfloat::BigFloat::fromInt(value->number));
}

/**
 * @brief 校验大浮点数操作数并执行指定算术运算。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @param op 运算符文本或类别。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatArith(NativeContext &ctx, const Memory::Vector<Slot> &args, char op)
{
    RuntimeObject *left = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数运算");
    if (left == nullptr) return Slot::fromInt(0);
    RuntimeObject *right = nativeObject(ctx, args[1], ObjectKind::BigFloat, "大浮点数运算");
    if (right == nullptr) return Slot::fromInt(0);
    if (op == '/' && right->decimal.isZero())
    {
        ctx.error = "大浮点数除数是零";
        return Slot::fromInt(0);
    }
    /* 结果的精度取两个操作数精度的**最大值**（而不是只看左操作数）。 */
    const int precision = std::max(left->decimalPrecision, right->decimalPrecision);
    if (op == '+')
    {
        auto result = left->decimal + right->decimal;
        return nativeNewBigFloat(ctx, precision ? result.round(precision) : result, precision);
    }
    if (op == '-')
    {
        auto result = left->decimal - right->decimal;
        return nativeNewBigFloat(ctx, precision ? result.round(precision) : result, precision);
    }
    if (op == '*')
    {
        auto result = left->decimal * right->decimal;
        return nativeNewBigFloat(ctx, precision ? result.round(precision) : result, precision);
    }
    const int digits = precision != 0 ? precision : (ctx.bigFloatPrecision ? *ctx.bigFloatPrecision : 34);
    return nativeNewBigFloat(ctx, left->decimal.divide(right->decimal, digits), precision);
}

/**
 * @brief 执行大浮点数加法并封装运行时结果。
 * @param c 原生调用上下文。
 * @param a 第一个操作数或节点编号。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatAdd(NativeContext &c, const Memory::Vector<Slot> &a) { return bfloatArith(c, a, '+'); }
/**
 * @brief 执行大浮点数减法并封装运行时结果。
 * @param c 原生调用上下文。
 * @param a 第一个操作数或节点编号。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatSub(NativeContext &c, const Memory::Vector<Slot> &a) { return bfloatArith(c, a, '-'); }
/**
 * @brief 执行大浮点数乘法并封装运行时结果。
 * @param c 原生调用上下文。
 * @param a 第一个操作数或节点编号。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatMul(NativeContext &c, const Memory::Vector<Slot> &a) { return bfloatArith(c, a, '*'); }
/**
 * @brief 执行大浮点数除法并封装运行时结果。
 * @param c 原生调用上下文。
 * @param a 第一个操作数或节点编号。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatDiv(NativeContext &c, const Memory::Vector<Slot> &a) { return bfloatArith(c, a, '/'); }

/**
 * @brief 执行大浮点数取反并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatNeg(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数取负");
    return value ? nativeNewBigFloat(ctx, -value->decimal, value->decimalPrecision) : Slot::fromInt(0);
}

/**
 * @brief 执行大浮点数比较并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatCmp(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *left = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数比较");
    RuntimeObject *right = nativeObject(ctx, args[1], ObjectKind::BigFloat, "大浮点数比较");
    return (left && right) ? Slot::fromInt(left->decimal.compare(right->decimal)) : Slot::fromInt(0);
}

/**
 * @brief 执行大浮点数转换为文本并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatToStr(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数转字符串");
    if (value == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, value->decimal.toString(0))));
}

/**
 * @brief 执行大浮点数转换为整数并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatToInt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数转 int");
    if (value == nullptr) return Slot::fromInt(0);
    bigint::BigInt result = value->decimal.mantissa();
    const int exponent = value->decimal.exponent10();
    bigint::BigInt scale(1);
    for (int i = 0; i < std::abs(exponent); ++i) scale *= bigint::BigInt(10);
    result = exponent >= 0 ? result * scale : result / scale;
    return Slot::fromInt(bigIntToInt64(result));
}

/**
 * @brief 执行大浮点数转换为浮点数并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatToFloat(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数转 double");
    if (value == nullptr) return Slot::fromDouble(0.0);
    return Slot::fromDouble(std::strtod(value->decimal.toString(17).c_str(), nullptr));
}

/**
 * @brief 设置大浮点数对象的有效精度。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatSetValuePrecision(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "BigDecimal.setPrecision");
    if (value == nullptr) return Slot::fromInt(0);
    const std::int64_t digits = nativeInt(args[1]);
    if (digits < 1 || digits > 100000)
    {
        ctx.error = "BigDecimal 有效位数必须在 1..100000 之间，现在是 " + Memory::toString(digits);
        return Slot::fromInt(0);
    }
    const int previous = value->decimalPrecision != 0 ? value->decimalPrecision :
                                                        (ctx.bigFloatPrecision ? *ctx.bigFloatPrecision : 34);
    value->decimal = value->decimal.round(static_cast<int>(digits));
    value->decimalPrecision = static_cast<int>(digits);
    return Slot::fromInt(previous);
}

/* ---- 大浮点数扩展方法：幂 / 绝对值 / 取整 / 开方 ---- */

/**
 * @brief `__vm_bfloat_pow(值, 指数) -> bfloat`（`.pow(e)`，精确整数次幂）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatPow(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数幂运算");
    if (value == nullptr) return Slot::fromInt(0);
    const std::int64_t exponent = nativeInt(args[1]);
    if (exponent < 0)
    {
        ctx.error = "BigDecimal.pow 的指数不能是负数（负次幂请用除法自己算）";
        return Slot::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大浮点数幂运算", [&] {
        return nativeNewBigFloat(ctx, value->decimal.pow(static_cast<std::uint64_t>(exponent)),
                                 value->decimalPrecision);
    });
}

/**
 * @brief `__vm_bfloat_abs(值) -> bfloat`（`.abs()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatAbs(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数绝对值");
    if (value == nullptr) return Slot::fromInt(0);
    return nativeNewBigFloat(ctx, value->decimal.abs(), value->decimalPrecision);
}

/**
 * @brief `__vm_bfloat_is_integer(值) -> bool`（`.isInteger()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatIsInteger(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数整数判断");
    if (value == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(value->decimal.isInteger() ? 1 : 0);
}

/**
 * @brief 大浮点数取整家族（floor / ceil / trunc，结果是大整数）。
 * @param mode 取整方式：'f' 向负无穷 / 'c' 向正无穷 / 't' 向零。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatRoundToBigInt(NativeContext &ctx, const Memory::Vector<Slot> &args, char mode)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数取整");
    if (value == nullptr) return Slot::fromInt(0);
    switch (mode)
    {
    case 'f': return nativeNewBigInt(ctx, value->decimal.floor());
    case 'c': return nativeNewBigInt(ctx, value->decimal.ceil());
    default: return nativeNewBigInt(ctx, value->decimal.trunc());
    }
}

/**
 * @brief `__vm_bfloat_floor(值) -> bint`（`.floor()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatFloor(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bfloatRoundToBigInt(ctx, args, 'f'); }
/**
 * @brief `__vm_bfloat_ceil(值) -> bint`（`.ceil()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatCeil(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bfloatRoundToBigInt(ctx, args, 'c'); }
/**
 * @brief `__vm_bfloat_trunc(值) -> bint`（`.trunc()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatTrunc(NativeContext &ctx, const Memory::Vector<Slot> &args) { return bfloatRoundToBigInt(ctx, args, 't'); }

/**
 * @brief `__vm_bfloat_sqrt(值, 有效位数) -> bfloat`（`.sqrt(digits)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatSqrt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数开平方");
    if (value == nullptr) return Slot::fromInt(0);
    const std::int64_t digits = nativeInt(args[1]);
    if (digits < 1 || digits > 100000)
    {
        ctx.error = "BigDecimal.sqrt 的有效位数必须在 1..100000 之间，现在是 " + Memory::toString(digits);
        return Slot::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大浮点数开平方", [&] {
        return nativeNewBigFloat(ctx, value->decimal.sqrt(static_cast<int>(digits)), value->decimalPrecision);
    });
}

/**
 * @brief `__vm_bfloat_root(值, k, 有效位数) -> bfloat`（`.root(k, digits)`，k 次方根）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot bfloatRoot(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *value = nativeObject(ctx, args[0], ObjectKind::BigFloat, "大浮点数开 k 次方");
    if (value == nullptr) return Slot::fromInt(0);
    const std::int64_t k = nativeInt(args[1]);
    const std::int64_t digits = nativeInt(args[2]);
    if (k <= 0)
    {
        ctx.error = "BigDecimal.root 的根次必须是正整数";
        return Slot::fromInt(0);
    }
    if (digits < 1 || digits > 100000)
    {
        ctx.error = "BigDecimal.root 的有效位数必须在 1..100000 之间，现在是 " + Memory::toString(digits);
        return Slot::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大浮点数开 k 次方", [&] {
        return nativeNewBigFloat(ctx, value->decimal.root(static_cast<std::uint64_t>(k), static_cast<int>(digits)),
                                 value->decimalPrecision);
    });
}

/**
 * @brief 从对象句柄读取大浮点数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot readBigFloat(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    if (ctx.input == nullptr || ctx.inputIndex == nullptr)
    {
        ctx.error = "这个程序没有接输入，取不到大浮点数";
        return Slot::fromInt(0);
    }
    Memory::String text;
    if (!nextInputText(ctx, &text))
    {
        ctx.error = "输入不够，取不到大浮点数";
        return Slot::fromInt(0);
    }
    bool ok = false;
    const bigfloat::BigFloat value = bigfloat::BigFloat::fromString(text, &ok);
    if (!ok)
    {
        ctx.error = "输入项不是合法的大浮点数：\"" + text + "\"";
        return Slot::fromInt(0);
    }
    return nativeNewBigFloat(ctx, value);
}

#else

/**
 * @brief 报告当前构建未启用大浮点数功能。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot bigFloatDisabled(NativeContext &ctx)
{
    ctx.error = "这个构建没有把大浮点数编进来：用 STACKVM_WITH_BIGFLOAT=1 重建虚拟机";
    return Slot::fromInt(0);
}
#define STACKVM_BFLOAT_STUB(name) \
    Slot name(NativeContext &c, const Memory::Vector<Slot> &) { return bigFloatDisabled(c); }
STACKVM_BFLOAT_STUB(bfloatFromInt)
STACKVM_BFLOAT_STUB(bfloatFromUInt)
STACKVM_BFLOAT_STUB(bfloatFromFloat)
STACKVM_BFLOAT_STUB(bfloatFromStr)
STACKVM_BFLOAT_STUB(bfloatFromBigInt)
STACKVM_BFLOAT_STUB(bfloatAdd)
STACKVM_BFLOAT_STUB(bfloatSub)
STACKVM_BFLOAT_STUB(bfloatMul)
STACKVM_BFLOAT_STUB(bfloatDiv)
STACKVM_BFLOAT_STUB(bfloatNeg)
STACKVM_BFLOAT_STUB(bfloatCmp)
STACKVM_BFLOAT_STUB(bfloatToStr)
STACKVM_BFLOAT_STUB(bfloatToInt)
STACKVM_BFLOAT_STUB(bfloatToFloat)
STACKVM_BFLOAT_STUB(bfloatSetValuePrecision)
STACKVM_BFLOAT_STUB(bfloatPow)
STACKVM_BFLOAT_STUB(bfloatAbs)
STACKVM_BFLOAT_STUB(bfloatIsInteger)
STACKVM_BFLOAT_STUB(bfloatFloor)
STACKVM_BFLOAT_STUB(bfloatCeil)
STACKVM_BFLOAT_STUB(bfloatTrunc)
STACKVM_BFLOAT_STUB(bfloatSqrt)
STACKVM_BFLOAT_STUB(bfloatRoot)
STACKVM_BFLOAT_STUB(readBigFloat)
#undef STACKVM_BFLOAT_STUB

#endif

/* ---- 容器（vec / set / map）---- */

/**
 * @brief 按下标取第 `index` 个（越界报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param object 待访问的对象。
 * @param index 待访问的元素索引。
 * @param out 接收处理结果的输出对象。
 * @param who 诊断信息中使用的操作名称。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool vecIndex(NativeContext &ctx, RuntimeObject *object, std::int64_t index, std::size_t *out,
              const char *who)
{
    if (index < 0 || static_cast<std::size_t>(index) >= object->items.size())
    {
        ctx.error = Memory::String(who) + " 下标越界：" + Memory::toString(index) + "（长度 " + Memory::toString(object->items.size()) + "）";
        return false;
    }
    *out = static_cast<std::size_t>(index);
    return true;
}

/**
 * @brief 按一个字节读取 char 引用，适用于字符串字节及八字节标量槽。
 */
Slot charLoad(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    unsigned char byte = 0;
    if (!nativeRead(ctx, static_cast<std::size_t>(nativeInt(args[0])), reinterpret_cast<char *>(&byte), 1u))
        ctx.error = "字符引用读取地址无效";
    return Slot::fromInt(byte);
}
/**
 * @brief 按一个字节写入 char 引用，避免覆盖相邻字符。
 */
Slot charStore(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const unsigned char byte = static_cast<unsigned char>(nativeInt(args[1]));
    if (!nativeWrite(ctx, static_cast<std::size_t>(nativeInt(args[0])), reinterpret_cast<const char *>(&byte), 1u))
        ctx.error = "字符引用写入地址无效";
    return Slot::fromInt(0);
}

/**
 * @brief 创建空的运行时列表。
 *
 * `__vm_vec_new() -> vec`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecNew(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return nativeNewObject(ctx, ObjectKind::Vector);
}

/**
 * @brief 把宿主入口参数复制成 `std::lang::List<std::lang::String>`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot programArguments(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    const Slot handle = nativeNewObject(ctx, ObjectKind::Vector);
    RuntimeObject *object = ctx.objects == nullptr ? nullptr : ctx.objects->find(handle.integer);
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.programArguments == nullptr) return handle;
    object->items.reserve(ctx.programArguments->size());
    for (const Memory::String &argument : *ctx.programArguments)
    {
        ObjectValue value;
        value.isText = true;
        value.text = argument;
        object->items.push_back(std::move(value));
    }
    return handle;
}

/**
 * @brief 复制文本的原始 UTF-8 字节，避免字符边界索引影响二进制算法。
 * @param ctx 上下文。
 * @param args 文本地址。
 * @return 拥有型字节列表。
 */
Slot stringBytes(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text)) return Slot::fromInt(0);
    const auto result = nativeNewObject(ctx, ObjectKind::Vector);
    auto *vector = ctx.objects->find(nativeInt(result));
    if (!vector) return Slot::fromInt(0);
    vector->items.reserve(text.size());
    for (unsigned char byte : text)
    {
        ObjectValue value;
        value.integer = byte;
        vector->items.push_back(std::move(value));
    }
    return result;
}

/**
 * @brief `__vm_vec_pack(isText, isObject, values...) -> vec`：MAGEL 变长参数的调用点打包。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecPack(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (args.size() < 2u)
    {
        ctx.error = "变长参数打包至少需要两个类型标志";
        return Slot::fromInt(0);
    }
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = ObjectKind::Vector;
    const bool isText = nativeInt(args[0]) != 0;
    const std::int64_t isObject = nativeInt(args[1]);
    for (std::size_t index = 2u; index < args.size(); ++index)
    {
        ObjectValue value;
        if (!nativeValue(ctx, args[index], isText, isObject, true, &value))
        {
            for (const ObjectValue &stored : object->items) ctx.objects->releaseValue(stored);
            return Slot::fromInt(0);
        }
        object->items.push_back(std::move(value));
    }
    return Slot::fromInt(ctx.objects->add(std::move(object)));
}

/**
 * @brief `__vm_vec_push(vec, 值, 值是不是字符串, 值是不是对象句柄) -> 0`：追加到末尾。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecPush(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "vec.push");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), true,
                     &value)) return Slot::fromInt(0);
    object->items.push_back(std::move(value));
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_vec_insert(vec, 下标, 值, 是不是字符串, 是不是对象句柄) -> 0`：插到下标处（允许 == 长度）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecInsert(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "vec.insert");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    const std::int64_t index = nativeInt(args[1]);
    if (index < 0 || static_cast<std::size_t>(index) > object->items.size())
    {
        ctx.error = "vec.insert 的位置越界：" + Memory::toString(index) + "（长度 " + Memory::toString(object->items.size()) + "，只能落在 0..长度 之间）";
        return Slot::fromInt(0);
    }
    ObjectValue value;
    if (!nativeValue(ctx, args[2], nativeInt(args[3]) != 0, nativeInt(args[4]), true,
                     &value)) return Slot::fromInt(0);
    object->items.insert(object->items.begin() + index, std::move(value));
    return Slot::fromInt(0);
}

/**
 * @brief 借用列表元素的稳定地址，嵌套迭代同一元素共享单元并计数。
 */
Slot vecRef(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "foreach");
    if (object == nullptr) return Slot::fromInt(0);
    std::size_t index = 0;
    if (!vecIndex(ctx, object, nativeInt(args[1]), &index, "foreach")) return Slot::fromInt(0);
    const auto found = object->elementViews.find(index);
    if (found != object->elementViews.end())
    {
        ++found->second.users;
        return Slot::fromInt(found->second.address);
    }
    RuntimeObject::ElementView view;
    view.isText = nativeInt(args[2]) != 0;
    view.objectFlag = nativeInt(args[3]);
    view.isFloat = nativeInt(args[4]) != 0;
    view.address = nativeAllocate(ctx, std::max<std::size_t>(8u,
                                                             object->items[index].isStruct ? object->items[index].fields.size() * 8u : 8u));
    if (view.address == 0u)
    {
        ctx.error = "列表迭代引用内存不足";
        return Slot::fromInt(0);
    }
    if (!nativeWriteView(ctx, view, object->items[index]))
    {
        nativeReleaseViewContents(ctx, view);
        ctx.heap->release(view.address);
        return Slot::fromInt(0);
    }
    object->elementViews[index] = view;
    return Slot::fromInt(static_cast<std::int64_t>(view.address));
}
/**
 * @brief 结束列表元素借用，写回当前值，最后一个借用者释放单元及资源叶子。
 */
Slot vecUnref(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "foreach 结束");
    if (object == nullptr) return Slot::fromInt(0);
    for (auto it = object->elementViews.begin(); it != object->elementViews.end(); ++it)
    {
        if (it->second.address != static_cast<std::size_t>(nativeInt(args[1]))) continue;
        if (--it->second.users == 0u)
        {
            nativeReleaseViewContents(ctx, it->second);
            ctx.heap->release(it->second.address);
            object->elementViews.erase(it);
        }
        if (object->pendingRelease && !object->iteratorLeases && object->elementViews.empty())
            ctx.objects->release(nativeInt(args[0]));
        return Slot::fromInt(0);
    }
    ctx.error = "列表迭代引用已释放";
    return Slot::fromInt(0);
}

/**
 * @brief 最后一个租约释放时提交修改，释放单元并完成延期的源列表清理。
 */
IteratorLeaseState::~IteratorLeaseState()
{
    if (!context.objects || context.objects->shuttingDown) return;
    for (const auto &cell : cells)
    {
        if (!context.objects->find(source)) break;
        context.error.clear();
        vecUnref(context, { Slot::fromInt(source), Slot::fromInt(static_cast<std::int64_t>(cell.second)) });
    }
    auto *object = context.objects->find(source);
    if (object && object->iteratorLeases) --object->iteratorLeases;
    if (object && object->pendingRelease && !object->iteratorLeases && object->elementViews.empty()) context.objects->release(source);
}

/**
 * @brief 复制结构体状态到拥有型箱，接口借用共享稳定单元，值复制另建状态。
 * @param ctx 运行上下文。
 * @param args 状态地址、叶子布局、引用类型号和类型名。
 * @return 新状态箱句柄；失败记录运行错误。
 */
Slot iterBox(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    ObjectValue value;
    if (!nativeValue(ctx, args[0], false, nativeInt(args[1]), true, &value)) return Slot::fromInt(0);
    auto box = Memory::makeShared<RuntimeObject>();
    box->kind = ObjectKind::Obj;
    box->iteratorStateBox = true;
    box->iteratorLayout = nativeInt(args[1]);
    box->objTag = nativeInt(args[2]);
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[3])), &box->objTypeName))
    {
        ctx.objects->releaseValue(value);
        ctx.error = "迭代器状态类型名无效";
        return Slot::fromInt(0);
    }
    box->items.push_back(std::move(value));
    box->iteratorContext.heap = ctx.heap;
    box->iteratorContext.data = ctx.data;
    box->iteratorContext.objects = ctx.objects;
    return Slot::fromInt(ctx.objects->add(std::move(box)));
}

/**
 * @brief 分配内存后复位地址复用的生命周期标记。
 * @param ctx 环境。
 * @param bytes 字节数。
 * @return 分配地址。
 */
std::size_t nativeAllocate(NativeContext &ctx, std::size_t bytes)
{
    const auto address = ctx.heap->allocate(bytes);
    if (!address) Memory::restartSignal().recordFailure(Memory::WatchdogReason::PoolExhausted, bytes,
                                                        ctx.heap->liveBytes(), ctx.heap->liveBlocks(), 0u);
    if (address && ctx.objects) ctx.objects->beginStorage(address, bytes, true);
    return address;
}

/**
 * @brief 查询逻辑分配是否仍有效，重复 delete 必须跳过析构与数组载荷读取。
 * @param ctx 当前堆。
 * @param args 块首地址。
 * @return 活跃分配返回一。
 */
Slot heapLive(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    std::size_t capacity = 0;
    return Slot::fromInt(ctx.heap->capacityAt(static_cast<std::size_t>(nativeInt(args[0])), capacity));
}

/**
 * @brief 显式删除前检查块首，清扫后旧引用必须诊断。
 * @param ctx 当前堆。
 * @param args 块首。
 * @return 活跃时为一，墓碑时为零。
 */
Slot heapDeleteLive(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const auto address = static_cast<std::size_t>(nativeInt(args[0]));
    std::size_t capacity = 0;
    if (ctx.heap->capacityAt(address, capacity)) return Slot::fromInt(1);
    if (address && !ctx.heap->retired(address)) ctx.error = "panic：[MGL1205] delete 的来源已释放且墓碑已清扫，或不是有效块首";
    return Slot::fromInt(0);
}
/**
 * @brief 手动撤销释放身份的幂等保证。
 * @param ctx 当前堆。
 * @param args 无参数。
 * @return 清理身份数。
 */
Slot heapSweep(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    (void)args;
    const auto count = ctx.heap->sweep();
    if (count == static_cast<std::size_t>(-1))
    {
        ctx.error = "panic：当前宿主分配器未实现 std::lang::sweep";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<std::int64_t>(count));
}

/**
 * @brief 查询、结束或重新开始 std::lang::move 管理的存储生命周期。
 * @param ctx 本次运行的对象表；标记不跨虚拟机实例共享。
 * @param args 存储地址、字节数和子对象类型号（开始生命周期只需前两项）。
 * @return 查询返回存活标志；重复移动报告运行错误。
 */
template <int Operation>
Slot moveLifetime(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const auto address = static_cast<std::size_t>(nativeInt(args[0]));
    const auto bytes = static_cast<std::size_t>(nativeInt(args[1]));
    auto &ranges = ctx.objects->movedStorage;
    const auto type = Operation == 3 ? 0 : nativeInt(args[2]);
    const auto key = std::make_pair(address, type);
    std::size_t capacity = 0;
    const bool allocationLive = !ctx.heap->contains(address) || ctx.heap->capacityAt(address, capacity);
    const bool live = !bytes || (allocationLive && ranges.find(key) == ranges.end());
    if (Operation == 0) return Slot::fromInt(live);
    if (Operation == 1 && !live) ctx.error = "std::lang::move 的源对象已经销毁，必须先重新赋值";
    if (Operation == 2) ranges[key] = bytes;
    if (Operation == 3) ctx.objects->beginStorage(address, bytes);
    return Slot::fromInt(0);
}

/**
 * @brief 原子转移容器句柄并写回同策略空容器，禁止移动活跃借用来源。
 * @param ctx 运行上下文。
 * @param args 来源容器句柄槽的地址。
 * @return 移出的容器句柄。失败时记录运行错误。
 */
Slot iterTake(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    std::int64_t handle = 0;
    const auto address = static_cast<std::size_t>(nativeInt(args[0]));
    if (!nativeRead(ctx, address, reinterpret_cast<char *>(&handle), sizeof(handle)))
    {
        ctx.error = "intoIter 来源引用无效";
        return Slot::fromInt(0);
    }
    auto *source = ctx.objects->find(handle);
    if (!source || (source->kind != ObjectKind::Vector && source->kind != ObjectKind::Set && source->kind != ObjectKind::Map))
    {
        ctx.error = "intoIter 来源不是容器";
        return Slot::fromInt(0);
    }
    if (source->iteratorLeases || !source->elementViews.empty() || source->mapComparing)
    {
        ctx.error = "活跃引用或比较回调期间不能 intoIter 转移容器";
        return Slot::fromInt(0);
    }
    auto empty = Memory::makeShared<RuntimeObject>();
    empty->kind = source->kind;
    empty->mapEqual = source->mapEqual;
    empty->mapHash = source->mapHash;
    const auto replacement = ctx.objects->add(std::move(empty));
    if (!nativeWrite(ctx, address, reinterpret_cast<const char *>(&replacement), sizeof(replacement)))
    {
        ctx.objects->release(replacement);
        ctx.error = "intoIter 来源引用不可写";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(handle);
}

/**
 * @brief 创建列表引用迭代租约，副本共享单元但游标位置彼此独立。
 * @param ctx 运行上下文。
 * @param args 源列表句柄。
 * @return 新租约句柄。失败时记录运行错误。
 */
Slot iterLeaseNew(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto *source = nativeObject(ctx, args[0], ObjectKind::Vector, "iter");
    if (!source) return Slot::fromInt(0);
    ++source->iteratorLeases;
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = ObjectKind::IteratorLease;
    object->iteratorLease = Memory::makeShared<IteratorLeaseState>();
    object->iteratorLease->context.heap = ctx.heap;
    object->iteratorLease->context.data = ctx.data;
    object->iteratorLease->context.objects = ctx.objects;
    object->iteratorLease->source = nativeInt(args[0]);
    return Slot::fromInt(ctx.objects->add(std::move(object)));
}

/**
 * @brief 返回借用来源当前长度。
 * @param ctx 运行上下文。
 * @param args 租约句柄。
 * @return 来源元素数量。失败时记录运行错误。
 */
Slot iterLeaseLen(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto *lease = nativeObject(ctx, args[0], ObjectKind::IteratorLease, "iter sizeHint");
    if (!lease) return Slot::fromInt(0);
    auto *source = nativeObject(ctx, Slot::fromInt(lease->iteratorLease->source), ObjectKind::Vector, "iter 来源");
    return Slot::fromInt(source ? static_cast<std::int64_t>(source->items.size()) : 0);
}

/**
 * @brief 返回租约持有的稳定元素地址，写入会通过列表的共享单元同步到来源。
 * @param ctx 运行上下文。
 * @param args 租约、索引、字符串标志、叶子布局与浮点标志。
 * @return 稳定元素地址。失败时记录运行错误。
 */
Slot iterLeaseRef(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    auto *lease = nativeObject(ctx, args[0], ObjectKind::IteratorLease, "iter next");
    if (!lease) return Slot::fromInt(0);
    auto &state = *lease->iteratorLease;
    const auto index = static_cast<std::size_t>(nativeInt(args[1]));
    const auto found = state.cells.find(index);
    if (found != state.cells.end()) return Slot::fromInt(static_cast<std::int64_t>(found->second));
    const auto result = vecRef(ctx, { Slot::fromInt(state.source), args[1], args[2], args[3], args[4] });
    if (ctx.error.empty()) state.cells[index] = static_cast<std::size_t>(result.integer);
    return result;
}

/**
 * @brief `__vm_vec_at(vec, 下标, 是不是字符串, 是不是对象句柄) -> 值`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecAt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "vec 取元素");
    if (object == nullptr) return Slot::fromInt(0);
    std::size_t index = 0u;
    if (!vecIndex(ctx, object, nativeInt(args[1]), &index, "vec")) return Slot::fromInt(0);
    Slot out;
    if (!nativeSlot(ctx, object->items[index], &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief `__vm_vec_set(vec, 下标, 值, 是不是字符串, 是不是对象句柄) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecSet(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "vec 改元素");
    if (object == nullptr) return Slot::fromInt(0);
    std::size_t index = 0u;
    if (!vecIndex(ctx, object, nativeInt(args[1]), &index, "vec")) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[2], nativeInt(args[3]) != 0, nativeInt(args[4]), true,
                     &value)) return Slot::fromInt(0);
    const auto view = object->elementViews.find(index);
    if (view != object->elementViews.end())
    {
        nativeReleaseViewContents(ctx, view->second);
        if (!nativeWriteView(ctx, view->second, value))
        {
            ctx.objects->releaseValue(value);
            return Slot::fromInt(0);
        }
    }
    ctx.objects->releaseValue(object->items[index]);
    object->items[index] = std::move(value);
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_vec_remove(vec, 下标) -> 0`：删掉下标处那个。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecRemove(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "vec.remove");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    std::size_t index = 0u;
    if (!vecIndex(ctx, object, nativeInt(args[1]), &index, "vec.remove")) return Slot::fromInt(0);
    ctx.objects->releaseValue(object->items[index]);
    object->items.erase(object->items.begin() + static_cast<std::ptrdiff_t>(index));
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_vec_pop(vec, 是不是字符串, 是不是对象句柄) -> 值`：弹出末尾（空表报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecPop(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "vec.pop");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    if (object->items.empty())
    {
        ctx.error = "vec.pop：表是空的";
        return Slot::fromInt(0);
    }
    Slot out;
    if (!nativeSlot(ctx, object->items.back(), &out)) return Slot::fromInt(0);
    ctx.objects->releaseValue(object->items.back());
    object->items.pop_back();
    return out;
}

/**
 * @brief `__vm_vec_len(vec) -> 元素个数`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecLen(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "len(vec)");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(object->items.size()));
}

/**
 * @brief 释放运行时列表的全部元素。
 *
 * `__vm_vec_clear(vec) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecClear(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "vec.clear");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    for (const ObjectValue &value : object->items) ctx.objects->releaseValue(value);
    object->items.clear();
    return Slot::fromInt(0);
}

/**
 * @brief List 的稳定迭代归并排序：O(n log n) 时间、O(n) 辅助空间，不使用递归栈。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecStableSort(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::sort");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    const std::size_t count = object->items.size();
    if (count < 2u) return Slot::fromInt(0);

#ifdef SIMPLE_SORT
    /* 希尔排序：就地、不额外分配，间距逐步缩到 1；代价是**不稳定**。
     * 定义 `SIMPLE_SORT` 时替换掉默认的自底向上归并，见 README 的构建开关一节。 */
    for (std::size_t gap = count / 2u; gap > 0u; gap /= 2u)
    {
        for (std::size_t index = gap; index < count; ++index)
        {
            ObjectValue held = std::move(object->items[index]);
            std::size_t slot = index;
            while (slot >= gap && held.key() < object->items[slot - gap].key())
            {
                object->items[slot] = std::move(object->items[slot - gap]);
                slot -= gap;
            }
            object->items[slot] = std::move(held);
        }
    }
    return Slot::fromInt(0);
#else
    Memory::Vector<ObjectValue> source = std::move(object->items);
    Memory::Vector<ObjectValue> target(count);
    for (std::size_t width = 1u; width < count;)
    {
        for (std::size_t left = 0u; left < count; left += width * 2u)
        {
            const std::size_t middle = std::min(left + width, count);
            const std::size_t end = std::min(left + width * 2u, count);
            std::size_t first = left;
            std::size_t second = middle;
            std::size_t output = left;
            while (first < middle && second < end)
            {
                /* 相等时取左段，稳定性由这一处分支保证。 */
                if (source[second].key() < source[first].key())
                    target[output++] = std::move(source[second++]);
                else
                    target[output++] = std::move(source[first++]);
            }
            while (first < middle) target[output++] = std::move(source[first++]);
            while (second < end) target[output++] = std::move(source[second++]);
        }
        source.swap(target);
        if (width > count / 2u) break;
        width *= 2u;
    }
    object->items = std::move(source);
    return Slot::fromInt(0);
#endif
}

/**
 * @brief `std::lang::algorithms::sortValue` 的值语义入口：复制 List，再对副本做稳定排序。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecSortedCopy(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.objects == nullptr)
    {
        ctx.error = "std::lang::algorithms::sort 缺少对象表";
        return Slot::fromInt(0);
    }
    Memory::String error;
    const std::int64_t copy = ctx.objects->copyHandle(nativeInt(args[0]), &error);
    if (copy == 0)
    {
        ctx.error = error;
        return Slot::fromInt(0);
    }
    vecStableSort(ctx, { Slot::fromInt(copy) });
    if (!ctx.error.empty())
    {
        ctx.objects->release(copy);
        return Slot::fromInt(0);
    }
    return Slot::fromInt(copy);
}

/**
 * @brief `__vm_vec_find(vec, 值, 是不是字符串, 是不是对象句柄) -> 下标 / -1`（线性找第一个相等的）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecFind(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector, "vec 查找");
    if (object == nullptr) return Slot::fromInt(-1);
    ObjectValue wanted;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &wanted)) return Slot::fromInt(-1);
    const ObjectKey key = wanted.key();
    for (std::size_t index = 0; index < object->items.size(); ++index)
    {
        if (object->items[index].key() < key) continue;
        if (key < object->items[index].key()) continue;
        return Slot::fromInt(static_cast<std::int64_t>(index));
    }
    return Slot::fromInt(-1);
}

/**
 * @brief 将列表项转换为算法回调可接收的值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param value 待转换、保存或参与计算的值。
 * @param out 接收处理结果的输出对象。
 * @param temporaryString 接收回调使用的临时字符串。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool vecCallbackValue(NativeContext &ctx, const ObjectValue &value, NativeValue *out,
                      std::size_t *temporaryString)
{
    *temporaryString = 0u;
    if (value.isStruct)
    {
        Slot materialized;
        if (!nativeSlot(ctx, value, &materialized)) return false;
        *out = NativeValue::fromInt(materialized.integer);
    }
    else if (value.isText)
    {
        *temporaryString = nativeAllocString(ctx, value.text);
        if (*temporaryString == 0u) return false;
        *out = NativeValue::fromInt(static_cast<std::int64_t>(*temporaryString));
    }
    else if (value.isFloat)
        *out = NativeValue::fromFloat(value.real);
    else
        *out = NativeValue::fromInt(value.integer);
    return true;
}

/**
 * @brief 调用谓词回调并取得布尔结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @param all 是否要求全部列表项满足谓词。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecPredicate(NativeContext &ctx, const Memory::Vector<Slot> &args, bool all)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         all ? "std::lang::algorithms::all" : "std::lang::algorithms::any");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    for (const ObjectValue &item : object->items)
    {
        NativeValue argument;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &argument, &temporary)) return Slot::fromInt(0);
        NativeValue result;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { argument }, result);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return Slot::fromInt(0);
        const bool accepted = result.asInt() != 0;
        if (all && !accepted) return Slot::fromInt(0);
        if (!all && accepted) return Slot::fromInt(1);
    }
    return Slot::fromInt(all ? 1 : 0);
}

/**
 * @brief 判断列表中的每一项是否都满足谓词。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecAll(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return vecPredicate(ctx, args, true);
}

/**
 * @brief 判断列表中是否至少有一项满足谓词。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecAny(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return vecPredicate(ctx, args, false);
}

/**
 * @brief 按顺序调用归约回调合并列表项。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecFold(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::fold");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    NativeValue accumulated = args[1].isFloat ? NativeValue::fromFloat(args[1].real) : NativeValue::fromInt(args[1].integer);
    for (const ObjectValue &item : object->items)
    {
        NativeValue value;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &value, &temporary)) return Slot::fromInt(0);
        NativeValue next;
        const bool invoked = (*ctx.invoker)(nativeInt(args[2]), { accumulated, value }, next);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return Slot::fromInt(0);
        accumulated = std::move(next);
    }
    return accumulated.isFloat() ? Slot::fromDouble(accumulated.floating) : Slot::fromInt(accumulated.integer);
}

/**
 * @brief 算法回调的返回值 → 容器里的值。
 *
 * `kind`：0 = 整数（`char` / `bool` 也在这一档），1 = 浮点（f64），2 = 字符串，
 * 3 = 对象句柄。回调只能传整数与浮点（`invoker` 的约定），所以字符串是**地址**、
 * 对象是**句柄**，都装在 `integer` 里。
 */
bool nativeCallbackResult(NativeContext &ctx, const NativeValue &produced, std::int64_t kind,
                          std::int64_t layout,
                          ObjectValue *out)
{
    out->isText = false;
    out->isFloat = false;
    out->ownsObject = false;
    out->objectIdentity = 0;
    out->integer = 0;
    out->real = 0.0;
    if (kind == 2)
    {
        out->isText = true;
        const std::size_t address = static_cast<std::size_t>(produced.asInt());
        const bool read = nativeReadCString(ctx, address, &out->text);
        /* 回调返回的堆串所有权归调用方（MAGEL 的 `return` 是 move）：内容已经复制进
         * `out->text`，这块堆串就地还掉，否则每个字符串元素漏一块。 */
        if (ctx.heap != nullptr) ctx.heap->release(address);
        if (!read)
        {
            ctx.error = "算法回调返回的字符串读不到（地址 " + Memory::toString(produced.asInt()) + "）";
            return false;
        }
        return true;
    }
    if (kind == 3)
        return nativeValue(ctx, Slot::fromInt(produced.asInt()), false, true, true, out);
    if (kind == 4)
        return nativeValue(ctx, Slot::fromInt(produced.asInt()), false, layout, true, out);
    if (kind == 1)
    {
        out->isFloat = true;
        out->real = produced.floating;
        return true;
    }
    out->integer = produced.asInt();
    return true;
}

/**
 * @brief 容器里的值复制一份：`filter` 留下的元素要和原表**各自**持有。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param source 提供输入数据的来源。
 * @param isText 是否将该值作为文本处理。
 * @param isObject 是否将该值作为对象句柄处理。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool nativeCopyValue(NativeContext &ctx, const ObjectValue &source, bool isText, bool isObject,
                     ObjectValue *out)
{
    if (source.isStruct)
    {
        *out = source;
        Memory::String error;
        if (!ctx.objects->copyValue(out, &error))
        {
            ctx.error = error;
            return false;
        }
        return true;
    }
    out->isText = isText;
    out->ownsObject = false;
    out->isFloat = false;
    out->objectIdentity = 0;
    out->integer = 0;
    out->real = 0.0;
    if (isText)
    {
        out->text = source.text; /* 字符串在 ObjectValue 里是值语义 */
        return true;
    }
    out->isFloat = source.isFloat;
    out->real = source.real;
    out->integer = source.integer;
    if (!isObject) return true;
    Memory::String error;
    const std::int64_t copy = ctx.objects->copyHandle(source.integer, &error);
    if (copy == 0)
    {
        ctx.error = error;
        return false;
    }
    const RuntimeObject *object = ctx.objects->find(copy);
    out->integer = copy;
    out->objectIdentity = object == nullptr ? source.objectIdentity : object->identity;
    out->ownsObject = true;
    return true;
}

/**
 * @brief 算法内建中途失败的统一收尾：释放已经攒下的元素，返回 0。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param items 待保存的元素集合。
 * @retval 封装为原生值的运行时结果。
 */
Slot algorithmAbort(NativeContext &ctx, Memory::Vector<ObjectValue> &items)
{
    for (const ObjectValue &value : items) ctx.objects->releaseValue(value);
    if (ctx.error.empty()) ctx.error = "算法内建执行失败";
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_vec_map(vec, 回调, 结果种类) -> 新 vec`：逐元素换成回调的返回值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecMap(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::map");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    const std::int64_t kind = nativeInt(args[2]);
    Memory::Vector<ObjectValue> items;
    items.reserve(object->items.size());
    for (const ObjectValue &item : object->items)
    {
        NativeValue argument;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &argument, &temporary))
            return algorithmAbort(ctx, items);
        NativeValue produced;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { argument }, produced);
        /* 字符串实参的临时块同时也是回调形参的存储：回调 `return 形参` 时结果
         * 就是它（MAGEL 的 return 是 move），这时不能先还，交给 `nativeCallbackResult`
         * 统一释放；结果不是它才在这里还。 */
        if (temporary != 0u && (kind != 2 || produced.asInt() != static_cast<std::int64_t>(temporary)))
            ctx.heap->release(temporary);
        if (!invoked) return algorithmAbort(ctx, items);
        ObjectValue value;
        if (!nativeCallbackResult(ctx, produced, kind, nativeInt(args[3]), &value))
            return algorithmAbort(ctx, items);
        items.push_back(std::move(value));
    }
    auto result = Memory::makeShared<RuntimeObject>();
    result->kind = ObjectKind::Vector;
    result->items = std::move(items);
    return Slot::fromInt(ctx.objects->add(std::move(result)));
}

/**
 * @brief `__vm_vec_filter(vec, 回调, 是不是字符串, 是不是对象) -> 新 vec`：留下回调为真的元素。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecFilter(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::filter");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    const bool isText = nativeInt(args[2]) != 0;
    const bool isObject = nativeInt(args[3]) != 0;
    Memory::Vector<ObjectValue> items;
    for (const ObjectValue &item : object->items)
    {
        NativeValue argument;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &argument, &temporary))
            return algorithmAbort(ctx, items);
        NativeValue produced;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { argument }, produced);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return algorithmAbort(ctx, items);
        if (produced.asInt() == 0) continue;
        ObjectValue value;
        if (!nativeCopyValue(ctx, item, isText, isObject, &value))
            return algorithmAbort(ctx, items);
        items.push_back(std::move(value));
    }
    auto result = Memory::makeShared<RuntimeObject>();
    result->kind = ObjectKind::Vector;
    result->items = std::move(items);
    return Slot::fromInt(ctx.objects->add(std::move(result)));
}

/**
 * @brief `__vm_vec_reduce(vec, 回调) -> 值`：拿**第一个元素**当初值往后折叠；空表报错。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecReduce(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::reduce");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    if (object->items.empty())
    {
        ctx.error = "std::lang::algorithms::reduce 不能作用在空表上（要给初值就用 fold）";
        return Slot::fromInt(0);
    }
    NativeValue accumulated;
    std::size_t seedTemporary = 0u;
    if (!vecCallbackValue(ctx, object->items[0], &accumulated, &seedTemporary))
        return Slot::fromInt(0);
    for (std::size_t index = 1u; index < object->items.size(); ++index)
    {
        NativeValue value;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, object->items[index], &value, &temporary))
            return Slot::fromInt(0);
        NativeValue next;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { accumulated, value }, next);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return Slot::fromInt(0);
        accumulated = std::move(next);
    }
    /* 初值那份临时串和回调返回的那份都**不在这里还**：可能是数据区字面量，
     * 也可能是只有一个元素时直接当结果用的那份（跟 `vecFold` 一个口径）。 */
    return accumulated.isFloat() ? Slot::fromDouble(accumulated.floating) : Slot::fromInt(accumulated.integer);
}

/**
 * @brief `__vm_vec_sum(vec, 是不是浮点) -> 值`：数值求和（空表给 0 / 0.0）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecSum(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::sum");
    if (object == nullptr) return Slot::fromInt(0);
    if (nativeInt(args[1]) != 0)
    {
        double total = 0.0;
        for (const ObjectValue &item : object->items)
            total += item.isFloat ? item.real : static_cast<double>(item.integer);
        return Slot::fromDouble(total);
    }
    std::int64_t total = 0;
    for (const ObjectValue &item : object->items)
        total += item.isFloat ? static_cast<std::int64_t>(item.real) : item.integer;
    return Slot::fromInt(total);
}

/**
 * @brief `__vm_vec_count_if(vec, 回调) -> 整数`：回调为真的元素个数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecCountIf(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::count");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    std::int64_t total = 0;
    for (const ObjectValue &item : object->items)
    {
        NativeValue argument;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &argument, &temporary)) return Slot::fromInt(0);
        NativeValue produced;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { argument }, produced);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return Slot::fromInt(0);
        if (produced.asInt() != 0) ++total;
    }
    return Slot::fromInt(total);
}

/**
 * @brief `__vm_vec_count_value(vec, 值, 是不是字符串, 是不是对象) -> 整数`：等于该值的元素个数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecCountValue(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::count");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue wanted;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &wanted)) return Slot::fromInt(0);
    const ObjectKey key = wanted.key();
    std::int64_t total = 0;
    for (const ObjectValue &item : object->items)
    {
        const ObjectKey candidate = item.key();
        if (candidate < key) continue;
        if (key < candidate) continue;
        ++total;
    }
    return Slot::fromInt(total);
}

/**
 * @brief 已排序 List 的 lower_bound 二分查找；相等元素返回第一项。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecBinarySearch(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::binarySearch");
    if (object == nullptr) return Slot::fromInt(-1);
    ObjectValue wanted;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &wanted))
        return Slot::fromInt(-1);
    const ObjectKey key = wanted.key();
    std::size_t low = 0u;
    std::size_t high = object->items.size();
    while (low < high)
    {
        const std::size_t middle = low + (high - low) / 2u;
        if (object->items[middle].key() < key)
            low = middle + 1u;
        else
            high = middle;
    }
    if (low >= object->items.size() || object->items[low].key() < key || key < object->items[low].key())
        return Slot::fromInt(-1);
    return Slot::fromInt(static_cast<std::int64_t>(low));
}

/**
 * @brief 使用比较回调查找列表中的极值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @param maximum 是否选取最大值。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecExtreme(NativeContext &ctx, const Memory::Vector<Slot> &args, bool maximum)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         maximum ? "std::lang::algorithms::max" : "std::lang::algorithms::min");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue first;
    const std::int64_t objectValue = nativeInt(args[3]);
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, objectValue, false, &first))
        return Slot::fromInt(0);
    const ObjectValue *selected = &first;
    for (const ObjectValue &item : object->items)
    {
        const bool replace = maximum ? selected->key() < item.key() : item.key() < selected->key();
        if (replace) selected = &item;
    }
    if (selected == &first && objectValue == 1) first.ownsObject = true;
    Slot result;
    if (!nativeSlot(ctx, *selected, &result)) return Slot::fromInt(0);
    return result;
}

/**
 * @brief 查找列表中的最小值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecMin(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return vecExtreme(ctx, args, false);
}

/**
 * @brief 查找列表中的最大值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecMax(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    return vecExtreme(ctx, args, true);
}

/**
 * @brief 接收 `[low, high]` List 与 value，返回落在闭区间内的值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot vecClamp(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *bounds = nativeObject(ctx, args[0], ObjectKind::Vector,
                                         "std::lang::algorithms::clamp");
    if (bounds == nullptr) return Slot::fromInt(0);
    if (bounds->items.size() != 2u)
    {
        ctx.error = "std::lang::algorithms::clamp 的边界胶水需要 low/high 两项";
        return Slot::fromInt(0);
    }
    if (bounds->items[1].key() < bounds->items[0].key())
    {
        ctx.error = "std::lang::algorithms::clamp 要求 low <= high";
        return Slot::fromInt(0);
    }
    ObjectValue value;
    const std::int64_t objectValue = nativeInt(args[3]);
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, objectValue, false, &value))
        return Slot::fromInt(0);
    const ObjectValue *selected = &value;
    if (value.key() < bounds->items[0].key())
        selected = &bounds->items[0];
    else if (bounds->items[1].key() < value.key())
        selected = &bounds->items[1];
    if (selected == &value && objectValue == 1) value.ownsObject = true;
    Slot result;
    if (!nativeSlot(ctx, *selected, &result)) return Slot::fromInt(0);
    return result;
}

/**
 * @brief 创建空的运行时集合。
 *
 * `__vm_set_new() -> set`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot setNew(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return nativeNewObject(ctx, ObjectKind::Set);
}

/**
 * @brief 为自定义 Set 元素绑定 Equal 与可选 Hash，复制容器时保留策略。
 */
Slot setNewPolicy(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const Slot result = nativeNewObject(ctx, ObjectKind::Set);
    if (auto *object = ctx.objects->find(result.integer))
    {
        object->mapEqual = nativeInt(args[0]);
        object->mapHash = nativeInt(args[1]);
    }
    return result;
}

/**
 * @brief Set 与 Map 共用精确 Equal、可选 Hash 及回调重入护栏。
 */
auto mapFind(NativeContext &ctx, RuntimeObject &object, const Slot &query, ObjectKey *lookup)
    -> Memory::Map<ObjectKey, ObjectValue>::iterator;

/**
 * @brief `__vm_set_add(set, 值, 是不是字符串, 是不是对象句柄) -> 0`（已经在里面就什么都不做）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot setAdd(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Set, "set.add");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &value)) return Slot::fromInt(0);
    ObjectKey key = value.key();
    const auto found = mapFind(ctx, *object, args[1], &key);
    if (!ctx.error.empty()) return Slot::fromInt(0);
    if (found == object->entries.end())
    {
        ObjectValue stored;
        if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), true,
                         &stored)) return Slot::fromInt(0);
        if (object->mapEqual && !ctx.objects->ownKey(&key, &ctx.error))
        {
            ctx.objects->releaseValue(stored);
            return Slot::fromInt(0);
        }
        object->entries.emplace(std::move(key), std::move(stored));
    }
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_set_has(set, 值, 是不是字符串, 是不是对象句柄) -> 0 / 1`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot setHas(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Set, "set.has");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &value)) return Slot::fromInt(0);
    ObjectKey key = value.key();
    const auto found = mapFind(ctx, *object, args[1], &key);
    return Slot::fromInt(found != object->entries.end() ? 1 : 0);
}

/**
 * @brief `__vm_set_remove(set, 值, 是不是字符串, 是不是对象句柄) -> 0 / 1`（在不在）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot setRemove(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Set, "set.remove");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &value)) return Slot::fromInt(0);
    ObjectKey key = value.key();
    const auto found = mapFind(ctx, *object, args[1], &key);
    if (!ctx.error.empty()) return Slot::fromInt(0);
    if (found == object->entries.end()) return Slot::fromInt(0);
    ctx.objects->releaseKey(found->first);
    ctx.objects->releaseValue(found->second);
    object->entries.erase(found);
    return Slot::fromInt(1);
}

/**
 * @brief `__vm_set_at(set, 第几个, 是不是字符串, 是不是对象句柄) -> 值`（按键的升序）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot setAt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Set, "set.at");
    if (object == nullptr) return Slot::fromInt(0);
    const std::int64_t wanted = nativeInt(args[1]);
    if (wanted < 0 || static_cast<std::size_t>(wanted) >= object->entries.size())
    {
        ctx.error = "set.at 的位置越界：" + Memory::toString(wanted) + "（元素个数 " + Memory::toString(object->entries.size()) + "）";
        return Slot::fromInt(0);
    }
    auto it = object->entries.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(wanted));
    /* 集合里存进去的"值"就是元素本身（`setAdd` 用的是 `entries[key] = value`），
     * 所以直接取 `second` 就行。 */
    Slot out;
    if (!nativeSlot(ctx, it->second, &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief `__vm_set_len(set) -> 元素个数`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot setLen(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Set, "len(set)");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(object->entries.size()));
}

/**
 * @brief 释放运行时集合的全部元素。
 *
 * `__vm_set_clear(set) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot setClear(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Set, "set.clear");
    if (object == nullptr) return Slot::fromInt(0);
    for (const auto &entry : object->entries)
    {
        ctx.objects->releaseKey(entry.first);
        ctx.objects->releaseValue(entry.second);
    }
    object->entries.clear();
    return Slot::fromInt(0);
}

/**
 * @brief 创建空的运行时映射。
 *
 * `__vm_map_new() -> map`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapNew(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return nativeNewObject(ctx, ObjectKind::Map);
}

/**
 * @brief 为自定义键 Map 绑定等于与可选哈希函数。
 * @param ctx 运行时上下文。
 * @param args Equal 和 Hash 函数地址；Hash 为零表示顺序等于查找。
 * @return 新建 Map 句柄。
 */
Slot mapNewPolicy(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const Slot result = nativeNewObject(ctx, ObjectKind::Map);
    if (auto *object = ctx.objects->find(result.integer))
    {
        object->mapEqual = nativeInt(args[0]);
        object->mapHash = nativeInt(args[1]);
    }
    return result;
}

/**
 * @brief 把存储键还原为回调可借用的槽，临时文本和聚合内存在回调后回收。
 * @param ctx 运行时上下文。
 * @param key 存入 Map 的键快照。
 * @param out 接收可传给 Equal 回调的键槽。
 * @param temporaries 登记回调结束时需要释放的堆地址。
 * @return 还原成功返回 true；失败设置运行时错误。
 */
bool mapCallbackKey(NativeContext &ctx, const ObjectKey &key, Slot *out,
                    Memory::Vector<std::size_t> *temporaries)
{
    if (key.isStruct)
    {
        const auto address = nativeAllocate(ctx, std::max<std::size_t>(8u, key.fields.size() * 8u));
        if (!address)
        {
            ctx.error = "Map 比较键时堆内存不足";
            return false;
        }
        temporaries->push_back(address);
        for (std::size_t i = 0; i < key.fields.size(); ++i)
        {
            Slot field;
            if (!mapCallbackKey(ctx, key.fields[i], &field, temporaries)) return false;
            std::int64_t bits = field.integer;
            if (field.isFloat) std::memcpy(&bits, &field.real, sizeof(bits));
            if (!ctx.heap->writeBytes(address + i * 8u, reinterpret_cast<const char *>(&bits), 8u))
            {
                ctx.error = "Map 比较键时写入临时槽失败";
                return false;
            }
        }
        *out = Slot::fromInt(static_cast<std::int64_t>(address));
    }
    else if (key.isText)
    {
        const auto address = nativeAllocString(ctx, key.text);
        if (!address) return false;
        temporaries->push_back(address);
        *out = Slot::fromInt(static_cast<std::int64_t>(address));
    }
    else if (key.isFloat)
        *out = Slot::fromDouble(key.real);
    else
        *out = Slot::fromInt(key.isObject && key.ownsObject ? key.ownedInteger : key.integer);
    return true;
}

/**
 * @brief 通过内建比较或自定义 Equal 查找键，Hash 可选并缓存到存储键。
 * @param ctx 运行时上下文。
 * @param object Map 对象。
 * @param query 原始查询键槽，传递给用户回调。
 * @param lookup 查询键快照，Hash 启用时补写缓存信息。
 * @return 找到的迭代器；未找到或回调失败返回 end。
 */
auto mapFind(NativeContext &ctx, RuntimeObject &object, const Slot &query, ObjectKey *lookup)
    -> Memory::Map<ObjectKey, ObjectValue>::iterator
{
    if (!object.mapEqual) return object.entries.find(*lookup);
    if (!ctx.invoker)
    {
        ctx.error = "当前后端不支持 Map 键回调";
        return object.entries.end();
    }
    struct Guard
    {
        RuntimeObject &object;
        /**
         * @brief 保存并切换当前运行时回调的临时状态。
         * @param value 待转换、保存或参与计算的值。
         */
        explicit Guard(RuntimeObject &value) : object(value) { object.mapComparing = true; }
        /**
         * @brief 恢复当前运行时回调的临时状态。
         */
        ~Guard() { object.mapComparing = false; }
    } guard(object);
    const auto nativeQuery = query.isFloat ? NativeValue::fromFloat(query.real) : NativeValue::fromInt(query.integer);
    if (object.mapHash)
    {
        NativeValue hash;
        if (!(*ctx.invoker)(object.mapHash, { nativeQuery }, hash)) return object.entries.end();
        lookup->hashed = true;
        lookup->hash = hash.asInt();
    }
    auto first = object.entries.begin();
    if (lookup->hashed)
    {
        ObjectKey lower;
        lower.hashed = true;
        lower.hash = lookup->hash;
        first = object.entries.lower_bound(lower);
    }
    for (auto it = first; it != object.entries.end(); ++it)
    {
        if (lookup->hashed && it->first.hash != lookup->hash) break;
        Memory::Vector<std::size_t> temporaries;
        struct Release
        {
            NativeContext &ctx;
            Memory::Vector<std::size_t> &addresses;
            /**
             * @brief 释放当前作用域持有的临时运行时对象。
             */
            ~Release()
            {
                for (auto it = addresses.rbegin(); it != addresses.rend(); ++it) ctx.heap->release(*it);
            }
        } release{ ctx, temporaries };
        Slot stored;
        if (!mapCallbackKey(ctx, it->first, &stored, &temporaries)) return object.entries.end();
        const auto nativeStored = stored.isFloat ? NativeValue::fromFloat(stored.real) : NativeValue::fromInt(stored.integer);
        NativeValue equal;
        if (!(*ctx.invoker)(object.mapEqual, { nativeStored, nativeQuery }, equal)) return object.entries.end();
        if (equal.asInt()) return it;
    }
    return object.entries.end();
}

/**
 * @brief `__vm_map_put(map, 键, 键是不是字符串, 键是不是对象句柄, 值, 值是不是字符串, 值是不是对象句柄) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapPut(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Map, "map.put");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue key;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &key)) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[4], nativeInt(args[5]) != 0, nativeInt(args[6]), true,
                     &value)) return Slot::fromInt(0);
    ObjectKey lookup = key.key();
    const auto found = mapFind(ctx, *object, args[1], &lookup);
    if (!ctx.error.empty())
    {
        ctx.objects->releaseValue(value);
        return Slot::fromInt(0);
    }
    if (found != object->entries.end())
    {
        ctx.objects->releaseValue(found->second);
        found->second = std::move(value);
        return Slot::fromInt(0);
    }
    ObjectKey storedKey = lookup;
    if (storedKey.isObject || storedKey.isStruct)
    {
        Memory::String error;
        if (!ctx.objects->ownKey(&storedKey, &error))
        {
            ctx.error = error;
            ctx.objects->releaseValue(value);
            return Slot::fromInt(0);
        }
    }
    object->entries.emplace(std::move(storedKey), std::move(value));
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_map_get(map, 键, 键是不是字符串, 键是不是对象句柄, 值是不是字符串, 值是不是对象句柄) -> 值`（没有这个键给"零值"）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapGet(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Map, "map.get");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue key;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &key)) return Slot::fromInt(0);
    ObjectKey lookup = key.key();
    const auto found = mapFind(ctx, *object, args[1], &lookup);
    if (!ctx.error.empty()) return Slot::fromInt(0);
    if (found == object->entries.end())
    {
        /* 没有这个键：给"零"（数值 0 / 空串），拿 has() 判断在不在。 */
        return (nativeInt(args[4]) != 0) ? Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, Memory::String()))) : Slot::fromInt(0);
    }
    Slot out;
    if (!nativeSlot(ctx, found->second, &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief `__vm_map_has(map, 键, 键是不是字符串, 键是不是对象句柄) -> 0 / 1`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapHas(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Map, "map.has");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue key;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &key)) return Slot::fromInt(0);
    ObjectKey lookup = key.key();
    const auto found = mapFind(ctx, *object, args[1], &lookup);
    return Slot::fromInt(found != object->entries.end() ? 1 : 0);
}

/**
 * @brief `__vm_map_remove(map, 键, 键是不是字符串, 键是不是对象句柄) -> 0 / 1`（在不在）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapRemove(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Map, "map.remove");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue key;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &key)) return Slot::fromInt(0);
    ObjectKey lookup = key.key();
    const auto found = mapFind(ctx, *object, args[1], &lookup);
    if (!ctx.error.empty()) return Slot::fromInt(0);
    if (found == object->entries.end()) return Slot::fromInt(0);
    ctx.objects->releaseKey(found->first);
    ctx.objects->releaseValue(found->second);
    object->entries.erase(found);
    return Slot::fromInt(1);
}

/**
 * @brief `__vm_map_len(map) -> 键值对个数`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapLen(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Map, "len(map)");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(object->entries.size()));
}

/**
 * @brief 释放运行时映射的全部键值。
 *
 * `__vm_map_clear(map) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapClear(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Map, "map.clear");
    if (object == nullptr) return Slot::fromInt(0);
    for (const auto &entry : object->entries)
    {
        ctx.objects->releaseKey(entry.first);
        ctx.objects->releaseValue(entry.second);
    }
    object->entries.clear();
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_map_key_at(map, 第几个, 键是不是字符串, 键是不是对象句柄) -> 键`（按键的升序）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapKeyAt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Map, "map.keyAt");
    if (object == nullptr) return Slot::fromInt(0);
    const std::int64_t wanted = nativeInt(args[1]);
    if (wanted < 0 || static_cast<std::size_t>(wanted) >= object->entries.size())
    {
        ctx.error = "map.keyAt 的位置越界：" + Memory::toString(wanted) + "（键值对个数 " + Memory::toString(object->entries.size()) + "）";
        return Slot::fromInt(0);
    }
    auto it = object->entries.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(wanted));
    const auto fromKey = [&](const auto &self, const ObjectKey &source) -> ObjectValue {
        ObjectValue value;
        value.isStruct = source.isStruct;
        value.isText = source.isText;
        value.isFloat = source.isFloat;
        value.integer = source.ownsObject ? source.ownedInteger : source.integer;
        value.objectIdentity = source.isObject ? source.integer : 0;
        value.real = source.real;
        value.text = source.text;
        value.ownsObject = source.ownsObject;
        for (const ObjectKey &field : source.fields) value.fields.push_back(self(self, field));
        return value;
    };
    ObjectValue key = fromKey(fromKey, it->first);
    Slot out;
    if (!nativeSlot(ctx, key, &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief `__vm_map_val_at(map, 第几个, 值是不是字符串, 值是不是对象句柄) -> 值`（按键的升序）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot mapValAt(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Map, "map.valAt");
    if (object == nullptr) return Slot::fromInt(0);
    const std::int64_t wanted = nativeInt(args[1]);
    if (wanted < 0 || static_cast<std::size_t>(wanted) >= object->entries.size())
    {
        ctx.error = "map.valAt 的位置越界：" + Memory::toString(wanted) + "（键值对个数 " + Memory::toString(object->entries.size()) + "）";
        return Slot::fromInt(0);
    }
    auto it = object->entries.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(wanted));
    Slot out;
    if (!nativeSlot(ctx, it->second, &out)) return Slot::fromInt(0);
    return out;
}

/**
 * `__vm_object_copy(句柄) -> 新句柄`：**复制一份**对象（`bint` / 容器都一样）。
 *
 * 为什么需要：`bint a = b;` / `vec<int> x = y;` 这类写法如果只把句柄抄一份，两个变量
 * 就指向同一个对象 —— 谁被重新赋值（旧值要还回去）另一个就悬空了。所以对象类类型
 * 定成**值语义**：赋值 / 初始化都复制一份（和结构体一个脾气），别名从源头上没有。
 */
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
 * @brief `__vm_object_free(句柄) -> 0`：把对象还回去（`delete` 用；不是句柄就报错）。
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

/**
 * @brief 查询当前存活分配占用的字节数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryLiveBytes(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return Slot::fromInt(ctx.heap == nullptr ? 0 : static_cast<std::int64_t>(ctx.heap->liveBytes()));
}

/**
 * @brief `__vm_watchdog_enable(watermarkBytes) -> int64`：程序侧打开软重启看门狗。
 *
 * 对应 Magel 的 `std::watchdog::enable(watermarkBytes)`。默认关闭：不调它就
 * `__vm_request_restart()` 只报 `NotEnabled`、不产生重启。水位由宿主每轮量（VM 只记下来）。
 * @param ctx 环境。
 * @param args `[水位字节数]`。
 * @return 0 = 成功；1 = 水位为负（参数非法）。
 */
Slot watchdogEnable(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    (void)ctx;
    const std::int64_t watermark = args.empty() ? 0 : nativeInt(args[0]);
    if (watermark < 0) return Slot::fromInt(1);
    Memory::restartSignal().enable(static_cast<std::size_t>(watermark));
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_request_restart() -> int64`：程序显式请求软重启（写标志 + 原因）。
 *
 * 对应 Magel 的 `std::watchdog::requestRestart()`。**未 `enable()` 时不产生重启**，
 * 返回 1（`NotEnabled`）；已开启则写 `WatchdogReason::ExplicitRequest`，VM 在下一条
 * 指令边界（安全点）以"可恢复终止"结束 `run()`。
 * @param ctx 环境。
 * @param args 无。
 * @return 0 = 已记录；1 = 没有 `enable()` 过。
 */
Slot requestRestart(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    (void)ctx;
    (void)args;
    return Slot::fromInt(Memory::restartSignal().requestRestart() ? 0 : 1);
}

/**
 * @brief 查询当前存活分配的块数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryLiveBlocks(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return Slot::fromInt(ctx.heap == nullptr ? 0 : static_cast<std::int64_t>(ctx.heap->liveBlocks()));
}

/**
 * @brief 查询内存分配的峰值字节数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryPeakBytes(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return Slot::fromInt(ctx.heap == nullptr ? 0 : static_cast<std::int64_t>(ctx.heap->peakBytes()));
}

/**
 * @brief 创建运行时 arena 对象。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaCreate(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return nativeNewObject(ctx, ObjectKind::Arena);
}

/**
 * @brief 从指定 arena 分配运行时内存。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaAllocate(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *arena = nativeObject(ctx, args[0], ObjectKind::Arena, "std::lang::Arena.allocate");
    if (arena == nullptr || ctx.heap == nullptr) return Slot::fromInt(0);
    const std::int64_t requested = nativeInt(args[1]);
    if (requested < 0)
    {
        ctx.error = "std::lang::Arena.allocate 的字节数不能为负";
        return Slot::fromInt(0);
    }
    if (requested == 0) return Slot::fromInt(0);
    const std::size_t address = nativeAllocate(ctx, static_cast<std::size_t>(requested));
    if (address == 0u)
    {
        ctx.error = "std::lang::Arena.allocate 分配失败";
        return Slot::fromInt(0);
    }
    arena->arenaBlocks.push_back(address);
    return Slot::fromInt(static_cast<std::int64_t>(address));
}

/**
 * @brief 释放 arena 管理的全部分配并重置状态。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param arena 待操作的运行时 arena。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool resetArena(NativeContext &ctx, RuntimeObject *arena)
{
    if (arena == nullptr || ctx.heap == nullptr) return false;
    for (const std::size_t address : arena->arenaBlocks)
    {
        Memory::String ignored;
        ctx.heap->release(address, &ignored);
    }
    arena->arenaBlocks.clear();
    return true;
}

/**
 * @brief 重置原生调用参数指定的 arena。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaReset(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *arena = nativeObject(ctx, args[0], ObjectKind::Arena, "std::lang::Arena.reset");
    resetArena(ctx, arena);
    return Slot::fromInt(0);
}

/**
 * @brief 销毁 arena 及其持有的分配。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaDrop(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.objects == nullptr) return Slot::fromInt(0);
    const std::int64_t handle = nativeInt(args[0]);
    RuntimeObject *arena = ctx.objects->find(handle);
    if (arena == nullptr) return Slot::fromInt(0); /* 值副本重复析构时保持幂等。 */
    if (arena->kind != ObjectKind::Arena)
    {
        ctx.error = "std::lang::Arena.drop 收到了错误种类的对象句柄";
        return Slot::fromInt(0);
    }
    resetArena(ctx, arena);
    ctx.objects->release(handle);
    return Slot::fromInt(0);
}

/* ---- 装箱格子（obj）----
 *
 * 盒子就是一个"值 + 类型号"：类型号由**前端在编译期**算好（`nameof(类型)` 给的也是它），
 * 所以 `obj.name()` 可以和 `nameof(...)` 直接比 —— 泛型分支就是这么写的。
 *
 * 它同时是**接口指针的运行时表示**：接口指针（`Shape mut*`）是"胖指针"，除了对象的
 * 地址还带着那个对象的具体类型号，调 `q.area()` 时正是靠它挑到具体实现的。
 */

/**
 * @brief `__vm_obj_new() -> obj`：造一个空格子（类型号 0 = 还没 set 过）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot objNew(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return nativeNewObject(ctx, ObjectKind::Obj);
}

/**
 * @brief `__vm_obj_set(obj, 值, 类型号, 值是不是字符串, 值是不是对象句柄, 类型名地址) -> 0`：把值装进去。
 *
 * 原来的值直接丢掉（格子只有一个），类型号与类型名一起换成新的。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objSet(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (args.size() != 6u && (args.size() < 7u || (args.size() - 7u) % 2u))
    {
        ctx.error = "obj.set 的内部文本回调参数不完整";
        return Slot::fromInt(0);
    }
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Obj, "obj.set");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[3]) != 0, nativeInt(args[4]), false,
                     &value)) return Slot::fromInt(0);
    Memory::String name;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[5])), &name))
    {
        ctx.error = "obj.set 读不到类型名（地址 " + Memory::toString(nativeInt(args[5])) + "）";
        return Slot::fromInt(0);
    }
    Memory::String textShape;
    if (args.size() >= 7u && !nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[6])), &textShape))
    {
        ctx.error = "obj.set 读不到文本布局";
        return Slot::fromInt(0);
    }
    object->objTextShape = std::move(textShape);
    object->objTextCallbacks.clear();
    for (std::size_t index = 7; index < args.size(); ++index) object->objTextCallbacks.push_back(nativeInt(args[index]));
    if (object->objOwnsObject) ctx.objects->release(object->objValue.integer);
    object->objTag = nativeInt(args[2]);
    object->objTypeName = std::move(name);
    object->objValue = std::move(value);
    object->objOwnsObject = nativeInt(args[4]) == 1 || (nativeInt(args[4]) == -1 && isObjectHandle(value.integer));
    object->objValue.ownsObject = object->objOwnsObject;
    return Slot::fromInt(0);
}

/**
 * @brief `obj.get` / `obj.get` 取指针 共用的**类型校验**。
 *
 * 前端把"这个位置上允许出现的类型号"全部算好跟在后面（`args[4..]`）：
 * 目标是具体类型时就是它自己，目标是接口指针时是"所有实现了这个接口的结构体指针"。
 * 对不上就写 `ctx.error`（`run()` 会转成运行期错误）。
 *
 * @param ctx 环境。
 * @param object 盒子。
 * @param args 实参：`[1]` 要取的类型的名字地址、`[2]`/`[3]`/`[4]` 形状标志、`[5..]` 允许的类型号。
 * @param who 谁在取（诊断文案）。
 * @return 允许返回 true。
 */
bool objCheckTag(NativeContext &ctx, const RuntimeObject *object, const Memory::Vector<Slot> &args,
                 const char *who)
{
    for (std::size_t index = 5u; index < args.size(); ++index)
    {
        if (nativeInt(args[index]) == object->objTag) return true;
    }
    Memory::String wanted = "?";
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &wanted)) wanted = "?";
    const Memory::String inside = object->objTypeName.empty() ? Memory::String("空盒子") : object->objTypeName;
    ctx.error = Memory::String(who) + "：类型对不上 —— Any 中是 " + inside + "，期望 " + wanted + "。请使用 isType<T> 或 tryGet<T>";
    return false;
}

/**
 * @brief `__vm_obj_get(obj, 要取的类型的名字地址, 是不是字符串, 是不是浮点, 是不是对象句柄, 允许的类型号…) -> 值`。
 *
 * 取出来的是**裸值**（整数 / 浮点 / 堆上的新串 / 指针的地址）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objGet(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Obj, "obj.get");
    if (object == nullptr) return Slot::fromInt(0);
    if (!objCheckTag(ctx, object, args, "obj.get")) return Slot::fromInt(0);
    const ObjectValue &value = object->objValue;
    if (nativeInt(args[2]) != 0)
    {
        if (!value.isText)
        {
            ctx.error = "obj.get：obj 里装的不是字符串，取不出字符串";
            return Slot::fromInt(0);
        }
        return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, value.text)));
    }
    if (nativeInt(args[3]) != 0) return Slot::fromDouble(value.real);
    if (nativeInt(args[4]) != 0 && object->objOwnsObject)
    {
        Memory::String error;
        const std::int64_t copy = ctx.objects->copyHandle(value.integer, &error);
        if (copy == 0)
        {
            ctx.error = error;
            return Slot::fromInt(0);
        }
        return Slot::fromInt(copy);
    }
    return Slot::fromInt(value.integer);
}

/**
 * @brief 判断对象是否带有指定类型标记。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objHasTag(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = ctx.objects->find(nativeInt(args[0]));
    if (object == nullptr || object->kind != ObjectKind::Obj) return Slot::fromInt(0);
    for (std::size_t index = 1u; index < args.size(); ++index)
        if (nativeInt(args[index]) == object->objTag) return Slot::fromInt(1);
    return Slot::fromInt(0);
}

/**
 * @brief `__vm_obj_get_ptr(obj, 要取的类型的名字地址, 0, 0, 1, 允许的类型号…) -> 新的 obj（胖指针）`。
 *
 * 接口指针走这一条：校验过关后**复制一份胖指针**（值语义），交给变量持有 ——
 * 于是"两个接口指针"各拿各的类型号，谁先出块被释放都不会把对方弄悬空。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objGetPointer(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Obj, "obj.get");
    if (object == nullptr) return Slot::fromInt(0);
    if (!objCheckTag(ctx, object, args, "obj.get")) return Slot::fromInt(0);
    Memory::String error;
    if (object->iteratorStateBox)
    {
        auto found = object->elementViews.find(0);
        if (found == object->elementViews.end())
        {
            RuntimeObject::ElementView view;
            view.objectFlag = object->iteratorLayout;
            view.address = nativeAllocate(ctx, std::max<std::size_t>(8, object->items[0].fields.size() * 8));
            if (!view.address)
            {
                ctx.error = "迭代器状态存储不足";
                return Slot::fromInt(0);
            }
            if (!nativeWriteView(ctx, view, object->items[0]))
            {
                nativeReleaseViewContents(ctx, view);
                ctx.heap->release(view.address);
                return Slot::fromInt(0);
            }
            found = object->elementViews.emplace(0, view).first;
        }
        auto pointer = Memory::makeShared<RuntimeObject>();
        pointer->kind = ObjectKind::Obj;
        pointer->objTag = object->objTag;
        pointer->objTypeName = object->objTypeName;
        pointer->objValue.integer = static_cast<std::int64_t>(found->second.address);
        return Slot::fromInt(ctx.objects->add(std::move(pointer)));
    }
    std::shared_ptr<RuntimeObject> copy = ctx.objects->copyObject(*object, &error);
    if (!copy)
    {
        ctx.error = error;
        return Slot::fromInt(0);
    }
    return Slot::fromInt(ctx.objects->add(std::move(copy)));
}

/**
 * @brief `__vm_obj_name(obj) -> 类型号`：和 `nameof(类型)` 是同一个编号（空格子给 0）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objName(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Obj, "obj.name");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(object->objTag);
}

/**
 * @brief 验证 MAGEL 引用非空，阻止未绑定存储暴露为有效引用。
 * @param ctx 运行上下文。
 * @param args 单个引用地址或接口句柄。
 * @return 原引用；零地址时设置运行错误。
 */
Slot requireReference(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const auto address = nativeInt(args[0]);
    if (address == 0) ctx.error = "引用尚未绑定有效元素；缺省值请使用 Option";
    return Slot::fromInt(address);
}

/**
 * @brief `__vm_obj_addr(obj) -> 地址`：胖指针里那个对象的地址（接口分派把它当 `self` 传下去）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objAddress(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeObject(ctx, args[0], ObjectKind::Obj, "接口指针");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(object->objValue.integer);
}

/**
 * @brief `__vm_iface_fail(类型号, 接口.方法 名地址) -> 0`：`q.area()` 分派不到实现时的报错出口。
 *
 * 两种情形都落到这里：接口指针是空的（类型号 0），或者它指着的对象没实现这个接口。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot ifaceFail(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String who = "接口方法";
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &who))
    {
        who = "接口方法";
    }
    const std::int64_t tag = nativeInt(args[0]);
    ctx.error = "接口分派失败：" + who + (tag == 0 ? " 的接口指针是空的（没指向任何对象）" : " 没有类型号 " + Memory::toString(tag) + " 的实现");
    return Slot::fromInt(0);
}

/**
 * @brief 取得原生调用参数对应的文件句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @param who 诊断信息中使用的操作名称。
 * @retval 匹配项的指针，未找到时为空指针。
 */
RuntimeObject *nativeFile(NativeContext &ctx, const Slot &handle, const char *who)
{
    RuntimeObject *object = nativeObject(ctx, handle, ObjectKind::File, who);
    if (object == nullptr) return nullptr;
    if (!object->file || !object->file->isOpen())
    {
        ctx.error = Memory::String(who) + " 用到的文件已经关闭";
        return nullptr;
    }
    return object;
}

/**
 * @brief `__vm_file_open(路径, 模式) -> file`。模式同 C 的 fopen：`r` / `w` / `a` / `r+`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileOpen(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String path;
    Memory::String mode;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &path))
    {
        ctx.error = "fopen 读不到路径字符串";
        return Slot::fromInt(0);
    }
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &mode))
    {
        ctx.error = "fopen 读不到模式字符串";
        return Slot::fromInt(0);
    }
    FileApi *const api = ctx.fileApi != nullptr ? ctx.fileApi : &defaultFileApi();
    Memory::String openError;
    std::shared_ptr<FileHandle> stream = api->open(path, mode, openError);
    if (!stream || !stream->isOpen())
    {
        ctx.error = openError.empty() ? "打不开文件：" + path : openError;
        return Slot::fromInt(0);
    }
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = ObjectKind::File;
    object->file = std::move(stream);
    object->filePath = path;
    return Slot::fromInt(ctx.objects->add(std::move(object)));
}

/**
 * @brief Result 包装层使用：打开失败只返回空句柄，不终止 VM。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileTryOpen(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const Slot result = fileOpen(ctx, args);
    ctx.error.clear();
    return result;
}

/**
 * @brief 判断文件对象的宿主句柄是否有效。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileValid(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = ctx.objects->find(nativeInt(args[0]));
    return Slot::fromInt(object != nullptr && object->kind == ObjectKind::File && object->file && object->file->isOpen() ? 1 : 0);
}

/**
 * @brief `__vm_file_read(file) -> string`：读下一行；EOF 返回空串。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileRead(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeFile(ctx, args[0], "fread");
    if (object == nullptr) return Slot::fromInt(0);
    Memory::String line;
    Memory::String error;
    if (!object->file->readLine(line, error))
    {
        ctx.error = error + "：" + object->filePath;
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, line)));
}

/**
 * @brief 将文本写入文件并返回实际写入字节数。
 *
 * `__vm_file_write(file, text) -> bytes`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileWrite(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeFile(ctx, args[0], "fwrite");
    if (object == nullptr) return Slot::fromInt(0);
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &text))
    {
        ctx.error = "fwrite 读不到要写入的字符串";
        return Slot::fromInt(0);
    }
    Memory::String error;
    if (!object->file->write(text, error))
    {
        ctx.error = error + "：" + object->filePath;
        return Slot::fromInt(0);
    }
    return Slot::fromInt(static_cast<std::int64_t>(text.size()));
}

/**
 * @brief 读取文件对象并为失败结果设置错误信息。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @retval 对应对象的指针。
 */
RuntimeObject *fileForResult(NativeContext &ctx, const Slot &handle)
{
    RuntimeObject *object = ctx.objects->find(nativeInt(handle));
    if (object == nullptr || object->kind != ObjectKind::File)
        return nullptr;
    object->fileError.clear();
    object->fileSucceeded = false;
    if (!object->file || !object->file->isOpen())
    {
        object->fileError = "文件已经关闭";
        return object;
    }
    return object;
}

/**
 * @brief 尝试从宿主文件读取一行文本。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileTryRead(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = fileForResult(ctx, args[0]);
    if (object == nullptr || !object->file || !object->file->isOpen()) return Slot::fromInt(0);
    Memory::String line;
    Memory::String error;
    if (!object->file->readLine(line, error))
    {
        object->fileError = error.empty() ? "读取文件失败" : error;
        return Slot::fromInt(0);
    }
    object->fileSucceeded = true;
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, line)));
}

/**
 * @brief 新文本接口的行读取，拒绝内嵌零字节并去掉 CRLF 的 CR。
 * @param ctx 调用上下文。
 * @param args 文件句柄。
 * @return 新行字符串槽；错误由 Result 层读取。
 */
Slot fileTryReadLine(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = fileForResult(ctx, args[0]);
    if (!object || !object->file || !object->file->isOpen()) return Slot::fromInt(0);
    Memory::String line, error;
    if (!object->file->readLine(line, error))
    {
        object->fileError = error.empty() ? Memory::String("读取文件失败") : error;
        return Slot::fromInt(0);
    }
    if (line.find('\0') != Memory::String::npos)
    {
        object->fileError = "text contains a zero byte";
        return Slot::fromInt(0);
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    object->fileSucceeded = true;
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, line)));
}

/**
 * @brief 尝试向宿主文件写入文本。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileTryWrite(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = fileForResult(ctx, args[0]);
    if (object == nullptr || !object->file || !object->file->isOpen()) return Slot::fromInt(-1);
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &text))
    {
        object->fileError = "读不到待写入字符串";
        ctx.error.clear();
        return Slot::fromInt(-1);
    }
    Memory::String error;
    if (!object->file->write(text, error))
    {
        object->fileError = error.empty() ? "写入文件失败" : error;
        return Slot::fromInt(-1);
    }
    object->fileSucceeded = true;
    return Slot::fromInt(static_cast<std::int64_t>(text.size()));
}

/**
 * @brief 通过 FileApi 读取原始字节列表。
 * @param ctx 上下文。
 * @param args 文件句柄及数量。
 * @return 拥有型列表或零句柄；错误由文件 Result 层读取。
 */
Slot fileTryReadBytes(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *file = fileForResult(ctx, args[0]);
    if (!file || !file->file || !file->file->isOpen()) return Slot::fromInt(0);
    const auto count = nativeInt(args[1]);
    if (count < 0 || static_cast<std::uint64_t>(count) > std::numeric_limits<std::size_t>::max())
    {
        file->fileError = "非法字节读取数量";
        return Slot::fromInt(0);
    }
    Memory::String bytes, error;
    if (!file->file->readBytes(static_cast<std::size_t>(count), bytes, error))
    {
        file->fileError = error.empty() ? Memory::String("字节读取失败") : error;
        return Slot::fromInt(0);
    }
    const auto result = nativeNewObject(ctx, ObjectKind::Vector);
    auto *vector = ctx.objects->find(nativeInt(result));
    if (!vector) return Slot::fromInt(0);
    vector->items.reserve(bytes.size());
    for (unsigned char byte : bytes)
    {
        ObjectValue value;
        value.integer = byte;
        vector->items.push_back(std::move(value));
    }
    file->fileSucceeded = true;
    return result;
}
/**
 * @brief 写入字节列表，保留零字节。
 * @param ctx 上下文。
 * @param args 文件及列表句柄。
 * @return 写入数量或 -1。
 */
Slot fileTryWriteBytes(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *file = fileForResult(ctx, args[0]);
    if (!file || !file->file || !file->file->isOpen()) return Slot::fromInt(-1);
    auto *vector = nativeObject(ctx, args[1], ObjectKind::Vector, "字节写入");
    if (!vector || !nativeRefreshViews(ctx, *vector)) return Slot::fromInt(-1);
    Memory::String bytes;
    bytes.reserve(vector->items.size());
    for (const auto &value : vector->items) bytes.push_back(static_cast<char>(value.integer));
    Memory::String error;
    if (!file->file->write(bytes, error))
    {
        file->fileError = error.empty() ? Memory::String("字节写入失败") : error;
        return Slot::fromInt(-1);
    }
    file->fileSucceeded = true;
    return Slot::fromInt(static_cast<std::int64_t>(bytes.size()));
}

/**
 * @brief 尝试刷新宿主文件输出缓冲区。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileTryFlush(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = fileForResult(ctx, args[0]);
    if (object == nullptr || !object->file || !object->file->isOpen()) return Slot::fromInt(-1);
    Memory::String error;
    if (!object->file->flush(error))
    {
        object->fileError = error.empty() ? "刷新文件失败" : error;
        return Slot::fromInt(-1);
    }
    object->fileSucceeded = true;
    return Slot::fromInt(0);
}

/**
 * @brief 尝试调整宿主文件的读写位置。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileTrySeek(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = fileForResult(ctx, args[0]);
    if (object == nullptr || !object->file || !object->file->isOpen()) return Slot::fromInt(-1);
    Memory::String error;
    std::int64_t position = -1;
    if (!object->file->seek(nativeInt(args[1]), static_cast<int>(nativeInt(args[2])),
                            position, error))
    {
        object->fileError = error.empty() ? "文件定位失败" : error;
        return Slot::fromInt(-1);
    }
    object->fileSucceeded = true;
    return Slot::fromInt(position);
}

/**
 * @brief 查询最近一次文件操作是否成功。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileOperationSucceeded(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = ctx.objects->find(nativeInt(args[0]));
    return Slot::fromInt(object != nullptr && object->kind == ObjectKind::File && object->fileSucceeded ? 1 : 0);
}

/**
 * @brief 读取最近一次文件操作的错误信息。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileLastError(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = ctx.objects->find(nativeInt(args[0]));
    const Memory::String error = object != nullptr && object->kind == ObjectKind::File ? object->fileError : "无效的 file 句柄";
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, error)));
}

/**
 * @brief 尝试关闭宿主文件句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileTryClose(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::int64_t handle = nativeInt(args[0]);
    RuntimeObject *object = ctx.objects->find(handle);
    if (object != nullptr && object->kind == ObjectKind::File)
        ctx.objects->release(handle);
    return Slot::fromInt(0);
}

/**
 * @brief 判断宿主文件是否已到末尾。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileEof(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeFile(ctx, args[0], "feof");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(object->file->eof() ? 1 : 0);
}

/**
 * @brief 关闭宿主文件句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot fileClose(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *object = nativeFile(ctx, args[0], "fclose");
    if (object == nullptr) return Slot::fromInt(0);
    ctx.objects->release(nativeInt(args[0]));
    return Slot::fromInt(0);
}

/**
 * @brief 设置程序退出码并终止当前执行。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programExit(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.exitRequested != nullptr) *ctx.exitRequested = true;
    if (ctx.exitCode != nullptr) *ctx.exitCode = static_cast<int>(nativeInt(args[0]));
    return Slot::fromInt(0);
}

/**
 * @brief 校验运行时断言，失败时报告错误。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programAssert(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (nativeInt(args[0]) == 0) ctx.error = "断言失败：assert(x) 的 x 为假";
    return Slot::fromInt(0);
}

/**
 * @brief 触发程序 panic 并保存诊断信息。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programPanic(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::size_t address = static_cast<std::size_t>(nativeInt(args[0]));
    Memory::String message;
    if (!nativeReadCString(ctx, address, &message))
    {
        ctx.error = "panic 读不到错误信息字符串（地址 " + Memory::toString(address) + "）";
        return Slot::fromInt(0);
    }
    ctx.error = "panic：" + message;
    return Slot::fromInt(0);
}

/* ---------------------------------------------------------------------------
 *  中断事件与软件定时器（`std::event::Timer`）
 *
 * 定时器句柄 = 槽下标 + 1，**单调递增、不复用**：`destroy` 之后原句柄要么被忽略、
 * 要么落到墓碑上，绝不会命中后面新建的定时器（宿主可能还攥着旧句柄）。
 * ------------------------------------------------------------------------- */

struct TimerSlot
{
    std::int64_t period = 0;    ///< 周期（时间单位与 VM 时钟一致）；0 = 只接受 `raise`。
    std::int64_t deadline = 0;  ///< 下次到期时刻；-1 = 还没起算（`start` 之后由主循环补）。
    std::int64_t callback = 0;  ///< 绑定的函数指针地址；0 = 没绑。
    std::int64_t ticks = 0;     ///< 已经交付过多少次。
    bool running = false;       ///< 软件定时器是否在跑。
    bool alive = false;         ///< `destroy` 之后为 false（墓碑）。
    std::size_t thread = 0u;    ///< 创建它的线程 id：中断只在**这个线程的 VM** 里交付。
};

struct TimerRuntime
{
    Memory::Vector<TimerSlot> slots;         ///< 下标 + 1 = 句柄。
    Memory::Vector<InterruptEvent> pending;  ///< 本次边界要交付的事件。
    std::size_t running = 0u;                ///< 正在跑的定时器个数（快路径用）。
};

/**
 * @brief 按句柄查找运行时定时器。
 * @param timers 运行时定时器集合。
 * @param handle 待操作的运行时句柄。
 * @retval 匹配项的指针，未找到时为空指针。
 */
TimerSlot *timerFind(TimerRuntime *timers, std::int64_t handle)
{
    if (timers == nullptr || handle <= 0) return nullptr;
    const std::size_t index = static_cast<std::size_t>(handle - 1);
    if (index >= timers->slots.size()) return nullptr;
    return &timers->slots[index];
}

/**
 * @brief 创建运行时定时器。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerCreate(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.timers == nullptr)
    {
        ctx.error = "这个运行上下文没有定时器表";
        return Slot::fromInt(0);
    }
    TimerSlot slot;
    slot.period = nativeInt(args[0]);
    if (slot.period < 0) slot.period = 0;
    slot.deadline = -1;
    slot.alive = true;
    slot.thread = ctx.threadId; /* 创建它的线程：中断只在这个线程的 VM 里跑。 */
    ctx.timers->slots.push_back(slot);
    return Slot::fromInt(static_cast<std::int64_t>(ctx.timers->slots.size()));
}

/**
 * @brief 为定时器绑定处理函数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerBind(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    TimerSlot *slot = timerFind(ctx.timers, nativeInt(args[0]));
    if (slot == nullptr || !slot->alive)
    {
        ctx.error = "定时器句柄无效或已经释放：" + Memory::toString(nativeInt(args[0]));
        return Slot::fromInt(0);
    }
    if (nativeInt(args[1]) == 0)
    {
        ctx.error = "定时器回调不能是空函数指针";
        return Slot::fromInt(0);
    }
    if (isObjectHandle(nativeInt(args[1])))
    {
        ctx.error = "定时器异步入口不支持带捕获的 lambda";
        return Slot::fromInt(0);
    }
    slot->callback = nativeInt(args[1]);
    return Slot::fromInt(0);
}

/**
 * @brief 启动运行时定时器。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerStart(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    TimerSlot *slot = timerFind(ctx.timers, nativeInt(args[0]));
    if (slot == nullptr || !slot->alive)
    {
        ctx.error = "定时器句柄无效或已经释放：" + Memory::toString(nativeInt(args[0]));
        return Slot::fromInt(0);
    }
    if (!slot->running)
    {
        slot->running = true;
        slot->deadline = -1; /* 起算点交给主循环（它才拿得到时钟）。 */
        if (ctx.timers != nullptr) ++ctx.timers->running;
    }
    return Slot::fromInt(0);
}

/**
 * @brief 停止运行时定时器。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerStop(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    TimerSlot *slot = timerFind(ctx.timers, nativeInt(args[0]));
    if (slot == nullptr || !slot->alive)
    {
        ctx.error = "定时器句柄无效或已经释放：" + Memory::toString(nativeInt(args[0]));
        return Slot::fromInt(0);
    }
    if (slot->running)
    {
        slot->running = false;
        if (ctx.timers != nullptr && ctx.timers->running > 0u) --ctx.timers->running;
    }
    return Slot::fromInt(0);
}

/**
 * @brief 查询定时器的触发计数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerTicks(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    /* 读计数对墓碑也放行：`destroy` 之后还能看到它交付过多少次（句柄不复用，不会串号）。 */
    const TimerSlot *slot = timerFind(ctx.timers, nativeInt(args[0]));
    return Slot::fromInt(slot == nullptr ? 0 : slot->ticks);
}

/**
 * @brief 查询定时器绑定的线程。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerThread(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const TimerSlot *slot = timerFind(ctx.timers, nativeInt(args[0]));
    return Slot::fromInt(slot == nullptr ? 0 : static_cast<std::int64_t>(slot->thread));
}

/**
 * @brief 判断定时器是否正在运行。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerRunning(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const TimerSlot *slot = timerFind(ctx.timers, nativeInt(args[0]));
    return Slot::fromInt(slot != nullptr && slot->alive && slot->running ? 1 : 0);
}

/* `destroy` 之后句柄是墓碑：`__timerAlive` 就是给"登记前校验"用的软检查，不报错。 */
/**
 * @brief 判断定时器句柄是否仍有效。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerAlive(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const TimerSlot *slot = timerFind(ctx.timers, nativeInt(args[0]));
    return Slot::fromInt(slot != nullptr && slot->alive ? 1 : 0);
}

/**
 * @brief 将定时器事件提交为运行时中断。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerRaise(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.timers == nullptr)
    {
        ctx.error = "这个运行上下文没有定时器表";
        return Slot::fromInt(0);
    }
    /* 程序自己触发也走"延迟到指令边界"的同一条路：处理器里再 raise 不会立刻重入。 */
    ctx.timers->pending.push_back(
        InterruptEvent{ nativeInt(args[0]), nativeInt(args[1]), 0, ctx.threadId });
    return Slot::fromInt(0);
}

/**
 * @brief `Interrupt.raise(source)`：按中断句柄投递 —— 函数指针 + 定时器 + 归属线程。
 *
 * 句柄里的线程不是当前线程就**空操作**（别的线程不受影响）；`callback` 非 0 时直接
 * 跑它，所以"没 `onInterrupt` 绑过回调的定时器"也能用句柄交付。
 */
Slot interruptRaise(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.timers == nullptr)
    {
        ctx.error = "这个运行上下文没有定时器表";
        return Slot::fromInt(0);
    }
    const std::size_t thread = static_cast<std::size_t>(nativeInt(args[1]));
    if (thread != ctx.threadId) return Slot::fromInt(0); /* 归属别的线程：忽略 */
    ctx.timers->pending.push_back(
        InterruptEvent{ nativeInt(args[0]), nativeInt(args[3]), nativeInt(args[2]), thread });
    return Slot::fromInt(0);
}

/**
 * @brief 销毁运行时定时器。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot timerDestroy(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    TimerSlot *slot = timerFind(ctx.timers, nativeInt(args[0]));
    if (slot == nullptr) return Slot::fromInt(0); /* 重复 destroy / 未知句柄都是空操作 */
    if (slot->running)
    {
        slot->running = false;
        if (ctx.timers != nullptr && ctx.timers->running > 0u) --ctx.timers->running;
    }
    slot->alive = false;
    slot->callback = 0;
    return Slot::fromInt(0);
}

/**
 * 名字 → 内部 C++ 原生函数。
 */
struct SlotNativeEntry
{
    const char *first;
    SlotNative second;
};
struct SlotNativeTable
{
    const SlotNativeEntry *begin_;
    std::size_t count_;
    /**
     * @brief 将名字解析为稳定整数身份，动态回调临时名称不自动驻留。
     * @param name 名字。
     * @return 已登记身份或 absent。
     */
    const SlotNativeEntry *find(const Memory::String &name) const
    {
        for (std::size_t i = 0; i < count_; ++i)
            if (name == begin_[i].first) return begin_ + i;
        return nullptr;
    }
    /**
     * @brief 取得容器末尾迭代器。
     * @retval 对应对象的指针。
     */
    const SlotNativeEntry *end() const { return nullptr; }
};
#include "machine_value_text.cpp"

const SlotNativeEntry kSlotNativeEntries[] = {
    { "__vm_value_to_string", { -1, valueToString } },
    { "__vm_exit", { 1, programExit } },
    { "__vm_assert", { 1, programAssert } },
    { "__vm_panic", { 1, programPanic } },
    { "__vm_str_lit", { 1, strLit } },
    { "__vm_str_len", { 1, strLen } },
    { "__vm_str_join", { -1, strJoin } },
    { "__vm_str_sub", { 3, strSub } },
    { "__vm_str_of_int", { 1, strOfInt } },
    { "__vm_str_of_flt", { 2, strOfFlt } },
    { "__vm_str_cmp", { 2, strCmp } },
    { "__vm_str_pos", { 2, strPos } },
    { "__vm_str_at", { 2, strAt } },
    { "__vm_str_set", { 3, strSet } },
    { "__vm_read_text", { 0, readText } },
    { "__vm_input_line", { 0, readText } },
    { "__vm_pause", { 0, pauseForDebug } },
    { "__vm_input_int", { 0, inputInt } },
    { "__vm_input_uint", { 0, inputUInt } },
    { "__vm_input_float", { 0, inputFloat } },
    { "__vm_input_bool", { 0, inputBool } },
    { "__vm_input_char", { 0, inputChar } },
    { "__vm_memcpy", { 3, memCopy } },
    { "__vm_memset", { 3, memSet } },
    /* 大整数（bint）：后端是 library/big_number */
    { "__vm_bint_from_int", { 1, bintFromInt } },
    { "__vm_bint_from_uint", { 1, bintFromUInt } },
    { "__vm_bint_from_str", { 1, bintFromStr } },
    { "__vm_bint_from_flt", { 1, bintFromFloat } },
    { "__vm_bint_add", { 2, bintAdd } },
    { "__vm_bint_sub", { 2, bintSub } },
    { "__vm_bint_mul", { 2, bintMul } },
    { "__vm_bint_div", { 2, bintDiv } },
    { "__vm_bint_mod", { 2, bintMod } },
    { "__vm_bint_neg", { 1, bintNeg } },
    { "__vm_bint_cmp", { 2, bintCmp } },
    { "__vm_bint_str", { 1, bintToStr } },
    { "__vm_bint_to_int", { 1, bintToInt } },
    { "__vm_bint_to_flt", { 1, bintToFloat } },
    { "__vm_bint_pow", { 2, bintPow } },
    { "__vm_bint_powmod", { 3, bintPowMod } },
    { "__vm_bint_gcd", { 2, bintGcd } },
    { "__vm_bint_lcm", { 2, bintLcm } },
    { "__vm_bint_mod_nonneg", { 2, bintModNonNeg } },
    { "__vm_bint_mod_inverse", { 2, bintModInverse } },
    { "__vm_bint_is_probable_prime", { 2, bintIsProbablePrime } },
    { "__vm_bint_next_probable_prime", { 1, bintNextProbablePrime } },
    { "__vm_bint_bit_length", { 1, bintBitLength } },
    { "__vm_bint_bit_and", { 2, bintBitAnd } },
    { "__vm_bint_bit_or", { 2, bintBitOr } },
    { "__vm_bint_bit_xor", { 2, bintBitXor } },
    { "__vm_bint_str_radix", { 2, bintToStrRadix } },
    { "__vm_bint_iroot", { 2, bintIRoot } },
    { "__vm_read_bint", { 0, readBigInt } },
    { "__vm_input_bint", { 0, readBigInt } },
    /* 任意精度十进制浮点（bfloat）：后端是 library/big_number */
    { "__vm_bfloat_from_int", { 1, bfloatFromInt } },
    { "__vm_bfloat_from_uint", { 1, bfloatFromUInt } },
    { "__vm_bfloat_from_flt", { 1, bfloatFromFloat } },
    { "__vm_bfloat_from_str", { 1, bfloatFromStr } },
    { "__vm_bfloat_from_bint", { 1, bfloatFromBigInt } },
    { "__vm_bfloat_add", { 2, bfloatAdd } },
    { "__vm_bfloat_sub", { 2, bfloatSub } },
    { "__vm_bfloat_mul", { 2, bfloatMul } },
    { "__vm_bfloat_div", { 2, bfloatDiv } },
    { "__vm_bfloat_neg", { 1, bfloatNeg } },
    { "__vm_bfloat_cmp", { 2, bfloatCmp } },
    { "__vm_bfloat_str", { 1, bfloatToStr } },
    { "__vm_bfloat_to_int", { 1, bfloatToInt } },
    { "__vm_bfloat_pow", { 2, bfloatPow } },
    { "__vm_bfloat_abs", { 1, bfloatAbs } },
    { "__vm_bfloat_is_integer", { 1, bfloatIsInteger } },
    { "__vm_bfloat_floor", { 1, bfloatFloor } },
    { "__vm_bfloat_ceil", { 1, bfloatCeil } },
    { "__vm_bfloat_trunc", { 1, bfloatTrunc } },
    { "__vm_bfloat_sqrt", { 2, bfloatSqrt } },
    { "__vm_bfloat_root", { 3, bfloatRoot } },
    { "__vm_bfloat_to_flt", { 1, bfloatToFloat } },
    { "__vm_bfloat_set_value_precision", { 2, bfloatSetValuePrecision } },
    { "__vm_read_bfloat", { 0, readBigFloat } },
    { "__vm_input_bfloat", { 0, readBigFloat } },
    /* 容器：vec / set / map */
    { "__vm_program_arguments", { 0, programArguments } },
    { "__vm_vec_new", { 0, vecNew } },
    { "__vm_vec_pack", { -1, vecPack } },
    { "__vm_vec_push", { 4, vecPush } },
    { "__vm_vec_insert", { 5, vecInsert } },
    { "__vm_str_ref", { 2, strRef } },
    { "__vm_char_load", { 1, charLoad } },
    { "__vm_char_store", { 2, charStore } },
    { "__vm_vec_ref", { 5, vecRef } },
    { "__vm_iter_box", { 4, iterBox } },
    { "__vm_heap_live", { 1, heapLive } },
    { "__vm_heap_delete_live", { 1, heapDeleteLive } },
    { "__vm_heap_sweep", { 0, heapSweep } },
    { "__vm_move_live", { 3, moveLifetime<0> } },
    { "__vm_move_require", { 3, moveLifetime<1> } },
    { "__vm_move_end", { 3, moveLifetime<2> } },
    { "__vm_move_begin", { 2, moveLifetime<3> } },
    { "__vm_iter_take", { 1, iterTake } },
    { "__vm_iter_lease", { 1, iterLeaseNew } },
    { "__vm_iter_lease_len", { 1, iterLeaseLen } },
    { "__vm_iter_lease_ref", { 5, iterLeaseRef } },
    { "__vm_vec_unref", { 2, vecUnref } },
    { "__vm_vec_at", { 4, vecAt } },
    { "__vm_vec_set", { 5, vecSet } },
    { "__vm_vec_remove", { 2, vecRemove } },
    { "__vm_vec_pop", { 3, vecPop } },
    { "__vm_vec_len", { 1, vecLen } },
    { "__vm_vec_clear", { 1, vecClear } },
    { "__vm_vec_stable_sort", { 1, vecStableSort } },
    { "__vm_vec_sorted_copy", { 1, vecSortedCopy } },
    { "__vm_vec_find", { 4, vecFind } },
    { "__vm_vec_binary_search", { 4, vecBinarySearch } },
    { "__vm_vec_min", { 4, vecMin } },
    { "__vm_vec_max", { 4, vecMax } },
    { "__vm_vec_clamp", { 4, vecClamp } },
    { "__vm_vec_all", { 2, vecAll } },
    { "__vm_vec_any", { 2, vecAny } },
    { "__vm_vec_fold", { 3, vecFold } },
    { "__vm_vec_map", { 4, vecMap } },
    { "__vm_vec_filter", { 4, vecFilter } },
    { "__vm_vec_reduce", { 2, vecReduce } },
    { "__vm_vec_sum", { 2, vecSum } },
    { "__vm_vec_count_if", { 2, vecCountIf } },
    { "__vm_vec_count_value", { 4, vecCountValue } },
    { "__vm_set_new", { 0, setNew } },
    { "__vm_set_new_policy", { 2, setNewPolicy } },
    { "__vm_set_add", { 4, setAdd } },
    { "__vm_set_has", { 4, setHas } },
    { "__vm_set_remove", { 4, setRemove } },
    { "__vm_set_at", { 4, setAt } },
    { "__vm_set_len", { 1, setLen } },
    { "__vm_set_clear", { 1, setClear } },
    { "__vm_map_new", { 0, mapNew } },
    { "__vm_map_new_policy", { 2, mapNewPolicy } },
    { "__vm_map_put", { 7, mapPut } },
    { "__vm_map_get", { 6, mapGet } },
    { "__vm_map_has", { 4, mapHas } },
    { "__vm_map_remove", { 4, mapRemove } },
    { "__vm_map_len", { 1, mapLen } },
    { "__vm_map_clear", { 1, mapClear } },
    { "__vm_map_key_at", { 4, mapKeyAt } },
    { "__vm_map_val_at", { 4, mapValAt } },
    { "__vm_object_copy", { 1, objectCopy } },
    { "__vm_closure_new", { 2, closureNew } },
    { "__vm_closure_capture", { 4, closureCapture } },
    { "__vm_closure_capture_copy", { 4, closureCaptureCopy } },
    { "__vm_closure_capture_move", { 6, closureCaptureMove } },
    { "__vm_closure_destructor", { 3, closureDestructor } },
    { "__vm_closure_ref", { 5, closureRef } },
    { "__vm_closure_get", { 2, closureGet } },
    { "__vm_function_copy", { 1, functionCopy } },
    { "__vm_object_free", { 1, objectFree } },
    { "std::memory::__memoryLiveBytes", { 0, memoryLiveBytes } },
    /* std::watchdog（软重启看门狗）：模块级原生函数按「模块限定名」登记；裸名一并留着，
     * 便于别的宿主 / 导出代码直接按裸名调用。 */
    { "std::watchdog::__watchdogEnable", { 1, watchdogEnable } },
    { "std::watchdog::__watchdogRequestRestart", { 0, requestRestart } },
    { "__watchdogEnable", { 1, watchdogEnable } },
    { "__watchdogRequestRestart", { 0, requestRestart } },
    { "std::memory::__memoryLiveBlocks", { 0, memoryLiveBlocks } },
    { "std::memory::__memoryPeakBytes", { 0, memoryPeakBytes } },
    { "std::memory::__arenaCreate", { 0, arenaCreate } },
    { "std::memory::__arenaAllocate", { 2, arenaAllocate } },
    { "std::memory::__arenaReset", { 1, arenaReset } },
    { "std::memory::__arenaDrop", { 1, arenaDrop } },
    /* 装箱格子（obj）：值 + 类型号；也是接口指针（胖指针）的运行时表示 */
    { "__vm_obj_new", { 0, objNew } },
    { "__vm_obj_set", { -1, objSet } },
    { "__vm_obj_get", { -1, objGet } }, /* 变参：后面跟一串"允许的类型号" */
    { "__vm_obj_has_tag", { -1, objHasTag } },
    { "__vm_obj_get_ptr", { -1, objGetPointer } },
    { "__vm_obj_name", { 1, objName } },
    { "__vm_obj_addr", { 1, objAddress } },
    { "__vm_reference", { 1, requireReference } },
    { "__vm_iface_fail", { 2, ifaceFail } },
    { "__vm_str_fmt", { -1, strFormat } },
    { "__vm_print", { -1, strPrint } },
    { "__vm_put_char", { 1, putChar } },
    { "__vm_file_open", { 2, fileOpen } },
    { "__vm_file_try_open", { 2, fileTryOpen } },
    { "__vm_file_valid", { 1, fileValid } },
    { "__vm_file_read", { 1, fileRead } },
    { "std::lang::__readLine", { 0, inputReadLine } },
    { "std::lang::__readLines", { 1, inputReadLines } },
    { "std::io::__fileTryReadLine", { 1, fileTryReadLine } },
    { "__fileTryReadLine", { 1, fileTryReadLine } },
    { "__readLine", { 0, inputReadLine } },
    { "__readLines", { 1, inputReadLines } },
    { "__vm_file_write", { 2, fileWrite } },
    { "__vm_file_eof", { 1, fileEof } },
    { "__vm_file_close", { 1, fileClose } },
    { "std::lang::__stringBytes", { 1, stringBytes } },
    { "std::io::__fileTryRead", { 1, fileTryRead } },
    { "std::io::__fileTryReadBytes", { 2, fileTryReadBytes } },
    { "std::io::__fileTryWriteBytes", { 2, fileTryWriteBytes } },
    { "std::io::__fileTryWrite", { 2, fileTryWrite } },
    { "std::io::__fileTryFlush", { 1, fileTryFlush } },
    { "std::io::__fileTrySeek", { 3, fileTrySeek } },
    { "std::io::__fileSucceeded", { 1, fileOperationSucceeded } },
    { "std::io::__fileLastError", { 1, fileLastError } },
    { "std::io::__fileTryClose", { 1, fileTryClose } },
    /* 中断事件 / 软件定时器（`std::event::Timer`）：交付在主循环的指令边界做。 */
    { "std::event::__timerCreate", { 1, timerCreate } },
    { "std::event::__timerBind", { 2, timerBind } },
    { "std::event::__timerStart", { 1, timerStart } },
    { "std::event::__timerStop", { 1, timerStop } },
    { "std::event::__timerTicks", { 1, timerTicks } },
    { "std::event::__timerRunning", { 1, timerRunning } },
    { "std::event::__timerAlive", { 1, timerAlive } },
    { "std::event::__timerThread", { 1, timerThread } },
    { "std::event::__timerRaise", { 2, timerRaise } },
    { "std::event::__interruptRaise", { 4, interruptRaise } },
    { "std::event::__timerDestroy", { 1, timerDestroy } },
};
const SlotNativeTable kSlotNatives{ kSlotNativeEntries, sizeof(kSlotNativeEntries) / sizeof(kSlotNativeEntries[0]) };

}  // namespace
