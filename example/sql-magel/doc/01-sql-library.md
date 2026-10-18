# 01 std::sql —— Magel 连接外部 SQL 数据库（magel/example/sql-magel 接口参考）


## 当前支持的数据库

`std::sql` 提供统一的连接、执行和结果集接口，SQL 文本由所选数据库执行。各数据库的建表语法、分页、函数和事务规则仍遵循自己的 SQL 方言，不自动翻译。

| 后端 | 连接入口 | 运行期依赖 |
| --- | --- | --- |
| MySQL | `Database::mysql(host, user, password, database, port)`；保留 `local(database)` | MySQL/MariaDB 客户端库；`MYSQL_LIBRARY` 可指定路径 |
| MariaDB | `Database::maria(host, user, password, database[, port])`，默认端口 3306 | 兼容 MySQL/MariaDB 客户端库与服务；`MYSQL_LIBRARY` |
| SQLite | `Database::sqlite(path)`，内存库用 `":memory:"` | 静态内置 SQLite 3.54.0，无需动态库或数据库服务 |
| PostgreSQL | `Database::postgres(options)`，支持 libpq 连接串或 URI | libpq；`POSTGRES_LIBRARY` 可指定路径，需要 PostgreSQL 服务 |
| Microsoft SQL Server | `Database::sqlServer(options)` | ODBC 管理器和已安装的 SQL Server ODBC 驱动，需要 SQL Server 服务 |
| 通用 ODBC | `Database::odbc(options)` | ODBC 管理器及对应驱动；`ODBC_LIBRARY` 可指定管理器路径 |

Windows ODBC 管理器默认使用 `odbc32.dll`，Linux/macOS 查找 unixODBC。SQLite 已静态编译进桌面 Magel，使用 SQL 的 AOT 程序也静态链接 SQLite；`SQLITE_LIBRARY` 不再生效。PostgreSQL 在其他平台查找 `libpq`；MySQL、libpq、ODBC 及对应驱动仍需在部署时提供。

### 静态 SQLite 的构建与交付

官方 amalgamation 源码固定为 SQLite 3.54.0，位于 `sqlite/`，来源与校验值见 [依赖说明](../../sqlite/README.md)。以 C 编译，启用 `SQLITE_THREADSAFE=1`、禁用动态扩展加载；MySQL/MariaDB、PostgreSQL、ODBC 仍使用动态客户端。

普通运行和使用 SQL 的 AOT 可执行文件无需 SQLite DLL。`.svmb` 仍保存指令，由桌面宿主的内置 SQLite 提供服务。嵌入式模式仍禁用 SQL。

AOT 使用 `--cc` / `CC` / `gcc` 编译 SQLite C 源码，再用 C++ 编译器静态链接；普通构建、`--make`、`--separate-modules` 均支持。未使用 SQL 时不生成或链接 SQLite 目标文件。SQLite 源码属于工具链内嵌资产，脱离仓库也能导出。

默认清理 SQLite 中间源码和目标文件。`--cpp-output-dir` 保留 C++ 源码及配套 `<exe>.sqlite3.c`，手动重建时须用 C 编译器编译该 C 文件（采用上述宏），然后与 C++ 目标文件一起链接；交付最终可执行文件时无需这些源码。`--keep-intermediate` 可保留目标文件。

```mc
import std::sql;
Database db = Database::sqlite(":memory:");
if (!db.isOpen()) { std::lang::panic(db.lastError()); }
ResultSet created = db.exec("CREATE TABLE items(id INTEGER PRIMARY KEY, name TEXT)");
ResultSet inserted = db.exec("INSERT INTO items VALUES(1, " + db.quote("中文 O'Reilly") + ")");
ResultSet rows = db.exec("SELECT id, name FROM items");
std::lang::println("{}", rows.render());
db.close();
```

其他连接入口：

```mc
Database pg = Database::postgres("host=127.0.0.1 port=5432 dbname=demo user=demo connect_timeout=5");
Database ms = Database::sqlServer("DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=demo;Trusted_Connection=yes;Encrypt=yes;");
```

`Database::connect` 已更名为 `Database::mysql`；MariaDB 使用 `Database::maria`。两者共用兼容客户端 ABI 和 `MYSQL_LIBRARY`，`local(database)` 仍是 MySQL 的本机快捷入口。

`driver` 保存 `Driver` 枚举，`backendName()` 返回 `mysql` / `mariadb` / `sqlite` / `postgresql` / `sqlserver` / `odbc`。跨后端拼接字符串字面量使用 **`db.quote(text)`**：MySQL 转义反斜杠，SQLite/PostgreSQL保留反斜杠，SQL Server 同时添加 Unicode 字面量前缀 `N`。全局 `quote(text)` 保留原 MySQL 行为。该接口处理值字面量，不处理表名、列名等标识符。

SQLite/PostgreSQL 的结果类型来自引擎列类型；ODBC 来自驱动列类型，数字形状的 TEXT 保持文本。整数与影响行数使用 int64；实数使用 float64。SQLite/ODBC 暂不提供二进制列接口，查询 BLOB 时返回明确错误，可在 SQL 中先转为十六进制文本。PostgreSQL 的 bytea 采用 libpq 文本表示；COPY 流不属于同步 ResultSet 接口，检测到后关闭连接并报告错误。

`hasTable(name)` 分别查询 MySQL、SQLite、PostgreSQL、SQL Server 的当前库或当前 schema 元数据；通用 ODBC 返回说明并要求调用方执行数据库自己的元数据查询。多语句执行返回最后一条语句的结果；事务是否自动包裹由数据库决定，PostgreSQL 的同次多语句 `PQexec` 有自己的事务规则。

测试入口（在仓库根目录运行）：

```sh
python tests/magel_sqlite.py
python tests/magel_sql_connections.py
python tests/magel_sql_backends.py
```

SQLite 测试将外部库路径设为不存在的文件，验证解释执行、普通 AOT、make、模块缓存和内嵌资产导出；生成的程序复制到隔离目录运行，只在 build 下写测试数据库。PostgreSQL 测试使用标明身份的 libpq ABI 夹具，覆盖错误连接、初始化失败、NULL、类型、64 位结果与重复执行。ODBC 实库检查优先使用 Windows 已安装的 SQLite3 ODBC Driver，其他环境可通过 `MAGEL_TEST_ODBC_DRIVER` 指定 SQLite ODBC 驱动；缺少驱动会明确报告未运行。夹具和 SQLite ODBC 测试不等于连接过真实 PostgreSQL 或 SQL Server 服务。

实现契约参考 [SQLite prepare](https://www.sqlite.org/c3ref/prepare.html)、[PostgreSQL libpq 执行](https://www.postgresql.org/docs/current/libpq-exec.html)、[ODBC SQLDriverConnect](https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqldriverconnect-function) 与 [SQLGetData](https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdata-function)。

以下保留原 MySQL 实现与学生管理示例的详细说明。MySQL 专有符号、加载规则和实现细节只适用于该后端；多数据库统一行为以上文、当前源码及 [`doc/library/std-sql.md`](../../doc/library/std-sql.md) 为准。

## 1. 分层

```
Magel 程序  import std::sql;
   │   std::sql::Database / ResultSet / DataCell      ← std/sql.mgl
   ▼
C++ 胶水  __sql* 句柄接口                              ← glue/std_sql.hpp
   │   （把 MySQL 的指针收进句柄表，只暴露整数句柄 / int64 / float64 / String）
   ▼
外部 SQL 引擎  MySQL 客户端库 libmysql                 ← 运行期动态加载，不链接导入库
```

**不自己实现 SQL。** 语句解析、执行计划、索引、事务、字符集都由 MySQL 负责；胶水层
只做三件事：连接、发语句、把结果集搬成 Magel 能用的形状。

## 2. 为什么需要一层胶水

MySQL 的 C API 全是指针（`MYSQL *`、`MYSQL_RES *`、`MYSQL_ROW`、`MYSQL_FIELD *`），
Magel 一侧以宿主句柄表示这些连接和结果集；Magel 本身同时支持字符串、结构体、枚举与容器。MySQL 胶水将原生指针保存在内部表中：

* 连接表 `databases()`：最多 32 个连接，句柄是槽位下标；
* 结果集表 `results()`：最多 256 个结果集，`exec` 返回的句柄同样是下标。

`exec` 会把结果集**整段取回**（行 × 列 → 文本/整数/实数），所以 Magel 侧可以随便
按行列下标读，不存在游标失效问题；读完由 `exec` 内部 `__sqlFree` 释放。

客户端库在**运行期**用 `LoadLibraryA` / `dlopen` 加载，构建时不链接导入库。查找顺序：

1. 环境变量 `MYSQL_LIBRARY`（完整路径）；
2. Windows：`C:\Program Files\MySQL\MySQL Server <版本>\lib\libmysql.dll`
   （同时把同目录树的 `bin` 加进 DLL 搜索路径，OpenSSL 依赖在那里）；
3. 系统搜索路径里的 `libmysql.dll` / `libmysqlclient.dll` / `libmariadb.dll`；
4. 其它平台：`libmysqlclient.so.21` / `.so` / `libmariadb.so` / `.dylib`。

找不到时 `Database::lastError()` 会说明原因，`mysql` / `maria` 返回“未打开”的连接。

## 3. 接口清单

### 3.1 Database

| 成员 | 说明 |
|---|---|
| `static Database mysql(host, user, password, database, port)` | 连接指定主机与库；`database` 为空表示先不选库 |
| `static Database mysql(host, user, password, database)` | 默认端口 3306 |
| `static Database maria(host, user, password, database[, port])` | MariaDB，默认端口 3306 |
| `static Database local(database)` | 本机开发默认：`127.0.0.1:3306`、`root` / `123456` |
| `static Database sqlite(path)` / `postgres(options)` / `sqlServer(options)` / `odbc(options)` | 对应后端连接入口 |
| `String backendName()` / `String quote(text)` | 驱动名与后端对应的字符串字面量 |
| `bool isOpen()` | 连接是否可用 |
| `String lastError()` | 最近一次失败原因（连接期错误也在里面） |
| `int64 affectedRows()` | 最近一次写的影响行数 |
| `void close()` | 关闭连接；重复调用安全 |
| `ResultSet exec(sql)` | 执行一条或多条语句（分号分隔），返回结果集 |
| `bool execOk(sql)` | 只关心成败 |
| `String scalar(sql)` | 一行一列的文本结果 |
| `int64 count(sql)` | 一行一列的整数结果（`SELECT COUNT(*)` 常用） |
| `bool hasTable(name)` | 当前所选库里表是否存在 |

### 3.2 ResultSet

`ok` / `error` / `affected` / `columns` / `rows` 是字段，另外提供
`rowCount()`、`colCount()`、`isEmpty()`、`columnName(i)`、`columnIndex(name)`、
`at(r, c)`、`textAt(r, c)`、`intAt(r, c)`、`realAt(r, c)`、`scalar()`、`count()`，
以及把整张表渲染成等宽文本的 `render()`。

### 3.3 DataCell

`kind`（`CELL_NULL` / `CELL_INT` / `CELL_REAL` / `CELL_TEXT`）、`intValue`、
`realValue`、`textValue`，方法 `isNull()` / `isNumber()` / `text()` / `asInt()` /
`asReal()`。

### 3.4 辅助函数

| 函数 | 说明 |
|---|---|
| `quote(text)` | 包成 SQL 字符串字面量，转义 `'` 与 `\` |
| `intLiteral(v)` / `realLiteral(v)` | 整数 / 实数字面量 |
| `displayWidth(text)` | UTF-8 显示宽度（非续字节计数） |

## 4. 实现要点

1. **多结果集**：连接时带上 `CLIENT_MULTI_STATEMENTS`，`exec` 按 MySQL 官方写法推进
   ——`mysql_store_result` 取结果、`mysql_next_result` 的 `0` 表示还有、
   `-1` 表示没有更多、`>0` 才是错误。`USE`、`SET` 这类没有结果集的语句拿到的正是
   `-1`，不能当成失败。

2. **影响行数**：`mysql_affected_rows` 在 `mysql_next_result` 返回「没有更多结果」
   之后会被重置，所以语句跑完再问连接只会得到 0。胶水在推进结果集的循环里把值抄进
   结果句柄，Magel 侧用 `__sqlResultAffected` 取，`UPDATE` / `DELETE` 的影响行数才准。

3. **格子类型**：MySQL 根据客户端返回的列类型区分整数、实数与文本，
   文本列中的 `"123"` 保持 `CELL_TEXT`；`NULL` 单独标 `CELL_NULL`。

4. **字符集**：连接成功后设 `utf8mb4`，中文进出都按 UTF-8。

5. **错误文本**：常驻的错误消息放在定长 `char[512]` 缓冲里，不用 `Memory::String`：
   宿主侧运行时分配器在进程退出时会核对活动块，长期挂着的 `Memory::String` 会被
   算成泄漏；只有返回值（VM 会立刻复制走）才临时构造字符串。

## 5. 字符串复制与资源边界

字符串遵循值语义。`String copy = text;` 可以从形参取得独立副本，`pad(result.textAt(r, c), 12)` 也可以直接传入拥有型临时结果；调用点负责临时实参的生命周期。早期的形参复制和临时实参缺陷已修复，不应继续把规避写法描述为当前限制。

SQL 客户端提供的借用缓冲区仍须由胶水复制为拥有型字符串，不能让结果集释放后的原生地址逃入语言值。连接和结果资源的清理需分别验证；语言字符串没有泄漏不等于数据库客户端句柄已经释放。当前规则见[核心概念 §1–4](../../doc/03-core-concepts.md)，实库回归入口为 `tests/magel_sqlite.py` 和 `tests/magel_sql_backends.py`。

## 6. 示例：学生信息管理系统

[`../example/student/main.mgl`](../example/student/main.mgl) 是需求里的学生信息管理系统：

* 启动时连 MySQL、建库 `student_db`、执行 `sql/schema.sql` 与 `sql/seed.sql`；
* 学生 / 班级 / 成绩三套子菜单，增删改查、显示全部、成绩统计；
* 成绩列表用一条 `LEFT JOIN` + `CASE WHEN` 算出等级，统计用
  `COUNT/AVG/MAX/MIN/SUM(CASE …)` 一次取回；
* `--answers 文件` 从 UTF-8 文件读回答，自检脚本据此对 stdout 逐行比对。

[`../example/ipage-student/main.mgl`](../example/ipage-student/main.mgl) 是**同类需求的第二份实现**，
数据换成外部工程：它读写 ipage-student（Java + Spring MVC + MyBatis）自己的 `student` 库，
不依赖那一侧的框架，并把后者的登录与权限门限搬了过来：

* 登录核对 `user` 表，按 `user_permission` 的四个值决定可见菜单；
* 权限判定沿用「用户等级 >= 门槛等级」，门限取自对方各模型 `@ModelClass` 的
  `look` / `edit` / `delete`（学生/成绩的门限是学生，班级/课程是教师，用户管理是超级管理员）；
* 班级、课程、学生三张列表分别带 `COUNT` / `AVG` 聚合列，成绩列表连表算等级，另有按课程与
  按班级两组统计；
* `--init <student.sql>` 在库里缺表时就地建库灌数据，`--reinit` 强制重导入。

两份示例各带一个冒烟自检：`tests/check.sh`（`student_db`）与 `tests/check-ipage.sh`
（独立测试库 `student_check` + `tests/fixture_ipage.sql` 确定性夹具，不碰真实库）。

## 7. 本机数据库集成回归

`tests/magel_sql_live.py` 通过环境变量 `MAGEL_SQL_TEST_PASSWORD` 读取密码，并通过标准输入传给测试程序；源码不写连接密码。默认连接本机 MySQL 的 `root` 用户、`mysql` 数据库，以及 PostgreSQL 的 `postgres` 用户、`postgres` 数据库。客户端库可通过 `MYSQL_LIBRARY`、`POSTGRES_LIBRARY` 指定。

回归仅创建连接内的临时表，验证 VM/AOT 的 CRUD、事务提交与回滚、64 位整数、数值外观文本、中文/emoji、NULL、重复主键后的恢复与重复关闭。临时表在连接关闭后删除。MariaDB 的入口和默认端口由 `tests/magel_sql_connections.py` 的 ABI 夹具验证；夹具通过不代表真实 MariaDB 服务通过。

2026-10-12 本机验收：MySQL 8.0.37、PostgreSQL 17.2 的上述真实服务测试均通过 VM/AOT；SQLite 通过解释执行、普通 AOT、Makefile、分模块及离线资产导出，ODBC 的 SQLite 驱动通过真实读写回归。SQL Server、MariaDB 未安装，未声明真实服务验收通过。
