/**
 * @file execution_types.hpp
 * @brief 跨线程状态、事件及存储宽度约定。
 */
#pragma once
#include "vm_config.hpp"

namespace stackvm
{
/**
 * @brief 跨线程操作状态，超时不表示目标任务已停止。
 */
enum class TransferStatus : std::int64_t
{
    Ready = 0,
    Timeout = 1,
    Closed = 2,
    Cancelled = 3,
    Full = 4,
    Failed = 5,
    Invalid = 6
};
/**
 * @brief 宿主或定时源按值投递的事件，不保存用户函数指针。
 */
struct RuntimeEvent
{
    std::int64_t timer = 0;
    std::int64_t tag = 0;
    std::int64_t source = 0;
    std::uint64_t missed = 0;
};
/**
 * @brief 宿主持有的投递函数；不使用 VM 内存池，允许在运行和内存池结束后释放。
 */
using EventPoster = std::function<TransferStatus(RuntimeEvent)>;
struct EventQueue;
struct ExecutionControl;
struct ThreadRuntime;
struct SharedRuntime;
class Machine;

/**
 * @brief VM 内存的最小寻址/分配单元：1 字节。宽值连续占用多个单元。
 */
inline constexpr std::size_t kRuntimeCellBytes = 1u;
static_assert(static_cast<unsigned char>(~0u) == 255u, "VM 要求宿主使用八位字节");
static_assert(sizeof(std::uint8_t) == 1u && alignof(std::uint8_t) == 1u, "VM u8 必须占一个字节且按一字节对齐");
/**
 * @brief 当前整数、浮点、地址和对象句柄的存储宽度。
 */
inline constexpr std::size_t kScalarStorageBytes = 8u;

// ---------------------------------------------------------------------------
//  四元式（与 library/transfer 的文本格式兼容）
// ---------------------------------------------------------------------------
}  // namespace stackvm
