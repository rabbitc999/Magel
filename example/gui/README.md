# Magel 简易 GUI（WinAPI / Qt / C ABI）

[`gui.mgl`](gui.mgl) 是 Magel 可 `import gui;` 的公共声明；[`qt/gui_qt.cpp`](qt/gui_qt.cpp)
编成 `libmcguiqt.so`（Windows 上是 `mcguiqt.dll`），用 Qt Widgets 创建窗口与控件；
[`glue.cpp`](glue.cpp) 把 Magel 符号绑到共享库的 C 函数。Magel 与库之间只传固定宽度整数与
UTF-8 字符串，不传递窗口指针或控件结构。

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
`waitEvent` 只等待有限时间片，超时返回 0，让 VM 继续显式接收事件。

## 运行示例（Linux）

需要 Qt5 Widgets 开发包、`g++`，以及已构建的 `magel/magel`；`MAGEL_DEMO` 可覆盖编译器路径。
无头验证直接从仓库根目录运行：

```sh
python magel/example/gui/test_qt.py            # 用 QT_QPA_PLATFORM=offscreen
```

脚本会构建 `libmcguiqt.so`、编译两个示例、跑 `counter close` 与 `qt_events` 并比对输出。

有交互桌面时手动跑：

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
| `gui::waitEvent()` | 泵一小段时间片；有事件返回事件，超时返回 0（返回程序显式处理事件）。 |
| `gui::close()` | 主动关闭窗口。 |

事件回调（Qt 后端）：

显式接收的 source 值约定：按钮点击 = 控件 ID（>0），窗口关闭 = -1，Qt 周期事件 = 0。

## 显式事件示例

[examples/qt_events.mgl](examples/qt_events.mgl) 登记一个事件标签，通过 std::event::tryReceive() 显式处理按钮、周期和关窗事件。GUI 泵只收集宿主事件，不调用 Magel 用户函数。队列容量固定为 256，gui::droppedEvents() 报告满队列或运行结束后的投递失败；需要保留逐条业务数据时应在宿主侧施加背压。

## ABI 与验证

`python magel/example/gui/test_qt.py` 在 `QT_QPA_PLATFORM=offscreen` 下验证库构建、UTF-8
文本往返、事件注入、Qt 周期事件与每 N 条指令泵；`test_winapi.py` / `test_program.py`
验证当前 WinAPI 桌面后端（需要 Windows）。
