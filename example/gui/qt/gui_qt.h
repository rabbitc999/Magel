/* Magel Qt GUI 后端的 C ABI。
 *
 * 与 WinAPI 后端同名（`mcgui_*`），所以 `gui.mgl` 与 `glue.cpp` 的绑定方式不变；
 * MAGEL 与 DLL/so 之间同样只传定宽整数和 UTF-8 字符串。Qt 侧额外提供事件回调：
 * 按钮点击 / 关窗 / Qt 周期事件发生时，通过注册的 C 回调交回胶水层，由胶水
 * 转成 `Machine::raiseInterrupt`，最终在 VM 的指令边界交付 MAGEL 中断处理函数。
 */
#ifndef MAGEL_GUI_QT_H
#define MAGEL_GUI_QT_H

#include <stdint.h>

#if defined(_WIN32)
#define MCGUI_EXPORT __declspec(dllexport)
#else
#define MCGUI_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Qt 事件回调：`event` 是事件值，`source` 是来源计数，`user` 是注册时的上下文。
 *
 * 在 Qt 派发事件的线程（也就是调用 `mcgui_poll_event` / `mcgui_wait_event` /
 * `mcgui_pump` 的 VM 线程）上**同步**调用。事件值：按钮点击 = 控件 ID，
 * 窗口关闭 = -1，Qt 周期事件 = 0（此时 `source` 是第几次）。
 */
typedef void (*mcgui_callback_t)(int64_t event, int64_t source, void *user);

MCGUI_EXPORT int32_t mcgui_open(int32_t width, int32_t height, const char *title);
MCGUI_EXPORT int32_t mcgui_label(int32_t id, int32_t x, int32_t y, int32_t width,
                                 int32_t height, const char *text);
MCGUI_EXPORT int32_t mcgui_button(int32_t id, int32_t x, int32_t y, int32_t width,
                                  int32_t height, const char *text);
MCGUI_EXPORT int32_t mcgui_text_box(int32_t id, int32_t x, int32_t y, int32_t width,
                                    int32_t height, const char *text);
MCGUI_EXPORT int32_t mcgui_set_text(int32_t id, const char *text);
MCGUI_EXPORT const char *mcgui_get_text(int32_t id);
MCGUI_EXPORT int32_t mcgui_poll_event(void);
MCGUI_EXPORT int32_t mcgui_wait_event(void);
MCGUI_EXPORT void mcgui_close(void);

MCGUI_EXPORT void mcgui_pump(void);
MCGUI_EXPORT int32_t mcgui_post_event(int32_t event);
MCGUI_EXPORT void mcgui_start_timer(int32_t interval_ms);
MCGUI_EXPORT void mcgui_stop_timer(void);
MCGUI_EXPORT void mcgui_set_callback(mcgui_callback_t callback, void *user);

#ifdef __cplusplus
}
#endif

#endif /* MAGEL_GUI_QT_H */
