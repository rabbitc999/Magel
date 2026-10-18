# 命令行手册

查询 API 必须显式写 `api` 子命令：`magel man api std` 列出标准库模块，`magel man api std::text::Text` 查看类型。省略 `api` 的 `magel man std` 或 `magel man std::text::Text` 会报参数错误并给出正确命令。

`magel man` 从可执行文件中的标准库索引查询 API，无需网络、Python 或仓库目录。库 API 的完整网页参考仍在 [doc/library](../library/README.md)；本目录记录命令行为、数据结构及 [LSP 增量提示与缓存](lsp.md)。[English](en/README.md)

## 查询

```sh
magel man
magel man api std::text
magel man api std::text::Text
magel man api std::text::Text::split
magel man api --search delimiter
magel man api --search split --exact
magel man --json std::text::Text::split
magel man api std::text::Text::split --run
magel man --lang en std::text::Text::split
```

| 形式 | 内容 |
| --- | --- |
| 无目标 | 按核心与类型、容器与迭代、文本与数据、数学与数值、输入输出与集成、并发与事件、内存与系统分组列出模块 |
| 模块 | 模块说明及类型、函数、常量、字段，一行一条签名 |
| 模块::类型 | 类型说明及字段和方法 |
| 模块::符号或模块::类型::方法 | 签名、参数含义与默认值、返回值、错误边界、调用形式和已有示例；重载逐条显示 |
| `--search 关键词` | 搜索模块名、符号名、参数名、说明、注释和示例代码；默认大小写不敏感的子串匹配 |
| `--exact` | 配合搜索，按完整字段且区分大小写匹配；例如 `split` 可以匹配方法的末段名称 |
| `--json` | 只输出 JSON，适合脚本；失败诊断也进入 diagnostics |
| `--lang zh|en` | 覆盖工具链显示语言，作用于标题和诊断 |
| `--run` | 执行当前目标关联的第一个完整示例，在文档之后附加结果 |

目标支持标准库限定名，也支持省略 `std::` 的模块前缀。拼错目标时显示最多八个候选并以状态码 1 退出。`--search` 与具体目标不能同时使用。

交互终端使用颜色；`NO_COLOR` 环境变量关闭颜色。超过 24 行且 PATH 中存在 `less` 时使用 `less -R` 分页。重定向或 `--json` 输出时使用普通 UTF-8，便于保存和管道处理。Windows 通过控制台接口开启 UTF-8 和虚拟终端颜色。

库说明目前复用中文原文，`--lang en` 会翻译界面标题及诊断，并注明说明原文语言。JSON 的 `language` 是显示语言，`sourceLanguage` 是说明原文语言；不存在的翻译不会替换成另一套 API 数据。

## 示例执行

示例来自既有正式样例，附带输入、命令行实参、编译选项和预期输出。`--run` 把完整样例写进临时目录，用当前 magel 编译运行；执行目录和输出文件也在该临时目录，结束后清理。例子可能包含模块的多个 API，不会把网页中的单行调用片段当成独立程序。

没有关联示例时返回 MGL1702；编译、执行失败或超过 60 秒返回 MGL1703。JSON 的 `execution` 包含 `example`、`exitCode`、`stdout`、`stderr`，stdout 保留运行平台的换行形式。目标不存在返回 MGL1701，并在 `candidates` 和诊断 notes 中列候选。无效命令参数以状态码 2 退出。

## JSON 结构

结构版本为 1，字段采用与编译诊断相同的小驼峰风格。顶层公共字段如下；查询类别另附相应数组。

| 字段 | 类型与含义 |
| --- | --- |
| `schemaVersion` | 整数，当前为 1 |
| `language`, `sourceLanguage` | 字符串，显示语言与说明原文语言 |
| `kind`, `target` | 字符串，查询类别及解析后的目标 |
| `modules` | 模块数组；含 name、title、documentation、capability、sourceLanguage、examples |
| `symbols` | 符号数组；含 module、name、kind、owner、signature、type、documentation、parameters、examples 等现有索引字段 |
| `diagnostics` | 诊断数组，沿用 code、severity、message、span、source、notes |
| `groups` | 模块列表按能力域分组时提供 |
| `query`, `candidates`, `execution` | 分别在搜索、未找到目标、执行示例时提供 |

函数的 `parameters` 提供 name、type、documentation、default；无默认值时 default 为空字符串。`returns` 提供 type 与 documentation，`errors` 提供模块的边界说明、前置断言和 panic 信息。示例对象提供 id、source、code、flags、arguments、input、expected、language；完整查询将索引中的示例 ID 解析为对象。调用者应允许出现新的可选字段。

## 数据维护

```sh
magel man --update
python tools/library_reference.py --check
sh gen.sh
make -j4
```

`--update` 是开发路径，需 Magel 源码仓库及 Python，等同于运行 [tools/library_reference.py](../../tools/library_reference.py)。它重新生成 [库参考](../library/README.md)、API 索引和 LSP 共用的声明数据；生成后需重新构建 magel 才会更新内嵌内容。普通 `man` 查询不调用 Python。

CI 通过 `--check` 检查生成数据同步，通过 [tests/magel_man.py](../../tests/magel_man.py) 验证模块、类型、方法、全文搜索、JSON、双语界面、候选和示例执行。`magel help` 显示工具链命令用法，`magel man` 查询库 API。

[返回文档索引](../README.md)

## 内嵌书稿、关键字与 AI SKILL

`magel man book` 将书稿 Markdown 和图片导出到当前调用目录的 `book/`，保留图片相对路径并列出章节，`magel man book 6` 查看章节；也可使用 `A`、`B`、文件名或完整标题。`magel man keyword` 列出关键字，`magel man keyword break` 从权威文法与语言参考显示语法、说明及示例。`magel man skill` 输出完整 AI SKILL，可重定向保存为 `SKILL.md`；VS Code 插件的 `skills/magel-programming/SKILL.md` 与它逐字一致。

上述数据均在构建时压缩内嵌，查询不访问仓库或网络。书稿和 SKILL 目前使用中文原文，图片在终端 Markdown 中保留引用；配图版请阅读书稿 PDF。三类查询均支持 `--json`，不支持 `--run` 或 `--exact`。不存在的章节或关键字返回 MGL1701。源文件变更后运行 `python tools/embed_manual.py`，CI 用 `--check` 检查同步，`gen.sh` 的资产生成也会刷新该数据。

导出只更新内嵌书稿同名文件，不删除其他文件；目标路径必须保持在调用目录的 book 文件夹内。

单章终端输出将图片转换为 `[图片]\(images/文件名)` 链接；JSON 与导出的 Markdown 保留原始图片语法。

单独执行 `magel man` 与 `magel man help` 均显示用法；`magel man api` 显示 API 分类，`magel man api XXX` 查询内嵌 `doc/library/` 的 API。


`magel man macro` 查看宏定义、调用、有序多括号、独立变长参数与跨行大括号帮助；内容内嵌在编译器中，也支持 `--json`。
