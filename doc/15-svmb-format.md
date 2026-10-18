# .svmb 二进制格式与逆向分析

从 control.mgl 的已知输出开始，再检查其编译产物，才有依据解释指令、标签、类型和调试记录。
本文的逐字节分析针对历史 PESVMB v4 标本，不是当前产物格式；压缩格式见[无名变长符号表](16-compact-symbols.md)。

## 当前格式与历史标本的区别

当前普通容器是 `PESVMB\r\n`、格式版本 `5`；压缩容器是 `MCVM0005`、版本 `5`。
现行加载器要求对应的 v5 格式，旧字节码须从源码重新编译，不能只修改头部版本号。
普通头部仍为 24 字节，保存版本、特征位、payload 长度与 FNV-1a 32 校验。
VM 指令、标签、类型、形参、导出及调试记录之后还有内部执行序列；
反射特征位启用时，内部执行序列后保存反射元数据段。四元式文本不写进 `.svmb`。
权威实现见 [普通容器编解码](../library/stackvm/stackvm.cpp) 和
[紧凑容器编解码](../library/stackvm/parts/symbol_container.cpp)。

下文 v4 的七节布局、标本字节、操作码数、空间比例与历史采集脚本只用于理解旧证据。
现行产物不能直接交给只识别 v4 的标本解析脚本；分析时先核对魔数和格式版本。

## 先知道源程序做了什么

[control.mgl](../example/samples/control.mgl) 累加 1–5，并按负数、零、正数分支。正式输出第一行为 `sum 1..5 = 15`，完整输出见 [control.txt](../example/expected/control.txt)。先运行，再生成两份字节码：

```sh
mkdir -p build/doc-example
./magel svmb example/samples/control.mgl --no-compress-symbols --out build/doc-example/control
./magel svmb example/samples/control.mgl --no-compress-symbols --debug --out build/doc-example/control-debug
```

两份程序的语义相同，第二份额外保存来源位置。观察产物时区分指令、数据、布局与调试记录；“能反汇编”不等于恢复原注释、表达式拼写和全部类型声明。

## 源码行为如何对应字节码

for 初始化、比较、累加和更新分别降级为赋值、条件跳转、算术和回跳；classify 的 if/elif/else 变为条件与返回路径。函数和变量名保存在普通格式的符号信息中，可能降低分析成本；当前源码行号链路可保留 .mgl 来源，但默认不写调试记录。


## 如何阅读下方历史标本

[`examples/svmb/`](examples/svmb/README.md) 保存旧 control.svmb 标本、解析脚本和 evidence 原始输出。标本为 662750 字节，SHA-256 前后缀 CF30A3C8…7ABE；其条目数、调试比例和 9700X 性能属于采样时环境，不是重新编译当前 control.mgl 必须得到的固定值。

复现时应检查当前文件自身的长度、节数、校验及对应源码，不因体积不同就判定格式错误。下面的七节布局、逐字节解码与逆向成本保留为该标本的完整格式参考；源码行号的现行规则见 [panic 调用栈](18-panic-trace.md#magel-行级来源)。

## 1. 结论先给

| 手上有什么 | 能还原到什么程度 | 难度 |
| --- | --- | --- |
| 源码 + 仓库工具 | 全部；`disasm` 一条命令就有汇编清单 | 无 |
| 只有 `.svmb`（`--debug` 构建） | 函数名、变量名、类型、逐条指令、来源四元式行号全在；写个反编译器可得接近源码的伪代码 | 低 |
| 只有 VM 可执行文件 + `.svmb` | 同上——VM 二进制里就带着按操作码顺序排列的助记符表 | 低 |
| 只有 VM 可执行文件（没有 `.svmb`，规范未知） | 先恢复容器布局，再恢复操作码语义；解释器自带 `--trace`/`--debug`，可以动态对照 | 中 |
| 只有一段不认识的字节码，没有 VM、没有样例 | 只能靠统计与差分猜；自描述结构（长度前缀、UTF-8 名字块）仍能救回大半 | 中高 |
| 剥掉调试节的 `.svmb`（缺省；兼容别名 `--no-debug`） | 少一层“这段指令对应源码第几行”，符号名、类型、导出照旧 | 低（体积省 62.3%，见 [8.5](#85-剥离调试节实测还剩什么)） |
| 连名字也要剥掉的 `.svmb` | 只剩指令流与容器结构 | 增加语义恢复工作（现有 --compress-symbols，见 [无名变长符号表](16-compact-symbols.md)） |

关键一句话：**这份格式的设计目标不是抗逆向**。变量的寻址单位是**名字**、类型表要参与
浮点判定、调试节可以整节带上（缺省不带、加 `--debug` 就有），三者叠加让**带调试节**的
`.svmb` 可读性接近"带符号表的未优化字节码"，而不是"编译后的机器码"。

## 2. 容器格式

整个文件只有一个头部加一段 payload：

| 偏移 | 长度 | 字段 |
| --- | ---: | --- |
| 0x00 | 8 | magic `PESVMB\r\n`（`50 45 53 56 4d 42 0d 0a`） |
| 0x08 | 2 | u16 版本 = `4`（历史标本；现行加载器要求 `5`） |
| 0x0A | 2 | u16 flags：普通程序为 `0`，反射能力为 `0x0002` |
| 0x0C | 8 | u64 payload 字节数（小端） |
| 0x14 | 4 | u32 校验和（FNV-1a 32，作用在 payload 上） |
| 0x18 | N | payload |

实测样本的头 24 字节：

```text
50 45 53 56 4d 42 0d 0a  04 00  00 00  c6 1c 0a 00 00 00 00 00  45 ce 8e f6
P  E  S  V  M  B  \r \n  v4     flags0   payload = 662726 字节      FNV-1a = 0xF68ECE45
```

payload 里的基本编码原语，全部小端：

| 原语 | 编码 |
| --- | --- |
| `u8` / `u16` / `u32` / `u64` | 定长小端 |
| `text` | u32 长度 + UTF-8 字节 |
| `varText` | ULEB128 长度 + UTF-8 字节 |
| `count` | u32 |
| `varUInt` | ULEB128 |
| `varInt` | ZigZag + ULEB128 |

校验和是**无密钥**的 FNV-1a（初值 `2166136261`、乘子 `16777619`），它挡的是文件损坏，
不是篡改：改一个字节会得到 `二进制程序校验和不匹配`，截断尾部 10 字节会得到
`二进制程序长度字段不匹配`，但按同样算法重算校验和后就能通过。**不要把它当安全边界。**

## 3. payload 的七节

顺序固定：程序名 → 指令 → 标签 → 类型 → 形参 → 导出 → 调试（v4 起）。实测条目数与体积：

| 节 | 条目 | 字节 | 占比 | 内容 |
| --- | ---: | ---: | ---: | --- |
| 程序名 | 1 | 11 | 0.0% | 固定 `magel` |
| 指令 | 16 728 | 164 122 | 24.8% | 每条一条指令记录（见第 4 节） |
| 标签表 | 1 263 | 31 340 | 4.7% | `名字 → 指令下标`（函数/方法/内部跳转标签） |
| 类型表 | 1 135 | 39 717 | 6.0% | `名字 → 类型名 / 位宽 / 有无符号 / 是否浮点 / 字节数` |
| 形参表 | 265 | 13 696 | 2.1% | `函数名 → 形参名列表` |
| 导出表 | 35 | 767 | 0.1% | 公开函数名 |
| 调试信息 | 8 204 | 413 073 | **62.3%** | `指令下标 → 函数 / 文件 / 行 / 四元式行` |

三点值得注意：

- **调试节占了大头**（六成以上），体积主要由它决定。
- `control.mgl` 只有 29 行，却有 16 728 条指令、1 263 个标签、5 726 个不同字符串：在标本生成时
  （2026-10-01，标准库尚未拆分），编译器把整个标准库（含矩阵、复数、正则、大整数/大浮点后端）
  连同用户代码一起编进同一份字节码，所以 `.svmb` 带着标准库的全部符号，这也是"名字泄漏"看起来
  格外夸张的原因。标准库后来拆成核心 `source/std/lang.mgl`（自动加载）+ 按需 `import std::xxx;` 后，只用核心
  prelude 的程序，`.svmb` 就只带核心符号了；标本保留它生成时的形态。
- 调试节**给每条指令都存了一份编译时的 `.quad` 路径字符串**。换个目录用同样的源码重编，
  七节条目数全都一样、调试节之前的 payload 逐字节一致，但文件会长出"路径长度差 × 8204 条"
  那么多字节（实测 33 × 8204 = `+270 732`）。这条差值的吻合反过来证明了调试节的内容，
  也是"路径会烙进产物"这件事的直接证据（见 [`evidence-01-container.txt`](examples/svmb/evidence-01-container.txt)）。

## 4. 指令记录编码

一条指令 = `u8 操作码` + `u8 操作数位图` + 按位图出现的选择字段：

| 位 | 含义 | 编码 |
| --- | --- | --- |
| 0 | `format` 存在 | u8（打印格式，等于默认的 Signed 时不写） |
| 1 | 有符号/浮点标志 | u8（bit0 无符号，bit1 浮点） |
| 2 | 有整数字段 | `varInt` |
| 3 | 有浮点位模式 | u64（`float64` 的原始位） |
| 4 | 四元式下标与指令下标不同 | `varUInt` |
| 5 | 有名字 | `varText` |
| 6 | 有第二个名字（返回值目的地等） | `varText` |
| 7 | 有文本值（`PushText` 的字符串） | `varText` |

操作码本身是 1 字节枚举，共 **54** 个（`PushInt`=0 … `StoreAt`=53）。实测这个程序用到
37 种，最频繁的是 `Load`(5 397) / `Store`(3 212) / `Declare`(1 325) / `PushInt`(1 199)。

`control.svmb` 开头 6 条指令的逐字节解码（记录从文件偏移 39 开始 = 24 头部 + 11 程序名 + 4 计数）：

| 下标 | 偏移 | 解码 |
| --- | --- | --- |
| 0 | 39..45 | `op=48(Call)` `mask=00100000` `name='main'` |
| 1 | 46..47 | `op=50(Halt)` `mask=00000000`（两个字节就够） |
| 2 | 48..58 | `op=1(PushDouble)` `mask=00001100` `intValue=6` `doubleBits=0x400921fb54442d18`（= 3.1415926535897931） |
| 3 | 59..75 | `op=4(Store)` `mask=00100000` `name='std::math::PI'` |
| 4 | 76..83 | `op=5(Declare)` `mask=00100000` `name='i$451'` |
| 5 | 84..85 | `op=0(PushInt)` `mask=00000000`——立即数为 0 时连值字节都省了 |

这和 `magel-stackvm disasm` 的输出逐条对得上（`0: CALL main` / `1: HALT` /
`2: PUSH 3.1415926535897931` / `3: STORE std::math::PI` / `4: DECLARE i$451`）。
`intValue` 是"随操作码变含义"的整数字段（`(decl …)` 存位宽、`(checkptr …)` 存元素步长），
上面第 2 条里的 `6` 与该指令来源的四元式下标一致（`.quad` 第 6 条正是
`(:=, 3.1415926535897931, _, std::math::PI)`）。

## 5. 里面能看到什么

| 信息 | 证据（control.svmb 实测） |
| --- | --- |
| 公开函数名 | 导出表 35 项：`classify`、`main`、`std::math::abs$double`、`assert$bool$string` … |
| 全部函数与方法名 | 标签表 1 263 项：`Arena.allocate`、`Matrix__f64.solve`、`Complex__f64.leftMul` … |
| 泛型单态化后的签名 | 名字里带类型参数与重载区分，如 `Matrix__f64.leftMul$double`、`std::math::max$int$int` |
| 局部变量名 | 指令里直接出现 `i$451`、`sum$452`、`value$194`（`$` 后是作用域内的唯一编号，同一名字在不同位置编号不同） |
| 变量类型 | 类型表：`('i', 'i64', 64, 0, 8)`、`('__magel_enum_arg_Option__int_Value_0', 'i64', 64, 0, 8)` |
| 枚举/结构体内部命名 | `__magel_enum_arg_IoError_ReadFailed_0`、`Option__char` 这类编译器合成名 |
| 控制流结构线索 | 编译器自带标签名 `else_1`、`endif_2`、`__magel_loop_101`、`__magel_brk_103` 直接标出循环与分支边界 |
| 逐条指令的源码位置线索 | 调试节 8 204 条：函数名 927 个 + `.quad` 行号（实测 40~10 675） |

也就是说：反汇编是零成本的（`disasm` 一条命令），而**恢复"谁的代码"也几乎零成本**——
符号名、类型、循环边界都还在。

## 6. 里面看不到什么

| 缺失 | 说明 |
| --- | --- |
| 注释 | 显然不在 |
| 源表达式结构 | 已经降级成栈式指令，`sum = sum + i` 只剩 `LOAD/LOAD/ADD/STORE` 加临时变量 `t2` |
| `.mgl` 文件名与行号 | 调试节的 `file` 实测是 `…/control.quad`、`line` 恒为 0。`.mgl` 行号链路**还没打通**，现在能拿到的只有 `.quad` 行号 |
| 真正的源码作用域 | 类型表是 `名字 → 类型` 的**全局映射**（1 135 条、无重名），同一名字在别处被重新声明会覆盖：`sum` 在 `main` 里是 `int64`，表里却是 `float64`，因为 `source/std/lang.mgl` 里另有 `float64 sum = 0.0;`。所以类型表能当符号表用，但不能当成"每个使用点的精确类型" |
| 字符串字面量 | 不在 `.svmb` 里。数据区是独立的 `.data.bin`（5736 字节），而且占位符已经编译成格式化模板：源码 `"sum 1..5 = {}"` 在数据区里是 `sum 1..5 = %d`。只看 `.svmb` 拿不到用户字符串 |
| 类型/函数的完整定义 | 只有名字、位宽、字节数、形参名；结构体字段布局在 `.layout` 里，不在 `.svmb` 里 |

## 7. VM 自身：原生反汇编

把 VM 编译出来再看它自己，会得到一份很有意思的对照（两档都是本机 2026-10-01 的构建）：

| 构建 | 文件大小 | 符号情况 |
| --- | ---: | --- |
| MSVC x64 Release | 609 792 字节 | COFF 符号表只有 7 项 ≈ 无符号 |
| GCC x64 Release | 1 148 954 字节 | `nm -C` 3 631 个符号，含 `stackvm::Machine::run(stackvm::Program const&, …)` 这类完整 C++ 名字 |

`objdump -d` 能直接定位解释器主循环（`.text` 约 798 KB，`Machine::run` 是个巨大函数），
反汇编出来就是普通 C++ 代码：`push %r15` / `sub $0x598,%rsp` / `mov %rdx,%rbx` …，
读起来不比读一份没注释的 C++ 难多少。

**更要紧的是助记符表**：`.rdata` 里存着一串按操作码顺序排列的助记符模板，`strings` 就能捞出来：

```text
PUSH  / LOAD  / STORE  / DECLARE  / ALLOC / FREE / DROP / CHECK  / CHECKPTR  /
CALLPTR / NEG / NOT / ADD / SUB / MUL / DIV / MOD / SHL / SAR / SHR / AND / OR / XOR /
EQ / NE / LT / LE / GT / GE / LTU / LEU / GTU / GEU / I2F / F2I / MULWIDE / DIVWIDE /
JMP  / JZ  / JNZ  / PRINT … / READ  / GETC  / PUTC / PARAM  / CALL  / RET / HALT / ADDR  /
LOADAT / STOREAT
```

实测（见 [`evidence-05-vm-native.txt`](examples/svmb/evidence-05-vm-native.txt)）：GCC 档 exe 里
能找到一条 **57 项**的连续助记符串（含 `PUSH`、`CALLPTR`、`PRINT` 的重复项，去重后正好是反汇编器
用到的全部 51 个助记符），表里的顺序就是操作码编号顺序；MSVC 档也有一条 54 项的连续串，
`DECLARE` / `FREE` / `DROP` 不在这段里——链接器把它们重排或合并掉了。
这条最要紧的地方在于——**只要拿到 VM 可执行文件，"未知格式"这个前提就没了**：
操作码编号 → 助记符的映射可以从 exe 里直接恢复，再配合 `.svmb` 里自描述的名字表，
反汇编器的三个要件（指令边界、操作码名、名字表）全部到手。

另外，VM 命令行自带三种取向的观察手段，对逆向同样好用：

```sh
magel-stackvm disasm prog.svmb          # 静态：得到可读汇编清单
magel-stackvm run prog.svmb --trace     # 动态：逐条打印执行的指令
magel-stackvm run prog.svmb --debug     # 动态 + 位置：按四元式行号打印
magel-stackvm check prog.svmb           # 只校验容器与程序合法性
```

## 8. 逆向难度分级

### 8.1 有源码（基准）

`magel-stackvm disasm` 一条命令得到 16 728 行汇编清单，标签、变量名、操作码全在。
这是"格式完全已知"的上界，后面每一档都在往这个方向上靠。

### 8.2 只有 `.svmb`

格式是**自描述**的：`u32 长度前缀 + UTF-8` 的名字块、ULEB128 的计数字段、按位图出现的
可选字段。不需要任何外部文档，从 magic 与长度字段切出 payload，再按"长度前缀 + 文本"扫一遍，
就能把导出表、标签表、类型表分离出来；有了标签表就有了函数边界，有了操作码字节的直方图
就能判断哪些是加载/存储/跳转。第 11 节给了一份 40 行的解析器，这就是全部前置成本。

真正的工作量在"把栈式指令还原成表达式与控制流"：需要模拟求值栈、把 `t1/t2` 临时变量折叠回
表达式、用跳转目标重建基本块。而这部分也有捷径——`else_1`/`endif_2`/`__magel_loop_101` 这些
标签名本身就是控制流注释。综合判断：**比反编译优化过的 native 代码容易一个档次**，
最接近的类比是"未优化、且带着 LocalVariableTable / LineNumberTable 的 JVM `.class`"，
但比 `.class` 还多一样东西：类型表里带位宽与符号性。

### 8.3 只有 VM 可执行文件 + `.svmb`

难度**基本不变**（见第 7 节）：助记符表在 exe 里，导出/标签名字在字节码里。
唯一损失是"容器字段的确切语义"，而这一块靠试错（改字节看报错、截断看长度校验）很快能补齐。

### 8.4 只有一段不认识的机器码

这是真正困难的一档：既不知道容器布局，也不知道操作码语义，还没有 VM 可以对照。可做的
步骤与抓手如下（按性价比排序）：

1. **切容器**：找固定 magic 与长度字段。自描述格式在这里帮了大忙——u64 长度 + u32 校验
   一类的"看起来像长度"的数值，用 `文件长度 − 偏移 == 该字段` 一验即中。
2. **找校验算法**：先用"改一字节 → 报校验错"确认存在校验，再拿常见 32 位哈希（FNV-1a、
   CRC32、Adler32）去凑；目的是能自由改字节做实验，不必真的破解。
3. **找字符串表**：按"小端 u32 长度 + 紧跟等长 UTF-8"扫描高频模式，能把名字块抽出来。
   名字块一旦出来，结构就基本透明了：导出名、函数名、变量名、类型名彼此的长度分布与
   排列方式不同，很容易分组。
4. **建立操作码表**（最硬的一块）。没有 VM 时的杠杆：① 直方图与上下文——哪种字节后面总
   跟着一段文本、哪种总跟着两字节跳转偏移；② **同格式样本差分**——同一份程序改一行源码
   再编一份，两份字节码的差异区间直接圈出"这一行对应的指令"，反复做就能标注大量操作码；
   ③ 有 VM 时换成动态差分：把它 `--trace`/`--debug`/单步跑起来，改一个操作码看状态机怎么变。
5. **语义还原**：栈式字节码的栈深可以静态推导（每条指令的净压栈量固定），据此能重建表达式
   树；跳转目标聚类得到基本块，再按"回边/嵌套关系"恢复 `if`/`while`/`for`。

直观结论：**只有在"既没有参考实现、又没有同格式样本"时，分析成本才会显著跳升**，
而且跳升主要来自"凭空猜操作码语义"这一步。本项目的格式恰好让这一步变得不必要：
字节码自描述、名字齐全、VM 又是开源的。

### 8.5 剥离调试节：实测还剩什么

> 开关口径（现在的写法）：产物**缺省不带调试信息**，用 `--debug` 才带上。本文早期记录里
> 用的 `--no-debug` 仍然被接受，是"不带"的显式兼容别名，含义与缺省一致；下面表格里的
> `--no-debug` 一列就是"缺省（不带）"。

调试节是这份格式里唯一**可以整节不要**的部分，这已经是开关：

| 入口 | 带上调试节 | 缺省（不带） |
| --- | --- | --- |
| Magel 编译器 | `magel run app.mgl --emit --svmb --debug` | 不传 `--debug`（或兼容别名 `--no-debug`） |
| 项目级默认 | `magel.toml` 里 `[magel] debug = true`（缺省 `false`，见 [09 工具链 §3](09-toolchain.md)） | 缺省 / `debug = false` |
| StackVM CLI | `magel-stackvm compile app.quad --debug` | 不传（或兼容别名 `--no-debug`） |
| 独立 exe 导出 | `magel-emit … --debug`（才写 `program.debug.add(…)` 与 `.quad` 路径） | 缺省不写 |

写出来的仍然是合法的 v4——调试节在场、但 0 条。同一份 `control.mgl` 的实测对照：

| 节 | 带调试节（`--debug`） | 缺省（`--no-debug`） |
| --- | ---: | ---: |
| 程序名 | 11 | 11 |
| 指令 | 164 122 | 164 122 |
| 标签表 | 31 340 | 31 340 |
| 类型表 | 39 717 | 39 717 |
| 形参表 | 13 696 | 13 696 |
| 导出表 | 767 | 767 |
| 调试信息 | 413 073（8 204 条） | 4（0 条） |
| **合计** | **662 750** | **249 681（−62.3%）** |

五节的条目数一个不少，payload 里调试节之前的部分**逐字节相同**；`check` 依旧报告
“16728 条栈指令，1263 个标签，1135 个变量声明，35 个公开入口”。

**省下来的是位置，不是身份。** 剥离之后：

- 程序照跑，输出一字不差（`sum 1..5 = 15` …）。
- `disasm` 还是原来那份：`STORE std::math::PI`、`DECLARE i$451`、`DECLARE sum$452` 全都在。
- 丢的是位置：`run --debug` 只给得出 `调试[0] ?`，并提示“这个程序没有带调试信息”，
  出错位置与源码级调用栈一并没了（VM 是优雅降级，不会崩）。

所以这一刀砍掉的是“这段伪代码属于源码第几行”，而不是“这个函数叫什么、这个变量什么类型”。
把上一节那张分类表换成实测结论：

| 内容 | 体积占比 | 能否剥离 | 剥掉的代价 |
| --- | ---: | --- | --- |
| 调试节 | 60.9% | 能，就是本节这个开关 | `--debug`、出错位置、源码级调用栈失效；名字一个不少 |
| 变量名字符串 | 指令流的一部分 | 不能 | `Load` / `Store` / `Declare` 按**名字**寻址（`findSlot(instruction.name)`），删了程序不跑 |
| 类型表 | 6.0% | 不能 | 参与执行语义（判断 `Load` / `Store` 走整数还是浮点） |
| 导出 / 标签名 | 5% 左右 | 不能 | 调用、跳转、导出接口都靠标签名解析 |

**这份格式的信息量不是“忘了剥”，而是设计使然**——变量按名字寻址、类型表参与判定，
决定了名字必须留在产物里。真想进一步脱敏，得先把 VM 的寻址单位从“名字”改成“槽号”、
把类型表外置，那是语言/虚拟机层面的改动，会同时打破 `.svmb` 向后兼容与调试能力，
不该顺手做。

顺带一个能解释“调试节为什么这么大”的实测：同一份源码、同一个编译器，只把 `--out` 的路径
从 `tmp/svmb-study/control` 换成 `tmp/svmb-strip/full`（短 3 个字符），**带调试节**的产物就
从 662 750 变成 638 138 字节——每条指令都存一份路径字符串，路径每多一个字符，文件就多
8 204 字节。剥离版没有这个性质：`control-stripped.svmb` 换任何目录、任何文件名重新生成，
字节完全一致（SHA-256 `08554811…`，见
[`evidence-07-strip-debug.txt`](examples/svmb/evidence-07-strip-debug.txt)）。

不经过 Magel 编译器、直接用 `magel-stackvm compile` 打包一份 `.quad` 时同理：同一份
`control.quad` 缺省（不带调试节）是 270 143 字节，加 `--debug` 才 773 456 字节（这里的
exports 是全部 1263 个标签，所以两份都比 Magel 前端产出的略大）。

## 9. 容器能力标志

普通容器使用 `0x0002` 表示反射能力；其余未定义标志必须拒绝加载。历史 `0x0001` 载荷加密位已移除，旧加密文件须从源码重新导出。校验和仅用于发现损坏，不提供身份认证。

## 10. 只有独立 exe 的时候

本节内嵌 VM 导出的测量属于历史记录。当前已删除该导出模式，现行原生产物使用 AOT；`.svmb` 格式及前文的二进制分析不受影响。

`magel-emit --target cpp` 会把 VM 源码与程序一起内嵌。实测导出 `control.mgl`：
生成源码 11 127 109 字节（内嵌 VM 408 669 + 大整数后端 52 542 + 大浮点 21 744 字节），
`g++ -std=c++17 -O0` 编出 **15 441 180 字节**的 exe。在它身上做 `strings` 等价扫描：

| 探测串 | 带调试表 | `magel-emit --no-debug` |
| --- | --- | --- |
| `main`、`classify` | 命中 | 命中 |
| `i$451`、`sum$452`、`value$194` | 命中 | 命中 |
| `std::math::abs$double` | 命中 | 命中 |
| `__magel_loop_101`、`else_1`、`endif_2` | 命中 | 命中 |
| `control.quad`（生成时的 `.quad` 路径） | 命中 | **未命中** |
| `sum 1..5 = %d`（已格式化的字面量） | 命中 | 命中 |
| `PUSH `、`DECLARE `（助记符表） | 命中 | 命中 |
| 文件大小 | 15 441 180 | 13 414 172（−2 027 008，−13.1%） |

长度 ≥ 6 的可读字符串 42 965 条、其中不同 41 323 条（原始探测输出见
[`evidence-06-exported-exe.txt`](examples/svmb/evidence-06-exported-exe.txt)）。结论：**独立 exe
不但没有变难，反而把三样东西塞进了同一个文件**——带符号名的字节码、带助记符表的解释器、以及
数据区的格式化字符串。只拿到 exe 的逆向者不需要先分离字节码，用 `strings` + 反汇编 VM 主循环
就能开工。

导出侧同一个开关（缺省就不写；要显式说"不带"可用兼容别名 `--no-debug`，见
[06 部署](06-deployment.md)）：不写 `program.debug.add(…)`，于是产物里既没有位置表、
也没有生成期的 `.quad` 路径字符串——上表那 2 MB 就是它。
但对比两列可以看到：**除了路径，名字一个都没少**，`i$451`、`sum$452`、`classify` 在剥离版里
照样一搜就中。这与 `.svmb` 侧是同一个结论：能剥的是“位置”，剥不掉的是“身份”。

## 11. 从产物到伪代码的实际路径

按上面几节，一条"最短路径"大致是：

| 步骤 | 手段 | 难度 |
| --- | --- | --- |
| 1 切出字节码 / 汇编清单 | `disasm`，或按第 2、3 节自己解析 | 极低 |
| 2 恢复操作码名 | exe 里 `strings` 捞 54 个助记符，按序对齐即可 | 低 |
| 3 恢复函数边界与调用图 | 标签表 + `Call`/`Ret` 指令 | 低 |
| 4 恢复变量与类型 | 类型表 + 指令里的 `名字$编号` | 低 |
| 5 恢复控制流 | 跳转目标聚类成基本块；`else_*`/`endif_*`/`__magel_loop_*` 名字直接帮忙 | 中 |
| 6 恢复表达式 | 模拟求值栈、折叠临时变量 `t1/t2` | 中 |
| 7 恢复源码位置 | 本文旧标本仅记录四元式行号；当前 `--debug` 已支持 `.mgl` 来源，见 [panic 调用栈](18-panic-trace.md) | 取决于产物是否保留调试信息 |
| 8 还原结构体/枚举定义 | 需要 `.layout`（不在 `.svmb` 里）或从类型表的名字推断 | 中 |

## 12. 最小解析器

不需要任何依赖，40 行左右就能把七节拆开（[`examples/svmb/svmb_dump.py`](examples/svmb/svmb_dump.py)
是这次用的完整版）：

```python
MAGIC = b"PESVMB\r\n"

class R:
    def __init__(self, data): self.d, self.p = data, 0
    def take(self, n):
        out = self.d[self.p:self.p + n]; self.p += n; return out
    def u8(self): return self.take(1)[0]
    def u16(self): return int.from_bytes(self.take(2), "little")
    def u32(self): return int.from_bytes(self.take(4), "little")
    def u64(self): return int.from_bytes(self.take(8), "little")
    def uleb(self):
        shift = value = 0
        while True:
            b = self.u8(); value |= (b & 0x7F) << shift
            if not b & 0x80: return value
            shift += 7
    def text(self): return self.take(self.u32()).decode()
    def vtext(self): return self.take(self.uleb()).decode()

def parse(path):
    blob = open(path, "rb").read()
    r = R(blob)
    assert r.take(8) == MAGIC
    version, flags, size, checksum = r.u16(), r.u16(), r.u64(), r.u32()
    name = r.text()
    code = r.u32()                      # 指令数
    for _ in range(code):               # 每条指令：op + 操作数位图 + 按位取字段
        op, mask = r.u8(), r.u8()
        if mask & 1: r.u8()
        if mask & 2: r.u8()
        if mask & 4: r.uleb()
        if mask & 8: r.u64()
        if mask & 16: r.uleb()
        for bit in (32, 64, 128):
            if mask & bit: r.vtext()
    labels = [(r.text(), r.u64()) for _ in range(r.u32())]
    types = [(r.text(), r.text(), int.from_bytes(r.take(4), "little", signed=True),
              r.u8(), r.u64()) for _ in range(r.u32())]
    params = [[r.text() for _ in range(r.u32())] for _ in range(r.u32())]
    exports = [r.text() for _ in range(r.u32())]
    debug = []
    if version >= 4:
        for _ in range(r.u32()):
            debug.append((r.uleb(), r.vtext(), r.vtext(), r.uleb(), r.uleb()))
    return dict(name=name, code=code, labels=labels, types=types,
                params=params, exports=exports, debug=debug)
```

## 13. 复现命令

下面第 2 步之后用的都是**已入库的标本**（[`examples/svmb/`](examples/svmb/README.md)，
含 `.svmb`、`.data.bin`、`.layout`、`.quad`），所以不需要先编译；第 1 步只是说明标本怎么来的。

```sh
# 1. 标本是怎么来的（`--out` 决定烙进调试节的路径，换个目录会改变产物大小，不改程序本体）
magel/magel run magel/example/samples/control.mgl --emit --svmb --out tmp/svmb-study/control

# 2. 静态看汇编清单（0 基指令下标，和 --trace 的下标一致）
magel-stackvm disasm magel/doc/examples/svmb/control.svmb | head -40

# 3. 动态看执行与位置
magel-stackvm run magel/doc/examples/svmb/control.svmb \
    --data magel/doc/examples/svmb/control.data.bin \
    --layout magel/doc/examples/svmb/control.layout --limit 12 --trace
magel-stackvm run magel/doc/examples/svmb/control.svmb \
    --data magel/doc/examples/svmb/control.data.bin \
    --layout magel/doc/examples/svmb/control.layout --limit 12 --debug

# 4. 篡改 / 截断，观察完整性检查
python -c "d=bytearray(open('magel/doc/examples/svmb/control.svmb','rb').read()); d[100]^=0x5A; open('tmp/tampered.svmb','wb').write(bytes(d))"
magel-stackvm check tmp/tampered.svmb                 # 二进制程序校验和不匹配

# 5. 结构统计（解析器同目录）
python magel/doc/examples/svmb/svmb_dump.py magel/doc/examples/svmb/control.svmb

# 6. VM 自身的原生反汇编（GCC 档带符号；先按 magel/benchmarks/compare-vm.ps1 构建）
objdump -d --start-address=0x14006be70 --stop-address=0x14006bf50 \
    tmp/vm-9700x-pool/gcc64/magel-stackvm.exe
nm -C tmp/vm-9700x-pool/gcc64/magel-stackvm.exe | wc -l      # 3631

# 7. 导出独立 exe，再看它泄漏什么
build/magel-emit/magel-emit --quad magel/doc/examples/svmb/control.quad \
    --data magel/doc/examples/svmb/control.data.bin \
    --layout magel/doc/examples/svmb/control.layout \
    --out-dir tmp/svmb-study/export --target cpp-aot \
    --glue magel/source/glue/cpp_glue.cpp --stack-slots 128
g++ -std=c++17 -O0 -I magel -I magel/library/stackvm \
    tmp/svmb-study/export/control.target.cpp -o tmp/svmb-study/export/program.exe
tmp/svmb-study/export/program.exe --memory-size 536870912

# 8. 剥离调试节（第 8.5 节）
magel/magel run magel/example/samples/control.mgl --emit --svmb --no-debug --out tmp/svmb-strip/stripped
# 或者项目级：magel.toml 的 [magel] 下写 debug = false
# VM 侧同理：magel-stackvm compile app.quad --no-debug --out app.svmb

```

本文所有原始输出都由一个脚本重新生成，改完标本或换了 VM 就重跑一遍：

```sh
python magel/doc/examples/svmb/collect.py --parser magel/magel.exe \
    --vm tmp/vm-9700x-pool/x64/Release/magel-stackvm.exe \
    --vm-opcode tmp/vm-9700x-pool/gcc64/magel-stackvm.exe \
    --exported tmp/svmb-study/export/program.exe
```

`magel-stackvm` 用 `magel/library/stackvm` 的 CMake 工程构建（MSVC Win32/x64、GCC x64 三档），
`tmp/vm-9700x-pool/` 下就是本文用的两份；`.svmb` 可以跨 32/64 位加载，
但可执行文件必须与宿主位数一致。

七份原始输出对应本文各节：`evidence-01` 容器、`evidence-02` 七节、
`evidence-03` 指令编码、`evidence-04` 运行时与完整性、`evidence-05` VM 原生侧、
`evidence-06` 导出 exe、`evidence-07` 剥离调试节前后对照。
标本（带/不带调试节各一份）、解析器、基准源码与证据的说明见
[`examples/svmb/README.md`](examples/svmb/README.md)。

## 14. 一句话总结

普通未压缩的 `.svmb` 是自描述且带符号表的字节码容器，由 StackVM 执行；源码位置默认不保存，仅在 `--debug` 时写入。用 `disasm` 看它就是汇编清单；不用任何工具硬啃也能按"长度前缀 + 文本块 + 操作码位图"
把七节拆开。真正的逆向成本不在格式，而在最后那一步——把栈式指令重新组织成人类可读的
表达式与控制流，而这一步因为标签名、变量名、类型表的存在而大幅缩短。
本文历史标本中，**调试节**占 62.3%，省去调试记录后文件约小六成；这不是所有程序的固定比例。调试开关为：
**默认就不带**，要带上才写 `--debug`（Magel 编译器 / VM CLI）或 `[magel] debug = true`
（项目级；`--no-debug` 是"不带"的兼容别名），代价只有一个——位置信息没了；普通格式仍保留变量名与类型表。当前也支持无名变长符号容器，加载器会识别相应格式；具体能力与限制见[无名变长符号表](16-compact-symbols.md)。
