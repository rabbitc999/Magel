# FK-STM32H743XIH6 板端 Magel VM

本目录已内置从原 `STM32_LUA_CAR/STM32H743XIH6` 拷入的 HAL/CMSIS、BSP、FatFS、
LCD、按键及其构建依赖，提供独立启动入口，构建与烧录不再需要原工程目录。
共享 VM 仍直接编译 Magel 仓库的 `library/stackvm/stackvm.cpp`，没有 VM 源码副本。

本地依赖目录如下：

| 路径 | 内容 |
| --- | --- |
| [`sdk/Board/`](sdk/Board/) | 原板级系统、SDRAM、LCD、按键、串口及其他 BSP 驱动 |
| [`sdk/Libraries/`](sdk/Libraries/) | CMSIS、STM32H7 HAL/LL、USB MSC 库及 H743 启动文件 |
| [`sdk/Middleware/`](sdk/Middleware/) | FatFS、内存池、命令、GUI、表达式和故障处理依赖 |
| [`sdk/User/`](sdk/User/) | 原构建清单所需的配置头、日志、存储等应用辅助源码 |
| [`sdk/Project/CMake/`](sdk/Project/CMake/) | Clang 工具链、链接脚本、新库系统调用和原源文件清单 |
| [`tools/h7tool/`](tools/h7tool/) | 内附的 Flash 备份、烧录及 H7-TOOL 兼容脚本 |
| `toolchain/` | 本地安装的 Arm C/C++ 运行库及下载缓存，不入库 |

导入来源、许可证与整理范围见 [`sdk/README.md`](sdk/README.md)，每个导入文件的源哈希及
本地哈希见 [`sdk-snapshot.json`](sdk-snapshot.json)。原工程保持原样；后续改板端库请修改本目录副本。

## 界面与程序生命周期

默认上电先自动运行一次内置 benchmark，完成测速与按键演示后返回 LCD 程序选择页：

benchmark 测速完成后显示 `MODE/ENTER to exit`，此时 **MODE 或 ENTER** 可立即返回菜单，
实体按下连续消抖 30 ms 即可识别，不必等到松开；单击、长按、双击和重复事件也可退出。
UP/DOWN 继续用于按键演示，未操作时约 30 秒自动返回。返回后会清理全部事件并等待
实体按键释放 30 ms，防止同一次退出操作再次启动程序。整数计时内核内不轮询按键或串口。

- **UP / DOWN**：选择程序，列表自动分页并循环。
- **ENTER**：运行当前程序。
- **MODE**：重新扫描 `0:/magel/`。
- 程序正常退出、`panic`、VM 指令预算超限或映像加载失败：记录 USART1 诊断，
  析构本轮程序、数据区、虚拟机与注册表，自动返回选择页。最近状态显示在屏幕底部。

始终提供三个内置入口：整数测速、正常退出示例、故意 `panic` 的示例。没有存储介质时也能验证
启动与回到菜单。FatFS 目录中的 `.svmb` 文件追加到列表，最多显示 32 项（含内置项）。
当前文件名和 LCD 文本支持可打印 ASCII；长文本截断到屏幕宽度。

这是一份独立 VM 固件：不初始化电机、舵机和小车控制任务。TIM6 保留毫秒时基与按键消抖，
TIM7 仅确认中断。程序运行期间菜单暂停处理，程序应自行调用按键胶水；默认每轮限制
10,000,000 条 VM 指令。硬件 Fault、原生 C/C++ 越界及无法恢复的分配器耗尽不等同于
语言 `panic`，不能承诺这些情况也能返回菜单。

## 内存与执行配置

从参考 BSP 的 `Memory_Malloc(MP_RAM_EX, ...)` 申请三块独立 SDRAM 内存：

| 用途 | 容量 |
| --- | ---: |
| 标准 C++ 元数据分配 | 2 MiB |
| VM 的 `Memory::` 容器、解码和执行状态 | 16 MiB |
| Magel 程序字节堆 | 4 MiB |

总预算 **22 MiB**。分配由现有 SDRAM 堆管理器完成，避开 LCD 保留区及分配表，
不直接覆盖固定地址。此预算依据参考工程的 24 MiB 外部堆配置与 FMC 的 32 MiB 几何布局；
不能直接用于只有 16 MiB SDRAM 的硬件。若修改硬件容量，需要同时校正 BSP 内存布局、
链接脚本和本文件对应的三项预算。

SDRAM 初始化前的少量 C++ 静态构造使用 newlib 启动堆；通过
`MCU_METADATA_BOOTSTRAP_MALLOC=1` 明确启用，并能在切换到 SDRAM 后正确释放早期对象。
大容量回退数组设为 64 字节，避免把 MiB 级数组放进片内 `.bss`。测试快照按 Cache Line
对齐并写回 DCache，供 SWD 正确采集。

板端关闭异常、RTTI、操作系统线程、AOT、宿主进程和大数，开启 FatFS 后端。
VM 的寄存器执行、准备与融合等优化沿用核心最高默认配置；宿主编译器生成完整优化后的 IR。
独立 MCU 构建关闭反射，因此生成程序时必须显式传入 `--no-reflection`。

## 构建

需要 CMake、Clang/LLVM、Make，以及 Cortex-M7 硬浮点的 Arm GNU C/C++ 头文件和运行库。
本目录的下载脚本从 Arm 官方 ZIP 提取 Cortex-M7 硬浮点 C/C++ 头文件、`libstdc++`、
`libsupc++`、newlib 和 libgcc，并由 ZIP CRC 校验提取结果。编译器和标准运行库属于本机工具链，
不作为板级源库提交；默认安装到本示例的 `toolchain/arm-runtime/`。

从 Magel 仓库根目录执行：

```powershell
python example/mcu/FK-STM32H743XIH6/tools/fetch_cpp_runtime.py `
  --cache example/mcu/FK-STM32H743XIH6/toolchain/cache `
  --destination example/mcu/FK-STM32H743XIH6/toolchain/arm-runtime

cmake -S example/mcu/FK-STM32H743XIH6 -B example/mcu/FK-STM32H743XIH6/build-local `
  -G "MinGW Makefiles" `
  -DCMAKE_TOOLCHAIN_FILE="$PWD/example/mcu/FK-STM32H743XIH6/sdk/Project/CMake/arm-clang.cmake" `
  -DCMAKE_MAKE_PROGRAM="$(Get-Command make.exe | Select-Object -ExpandProperty Source)"

cmake --build example/mcu/FK-STM32H743XIH6/build-local -j 4
```

`FK_BOARD_ROOT` 默认使用本目录的 `sdk/`，`ARM_RUNTIME_ROOT` 默认使用本目录的
`toolchain/arm-runtime/`，无需再指定参考工程路径；已有外部工具链也可显式覆盖后者。
如果旧的 `build-arm/` 缓存还保存着外部 SDK 路径，请使用新的 `build-local/` 目录。
默认宿主编译器为根目录 `magel.exe`，可用 `-DMAGEL_COMPILER=...` 替换。
输出 ELF 为 `build-local/board-sdk/magel-fk-h743.elf`；复用参考工程的后处理命令，
HEX/BIN 名称仍为 `build-local/board-sdk/stm32_lua_car.{hex,bin}`。

## FatFS 部署

参考驱动的 `0:` 是外部 W25QXX 上的 FatFS 卷，并非自动识别任意 SD 卡。
本示例只挂载、不格式化。其他介质需沿用 BSP 的 `diskio` 驱动添加卷映射。

构建过程在 `build-local/assets/` 生成部署文件。将每个程序对应的三份文件放到同一目录：

```text
0:/magel/hello.svmb
0:/magel/hello.data.bin
0:/magel/hello.layout
0:/magel/panic.svmb
0:/magel/panic.data.bin
0:/magel/panic.layout
```

自定义程序可以这样生成，文件名前缀需要保持一致：

```powershell
magel svmb app.mgl --no-reflection --out app
```

缺少配套文件或映像损坏时显示失败并返回菜单，不替换为另一份程序继续执行。
单个部署文件限制为 1 MiB。FatFS 文件接口支持读、写、定位、同步与追加；
追加模式在每次写入前定位到文件尾。

## LCD、按键及计时胶水

在 Magel 程序中包含 [`programs/board.mgl`](programs/board.mgl)：

```magel
%include "board.mgl"
int64 main() {
    lcdClear(0);
    lcdFill(8, 40, 100, 20, 2016);
    lcdText(8, 8, "Hello Magel", 65535, 0);
    println("size={}x{}", lcdWidth(), lcdHeight());
    for (int64 i = 0; i < 500; i += 1) {
        if (keyClicked(1)) { break; }
        boardDelay(10);
    }
    return 0;
}
```

| 接口 | 含义 |
| --- | --- |
| `lcdClear(color)` | RGB565 清屏 |
| `lcdFill(x, y, width, height, color)` | 检查边界后填充矩形 |
| `lcdText(x, y, text, foreground, background)` | 16 像素字体的单行 ASCII 文本 |
| `lcdWidth()` / `lcdHeight()` | 实际屏幕尺寸 |
| `keyPressed(key)` | 当前物理按下状态 |
| `keyClicked(key)` | 消费消抖后的单击事件 |
| `boardExitRequested()` | 消费 MODE/ENTER 退出事件或检测实体按下，供程序协作退出 |
| `boardDelay(ms)` | 0～1000 ms 延时，中断继续运行 |
| `boardCycles()` | DWT 32 位周期计数 |
| `boardMillis()` | TIM6 毫秒时基 |
| `boardClockHz()` | 实际内核时钟 |

键号为 **0=MODE、1=ENTER、2=UP、3=DOWN**。颜色范围为 0～65535。
非法坐标、颜色、键号或不可显示文本会报告 VM 错误，并按正常错误路径返回菜单。
LCD 依赖单独的 C 桥接层，避免参考 BSP 中的 C 头文件影响 C++ 标准库的语言链接。
USART1 使用参考 BSP 的 **PA9/PA10**；输出使用同步发送路径，输入从 COM1 FIFO 非阻塞读取。

USART1 为 **115200、8N1**，兼容原工程的文本虚拟按键命令，以换行结束：

```text
key mode
key enter long
key mode double
key enter repeat
key up
key down
```

可选事件为 `click`（默认）、`long`、`double`、`repeat`；成功回显 `KEY_OK <按钮> <事件>`，
无效或超长命令回显 `KEY_ERROR`，不执行截断命令。这些命令调用与实体按键相同的 BSP 事件入口。
这里只启用按键操作。其他串口内容保留给 VM 标准输入，不再先由原命令回调吞掉；输入队列容量为
1024 字节，满后丢弃新增字节，以 `key ` 开头的行保留给调试命令。命令允许分段到达，
在菜单、`boardDelay` 和 `boardExitRequested` 中轮询；不在性能计时内核中处理。

## 实机测速与恢复

[`programs/benchmark.mgl`](programs/benchmark.mgl) 和 `nativeIntegerLoop` 执行相同的
31 位整数递推：每轮 10,000 次，先预热，再记录五轮周期数并核对校验值。
计时区间包含函数调用和原生胶水边界，不包含串口输出、LCD 刷新或字节码加载。
这是特定整数内核的 VM/原生对照，不代表全部应用性能；单段计时须小于一次 DWT 回绕周期
（480 MHz 下约 8.95 秒）。

普通构建默认 `FK_AUTORUN_BENCHMARK=ON`、`FK_BOARD_SELFTEST=OFF`：上电运行一次测速，
随后进入最多约 30 秒的按键演示，MODE/ENTER 可提前返回程序选择页；正常运行时不会自动执行故意 panic 示例。
如需直接进入选择页，显式设置 `-DFK_AUTORUN_BENCHMARK=OFF`。
自动实机回归时添加 `-DFK_BOARD_SELFTEST=ON`，依次执行测速、故意 panic、正常退出，
并在每次返回后绘制选择页。

安装 `pyocd`、`pyserial`、`pyelftools` 后，执行：

```powershell
python example/mcu/FK-STM32H743XIH6/tools/capture_benchmark.py `
  --elf example/mcu/FK-STM32H743XIH6/build-local/board-sdk/magel-fk-h743.elf `
  --image example/mcu/FK-STM32H743XIH6/build-local/board-sdk/stm32_lua_car.bin `
  --flash `
  --out example/mcu/FK-STM32H743XIH6/results/benchmark.json
```

`--board-tools` 默认指向本示例内的 `tools/h7tool/`，无需外部脚本。
`--flash` 会先保存完整 2 MiB Flash 备份及 SHA-256，再烧录并逐字节回读验证。
备份位于本示例 `build-local/hardware-backups/sequential_*/`。不加 `--flash` 时只采集正在运行的
对应固件；ELF 必须匹配。多探针用 `--uid` 指定。工具以 AP2 核对芯片 ID、启用易失调试域，
并给 pyOCD 早期连接使用 AP2 的 DBGMCU 镜像；不修改系统安装包，不跳过芯片型号和容量检查。
工具禁用此探针不兼容的 `TransferBlock`；默认烧录器使用普通 `DAP_Transfer` 批次传输，
先核对上传到 SRAM 的算法，再逐扇区擦除、逐个 1 KiB 页编程并回读，连接失败时有限重试。
扇区尾部的映像外数据从备份保留，完整映像核对一致后才启动。本地 SDK 固件已使用该路径完成烧录和验证。
历史烧录曾出现空白编程页；若实际差异仅为全空编程页，也可使用严格补写工具：它先保存当前完整 Flash，
逐页核对差异，只补写完整的全 `0xFF` 页，不执行擦除；非空错误页直接拒绝。

```powershell
python example/mcu/FK-STM32H743XIH6/tools/repair_erased_pages.py `
  --image example/mcu/FK-STM32H743XIH6/build-local/board-sdk/stm32_lua_car.bin `
  --backup-dir example/mcu/FK-STM32H743XIH6/build-local/hardware-backups
```

补写后会整映像回读，完全一致才复位运行；之后运行上面的采集命令，省略 `--flash`。

采样通过 SWD 读取已写回 RAM 的快照，不要求 USART1 已接到电脑，也不在计时期间暂停 CPU。
仅为启用 `FK_BOARD_SELFTEST=ON` 的自测固件添加 `--wait-ui`：它额外核对选择页返回记录的程序顺序和状态
`0、5、0`，分别对应正常退出、panic 和再次正常退出。
测量原始数据、实际频率、固件哈希和选择页记录保存在输出 JSON 中。

恢复原固件时，把首次备份作为 `--image` 传给 `tools/flash_board.py`，仍会先备份当前 Flash 并回读验证。
下面是内置库之前那次实机工作的历史备份位置，不是当前构建或烧录的依赖路径。
本次开发板首次原固件备份为参考工程中的
`Tests/h7tool/results/auto_flash_20261015_153451_625239/original_flash.bin`（2 MiB），
SHA-256 为 `474d263941f2505aca016f8952c03cf4cc8cd5e7c61562e8673ffe0e65ef7dc7`。
后续烧录产生的备份可能已经是 Magel 固件，应按首次备份文件与哈希识别原固件。

## 实机结果（2026-10-15）

以下为内置板级库之前已完成的两次实机测速记录，对应固件哈希保持原值。
本地 SDK 固件随后已完成交叉编译、烧录和完整回读验证；下表没有用新固件重新采样。

STM32H743XIH6 / Cortex-M7，实际时钟 **480 MHz**。固件由 Clang 23.1.2 以 `-Os`
编译，使用 Arm GNU 14.3.Rel1 的硬浮点 C/C++ 运行库；VM 使用共享核心的默认最高执行优化，
元数据、运行时和程序字节池分别从 SDRAM 申请 2 / 16 / 4 MiB。
Flash 映像为 **1,649,516 字节（约 1.57 MiB）**，片内 AXI `.data` + `.bss` 为
**320,560 字节（约 313.05 KiB）**，另有 DTCM 启动堆与栈；SDRAM 保留段不计作片内 RAM。

整份固件已回读一致，SHA-256：
`b61f5feaf5bf371c657e39633ee561528802a143aa4afefca96b94aa95bd9433`。

每轮执行 10,000 次 31 位整数递推，所有样本的校验值均为 **1254053515**。
第二次复测先复位并断开调试会话，等待自测完成，再连接读取结果。两次都保留全部五轮样本：

| 轮次 | 首次 VM / ms | 首次原生胶水 / ms | 隔离复测 VM / ms | 隔离复测原生胶水 / ms |
| --- | ---: | ---: | ---: | ---: |
| 1 | 18.650708 | 10.923108 | 18.647987 | 11.031012 |
| 2 | 8.159879 | 0.410529 | 8.164300 | 0.410108 |
| 3 | 8.144942 | 0.404712 | 8.143512 | 0.528200 |
| 4 | 8.146217 | 0.422433 | 8.136862 | 0.409892 |
| 5 | 8.143917 | 0.403692 | 8.134267 | 0.560992 |
| 中位数 | 8.146217 | 0.410529 | 8.143512 | 0.528200 |

VM 每次递推约 **391 个周期**，中位耗时约 **8.14 ms / 10,000 次**，对应约
**123 万次递推/秒**。以两次各自的中位数相除，VM/原生胶水耗时比分别为
**19.84** 和 **15.42**。这只是此整数内核与当前宿主路径的比较。

第一轮两条路径均出现约 10 ms 的额外耗时；隔离 SWD 连接后仍存在，因此不能把它归因于采集连接。
目前没有进一步隔离这部分开销，不删除首轮，也不把首轮当作稳定循环吞吐。
中断、按键时基和 Cache 均保持实际运行配置，原生短区间的相对波动更明显。

性能差异的实现原因是：原生循环执行直接的机器指令，而 VM 还要处理指令分派、运行时值、
操作数和控制流；SDRAM 中的执行状态与片内 Cache 也参与访存。当前测量没有分别关闭这些因素，
不能为每一项分配耗时百分比。C++ 对照通过 `NativeInteger` 原生胶水进入，
其循环条件读取 `args[0].asInt()`，计时包含参数访问、原生边界与周围的 VM 调度；
**这不是独立裸 C++ 循环的性能上限，也不是 AOT 对照**。

Magel 测试内核如下，完整调用、预热、计时和校验见
[`programs/benchmark.mgl`](programs/benchmark.mgl)，原生对照见
[`magel_board.cpp`](magel_board.cpp) 的 `NativeInteger`：

```mgl
int64 integerLoop(int64 count, int64 seed) {
    int64 value = seed;
    for (int64 i = 0; i < count; i += 1) {
        value = (value * 1664525 + 1013904223) & 2147483647;
    }
    return value;
}
```

两次实机自测都记录了程序顺序 **测速 → 故意 panic → 正常退出**，状态为 **0 → 5 → 0**，
并在每次返回后调用选择页绘制。上述历史记录来自 `FK_BOARD_SELFTEST=ON` 自测固件。
后来烧录的本地 SDK 固件采用普通构建默认值，仅自动运行 benchmark，再返回选择页，不执行故意 panic。
该记录验证了实机程序与页面返回路径；尚未通过人工逐个按键确认屏幕显示及物理按键事件，
也未把外部 FatFS 程序装载成功作为已完成的实机测试。

原始数据：[`首次采样`](results/benchmark.json)、
[`隔离复测`](results/benchmark-isolated.json)。

本地 SDK 固件还通过 USART1 做过两项烟雾检查：`key up` 收到 `KEY_OK up click` 和按键演示输出，
`key mode invalid` 收到 `KEY_ERROR`，见 [`串口检查记录`](results/uart-key-smoke.json)。
MODE/ENTER 的八种虚拟事件组合已由 `tests/mcu_fk_controls.py` 在主机中验证；
实机自动组合检查未完成，按用户要求停止后没有继续访问开发板。
