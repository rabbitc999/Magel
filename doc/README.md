# Magel 文档

已实现功能、默认配置和平台边界统一见[当前能力与实现边界](17-capabilities.md)。

> **中文（默认）** · [English](en/README.md)
>
> 英文版与中文版内容对应，代码片段两边完全一致、都经过实测运行。中文版优先维护；
> 两处说法有出入时以中文版为准。

**一次编写，处处运行。** 一份逻辑，从 MCU 到云端；值语义、非空引用、无 GC。

Magel 让业务逻辑成为一份可以独立交付的资产：写一次，从桌面到嵌入式板端用同一份逻辑；交付出去的是字节码或本地程序，不是源码；没有垃圾回收器，资源在离开作用域时确定性释放。

当前语言与工具链版本见 [`VERSION`](../VERSION)，变更记录见 [`CHANGELOG.md`](../CHANGELOG.md)。

让 AI 编写 Magel 程序可使用仓库自带的 [Magel 编程 Skill](../skills/magel-programming/SKILL.md)，其中包含语法选择、标准库查阅、宏示例与运行验证步骤。

编译与运行的图解见 [编译过程](../README.md#编译过程) 和 [运行过程](../README.md#运行过程)。

---

## 你想做什么？

| 你是谁 | 从哪开始 |
| --- | --- |
| 想从头学 Magel，一章只讲一件事 | 入门书[《玩转 Magel 语言》](book/README.md)（第 1 章装工具链，一路到综合项目） |
| 第一次听说 Magel | [使命宣言](00-vision.md) → [5 分钟快速开始](01-quickstart.md) |
| 评估它能不能用在我的项目上 | [使命宣言](00-vision.md)（含"不适合谁"）→ [跨平台部署](06-deployment.md) |
| 开始写代码 | [语言速览](02-language-tour.md) → [语言参考](04-language-reference.md) |
| 想弄明白值语义和引用 | [核心概念](03-core-concepts.md)，显式转移所有权见语言参考的 `move` |
| 要把程序交给客户 / 烧进设备 | [交付安全](05-delivery-security.md) → [跨平台部署](06-deployment.md) |
| 想知道标准库有什么 | [标准库概览](07-stdlib-overview.md) → [库函数 API 参考](library/README.md) |
| 想看仓库里有什么能直接跑的 | [示例项目](13-projects.md)（GUI、Web、成绩管理、板端 VM……） |
| 要和 C/C++ 宿主打通 | [宿主集成与 C ABI](08-host-integration.md) |
| 查命令行、配置、测试、多诊断与独立借用检查 | [工具链使用](09-toolchain.md) |
| 在终端查 API，或了解编辑器增量缓存 | [命令行手册](man/README.md) → [LSP 增量提示与缓存](man/lsp.md) |
| 使用 VSCode 的工程提示、标准库文档和在线依赖 | [编辑器插件](../vscode/README.md) |
| 查 `.svmb` 容器与符号压缩 | [15 `.svmb` 二进制格式](15-svmb-format.md) → [16 无名变长符号表](16-compact-symbols.md) |
| 用注解 / 反射，或接宿主内存 | [19 注解与反射](19-reflection.md)、[20 内存分配器](20-memory-allocator.md) |
| 查语法的权威写法 | [EBNF 语法参考](14-grammar-ebnf.md) |
| 想知道这门语言怎么来的 | [99 Magel 的故事](99-magel-story.md) |
| 要升级版本，怕代码突然不编了 | [21 语义冻结清单](21-semantics-frozen.md)（核心契约、兼容性与实现边界） |
| 想参与贡献 / 看路线 | [路线图与生态愿景](10-roadmap.md) |

---

## 文档结构

| 编号 | 文档 | 面向 | 讲什么 |
| --- | --- | --- | --- |
| [00](00-vision.md) | 使命宣言 | 决策者、潜在用户 | 软件交付的三重困境，Magel 的回答，适合谁 / 不适合谁 |
| [01](01-quickstart.md) | 5 分钟快速开始 | 第一次用 Magel 的人 | 安装、第一个程序、常用命令速查 |
| [02](02-language-tour.md) | 语言速览 | 有经验的开发者 | 用可运行的小片段走一遍 Magel 长什么样 |
| [03](03-core-concepts.md) | 核心概念 | 准备深入使用的人 | 值语义、非空引用、无 GC 的资源管理，为什么这样设计 |
| [04](04-language-reference.md) | 语言参考 | 日常写代码的人 | 按主题查阅语法、语义、示例与常见错误（含泛型嵌套、默认类型与单态化边界） |
| [05](05-delivery-security.md) | 交付安全 | 需要对外交付的团队 | 交付形态选择、四步法、逆向成本与边界声明 |
| [06](06-deployment.md) | 跨平台部署 | 做部署的人 | 三种执行方式、桌面 / 嵌入式 / Web、内存池与看门狗 |
| [07](07-stdlib-overview.md) | 标准库概览 | 想知道"能做什么"的人 | 按能力域列出现有模块与核心 API |
| [08](08-host-integration.md) | 宿主集成与 C ABI | 做集成的人 | 胶水绑定、abi 块、类型映射、可空引用 |
| [09](09-toolchain.md) | 工具链使用 | 日常用 magel 命令的人 | 命令行、magel.toml、在线依赖与共享缓存、编译选项、测试、LSP、显示语言 |
| [10](10-roadmap.md) | 路线图与生态愿景 | 贡献者、长期用户 | 短中长期目标与可验证里程碑 |
| [13](13-projects.md) | 示例项目 | 想直接跑东西的人 | 仓库里的应用型与能力型示例：位置、运行命令、验证入口与前置条件 |
| [14](14-grammar-ebnf.md) | EBNF 语法参考 | 查文法细节的人 | 从 `source/magel.gram` 生成的全部产生式与词法登记，权威语法的可读形式 |
| [15](15-svmb-format.md) | `.svmb` 二进制格式 | 做格式分析与逆向评估的人 | 容器与节布局、逐字节解码、逆向成本 |
| [16](16-compact-symbols.md) | 无名变长符号表 | 关注体积与诊断隐私的人 | `--compress-symbols` 的格式、工具与空间对比 |
| [17](17-capabilities.md) | 当前能力与实现边界 | 评估与集成人员 | 语言、VM/AOT、借用、反射、标准库及板端的统一说明 |
| [18](18-panic-trace.md) | panic 与源码行号 | 排查崩溃的人 | 失败输出、函数栈、源码位置与压缩隐藏策略 |
| [19](19-reflection.md) | 注解与反射 | 用注解 / 动态调用的人 | 声明元数据、类型化查询与封闭值动态调用 |
| [20](20-memory-allocator.md) | 内存分配器 | 接宿主内存的人 | 池与容量统计、分配范围检查、外部缓冲区接入 |
| [22](22-tombstone-research.md) | 墓碑机制调研 | 修改运行时的人 | 原存储结构、分配与释放路径、清扫及重启插入点、fn 闭包借用参考 |
| [99](99-magel-story.md) | Magel 的故事 | 好奇这门语言怎么来的人 | 从 Magic-C 到 Magel：撞过哪几次墙、删掉了什么、还剩什么没做完 |
| [21](21-semantics-frozen.md) | 语义冻结清单 | 要采用、或要升级的人 | 核心语义、版本规则与仍需完善的实现边界 |

---

## 配套资源

- [`example/samples/`](../example/samples/) —— 可运行的正式样例，标准输出在 [`example/expected/`](../example/expected/)。
- [`example/samples-error/`](../example/samples-error/) —— 预期报错的样例，诊断文本在 [`example/expected-error/`](../example/expected-error/)。
- [`doc/examples/`](examples/) —— 需要配套工程布局的示例（ABI、SQLite、svmb、GUI）。
- [`doc/examples/`](examples/) 里的配套工程总览见 [13 示例项目](13-projects.md)。
- [项目 README](../README.md) —— 构建、快速使用、调试、配置。
- [入门书《玩转 Magel 语言》](book/README.md) —— 从零开始的 25 章教程与附录，含前后端分离学生系统（配截图）和 H743 解释器实战；完整示例、生成内容与硬件配置按对应回归核验。
- [完整语法参考（EBNF）](14-grammar-ebnf.md) —— 文法的权威形式，由 [`source/magel.gram`](../source/magel.gram) 生成。

## 英文文档

[`doc/en/`](en/README.md) 是英文版，与上面各篇一一对应（含项目 README 的英文版，共 15 篇）：

| 英文版 | 对应这篇中文 |
| --- | --- |
| [../README.en.md](../README.en.md) | [项目 README](../README.md)（构建、快速使用、调试、配置） |
| [en/README.md](en/README.md) | [文档入口](README.md) |
| [en/00-vision.md](en/00-vision.md) | [00 使命宣言](00-vision.md) |
| [en/01-quickstart.md](en/01-quickstart.md) | [01 快速开始](01-quickstart.md) |
| [en/02-language-tour.md](en/02-language-tour.md) | [02 语言速览](02-language-tour.md) |
| [en/03-core-concepts.md](en/03-core-concepts.md) | [03 核心概念](03-core-concepts.md) |
| [en/04-language-reference.md](en/04-language-reference.md) | [04 语言参考](04-language-reference.md) |
| [en/05-delivery-security.md](en/05-delivery-security.md) | [05 交付安全](05-delivery-security.md) |
| [en/06-deployment.md](en/06-deployment.md) | [06 跨平台部署](06-deployment.md) |
| [en/07-stdlib-overview.md](en/07-stdlib-overview.md) | [07 标准库概览](07-stdlib-overview.md) |
| [en/08-host-integration.md](en/08-host-integration.md) | [08 宿主集成与 C ABI](08-host-integration.md) |
| [en/09-toolchain.md](en/09-toolchain.md) | [09 工具链使用](09-toolchain.md) |
| [en/10-roadmap.md](en/10-roadmap.md) | [10 路线图与生态愿景](10-roadmap.md) |
| [en/13-projects.md](en/13-projects.md) | [13 示例项目](13-projects.md) |
| [en/99-magel-story.md](en/99-magel-story.md) | [99 Magel 的故事](99-magel-story.md) |

几点说明：

- **只翻译说明文字，代码片段原样保留**（含片段里的中文注释）。文档承诺"每段代码都实测运行过"，
  校验脚本 `tmp/{tour,core,ref}_check.sh` 是从脚本内的 heredoc 生成片段的；翻了注释两边就对不上，
  这个承诺就弱了。
- **中文版优先维护**。两版说法有出入时以中文版为准。
- **没翻的部分**：底层规格 7 篇（[14](14-grammar-ebnf.md)–[20](20-memory-allocator.md)）、`doc/examples/`（配套工程）、
  `doc/book/`（入门书《玩转 Magel 语言》）。
  英文版里指向这几处的链接仍链到中文原文。
- 根目录 README 保持中文，另有一份逐节对应的英文版 [`README.en.md`](../README.en.md)。
- 英文版内的相对路径多一层（指向 `example/` 写 `../../example/`）。

## 其余入口

- 多值返回与泛型元组：[语言参考 §4](04-language-reference.md#多值返回与泛型元组)，[解释执行 / AOT 回归](../tests/magel_tuple_returns.py)。
- 带值退出与循环穿透：[语言参考](04-language-reference.md#块表达式与插值输出)、[完整示例](../example/samples/break_values.mgl)、[解释执行 / AOT 回归](../tests/magel_block_expression.py)。

- [11 借用控制流与算法验证](11-algorithm-validation.md)：独立数据流检查、链表 / BST / DFS / BFS、NOIP2015 运输计划与暴力对照。

- [12 VM 性能对比](12-vm-performance.md)：Magel 0.18.3 / 0.18.2 的 VM 与 AOT、JVM 不带与带即时编译、Python、Lua、原生 C++ 的同机五类实测；包含完整测试代码、性能差异原因、计时口径和两轮原始数据。

- 静态 SQLite 与 MySQL/MariaDB 连接入口：[SQL 接口](../example/sql-magel/doc/01-sql-library.md)、[SQLite 实库回归](../tests/magel_sqlite.py)、[连接 ABI 夹具回归](../tests/magel_sql_connections.py)、[MySQL/PostgreSQL 实库回归](../tests/magel_sql_live.py)。

- [Any、显式泛型与通用数据工具](22-any-generics-data.md)

- [资源所有权移动](23-ownership-move.md)

- [字面量定型、运算符推导与宏](24-literals-operators-macros.md)

- [执行与交付](25-execution-delivery.md)

- [自定义错误传播与 if 表达式](26-error-propagation-if.md)

- [匹配、插值、泛型与作用域清理](27-match-interpolation-scope.md)

- [返回表达式、具名闭包与字段派生](28-declaration-sugars.md)

- 带值退出的默认移动：[语义与清理顺序](04-language-reference.md#带值退出的移动与清理)、[资源身份和析构回归样例](../example/samples/break_move.mgl)。
