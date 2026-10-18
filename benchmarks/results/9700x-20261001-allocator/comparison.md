# 9700X：C 内存池与 malloc/free 对比

CPU：AMD Ryzen 7 9700X 8-Core Processor；系统：Microsoft Windows 11 专业版 10.0.26200。
MSVC 19.44.35228.0 / GCC 11.2.0；MSVC Release /O2 /DNDEBUG; GCC Release -O3 -DNDEBUG; C++17。
每项预热一次，正式运行 7 次取中位数；计时只包含 Machine::run。默认池为 512 MiB；malloc/free 不限制总容量，峰值不可用。

两模式复用完全相同的字节码：`E5352EE783026C351475227A368876D519B6CE9C0FC6CDA3F82CA82D39D38FF3`。
三种构建在两种模式下均通过 288 项断言和 x86/x64 交叉加载。

| 构建 | 负载 | 池中位数 ms | malloc 中位数 ms | malloc 相对池 | 池峰值字节 |
| --- | --- | ---: | ---: | ---: | ---: |
| MSVC Win32 | mc | 764.631 | 767.107 | +0.32% | 6684676 |
| MSVC Win32 | vm | 551.672 | 573.979 | +4.04% | 11336 |
| MSVC x64 | mc | 766.573 | 751.315 | -1.99% | 8625352 |
| MSVC x64 | vm | 522.040 | 502.930 | -3.66% | 16552 |
| GCC x64 | mc | 675.480 | 700.643 | +3.73% | 7373680 |
| GCC x64 | vm | 530.367 | 531.687 | +0.25% | 16288 |

正百分比表示 malloc 较慢。池峰值包含加载、预热及运行期间 VM 进程通过适配层分配的内存。
这两个整数循环负载只提供当前机器的对照，微小差距可能来自测量波动；不代表密集分配、ABI 或其他程序。

复现：
```powershell
./magel/benchmarks/compare-vm.ps1 -ExpectedCpu '9700X' -OutputDirectory tmp/vm-9700x-pool
./magel/benchmarks/compare-vm.ps1 -ExpectedCpu '9700X' -UseMalloc -ProgramPrefix tmp/vm-9700x-pool/magel-loop -OutputDirectory tmp/vm-9700x-malloc
```
