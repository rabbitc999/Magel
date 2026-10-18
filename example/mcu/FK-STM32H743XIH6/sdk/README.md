# 内置 FK-STM32H743XIH6 板级库

本目录从用户提供的 `STM32_LUA_CAR/STM32H743XIH6` 工作区导入，供父目录的 Magel 板端工程构建。
它包含本板所用的原库源码和必要构建依赖，不需要在电脑上保留原工程路径。

- `Board/`：系统和板级驱动，包括 SDRAM、LCD、按键、USART、QSPI 等。
- `Libraries/`：CMSIS 核心与 H743 设备头、H743 启动和系统文件、STM32H7 HAL/LL、USB MSC。
- `Middleware/`：FatFS、Allocator、Command、Expression、GUI 及 MDK 故障处理辅助源码。
- `User/`：原构建清单需要的配置、日志、存储及其他辅助源码；原 `User/main.c` 不参与 Magel 固件。
- `Project/CMake/`：工具链、源文件清单、链接脚本、新库系统调用与异常入口。

未导入原工程的构建产物、Keil 工程、其他 H7 芯片设备头和无关 FreeRTOS/遥控器模块。
Magel 启动入口仍是父目录 `main.c`，不调用原小车任务启动流程。
Magel VM 核心继续复用仓库 `library/stackvm/`，本目录没有 VM 源码副本。

导入时对 C/C++ 应用仓库根 `.clang-format`，将 GB18030/GBK 中文源码转为 UTF-8，
整理文档注释为多行块，并给导入的 C++ 函数及其公开声明补充中文说明；供应商版权与许可声明保留。
原 SDK 构建文件仅调整默认运行库路径，板端构建与行为配置由父目录维护。
`Project/CMake/newlib_syscalls.c` 增加 `FK_VM_DEBUG_SYNC_IO` 条件：独立 VM 固件的 `_write`
使用 USART1 同步发送，保证未运行原工程后台串口任务时也能输出诊断；其他配置仍沿用原发送路径。
原工作区包含未提交修改，快照包含这些文件当时的内容。
具体来源提交、原文件 SHA-256 和本地整理后 SHA-256 见 [`../sdk-snapshot.json`](../sdk-snapshot.json)。
本地哈希以 LF 行尾计算，避免 Git 在 Windows 转换行尾造成误报。

原项目的许可文本保存在 [`LICENSE`](LICENSE)；供应商组件分别保留原有许可文本：

- [`CMSIS Device / ST`](Libraries/CMSIS/Device/ST/STM32H7xx/LICENSE.txt)
- [`STM32H7 HAL`](Libraries/STM32H7xx_HAL_Driver/LICENSE.txt)
- [`STM32 USB Device`](Libraries/STM32_USB_Device_Library/LICENSE.txt)
- [`FatFS`](Middleware/FatFs/LICENSE.txt)

本地 H7-TOOL 工具同样保留原项目许可，放在父目录 `tools/h7tool/`。
Arm 编译器及 C/C++ 标准运行库是另外安装的工具链依赖，下载到父目录 `toolchain/`，不作为源码库提交。
构建入口与完整命令见 [`../README.md`](../README.md)。
