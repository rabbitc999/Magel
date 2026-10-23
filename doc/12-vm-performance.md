# VM 性能对比：VM、JVM、Python、Lua、C++

本页保留 **2026-10-15 同一台机器上的历史实测**，被测 ChaLi 版本为 **0.18.3 / 0.18.2**，比较五类负载。
这些数据不作为当前 VERSION 的重新实测；当前能力见[统一说明](17-capabilities.md)。
ChaLi 两版均使用默认最高 IR 与 VM 优化，并编入反射。

下面给出实测、完整测试代码及实现证据，不从语言类型预设性能排序。

## 1. 测试环境与七个配置

| 项目 | 本轮配置 |
| --- | --- |
| 日期 | 2026-10-15，Asia/Shanghai |
| 系统 | Windows 10 10.0.19045，x64 |
| CPU / 内存 | AMD Ryzen 5 5600X，6 核 12 线程，32 GB RAM |
| ChaLi 0.18.3 | `v0.18.3` / `b5cfc9f2720c149bca7626bbaa5cfd52ad8f5576` |
| ChaLi 0.18.2 | `v0.18.2` / `320108bfda7c0b7035ed019a0010a87b43fe0e71` |
| 两版 ChaLi 编译器与 VM | GCC/MinGW 11.2.0，`-O2 -DNDEBUG`；默认最高 IR / VM 优化，反射开启；各自编译源码并运行各自字节码 |
| JVM 不带即时编译 | Oracle Java 22.0.1+8-16，HotSpot 64-Bit Server VM，`java -Xint` |
| JVM 带即时编译 | 同一 JDK、同一 `.class`，`java -Xbatch`；保留 HotSpot 默认分层编译，测量前完整预热 20 次 |
| Python | CPython 3.12.3，64 位；默认解释器，未启用 JIT |
| Lua | 官方 Lua 5.5.1 源码，MinGW 官方构建目标，`-O2`，无 JIT |
| C++ | 原生 C++17，GCC/MinGW 11.2.0，`-O2 -DNDEBUG`；没有使用 `-ffast-math`、LTO 或手写闭式求和 |

JVM 两列共用 Java 源码和 class 文件，只改变执行模式与对应预热次数。

## 2. 五类负载、输入与校验值

所有上限 `n` 都来自运行时输入，循环内不打印。整数数据在各语言中均不溢出，
浮点用例统一使用双精度；本轮不拿 ChaLi 的 `float32` 与其他语言的双精度作比较。

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
| ChaLi 0.18.3 VM | 135.138 | 33.099 | 353.936 | 609.619 | 85.360 |
| ChaLi 0.18.2 VM | 134.522 | 32.659 | 491.668 | 633.749 | 84.069 |
| JVM 不带即时编译 `-Xint` | 145.578 | 22.043 | 25.071 | 26.915 | 16.734 |
| JVM 带即时编译 `-Xbatch` | 2.302 | 0.377 | 1.299 | 0.231 | 0.479 |
| Python | 400.934 | 61.285 | 71.810 | 72.427 | 51.250 |
| Lua | 57.000 | 11.000 | 11.000 | 16.000 | 11.000 |
| 原生 C++ | 2.200 | 0.431 | 1.295 | 0.218 | 0.433 |

共 **490 次正式测量**，另有预热；其中保存了版本、参数、校验和、逻辑步骤、生成源码摘要
和测试二进制 SHA-256。没有从旧报告摘取数字拼接本表。

两轮中不带 JIT 的 JVM 整数累加中位值为 **140.335 / 145.578 ms**。
本页保留两轮数据，不用一个中位数代替稳定性分析。
其他配置也应结合 JSON 中全部样本解读；尤其是原生/JIT 的亚毫秒用例与 Lua 粗粒度计时。

### 3.1 ChaLi 0.18.3 相对 0.18.2

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

## 4. 实际测试代码示例

下面给出整数累加的完整测试代码；这同一份 ChaLi 源码分别供 0.18.3 与 0.18.2 使用，
同一份 Java 源码分别供 JVM 的两种模式使用。
[测试代码全集](examples/vm-performance.md)。它们由脚本直接保存自实际执行输入，源码摘要也在 JSON 内。

### 4.1 ChaLi：0.18.3 与 0.18.2

输入 `10000000`，输出 `49999995000000`。VM 计时由外部 C++ 运行器包围执行过程。

```cpp
int64 mix(int64 x){return x*17;}
void main(){int64 n=inputInt(),i=0;int64 total=0;while(i<n){total+=i;i+=1;}println("{}",total);}
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
因此 JVM JIT/C++ 的这一行主要剩下紧凑的算术循环，不能用它与 ChaLi 的耗时比直接换算单次函数调用开销。

### 5.2 为什么 JVM 不带 JIT 与带 JIT 差异很大

`-Xint` 让 Java 方法按字节码解释执行，不走上述热点即时编译；每轮仍有解释器工作。
`-Xbatch` 允许 JIT，并让编译同步完成；20 次完整预热把测量重点放在热执行状态。
两列采用相同计算源码和结果，但执行技术不同，这正是本页坚持分列的原因。
这组数据不能用于推断 Java 冷启动、首请求或短命令行程序的总延迟。

### 5.3 为什么 ChaLi 的整数与浮点表现不同

ChaLi 的整数循环可以进入[寄存器区域执行核心](../source/stackvm/register_core.hpp)，
在一个批次中连续执行多条内部指令；它仍是解释执行，没有把热点编译成原生机器码。

0.18.3 新增的[浮点融合路径](../source/stackvm/vm_execution.cpp)合并了加载、运算、保存以及部分比较跳转，
减少进入主执行循环的次数，保留原始 PC、逻辑步骤、float32 舍入和观察者回退。
这是浮点步骤相同而耗时明显下降的实现依据；但浮点仍使用槽位绑定、类型检查与融合后的分派，
尚未成为与整数相同的完整浮点区域核心，所以离 JIT/原生紧凑循环仍有明显距离。

### 5.4 为什么 Python 在部分用例中反而更快

动态类型不等于每个负载都更慢。本机 CPython 3.12 在预热后的浮点循环中可观察到
`BINARY_OP_ADD_FLOAT` 自适应专用字节码，避免每次都走最通用的加法路径。
ChaLi 的类型是静态的，但本轮测试的执行器仍承担槽位访问、类型归一化与解释器服务成本；
静态类型本身不会自动把这些成本消除。这能解释为什么不能仅凭“C++ 写的静态语言 VM”就预言它必胜 Python。
本轮未对两套运行时的分配或指令占比做量化剖析。

### 5.5 为什么函数调用和数组仍是 ChaLi 的改进重点

ChaLi 的[调用入口](../source/stackvm/vm_execution.cpp)仍需要处理参数队列、参数归一化、
调用帧与局部槽、返回目的地；小帧已有缓存，仍保留这些服务流程。
Lua 的测试字节码也保留 `CALL`，说明它不是依靠本轮的原生内联取得结果；
两种解释器调用路径的实际成本差异，需要后续用剖析和开关对照进一步拆分。

数组用例也有语义成本差异：ChaLi 经过[下标/指针容量检查与内存服务](../source/stackvm/vm_execution.cpp)，
Lua 使用 table，Python 使用 list，Java 使用 `long[]`，C++ 使用固定长度原生数组。
C++ 源码没有逐次运行时数组边界检查；其他实现的表示和检查策略不同。
因此这行是实际数组写法的端到端表现，不能当成相同底层访存指令的比较，
更不能把取消 ChaLi 检查当作默认提速方案。

## 6. 测量口径与复现

### 6.1 计时包含什么

| 配置 | 计时器 | 计时范围 | 正式测量前预热 |
| --- | --- | --- | ---: |
| 两版 ChaLi VM | `std::chrono::steady_clock` | 已加载字节码的执行准备、执行及最终结果输出 | 1 次 |
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
需要同机的 JDK、Python、GCC/MinGW，以及两版 ChaLi 编译器和对应 VM 运行器。
按[工具链文档](09-toolchain.md)分别构建 `v0.18.3` 与 `v0.18.2`；每个运行器必须使用自己版本的头文件与静态库。

在各自版本源码目录编译运行器：

```sh
g++ -std=c++17 -O2 -DNDEBUG -DSTACKVM_WITH_REFLECTION=1 -DCHALI_CUSTOM_ALLOCATOR=1 -Ilibrary -Isource/stackvm tests/benchmark_vm_program.cpp build/chali-stackvm/libstackvm.a -pthread -o build/vm-program-benchmark.exe
```

Lua 源码沿用[官方 5.5.1 归档](https://www.lua.org/ftp/lua-5.5.1.tar.gz)。
先用 [benchmark_vm_lua.py](../tests/benchmark_vm_lua.py) 完成下载和构建；新脚本再次核对源码包 SHA-256。

```text
1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce
```

运行时应指定自己构建的两版二进制路径。

脚本核对每个输出并保存 JSON。本页不依赖当前仓库中存在这些忽略的二进制；它们需要由对应版本构建获得。
本次改动只重写报告和补充测试脚本，不改变 `v0.18.3` 的编译器、版本号或历史标签。

## 7. 原始数据与旧轮次

- [本轮内联、汇编与字节码诊断证据](benchmarks/vm-baselines-evidence-0.18.3.json)
- 旧的仅 Lua 对照：[第一轮](benchmarks/vm-lua-0.18.3.json)、[重复轮](benchmarks/vm-lua-0.18.3-repeat.json)
- [旧的 switch / 标签分派对照](benchmarks/register-dispatch-0.18.3.json)

旧轮次保留为归档，不混入本页主表。本页结论只针对列出的代码、输入与配置，
不把这些微基准扩展成语言整体性能排名，也不把桌面宿主结果当作真实 embedded Linux 硬件时延。
