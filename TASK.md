# Magel 当前任务交接

## 最新授权与停止点

用户最初要求三项工作，每完成一项分别提交并推送 origin。任务 1 已完成并忽略。当前推进的是
任务 2（编译器渗透测试）与任务 3（反射版学生信息管理系统）。本轮已完成一轮渗透测试并修复
其中四个已确认缺陷，未修完的部分仍然如实挂在下面，不宣称验收完成。

仓库为 `D:/Desktop/Workspace/Magel`，分支 master。**本机 `origin` 指向 GitHub，Gitee 是 `gitee`
远端**；未经用户明确同意不得推送 GitHub，日常推送一律用 `git push gitee master`。
程序版本 0.24.1，扩展版本 1.0.0。遵循根 AGENTS.md：生成资产从源维护；反射前端/VM 同时默认
开启，VM 和四元式维持默认最高优化。

## 1. 模块化拆分：已提交并推送（本次不再处理）

- [x] 提交 `036addf`：按职责拆分大数与语法树库并同步构建资产，已推送。
- [x] bignum、simple_ast 独立编译；同步 Make/CMake、CAS 独立 Makefile、内嵌资产与中英文工具链文档。
- [ ] 扩展整套门禁未全部完成（交接时人工停止，不能称全绿）。

## 2. 编译器渗透测试与修复：已完成一轮，仍有未修项

### 已完成的重建与验证

- [x] 重跑 `gen.sh`（根目录 `ParseEngine.exe` 支持 `--cpp-lexer`，与 `D:/Desktop/Workspace/ParseEngine/build/bin/` 那份 md5 相同 `3cb8d57a…`）并重建根编译器 `magel.exe`（84,520,448 字节）。
- [x] 既有定向回归 `python tests/magel_compiler_safety.py` 通过（数值边界、平台模板、拥有值传播、数据快照）。
- [x] 修掉该脚本在本机的 Windows 假失败：`run_snapshot` 按 `\n` 比对，而控制台给出 CRLF，已改为比对前归一化行尾。这是测试脚本的平台适配问题，不是编译器缺陷。
- [x] 新增渗透测试入口 `tests/magel_penetration.py`：30 个攻击用例，结果归类为 UB / CRASH / TIMEOUT / FAIL / OUTPUT，只收集事实不做判定。

### 已修复的缺陷（4 个，均已用重建后的编译器验证）

- [x] 浮点转整数越界 UB：`(int64)1e300` 静默得到 `INT64_MIN`。修法为运行期 `Op::ToInt` 加范围检查（`floatFitsInteger`），常量折叠路径遇到越界不再折叠，改为运行期报「浮点转整数超出目标范围」。
- [x] 十进制整数字面量溢出静默饱和：`99999999999999999999` 变成 `INT64_MAX`。修法为 `strtoll` 后检查 `ERANGE` 与完整消费，编译期报「十进制整数超出 int64 范围」。
- [x] 浮点字面量溢出：`1e999` 报出无关的「读未定义的变量：inf.0」。修法为字面量检查有限性，编译期报「浮点字面量超出可表示范围」。
- [x] JSON / TOML 嵌套无深度预算：两万层嵌套把宿主栈打爆（Windows 异常码 `0xC00000FD`）。修法为两边解析器各加 256 层预算与 RAII 深度守卫，超限记为解析诊断。

### 已确认但仍未修的缺陷（6 个）

- [ ] 巨大局部定长数组：运行期 arena 耗尽后 `std::bad_alloc` 逃逸成 `terminate`（退出码 3），不是优雅诊断。struct 字段有「定长数组字段超过当前内存预算」的编译期检查，局部变量没有，口径不一致。
- [ ] 定长数组维数连乘用 `int` 且无溢出检查（`source/semantics/lowering.cpp:1852/2676/4494`、`lifetime.cpp:205`、`aggregate_ownership.cpp:932`），溢出后循环上界与槽偏移成错值。
- [ ] 字符串切片负端点被静默吞掉：`"abcdef"[0:-2]` 得到 `abcdef`（`std_native_record.cpp` 的 `strSub` 把任意负 end 当成「到末尾」）。
- [ ] `vecFold` / `vecMap` / `vecFilter` / 谓词回调持引用迭代时重入 VM，回调里 `push` / `clear` 会让引用与迭代器失效（`std_collection.cpp` 约 1293/1348/1497/1541 行）。
- [ ] `vecPush` / `vecSet` 缺 for-in 守卫：`remove` 有「活跃 for-in 引用期间不能移动或删除列表元素」，`push` 没有，会无限增长直到耗尽 arena。
- [ ] `INT64_MIN / -1` 静默给出 `-9223372036854775808`；`1 << 64` 静默得到 `1`。两者都应报错或有明确定义。

### 已验证为「有正确防护、不是缺陷」的行为

深表达式链（宏预算拦截）、巨大 struct 数组（内存预算拦截）、深递归（受控「运行内存池容量不足」）、
数组正负越界与 `new[-1]`、空表 `pop`、UTF-8 多字节中间切片、`?` 用在非 Result 返回上下文
（编译期拒绝）、嵌套三层 `?`、浮点除零。

### 工具链改动

- [x] `gen.sh` 默认启用 ParseEngine 增量分析缓存（`--incremental --cache-dir build/parse-engine-cache`），`REBUILD=1 sh gen.sh` 可强制全量。实测：分析阶段 175,751 ms → 132 ms，`gen.sh` 总耗时 197 s → 20 s，产物与全量逐字节一致（比对 `magel_parser.cpp/hpp` 与 `magel_main.cpp` 的 md5）。

## 3. 反射版学生信息管理系统：草稿已保存，尚未完成

- [x] 保存 `example/reflection-students/main.mgl` 与中英文草稿说明。模型字段 id/name/age/className/score，使用 @Reflect、SQL 模型注解、反射 JSON 编解码、参数化 SQLite。
- [x] 已写入命令 add/update/get/delete/list/search/schema，以及学号、姓名/班级、年龄、成绩校验。这里只表示代码存在，不表示各命令验收完成。
- [x] 本机草稿经编译生成字节码后，用共享 VM 验证空列表、新增中文学生、按学号查询和跨进程持久化列表；这些操作返回 0 且无 VM 资源残留错误。
- [x] 保存 `tests/magel_program_runner.cpp`：加载 .svmb/.data.bin/.layout、完整标准胶水、UTF-8 参数和 Machine::setProgramArguments，检查停机堆/对象残留。
- [ ] 完成自动测试脚本，覆盖更新、删除、搜索、排序、schema、重复学号、非法 JSON/字段类型/范围、SQL 注入文本、数据库打开错误、重启持久化和零资源残留。删除阶段在交接时中止，不能标记通过。
- [ ] `magel FILE.svmb 参数` 不是现有编译器的字节码运行接口：不要继续这样测试，它会把二进制当源码解析。字节码验收使用上述共享 VM 宿主。用户日常可运行源码：`magel main.mgl 数据库路径 命令 [参数]`。
- [ ] 核对嵌套 ? 问题对草稿的影响。草稿已尽量将结果保存到局部变量，再构造外层 Result，未用新编译器全面验收。
- [ ] 完成中英文正式使用说明、doc/13-projects.md 及英文页/索引同步；移除「未验收草稿」口径的前提是确已通过验收。
- [ ] 按 AGENTS.md 重跑内嵌、构建和对应门禁、打包 VSIX，核对版本与内嵌字节；本项单独提交推送。

## 接手命令与环境（本机，2026-10-21 更新）

| 项 | 本机实际 |
| --- | --- |
| 仓库路径 | `D:/Desktop/Workspace/Magel`（旧记录 `G:/Desktop/magel` 已失效，本机无该路径） |
| 生成器 | 根目录 `ParseEngine.exe` 即可（`gen.sh` 会优先找它），与 `D:/Desktop/Workspace/ParseEngine/build/bin/ParseEngine.exe` 同 md5 |
| 构建 | **只有 `mingw32-make`，没有 `make`**：`mingw32-make -j4` |
| 工具链 | `g++` 在 `/c/MINGW64_YACC/bin`（11.2.0），需加进 PATH：`export PATH="/c/MINGW64_YACC/bin:/c/Program Files/Git/usr/bin:/usr/bin:$PATH"` |
| WSL | **没有已安装的发行版**，旧记录里 `/home/rabbit/magel-audit` 的 Linux sanitizer 验证本机做不了 |
| 远端 | `origin` = GitHub（**不要推送**），`gitee` = Gitee（日常推送用它） |

旧机器的 `tmp/` 证据日志不在本机。

后续先生成/重建，再做定向回归。TASK.md 只有在所有剩余验收、文档和产物完成并核对证据后，才能随最终交付提交删除。
