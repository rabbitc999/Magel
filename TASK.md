# 当前授权任务

按顺序完成，每项验收后分别提交并推送 origin master。

## 1. 模块化拆分
- [x] 按职责拆分 library/bignum 与 library/simple_ast，独立编译实现，保持接口。
- [x] 同步 Make/CMake、导出模板、内嵌资产、直接编译测试与文档。
- [x] 模块单测、整数/高精度差分、CAS 独立测试、导出编译器、增量依赖、VSIX 与版本/字节核对。
- 整套快速门禁已执行，完成生成、构建及宏/反射等专项；全样例扩展验收继续运行，最终交付前汇总。
- [x] 本项以独立中文提交推送 origin；提交与推送结果以 Git 记录核对。

## 2. 已确认问题的修复与完善（不再扩展探索）
- [x] 已完成环境与初步边界检查；按用户后续要求停止扩大语料和寻找新问题。
- [ ] 仅处理已确认的数据导出转换/精度、BigFloat 溢出、嵌套 ? 释放、并发数据加载及 Linux 构建问题，保留针对性回归与未解决限制。
- [ ] 同步文档、产物，运行对应门禁，中文提交并推送 origin。

## 3. 反射版学生信息管理系统
- [ ] 基于实际反射 API 完成可运行系统，覆盖增删改查、校验与持久化。
- [ ] 添加自动验收与中英文使用说明，同步索引及产物，运行对应门禁。
- [ ] 中文提交并推送 origin。

全部验收证据齐全后，随最后一次交付提交删除本文件。

## 验收记录（持续更新）

- 模块单测：`tmp/module-bignum.log`（2/2）与 `tmp/module-ast.log`（1/1）。
- 大整数差分：`tmp/bigint-module-test.log`、`tmp/bigint-portable-module-test.log`，各 2811 项、8433 次比较。
- 大浮点差分：`tmp/bigfloat-module-test.log`，398 项。
- 增量 dry-run：`tmp/module-incremental.log`，修改 ast_node.cpp 仅重编该对象。
- 本次 C/C++ 文件格式通过；全仓格式检查受既有 CAS 文件格式问题影响，未改动外部原件。
- 门禁与导出测试使用 `ENGINE=G:/Desktop/ParseEngine/build/bin/ParseEngine.exe`；仓库根旧生成器不支持导出使用的 --cpp-lexer 参数。

- 导出工程：`tmp/module-generator.log` 通过；CAS 独立 12 项：`tmp/module-cas-library.log` 通过；CAS 语言 20 项：`tmp/module-cas-recheck.log` 通过。
- VSIX：`tmp/module-vsix-verification.json`，插件 1.0.0、编译器 0.24.1，内嵌 84514304 字节与根编译器一致。
- 完整门禁初次链接被预检进程占用，重链接后续跑 `tmp/module-ci-resume.log`；CAS 路径清单已补齐并专项复核，并发默认产物覆盖已隔离输出复核。
- 独立 VM CMake 暴露既有 CAS include/宿主依赖缺失，作为第 2 项已确认构建问题完善；库自身 CMake 与独立导出编译器均通过。
