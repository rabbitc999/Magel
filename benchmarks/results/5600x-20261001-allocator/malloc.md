# Magel StackVM 位数与编译器性能对比

CPU：AMD Ryzen 5 5600X 6-Core Processor；系统：Microsoft Windows 10 专业版 10.0.19045。
编译器：MSVC 19.44.35229.0、GCC 11.2.0；每项预热一次，正式运行 7 次取中位数。
MSVC 使用 Release /O2 /DNDEBUG；GCC 使用 Release -O3 -DNDEBUG。
分配器：C malloc/free；峰值内存不可用。
计时只包含 Machine::run，不含 Magel 编译、加载或进程启动。

| 构建 | 负载 | 指令数 | 中位数（ms） | MIPS | 校验 |
| --- | --- | ---: | ---: | ---: | --- |
| MSVC Win32 | Magel 源码 loop.mgl | 20000035 | 1,093.031 | 18.298 | asserted |
| MSVC Win32 | VM 整数循环 | 18000007 | 819.310 | 21.970 | 499999500000 |
| MSVC x64 | Magel 源码 loop.mgl | 20000035 | 1,052.332 | 19.005 | asserted |
| MSVC x64 | VM 整数循环 | 18000007 | 755.190 | 23.835 | 499999500000 |
| GCC x64 | Magel 源码 loop.mgl | 20000035 | 1,010.012 | 19.802 | asserted |
| GCC x64 | VM 整数循环 | 18000007 | 830.140 | 21.683 | 499999500000 |

三种构建均通过 288 项断言；x86 / x64 生成的堆示例 .svmb SHA-256 相同：9A9341030CD901F9F0CC0E845078BE5B9AAF4EDF957D8B12CE02A04AEEB5E80F。
x86 / x64 交叉运行均输出 42 7 0；同一份 Magel 字节码在三种构建下通过断言。
32 位 Windows 必须运行 Win32 VM；C ABI 动态库须与 VM 进程位数相同。
结果只对应当前机器、构建选项和工作负载，不代表其他 CPU 的性能。
