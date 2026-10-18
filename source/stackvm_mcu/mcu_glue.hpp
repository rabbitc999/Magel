#ifndef PARSE_ENGINE_MCU_VM_GLUE_HPP
#define PARSE_ENGINE_MCU_VM_GLUE_HPP
/**
 * @file mcu_glue.hpp
 * @brief 板端 HAL 胶水的接入点。
 *
 * 桌面构建里标准库胶水由 `magel_glue::Registry`（`glue/std_*.hpp`）注册；
 * 板端不链那一整套，只挂**产品真正用到的**那几个宿主函数。这里放的是最小样板，
 * 也是"哪些名字必须由宿主提供"的清单（见 README）。
 */

#include "stackvm.hpp"

#include <cstddef>

namespace mcuGlue
{
/**
 * @brief 板端输出下沉：`print` / `println` / `putChar` 与 `putXXX` 系列都走它。
 * @param user 上下文。
 * @param bytes 字节。
 * @param count 字节数。
 */
using OutputSink = void (*)(void *user, const char *bytes, std::size_t count);

/**
 * @brief 设置输出下沉（串口 / OLED / RTT / 内存环形缓冲）。
 * @param sink 下沉；`nullptr` = 只往 `RunResult::text` 累积。
 * @param user 上下文。
 * @param keepText 是否同时累积（板端长跑建议 false，省 RAM）。
 */
void SetOutput(OutputSink sink, void *user = nullptr, bool keepText = true);
/**
 * @brief 直接往当前输出下沉写一段（板端自己的代码也能用）。
 */
void Emit(const char *bytes, std::size_t count);
/**
 * @brief 直接往当前输出下沉写 C 字符串。
 */
void Emit(const char *text);

/**
 * @brief 把板端 HAL 胶水挂到这台机器上。
 *
 * 契约：**回调不许抛异常**（板端口径 `-fno-exceptions`），要报错写
 * `NativeContext::error`。
 * @param machine 目标虚拟机（生命周期必须覆盖 `run()`）。
 * @param customRegistry 可选的宿主注册表；其生命周期必须覆盖虚拟机执行。
 */
void Register(stackvm::Machine &machine, stackvm::NativeRegistry *customRegistry = nullptr);
}  // namespace mcuGlue

#endif
