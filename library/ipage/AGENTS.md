# ipage SQL 核心维护约定

本目录是 Magel 直接维护的 C++17 SQL 生成核心，不再作为独立 Git 仓库维护。
只保留字段、模型元数据、条件树、查询和数据库无关的方言接口。
不恢复 Java/Maven、数据库驱动、连接、会话、建表同步或重复数据库源码。
宿主适配在 Magel 的 `source/glue/std_sql_ipage.cpp` 和 `std_sql.cpp` 中维护。

- 依赖仅限 C++17 标准库。`source/Makefile` 和 `source/CMakeLists.txt` 都必须可用。
- 标识符经过 `Dialect::quote` / `qualify`；字段名通过 `ModelMeta` 白名单校验。
- 参数值只进入占位符和 `SqlStatement::params()`，不得拼接到 SQL 文本。
- 表名来自 `ModelMeta::table()`，别名使用 `t_` 前缀；聚合分组覆盖非聚合选择列。
- `Field<T,M>` 负责声明期成员绑定，`FieldInfo` 与非模板 `Query` 负责运行期生成。
- 中文 Doxygen、文件命名和格式沿用仓库根目录 AGENTS.md。
- 核心测试只断言 SQL、参数、白名单、连接与聚合结构，不连接数据库。
  修改宿主适配时额外运行 Magel 动态 SQL 的真实数据库回归。
- 修改核心后同步 Magel 构建、内嵌及导出资产；根目录 `TASK.md` 若存在则记录当前任务。
