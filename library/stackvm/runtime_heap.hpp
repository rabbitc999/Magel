/**
 * @file runtime_heap.hpp
 * @brief VM 内部 runtime heap 模块的类型与接口。
 */
#pragma once
#include "runtime_slot.hpp"

namespace stackvm::runtimeDetail
{
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
 * 释放立即撤销活跃身份；重复释放、内部地址及未知地址均返回失败。
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
     */
    void reset(std::size_t base) override;

    /**
     * @brief 取得底层内存区域的起始地址。
     *
     * 堆区起始地址。
     * @retval 对应运行时内存区域的地址。
     */
    [[nodiscard]] std::size_t base() const override;

    /**
     * @brief 分配一块字节寻址的内存。
     * @param bytes 字节数。
     * @return 地址；参数为 0 或没内存了返回 0。
     */
    std::size_t allocate(std::size_t bytes) override;

    /**
     * @brief 释放一块内存（地址 0 当空指针放过；相邻空闲块合并）。
     * @param address 地址。
     * @param reason 失败原因（可选）。
     * @return 成功（或空指针）返回 true。
     */
    bool release(std::size_t address, Memory::String *reason = nullptr) override;

    /**
     * @brief 判断是否属于编码堆空间，包括已清扫身份。
     * @param address 地址。
     * @return 属于堆空间为真。
     */
    [[nodiscard]] bool contains(std::size_t address) const override;

    /**
     * @brief 地址所在的**已分配块**从这里起还能用多少字节。
     * @param address 地址。
     * @param capacity 输出：从 address 到块尾的字节数。
     * @return 地址不在任何已分配块里（没分配 / 已经 free）返回 false。
     */
    bool capacityAt(std::size_t address, std::size_t &capacity) const override;

    /**
     * @brief 查询活跃字节数。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t liveBytes() const override;
    /**
     * @brief 查询活跃块数。
     * @return 块数。
     */
    [[nodiscard]] std::size_t liveBlocks() const override;

    /**
     * @brief 查询活跃字节峰值。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t peakBytes() const override;

    /**
     * @brief 读 8 字节（只允许读**已分配**的块）。
     * @param address 地址。
     * @param out 结果。
     * @return 越界 / 已释放返回 false。
     */
    bool readInt(std::size_t address, std::int64_t &out) const;

    /**
     * @brief 写 8 字节。
     * @param address 地址。
     * @param value 值。
     * @return 越界 / 已释放返回 false。
     */
    bool writeInt(std::size_t address, std::int64_t value);

    /**
     * @brief 读 8 字节，按 IEEE-754 双精度解释。
     * @param address 地址。
     * @param out 结果。
     * @return 越界 / 已释放返回 false。
     */
    bool readDouble(std::size_t address, double &out) const;

    /**
     * @brief 把双精度写成 8 字节。
     * @param address 地址。
     * @param value 值。
     * @return 越界 / 已释放返回 false。
     */
    bool writeDouble(std::size_t address, double value);

    /**
     * @brief 按字节读一段（字符串跨 8 字节槽，不能按槽读）。
     * @param address 地址。
     * @param out 输出缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 这 count 个字节全在**同一块已分配内存**里返回 true。
     */
    bool readBytes(std::size_t address, char *out, std::size_t count) const override;

    /**
     * @brief 按字节写一段。
     * @param address 地址。
     * @param in 输入缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 这 count 个字节全在**同一块已分配内存**里返回 true。
     */
    bool writeBytes(std::size_t address, const char *in, std::size_t count) override;

private:
    /**
     * @brief 将有效逻辑地址转换为物理偏移。
     * @param address 已验证的逻辑地址。
     * @return 字节区偏移。
     */
    std::size_t physicalOffset(std::size_t address) const;
    /**
     * @brief 整段范围必须落在同一活跃分配内。
     * @param address 地址。
     * @param count 长度。
     * @return 可访问时为 true。
     */
    [[nodiscard]] bool usableRange(std::size_t address, std::size_t count) const;

    /**
     * @brief 地址是否可读写：落在某个**已分配**块里，且 8 个字节都在块内。
     * @param address 运行时内存地址。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    [[nodiscard]] bool usable(std::size_t address) const;

    /**
     * @brief 更新峰值。
     */
    void refreshPeak();

    execution_detail::AllocationIndex index_;  ///< 两端共用的逻辑区间索引与缓存。
    std::size_t base_ = 8u;                    ///< 堆区起始地址。
    Memory::Vector<std::uint8_t> bytes_;       ///< 字节区（从 base_ 开始）。
    Memory::Vector<Block> blocks_;             ///< 块表，按物理偏移升序。
    std::size_t peak_ = 0u;                    ///< 峰值。
    mutable std::mutex mutex_;                 ///< 串行化 allocate / release。
};
#else /* !STACKVM_WITH_DEFAULT_HEAP */
/**
 * @brief 关闭默认字节堆时使用的空堆：**所有分配都失败**，统计恒为 0。
 *
 * 该配置必须通过 `Machine::setHeapAllocator` 注入堆分配器。
 * 万一没注入，这里让每次分配明确失败（`alloc` 报"堆内存不足" → 记分配失败标志 →
 * 下一个安全点可恢复终止），而不是隐式回退到已关闭的默认堆。
 */
class NoHeapAllocator final : public HeapAllocator
{
public:
    /**
     * @brief 将内存分配器恢复到初始状态。
     */
    void reset(std::size_t) override;
    /**
     * @brief 取得底层内存区域的起始地址。
     * @retval 对应运行时内存区域的地址。
     */
    [[nodiscard]] std::size_t base() const override;
    /**
     * @brief 分配指定字节数的运行时内存。
     * @retval 对应运行时内存区域的地址。
     */
    [[nodiscard]] std::size_t allocate(std::size_t) override;
    /**
     * @brief 释放指定地址对应的运行时资源。
     * @param reason 接收释放失败原因或停止原因的对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool release(std::size_t, Memory::String *reason = nullptr) override;
    /**
     * @brief 判断地址是否属于当前内存区域。
     * @retval 地址属于当前内存区域时为 true，否则为 false。
     */
    [[nodiscard]] bool contains(std::size_t) const override;
    /**
     * @brief 查询指定地址所在分配块的容量。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    [[nodiscard]] bool capacityAt(std::size_t, std::size_t &) const override;
    /**
     * @brief 查询活跃字节数。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t liveBytes() const override;
    /**
     * @brief 查询活跃块数。
     * @return 块数。
     */
    [[nodiscard]] std::size_t liveBlocks() const override;
    /**
     * @brief 查询活跃字节峰值。
     * @return 字节数。
     */
    [[nodiscard]] std::size_t peakBytes() const override;
    /**
     * @brief 从指定地址读取一段字节数据。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    [[nodiscard]] bool readBytes(std::size_t, char *, std::size_t) const override;
    /**
     * @brief 将字节数据写入指定地址。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool writeBytes(std::size_t, const char *, std::size_t) override;
};
#endif
}  // namespace stackvm::runtimeDetail
