# Magel 当前任务交接

## 最新授权与停止点

用户最初要求三项工作，每完成一项分别提交并推送 origin。随后要求停止第 2 项新增探索，只完善已确认问题；不能修复的已知不安全用法应直接报错。最后明确要求“现在写完 TASK.md 直接提交”。因此本次保存现状并停止实施，不宣称第 2、3 项验收完成。

仓库为 `G:/Desktop/magel`，分支 master；只推送 `origin`（Gitee），未经同意不能推送 GitHub。程序版本 0.24.1，扩展版本 1.0.0。遵循根 AGENTS.md：生成资产从源维护；反射前端/VM 同时默认开启，VM 和四元式维持默认最高优化。

## 1. 模块化拆分：已提交并推送

- [x] 提交 `036addf`：按职责拆分大数与语法树库并同步构建资产，已推送 origin/master。
- [x] bignum：整数乘法、除法、数论、位运算、转换及十进制方根独立编译。
- [x] simple_ast：类型、节点、数据导出独立编译；共享头仅保留轻量布局查询。
- [x] 同步 Make/CMake、CAS 独立 Makefile、直接编译测试、内嵌资产、导出工程、中英文工具链文档与 CHANGELOG。未修改外部 ParseEngine 副本。
- [x] 两库 CMake 单测 2/2 与 1/1；整数原生/可移植路径各 2811 项、8433 次比较；高精度数学 398 项；CAS 独立 12 项、语言 20 项；独立导出编译器完整构建并验证通过。
- [x] 增量 dry-run 证明只改 ast_node.cpp 只重编该对象；本次 C++ 文件格式通过。全仓格式检查另有既有 CAS 文件格式问题，未批量处理。
- [x] VSIX 已打包：插件 1.0.0、编译器 0.24.1；当时内嵌编译器 84514304 字节，SHA256 `c42dc0d1661d17a12a6e9f3a944f3647ac556e2da22eee6e5e762b43ee47c8b6`，与当时根编译器逐字节一致。
- [ ] 扩展整套门禁未全部完成。生成、构建、宏及反射/SQL 等专项已通过；全正常样例继续到错误样例阶段后，按最后交接指令终止。调度器显示失败是本次人工停止造成的，不能称整套门禁全绿。初次链接曾被预检 exe 占用，重链已通过；CAS 默认产物覆盖曾干扰测试，隔离输出复核通过。

证据（本机 tmp 不入库）：`module-bignum-final.log`、`module-ast.log`、`bigint-module-test.log`、`bigint-portable-module-test.log`、`bigfloat-module-test.log`、`module-cas-library.log`、`module-cas-recheck.log`、`module-generator.log`、`module-incremental.log`、`module-vsix-verification.json`、`module-ci-resume.log`、`ci/module-resume/014.log`。

## 2. 已确认问题修复：源码已保存，未完成交付验收

不再开展随机探索。此前已完成的 205 项初始探测与 1025 项确定性源码语料没有记录崩溃/超时；它们不证明没有其他问题。只继续下列已确认问题：

- [x] 已写入 ast_data.cpp：整数不再经过 double，保护浮点转 int64 范围。
- [x] 已写入 bigfloat_roots.cpp：幂运算先检查再乘；方根拒绝会导致内部算术溢出的精度，避免精度翻倍溢出和 uint64 根次的有符号收窄。
- [x] 已写入 boxing.cpp：枚举构造参数接管拥有型表达式临时值时调用 TakeExpressionTempValue 清空原槽，避免 `Ok(value()?)` 重复释放。
- [x] 已写入 magel_semantics.cpp：VM 从本次编译的内存数据快照装载，避免同前缀磁盘覆盖改变正在执行的程序。
- [x] 已写入平台修复：BigFloat 的 LP64 构造重载；JSON/TOML 完整类型后的 emplace 模板定义；网络胶水 htons 成员改名；独立 VM CMake 补齐 CAS、SQL、SQLite、压缩、include 和宿主依赖。
- [x] 原生边界测试 `python tests/magel_compiler_safety.py --native-only` 在 Windows 通过，见 `tmp/safety-native-windows.log`。独立 VM CMake 测试目标成功编译链接，见 `tmp/safety-stackvm.log`；尚未运行该目标。
- [x] Linux 隔离副本中用 UBSan 复现过三处越界/溢出；嵌套字符串 ? 的候选修复已在非 PIE ASan/UBSan 编译器中输出 hello 且返回 0。该隔离编译器仍打印标准胶水注册失败空诊断，尚未处理，不能称整个 Linux 编译器验证完成。
- [ ] 重跑 gen.sh 并完整重建根编译器：本轮 gen 已更新内嵌 cpp，但外部生成器在交接时停止，解析器生成未确认完成。根 magel.exe 与 vscode/bin、VSIX 仍是第 1 项编译器，不能当作已包含这些修复。
- [ ] 运行 `python tests/magel_compiler_safety.py` 的完整定向回归：字符串/容器成功与错误传播、数据快照覆盖测试尚未用重建后的根编译器验收。
- [ ] Linux sanitizer 原生完整测试需复核；早期测试脚本有两个断言口径错误（忽略数据区首 8 字节保留区、把科学记数法与普通格式混淆），已修正正式测试，不能将那两次断言失败当作库缺陷。
- [ ] 若任何已知不安全用法仍无法修复，按用户要求加入明确诊断并添加拒绝回归，不可继续放任其运行期无效释放。
- [ ] 同步文档实际边界、内嵌资产、重建后的 VSIX 及两种版本/字节，执行对应门禁后再标记本项完成。

正式入口：`tests/magel_native_safety.cpp`、`tests/magel_compiler_safety.py`、`tests/fixtures/compiler_safety_owned_try.mgl`；已登记快速门禁和并行白名单。说明在 doc/37-compiler-safety.md 及英文对应页，已显式标注未完整验收。

## 3. 反射学生系统：草稿已保存，尚未完成

- [x] 保存 `example/reflection-students/main.mgl` 与中英文草稿说明。模型字段 id/name/age/className/score，使用 @Reflect、SQL 模型注解、反射 JSON 编解码、参数化 SQLite。
- [x] 已写入命令 add/update/get/delete/list/search/schema，以及学号、姓名/班级、年龄、成绩校验。这里只表示代码存在，不表示各命令验收完成。
- [x] 本机草稿经编译生成字节码后，用共享 VM 验证空列表、新增中文学生、按学号查询和跨进程持久化列表；这些操作返回 0 且无 VM 资源残留错误。见 tmp/students-probe-tests.log。
- [x] 保存 `tests/magel_program_runner.cpp`：加载 .svmb/.data.bin/.layout、完整标准胶水、UTF-8 参数和 Machine::setProgramArguments，检查停机堆/对象残留。
- [ ] 完成自动测试脚本，覆盖更新、删除、搜索、排序、schema、重复学号、非法 JSON/字段类型/范围、SQL 注入文本、数据库打开错误、重启持久化和零资源残留。删除阶段在交接时中止，不能标记通过。
- [ ] `magel FILE.svmb 参数` 不是现有编译器的字节码运行接口：不要继续这样测试，它会把二进制当源码解析。字节码验收使用上述共享 VM 宿主。用户日常可运行源码：`magel main.mgl 数据库路径 命令 [参数]`。
- [ ] 核对嵌套 ? 问题对草稿的影响。草稿已尽量将结果保存到局部变量，再构造外层 Result，未用新编译器全面验收。
- [ ] 完成中英文正式使用说明、doc/13-projects.md 及英文页/索引同步；移除“未验收草稿”口径的前提是确已通过验收。
- [ ] 按 AGENTS.md 重跑内嵌、构建和对应门禁、打包 VSIX，核对版本与内嵌字节；本项单独提交推送。

## 接手命令与环境

根目录旧 ParseEngine.exe 不支持现有 --cpp-lexer 参数。生成使用 `ENGINE=G:/Desktop/ParseEngine/build/bin/ParseEngine.exe sh gen.sh`，随后 `make -j4`。Windows PowerShell 可先设置 `$env:ENGINE='G:/Desktop/ParseEngine/build/bin/ParseEngine.exe'` 再执行 `sh gen.sh`。不要直接改生成产物。

WSL 为 Ubuntu-22.04，用户 rabbit；持久隔离目录 `/home/rabbit/magel-audit`。不要把长期证据只放 Linux /tmp（会被清理）。ASan 大型 PIE 编译器在本环境启动崩溃，隔离验证使用 -no-pie 链接；这不是已完成的跨平台交付配置。

后续先生成/重建，再做定向回归。用户已要求由其他 AI 继续测试，本次不再启动任何验证。TASK.md 只有在所有剩余验收、文档和产物完成并核对证据后，才能随最终交付提交删除。
