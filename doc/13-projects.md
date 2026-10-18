# 13 示例项目

前面各篇讲「怎么写」。这一篇讲**仓库里已经有什么能跑的东西**——每个示例给出位置、
演示的能力、运行命令、验证入口，以及跑起来需要的前置条件。

所有命令都**在仓库根目录**执行（Windows 上是 `magel.exe`，下面统一写 `magel`）。

---

## 1. 一眼看全

| 示例 | 位置 | 演示什么 | 前置 |
| --- | --- | --- | --- |
| [简易 GUI](#31-简易-gui-gui) | [`example/gui/`](../example/gui) | 窗口与控件、事件回调经运行时投递器交付 | Windows + MinGW，或 Qt5 |
| [海龟绘图](#32-海龟绘图-turtle_winapi) | [`doc/examples/turtle_winapi/`](examples/turtle_winapi/) | `abi` 块 + 动态库 + GDI 画布 | Windows + MinGW |
| [Web 学生信息管理](#41-web-学生信息管理) | [`example/web-students/`](../example/web-students/) | `std::net` 起 HTTP、`std::sql` 访问 SQLite、同源页面 | 无 |
| [学生成绩管理 · 控制台版](#42-学生成绩管理--控制台版) | [`example/sql-magel/example/ipage-student/`](../example/sql-magel/example/ipage-student) | 真实 MySQL 库、四角色权限矩阵、菜单回收 | MySQL |
| [学生成绩管理 · 前后端分离版](#43-学生成绩管理--前后端分离版) | [`example/sql-magel/example/ipage-student-server/`](../example/sql-magel/example/ipage-student-server) | 后端只出 JSON API、前端独立托管、跨源 CORS | MySQL + 静态服务器 |
| [计算器（两种 AST）](#51-计算器两种-ast) | [`example/calculator/`](../example/calculator/) | 递归下降解析、值语义下的递归拥有 | 无 |
| [模块编译](#52-模块编译) | [`example/module-compilation/`](../example/module-compilation/) | 多模块工程的 AOT 局部重编译 | 无 |
| [注解与反射](#53-注解与反射) | [`example/reflection/`](../example/reflection/) | 编译期注解、运行期元数据查询 | 需开启反射构建宏 |
| [标准库配套程序](#54-标准库配套程序) | [`doc/examples/library/`](examples/library/) | io / sqlite / watchdog / reflect 四段可跑程序 | 部分需反射 |
| [C ABI 配套示例](#55-c-abi-配套示例) | [`doc/examples/abi_buffers/`](examples/abi_buffers/)、[`abi_dynamic/`](examples/abi_dynamic/) | 静态缓冲与动态库两种绑定方式 | C 编译器 |
| [`.svmb` 证据包](#6-交付与逆向svmb-证据包) | [`doc/examples/svmb/`](examples/svmb/) | 字节码标本、逐节解剖、可复现的逆向成本数据 | 无 |
| [板端虚拟机](#71-板端虚拟机-mcu_vm) | [`source/stackvm_mcu/`](../source/stackvm_mcu) | `-fno-exceptions` 口径下的 `.svmb` 运行时 | C++ 编译器 |
| [胶水回调](../example/native-callbacks/README.md) | [`example/native-callbacks/`](../example/native-callbacks/) | 同一胶水在 VM/AOT 调用具名函数、闭包、引用及高阶回调 | C++ 编译器 |
| [编辑器语言服务器](#72-编辑器语言服务器与-vs-code-插件) | [`source/lsp/`](../source/lsp)、[`vscode/`](../vscode/) | 宏配对检查、语义高亮、真诊断 | 无 / VS Code |

---

## 2. 怎么读这一篇

每个示例都按同一套小标题写：

- **演示什么** —— 它想证明语言的哪一面；
- **怎么跑** —— 最省事的一条命令；
- **怎么验证** —— 对应的专项回归脚本（跑得过才有资格叫"示例"）；
- **边界** —— 前置条件，以及它**故意没做**的事。

示例分两类，别混：**应用型**（GUI、Web 学生管理、成绩管理）演示"能拿它做出什么"；
**能力型**（计算器、反射、模块编译、标准库配套）演示"某个语言/库特性长什么样"。

---

## 3. 桌面与 GUI

这三个都要求**有可用的交互桌面**（无头环境请用各节的 Qt 无头路径）。

### 3.1 简易 GUI（gui）

**演示什么**：Magel 侧只写 [`gui.mgl`](../example/gui/gui.mgl) 的声明，窗口与控件在宿主库里实现
（[`example/gui/winapi/gui.c`](../example/gui/winapi/gui.c) 或 [`example/gui/qt/gui_qt.cpp`](../example/gui/qt/gui_qt.cpp)），
两者之间只传固定宽度整数与 UTF-8 字符串——**不传窗口指针或控件结构**。

事件走的是和软件定时器同一套「异步发生、安全点交付」机制：GUI 事件经 C 回调交回胶水层，
由运行时投递器排队，VM / AOT 在下一个执行边界交付 Magel 中断函数。

**怎么跑**：

```powershell
python tests/magel_desktop.py        # WinAPI：出 GUI 与海龟两个产物
```

```sh
python gui/test_qt.py                # Qt 离屏：构建、UTF-8 往返、事件注入、pumpEvery
```

手动跑 Qt 版：`sh gui/qt/build.sh build/magel-gui-qt`，再
`magel aot gui/examples/counter.mgl -I gui --glue gui/glue.cpp --out build/magel-gui-qt/counter`。

**怎么验证**：`tests/magel_desktop.py`（WinAPI）、`example/gui/test_qt.py`（Qt 离屏）、
`example/gui/test_winapi.py`、`example/gui/test_program.py`。

**边界**：客户区尺寸限 100–4096 像素，控件 ID 在 1–65535 之间且不可重复；
共享库必须与可执行文件处于同一工作目录（胶水按相对路径定位）；
事件值约定为「按钮点击 = 控件 ID（>0）、关窗 = -1、Qt 周期事件 = 0」。

### 3.2 海龟绘图（turtle_winapi）

**演示什么**：最小但完整的 C ABI 桌面示例。[`demo.mgl`](examples/turtle_winapi/demo.mgl)
用宿主函数声明编排海龟路径，[`turtle.c`](examples/turtle_winapi/turtle.c) 编成 `turtle.dll`
用 WinAPI 创建窗口与 GDI 画布，[`glue.cpp`](examples/turtle_winapi/glue.cpp) 由
`magel aot --glue` 嵌入独立程序。**VM 不接触 `HWND` / `HDC` / 画布内存**——所有画布状态
都归窗口线程，Magel 调用经 `SendMessageW` 同步交给窗口过程。

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

## 4. Web 与服务端

### 4.1 Web 学生信息管理

**演示什么**：Magel 自己当服务端——`std::net` 监听 HTTP、`std::sql` 访问真实 SQLite，
浏览器调**同源** JSON 接口，不需要任何 Web 框架或前端依赖。支持新增、查看、编辑、删除，
页面可按学号 / 姓名 / 邮箱过滤，服务端做唯一性与范围校验。

**怎么跑**：

```powershell
.\magel.exe run example/web-students/main.mgl --out build/web-students -- build/students.db 8080
```

然后访问 `http://127.0.0.1:8080/`。SQLite 已静态内置，无需准备 DLL。
`--` 之后的参数直接传给 Magel `main`（依次是库文件、端口、请求上限、页面路径、就绪文件）；
SQLite **不会**自动建目录，`build/` 要事先存在。

**怎么验证**：

```sh
python tests/magel_web_students.py            # 解释执行 + AOT，每个后端 62 个真实 HTTP 请求
python tests/magel_web_students_browser.py --browser "<Chrome 或 Edge 的路径>"
```

后者用真实浏览器操作表单，并核对 HTML 注入防护与落盘 SQL 行数。

**边界**：本地单连接顺序处理的文本 HTTP；不支持 TLS、分块传输、长连接、文件上传、
登录与权限管理。建表语句、自增主键与 `last_insert_rowid()` 是 SQLite 方言，
换后端时要一并改写这些 SQL。

### 4.2 学生成绩管理 · 控制台版

**演示什么**：把 ipage-student（Java + Spring MVC + MyBatis 的实训工程）的登录与增删查改
用 Magel 重写成控制台程序，**不依赖 ipage 框架**，直接读写它自己的 `student` 库。
四角色权限矩阵（学生 3 / 教师 5 / 管理员 5 / 超管 6 个菜单项）、菜单先过滤再判两次、
删除前打印影响面，都在一个文件里。

**怎么跑**：

```sh
magel sql-magel/example/ipage-student/main.mgl                      # 库已存在
magel sql-magel/example/ipage-student/main.mgl -- --init ../ipage-student/student.sql
magel sql-magel/example/ipage-student/main.mgl -- --answers sql-magel/tests/input_ipage.txt
```

`--answers` 是脚本化回答，用于自检与演示。

**怎么验证**：`sh sql-magel/tests/check-ipage.sh`（输出应为 `ok ipage` / 检查通过）。
自检**不碰真实库**：在独立库 `student_check` 里用 `example/sql-magel/tests/fixture_ipage.sql`
配合 `--reinit` 重建，自增主键每次从 1 开始，脚本跑完后测试库回到初始状态。

**边界**：口令明文比对（`student.sql` 里就是明文），**样式对齐优先于安全增强**；
详见 [SQL 接口](../example/sql-magel/doc/01-sql-library.md)。

### 4.3 学生成绩管理 · 前后端分离版

**演示什么**：把控制台版升格成**前后端分离**的应用。后端
[`main.mgl`](../example/sql-magel/example/ipage-student-server/main.mgl) 只提供 `/api/*` 的 JSON 接口
（`/`、`/index.html` 一律 404），前端 [`web/`](../example/sql-magel/example/ipage-student-server/web)
是纯静态资源、由**另一个进程**托管，两者不同源，靠 CORS 通信。

| 关注点 | 做法 |
| --- | --- |
| 页面归属 | 后端不返回页面，静态资源独立部署 |
| 跨源 | 每个响应（含 `OPTIONS` 预检）都带 CORS 头，可用 `--allow-origin` 收窄 |
| 身份 | `Authorization: Bearer <64 位十六进制令牌>`；会话在内存，重启即失效 |
| 权限 | 权限表是唯一来源，后端逐动作判定；前端菜单与按钮只按 `/api/session` 返回的门槛渲染 |
| 注入 | 前端一律 `textContent` 写用户数据，不拼 HTML |

**怎么跑**（两个终端）：

```sh
# ① 后端（库已存在；不存在就加 -- --init ../ipage-student/student.sql）
magel run sql-magel/example/ipage-student-server/main.mgl --out build/ipage-server -- \
    --listen-port 8080

# ② 前端静态服务器
python -m http.server 5500 --directory sql-magel/example/ipage-student-server/web
```

访问 `http://127.0.0.1:5500/`；后端不在默认 8080 时用
`http://127.0.0.1:5500/?api=http://127.0.0.1:9000` 覆盖（前端连哪儿只写在
`web/config.js` 一处）。

**怎么验证**：

```sh
python tests/magel_ipage_student_server.py            # run 与 aot 各 5 组，各 207 个真实请求
python tests/magel_ipage_student_server_browser.py --browser "<Chrome 或 Edge 的路径>"
```

接口脚本覆盖登录 / 注销 / 坏令牌、四种权限的模块可见性与动作门槛、五模块增删查改与
聚合统计、CORS 预检、后端不返回页面、HTTP 报文边界。浏览器脚本另起一个线程内静态服务器，
**真的跨源**跑一遍登录、按权限收放菜单与按钮、增改查删、统计页与注入防护。

**边界**：无状态 HTTP 上的「最多 3 次登录」由前端计数，绕过前端直连接口就没有次数限制；
会话没有过期时间与刷新机制；口令仍是明文比对。真要上线必须补哈希、TLS、会话过期与限流。

---

## 5. 语言与库能力示例

### 5.1 计算器（两种 AST）

**演示什么**：同一个表达式解释器（词法分析、递归下降解析、指令生成、栈机执行、
逐行错误报告）用**两种 AST 表示**各写一遍：扁平版 `List<Node>` + 子节点索引表，
嵌套版枚举载荷持有 `List<Expr>`。后者用来验证值语义下递归拥有局部变量的复制与清理。

**怎么跑**：

```sh
magel run example/calculator/calc.mgl
magel run example/calculator/calc_nested.mgl
magel aot example/calculator/calc.mgl --out build/calculator && build/calculator
```

默认输入的 12 行里 7 行成功、5 行报应用错误（含除零、语法错误、未定义变量，
以及 `min(x, y, z)` 参数过多）。

**怎么验证**：`python tests/magel_recursive_ownership.py`——同时检查解释执行、AOT、
两种表示的数值与错误报告，以及资源诊断。

### 5.2 模块编译

**演示什么**：多模块工程的 AOT 路径——按模块缓存本地目标文件、局部重编译、
依赖失效与统一链接。工程布局见 [`example/module-compilation/magel.toml`](../example/module-compilation/magel.toml)。

**怎么跑**：

```sh
magel aot example/module-compilation/main.mgl --separate-modules --out build/module-demo
```

**怎么验证**：`python tests/magel_module_compilation.py`。
相关选项（`--separate-modules` / `--module-cache`）见[跨平台部署](06-deployment.md)。

### 5.3 注解与反射

**演示什么**：编译期用户注解（含默认值、位置与命名实参）打在接口、结构体、枚举、字段、
静态字段、构造函数与方法上；运行期查询元数据、按名调用。

**怎么跑**：根目录工具链默认已同时开启编译器与 VM 反射，直接执行：

```sh
magel --reflection example/reflection/annotations.mgl --out build/annotation-example
```

**怎么验证**：`python tests/magel_annotations.py`（会生成并实际运行 VM 与 AOT 两种独立程序）。

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

### 7.1 板端虚拟机（mcu_vm）

**演示什么**：不是"另一个解释器"，而是**一套编译口径**——同一份 VM 源码按板端约束编译，
目标平台按 H743（约 1 MB RAM / 2 MB Flash）起步。`-fno-exceptions` 让 `try` / `catch` /
`throw` **直接编译不过**：异常自由不靠人查，靠编译器拦。

**怎么跑**：

```sh
make -C source/stackvm_mcu                 # 出 build/stackvm_mcu/mcu-host（桌面可跑的板端宿主）
build/stackvm_mcu/mcu-host program.svmb data.data.bin data.layout
make -C source/stackvm_mcu show-flags      # 打印这次实际用的口径
```

**怎么验证**：`python tests/mcu_vm.py --sweep`（已进 `ci.sh`）。
完整口径表见 [`source/stackvm_mcu/README.md`](../source/stackvm_mcu/README.md)。

FK-STM32H743XIH6 的实际板端工程位于
[`example/mcu/FK-STM32H743XIH6/`](../example/mcu/FK-STM32H743XIH6/README.md)：
VM 大内存池放在 SDRAM，USART1 输出调试信息，FatFS 装载程序，LCD 与按键负责选择和启动；
程序正常退出或 panic 后返回选择页。工程自带原板级库，默认上电先运行一次 benchmark，
测速完成后 MODE/ENTER 可退出，USART1 的 `key mode` / `key enter` 虚拟按键命令也走同一退出路径。
工程还提供 DWT 周期计数与相同整数内核的原生 C++ 对照。

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
| `python tests/magel_desktop.py [--turtle-only]` | WinAPI GUI / 海龟两套桌面产物 |
| `python gui/test_qt.py <目录>`（Linux） | Qt 后端的构建、UTF-8、事件注入与 `pumpEvery` |
| `python tests/magel_web_students.py` | Web 学生管理：解释执行 + AOT |
| `python tests/magel_web_students_browser.py --browser …` | Web 学生管理的真浏览器流程 |
| `sh sql-magel/tests/check-ipage.sh` | 成绩管理控制台版（不碰真实库） |
| `python tests/magel_ipage_student_server.py` | 成绩管理前后端分离版接口 |
| `python tests/magel_ipage_student_server_browser.py --browser …` | 前后端分离版的跨源浏览器流程 |
| `python tests/magel_recursive_ownership.py` | 计算器的两种 AST 表示 |
| `python tests/magel_module_compilation.py` | 模块化 AOT 与局部重编译 |
| `python tests/magel_annotations.py` | 注解与反射 |
| `python tests/magel_library_reference.py` | 标准库配套程序 |
| `python tests/magel_abi_values.py` | C ABI 配套示例 |
| `python tests/mcu_vm.py --sweep` | 板端 VM 口径 |

需要图形环境或数据库的那几条**不在 `ci.sh` 里**，属于专项回归：桌面类要 Windows + MinGW
（或 Qt5），成绩管理要本机 MySQL 在跑。

---

## 9. 下一步

| 你想 | 去哪 |
| --- | --- |
| 自己写一个能交付的程序 | [交付安全](05-delivery-security.md) → [跨平台部署](06-deployment.md) |
| 把 Magel 接到 C/C++ 宿主 | [宿主集成与 C ABI](08-host-integration.md) |
| 跟着书从零学语言 | [《玩转 Magel 语言》](book/README.md) |
| 查某个库函数 | [库函数参考](library/README.md) |
| 查命令行与工程配置 | [工具链使用](09-toolchain.md) |
