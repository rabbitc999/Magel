/**
 * @file heap_allocator.hpp
 * @brief 宿主字节堆分配和访问协议。
 */
#pragma once
#include "quad_model.hpp"

namespace stackvm
{
/**
 * @brief 可替换的 VM 字节堆。
 *
 * `Machine::run()` 每次执行前都会调用 `reset(base)`；实现必须清空上一轮状态，并把地址 0
 * 留给空指针。接口管理的是 CHALI/四元式 `alloc`、字符串和按地址 load/store 使用的字节堆，
 * 不接管 C++ 调用帧、STL 容器或运行期对象表的宿主内存。
 * 默认堆与目标宿主池会复用物理字节，但通过槽号和分配代次区分逻辑身份；旧引用不能访问
 * 新分配对象。释放之后不得再解引用或重复释放；不保存释放墓碑。
 * 自定义堆应分别提供活跃区间与不复用的逻辑身份，不得把物理地址复用当作身份复用。
 */
class HeapAllocator
{
public:
    /**
     * @brief 释放宿主分配器接口。
     */
    virtual ~HeapAllocator() = default;

    /**
     * @brief 重置本轮分配状态并设置数据区之后的堆基址。
     * @param base 堆逻辑地址的起点。
     */
    virtual void reset(std::size_t base) = 0;
    /**
     * @brief 查询当前堆的逻辑起始地址。
     * @return 本轮堆基址。
     */
    [[nodiscard]] virtual std::size_t base() const = 0;
    /**
     * @brief 分配连续字节并建立独立存活身份。
     * @param bytes 需要的字节数。
     * @return 分配地址；失败返回零。
     */
    [[nodiscard]] virtual std::size_t allocate(std::size_t bytes) = 0;
    /**
     * @brief 释放一个仍存活的完整分配块。
     * @param address 分配块的起始地址。
     * @param reason 可选失败原因接收器。
     * @return 成功释放时为真；无效或重复释放时为假。
     */
    virtual bool release(std::size_t address, Memory::String *reason = nullptr) = 0;
    /**
     * @brief 检查地址是否位于存活分配中。
     * @param address 待检查的逻辑地址。
     * @return 属于存活分配时为真。
     */
    [[nodiscard]] virtual bool contains(std::size_t address) const = 0;
    /**
     * @brief 查询地址开始的连续存活字节数。
     * @param address 待检查的逻辑地址。
     * @param capacity 接收该地址到所属分配末端的容量。
     * @return 地址存活时为真。
     */
    [[nodiscard]] virtual bool capacityAt(std::size_t address,
                                          std::size_t &capacity) const = 0;
    /**
     * @brief 统计仍存活的分配字节数。
     * @return 当前存活字节数。
     */
    [[nodiscard]] virtual std::size_t liveBytes() const = 0;
    /**
     * @brief 统计仍存活的分配块数。
     * @return 当前存活块数。
     */
    [[nodiscard]] virtual std::size_t liveBlocks() const = 0;
    /**
     * @brief 查询本轮存活字节峰值。
     * @return 重置后达到的最大存活字节数。
     */
    [[nodiscard]] virtual std::size_t peakBytes() const = 0;
    /**
     * @brief 从存活堆区间复制字节到宿主。
     * @param address 起始逻辑地址。
     * @param out 接收数据的宿主缓冲区。
     * @param count 读取字节数。
     * @return 完整区间可读时为真。
     */
    [[nodiscard]] virtual bool readBytes(std::size_t address, char *out,
                                         std::size_t count) const = 0;
    /**
     * @brief 将宿主字节复制到存活堆区间。
     * @param address 起始逻辑地址。
     * @param data 源宿主缓冲区。
     * @param count 写入字节数。
     * @return 完整区间可写时为真。
     */
    virtual bool writeBytes(std::size_t address, const char *data,
                            std::size_t count) = 0;

    /**
     * @brief 读 8 字节小端。
     * @param address 地址（数据区内的偏移）。
     * @param out 结果。
     * @return 越界返回 false。
     */
    [[nodiscard]] bool readInt(std::size_t address, std::int64_t &out) const;
    /**
     * @brief 写 8 字节小端。
     * @param address 地址。
     * @param value 值。
     * @return 越界返回 false。
     */
    bool writeInt(std::size_t address, std::int64_t value);
    /**
     * @brief 读 8 字节小端，按 IEEE-754 双精度解释（`(load:f64 …)` 用它）。
     * @param address 地址。
     * @param out 结果。
     * @return 越界返回 false。
     */
    [[nodiscard]] bool readDouble(std::size_t address, double &out) const;
    /**
     * @brief 把双精度写成 8 字节小端（`(store:f64 …)` 用它）。
     * @param address 地址。
     * @param value 值。
     * @return 越界返回 false。
     */
    bool writeDouble(std::size_t address, double value);
};
}  // namespace stackvm
