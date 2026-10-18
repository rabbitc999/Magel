# VM 性能对比：VM、AOT、JVM、Python、Lua、C++

本页保留 **2026-10-15 同一台机器上的历史实测**，被测 Magel 版本为 **0.18.3 / 0.18.2**，比较五类负载。
这些数据不作为当前 VERSION 的重新实测；当前能力见[统一说明](17-capabilities.md)。
Magel 两个版本分别列出 VM 与 AOT，JVM 分成“不带即时编译”和“带即时编译”，共九个配置。
Magel 两版均使用默认最高 IR 与 VM 优化，并编入反射。

**报告范围**：两版 VM、两版 AOT、JVM 两种模式、Python、Lua 与原生 C++ 都在本轮重新测量。
AOT 是 Magel 源码经各自版本导出并编译后的原生程序，与解释执行的 VM、手写 C++ 分列。
下面给出实测、完整测试代码及实现证据，不从语言类型预设性能排序。

## 1. 测试环境与九个配置

| 项目 | 本轮配置 |
| --- | --- |
| 日期 | 2026-10-15，Asia/Shanghai |
| 系统 | Windows 10 10.0.19045，x64 |
| CPU / 内存 | AMD Ryzen 5 5600X，6 核 12 线程，32 GB RAM |
| Magel 0.18.3 | `v0.18.3` / `b5cfc9f2720c149bca7626bbaa5cfd52ad8f5576` |
| Magel 0.18.2 | `v0.18.2` / `320108bfda7c0b7035ed019a0010a87b43fe0e71` |
| 两版 Magel 编译器与 VM | GCC/MinGW 11.2.0，`-O2 -DNDEBUG`；默认最高 IR / VM 优化，反射开启；各自编译源码并运行各自字节码 |
| 两版 Magel AOT | 各自版本的编译器、内嵌运行时；`--opt-level=2 --unchecked --keep-intermediate`，GCC/MinGW 11.2.0；每进程 1 次预热 |
| JVM 不带即时编译 | Oracle Java 22.0.1+8-16，HotSpot 64-Bit Server VM，`java -Xint` |
| JVM 带即时编译 | 同一 JDK、同一 `.class`，`java -Xbatch`；保留 HotSpot 默认分层编译，测量前完整预热 20 次 |
| Python | CPython 3.12.3，64 位；默认解释器，未启用 JIT |
| Lua | 官方 Lua 5.5.1 源码，MinGW 官方构建目标，`-O2`，无 JIT |
| C++ | 原生 C++17，GCC/MinGW 11.2.0，`-O2 -DNDEBUG`；没有使用 `-ffast-math`、LTO 或手写闭式求和 |

这里的 C++ 是直接编译手写 C++ 测试程序，**不是 Magel AOT 导出**；两版 AOT 各有独立行。
AOT 使用默认的 unchecked 口径，关闭可选分配护栏和 panic 栈记录；显式下标与指针检查不因此全部删除。
本轮未测 AOT `--checked`，不把这组结果外推到 checked 配置。
JVM 两列共用 Java 源码和 class 文件，只改变执行模式与对应预热次数。

## 2. 五类负载、输入与校验值

所有上限 `n` 都来自运行时输入，循环内不打印。整数数据在各语言中均不溢出，
浮点用例统一使用双精度；本轮不拿 Magel 的 `float32` 与其他语言的双精度作比较。

| 用例 | n | 循环主体 | 期望结果 |
| --- | ---: | --- | ---: |
| 整数累加 | 10,000,000 | `total += i` | 49999995000000 |
| 奇偶分支 | 1,000,000 | 偶数加 `i`，奇数减 `i` | -500000 |
| float64 累加 | 2,000,000 | `total += 0.5` | 1000000 |
| 函数调用 | 1,000,000 | `total += mix(i)`，`mix(x) = x * 17` | 8499991500000 |
| 数组读取 | 1,000,000 | 从 `[1,2,3,4]` 循环取元素并累加 | 2500000 |

每次计时结果均与上表独立期望值核对。函数调用用例允许各语言正常内联和优化，
所以它衡量的是“这段带函数源码最终执行多快”，不是强制保留调用的纯调用开销测试。

## 3. 同轮实测结果

以下为**第二轮**各配置七次测量的中位数，单位 **ms，越小越快**。
JVM 两种模式独立列出，不合并为一个 JVM 数字。

| 配置 | 整数累加 | 奇偶分支 | float64 累加 | 函数调用 | 数组读取 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Magel 0.18.3 VM | 135.138 | 33.099 | 353.936 | 609.619 | 85.360 |
| Magel 0.18.2 VM | 134.522 | 32.659 | 491.668 | 633.749 | 84.069 |
| Magel 0.18.3 AOT | 2.170 | 0.439 | 1.300 | 0.227 | 36.900 |
| Magel 0.18.2 AOT | 2.170 | 0.440 | 1.300 | 0.439 | 33.500 |
| JVM 不带即时编译 `-Xint` | 145.578 | 22.043 | 25.071 | 26.915 | 16.734 |
| JVM 带即时编译 `-Xbatch` | 2.302 | 0.377 | 1.299 | 0.231 | 0.479 |
| Python | 400.934 | 61.285 | 71.810 | 72.427 | 51.250 |
| Lua | 57.000 | 11.000 | 11.000 | 16.000 | 11.000 |
| 原生 C++ | 2.200 | 0.431 | 1.295 | 0.218 | 0.433 |

[原始数据](benchmarks/vm-baselines-aot-0.18.3.json)包含两轮 × 五用例 × 九配置 × 七样本，
共 **630 次正式测量**，另有预热；其中保存了版本、参数、校验和、逻辑步骤、生成源码摘要
和测试二进制 SHA-256。没有从旧报告摘取数字拼接本表。

两轮中不带 JIT 的 JVM 整数累加中位值为 **140.335 / 145.578 ms**。
本页保留两轮数据，不用一个中位数代替稳定性分析。
其他配置也应结合 JSON 中全部样本解读；尤其是原生/JIT 的亚毫秒用例与 Lua 粗粒度计时。

### 3.1 Magel 0.18.3 相对 0.18.2

变化为 `(0.18.3 耗时 / 0.18.2 耗时 - 1) × 100%`，负数表示耗时下降。
本页把低于 5% 的变化作为噪声，5%–10% 作为需要重复支持的弱信号，超过 10% 才记明显变化；
这是工程判断阈值，不是置信区间或统计显著性检验。

| 用例 | 第一轮 VM 变化 | 第二轮 VM 变化 | 结论 |
| --- | ---: | ---: | --- |
| 整数累加 | +0.28% | +0.46% | 结合两轮样本判断，不以单次波动宣称收益 |
| 奇偶分支 | +0.41% | +1.35% | 结合两轮样本判断，不以单次波动宣称收益 |
| float64 累加 | -28.41% | -28.01% | 两轮均明显下降 |
| 函数调用 | -4.35% | -3.81% | 结合两轮样本判断，不以单次波动宣称收益 |
| 数组读取 | +1.82% | +1.54% | 结合两轮样本判断，不以单次波动宣称收益 |

两版浮点用例的逻辑步骤同为 **30,000,030**，整数、分支与调用用例步骤也相同；
数组用例仅少了四个初始化步骤。浮点差异因此不能解释为少跑了循环或修改了计算结果。
本轮不测前端编译耗时，也不根据运行时表格宣称编译速度提升。

### 3.2 两版 AOT 与原生 C++

前两列仍是 `(0.18.3 / 0.18.2 - 1) × 100%`，但只比较 AOT。
最后一列为第二轮 `0.18.3 AOT / 手写 C++` 耗时比，大于 1 表示 AOT 更慢。
原生/JIT 的短用例对调度噪声敏感；不要把小百分比或单轮变化当成已证实的优化收益。

| 用例 | 第一轮 AOT 版本变化 | 第二轮 AOT 版本变化 | 0.18.3 AOT / C++ |
| --- | ---: | ---: | ---: |
| 整数累加 | +0.46% | +0.00% | 0.99× |
| 奇偶分支 | -0.91% | -0.23% | 1.02× |
| float64 累加 | +0.77% | +0.00% | 1.00× |
| 函数调用 | -45.18% | -48.29% | 1.04× |
| 数组读取 | +9.82% | +10.15% | 85.18× |

两版 AOT 来自各自编译器及各自内嵌运行时，未把旧版导出器与新版磁盘运行时混用。
AOT 表内不包含导出、C++ 编译、链接、进程启动和运行时初始化耗时。
前三类 AOT 用例的版本差异均不足 1%。调用用例两轮耗时下降约 45%–48%，但热点汇编相同，
不能据此宣称算法或内联策略改善，详见 5.6。数组读取两轮耗时上升约 10%，应作为需要定位的
版本回退线索保留，不能被标量用例接近 C++ 的结果掩盖。

## 4. 实际测试代码示例

下面给出整数累加的完整测试代码；这同一份 Magel 源码分别供 0.18.3 与 0.18.2 使用，
同一份 Java 源码分别供 JVM 的两种模式使用。
包含其余四类负载在内的 **30 份完整源码**（五负载 × 五种语言，另含 Magel AOT 计时入口）见
[测试代码全集](examples/vm-performance.md)。它们由脚本直接保存自实际执行输入，源码摘要也在 JSON 内。

### 4.1 Magel：0.18.3 与 0.18.2

输入 `10000000`，输出 `49999995000000`。VM 计时由外部 C++ 运行器包围执行过程。

```cpp
int64 mix(int64 x){return x*17;}
int64 main(){int64 n=inputInt(),i=0;int64 total=0;while(i<n){total+=i;i+=1;}println("{}",total);return 0;}
```

### 4.2 JVM：同一 Java 代码，分别执行两种模式

不带即时编译使用 `java -Xint`，传入预热次数 `1`，标准输入提供两次 `10000000`；
带即时编译使用 `java -Xbatch`，传入预热次数 `20`，标准输入提供二十一次 `10000000`。
最后一次读取发生在计时前，前面的读取用于预热；`volatile sink` 保留计算结果。

```java
import java.util.Scanner;
public final class Case_integer_loop {
    private static volatile long sink;
    private static long mix(long x) { return x*17; }
    private static long calculate(long n) {
        long i=0; long total=0;
        while(i<n) { total+=i; ++i; }
        return total;
    }
    public static void main(String[] args) {
        Scanner input=new Scanner(System.in);
        int warmups=Integer.parseInt(args[0]);
        for(int i=0;i<warmups;++i) sink=calculate(input.nextLong());
        long n=input.nextLong();
        long start=System.nanoTime();
        long value=calculate(n);
        long elapsed=System.nanoTime()-start;
        sink=value;
        System.out.println((elapsed/1e6)+" "+value);
    }
}
```

### 4.3 Python

命令行传入 `10000000`。循环上限是函数局部参数，避免把全局变量查找额外混进每次循环条件。

```python
import sys,time
n=int(sys.argv[1])
def mix(x): return x*17
def calculate(n):
    i,total=0,0
    values=[1,2,3,4]
    while i<n:
        total += i
        i+=1
    return total
calculate(n)
start=time.perf_counter_ns()
result=calculate(n)
print((time.perf_counter_ns()-start)/1e6,result)
```

### 4.4 Lua

命令行传入 `10000000`。循环上限同样作为函数局部参数传递。

```lua
local n=tonumber(arg[1])
local function mix(x) return x*17 end
local function calculate(n)
local i,total=0,0
local values={1,2,3,4}
while i<n do total=total+i;i=i+1 end
return total end
calculate(n)
local start=os.clock()
local result=calculate(n)
print((os.clock()-start)*1000,result)
```

### 4.5 原生 C++

标准输入提供两次 `10000000`：一次预热，一次正式执行。`calculate` 禁止内联到测量包装层，
每次重新从标准输入取上限，`volatile sink` 保留结果；内部的 `mix` 仍允许正常内联。

```cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
static volatile std::int64_t sink;
static std::int64_t mix(std::int64_t x) { return x*17; }
__attribute__((noinline)) static std::int64_t calculate(std::int64_t n) {
    std::int64_t i=0; std::int64_t total=0;
    while(i<n) { total+=i; ++i; }
    return total;
}
int main() {
    std::int64_t n; if(!(std::cin>>n)) return 2;
    sink=calculate(n);
    if(!(std::cin>>n)) return 2;
    const auto start=std::chrono::steady_clock::now();
    const auto value=calculate(n);
    const auto end=std::chrono::steady_clock::now();
    sink=value;
    std::cout<<std::setprecision(17)<<std::chrono::duration<double,std::milli>(end-start).count()<<' '<<value<<'\n';
}
```

### 4.6 Magel AOT：两个版本使用同一份计时入口

两版都先输入一次 `n` 预热，再输入一次 `n` 测量。计时后比较预热结果与正式结果，
保留预热计算；每次重新读取上限，防止复用上一轮求和结果。计时使用 `std::clock::nowNanos()`，
只包围 `calculate`，输出在计时之后。示例只整理空白，实际生成文件的摘要保存在 JSON 中。

```cpp
import std::clock;
int64 mix(int64 x) { return x * 17; }
int64 calculate(int64 n)
{
    int64 i = 0;
    int64 total = 0;
    while (i < n)
    {
        total += i;
        i += 1;
    }
    return total;
}
int64 main()
{
    int64 first = inputInt();
    int64 warmup = calculate(first);
    int64 n = inputInt();
    int64 start = std::clock::nowNanos();
    int64 value = calculate(n);
    int64 elapsed = std::clock::nowNanos() - start;
    if (warmup != value) { return 1; }
    println("{} {}", elapsed / 1000000.0, value);
    return 0;
}
```

例如，两版分别执行：

```sh
magel-0.18.3.exe aot integer_loop-aot.mgl --out loop-aot-103.exe --opt-level=2 --unchecked --keep-intermediate
magel-0.18.2.exe aot integer_loop-aot.mgl --out loop-aot-102.exe --opt-level=2 --unchecked --keep-intermediate
```

脚本实际从仓库外的隔离目录调用编译器，确保两版分别使用内嵌资产，详见复现说明。

## 5. 性能差异的原因与证据

以下区分已确认的实现机制与尚未量化的归因。本轮没有进行采样剖析，
所以不把某条路径说成占总时间的固定百分比。
[诊断证据](benchmarks/vm-baselines-evidence-0.18.3.json)来自计时结束后的独立运行，
包含 JVM 内联日志、C++ 汇编、Python 自适应字节码及 Lua 字节码；诊断开关未用于正式计时。

### 5.1 为什么带 JIT 的 JVM 接近原生 C++

带 JIT 的 JVM 在预热后可以执行热点方法生成的机器码；原生 C++ 在执行前已经编译为机器码。
这两个配置均不必为源码中的每一步算术重复进行 VM 指令分派，因此在本轮小循环中接近。

函数用例有直接证据：HotSpot 日志显示 `Case_calls::mix (6 bytes) inline (hot)`；
C++ 的 `calculate(long long)` 汇编没有调用 `mix`，并把乘 17 的序列化为每轮增加 `0x11` 的归纳变量。
因此 JVM JIT/C++ 的这一行主要剩下紧凑的算术循环，不能用它与 Magel 的耗时比直接换算单次函数调用开销。

### 5.2 为什么 JVM 不带 JIT 与带 JIT 差异很大

`-Xint` 让 Java 方法按字节码解释执行，不走上述热点即时编译；每轮仍有解释器工作。
`-Xbatch` 允许 JIT，并让编译同步完成；20 次完整预热把测量重点放在热执行状态。
两列采用相同计算源码和结果，但执行技术不同，这正是本页坚持分列的原因。
这组数据不能用于推断 Java 冷启动、首请求或短命令行程序的总延迟。

### 5.3 为什么 Magel 的整数与浮点表现不同

Magel 的整数循环可以进入[寄存器区域执行核心](../library/stackvm/register_core.hpp)，
在一个批次中连续执行多条内部指令；它仍是解释执行，没有把热点编译成原生机器码。

0.18.3 新增的[浮点融合路径](../library/stackvm/parts/execution_fusion.inc)合并了加载、运算、保存以及部分比较跳转，
减少进入主执行循环的次数，保留原始 PC、逻辑步骤、float32 舍入和观察者回退。
这是浮点步骤相同而耗时明显下降的实现依据；但浮点仍使用槽位绑定、类型检查与融合后的分派，
尚未成为与整数相同的完整浮点区域核心，所以离 JIT/原生紧凑循环仍有明显距离。

### 5.4 为什么 Python 在部分用例中反而更快

动态类型不等于每个负载都更慢。本机 CPython 3.12 在预热后的浮点循环中可观察到
`BINARY_OP_ADD_FLOAT` 自适应专用字节码，避免每次都走最通用的加法路径。
Magel 的类型是静态的，但本轮测试的执行器仍承担槽位访问、类型归一化与解释器服务成本；
静态类型本身不会自动把这些成本消除。这能解释为什么不能仅凭“C++ 写的静态语言 VM”就预言它必胜 Python。
本轮未对两套运行时的分配或指令占比做量化剖析。

### 5.5 为什么函数调用和数组仍是 Magel 的改进重点

Magel 的[调用入口](../library/stackvm/parts/execution.cpp)仍需要处理参数队列、参数归一化、
调用帧与局部槽、返回目的地；小帧已有缓存，仍保留这些服务流程。
Lua 的测试字节码也保留 `CALL`，说明它不是依靠本轮的原生内联取得结果；
两种解释器调用路径的实际成本差异，需要后续用剖析和开关对照进一步拆分。

数组用例也有语义成本差异：Magel 经过[下标/指针容量检查与内存服务](../library/stackvm/parts/register_execution.inc)，
Lua 使用 table，Python 使用 list，Java 使用 `long[]`，C++ 使用固定长度原生数组。
C++ 源码没有逐次运行时数组边界检查；其他实现的表示和检查策略不同。
因此这行是实际数组写法的端到端表现，不能当成相同底层访存指令的比较，
更不能把取消 Magel 检查当作默认提速方案。

### 5.6 为什么 AOT 与 VM、手写 C++ 仍不同

AOT 把 IR 导出成直接执行的 C++ 控制流与标量运算，再由 GCC `-O2` 编译为机器码，
不再逐条分派 VM 指令。导出代码可见 `magel_addi`、`magel_norm_i` 及直接函数调用，
因此它与 VM 的执行成本结构不同，不能用 VM 的浮点融合收益直接预测 AOT 的版本收益。

AOT 也不等于手写 C++：它保留 Magel 的整数回绕、运行时桥、资源管理和显式检查语义。
数组导出可见 `rt.checkIndex`、`rt.checkPointer` 与 `rt.loadMemory`；手写 C++ 用例是原生固定数组。
`--unchecked` 关闭可选护栏与 panic 栈，不是把上述所有语义服务都删除。
这些机制解释了两列为什么可能不完全相同；[5.7 节](#57-22122-倍的口径代价从哪来同源单开关实验)用一个只切开关的对照把其中占大头的部分量出来了。
本轮两版 AOT 调用用例的热点循环均为 `add / add 17 / cmp / jne`，且没有调用 `mix`；
因此观察到的版本耗时差不能解释为少做了乘法或消除了旧版函数调用。
新版该循环从 `0x140017460` 开始，旧版从 `0x140017078` 开始，代码布局和对齐不同。
布局、对齐或其他固定开销是待验证的可能因素，本轮没有通过控制变量或采样剖析确定其贡献。
[AOT 构建证据](benchmarks/vm-baselines-aot-evidence-0.18.3.json)保留了各版本运行时标记、构建参数、
生成代码摘要、热点汇编及循环/检查片段。该证据在正式测量结束后提取。

### 5.7 「22–122 倍」的口径代价从哪来：同源单开关实验

上面的倍数长期缺一个量化归因。补法是**只动一个开关**：源码、优化等级、g++ 命令行、
输入、重复次数全部不变，只把 `magel aot` 的 `--checked` 换成 `--unchecked`。
脚本 [benchmark_aot_flag_isolation.py](../tests/benchmark_aot_flag_isolation.py)，
原始数据 [aot-flag-isolation-2026-10-16.json](benchmarks/aot-flag-isolation-2026-10-16.json)。

关键前提是**生成代码除了开关本身没有任何差别**。脚本对两个版本的生成 C++ 逐行比对并把
差异写进报告，实测 3.5 MB 生成文件中差异**只有 3 行**：

```cpp
-#define STACKVM_WITH_ALLOCATION_GUARDS 1
+#define STACKVM_WITH_ALLOCATION_GUARDS 0
+#define NO_MAGEL_PANIC_TRACE 1
```

`NO_MAGEL_PANIC_TRACE` 会把 [stackvm.hpp](../library/stackvm/stackvm.hpp) 里的
`STACKVM_WITH_PANIC_TRACE` 置 0，于是生成代码中每条语句前后的
`rt.locateTrace(file, line)` 整段被裁掉。也就是说，下面所有倍数都只能归给这两个宏，
不能归给"生成的代码不一样"。

AOT 五算例（ms，n = 1,000,000，3 轮 × 5 次取中位数）：

| 用例 | `--unchecked` | `--checked`（默认） | 手写 C++ | checked / unchecked | unchecked / C++ |
| --- | ---: | ---: | ---: | ---: | ---: |
| scalar | 2.551 | 108.005 | 2.547 | **42.3×** | **1.00×** |
| fixed_array | 8.060 | 279.363 | 0.182 | **34.7×** | 44.4× |
| heap_array | 14.511 | 326.999 | 0.278 | **22.5×** | 52.2× |
| container | 1136.713 | 1478.025 | 0.273 | 1.30× | 4161× |
| function_calls | 2.636 | 320.551 | 2.546 | **121.6×** | **1.04×** |

VM 基线的那五类负载只切同一个开关（ms，n = 1,000,000，每进程一个样本）：

| 负载 | `--unchecked` | `--checked`（默认） | checked / unchecked |
| --- | ---: | ---: | ---: |
| integer_loop | 0.190 | 80.8 | **425×** |
| branches | 0.376 | 137.0 | **364×** |
| float64_loop | 0.372 | 75.7 | **203×** |
| calls | 0.210 | 269.0 | **1281×** |
| array_reads | 4.530 | 164.0 | 36.2× |

**代价机制。** `--checked` 下 `RuntimeBridge::locateTrace` 的实现在
[aot_bridge.cpp](../library/stackvm/parts/aot_bridge.cpp) 里是一次
`std::lock_guard<std::mutex>` 加 `std::this_thread::get_id()` 查 `activeTrace` map；
它定义在静态库中，生成的 C++ 调用它时**不能内联**。`scalar` 的循环体里有 **7 条**
`locateTrace`，(108.005 − 2.551) ms ÷ (7 × 10⁶) ≈ **15 ns/次**，与"一次未内联的
互斥锁加线程 id 查表"的量级吻合。`function_calls` 倍数最大，因为每次迭代除循环体
8 条 `locateTrace` 外，还要构造/析构一次 `RuntimeCallFrame`（两者各自加锁）并进入
`magel_fn_callStep` 的 6 条——约 16 次受锁操作/迭代。而 `container` 的主循环每次迭代
约 20 条 `locateTrace` 只贡献 341 ms（≈17 ns/次），因为旁边两次 `callNative` 已经贵了 50 倍。

**两个反证。** 其一，两种口径下 VM 五类负载的校验值**逐位相同**（报告 `vm_loads_checksum_match`
全为 true），换开关没有改计算结果，只是变慢。其二，`--unchecked` 不是"被优化掉了"：
整数负载在 n = 10⁵ / 10⁶ / 10⁷ 下耗时 0.0226 / 0.186 / 1.82 ms，每乘 10 倍 n 耗时乘
8.2 倍与 9.8 倍，呈线性增长且校验值正确。

**剩下的是什么。** 关掉护栏后剩下的差距与开关无关，来自运行时边界：

- `container` 的 `values[k]` 读写没有被降级成直接访存，而是
  `rt.callNative("__vm_vec_set", ...)` / `rt.callNative("__vm_vec_at", ...)`：按字符串名
  查原生函数表、`magel_native_args.resize(N)`、逐个 `RuntimeValue::ofInt` 装箱。
  1136.713 ms ÷ (2 × 10⁶ 次) ≈ **568 ns/次**，所以关掉护栏后仍是手写 C++ 的 **4161 倍**。
- `fixed_array` / `heap_array` 走 `rt.storeInteger` / `rt.loadInteger` 逐元素运行时原语加
  `rt.checkIndex`，同样在静态库里、不能内联；关掉护栏后仍是 C++ 的 44–52 倍。
- `scalar` 与 `function_calls` 的循环体只剩 `static` 定义在生成 C++ 内、可内联的
  `magel_addi` / `magel_muli` / `magel_norm_i`，所以关掉护栏后与手写 C++ 打平。

**三条结论。**

1. `--checked` 是 `magel aot` 的**默认**口径；本文件[第 3 节](#3-同轮实测结果)那张历史表用的是
   `--unchecked`。两节数字不能直接相减或相比，跨节引用时必须先对齐口径。
2. 默认口径下 22–122 倍（VM 负载 36–1281 倍）的差距**几乎全部来自 panic 追踪这一个开关**，
   不是生成代码质量。关掉它，`scalar` 与 `function_calls` 与手写 C++ 持平。
3. 真正值得优化的不是护栏，而是运行时边界：容器按字符串名查表加装箱（约 568 ns/次）、
   数组的 `storeInteger` / `loadInteger` 逐元素调用。这两处关掉护栏也不会变快。

### 5.8 这些倍数该怎么读：分母决定结论

同一份耗时数字，换一个分母就是另一个结论。按本文件已有的数据，**分母必须写明**：

| 对照对象 | Magel AOT | Magel VM 1.0.0 | 读法 |
| --- | --- | --- | --- |
| 手写 C++ `-O2` | 1.02× / 1.03× / 1.00× / 1.02× / **26.0×** | 52× / 71× / 369× / 2184× / 340× | AOT 除数组外与原生持平 |
| JVM `-Xbatch`（JIT 热执行） | 0.95× / 1.18× / 1.01× / 0.96× / 15.4× | 48× / 81× / 374× / 2072× / 202× | AOT 与 C2 同量级；VM 落后两个数量级 |
| JVM `-Xint`（纯解释） | 0.037× / 0.040× / 0.071× / 0.015× / 0.70× | **1.9× / 2.7× / 26× / 33× / 9.1×** | 解释对解释，VM 只在整数和分支上接近 |
| CPython 3.12 | 0.006× / 0.008× / 0.014× / 0.004× / 0.13× | **0.32× / 0.59× / 0.20× / 0.13× / 1.64×** | VM 五场赢四场，只在数组读取上输 |
| Lua 5.5.1（无 JIT） | 0.051× / 0.063× / 0.105× / 0.014× / 0.59× | **2.6× / 4.3× / 39× / 31× / 7.7×** | VM 五场全输 |

（单元格是 `该实现耗时 ÷ 对照耗时`，小于 1 表示 Magel 更快；两列都取
[九配置本机复测](#vm-九配置本机复测2026-10-16)的第二轮中位数，因此同一格可以横向相加比较。
列序为整数累加 / 奇偶分支 / float64 累加 / 函数调用 / 数组读取。）

按分母分开看：

- **对原生 C++**：结论是好的。前四个算例 AOT 与手写 C++ 落在 1.00–1.03 倍内，数组读取
  从 0.18.3 的 85.18 倍收窄到 26.02 倍，是本版最大的单项改善。
- **对 JVM 的 JIT**：结论也是好的。AOT 与 `-Xbatch` 在前四个算例上落在 0.95–1.18 倍，
  说明 AOT 已经进入即时编译热执行所在的量级；数组那一格 15.4 倍另属于 5.5 节说的语义成本差异。
- **对 JVM 的纯解释器**：结论是**不好**。整数累加只慢 1.9 倍、分支慢 2.7 倍，可以说接近；
  但 float64 慢 26 倍、函数调用慢 33 倍，说明这两条指令路径的实现远比"解释执行"本身贵。
- **对 CPython**：结论偏**中性偏好**。五个用例赢四个（最快的一格快 296 倍），
  只有数组读取输 1.64 倍。
- **对 Lua**：结论是**不好**。五个用例全输，2.6 倍到 39 倍。

同一台 VM 内部的差异同样值得注意：把耗时除以各自逻辑步数，每步成本从整数累加的
**0.63 ns/步**（[寄存器区域执行核心](../library/stackvm/register_core.hpp)批量连续执行）
到函数调用的 **14.89 ns/步**，相差 24 倍。换句话说瓶颈**不在指令分派本身**，
而在少数指令的实现路径上——函数调用、浮点、容器与数组原语。
这也解释了为什么 5.3 节的浮点融合路径能拿到两轮一致的两位数收益。

三条保留：本轮全部是微基准，只覆盖列出的代码与输入，不能外推成语言整体排名；
`--checked` 默认口径是拿性能换诊断能力的**有意取舍**，"关掉它就快"不等于应该关掉；
AOT 关了护栏后数组仍慢 44 倍、容器仍慢 4161 倍，这两个缺口与护栏无关。


## 6. 测量口径与复现

### 6.1 计时包含什么

| 配置 | 计时器 | 计时范围 | 正式测量前预热 |
| --- | --- | --- | ---: |
| 两版 Magel VM | `std::chrono::steady_clock` | 已加载字节码的执行准备、执行及最终结果输出 | 1 次 |
| 两版 Magel AOT | `std::clock::nowNanos()` | `calculate` 调用 | 1 次 |
| JVM 不带即时编译 | `System.nanoTime` | `calculate` 调用 | 1 次 |
| JVM 带即时编译 | `System.nanoTime` | `calculate` 调用 | 20 次 |
| Python | `time.perf_counter_ns` | `calculate` 调用 | 1 次 |
| Lua | `os.clock` | `calculate` 调用 | 1 次 |
| C++ | `std::chrono::steady_clock` | `calculate` 调用 | 1 次 |

所有配置都排除进程启动、源码编译和程序加载；非 VM 配置的结果输出在计时之后。
VM 包含一次执行准备与最终输出，其他配置只包围计算函数，边界并非完全相同；
这里公开这种差异，不用估算值扣除它。Lua 的 `os.clock` 与其余单调计时器来源不同，
Windows 上样本常呈整数毫秒台阶。很短的原生/JIT 测量及受调度影响的样本应结合原始数据看待。

编译完成后才开始计时。每个进程只记录一个正式样本，每一轮轮转配置顺序；
所有配置串行执行，正式测量期间没有并行构建、其他测试或诊断剖析。
两轮各重复七次，取各轮中位数，不用最小值替代中位数，也不合并两轮掩盖波动。

### 6.2 运行命令

入口脚本：[benchmark_vm_baselines.py](../tests/benchmark_vm_baselines.py)。
需要同机的 JDK、Python、GCC/MinGW，以及两版 Magel 编译器和对应 VM 运行器。
按[工具链文档](09-toolchain.md)分别构建 `v0.18.3` 与 `v0.18.2`；每个运行器必须使用自己版本的头文件与静态库。

AOT 的构建阶段把两版编译器复制到仓库外的临时目录，并从该目录运行，强制使用版本对应的
内嵌运行时。如果从当前仓库直接调用旧版编译器，旧导出器可能读取新版磁盘资产，污染版本对照。
脚本对两版、五类负载分别生成十个 AOT 可执行文件；只在构建阶段并行两项，全部完成后才开始串行计时。
`--reuse-builds` 仅在源码、编译器、GCC 版本、参数、资产隔离口径及产物哈希一致时复用已有 AOT 程序。

在各自版本源码目录编译运行器：

```sh
g++ -std=c++17 -O2 -DNDEBUG -DSTACKVM_WITH_REFLECTION=1 -DMAGEL_CUSTOM_ALLOCATOR=1 -Ilibrary -Ilibrary/stackvm tests/benchmark_vm_program.cpp build/magel-stackvm/libstackvm.a -pthread -o build/vm-program-benchmark.exe
```

Lua 源码沿用[官方 5.5.1 归档](https://www.lua.org/ftp/lua-5.5.1.tar.gz)。
先用 [benchmark_vm_lua.py](../tests/benchmark_vm_lua.py) 完成下载和构建；新脚本再次核对源码包 SHA-256。

```text
1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce
```

本机本轮命令如下；其他机器应替换为自己的两版二进制路径：

```sh
python tests/benchmark_vm_baselines.py --magel magel.exe --runner tmp/optimization103/runner.exe --baseline-magel tmp/optimization103/magel-0.18.2.exe --baseline-runner tmp/optimization103/baseline-runner.exe --repeats 7 --rounds 2 --reuse-builds --output doc/benchmarks/vm-baselines-aot-0.18.3.json
```

`--baseline-magel` / `--baseline-runner` 是可选的：两者都不给时只测当前版本，给出后按上表追加旧版 VM 与 AOT 两行。
脚本在 `build/vm-baselines/` 生成全部源码、Java class、C++ 可执行文件、各版本字节码与 AOT 程序，
核对每个输出并保存 JSON。本页不依赖当前仓库中存在这些忽略的二进制；它们需要由对应版本构建获得。
本次改动只重写报告和补充测试脚本，不改变 `v0.18.3` 的编译器、版本号或已发布标签。

## 7. 原始数据与旧轮次

- [本轮九配置、两轮完整测量](benchmarks/vm-baselines-aot-0.18.3.json)
- [本轮内联、汇编与字节码诊断证据](benchmarks/vm-baselines-evidence-0.18.3.json)
- [五种语言、VM/AOT 两套入口的完整测试代码](examples/vm-performance.md)
- [此前未包含 AOT 的七配置测量](benchmarks/vm-baselines-0.18.3.json)
- 旧的仅 Lua 对照：[第一轮](benchmarks/vm-lua-0.18.3.json)、[重复轮](benchmarks/vm-lua-0.18.3-repeat.json)
- [旧的 switch / 标签分派对照](benchmarks/register-dispatch-0.18.3.json)
- 2026-10-16 本机换机复测：[checked AOT 与五个语言对照](benchmarks/aot-optimization-2026-10-16-recheck.json)、[VM 九配置](benchmarks/vm-baselines-1.0.0-recheck.json)
- 2026-10-16 同源单开关隔离实验（[5.7 节](#57-22122-倍的口径代价从哪来同源单开关实验)）：[原始数据](benchmarks/aot-flag-isolation-2026-10-16.json)、[脚本](../tests/benchmark_aot_flag_isolation.py)

旧轮次保留为归档，不混入本页主表。本页结论只针对列出的代码、输入与配置，
不把这些微基准扩展成语言整体性能排名，也不把桌面宿主结果当作真实 MCU 硬件时延。

## STM32H743 实机补充

[FK-STM32H743XIH6 板端工程](../example/mcu/FK-STM32H743XIH6/README.md) 已在
480 MHz Cortex-M7 上采集 DWT 周期数，使用 SDRAM 的 2 MiB 元数据池、16 MiB 运行时池和
4 MiB 字节池。Clang 以 `-Os` 构建，共享 VM 的执行优化保持默认最高配置。

同一段 10,000 次整数递推，两次独立启动的 VM 中位数分别为 **8.146217 ms** 与
**8.143513 ms**，约 **391 周期/次**、**123 万次/秒**。当前原生 C++ 胶水路径中位数为
**0.410529 ms** 与 **0.528200 ms**，对应耗时比 **19.84** 与 **15.42**。
该原生路径的循环条件仍读取胶水参数，且区间包含 VM 调度与原生边界，不能当作裸 C++ 或 AOT 的上限。

两次都保留了显著偏慢的第一轮，隔离 SWD 连接后该现象仍存在；原因尚未细分。
VM 包含分派、运行时值和控制流处理，原生代码直接执行机器指令；SDRAM、Cache、
中断和胶水也影响计时，目前没有对各项作独立归因。完整样本、代码、固件哈希及测试边界见
[板端报告](../example/mcu/FK-STM32H743XIH6/README.md#实机结果2026-10-15)，原始记录为
[首次采样](../example/mcu/FK-STM32H743XIH6/results/benchmark.json) 与
[隔离复测](../example/mcu/FK-STM32H743XIH6/results/benchmark-isolated.json)。
这些结果来自真实 MCU，不能与本页桌面环境的耗时直接作硬件无关排名。

## checked AOT 优化与 JDK 对照（2026-10-16）

新旧 Magel 均使用 `-O3 --checked`、反射与完整四元式优化；基线为提交 `12c4dc4`，候选为当前 1.0.0 工作树。每个算例执行 1,000,000 次迭代，独立启动三轮，每轮采样五次，表中为 15 次测量的中位数，单位毫秒。全部校验值均一致。计时排除启动、编译与打印，包含数组、容器的初始化与清理。

| 算例 | 优化前 | 优化后 | 加速比 | JDK 8 | JDK 22 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 整数递推 | 387.8156 | 138.2660 | 2.80× | 3.0202 | 3.4046 |
| 固定数组 | 1046.6111 | 353.9385 | 2.96× | 0.7019 | 0.4898 |
| 堆数组 | 1301.1306 | 428.8107 | 3.03× | 0.7017 | 0.4881 |
| 容器 | 2941.7599 | 2115.1724 | 1.39× | 2.3144 | 3.5630 |
| 函数调用 | 883.2179 | 387.9722 | 2.28× | 3.0226 | 3.4214 |

checked 下的收益来自标量专用内存访问、数组检查成功路径、调用帧内宿主参数复用，以及减少源码定位的字符串构造与复制；检查与 panic 跟踪仍开启。Java 使用 Temurin 8u504-b01、Oracle 22.0.1，参数为 `-Xms256m -Xmx256m -Xbatch`，每进程预热 20 次；AOT 预热一次。Java 的两种数组算例均为 `long[64]`，容器为 `ArrayList<Long>`，Magel 使用自身数组与 `List<int64>`，数据布局和 JIT 行为不同。这是算法对照，Java 在这些算例上仍明显更快，不能据此推断整门语言的性能排名。

[原始测量、环境与哈希](benchmarks/aot-optimization-2026-10-16.json)；[复测脚本](../tests/benchmark_aot_optimization.py)。

### 本机复测：Windows 11 / Zen5，追加 C++、Lua、Python

上表的环境无法在本机重现，因此做了一轮**换机复测**：算例、迭代次数、预热、轮次与单位全部沿用上表口径（1,000,000 次迭代、独立启动三轮、每轮五次、取 15 次中位数、`-O3 --checked`、AOT 预热一次、Java 预热 20 次），只替换硬件，并补上原生 C++、Lua、Python 三列。两台机器的绝对值不可直接跨机排名，下表只用来核对**同机相对关系**是否稳定。

| 项目 | 历史轮（2026-10-16） | 本机复测（2026-10-16） |
| --- | --- | --- |
| 系统 | Windows 10 10.0.19045 | Windows 11 10.0.26200 |
| CPU | AMD Ryzen 5 5600X（Zen3，6 核 12 线程） | AMD Ryzen 7 9700X（Zen5，8 核 16 线程） |
| JDK 8 | Temurin 8u504-b01 | Oracle JRE 8u421-b09（见下方口径说明） |
| JDK 22 | Oracle 22.0.1+8-16 | 同左 |
| Magel AOT | 1.0.0 工作树，`-O3 --checked` | 同左，候选树与上表一致（`aot.cpp` / `aot_bridge.cpp` 哈希相同） |
| C++ | 未测这五个算例 | GCC/MinGW 11.2.0，`-O2 -DNDEBUG` |
| Lua | 未测这五个算例 | 官方 5.5.1 源码，mingw 目标，`-O2`，无 JIT |
| Python | 未测这五个算例 | CPython 3.12.3，未启用 JIT |

本机复测，中位数，单位 ms：

| 算例 | Magel AOT | 原生 C++ | JDK 8 | JDK 22 | Lua | Python |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 整数递推 | 108.7904 | 2.5543 | 2.5557 | 2.8706 | 6.000 | 48.6253 |
| 固定数组 | 274.1098 | 0.1816 | 0.4730 | 0.3432 | 9.000 | 47.1594 |
| 堆数组 | 329.2174 | 0.2786 | 0.4718 | 0.3440 | 9.000 | 47.2513 |
| 容器 | 1401.9689 | 0.2818 | 1.4372 | 1.9939 | 9.000 | 47.3142 |
| 函数调用 | 308.8008 | 2.5514 | 2.5607 | 2.9863 | 15.000 | 63.6530 |

同一列在两台机器上的比值（本机 ÷ 历史，小于 1 表示本机更快）：

| 算例 | Magel AOT | JDK 8 | JDK 22 |
| --- | ---: | ---: | ---: |
| 整数递推 | 0.79 | 0.85 | 0.84 |
| 固定数组 | 0.77 | 0.67 | 0.70 |
| 堆数组 | 0.77 | 0.67 | 0.70 |
| 容器 | 0.66 | 0.62 | 0.56 |
| 函数调用 | 0.80 | 0.85 | 0.87 |

本机全部实现都落在历史的 **0.56–0.87 倍**区间，与 Zen5 对 Zen3 的单核提升方向一致；换算关系在五个算例上同号，没有出现某一列独自偏离的迹象。跨机绝对值不可比，这条结论只说明相对关系在两台机器上都能复现。

同机排序（本机，由快到慢）：

| 算例 | 排序 |
| --- | --- |
| 整数递推 | C++ 2.55 ≈ JDK 8 2.56 ≈ JDK 22 2.87 < Lua 6.0 < Python 48.6 < Magel AOT 108.8 |
| 固定数组 | C++ 0.18 < JDK 22 0.34 < JDK 8 0.47 < Lua 9.0 < Python 47.2 < Magel AOT 274.1 |
| 堆数组 | C++ 0.28 < JDK 22 0.34 < JDK 8 0.47 < Lua 9.0 < Python 47.3 < Magel AOT 329.2 |
| 容器 | C++ 0.28 < JDK 8 1.44 < JDK 22 1.99 < Lua 9.0 < Python 47.3 < Magel AOT 1402.0 |
| 函数调用 | C++ 2.55 ≈ JDK 8 2.56 < JDK 22 2.99 < Lua 15.0 < Python 63.7 < Magel AOT 308.8 |

三点值得注意：

- **Lua 比 Python 快 4–8 倍，但本轮只承认量级。** Lua 官方解释器在 Windows 上 `os.clock` 以 1 ms 为台阶，样本全部落在整数毫秒（如整数递推 15 次落在 5/6/7 ms），单次量化误差可达 ±15%。用外部墙钟交叉验证：`n = 10,000,000` 的整数递推，`os.clock` 报 51–53 ms，进程墙钟 56–58 ms（含约 4 ms 启动），两处一致，量级可信但小数位不应被当作有效精度。
- **Python 的后三列几乎相同（47.2 / 47.3 / 47.3 ms）不是巧合。** Python 里定长、堆数组、容器都是同一个 `list`，三个算例退化为同一段代码；Lua 同理（`table` 兼作两者）。Java 与 Magel 才有栈数组、堆数组与容器的分别，所以 `fixed_array` 与 `heap_array` 在 Java 列也是同一份实现。
- **Java 的容器算例仍然是 JDK 8 快于 JDK 22（1.44 对 1.99 ms）。** 历史轮同一方向的比值更大（2.31 对 3.56 ms），两轮一致，`ArrayList<Long>` 的装箱与扩容在这两个虚拟机上表现本就不同。

口径与偏差说明：本机没有 Temurin 8u504 完整 JDK，JDK 8 列改用本机已有的 Oracle JRE 8u421-b09 运行，类文件由 JDK 22 的 `javac --release 8` 产出（JRE 不含 `javac`）；历史轮为完整 Temurin JDK。这一替换只影响 JDK 8 一行，且该行与 JDK 22 的相对关系与历史轮同向。C++ 列五个算例均为 `noinline`，只禁止折进计时包装层，`function_calls` 内部的 `callStep` 仍允许内联。

[本机复测原始数据、样本与环境](benchmarks/aot-optimization-2026-10-16-recheck.json)；[复测脚本](../tests/benchmark_aot_optimization.py)（`--lua`、`--python`、`--cpp` 三个开关给出后即追加对应列）。Lua 解释器由 [benchmark_vm_lua.py](../tests/benchmark_vm_lua.py) 的同一份官方源码包构建，SHA-256 仍为 `1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce`。

## VM 九配置本机复测（2026-10-16）

上面的 [第 3 节](#3-同轮实测结果) 那张九配置表测的是**解释执行的 VM**，和前面 AOT 那节不是同一批算例。这里把它在本机重测一遍，口径沿用原表：五类负载、循环上限由运行时输入、正式测量前预热一次、每个进程只记一个样本、每轮轮转配置顺序、全部串行，同样是**两轮 × 七次**。

只有两处与历史表不同：

- 本机只有 1.0.0 一个 Magel 版本，历史表里占两行的 0.18.3 / 0.18.2 收敛为 `Magel 1.0.0` 一行，没有为凑齐九行去构建旧版编译器。
- 硬件从 Ryzen 5 5600X（Zen3，6 核）换成 **Ryzen 7 9700X**（Zen5，8 核 16 线程），系统从 Windows 10 19045 换到 Windows 11 26200。JDK、Python、Lua、GCC 版本不变。

本机复测（**第二轮**中位数，单位 ms）：

| 配置 | 整数累加 | 奇偶分支 | float64 累加 | 函数调用 | 数组读取 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Magel 1.0.0 VM | 94.609 | 25.933 | 272.296 | 401.900 | 61.874 |
| Magel 1.0.0 AOT | 1.850 | 0.375 | 0.737 | 0.187 | 4.740 |
| JVM 不带即时编译 `-Xint` | 50.304 | 9.437 | 10.435 | 12.171 | 6.809 |
| JVM 带即时编译 `-Xbatch` | 1.956 | 0.319 | 0.729 | 0.194 | 0.307 |
| Python | 299.783 | 44.210 | 53.357 | 53.410 | 37.694 |
| Lua | 36.000 | 6.000 | 7.000 | 13.000 | 8.000 |
| 原生 C++ | 1.819 | 0.364 | 0.738 | 0.184 | 0.182 |

两轮同号且接近（Magel VM 整数累加 94.301 / 94.609，原生 C++ 1.8253 / 1.8194），没有哪一轮被调度噪声带偏。五个用例的逻辑步数两轮一致：整数 **150,000,030**、分支 **25,500,030**、float64 **30,000,030**、调用 **27,000,030**、数组 **33,000,136**；七次重复的校验值也各自唯一，说明没有少跑循环或改掉计算结果。

### 跨机不可比，但同机比值可比

绝对耗时受硬件影响，不能跨机相减。下面用**同机比值**对照，这类比值对硬件不敏感。

`Magel AOT / 原生 C++`，越接近 1 越贴近手写 C++：

| 用例 | 0.18.3 | 1.0.0 |
| --- | ---: | ---: |
| 整数累加 | 0.99× | 1.02× |
| 奇偶分支 | 1.02× | 1.03× |
| float64 累加 | 1.00× | 1.00× |
| 函数调用 | 1.04× | 1.02× |
| 数组读取 | **85.18×** | **26.02×** |

`Magel AOT / Magel VM`，即同一版本内 AOT 相对自身解释器快多少，越小 AOT 优势越大：

| 用例 | 0.18.3 | 1.0.0 |
| --- | ---: | ---: |
| 整数累加 | 0.016× | 0.020× |
| 奇偶分支 | 0.013× | 0.014× |
| float64 累加 | 0.0037× | 0.0027× |
| 函数调用 | 0.00037× | 0.00047× |
| 数组读取 | 0.432× | **0.077×** |

两条都指向同一件事：**数组读取的 AOT 短板基本补上了。** 0.18.3 的 AOT 数组用例比手写 C++ 慢 85 倍，且只比自己的 VM 快 2.3 倍——[3.2 节](#32-两版-aot-与原生-c) 当时把它列为"需要定位的版本回退线索"保留。1.0.0 上这两个比值分别变成 26 倍和 13 倍，缺口收窄约一个量级；其余四个用例的 AOT/C++ 比值稳定在 1.00–1.03 倍，没有被这次变化带偏，说明这不是整表平移。

### 其他配置的跨机比值

同一配置在两台机器上的比值（本机 ÷ 历史，取**第二轮**）：

| 配置 | 整数累加 | 奇偶分支 | float64 累加 | 函数调用 | 数组读取 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Magel 0.18.3 VM → 1.0.0 VM | 0.70 | 0.78 | 0.77 | 0.66 | 0.72 |
| Magel 0.18.3 AOT → 1.0.0 AOT | 0.85 | 0.85 | 0.57 | 0.82 | 0.13 |
| JVM 不带即时编译 `-Xint` | 0.35 | 0.43 | 0.42 | 0.45 | 0.41 |
| JVM 带即时编译 `-Xbatch` | 0.85 | 0.85 | 0.56 | 0.84 | 0.64 |
| Python | 0.75 | 0.72 | 0.74 | 0.74 | 0.74 |
| Lua | 0.63 | 0.55 | 0.64 | 0.81 | 0.73 |
| 原生 C++ | 0.83 | 0.85 | 0.57 | 0.84 | 0.42 |

JVM、Python、Lua、C++ 四列的比值集中在 0.35–0.85，方向一致；`-Xint` 整列落在 0.35–0.45，说明这台机器对**解释执行**同样更快，不是只有原生代码受益。float64 与数组两列比值普遍偏低（0.42–0.64，AOT 数组那格 0.13 另有前述回退修复），与 Zen5 在浮点和访存上的提升方向相符，且同时出现在 C++ 与 JVM 上，是硬件现象而非某一门语言的异常。

Python 与 Lua 的相对关系也稳定：Python 比 Lua 慢 4–8 倍（本机 4.1–8.3 倍，历史 4.5–7.0 倍）。Lua 的 `os.clock` 在 Windows 上仍是 **1 ms 台阶**，该列样本全部落在整数毫秒，小数位不应被当作有效精度；原表的小数位同样如此。

和 AOT 那节一样，这里只做同机对照。490 次正式测量只覆盖列出的代码、输入与配置，不把这些微基准扩展成语言整体性能排名。

[本机复测原始数据](benchmarks/vm-baselines-1.0.0-recheck.json)含两轮 × 五用例 × 七配置 × 七样本共 **490 次正式测量**（另有预热），并保存了生成源码摘要、字节码与 AOT 产物哈希、逻辑步数、环境与全部样本。运行命令：

```sh
python tests/benchmark_vm_baselines.py --magel magel.exe --runner build/vm-program-benchmark.exe --magel-label "Magel 1.0.0" --repeats 7 --rounds 2 --output doc/benchmarks/vm-baselines-1.0.0-recheck.json
```

`--baseline-magel` / `--baseline-runner` 已改为可选：给出后脚本会额外追加旧版 VM 与 AOT 两行，并沿用同一套仓库外隔离构建流程。VM 运行器仍按 [6.2 节](#62-运行命令) 的命令从 `tests/benchmark_vm_program.cpp` 与当前 `build/magel-stackvm/libstackvm.a` 构建。
