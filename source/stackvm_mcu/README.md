# 独立 MCU VM 宿主

桌面和板端只编译一份 [`library/stackvm/stackvm.cpp`](../../library/stackvm/stackvm.cpp)。
本目录提供宿主、静态池、文件系统和平台适配，不保留 VM 源码副本。
语言语义与公共运行时能力见[当前能力](../../doc/17-capabilities.md)，实际板级配置见
[FK-STM32H743XIH6](../../example/mcu/FK-STM32H743XIH6/README.md)。

## 构建配置与边界

[`Makefile`](Makefile) 和 [`CMakeLists.txt`](CMakeLists.txt) 构建可在桌面运行的 mcu-host，
用于验证板端编译配置和字节码等价性；这一步不访问硬件。

| 配置 | 本目录默认值与含义 |
| --- | --- |
| 编译器 | C++17，禁用 exceptions、RTTI 和线程安全静态初始化 |
| STACKVM_EMBEDDED | 1，使用平台控制台与板端宿主路径 |
| STACKVM_WITH_THREADS / STACKVM_WITH_AOT | 0，不编 OS 线程与 AOT 桥 |
| STACKVM_WITH_HOST_IO | 0，不使用桌面文件装载；宿主先读映像，再从内存解码 |
| STACKVM_WITH_HOST_EXCEPTION_GUARD | 0，宿主回调不得抛 C++ 异常 |
| STACKVM_WITH_DEFAULT_HEAP | 0，由宿主注入字节池 |
| STACKVM_WITH_BIGINT / STACKVM_WITH_BIGFLOAT | 0，默认不编大数 |
| 反射 | 默认关闭；编译输入程序显式使用 `--no-reflection` |
| 执行优化 | 沿用共享核心的默认最高优化；不因 MCU 配置自动降低 IR/VM 优化 |

整数、浮点、字符串、容器、结构体、枚举、引用、闭包、泛型、接口与协作式协程按共享核心及
实际胶水运行。关闭 OS 线程不等于关闭所有协程。标准库能否使用还取决于导入门禁、宿主注册
和内存预算；不承诺桌面全部模块在当前默认宿主中可用。
NativeCallContext 已有字符串/容器读写及同步函数调用接口；本目录不再维护旧版“缺七种原生函数”的迁移清单。

## 主机验证

在仓库根目录执行：

```sh
make -C source/stackvm_mcu
./magel svmb example/samples/hello.mgl --no-reflection --out build/mcu-hello
./build/stackvm_mcu/mcu-host build/mcu-hello.svmb build/mcu-hello.data.bin build/mcu-hello.layout
python tests/mcu_vm.py --magel magel.exe
```

Windows 使用 mcu-host.exe；其他平台的 `--magel` 参数使用本机可执行文件路径。
回归脚本会按其定义的测试预算重建宿主、检查异常符号，再运行选定样例、文件系统、移动、
输出和软重启用例。测试预算与默认 Makefile 预算、真实板级预算分别描述，不互相代用。
最近一次交付验证为 42 个选定样例等价性及附加宿主检查，未据此宣称全量桌面库已上板。

## 三类内存预算

| 类别 | 内容 | 配置入口 |
| --- | --- | --- |
| 元数据池 | 标准 C++ 分配、解码辅助与宿主对象 | MCU_METADATA_BYTES / SetMetadata |
| 运行时 Arena | Memory::String、Vector、执行状态与运行时容器 | MCU_RUNTIME_ARENA_BYTES / SetRuntimeArena |
| 字节池 | 程序分配、字符串数据和字节存储 | MCU_VM_ARENA_BYTES / SetBytePool |

缺省数组大小以 Makefile/CMake 和实现中的宏为准；链接时的占位数组与运行期注入的外部池也要分开。
H743 工程实际从 SDRAM 分配 2/16/4 MiB，总计 22 MiB；它不代表小 SRAM MCU 的最小占用。
元数据、运行时峰值和字节堆使用量都应针对实际程序测量，历史解码倍数不作为当前固定比例。

外部池应按顺序初始化，所有返回值必须检查：

1. 初始化 SDRAM，提供对齐且不与 LCD/其他区域重叠的缓冲区。
2. SetMetadata 与 SetRuntimeArena 安装外部缓冲区。
3. InstallRuntimeArena 设置当前 Memory 分配器。
4. SetBytePool 构造字节池；必须位于运行时 Arena 安装之后，因为索引自身也分配 Memory 容器。
5. 创建 Machine、Program、DataArea 和注册表，再加载并执行程序。

`MCU_METADATA_BOOTSTRAP_MALLOC=1` 允许外部内存就绪前暂用启动期 C 堆；安装外部池后新分配
进入 SDRAM，早期对象仍由原分配器释放。默认关闭，是否启用由板级启动流程决定。
已使用的池不可任意切换。软重启须结束并释放整轮运行期对象后重置 Arena，不能跨轮保留引用。

## 文件、控制台与回调

MCU_VM_FILE_BACKEND 为 0（关闭）、1（C stdio 主机验证）或 2（FatFS，需要 ff.h 与板级实现）。
也可通过 mcuFilesystem::SetApi 注入 FileApi。此配置控制 VM 文件操作，不等同于前端
`[embedded]` 的固定模块禁用清单；后者会在导入时拒绝部分桌面模块。

控制台通过 mcuConsole 和输出 sink 接入 USART/OLED/RTT；默认宿主回调注册见 mcu_glue.cpp。
板端可用 NativeCallContext::invoke 同步回调 Magel 函数值或闭包，生命周期、参数数量和错误
传播遵循[宿主契约](../../doc/08-host-integration.md)。不将运行时地址转换成机器函数指针。

## 实机证据与故障边界

H743 的构建、Flash 备份/校验、历史性能数据、虚拟按键及程序选择页记录集中维护在
[板级 README](../../example/mcu/FK-STM32H743XIH6/README.md)。历史固件与测量哈希保持原值，
主机回归、交叉编译与实机验证分别记录。
语言 panic、正常退出和可处理的预算失败可由宿主回收本轮并返回选择页；硬件 Fault、宿主 C/C++
越界及不可恢复耗尽不保证沿这一路径恢复。无 GC 也不自动提供硬实时保证。
