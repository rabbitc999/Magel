# Magel 当前任务交接

## 最新授权与停止点

用户最初要求三项工作。任务 1（模块化拆分）已完成并忽略；任务 2（编译器渗透测试）与  
任务 3（反射版学生信息管理系统）本轮已完成，并已提交推送 gitee 与 GitHub。

仓库为 `D:/Desktop/Workspace/Magel`，分支 master。远端 `origin` = GitHub、`gitee` = Gitee；  
用户已明确授权推送 GitHub（2026-10-22），日常推送仍用 `git push gitee master`。  
程序版本 0.24.1，扩展版本 1.0.0。遵循根 AGENTS.md：生成资产从源维护；反射前端/VM 同时默认  
开启，VM 和四元式维持默认最高优化。

## 1. 模块化拆分：已提交并推送（本次不再处理）

- [x] 提交 `036addf`：按职责拆分大数与语法树库并同步构建资产，已推送。
- [x] bignum、simple_ast 独立编译；同步 Make/CMake、CAS 独立 Makefile、内嵌资产与中英文工具链文档。

## 2. 编译器渗透测试与修复：已完成，无未修项

### 工具链

- [x] `tests/magel_penetration.py`：34 个攻击用例，结果归类为 UB / CRASH / TIMEOUT / FAIL / OUTPUT，只收集事实不做判定。
- [x] `tests/magel_compiler_safety.py` 定向回归通过；修掉该脚本在本机的 Windows 假失败（`run_snapshot` 比对前归一化 CRLF）。
- [x] `gen.sh` 默认启用 ParseEngine 增量分析缓存（`--incremental --cache-dir build/parse-engine-cache`），`REBUILD=1 sh gen.sh` 可强制全量。实测分析阶段 175,751 ms → 132 ms、`gen.sh` 197 s → 20 s，产物与全量逐字节一致。
- [x] 重建根编译器 `magel.exe`（84,525,056 字节）。

### 已修复的缺陷（10 个，均已用重建后的编译器验证）

- [x] 浮点转整数越界 UB：运行期 `Op::ToInt` 加范围检查（`floatFitsInteger`），常量折叠遇到越界不再折叠，改为运行期报「浮点转整数超出目标范围」。
- [x] 十进制整数字面量溢出静默饱和：`strtoll` 后检查 `ERANGE` 与完整消费，编译期报「十进制整数超出 int64 范围」。
- [x] 浮点字面量溢出：字面量检查有限性，编译期报「浮点字面量超出可表示范围」。
- [x] JSON / TOML 嵌套无深度预算：两边各加 256 层预算与 RAII 深度守卫，超限记为解析诊断。
- [x] 巨大局部定长数组：编译期按运行内存预算拒绝（此前 arena 耗尽后 `std::bad_alloc` 逃逸成 `terminate`），与 struct 字段口径一致。
- [x] 定长数组维数连乘 `int` 溢出：新增 `FixedArrayElementCount`（64 位累加并夹到 int 上界），替换 lowering / lifetime / resource_lifetime / aggregate_ownership / types 全部 7 处连乘。
- [x] 字符串切片负端点被静默吞掉：`strSub` 拒绝负端点，报「切片端点不能为负数（String 的切片按字节算）」。`end == -1` 是内建 ABI 里「到末尾」的约定（数组复制、`Text.split` 都走这条路），单独保留；第一版连它一起拒了，造成 6 个样例回归失败，已修正。
- [x] `vecFold` / `vecMap` / `vecFilter` / 谓词回调持引用迭代时重入 VM：新增 `IteratorLeaseScope` 租约，回调里 `push` / `clear` 被拒绝。
- [x] `vecPush` / `vecSet` 缺 for-in 守卫：与 `remove` 一样受「活跃 for-in 引用期间不能移动或删除列表元素」保护。
- [x] `INT64_MIN / -1` 与超宽移位：`INT64_MIN / -1` 报「整数除法溢出…」；移位位数超出 0..63 报「移位位数超出整数位宽（有效范围 0..63）」。栈机（`vm_execution.cpp`）、寄存器机（`register_core.hpp`）与四元式常量折叠（`quad_optimizer.cpp`）三处同步处理。

### 渗透测试当前结果

34 个用例中 27 个 FAIL、7 个 OUTPUT，**没有 UB / CRASH / TIMEOUT**。FAIL 即「被诊断拦下」，  
是本轮想要的结果。边界表与复现入口见 `doc/37-compiler-safety.md`（含英文页）。

### 已验证为「有正确防护、不是缺陷」的行为

深表达式链（宏预算拦截）、巨大 struct 数组（内存预算拦截）、深递归（受控「运行内存池容量不足」）、  
数组正负越界与 `new[-1]`、空表 `pop`、UTF-8 多字节中间切片、`?` 用在非 Result 返回上下文  
（编译期拒绝）、嵌套三层 `?`、浮点除零。

## 3. 反射版学生信息管理系统：已完成

- [x] `example/reflection-students/main.mgl`：模型 `@Reflect` + `@std::sql::ModelClass("students")` + `@ModelField(id: true)`，字段清单由反射读取，全部用户输入绑成 SQL 参数。
- [x] 命令 `schema` / `add` / `update` / `get` / `delete` / `list [字段 [asc|desc]]` / `search 姓名` 全部实测通过。
- [x] 校验与错误路径实测：`学号必须是正整数`、`年龄必须在 1 到 150 之间`、`成绩必须在 0 到 100 之间`、`姓名必须为 1 到 120 字节的非空文本`、`学生不存在`、`模型 Student 没有字段 badfield`、`排序方向必须是 asc 或 desc`、JSON 解析位置诊断、命令用法提示。
- [x] 跨进程持久化实测：关掉终端再 `list` 数据仍在。
- [x] 中英文 README 去掉「未验收草稿」口径，改为正式使用说明。
- [x] 书稿新增第 23 章《学生信息管理系统》（`doc/book/23-学生信息管理系统.md`），同步 README 索引、`md2tex.py` 映射、`book.tex` 输入，并生成 `latex/chapters/ch23.tex`。
- [x] `tests/magel_book_updates.py` 里「22 章」的硬编码断言改为跟随 `md2tex.FILES` 推导，并逐条核对 `book.tex` 的 `\input`；重新内嵌手册（`tools/embed_manual.py` + `tools/embed_stdlib.py`）并重建编译器。
- [x] `doc/13-projects.md` 与英文页新增「反射版学生管理系统」条目。
- [ ] 自动回归脚本（覆盖上述全部命令与错误分支）尚未写成；本轮靠逐条实测记录结果。
- [x] `latex/book.pdf` 未重新构建（本机无 TeX Live）；只更新了 `.tex` 源。

### 运行方式提醒

`magel FILE.svmb 参数` 不是现有编译器的字节码运行接口，会把二进制当源码解析。  
字节码验收使用共享 VM 宿主 `tests/magel_program_runner.cpp`。用户日常运行源码：  
`magel main.mgl 数据库路径 命令 [参数]`。

## 接手命令与环境（本机，2026-10-22 更新）

| 项    | 本机实际                                                                                                                           |
| ---- | ------------------------------------------------------------------------------------------------------------------------------ |
| 仓库路径 | `D:/Desktop/Workspace/Magel`（旧记录 `G:/Desktop/magel` 已失效，本机无该路径）                                                                |
| 生成器  | 根目录 `ParseEngine.exe` 即可（`gen.sh` 会优先找它）                                                                                       |
| 构建   | **只有 `mingw32-make`，没有 `make`**：`mingw32-make -j4`                                                                             |
| 工具链  | `g++` 在 `/c/MINGW64_YACC/bin`（11.2.0），需加进 PATH：`export PATH="/c/MINGW64_YACC/bin:/c/Program Files/Git/usr/bin:/usr/bin:$PATH"` |
| WSL  | **没有已安装的发行版**，旧记录里 `/home/rabbit/magel-audit` 的 Linux sanitizer 验证本机做不了                                                        |
| 远端   | `origin` = GitHub（已授权推送），`gitee` = Gitee（日常推送用它）                                                                               |

改 `source/magel_semantics.cpp` 或 `source/semantics/*.cpp` 后必须重跑 `gen.sh`（语义文件被  
包含进生成的 `magel_parser.cpp`），只 `make` 不会生效。

旧机器的 `tmp/` 证据日志不在本机。
