#ifndef PARSE_ENGINE_MCU_VM_FILESYSTEM_HPP
#define PARSE_ENGINE_MCU_VM_FILESYSTEM_HPP
/**
 * @file mcu_filesystem.hpp
 * @brief 板端文件系统接入层：**可以整个关掉**，也可以接 FatFS 或你自己的 VFS。
 *
 * 分三层，按优先级：
 *   1. **板端注入**（`SetApi(&myApi)`）——有 RTOS VFS / 厂家库 / 只读资源表时用这个；
 *      `FileApi` 只有一个 `open`，句柄七个动作，很容易对接；
 *   2. **编译期后端**（`-DMCU_VM_FILE_BACKEND=…`）：
 *        `0` 关闭（缺省）= 桩，`__vm_file_*` 一律报"这个构建没有文件系统"；
 *        `1` C stdio   = 桌面回归用（`std::fopen` 一家子）；
 *        `2` FatFS     = 板端用，`-I` 指向 SDK 的 `ff.h` 并链上 FatFS；
 *   3. 都不给 → 关闭。
 *
 * 与 VM 的关系：VM 只依赖 `stackvm::FileApi` / `stackvm::FileHandle` 这组抽象
 * （`library/stackvm/stackvm_glue.hpp`），所以换文件系统**不用重新编 VM**。
 *
 * MCU 上的两条硬约束按定容实现：打开文件数 `MCU_VM_MAX_OPEN_FILES`（缺省 4，静态槽），
 * 按行读逐字节到 `\n`（不设行长上限，也不额外要缓冲）。
 */

#include "stackvm.hpp"

namespace mcuFilesystem
{
/**
 * @brief 编译期选择的后端。
 */
enum class Backend : int
{
    Disabled = 0,  ///< 关闭：没有文件系统。
    Stdio = 1,     ///< `std::fopen`（桌面回归）。
    FatFs = 2,     ///< FatFS（板端）。
};

/**
 * @brief 本次构建配置的后端。
 */
Backend ConfiguredBackend();
/**
 * @brief 后端名字（诊断 / footprint 报告用）。
 */
const char *BackendName();
/**
 * @brief 这个构建到底能不能打开文件（关闭时 `open` 必失败）。
 */
bool Available();

/**
 * @brief 板端注入自己的文件系统实现（优先于编译期后端）。
 *
 * 必须在 `Machine::run()` 之前调用；`api` 的生命周期要覆盖整个运行期。
 * @param api 实现；传 `nullptr` 退回编译期后端。
 */
void SetApi(stackvm::FileApi *api);
/**
 * @brief 当前生效的实现（注入的优先，否则编译期后端）。
 */
stackvm::FileApi &CurrentApi();
}  // namespace mcuFilesystem

#endif
