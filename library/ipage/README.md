# ipage C++ SQL 生成核心

本目录由 ChaLi 直接维护，只依赖 C++17 标准库。
保留模型元数据、字段绑定、条件树、查询、占位符与参数列表，以及标准 SQL 方言接口。
Java/Maven、重复 SQLite 源码、数据库连接、会话和数据库模式同步层已经移除。
数据库方言和执行由宿主提供；ChaLi 的适配位于 `source/glue/std_sql_ipage.cpp`。

## 构建与验证

在本目录的 `source/` 中执行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows 使用 MinGW 时在配置命令中加 `-G "MinGW Makefiles"`。
也可在提供 POSIX shell 的环境运行 `make`，同时构建示例并执行测试。
示例仅打印生成的 SQL；测试覆盖参数保真、列白名单、排序、分页和默认方言。

## 目录

- `source/ipage/field.hpp`：字段语义、单元格和过滤操作符。
- `source/ipage/model.hpp`：声明期绑定和运行期模型元数据。
- `source/ipage/sql.{hpp,cpp}`：条件、连接、选择列和查询生成。
- `source/ipage/dialect.{hpp,cpp}`：数据库无关的方言接口和默认标准 SQL 方言。

`Query::build()` 返回 SQL 和有序参数；调用方使用自己的数据库驱动绑定参数。
表和列均经过方言引用，过滤、选择和排序字段必须存在于模型中。
