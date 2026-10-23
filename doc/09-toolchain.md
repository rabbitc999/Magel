# 09 工具链与命令行

ChaLi 只通过共享 VM 执行程序。源码经过前处理、语法与语义检查，再降级为四元式和 VM 指令；支持导出四元式与 `.svmb` 字节码。

## 执行与检查

```sh
chali run app.chl
chali check app.chl
chali check app.chl json
chali emit app.chl out build/app
chali svmb app.chl out build/app
```

直接传入 `.chl` 文件也会运行，文件后的参数传给程序；显式 `run` 可配置产物前缀。
`emit` 输出 `.quad`、`.data.bin` 和 `.layout`；`svmb` 还输出 VM 字节码。运行时资产与标准库内嵌在编译器中。

## 开发工具

```sh
chali new demo name demo
chali test tests backend interpret
chali fmt check src
chali deps lock project . offline
chali man List
chali lsp
chali gen compiler out build/compiler
chali clear out build/compiler
```

生成器用于独立编译器工程；宿主集成通过共享 VM、宿主注册表和字节码加载接口完成。
可恢复的运行失败、调用栈及宿主错误按 VM 的统一协议传播。

## 默认构建与优化

反射是语言的固定组成、不可裁剪：前端及运行时始终编入，编译流程始终执行完整的反射处理；`--no-reflection` 只决定这次产物写不写反射元数据，不减少工作量。四元式默认执行完整优化流程；VM 默认启用最高可用执行优化。
`--memory-size` 与 `--step-limit` 设置 VM 资源预算；`--threads` 启用隔离线程。编译器帮助列出当前支持的选项。

后端调试开关只影响诊断输出与执行路径，不改变程序语义，分两组：

| 开关 | 作用 |
| --- | --- |
| `CHALI_NO_OPT=1` | 跳过语义级四元式化简（`quad_optimizer`） |
| `CHALI_DUMP_OPT=1` | 把化简后的四元式打到 stderr |
| `CHALI_NO_MACHINE_OPT=1` | 关掉机器级优化整层（内联 / 折叠传播 / GVN / DCE / 强度削减 / LICM） |
| `CHALI_NO_OPT_INLINE` / `_STRENGTH` / `_FOLD` / `_GVN` / `_DCE` / `_LICM` | 只关掉其中某一遍，定位「哪一遍改了语义」时逐遍二分 |
| `CHALI_DUMP_MACHINE=1` | 把机器级优化后的四元式打到 stderr |
| `CHALI_DUMP_IR=1` | 打出规范化中间表示（基本块、前驱后继、定义-使用与回边） |
| `CHALI_DUMP_FRAME=1` | 打出每个函数的帧布局（符号 → 槽号）与分配自检结论 |
| `CHALI_NO_SLOT_REUSE=1` | 退回「形参在前 + 声明顺序一人一槽」，不看活跃性 |
| `CHALI_ONE_SLOT_PER_SYMBOL=1` | 一人一槽但仍走分配器，用来分辨「别名」与「编号」两类问题 |
| `CHALI_NO_REGISTER_CORE=1` | 关掉寄存器执行核、退回通用栈解释 |

双核、复用前后、优化前后三组对照都必须逐字节一致（文本除 CRLF/LF 与可忽略空白外），这条差分由 `tests/vm_differential.py` 守：它拿同一个源码跑五档开关（默认 / 栈解释核 / 不复用槽位 / 关机器级优化 / 只关内联），比对退出码、标准输出、诊断与调用栈；唯一放宽的是调用栈里的 `ip`——字节码长度被优化改变时它必然变。

四元式条数、指令选择覆盖率与帧槽复用前后都能在 `--stats` 里看到：

```text
机器级优化：四元式 16810 → 16628（折叠 2，传播 4，合并 1，削减 16，外提 0，内联 6 处（+114 条），删除 296）
指令选择：目标指令 3062 条，覆盖栈指令 10492/26865（39.1%，通用回退 16373 条）
帧槽分配：362 个函数，符号 1622 个 → 槽 1346 个（省掉 276 个）
```

项目配置的运行方式为 `interpret` 与 `svmb`。
配置见[工程与包管理](13-projects.md)，字节码约定见[SVMB 格式](15-svmb-format.md)。

## 源码模块与增量构建

`source/` 的 C/C++ 文件按模块职责使用下划线名称；类、成员和函数保持既有命名。
标准库胶水统一位于 `source/glue/std_xxx.{hpp,cpp}`：公共头暴露绑定入口，平台适配、记录布局和
算法实现在对应 `.cpp` 独立编译。`std_binding.hpp` 只提供注册表前置声明；模板需要的
结构化数据与类型转换定义保留在头文件中。VM 核心继续使用通用宿主协议。

泛型、闭包、工程清单和诊断分别由 `chali_generic.cpp`、`chali_lambda.cpp`、
`chali_generic_closure.cpp`、`chali_package.cpp` 和 `chali_report.cpp` 编译。
`macro/` 独立编译卫生绑定、结构化替换、宏注册、正则匹配、原始片段和编辑器宏头模型；
`syntax/` 提供普通源码分组及常量求值，`editor/` 输出真实编译分析。
AST 声明位于 `chali_ast.hpp`；`semantics/` 中的符号登记、卫生展开、泛型闭包和程序降级
由权威文法装配进生成解析器。语言服务位于 `lsp/`，工程生成器位于 `generator/`。

修改文法后执行 `sh gen.sh`，然后执行 `make -j4`。普通 C++ 实现变化由 `.d` 文件追踪实际包含关系。
`make` 自动检查标准库、胶水及手册的内嵌输入，内容由 `tools/embed_stdlib.py` 生成并单独编译为
`chali_stdlib_embedded.o`；压缩数据不再被公共头包含。生成输入使用时间戳记录，内容未变化时不重复写入
生成源码。独立导出的编译器工程也将这些实现作为单独的 CMake 源文件链接。

宿主接入胶水时同时包含 `chali_glue.hpp` 与所需 `std_xxx.hpp`，并链接对应 `.cpp` 或宿主记录库。
只有绑定接口的头文件不会隐式带入全部实现；完整构建命令见[同步回调示例](../example/native-callbacks/README.md)。

## 标准库预编译单元

`import` 标准库时，标准库（`std::lang` 加本次实际 import 的 `std::xxx`）在合并源码里占绝大部分，
而这部分每次编译都一样。编译器在标准库段末尾插一行分界标记，把整份源码切成「标准库段 + 用户段」
两次解析，并把标准库段那份解析产物（语法节点、类型池、标识符池、顶层声明）序列化成**预编译单元**
写进缓存目录；下次遇到同一份标准库段时直接装回竞技场，跳过标准库段的词法 / LR 解析与节点构建。

- 缓存目录默认 `build/chali-stdlib-cache`；`--stdlib-cache-dir DIR` 或环境变量
  `CHALI_STDLIB_CACHE_DIR` 指定别处。
- `--no-stdlib-cache` 或 `CHALI_NO_STDLIB_CACHE=1` 关闭；关闭后与一次性解析整段源码等价。
- 单元键 = 标准库段文本哈希 + 编译器版本 + 构建戳（`__DATE__ __TIME__`）：改了 `source/std/*.chl`
  （重建内嵌资产后）或换了编译器二进制，缓存自动失效。写盘走原子替换；读不回来只是"这份缓存用
  不了"，静默退回全量解析，结果与不用缓存完全一致。
- `--stats` 打印「命中 / 回写 / 未启用」。

反射是语言的固定组成、不可裁剪，缓存也不会跳过它：元数据与查询源码那一轮每次编译都照跑，
所以 `import std::json` 这类程序的总耗时仍主要由反射决定（见[当前能力](17-capabilities.md)）。

## 基础库独立编译单元

`library/simple_ast` 的类型、节点构造、四元式生成和数据导出分别独立编译；`library/bignum` 的整数乘除法、数论、位操作、转换和十进制方根也按职责分离。公开头保留接口，Make 自动依赖记录各单元的实际包含关系。导出编译器和内嵌资产包含全部实现，独立构建使用各库 CMake 目标。具体文件职责见 [AST 库说明](../library/simple_ast/README.md) 与 [大数库说明](../library/bignum/README.md)。
