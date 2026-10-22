# Magel —— 代码编写要求与提交要求

本文件只写**通用的代码编写要求与提交要求**；具体功能的设计与实现进度以代码、文档和
回归测试为准。已实现能力写入对应功能文档，历史记录由 Git 提交及 `CHANGELOG.md` 条目
承载，不把待办计划当作当前能力说明。

根目录 `TASK.md` 若存在，记录当前已授权任务及验收清单；实施过程中同步维护，不把计划写成已实现能力。
只有其中要求全部实现并完成验收、文档和产物同步后，才删除 `TASK.md`；未完成时保留未完成项，不以结束一次会话代替任务完成。
删除前逐项核对验收证据；删除 `TASK.md` 的改动随最终交付提交，不得用删除任务文件掩盖未完成工作。

## 硬性要求

1. **必须写完整的文法文件**：`source/magel.gram` 是 Magel 的**权威语法定义**，产生式要覆盖
   完整语法结构（文档 / 块 / 语句 / 参数 / 列表 / 嵌套），关键字用
   `%token NAME "字面量"` **逐个登记**，标点/字符串/数字/标识符各走
   `%token` / `%token regex` / `%identifier`，注释走 `%trivia`，空白走 `%space`；
   语义动作（`$$` / `$n`）只调用 `%code` 里的语义层。
   禁止用一个大 `KW` / 一个万能 Token / 一条"任意 token 流"的产生式把整门语言糊过去，
   也禁止把每个关键字只当成待提取的词素堆进正则——关键字、块、命令都要能在产生式里被点名与组合。
2. **解析流程放进解析器生成器**：块栈、逐行折叠、驱动 `Parser::Engine::runText`
   的流程用 `%code` / `%epilogue include` 内嵌进生成产物，不手写解析驱动。
   `magel_parser.cpp` / `magel_parser.hpp` / `magel_parser_user.h` / `magel_main.cpp`
   都是 `sh gen.sh` 生成到 `source/` 的产物，不入库；对象文件和静态库写入 `build/magel-frontend/`；
   `Makefile` 是手写的，`gen.sh` 不会覆盖它。
3. **引用到的库各自拷一份进本项目**：`library/` 下的库是 ParseEngine 仓库 `library/` 同名目录的副本；
   控制台头 `library/console/utf8.hpp` 在本仓库独立维护。
   本项目要能单独克隆、单独构建，**不依赖 `library/`**。
   **`library/` 副本只在本仓库演进，不同步回 `D:/Desktop/Workspace/ParseEngine/library/`**：
   两个仓库的库已经分叉，往回同步属于多余动作，也不要顺手改外面那份（反之亦然）。
   只有 `library/console/utf8.hpp` 这类本仓库独立维护的文件例外。
4. **固化生成资产**：标准库 `.mgl` 与生成器运行时资产在构建期内嵌成 C++ 源
   （[`tools/embed_stdlib.py`](tools/embed_stdlib.py) → `source/magel_stdlib_embedded.cpp`），
   运行时不读磁盘，保证单个可执行文件可用。改了任何 `.mgl` 或胶水头后必须重跑内嵌、重建编译器，再跑测试。
5. **完整文档注释**：头文件的公开类型/函数与 `.cpp` 中的所有具名函数定义都要中文
   Doxygen 文档注释，包括内部辅助函数、成员函数、构造/析构函数及测试入口。
   注释写在对应定义前，使用 `/** ... */`，`@brief`、每个 `@param`、
   `@return` / `@retval`、`@note` 各占独立一行；无参数时不写空的 `@param`，
   无返回值及构造/析构函数不虚构返回值。注释必须解释实际功能、参数用途与返回含义，
   不能只复述函数名或填占位语句，不使用紧凑形式或 `///` 代替函数文档块。
6. **统一 VM 核心**：`library/stackvm/` 是桌面与嵌入式 Linux 共用的 VM 核心。
   目标平台需具备受支持的 C++17 工具链与操作系统宿主服务；构建宏、资源预算和胶水
   按实际宿主配置维护，不承诺单片机或裸机支持。
7. 文档索引用 [`doc/README.md`](doc/README.md)；改动完成后 `git add -A` → commit →
   `git push origin master`（远端 <https://gitee.com/rabbitc999/magel.git>）。
   仓库里另配的 `github` 远端（<https://github.com/rabbitc999/Magel.git>）默认不动：
   向它推送任何分支或 tag 之前，**必须先取得用户明确同意**；未经同意不得推送，
   也不要把它当成 `origin` 的自动镜像顺手同步。
8. **程序与插件分别记录版本**：根目录 [`VERSION`](VERSION) 是 Magel 程序版本，当前为
   `0.24.2`；[`vscode/package.json`](vscode/package.json) 的插件 `version` 独立保持 `1.0.0`，
   `magelCompilerVersion` 必须与 `VERSION` 一致。内嵌编译器报告程序版本，VSIX 清单和包名
   使用插件版本。交付前在 `vscode/` 执行 `npm run package`，生成
   `vscode/dist/magel-editor-1.0.0.vsix`，核对两种版本及内嵌编译器字节。
9. **编译器默认编入反射**：构建、测试及交付 Magel 编译器时，默认同时启用
   `MAGEL_WITH_REFLECTION=1` 与 `STACKVM_WITH_REFLECTION=1`，以根目录 `features.mk`
   的默认值为准；不得只开启前端或只开启运行时。本规则包括 VS Code 插件内嵌的编译器。
   显式验证无反射裁剪配置时才覆盖为 `0`。
   编入反射能力后，沿用前端默认启用反射的行为；单次源程序编译需要关闭时，
   显式使用 `--no-reflection`，不要把省略 `--reflection` 当作关闭反射。
10. **VM 与四元式默认最高优化**：日常构建、运行、测试及交付默认使用当前实现支持的
    最高优化模式；新增优化级别或优化开关时同步维护默认配置，不得默认降级。
    VM 默认启用寄存器执行核心、执行准备与融合、跳转折叠、整数专用操作码、
    基本块执行和运行时服务快速路径；编译器支持时启用标签地址分派，不支持时沿用
    对应平台的最佳可用分派方式。四元式默认执行完整的 `stackvm::optimize` 优化流程，
    使用优化器的标准迭代预算（当前 `maxRounds = 4`），不要减少优化轮次或跳过优化阶段。
    数组及标量快速路径保留存活内存与边界检查。只有用户明确要求，或为调试、差分测试、
    性能对照验证时才显式降低优化；关闭开关不得成为日常默认配置。
11. **IR 术语约定**：本项目沟通中，用户说的"IR"默认指 Magel 的四元式中间表示；
    "IR 优化""IR 输出""IR 检查"等均按四元式理解。涉及其他中间表示时须明确其名称。
12. **只提供 VM 执行**：语言程序通过共享 VM 执行；交付使用源码、四元式或字节码，
    宿主通过共享 VM、原生注册表和同步回调集成。未知文件名报告“不认识的文件”。
13. **胶水调用使用统一 VM 协议**：同步回调复用 `NativeCallContext::invoke` 与
    `magel_glue::Function`，验证参数、返回值、闭包、重入和错误传播；检查实参数量，
    保留原始诊断。函数值与上下文按接口生命周期使用，不把运行时地址转换为机器函数指针。
14. **门禁按改动范围执行**：只改注释（不进入产物的文字，不改任何语义或生成结果）时不必跑
    [`ci.sh`](ci.sh) / [`check.sh`](check.sh) 整套门禁，确认能正常构建即可提交。
    改动一旦涉及文法、语义、标准库、内嵌资产、文档正文或测试，按改动大小跑对应门禁
    （日常用 `sh ci.sh` 快速门禁，发版用 `sh ci.sh --full`）；
    同一次改动里混了注释以外的内容就整体按代码改动处理，不能借「只改注释」跳过门禁。
    独立回归默认并行运行，`--jobs 1` 可用于串行复核；新增并行登记前检查临时目录、端口、
    生成资产与构建依赖。共享资源、生成与构建任务保留串行屏障，不为提速省略检查。

## C/C++ 格式与语义要点

### 文件组织与增量编译

- 工程内部的 `.c` / `.cpp` / `.h` / `.hpp` 按模块职责组织，文件名使用清楚的下划线名称；
  不使用 `pass1_xxx`、`pass2_xxx` 等阶段序号名称。`source/glue/` 的标准库胶水及其内部
  辅助模块统一使用 `std_xxx` 文件名，名称体现具体功能。
- 大型头文件必须审查声明与实现的边界：将可独立编译的非模板函数、非模板成员实现和
  私有实现迁入对应 `.c` / `.cpp`，头文件保留必要的类型、接口声明、模板定义和确有需要的
  轻量内联实现。不得仅将实现改为被头文件包含的 `.cpp`，也不得通过继续包含大型实现头
  使拆分只改变文件数量。
- 内部类型、成员与函数沿用项目既有的类 Qt 命名规范；文件名的下划线规范不用于批量改名
  类、函数或公开 API。维护既有接口语义及 ABI，避免为整理文件引入无关行为变化。
- 拆分或重命名时同步维护所有引用、构建目标、自动依赖、裁剪配置、内嵌资产和导出工程模板。
  从头文件迁出的实现必须作为独立编译单元链接；解析器生成器的语义分片仍按权威文法装配。
  生成资产只从生成源维护。用增量构建核对只修改实现时
  的重编范围，避免因公共头或笼统依赖触发无关模块重编。

根目录 [`.clang-format`](.clang-format) 是全项目 C/C++ 源码及头文件的唯一格式规范：
四空格缩进、不用 Tab、自定义大括号换行、指针符号靠变量、不限制行宽、不排序 include。
工程生成器 `source/generator/` 不适用格式整理；
解析器、驱动、内嵌标准库与消息表等自动生成资产从生成源维护，不直接批量改写。
排除清单统一记录在 [`.clang-format-ignore`](.clang-format-ignore)。有逐字节同步关系的文件
仍须使用对应同步脚本保持一致；不要因此覆盖外部 ParseEngine 仓库。

```sh
python tools/format_cpp.py           # 整理受版本控制的 C/C++ 文件
python tools/format_cpp.py --check   # 只检查格式
```

函数注释示例（按实际签名和行为填写，说明仅在需要时添加）：

```cpp
/**
 * @brief 从文本中查找指定名称的配置项。
 * @param text 待查询的配置文本。
 * @param name 配置项名称。
 * @retval 返回配置项内容；名称不存在时返回空字符串。
 * @note 不修改输入文本。
 */
```

其余语言层语义要点（match 重叠警告 `MGL1601` 不升级为错误、匿名函数只用 `fn`、
`yield` / `break level` 值语义、默认值文本统一走 `toString()`、`$macro` 仅小写、
`#derive` 只生成 Equal/Hash/Clone 等）以权威文法
[`source/magel.gram`](source/magel.gram)、语义冻结清单
[`doc/21-semantics-frozen.md`](doc/21-semantics-frozen.md) 与回归测试为准，此处不再重复展开。

## 图形口径（图标与文档配图）

Magel 的图标与总览图都由**仓库里的 Python 脚本画**（PIL），没有外部设计稿：

| 用途 | 脚本 | 产物 |
| --- | --- | --- |
| 语言标志 / 插件图标 | [`tools/make_icon.py`](tools/make_icon.py) | `doc/images/magel-logo.png`（512）、`doc/images/magel.ico`（16→256 多尺寸）、`vscode/icon.png`（256） |
| 总览图 | [`tools/make_overview.py`](tools/make_overview.py)（画布与配色在 [`tools/overview_canvas.py`](tools/overview_canvas.py)） | `doc/overview/Magel-overview.{png,svg}` |
| 编译与运行流程（中英文） | [`tools/make_pipeline_diagrams.py`](tools/make_pipeline_diagrams.py)（复用总览图配色） | `doc/images/magel-compilation{,-en}.{png,svg}`、`doc/images/magel-execution{,-en}.{png,svg}` |

**图标取色**（深色版式，新图沿用这一套，别另起炉灶）：
底色 `#1b2b4b` → `#0d1728`（圆角底）、底描边 `#345480`、金 `#ffd166`（强调）、
白 `#f4f7fb`（主字）、棒身 `#56462c` 描边 / `#f7f3e8` 亮芯。
总览图是**浅色版式**，配色在 `tools/overview_canvas.py` 顶部的常量里。

**画法约定**：在 3 倍逻辑画布上画（512 → 1536）再用 LANCZOS 降采样，线端补圆；
字体按平台挑（Windows Segoe UI Bold → Linux DejaVu → macOS Arial），取不到就退回 PIL 默认字体。
**图标语义**：魔术棒（"魔"）+ `C`（C 系语法）+ `Value`（值语义），改图标别丢了其中任何一个。
产物入库、脚本随时可重跑（确定性：同样的输入必然同样的字节）；改了脚本就把产物一起提交。

## 文档链接检查

书稿与文档的标题层级保持简洁，尽量避免只有一个子小节的编号层级，例如父节
`3.7` 下仅有 `3.7.1`、没有其他同级子节。此类内容优先并入父节，确有独立标题需要时
使用不编号的小标题；只有存在多个并列主题且分层有助于阅读时，才使用编号子节。
Markdown 与 LaTeX/PDF 保持相同的编号意图：不编号的小标题不得在转换时自动生成
孤立的 `3.7.1` 等编号，也不得为了凑编号拆出无意义的子节。调整时同步核对目录、
交叉引用和生成规则。

书稿番外统一放在对应章节的最后一节，小结、习题与参考解答排在番外之前；
番外附带的技术对照随正文一起放在章末。移动内容时同步更新节号和引用。

`doc/` 只收录持续维护的功能、使用、设计与测试文档，不收录发布声明；文件名、标题和介绍按
功能组织，不带版本号或"本版新增"等发布措辞。版本发布历史统一维护在根目录 `CHANGELOG.md`。
中英文功能文档同步维护；改名时同步更新所有引用。当前能力、默认配置和实现边界统一维护在
[`doc/17-capabilities.md`](doc/17-capabilities.md) 及英文对应页；修改说明时核对权威文法、
`features.mk`、实际宿主配置与回归，不从旧路线图推断已实现能力。
标准库目录引用 API 索引，不在总览另写易漂移的模块总数。历史基准保留实际版本、日期、
配置和原始数据，不能在改 tag 时把旧测量标成当前版本。书稿改动同步生成 LaTeX 和 PDF。

[`doc/`](doc/README.md) 与 [`doc/en/`](doc/en/README.md) 中已有的中英文对应页同步维护；
尚无独立英文译文的底层规格由英文索引链接中文原文。跨页引用一律走**相对路径**
（引用仓库根写 `../`、引用 `doc/examples/` 写 `examples/`、引用 `doc/library/` 写 `library/`）。

| 脚本 | 管什么 |
| --- | --- |
| [`tools/check_doc_links.py`](tools/check_doc_links.py) | `doc/` 下所有 `.md` 的相对链接与图片是否存在（`doc/book/` 另有校验） |
| [`tools/export_magel_ebnf.py`](tools/export_magel_ebnf.py) | EBNF 附录与 `source/magel.gram` 是否同步；`--check` 只比对，不带参数则重新生成 |

两条都已接进 [`ci.sh`](ci.sh) 的文档门禁，有缺失就红。

## CHANGELOG 与版本口径

语言尚未发布，当前开发版本为 **0.24.2**。此前累计的 1.0.0 开发记录细分为
`0.19.0`–`0.19.15` 与 `0.20.0`–`0.20.16`；更早的 1.x 已重排为 `0.18.0`–`0.18.6`。
历史版本都使用三段 `0.x.x`；VS Code 扩展包独立使用 `1.0.0`。

[`CHANGELOG.md`](CHANGELOG.md) 由 **Python 从 JSON 生成**：数据源是
[`tools/changelog_data.json`](tools/changelog_data.json)，**每个版本是一个 JSON 对象**
（`version` / `title` / `entries`，版本下直接维护条目列表，子项用 `items`），不指定日期或日期分组。
改版本记录时改 JSON，然后跑 `python tools/changelog.py --build` 重新生成 Markdown；
[`tools/changelog.py`](tools/changelog.py) `--check` 校验 md 与 JSON 逐字节一致、
`VERSION` ↔ 顶级节、语言 tag ↔ 对应节（扩展包版本 tag 单独核对）、版本号唯一且自新到旧递减。当前版本之前的历史版本
只保留一行摘要。

```sh
python tools/changelog.py --build   # 从 JSON 重新生成 CHANGELOG.md
python tools/changelog.py --check   # 校验 md 与 JSON / VERSION / tag 对齐
```

已接进 [`ci.sh`](ci.sh) 的文档门禁，有漂移就红。

## Git 提交信息（全仓库强制约束）

**提交标题与正文（如有）一律使用中文，不用 `feat:` / `fix:` 之类的冒号前缀**：
直接写中文说明，如 `修复大津法二值化的阈值偏移`，不要写 `fix: 修复…`。
文件名、命令、API 标识符和必要的技术术语可以保留原文。

提交身份规范（适用于新建、补写、amend 及历史重写提交）：

- 分别检查作者（author）与提交者（committer）的名称。名称同时包含"小兔"和"糖糖"时，
  该身份的名称必须统一为 **`小兔糖糖OvO`**，邮箱统一为 **`rabbitc999@outlook.com`**。
- 不满足上述条件的身份保留其真实名称和邮箱，不得批量覆盖其他贡献者或平台提交者的身份。
- 当前工作身份用 `git config --local user.name "小兔糖糖OvO"` 与
  `git config --local user.email "rabbitc999@outlook.com"` 配置；提交前用
  `git var GIT_AUTHOR_IDENT`、`git var GIT_COMMITTER_IDENT` 核对实际身份。

书稿 Magel 示例使用四空格缩进，运算符两侧及逗号后留空格，函数体和控制分支展开为多行，
不把多条语句压成一行。排版不得改变字符串文本与宏原始文本；同步 Markdown、LaTeX、PDF，
并检查 PDF 的代码空格、缩进和可复制性。
