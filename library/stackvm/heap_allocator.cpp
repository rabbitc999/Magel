/**
 * @file heap_allocator.cpp
 * @brief 公开字节堆接口的标量读写适配。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 读 8 字节小端。
 * @param address 地址（数据区内的偏移）。
 * @param out 结果。
 * @return 越界返回 false。
 */
bool HeapAllocator::readInt(std::size_t address, std::int64_t &out) const
{
    return allocatedHeapRange(*this, address, sizeof(out)) && readBytes(address, reinterpret_cast<char *>(&out), sizeof(out));
}

/**
 * @brief 写 8 字节小端。
 * @param address 地址。
 * @param value 值。
 * @return 越界返回 false。
 */
bool HeapAllocator::writeInt(std::size_t address, std::int64_t value)
{
    return allocatedHeapRange(*this, address, sizeof(value)) && writeBytes(address, reinterpret_cast<const char *>(&value), sizeof(value));
}

/**
 * @brief 读 8 字节小端，按 IEEE-754 双精度解释（`(load:f64 …)` 用它）。
 * @param address 地址。
 * @param out 结果。
 * @return 越界返回 false。
 */
bool HeapAllocator::readDouble(std::size_t address, double &out) const
{
    return allocatedHeapRange(*this, address, sizeof(out)) && readBytes(address, reinterpret_cast<char *>(&out), sizeof(out));
}

/**
 * @brief 把双精度写成 8 字节小端（`(store:f64 …)` 用它）。
 * @param address 地址。
 * @param value 值。
 * @return 越界返回 false。
 */
bool HeapAllocator::writeDouble(std::size_t address, double value)
{
    return allocatedHeapRange(*this, address, sizeof(value)) && writeBytes(address, reinterpret_cast<const char *>(&value), sizeof(value));
}
}  // namespace stackvm
