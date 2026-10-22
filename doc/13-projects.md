# 13 示例项目

前面各篇讲「怎么写」。这一篇讲**仓库里已经有什么能跑的东西**——每个示例给出位置、
演示的能力、运行命令、验证入口，以及跑起来需要的前置条件。

所有命令都**在仓库根目录**执行（Windows 上是 `magel.exe`，下面统一写 `magel`）。

---

## 1. 一眼看全

| 示例 | 位置 | 演示什么 | 前置 |
| --- | --- | --- | --- |
| [海龟绘图](#31-海龟绘图-turtle_winapi) | [`doc/examples/turtle_winapi/`](examples/turtle_winapi/) | `abi` 块 + 动态库 + GDI 画布 | Windows + MinGW |
| [计算器（两种 AST）](#51-计算器两种-ast) | [`example/calculator/`](../example/calculator/) | 递归下降解析、值语义下的递归拥有 | 无 |
| [注解与反射](#53-注解与反射) | [`example/reflection/`](../example/reflection/) | 编译期注解、运行期元数据查询 | 需开启反射构建宏 |
| [标准库配套程序](#54-标准库配套程序) | [`doc/examples/library/`](examples/library/) | io / sqlite / watchdog / reflect 四段可跑程序 | 部分需反射 |
| [C ABI 配套示例](#55-c-abi-配套示例) | [`doc/examples/abi_buffers/`](examples/abi_buffers/)、[`abi_dynamic/`](examples/abi_dynamic/) | 静态缓冲与动态库两种绑定方式 | C 编译器 |
| [`.svmb` 证据包](#6-交付与逆向svmb-证据包) | [`doc/examples/svmb/`](examples/svmb/) | 字节码标本、逐节解剖、可复现的逆向成本数据 | 无 |
| [胶水回调](../example/native-callbacks/README.md) | [`example/native-callbacks/`](../example/native-callbacks/) | 同一胶水在 VM 调用具名函数、闭包、引用及高阶回调 | C++ 编译器 |
| [编辑器语言服务器](#72-编辑器语言服务器与-vs-code-插件) | [`source/lsp/`](../source/lsp)、[`vscode/`](../vscode/) | 宏配对检查、语义高亮、真诊断 | 无 / VS Code |

---

## 2. 怎么读这一篇

每个示例都按同一套小标题写：

- **演示什么** —— 它想证明语言的哪一面；
- **怎么跑** —— 最省事的一条命令；
- **怎么验证** —— 对应的专项回归脚本（跑得过才有资格叫"示例"）；
- **边界** —— 前置条件，以及它**故意没做**的事。

示例分两类，别混：**应用型**（学生信息管理、绘图）演示"能拿它做出什么"；
**能力型**（计算器、反射、模块编译、标准库配套）演示"某个语言/库特性长什么样"。

---

## 3. 桌面绘图与游戏

桌面示例需要可用的交互桌面；无头验证方式见各示例说明。

### 3.1 海龟绘图（turtle_winapi）

**怎么跑**：

```powershell
python tests/magel_desktop.py --turtle-only
Set-Location .\build\magel-turtle-winapi
.\demo.exe            # 绘制 Koch 雪花；关窗退出
.\demo.exe close      # 绘制并保存后直接关闭（自动验收用）
```

程序在当前目录写出 `turtle.bmp`。

**边界**：需要 Windows 与可用的交互桌面；DLL 必须与 exe 同目录。
若 Magel 声明、C 函数签名或调用约定不一致，属于 C ABI 边界错误——
背景见[宿主集成](08-host-integration.md)。

---

## 4. 数据库示例

### 4.1 反射版学生管理系统

**演示什么**：`@Reflect` 与 SQL 模型注解派生出建表语句、查询列、JSON 键和排序字段检查；
所有用户输入绑成 SQL 参数；字段清单只在模型里声明一次。

**怎么跑**（在 `example/reflection-students` 目录里）：

```sh
magel main.mgl students.db add '{"id":1,"name":"张伟","age":18,"className":"高一(3)班","score":92}'
magel main.mgl students.db list score desc
magel main.mgl students.db schema
```

**怎么验证**：逐条跑 `add` / `update` / `get` / `delete` / `list` / `search` / `schema`，
以及非法 JSON、越界年龄成绩、未知排序字段和不存在的学号。

**边界**：需要反射版工具链；模型字段变化会影响既有数据库（旧库缺列会被「数据库缺少字段」拦下）。

入门讲解见书稿[第 23 章 学生信息管理系统](book/23-学生信息管理系统.md)。

### 4.2 其他数据库示例

数据库基础用法见[SQL 示例](../example/sql-magel/README.md)，反射与参数化查询见[动态 SQL 说明](36-reflection-serialization-sql.md)。

## 5. 语言与库能力示例

### 5.1 计算器（两种 AST）

**演示什么**：同一个表达式解释器（词法分析、递归下降解析、指令生成、栈机执行、
逐行错误报告）用**两种 AST 表示**各写一遍：扁平版 `List<Node>` + 子节点索引表，
嵌套版枚举载荷持有 `List<Expr>`。后者用来验证值语义下递归拥有局部变量的复制与清理。

**怎么跑**：

默认输入的 12 行里 7 行成功、5 行报应用错误（含除零、语法错误、未定义变量，
以及 `min(x, y, z)` 参数过多）。

### 5.2 模块编译

**怎么跑**：

**怎么验证**：`python tests/magel_module_compilation.py`。
相关选项（`--separate-modules` / `--module-cache`）见[跨平台部署](06-deployment.md)。

### 5.3 注解与反射

**演示什么**：编译期用户注解（含默认值、位置与命名实参）打在接口、结构体、枚举、字段、
静态字段、构造函数与方法上；运行期查询元数据、按名调用。

**怎么跑**：根目录工具链默认已同时开启编译器与 VM 反射，直接执行：

```sh
magel --reflection example/reflection/annotations.mgl --out build/annotation-example
```

**怎么验证**：`python tests/magel_annotations.py`（会生成并实际运行 VM 两种独立程序）。

**边界**：注解保存于编译器 AST 并完成类型检查；`std::reflect::` 的完整运行期 API 见
[库函数参考](library/README.md)。

### 5.4 标准库配套程序

**演示什么**：需要一点工程布局才跑得起来的库用法。[`doc/examples/library/`](examples/library/)
下有四个：`io.mgl`（文件创建/写入/刷新/定位）、`sqlite.mgl`（内置 SQLite 内存库与
Unicode 字符串）、`watchdog.mgl`（未启用与错误水位分支）、`source/std/reflect.mgl`（类型与结构字段查询）。

**怎么跑**：

```sh
magel run doc/examples/library/io.mgl -- build/library-example.txt
magel run doc/examples/library/sqlite.mgl
```

**怎么验证**：`python tests/magel_library_reference.py`——核对声明、链接并执行 23 个完整程序。

### 5.5 C ABI 配套示例

**演示什么**：两种绑定姿势。[`abi_buffers/`](examples/abi_buffers/) 传静态缓冲，
[`abi_dynamic/`](examples/abi_dynamic/) 走动态库 + 胶水。两份都是 `native.c` + `demo.mgl`
的小组合，适合当自己写绑定时对着抄的最小骨架。

**怎么验证**：`python tests/magel_abi_values.py`。
类型映射与 `abi` 块语法见[宿主集成与 C ABI](08-host-integration.md)。

---

## 6. 交付与逆向：`.svmb` 证据包

**演示什么**：[`doc/examples/svmb/`](examples/svmb/) 是
[`.svmb` 二进制格式解剖与逆向难度](15-svmb-format.md)
的证据来源。标本 `control.svmb` 662 750 字节、SHA-256 `CF30A3C8…A757ABE`；
同一份源码加 `--no-debug` 的产物 249 681 字节、SHA-256 `08554811…A6007476D`，
**不含任何路径字符串**，换个目录重新生成字节完全一致。

里面的每一个数字都由 [`collect.py`](examples/svmb/collect.py) 跑出来，**不手写**；
工具缺失时对应那一节写「跳过」，不伪造内容。

**怎么跑**：

```sh
magel run example/samples/control.mgl --emit --svmb --out tmp/svmb-study/control
python doc/examples/svmb/collect.py --parser ./magel.exe
```

（`collect.py` 还可接 `--vm` / `--vm-opcode` / `--exported` 补上 VM 与导出程序的证据。）

**边界**：再次生成**不会与原标本逐字节相同**——调试节给每条指令都存了一份编译时的
`.quad` 路径字符串（实测 33 字节 × 8204 条 = `+270732` 字节），换目录编译产物大小就变；
但程序本体逐字节一致。`svmb_dump.py` 是最小解析器，纯标准库。

---

## 7. 工具链配套

### 7.1 嵌入式 Linux 宿主

使用目标 Linux 的 C++17 工具链构建 `library/stackvm/` 和原生胶水，随程序交付
`.svmb`、数据区及布局文件，并提供宿主使用的系统库。资源预算与执行边界见
[部署说明](06-deployment.md)及[书稿交付与部署章节](book/22-交付与部署.md)。

### 7.2 编辑器语言服务器与 VS Code 插件

**演示什么**：两条互补的路子。工具链自带的 `magel lsp`（内嵌在编译器里，跑真前端，
给真实错误码与精确位置）负责完整前端；[`source/lsp/`](../source/lsp) 这个 C++17 小程序**不依赖编译器**，
离线检查 `$macro` / `$replace` / `$for` 的配对、参数、调用与嵌套，另给语义高亮。

**怎么跑**：

```sh
magel lsp                      # 工具链自带
make -C lsp                    # 独立的宏编辑器语言服务器
```

VS Code 插件在 [`vscode/`](../vscode/)，语法高亮、补全、跳转与断点调试见其
[README](../vscode/README.md)。

**怎么验证**：`python tests/magel_lsp_api.py`、`python tests/magel_lsp_symbols.py`、
`python tests/magel_native_lsp_parity.py`。

---

## 8. 回归入口速查

| 命令 | 覆盖 |
| --- | --- |
| `sh check.sh` | `example/samples/` 全部样例 + 预期输出比对 |
| `python tests/magel_desktop.py [--turtle-only]` | 海龟绘图的 WinAPI 产物 |
| `python tests/magel_recursive_ownership.py` | 计算器的两种 AST 表示 |
| `python tests/magel_annotations.py` | 注解与反射 |
| `python tests/magel_library_reference.py` | 标准库配套程序 |
| `python tests/magel_abi_values.py` | C ABI 配套示例 |

需要图形环境或数据库的检查属于专项回归：桌面绘图需要 Windows 与 MinGW，外部数据库示例需要对应服务和客户端库。

---

## 9. 下一步

| 你想 | 去哪 |
| --- | --- |
| 自己写一个能交付的程序 | [交付安全](05-delivery-security.md) → [跨平台部署](06-deployment.md) |
| 把 Magel 接到 C/C++ 宿主 | [宿主集成与 C ABI](08-host-integration.md) |
| 跟着书从零学语言 | [《玩转 Magel 语言》](book/README.md) |
| 查某个库函数 | [库函数参考](library/README.md) |
| 查命令行与工程配置 | [工具链使用](09-toolchain.md) |
