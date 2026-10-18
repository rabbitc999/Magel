# 5600X：C 内存池与 malloc/free 对比

CPU：AMD Ryzen 5 5600X 6-Core Processor；系统：Microsoft Windows 10 专业版 10.0.19045。
MSVC 19.44.35229.0 / GCC 11.2.0；MSVC Release /O2 /DNDEBUG; GCC Release -O3 -DNDEBUG; C++17。
每项预热一次，正式运行七次取中位数；计时只包含 Machine::run。默认池为 512 MiB；malloc/free 不限制总容量，峰值不可用。

两模式复用完全相同的字节码：`218B16BA8399CB8A2E58C68DA86F4442BC5465370E6EE1ECA6FF4A5A843E9CB6`。
三种构建在两种模式下均通过 288 项断言和 x86/x64 交叉加载。

| 构建 | 负载 | 池中位数 ms | malloc 中位数 ms | malloc 相对池 | 池峰值字节 |
| --- | --- | ---: | ---: | ---: | ---: |
| MSVC Win32 | mc | 1078.528 | 1093.031 | +1.34% | 6822340 |
| MSVC Win32 | vm | 819.259 | 819.310 | +0.01% | 11336 |
| MSVC x64 | mc | 1043.886 | 1052.332 | +0.81% | 8763104 |
| MSVC x64 | vm | 740.207 | 755.190 | +2.02% | 16552 |
| GCC x64 | mc | 1009.627 | 1010.012 | +0.04% | 7447072 |
| GCC x64 | vm | 815.957 | 830.140 | +1.74% | 16288 |

正百分比表示 malloc 较慢。池峰值包含加载、预热及运行期间 VM 进程通过适配层分配的内存。
这两个整数循环负载只提供当前机器的对照，微小差距可能来自测量波动；不代表密集分配、ABI 或其他程序。

复现：
```powershell
./magel/benchmarks/compare-vm.ps1 -ExpectedCpu '5600X' -OutputDirectory tmp/vm-pool
./magel/benchmarks/compare-vm.ps1 -ExpectedCpu '5600X' -UseMalloc -ProgramPrefix tmp/vm-pool/magel-loop -OutputDirectory tmp/vm-malloc
```
