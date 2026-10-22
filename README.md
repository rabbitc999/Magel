# ChaLi 语言

> **一次编写，处处运行。**
> 一份逻辑，从嵌入式 Linux 到云端；值语义、非空引用、无 GC。
>
> <sub>Write once, run anywhere. · 一度書けば、どこでも動く。</sub>

**中文（默认）** · [English](README.en.md)

ChaLi 是一门静态类型、值语义优先、不带 GC 的通用编程语言，提供借用来源与控制流检查。它借用 C 的表达式与块语法，但类型、所有权与运行时行为由 ChaLi 自行定义，面向"同一份程序从桌面一直到嵌入式 Linux"的场景。

ChaLi 编译器由本仓库的 `source/chali.gram` 经 [ParseEngine](https://github.com/rabbitc999/ParseEngine) 生成前端，把 `.chl` 源码降级为 StackVM 四元式，再组装为 VM 指令。程序可在内存中执行，也可以保存为 `.svmb` 字节码，交给共享 VM 在目标宿主上运行。

当前已实现能力、默认配置与平台边界统一见[能力说明](doc/17-capabilities.md)，版本以 [`VERSION`](VERSION) 为准。

## ChaLi 是哪五个字母

语言名不是缩写拼出来的好听词，每个字母锁一条不可回退的契约：

| 字母 | 含义 | 契约 |
| --- | --- | --- |
| **C** | Copy（深拷贝值语义） | 赋值就是彻底的复制，绝不共享内存。大对象必须显式用引用传。 |
| **H** | Hygienic（卫生宏） | 在编译期做安全的代码替换，绝不意外捕获或污染外部变量。 |
| **A** | Any（万能盒子 / 类型擦除） | 没有泛型约束时，可以装下任何类型的数据。 |
| **L** | Lifetime（生命周期 / 借用检查器） | 引用永远不能活得比它的数据源更久，否则编译直接报错（`CHL1204`）。 |
| **I** | Interface（接口 / 运行期多态） | 定义一组共同的“契约”，运行期根据对象的实际类型进行分派。 |

每条契约的可跑判据与「什么算违反」见[名称冻结：ChaLi 五个字母](doc/40-name-freeze.md)。

## 编译过程

条件预处理之后，文法生成的前端构建 AST；卫生宏在语义分析期间展开，并与类型推导配合。名称、类型、泛型和独立借用检查完成后，生成并优化四元式，再组装 VM 指令。
`.svmb` 保存 VM 指令、预先降低的内部执行序列及必要元数据；四元式文本不进入字节码文件。

![ChaLi 编译流程：源码、条件预处理、解析、语义期宏展开、借用检查、四元式优化与 VM 装配](doc/images/chali-compilation.png)

## 运行过程

源码执行和字节码执行最终进入同一套 VM：绑定符号与入口、按函数分配连续槽位后，
进入独立整数寄存器核心，连续执行区域内的运算、复制、跳转、整数内存访问、下标检查与参数传递；
内存访问复用现有校验服务，函数调用复用已返回的小帧，其他复杂操作继续使用运行时服务。
调试、动态值或精确边界要求逐条执行时使用基础指令路径。

![ChaLi 运行流程：VM 内部指令、精确执行回退](doc/images/chali-execution.png)

配图由 [`tools/make_pipeline_diagrams.py`](tools/make_pipeline_diagrams.py) 生成，运行
`python tools/make_pipeline_diagrams.py` 可重建中英文 PNG 和 SVG。

## 文档

| 文档 | 面向 | 内容 |
| --- | --- | --- |
| [文档入口](doc/README.md) | 所有人 | 按「你想做什么」跳转到下面任意一篇 |
| [00 使命宣言](doc/00-vision.md) | 决策者、潜在用户 | 软件交付的三重困境与 ChaLi 的回答，含「不适合谁」 |
| [01 快速开始](doc/01-quickstart.md) | 第一次用的人 | 安装、第一个程序、常用命令速查 |
| [02 语言速览](doc/02-language-tour.md) | 有经验的开发者 | 14 段可运行片段走一遍 ChaLi 长什么样 |
| [03 核心概念](doc/03-core-concepts.md) | 准备深入使用的人 | 值语义、非空引用、无 GC 的资源管理 |
| [04 语言参考](doc/04-language-reference.md) | 日常写代码的人 | 按主题查阅语法、语义与常见错误 |
| [05 交付安全](doc/05-delivery-security.md) | 需要对外交付的团队 | 四种交付形态、四步法、逆向成本与边界声明 |
| [06 跨平台部署](doc/06-deployment.md) | 做部署的人 | 源码运行与字节码部署、桌面 / 嵌入式 Linux、资源与边界 |
| [07 标准库概览](doc/07-stdlib-overview.md) | 想知道「能做什么」的人 | 标准库模块按能力域速查 |
| [库函数 API 参考](doc/library/README.md) | 查阅库 API 的人 | 模块的声明、重载、参数、返回值、错误边界与完整示例 |
| [08 宿主集成与 C ABI](doc/08-host-integration.md) | 做集成的人 | `abi` 块、宿主胶水绑定、类型映射、可空引用 |
| [09 工具链使用](doc/09-toolchain.md) | 日常用 `chali` 的人 | 场景指令、选项、`chali.toml`、报错呈现、调试 |
| [17 当前能力](doc/17-capabilities.md) | 评估与集成人员 | 实现能力、默认配置、宿主/目标宿主及工具边界 |
| [10 路线图](doc/10-roadmap.md) | 贡献者、长期用户 | 短中长期目标与「明确不做的事」 |
| [99 方块里的 ChaLi 语言](doc/99-chali-in-blocks.md) | 喜欢比喻、想先建立直觉的人 | 用方块世界讲 MC（Magic-C）：值语义、非空引用、无 GC、迭代器、穷尽 match |

**英文版**：[`doc/en/`](doc/en/README.md) 维护对应的功能说明；未提供完整译文的底层规格从英文入口链接中文版。

- 中文说明优先维护，中英文共有的代码片段保持一致。
- 当前能力与默认配置见中英文能力页；标准库目录以 API 索引为准。
- 本文件、`README.en.md` 和 `doc/README.md` 提供语言切换入口。

配套资源：

- [`example/samples/`](example/samples/) —— 可运行的正式样例，标准输出在 [`example/expected/`](example/expected/)。
- [`example/samples-error/`](example/samples-error/) —— 预期报错的样例，诊断文本在 [`example/expected-error/`](example/expected-error/)。
- [`doc/examples/`](doc/examples/) —— 需要配套工程布局的示例（ABI、SQLite、svmb）。
- [完整语法参考（EBNF）](doc/14-grammar-ebnf.md) —— 文法的权威形式。

旧的开发者向 36 章已删除，其中属独立规格的 7 篇（[14 EBNF](doc/14-grammar-ebnf.md)–[20 内存分配器](doc/20-memory-allocator.md)）
迁入 `doc/` 并改写为当前口径，其余见 Git 历史。

## 构建

在项目根目录执行：

```sh
sh gen.sh          # 首次构建先生成解析器，需要 ParseEngine 文法生成器
make               # 构建 chali 与 build/chali-frontend/libparser.a
./chali example/samples/hello.chl
sh check.sh        # 运行正常与错误样例
```

首次构建先执行 `sh gen.sh`，再执行 `make`。解析器源码（`chali_parser.cpp` /
`chali_parser.hpp` / `chali_parser_user.h` / `chali_main.cpp`）是生成物，不纳入版本管理。

首次构建以及修改下面这些真源后需要重新生成；提交真源，不提交解析器生成物：

```sh
sh gen.sh          # 用 IELR 从 chali.gram 重新生成前端（需要 [ParseEngine](https://github.com/rabbitc999/ParseEngine) 文法生成器）
```

- `source/chali.gram` / `source/chali_token.gram`（文法）
- `source/chali_semantics.cpp` / `source/semantics/*.cpp`（`%epilogue include` 展开进 `chali_parser.cpp`）
- `source/chali_driver.cpp`（`%main include` 展开进 `chali_main.cpp`）

> 忘了跑 `gen.sh` 是最容易踩的坑：`chali_main.cpp` 是 `source/chali_driver.cpp` 的**生成副本**，
> 只 `make` 不会重新生成它，于是你会看到"改了没生效"。

`gen.sh` 需要一份 ParseEngine 文法生成器。最省事是把它放到项目根目录 `./ParseEngine[.exe]`；
也可以让它待在旁边的 ParseEngine 仓库里（`gen.sh` 会自动找 `../ParseEngine/build/bin/`）。

### 编入注解与反射

默认构建同时编入注解与反射前端、VM 运行时支持：

```sh
make                # 默认 CHALI_WITH_REFLECTION=1 STACKVM_WITH_REFLECTION=1
make reflection     # 保留的显式启用入口
```

反射是语言的固定组成，**不可裁剪**：`CHALI_WITH_REFLECTION` 与 `STACKVM_WITH_REFLECTION`
固定为 `1`（`features.mk` 里不是裁剪项，与 `NO_CHALI_*` 那套可选能力无关），反射前端与运行时
始终编入，编译流程始终执行完整的反射处理。

单次源码编译默认生成反射元数据及对应产物特征位；`--no-reflection` 不写这份元数据并拒绝程序里的
注解与反射使用 —— 它只决定这次的产物，**不减少编译期工作量**（元数据与查询源码那一轮照跑）。
部署到未编入反射的目标宿主 VM 时，必须使用 `--no-reflection`。
`make` 已把两项钉死为 1；只有直接 `-D` 手编才能关掉反射，那条路只用于验证编译器自身，不是受支持的裁剪配置。

## 快速使用

```sh
./chali run  example/samples/hello.chl        # 解释运行（默认）
./chali svmb example/samples/hello.chl --out ../tmp/hello   # 生成 .svmb
./chali new  hello-chali                       # 新建应用工程
```

`chali` 是统一工具链入口：前端、两档 C/C++ 导出、工程生成与清理都在同一程序里。更多子命令与 `--mode` 见 [工具链使用](doc/09-toolchain.md)。

### 调试

编译器本体就是调试适配器：

```sh
./chali --dap example/samples/hello.chl     # 按 DAP 在 stdio 上收发，供编辑器接入
```

VS Code 装了 `vscode/` 下的插件后，在「运行与调试」里选 **ChaLi** 即可直接在 `.chl` 上打断点
（插件拉起 `chali --dap <文件>`）。支持断点、继续、步进 / 步过 / 步出与调用栈；程序输出走
`output` 事件显示在调试控制台。变量面板暂为空（VM 还没有读栈帧槽的接口）。

### 报错

语法错误给到**文件、行号、列号**，并回显出错那一行、按出错位置的长度画波浪号：

```text
语法错误：example/samples-error/syntax_extern_impl_removed.chl 第 2 行第 8 列：期望 标识符 / 常量(const) / 可变(mut) / 新建(new) 等 17 个之一，实际是 实现(impl)
 2 | extern impl Value : Some = binding;
   |        ~~~~
```

- 波浪号的宽度就是出错 Token 的长度；制表符按 4 列制表位铺平，中文按 2 列算，所以列不会歪。
- 出错位置落在 `import` 引入的模块里时，指向**真正写错的那个文件**，不是展开后的主文件。
- 不再显示 Token 内部名：`SEMI` → `分号(;)`、`IMPL` → `实现(impl)`、`VARIABLE` → `标识符`。
- 报错文案跟随显示语言（`CHALI_LANG=zh|en` 或 `--lang`），同一处错误分别为：

  ```text
  期望 标识符 / 常量(const) / 可变(mut) / 新建(new) 等 17 个之一，实际是 实现(impl)
  expected one of identifier / constant(const) / mutable(mut) / new(new) (17 candidates), found impl(impl)
  ```

  语义错误（类型不匹配、未定义符号等）目前仍是中文——那些文案写在编译器核心里，
  不在报错呈现层。

## 语言速览

```c
struct Item { int64 value; };

int64 main() {
    List<Item> items;
    Item item;
    item.value = 7;
    items.push(item);
    println("{}", items.len());
    return 0;
}
```

值语义是默认：赋值、传参、返回都复制；要共享就显式取引用（`T const&` 只读、`T mut&` 可写），没有指针与地址算术。没有 GC，资源在离开作用域时确定性释放。标准库自带容器、迭代器、JSON/TOML、正则、路径与进程、网络、SQL、线性代数、并发等，详情见 [标准库概览](doc/07-stdlib-overview.md)。

## 配置

项目可用 `chali.toml` 设置运行方式、编译器路径与搜索目录；命令行参数优先。示例：

```toml
[chali]
run = "interpret"
opt_level = 2
main = "src/main.chl"
```

`[chali] main` 指定工程主文件：**不给源文件**执行时用它当入口（即构建项目）；
命令行显式给了文件就是单文件编译，不看主文件。`chali new` 生成的工程默认带这一行。

## Linux 部署

桌面和嵌入式 Linux 共用普通操作系统宿主与 VM 核心。宿主及原生依赖按目标架构构建，
部署字节码、数据和布局文件。通用 `--memory-size` 设置内存预算，模块能力由宿主注册的胶水决定。
详细流程见[部署说明](doc/06-deployment.md)。

## 目录结构

```text
chali/
├── chali.gram / chali_token.gram   文法：词法声明与产生式
├── gen.sh / Makefile               构建与自测
├── source/                         编译器前端、导出器与胶水
│   ├── std/                        标准库模块（.chl）
│   ├── glue/                       宿主侧 C++ 胶水
│   ├── allocator/                  桌面分配器
│   ├── lsp/ dap/                   编辑器集成
│   └── generator/            生成独立编译器工程的入口
├── library/                        可复用 C++ 库与目标宿主
│   ├── stackvm/                    VM 核心（桌面与目标宿主共用）
│   ├── bignum/ json/ toml/ sqlite/ console/ lztext/ …
│   └── …
├── doc/                            面向使用者的文档（00–20、99）
│   ├── en/                         英文版（入口 + 00–11、13、99，共 15 篇）
│   └── library/                    库函数 API 参考（按模块分页）
├── example/                        示例（samples、错误样例、calculator 等）
├── tests/                          Python 验收脚本
├── tools/                          图标与总览图等辅助脚本
└── sql-chali/                      SQL 最小用法示例
```

完整说明见对应目录下的 README 与上面的[文档](#文档)章节。

## 许可证

MIT，见 `LICENSE`。

解析器生成源码、头文件与报告统一位于 `source/`；对象文件与解析器静态库位于 `build/chali-frontend/`，不会散落到仓库根目录。
