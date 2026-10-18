# Magel 简易 GUI（WinAPI / Qt / C ABI）

[`gui.mgl`](gui.mgl) 是 Magel 可 `import gui;` 的公共声明；[`qt/gui_qt.cpp`](qt/gui_qt.cpp)
编成 `libmcguiqt.so`（Windows 上是 `mcguiqt.dll`），用 Qt Widgets 创建窗口与控件；
[`glue.cpp`](glue.cpp) 把 Magel 符号绑到共享库的 C 函数。Magel 与库之间只传固定宽度整数与
UTF-8 字符串，不传递窗口指针或控件结构。

两个后端均支持**事件回调**：GUI 事件发生时就地通过 C 回调交回胶水层，
胶水通过统一运行时投递器排队，由 VM / AOT 在下一个执行边界交付 Magel 中断事件函数。这样
GUI 事件走的是和[宿主集成](../doc/08-host-integration.md)里的回调投放同一套"异步发生、安全点交付"机制。

## Windows 原生后端

安装 MinGW 并将 `gcc`、`g++` 放入 PATH，先构建 `magel/magel.exe`，然后从仓库根目录运行：

```powershell
python tests/magel_desktop.py
# 编译器位于其他位置时：
python tests/magel_desktop.py --magel C:/path/to/magel.exe
```

脚本使用 WinAPI 构建兼容共享库，验证控件、UTF-8、按钮、事件回调、定时器及海龟绘图。
产物分别位于 `build/magel-gui-winapi` 和 `build/magel-turtle-winapi`，无需 Qt。
Windows 胶水默认加载工作目录的 `mcgui.dll`；设置 `MAGEL_GUI_LIBRARY` 可指定 Qt DLL
或其他兼容库的路径。其他平台默认使用 `libmcguiqt.so`。`startEventTimer` 返回 `void`。
`waitEvent` 只等待有限时间片，超时返回 0，让 VM 继续交付中断。

## 运行示例（Linux）

需要 Qt5 Widgets 开发包、`g++`，以及已构建的 `magel/magel`；`MAGEL_DEMO` 可覆盖编译器路径。
无头验证直接从仓库根目录运行：

```sh
python magel/example/gui/test_qt.py            # 用 QT_QPA_PLATFORM=offscreen
```

脚本会构建 `libmcguiqt.so`、编译两个示例、跑 `counter close` 与 `qt_interrupt` 并比对输出。

有交互桌面时手动跑：

```sh
mkdir -p build/magel-gui-qt
sh magel/example/gui/qt/build.sh build/magel-gui-qt
./magel/magel aot magel/example/gui/examples/counter.mgl -I magel/example/gui \
    --glue magel/example/gui/glue.cpp --out build/magel-gui-qt/counter
cd build/magel-gui-qt && ./counter          # 点击“加一”或“问候”，关窗退出
```

共享库要和 exe 在同一个工作目录（胶水用 `./libmcguiqt.so` 定位）。Windows 上把
`qt/build.sh` 的产物换成 `mcguiqt.dll` 即可，符号与布局约定完全一致。
[`winapi/gui.c`](winapi/gui.c) 是 Windows 原生后端；`glue.cpp` 按平台选择默认库，也可通过环境变量覆盖。

## Magel API

窗口与控件（与 WinAPI 版语义一致，尺寸单位是像素）：

| 调用 | 返回或行为 |
| --- | --- |
| `gui::open(width,height,title)` | 创建窗口；成功 1，失败 0。客户区尺寸 100–4096 像素。 |
| `gui::label/button/textBox(id,x,y,width,height,text)` | 添加控件；成功 1，失败 0。ID 在 1–65535 且不可重复。 |
| `gui::setText(id,text)` / `gui::getText(id)` | 设置或读取控件文本；`getText` 失败时返回空串。 |
| `gui::pollEvent()` | 处理当前待决事件；无事件 0，按钮点击返回其 ID，窗口关闭 -1。 |
| `gui::waitEvent()` | 泵一小段时间片；有事件返回事件，超时返回 0（让 VM 回去交付中断）。 |
| `gui::close()` | 主动关闭窗口。 |

事件回调（Qt 后端）：

| 调用 | 行为 |
| --- | --- |
| `gui::bindEvents(EventHandle)` | 把一个事件句柄（`std::event::Timer.handle` 或其他事件源）登记为 GUI / Qt 事件源；成功 1。 |
| `gui::unbindEvents()` | 取消登记。 |
| `gui::startEventTimer(intervalMillis)` | GUI 侧周期事件源；`0` = 每个泵回合一次，`>0` 用单调时钟限速。 |
| `gui::stopEventTimer()` | 停掉周期事件源。 |
| `gui::postEvent(event)` | 注入一个事件（测试 / 宿主脚本），与真实事件一样派发回调并入队。 |
| `gui::pump()` | 处理一轮 Qt 事件（非阻塞）。 |
| `gui::pumpEvery(instructions)` | 每 N 个执行边界自动 `pump()` 一次（VM 指令 / AOT 基本块）；传 0 取消。长计算循环里保持 GUI 响应与中断交付。 |

中断处理函数收到的事件值约定：按钮点击 = 控件 ID（>0），窗口关闭 = -1，Qt 周期事件 = 0。

## 事件回调示例

[`examples/qt_interrupt.mgl`](examples/qt_interrupt.mgl) 把三样东西串起来：

```c
import gui;
import std::event;

int64 clicks = 0; int64 ticks = 0; int64 closed = 0;
void onGuiEvent(int64 source) {
    if (source == -1) { closed = closed + 1; }
    if (source == 0) { ticks = ticks + 1; }
    if (source > 0) { clicks = clicks + 1; }
}

int64 main() {
    gui::open((int32)360, (int32)200, "Qt 事件回调");
    Timer events = Timer::everyNanos(0);        /* 只收 GUI 事件，不自动触发 */
    events.onInterrupt(onGuiEvent);
    gui::bindEvents(events.handle);
    gui::startEventTimer((int32)0);             /* 每个泵回合一次周期事件 */
    gui::pumpEvery((int64)64);                  /* 长循环里每 64 条指令泵一次 */
    int64 i = 0;
    while (i < 20000) { i = i + 1; }
    gui::pumpEvery((int64)0);
    gui::postEvent((int32)1);                   /* 注入点击 */
    gui::postEvent((int32)-1);                  /* 注入关窗 */
    gui::stopEventTimer();
    gui::unbindEvents();
    gui::close();
    return 0;
}
```

事件链是：GUI 事件 → `mcgui_callback_t` → `glue.cpp` 的桥 → 运行时中断队列 →
下一个执行边界交付 `onGuiEvent`。周期事件只在 `pump()` 里派发，而 `pumpEvery` 让指令
钩子定期调用 `pump()`，所以纯计算循环也能收到事件；钩子在中断交付期间不运行，避免套娃。

## ABI 与验证

共享库导出 `mcgui_open`、`mcgui_label`、`mcgui_button`、`mcgui_text_box`、
`mcgui_set_text`、`mcgui_get_text`、`mcgui_poll_event`、`mcgui_wait_event`、
`mcgui_close`，以及事件相关的 `mcgui_pump`、`mcgui_post_event`、`mcgui_start_timer`、
`mcgui_stop_timer`、`mcgui_set_callback`。`glue.cpp` 用 `DynamicLibrary::bind<R(Args...)>`
按精确 C 签名绑定到 `gui::` 声明；`const char *` 返回值由胶水立即复制进 Magel。
事件的原始符号用 `DynamicLibrary::resolve` 取，再包成带 `Context&` 的绑定，
通过 `interruptPoster()` 取得可跨宿主调用使用的投递器，通过 `setInstructionHook()` 安装泵钩子。投递器只在所属运行时的执行线程与运行期内有效，胶水不保存短命的 `Context&`；AOT 不要求提供 `Machine *`。

`python magel/example/gui/test_qt.py` 在 `QT_QPA_PLATFORM=offscreen` 下验证库构建、UTF-8
文本往返、事件注入、Qt 周期事件与每 N 条指令泵；`test_winapi.py` / `test_program.py`
验证当前 WinAPI 桌面后端（需要 Windows）。
