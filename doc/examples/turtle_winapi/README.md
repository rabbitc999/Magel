# WinAPI 海龟绘图：Magel + C ABI

这是 Windows 桌面示例。Magel 的 [`demo.mgl`](demo.mgl) 用宿主函数声明编排海龟路径；[`turtle.c`](turtle.c) 编成 `turtle.dll`，使用 WinAPI 创建窗口和 GDI 画布；[`glue.cpp`](glue.cpp) 由 `magel aot --glue` 嵌入独立程序，逐一从 DLL 绑定 C ABI 符号。VM 不接触 `HWND`、`HDC` 或画布内存。

## 构建与运行

在仓库根目录的 PowerShell 中，先构建 `magel/magel.exe` 并准备 MinGW，再运行；编译器路径可由测试的 `--magel FILE` 指定：

```powershell
python tests/magel_desktop.py --turtle-only
Push-Location build/magel-turtle-winapi
./demo.exe         # 绘制 Koch 雪花；关闭窗口后程序退出
Pop-Location
```

程序在当前目录保存 `turtle.bmp`。自动验收时运行 `./demo.exe close`，绘制并保存后直接关闭窗口。[`tests/magel_desktop.py`](../../../tests/magel_desktop.py)完成 DLL、Magel 四元式和独立 C++ exe 的构建；DLL 必须与 exe 放在同一运行目录。

## 边界和绘图语义

| Magel 声明 | DLL 的 C ABI | 行为 |
| --- | --- | --- |
| `int32 turtle_open(int32,int32)` | `int32_t turtle_open(int32_t,int32_t)` | 创建窗口；成功返回 1，失败返回 0；画布尺寸 64–4096。 |
| `void turtle_forward(float64)` | `void turtle_forward(double)` | 前进指定距离；抬笔时只移动。 |
| `void turtle_left(float64)` | `void turtle_left(double)` | 正角度逆时针，负角度顺时针；初始朝右。 |
| `void turtle_pen(int32)` | `void turtle_pen(int32_t)` | 0 抬笔，非 0 落笔。 |
| `void turtle_color(uint32)` | `void turtle_color(uint32_t)` | `0xRRGGBB` 颜色。 |
| `void turtle_clear()` | `void turtle_clear(void)` | 清白画布，回中心并朝右。 |
| `int32 turtle_save_bmp(String)` | `int32_t turtle_save_bmp(const char *)` | UTF-8 路径；写 32 位 BMP，成功返回 1。 |
| `void turtle_wait/close()` | 同名 `void(void)` | 等待用户关窗，或主动关窗并等待线程退出。 |

所有画布状态都归窗口线程。Magel 调用经 `SendMessageW` 同步交给窗口过程，依次更新 GDI 的 DIB 画布；`WM_PAINT` 将画布复制到窗口，重绘时图形仍在。`turtle_save_bmp` 在同一窗口线程把像素写入文件。这个示例一次只开一个窗口，并要求有可用的 Windows 交互桌面。

胶水只负责动态加载和类型绑定；若 Magel 声明、C 函数签名或调用约定不一致，属于 C ABI 边界错误。这里所有导出函数使用默认 C 调用约定；实际桌面环境以 64 位构建验证。背景知识见[宿主集成与 C ABI](../../08-host-integration.md)，WinAPI 的线程和消息语义见 [Microsoft 的窗口线程说明](https://learn.microsoft.com/en-us/windows/win32/procthread/creating-windows-in-threads)与 [`SendMessage` 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessage)。
