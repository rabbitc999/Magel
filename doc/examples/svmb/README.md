# `.svmb` 证据包

这一目录是[`.svmb` 二进制格式解剖与逆向难度](../../15-svmb-format.md)
的证据来源。文档里的每个数字都由 [collect.py](collect.py) 跑出来，**不手写**；工具缺失时对应
那一节写“跳过”，不伪造内容。

## 标本

| 文件 | 说明 |
| --- | --- |
| `control.svmb` | 标本本体，662 750 字节，SHA-256 `CF30A3C8718DFB2C564D347AD35DE2D991F64FB9B983228A5EDBEEE93A757ABE` |
| `control-stripped.svmb` | 同一份源码加 `--no-debug` 的产物，249 681 字节，SHA-256 `08554811E6DF5ECCF564B72F3CDACCFD2179DA6C696A06599EDBB33A6007476D`。它**不含任何路径字符串**，所以在哪个目录、用什么文件名重新生成，字节都完全一致 |
| `control.quad` | 与标本同时生成的四元式文件（调试节里的行号就是它的行号） |
| `control.data.bin` | 独立数据区（5736 字节，字符串字面量在这里，不在 `.svmb` 里） |
| `control.layout` | 数据区布局表（结构体字段与方法名在这里） |
| `svmb_dump.py` | 最小 `.svmb` 解析器，纯标准库 |

源程序是仓库里的 [`magel/example/samples/control.mgl`](../../../example/samples/control.mgl)（29 行），
生成命令（在仓库根目录执行）：

```sh
magel/magel run magel/example/samples/control.mgl --emit --svmb --out tmp/svmb-study/control
```

标本是 2026-10-01 用当时构建的 `magel` 生成的。**再次生成不会与原文件逐字节相同**：
调试节给每条指令都存了一份编译时的 `.quad` 路径字符串，换个目录编译，产物大小就会变
（实测路径长 33 字节 × 8204 条 = `+270732` 字节），但程序本体逐字节一致。
`collect.py` 会把这件事验给你看，所以标本不必因为"再生成对不上"而重新入库。

## 证据文件

| 文件 | 内容 |
| --- | --- |
| `evidence-01-container.txt` | 头部 24 字节逐字段、SHA-256、同源码重编的逐节比对与体积差解释 |
| `evidence-02-sections.txt` | 七节的条目数/字节数/占比，导出表、标签/类型/形参/调试样本，字符串统计 |
| `evidence-03-instructions.txt` | 前 12 条指令的逐字节解码、操作码直方图、带 `$` 作用域的变量名 |
| `evidence-04-runtime.txt` | `disasm` / `run --trace` / `run --debug` / 完整输出，以及损坏、截断、改完重算校验和三种实验 |
| `evidence-05-vm-native.txt` | 两档 VM 的符号数、`.text` 大小、`Machine::run` 原生反汇编开头、从 exe 里捞出的助记符串 |
| `evidence-06-exported-exe.txt` | 独立 exe 里的名字泄漏探测与可读串统计 |
| `evidence-07-strip-debug.txt` | `--no-debug` 前后逐节对照（净省 62.3%）、剩下的名字、丢掉的位置、以及"剥离版与构建目录无关"的复现 |

## 重新生成

```sh
python magel/doc/examples/svmb/collect.py \
    --parser magel/magel.exe \
    --vm       tmp/vm-9700x-pool/x64/Release/magel-stackvm.exe \
    --vm-opcode tmp/vm-9700x-pool/gcc64/magel-stackvm.exe \
    --exported tmp/svmb-strip/exp-full/program.exe \
    --exported-strip tmp/svmb-strip/exp-none/program.exe \
    --tests tmp/vm-9700x-pool/gcc64/magel-stackvm-tests.exe
```

`--vm` 是跑 `disasm` / `run` / `check` 用的 VM，`--vm-opcode` 是带符号的第二档构建
（两档都看，才能说明“符号在不在”这件事的差别），`--exported` / `--exported-strip` 是
`magel-emit` 导出的两份 exe（带调试表 / `--no-debug`）。所有路径都可以省，
省掉的节会写成“缺失，跳过”。

## 生成这批证据时的环境

- 机器：AMD Ryzen 7 9700X，Windows 11 26200。
- VM：MSVC 19.44.35228.0 x64 Release（609 792 字节）、GCC 11.2.0 x64 Release
  （1 148 954 字节），构建方式见 [`magel/benchmarks/compare-vm.ps1`](../../../benchmarks/compare-vm.ps1)。
- 编译器：`magel/magel.exe`，2026-10-01 构建。
- 导出 exe：`build/magel-emit/magel-emit` 加 `g++ -std=c++17 -O0`（编导出源码要带
  `-I magel -I magel/library/stackvm`）。带调试表那份 15 441 180 字节，`magel-emit --no-debug`
  那份 13 414 172 字节；`evidence-06` 里两份的探测结果并列，可以直接看到“剥了路径、
  名字还在”。
