/**
 * @file chali_memory_guard_test.cpp
 * @brief 验证自定义堆的容量边界优先于底层读写回调，并保留语言 panic 的调用栈。
 */
#include "stackvm.hpp"
#include <cstring>
#include <iostream>

/**
 * @brief 故意允许越界底层读写的测试堆，运行时必须先查询已分配区间。
 */
class ProbeHeap final : public stackvm::HeapAllocator
{
    std::size_t base_ = 0, size_ = 0;
    char bytes_[128]{};

public:
    mutable int reads = 0;
    int writes = 0;
    /**
     * @brief 重置分配状态。
     * @param base 虚拟堆基址。
     */
    void reset(std::size_t base) override
    {
        base_ = base;
        size_ = 0;
        reads = writes = 0;
    }
    /**
     * @brief 查询堆基址。
     * @return 虚拟地址。
     */
    std::size_t base() const override { return base_; }
    /**
     * @brief 分配唯一测试块。
     * @param count 字节数。
     * @return 成功地址或零。
     */
    std::size_t allocate(std::size_t count) override
    {
        if (size_ || !count || count > sizeof(bytes_)) return 0;
        size_ = count;
        return base_;
    }
    /**
     * @brief 释放测试块。
     * @param address 起始地址。
     * @param reason 可选错误文本。
     * @return 是否成功。
     */
    bool release(std::size_t address, Memory::String *reason = nullptr) override
    {
        (void)reason;
        if (address != base_ || !size_) return false;
        size_ = 0;
        return true;
    }
    /**
     * @brief 查询保留地址空间，不代表已经分配。
     * @param address 地址。
     * @return 是否属于池。
     */
    bool contains(std::size_t address) const override { return address >= base_ && address - base_ < sizeof(bytes_); }
    /**
     * @brief 查询活跃分配剩余容量。
     * @param address 地址。
     * @param out 剩余字节。
     * @return 是否有效。
     */
    bool capacityAt(std::size_t address, std::size_t &out) const override
    {
        if (address < base_ || address - base_ >= size_) return false;
        out = size_ - (address - base_);
        return true;
    }
    /**
     * @brief 查询活跃字节数。
     * @return 当前分配容量。
     */
    std::size_t liveBytes() const override { return size_; }
    /**
     * @brief 查询块数。
     * @return 零或一。
     */
    std::size_t liveBlocks() const override { return size_ ? 1 : 0; }
    /**
     * @brief 查询测试池峰值。
     * @return 测试池容量。
     */
    std::size_t peakBytes() const override { return sizeof(bytes_); }
    /**
     * @brief 模拟不可靠的底层读取。
     * @param address 地址。
     * @param out 输出。
     * @param count 字节数。
     * @return 总是成功。
     */
    bool readBytes(std::size_t address, char *out, std::size_t count) const override
    {
        ++reads;
        (void)address;
        std::memset(out, 0, count);
        return true;
    }
    /**
     * @brief 模拟不可靠的底层写入。
     * @param address 地址。
     * @param data 数据。
     * @param count 字节数。
     * @return 总是成功。
     */
    bool writeBytes(std::size_t address, const char *data, std::size_t count) override
    {
        ++writes;
        (void)address;
        (void)data;
        (void)count;
        return true;
    }
};

/**
 * @brief 检查分配边界、释放后访问、VM 错误与资源状态。
 * @return 零表示通过。
 */
int main()
{
    ProbeHeap heap;
    heap.reset(16);
    const auto address = heap.allocate(8);
    std::int64_t integer = 0;
    double real = 0;
    if (!heap.readInt(address, integer) || !heap.writeDouble(address, 1)) return 1;
    const int reads = heap.reads, writes = heap.writes;
#if !STACKVM_WITH_ALLOCATION_GUARDS
    // 测试堆的底层回调不访问实际地址，因此可安全验证裁剪后确实不查询分配范围。
    if (!heap.readDouble(address + 1, real) || !heap.writeInt(address + 1, 1)) return 2;
    if (!heap.release(address) || !heap.readInt(address, integer) || !heap.writeDouble(address, 1)) return 3;
    if (heap.reads != reads + 2 || heap.writes != writes + 2) return 4;
    std::cout << "custom heap unchecked OK\n";
    return 0;
#else
    if (heap.readDouble(address + 1, real) || heap.writeInt(address + 1, 1)) return 2;
    if (!heap.release(address) || heap.readInt(address, integer) || heap.writeDouble(address, 1)) return 3;
    if (heap.reads != reads || heap.writes != writes) return 4;
    stackvm::DataArea data;
    data.binary.resize(16, 0);
    stackvm::Program program;
    auto quads = stackvm::parseQuads("0: (decl, i64, global, p)\n1: (alloc, 8, _, p)\n"
                                     "2: (free, p, _, _)\n3: (load:i64, p, _, value)\n4: (halt, _, _, _)\n",
                                     "memory-guard.quad");
    if (!stackvm::assemble(quads, program)) return 5;
    stackvm::Machine vm;
    vm.setData(&data);
    vm.setHeapAllocator(&heap);
    auto result = vm.run(program);
    if (result.ok || result.error.rfind("panic", 0) != 0 || heap.reads != 0) return 6;
    std::cout << "custom heap memory guard OK\n";
#endif
}
