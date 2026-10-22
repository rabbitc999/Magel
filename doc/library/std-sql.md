# std::sql — 数据库

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::sql;
```

## 公共约定与错误边界

SQLite 静态内置，无需 sqlite3.dll，忽略 SQLITE_LIBRARY。MySQL/MariaDB 使用 MYSQL_LIBRARY；PostgreSQL 使用 POSTGRES_LIBRARY；ODBC 需要相应驱动。mysql/maria 默认端口 3306，connect 已移除。exec 返回最后一条语句的结果，失败检查 ok/error；结果已复制，不借用客户端缓冲区。事务由 SQL BEGIN/COMMIT/ROLLBACK 控制。拼字符串字面量使用 db.quote；它不转义表名/列名。目标宿主必须提供相应数据库驱动。 DataCell.asInt/asReal 只转换数值类型，其他类型返回 0/0.0；text 对 NULL 返回 "NULL"。ResultSet.scalar/count 在空结果上返回空串/0，at/intAt/realAt/textAt 的行列索引必须有效。

## 参考代码

完整程序：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

```powershell
chali run doc/examples/library/sqlite.chl
```

<details>
<summary>展开完整参考代码</summary>

```chali
import std::sql;

/** @brief 使用内置 SQLite，在内存库中演示执行、转义、结果与错误处理。 */
void main() {
    Database db = Database::sqlite(":memory:");
    if (!db.isOpen()) { panic(db.lastError()); }
    assert(db.execOk("CREATE TABLE people(id INTEGER PRIMARY KEY, name TEXT)"));
    String name = "中文 O'Reilly 😀";
    ResultSet inserted = db.exec("INSERT INTO people VALUES(1," + db.quote(name) + ")");
    if (!inserted.ok) { panic(inserted.error); }
    ResultSet rows = db.exec("SELECT id,name FROM people");
    assert(rows.ok && rows.rowCount() == 1);
    assert(rows.intAt(0,0) == 1 && rows.textAt(0,1) == name);
    ResultSet duplicate = db.exec("INSERT INTO people VALUES(1,'duplicate')");
    assert(!duplicate.ok);
    db.close();
    println("sqlite API ok");

}
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-408"></a>`struct SqlStatement` | 可独立保存、查看并执行的 SQL 与参数，参数为 JSON 标量数组。 |
| SqlStatement | <a id="symbol-409"></a>`String sql` | 包含占位符的 SQL 文本，不含参数值。 |
| SqlStatement | <a id="symbol-410"></a>`String parameters` | 按占位符顺序保存类型的 JSON 标量数组。 |
| 模块 | <a id="symbol-411"></a>`struct Query` | 值语义的动态查询；复制后可独立修改条件。 |
| Query | <a id="symbol-412"></a>`String model` | 从反射类型与注解构造的模型元数据 JSON。 |
| Query | <a id="symbol-413"></a>`List<String> filters` | 按添加顺序合取的参数化过滤条件。 |
| Query | <a id="symbol-414"></a>`List<String> orders` | 按添加顺序使用的字段排序声明。 |
| Query | <a id="symbol-415"></a>`List<String> columns` | 结果集按顺序保存的列名。 |
| Query | <a id="symbol-416"></a>`int64 limitValue` | 最大返回行数，零表示不限制。 |
| Query | <a id="symbol-417"></a>`int64 offsetValue` | 分页跳过的行数。 |
| 模块 | <a id="symbol-418"></a>`struct DataCell` | 结果集里的一个格子，保留类型与文本两种形态。 |
| DataCell | <a id="symbol-419"></a>`int64 kind` | 单元格种类：NULL、整数、浮点或文本。 |
| DataCell | <a id="symbol-420"></a>`int64 intValue` | 整数单元格保存的值。 |
| DataCell | <a id="symbol-421"></a>`float64 realValue` | 浮点单元格保存的值。 |
| DataCell | <a id="symbol-422"></a>`std::lang::String textValue` | 文本单元格保存的字符串。 |
| 模块 | <a id="symbol-423"></a>`struct DataRow` | 结果集的一行。 |
| DataRow | <a id="symbol-424"></a>`std::lang::List<DataCell> cells` | 该行按列顺序保存的单元格。 |
| 模块 | <a id="symbol-425"></a>`struct ResultSet` | 一次 exec 的结果：状态、错误、影响行数、结果集。 |
| ResultSet | <a id="symbol-426"></a>`bool ok` | 当前解析或请求是否成功。 |
| ResultSet | <a id="symbol-427"></a>`std::lang::String error` | 最近操作的错误说明。 |
| ResultSet | <a id="symbol-428"></a>`int64 affected` | 操作影响的记录数量。 |
| ResultSet | <a id="symbol-429"></a>`std::lang::List<std::lang::String> columns` | 结果集按顺序保存的列名。 |
| ResultSet | <a id="symbol-430"></a>`std::lang::List<DataRow> rows` | 按查询顺序保存的数据行。 |
| 模块 | <a id="symbol-431"></a>`enum Driver` | std::sql 支持的数据库引擎。 |
| Driver | `MySQL, SQLite, PostgreSQL, SqlServer, Odbc, MariaDB` | std::sql 支持的数据库引擎。 变体及载荷见声明。 |
| 模块 | <a id="symbol-432"></a>`#NonCopyable opcode Database` | 不可复制的数据库连接 opcode，状态和释放责任由宿主记录保存。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Query::orderBy](#api-0) | `void orderBy(String field, bool descending = false)` |
| [Query::select](#api-1) | `void select(String field)` |
| [Query::page](#api-2) | `void page(int64 limit, int64 offset = 0)` |
| [Query::build](#api-3) | `Result<SqlStatement,String> build(String operation = "select", String values = "{}")` |
| [DataCell::isNull](#api-4) | `bool isNull()` |
| [DataCell::isNumber](#api-5) | `bool isNumber()` |
| [DataCell::text](#api-6) | `std::lang::String text()` |
| [DataCell::asInt](#api-7) | `int64 asInt()` |
| [DataCell::asReal](#api-8) | `float64 asReal()` |
| [DataRow::size](#api-9) | `int64 size()` |
| [DataRow::at](#api-10) | `DataCell at(int64 index)` |
| [ResultSet::rowCount](#api-11) | `int64 rowCount()` |
| [ResultSet::colCount](#api-12) | `int64 colCount()` |
| [ResultSet::isEmpty](#api-13) | `bool isEmpty()` |
| [ResultSet::columnName](#api-14) | `std::lang::String columnName(int64 index)` |
| [ResultSet::columnIndex](#api-15) | `int64 columnIndex(std::lang::String name)` |
| [ResultSet::at](#api-16) | `DataCell at(int64 row, int64 column)` |
| [ResultSet::textAt](#api-17) | `std::lang::String textAt(int64 row, int64 column)` |
| [ResultSet::intAt](#api-18) | `int64 intAt(int64 row, int64 column)` |
| [ResultSet::realAt](#api-19) | `float64 realAt(int64 row, int64 column)` |
| [ResultSet::scalar](#api-20) | `std::lang::String scalar()` |
| [ResultSet::count](#api-21) | `int64 count()` |
| [ResultSet::pad](#api-22) | `std::lang::String pad(std::lang::String text, int64 width)` |
| [ResultSet::render](#api-23) | `std::lang::String render()` |
| [Database::mysql](#api-24) | `static Database mysql(String host, String user, String password, String database, int32 port)` |
| [Database::sqlite](#api-25) | `static Database sqlite(String path)` |
| [Database::postgres](#api-26) | `static Database postgres(String options)` |
| [Database::odbc](#api-27) | `static Database odbc(String options)` |
| [Database::sqlServer](#api-28) | `static Database sqlServer(String options)` |
| [Database::driver](#api-29) | `Driver driver()` |
| [Database::quote](#api-30) | `std::lang::String quote(std::lang::String text)` |
| [Database::backendName](#api-31) | `std::lang::String backendName()` |
| [Database::mysql](#api-32) | `static Database mysql(std::lang::String host, std::lang::String user, std::lang::String password, std::lang::String database)` |
| [Database::maria](#api-33) | `static Database maria(String host, String user, String password, String database, int32 port = (int32)3306)` |
| [Database::local](#api-34) | `static Database local(std::lang::String database)` |
| [Database::isOpen](#api-35) | `bool isOpen()` |
| [Database::lastError](#api-36) | `std::lang::String lastError()` |
| [Database::affectedRows](#api-37) | `int64 affectedRows()` |
| [Database::close](#api-38) | `void close()` |
| [Database::exec](#api-39) | `ResultSet exec(std::lang::String sql)` |
| [Database::exec](#api-40) | `ResultSet exec(SqlStatement statement)` |
| [Database::execOk](#api-41) | `bool execOk(std::lang::String sql)` |
| [Database::scalar](#api-42) | `std::lang::String scalar(std::lang::String sql)` |
| [Database::count](#api-43) | `int64 count(std::lang::String sql)` |
| [Database::hasTable](#api-44) | `bool hasTable(std::lang::String tableName)` |
| [quote](#api-45) | `std::lang::String quote(std::lang::String text, bool escapeBackslash)` |
| [quote](#api-46) | `std::lang::String quote(std::lang::String text)` |
| [intLiteral](#api-47) | `std::lang::String intLiteral(int64 value)` |
| [realLiteral](#api-48) | `std::lang::String realLiteral(float64 value)` |
| [displayWidth](#api-49) | `int64 displayWidth(std::lang::String text)` |
| [query](#api-50) | `Result<Query,String> query<T>()` |
| [filter](#api-51) | `Result<bool,String> filter<V>(Query mut& query, String field, V value, int64 operation = 0)` |
| [filterMany](#api-52) | `Result<bool,String> filterMany<V>(Query mut& query, String field, List<V> values, int64 operation = 11)` |
| [insert](#api-53) | `Result<SqlStatement,String> insert<T>(Query mut& query, T const& value)` |
| [update](#api-54) | `Result<SqlStatement,String> update<T>(Query mut& query, T const& value)` |

<a id="api-0"></a>

### Query::orderBy（1）

**声明**

```chali
void orderBy(String field, bool descending = false);
```

**行为**

添加排序列；列必须出现在模型中。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `field` | `String field` | 模型中声明的字段名或列名。 | 无 |
| `descending` | `bool descending` | 为 true 时按降序排列。 | `false` |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.orderBy(field, descending);
```

**源码**：[source/std/sql.chl:80](../../source/std/sql.chl#L80)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-1"></a>

### Query::select（2）

**声明**

```chali
void select(String field);
```

**行为**

选择模型字段，默认选择全部字段。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `field` | `String field` | 模型中声明的字段名或列名。 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.select(field);
```

**源码**：[source/std/sql.chl:82](../../source/std/sql.chl#L82)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-2"></a>

### Query::page（3）

**声明**

```chali
void page(int64 limit, int64 offset = 0);
```

**行为**

设置查询分页条数及偏移。

设置非负分页参数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `limit` | `int64 limit` | 最大返回行数，零表示不限制。 | 无 |
| `offset` | `int64 offset` | 字节偏移或 seek 相对位移 | `0` |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.page(limit, offset);
```

**源码**：[source/std/sql.chl:84](../../source/std/sql.chl#L84)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-3"></a>

### Query::build（4）

**声明**

```chali
Result<SqlStatement,String> build(String operation = "select", String values = "{}");
```

**行为**

生成带参数列表的动态 SQL。

生成 SQL 与参数；UPDATE/DELETE 强制要求过滤条件。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `operation` | `String operation` | 动态 SQL 操作名称或 ipage 操作符编号，按入口声明区分。 | `"select"` |
| `values` | `String values` | 参与操作的元素序列 | `"{}"` |

**返回值**

`Result<SqlStatement,String>`。生成带参数列表的动态 SQL。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.build(operation, values);
```

**源码**：[source/std/sql.chl:86](../../source/std/sql.chl#L86)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-4"></a>

### DataCell::isNull（5）

**声明**

```chali
bool isNull();
```

**行为**

判断是否为 null/NULL。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为 null/NULL。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isNull();
```

**源码**：[source/std/sql.chl:163](../../source/std/sql.chl#L163)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-5"></a>

### DataCell::isNumber（6）

**声明**

```chali
bool isNumber();
```

**行为**

判断单元格是否为整数或实数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断单元格是否为整数或实数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isNumber();
```

**源码**：[source/std/sql.chl:164](../../source/std/sql.chl#L164)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-6"></a>

### DataCell::text（7）

**声明**

```chali
std::lang::String text();
```

**行为**

返回单元格的文本表示。

取文本形态；空值给 "NULL"。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。返回单元格的文本表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.text();
```

**源码**：[source/std/sql.chl:167](../../source/std/sql.chl#L167)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-7"></a>

### DataCell::asInt（8）

**声明**

```chali
int64 asInt();
```

**行为**

提取整数值；具体转换规则见所属类型。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。提取整数值；具体转换规则见所属类型。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.asInt();
```

**源码**：[source/std/sql.chl:172](../../source/std/sql.chl#L172)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-8"></a>

### DataCell::asReal（9）

**声明**

```chali
float64 asReal();
```

**行为**

将单元格转换为实数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`float64`。将单元格转换为实数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.asReal();
```

**源码**：[source/std/sql.chl:178](../../source/std/sql.chl#L178)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-9"></a>

### DataRow::size（10）

**声明**

```chali
int64 size();
```

**行为**

返回存储的总长度。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回存储的总长度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.size();
```

**源码**：[source/std/sql.chl:189](../../source/std/sql.chl#L189)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-10"></a>

### DataRow::at（11）

**声明**

```chali
DataCell at(int64 index);
```

**行为**

读取从 0 开始的指定位置。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`DataCell`。读取从 0 开始的指定位置。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.at(index);
```

**源码**：[source/std/sql.chl:190](../../source/std/sql.chl#L190)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-11"></a>

### ResultSet::rowCount（12）

**声明**

```chali
int64 rowCount();
```

**行为**

返回结果行数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回结果行数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rowCount();
```

**源码**：[source/std/sql.chl:201](../../source/std/sql.chl#L201)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-12"></a>

### ResultSet::colCount（13）

**声明**

```chali
int64 colCount();
```

**行为**

返回结果列数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回结果列数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.colCount();
```

**源码**：[source/std/sql.chl:202](../../source/std/sql.chl#L202)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-13"></a>

### ResultSet::isEmpty（14）

**声明**

```chali
bool isEmpty();
```

**行为**

判断 Any 是否为空盒子，不公开内部执行期标签。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 Any 是否为空盒子，不公开内部执行期标签。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isEmpty();
```

**源码**：[source/std/sql.chl:203](../../source/std/sql.chl#L203)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-14"></a>

### ResultSet::columnName（15）

**声明**

```chali
std::lang::String columnName(int64 index);
```

**行为**

按列下标读取列名。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`std::lang::String`。按列下标读取列名。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.columnName(index);
```

**源码**：[source/std/sql.chl:204](../../source/std/sql.chl#L204)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-15"></a>

### ResultSet::columnIndex（16）

**声明**

```chali
int64 columnIndex(std::lang::String name);
```

**行为**

按列名查找列下标。

按列名找下标，找不到给 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |

**返回值**

`int64`。按列名查找列下标。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.columnIndex(name);
```

**源码**：[source/std/sql.chl:207](../../source/std/sql.chl#L207)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-16"></a>

### ResultSet::at（17）

**声明**

```chali
DataCell at(int64 row, int64 column);
```

**行为**

读取从 0 开始的指定位置。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `row` | `int64 row` | 从 0 开始的行下标 | 无 |
| `column` | `int64 column` | 列下标或列名 | 无 |

**返回值**

`DataCell`。读取从 0 开始的指定位置。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.at(row, column);
```

**源码**：[source/std/sql.chl:220](../../source/std/sql.chl#L220)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-17"></a>

### ResultSet::textAt（18）

**声明**

```chali
std::lang::String textAt(int64 row, int64 column);
```

**行为**

读取指定单元格的文本表示。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `row` | `int64 row` | 从 0 开始的行下标 | 无 |
| `column` | `int64 column` | 列下标或列名 | 无 |

**返回值**

`std::lang::String`。读取指定单元格的文本表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.textAt(row, column);
```

**源码**：[source/std/sql.chl:221](../../source/std/sql.chl#L221)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-18"></a>

### ResultSet::intAt（19）

**声明**

```chali
int64 intAt(int64 row, int64 column);
```

**行为**

读取指定单元格的整数表示。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `row` | `int64 row` | 从 0 开始的行下标 | 无 |
| `column` | `int64 column` | 列下标或列名 | 无 |

**返回值**

`int64`。读取指定单元格的整数表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.intAt(row, column);
```

**源码**：[source/std/sql.chl:222](../../source/std/sql.chl#L222)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-19"></a>

### ResultSet::realAt（20）

**声明**

```chali
float64 realAt(int64 row, int64 column);
```

**行为**

读取指定单元格的实数表示。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `row` | `int64 row` | 从 0 开始的行下标 | 无 |
| `column` | `int64 column` | 列下标或列名 | 无 |

**返回值**

`float64`。读取指定单元格的实数表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.realAt(row, column);
```

**源码**：[source/std/sql.chl:223](../../source/std/sql.chl#L223)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-20"></a>

### ResultSet::scalar（21）

**声明**

```chali
std::lang::String scalar();
```

**行为**

读取结果首行首列的文本表示。

第一行第一列文本（常用作标量查询）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。读取结果首行首列的文本表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.scalar();
```

**源码**：[source/std/sql.chl:226](../../source/std/sql.chl#L226)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-21"></a>

### ResultSet::count（22）

**声明**

```chali
int64 count();
```

**行为**

读取结果首行首列的整数表示。

第一行第一列整数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。读取结果首行首列的整数表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.count();
```

**源码**：[source/std/sql.chl:232](../../source/std/sql.chl#L232)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-22"></a>

### ResultSet::pad（23）

**声明**

```chali
std::lang::String pad(std::lang::String text, int64 width);
```

**行为**

按显示宽度补齐单元格文本。

补空格到指定显示宽度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |
| `width` | `int64 width` | 目标字节宽度或定宽字段大小 | 无 |

**返回值**

`std::lang::String`。按显示宽度补齐单元格文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.pad(text, width);
```

**源码**：[source/std/sql.chl:238](../../source/std/sql.chl#L238)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-23"></a>

### ResultSet::render（24）

**声明**

```chali
std::lang::String render();
```

**行为**

将结果集渲染为文本表格。

把结果集渲染成等宽文本表格（方便命令行查看）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。将结果集渲染为文本表格。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.render();
```

**源码**：[source/std/sql.chl:252](../../source/std/sql.chl#L252)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-24"></a>

### Database::mysql（25）

**声明**

```chali
static Database mysql(String host, String user, String password, String database, int32 port);
```

**行为**

连接 MySQL，返回带打开状态及错误信息的 Database。

连接 MySQL，返回拥有连接或失败诊断的独占值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `String host` | 主机名或 IP 地址 | 无 |
| `user` | `String user` | 数据库用户名 | 无 |
| `password` | `String password` | 数据库连接密码 | 无 |
| `database` | `String database` | 数据库名 | 无 |
| `port` | `int32 port` | 端口号 | 无 |

**返回值**

`Database`。连接 MySQL，返回带打开状态及错误信息的 Database。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::Database::mysql(host, user, password, database, port);
```

**源码**：[source/std/sql.chl:359](../../source/std/sql.chl#L359)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-25"></a>

### Database::sqlite（26）

**声明**

```chali
static Database sqlite(String path);
```

**行为**

打开文件 SQLite 或 :memory: 内存库。

打开 SQLite 文件或独立的内存数据库。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 文件/数据库路径 | 无 |

**返回值**

`Database`。打开文件 SQLite 或 :memory: 内存库。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::Database::sqlite(path);
```

**源码**：[source/std/sql.chl:363](../../source/std/sql.chl#L363)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-26"></a>

### Database::postgres（27）

**声明**

```chali
static Database postgres(String options);
```

**行为**

使用 libpq 连接串/URI 打开 PostgreSQL。

使用连接串打开 PostgreSQL。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `options` | `String options` | 后端连接选项/连接串 | 无 |

**返回值**

`Database`。使用 libpq 连接串/URI 打开 PostgreSQL。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::Database::postgres(options);
```

**源码**：[source/std/sql.chl:365](../../source/std/sql.chl#L365)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-27"></a>

### Database::odbc（28）

**声明**

```chali
static Database odbc(String options);
```

**行为**

使用 Unicode ODBC 连接选项打开数据源。

使用 Unicode ODBC 连接串连接数据源。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `options` | `String options` | 后端连接选项/连接串 | 无 |

**返回值**

`Database`。使用 Unicode ODBC 连接选项打开数据源。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::Database::odbc(options);
```

**源码**：[source/std/sql.chl:367](../../source/std/sql.chl#L367)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-28"></a>

### Database::sqlServer（29）

**声明**

```chali
static Database sqlServer(String options);
```

**行为**

使用 ODBC 连接选项打开 SQL Server。

使用已安装的 ODBC 驱动连接 SQL Server。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `options` | `String options` | 后端连接选项/连接串 | 无 |

**返回值**

`Database`。使用 ODBC 连接选项打开 SQL Server。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::Database::sqlServer(options);
```

**源码**：[source/std/sql.chl:369](../../source/std/sql.chl#L369)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-29"></a>

### Database::driver（30）

**声明**

```chali
Driver driver();
```

**行为**

当前连接声明的数据库驱动。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Driver`。当前连接声明的数据库驱动。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.driver();
```

**源码**：[source/std/sql.chl:371](../../source/std/sql.chl#L371)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-30"></a>

### Database::quote（31）

**声明**

```chali
std::lang::String quote(std::lang::String text);
```

**行为**

生成带单引号的 SQL 字符串字面量，按驱动处理反斜杠。

按当前数据库规则生成字符串字面量。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`std::lang::String`。生成带单引号的 SQL 字符串字面量，按驱动处理反斜杠。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.quote(text);
```

**源码**：[source/std/sql.chl:382](../../source/std/sql.chl#L382)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-31"></a>

### Database::backendName（32）

**声明**

```chali
std::lang::String backendName();
```

**行为**

返回驱动标识字符串。

返回当前数据库后端名称。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。返回驱动标识字符串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.backendName();
```

**源码**：[source/std/sql.chl:392](../../source/std/sql.chl#L392)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-32"></a>

### Database::mysql（33）

**声明**

```chali
static Database mysql(std::lang::String host, std::lang::String user, std::lang::String password, std::lang::String database);
```

**行为**

连接 MySQL，返回带打开状态及错误信息的 Database。

默认端口 3306 的连接。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `std::lang::String host` | 主机名或 IP 地址 | 无 |
| `user` | `std::lang::String user` | 数据库用户名 | 无 |
| `password` | `std::lang::String password` | 数据库连接密码 | 无 |
| `database` | `std::lang::String database` | 数据库名 | 无 |

**返回值**

`Database`。连接 MySQL，返回带打开状态及错误信息的 Database。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::Database::mysql(host, user, password, database);
```

**源码**：[source/std/sql.chl:405](../../source/std/sql.chl#L405)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-33"></a>

### Database::maria（34）

**声明**

```chali
static Database maria(String host, String user, String password, String database, int32 port = (int32)3306);
```

**行为**

连接 MariaDB，驱动标识为 mariadb，客户端 ABI 与 MySQL 兼容。

使用兼容的 MySQL/MariaDB 客户端连接 MariaDB，默认端口 3306。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `String host` | 主机名或 IP 地址 | 无 |
| `user` | `String user` | 数据库用户名 | 无 |
| `password` | `String password` | 数据库连接密码 | 无 |
| `database` | `String database` | 数据库名 | 无 |
| `port` | `int32 port` | 端口号 | `(int32)3306` |

**返回值**

`Database`。连接 MariaDB，驱动标识为 mariadb，客户端 ABI 与 MySQL 兼容。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::Database::maria(host, user, password, database, port);
```

**源码**：[source/std/sql.chl:410](../../source/std/sql.chl#L410)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-34"></a>

### Database::local（35）

**声明**

```chali
static Database local(std::lang::String database);
```

**行为**

使用本机默认 MySQL 配置连接 database。

本机开发用的默认连接：127.0.0.1:3306，用户 root，口令 123456。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `database` | `std::lang::String database` | 数据库名 | 无 |

**返回值**

`Database`。使用本机默认 MySQL 配置连接 database。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::Database::local(database);
```

**源码**：[source/std/sql.chl:415](../../source/std/sql.chl#L415)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-35"></a>

### Database::isOpen（36）

**声明**

```chali
bool isOpen();
```

**行为**

判断连接或监听器是否已打开。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断连接或监听器是否已打开。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isOpen();
```

**源码**：[source/std/sql.chl:419](../../source/std/sql.chl#L419)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-36"></a>

### Database::lastError（37）

**声明**

```chali
std::lang::String lastError();
```

**行为**

返回连接/网络操作最近的错误文本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。返回连接/网络操作最近的错误文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.lastError();
```

**源码**：[source/std/sql.chl:420](../../source/std/sql.chl#L420)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-37"></a>

### Database::affectedRows（38）

**声明**

```chali
int64 affectedRows();
```

**行为**

返回连接最近操作的影响行数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回连接最近操作的影响行数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.affectedRows();
```

**源码**：[source/std/sql.chl:421](../../source/std/sql.chl#L421)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-38"></a>

### Database::close（39）

**声明**

```chali
void close();
```

**行为**

关闭底层资源；文档节点会使同文档所有视图失效。

关闭连接；重复调用安全。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.close();
```

**源码**：[source/std/sql.chl:424](../../source/std/sql.chl#L424)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-39"></a>

### Database::exec（40）

**声明**

```chali
ResultSet exec(std::lang::String sql);
```

**行为**

执行 SQL，复制最后一条语句的结果并返回 ResultSet。

执行一条（或一段）SQL。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `sql` | `std::lang::String sql` | SQL 文本；可以包含多条语句，用分号分隔。 | 无 |

**返回值**

`ResultSet`。结果集；`ok` 为假时 `error` 里是服务器给出的诊断。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.exec(sql);
```

**源码**：[source/std/sql.chl:431](../../source/std/sql.chl#L431)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-40"></a>

### Database::exec（41）

**声明**

```chali
ResultSet exec(SqlStatement statement);
```

**行为**

在 SQLite 中绑定并执行一条 SqlStatement，严格检查参数数量并返回独立结果；其他后端报告不支持参数化执行。

执行生成的参数化语句；值通过驱动绑定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `statement` | `SqlStatement statement` | 包含 SQL 和绑定参数的语句值。 | 无 |

**返回值**

`ResultSet`。在 SQLite 中绑定并执行一条 SqlStatement，严格检查参数数量并返回独立结果；其他后端报告不支持参数化执行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.exec(statement);
```

**源码**：[source/std/sql.chl:436](../../source/std/sql.chl#L436)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-41"></a>

### Database::execOk（42）

**声明**

```chali
bool execOk(std::lang::String sql);
```

**行为**

执行 SQL 并仅返回是否成功。

执行并只关心成败。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `sql` | `std::lang::String sql` | SQL 语句文本，可包含多条语句 | 无 |

**返回值**

`bool`。执行 SQL 并仅返回是否成功。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.execOk(sql);
```

**源码**：[source/std/sql.chl:504](../../source/std/sql.chl#L504)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-42"></a>

### Database::scalar（43）

**声明**

```chali
std::lang::String scalar(std::lang::String sql);
```

**行为**

读取结果首行首列的文本表示。

执行一个"取一行一列"的查询，返回文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `sql` | `std::lang::String sql` | SQL 语句文本，可包含多条语句 | 无 |

**返回值**

`std::lang::String`。读取结果首行首列的文本表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.scalar(sql);
```

**源码**：[source/std/sql.chl:510](../../source/std/sql.chl#L510)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-43"></a>

### Database::count（44）

**声明**

```chali
int64 count(std::lang::String sql);
```

**行为**

读取结果首行首列的整数表示。

执行一个 `SELECT COUNT(...)` 之类的查询，返回整数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `sql` | `std::lang::String sql` | SQL 语句文本，可包含多条语句 | 无 |

**返回值**

`int64`。读取结果首行首列的整数表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.count(sql);
```

**源码**：[source/std/sql.chl:517](../../source/std/sql.chl#L517)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-44"></a>

### Database::hasTable（45）

**声明**

```chali
bool hasTable(std::lang::String tableName);
```

**行为**

检查指定表是否存在。

当前所选库里表是否存在。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `tableName` | `std::lang::String tableName` | 数据库表名 | 无 |

**返回值**

`bool`。检查指定表是否存在。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.hasTable(tableName);
```

**源码**：[source/std/sql.chl:524](../../source/std/sql.chl#L524)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-45"></a>

### quote（46）

**声明**

```chali
std::lang::String quote(std::lang::String text, bool escapeBackslash);
```

**行为**

生成带单引号的 SQL 字符串字面量，按驱动处理反斜杠。

把文本包成 SQL 字符串字面量，始终转义单引号，可选择转义反斜杠。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |
| `escapeBackslash` | `bool escapeBackslash` | 是否对反斜杠进行 MySQL 风格转义 | 无 |

**返回值**

`std::lang::String`。生成带单引号的 SQL 字符串字面量，按驱动处理反斜杠。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::quote(text, escapeBackslash);
```

**源码**：[source/std/sql.chl:551](../../source/std/sql.chl#L551)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-46"></a>

### quote（47）

**声明**

```chali
std::lang::String quote(std::lang::String text);
```

**行为**

生成带单引号的 SQL 字符串字面量，按驱动处理反斜杠。

按 MySQL 规则转义；跨后端代码请调用 db.quote(text)。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`std::lang::String`。生成带单引号的 SQL 字符串字面量，按驱动处理反斜杠。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::quote(text);
```

**源码**：[source/std/sql.chl:574](../../source/std/sql.chl#L574)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-47"></a>

### intLiteral（48）

**声明**

```chali
std::lang::String intLiteral(int64 value);
```

**行为**

生成整数 SQL 字面量。

把整数写成 SQL 字面量。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`std::lang::String`。生成整数 SQL 字面量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::intLiteral(value);
```

**源码**：[source/std/sql.chl:577](../../source/std/sql.chl#L577)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-48"></a>

### realLiteral（49）

**声明**

```chali
std::lang::String realLiteral(float64 value);
```

**行为**

生成实数 SQL 字面量。

把实数写成 SQL 字面量。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`std::lang::String`。生成实数 SQL 字面量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::realLiteral(value);
```

**源码**：[source/std/sql.chl:580](../../source/std/sql.chl#L580)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-49"></a>

### displayWidth（50）

**声明**

```chali
int64 displayWidth(std::lang::String text);
```

**行为**

估算文本表格显示宽度；ASCII/双字节按 1，三/四字节编码按 2。

按终端显示宽度算长度：ASCII 一列，中日韩等宽字符两列。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`int64`。估算文本表格显示宽度；ASCII/双字节按 1，三/四字节编码按 2。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::displayWidth(text);
```

**源码**：[source/std/sql.chl:583](../../source/std/sql.chl#L583)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-50"></a>

### query（51）

**声明**

```chali
Result<Query,String> query<T>();
```

**行为**

从模型注解与反射元数据建立 ipage 查询。

从带模型注解的反射类型创建查询。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<Query,String>`。从模型注解与反射元数据建立 ipage 查询。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::query<T>();
```

**源码**：[source/std/sql.chl:597](../../source/std/sql.chl#L597)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-51"></a>

### filter（52）

**声明**

```chali
Result<bool,String> filter<V>(Query mut& query, String field, V value, int64 operation = 0);
```

**行为**

为动态 SQL 添加参数化单值条件。

添加参数化条件；操作符编号与 ipage OperatorType 一致，默认相等。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `query` | `Query mut& query` | 需要配置或生成语句的查询对象引用。 | 无 |
| `field` | `String field` | 模型中声明的字段名或列名。 | 无 |
| `value` | `V value` | 输入值或写入的新值 | 无 |
| `operation` | `int64 operation` | 动态 SQL 操作名称或 ipage 操作符编号，按入口声明区分。 | `0` |

**返回值**

`Result<bool,String>`。为动态 SQL 添加参数化单值条件。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::filter<V>(query, field, value, operation);
```

**源码**：[source/std/sql.chl:605](../../source/std/sql.chl#L605)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-52"></a>

### filterMany（53）

**声明**

```chali
Result<bool,String> filterMany<V>(Query mut& query, String field, List<V> values, int64 operation = 11);
```

**行为**

为动态 SQL 添加多参数或空值条件。

添加 IN 或 NOT IN 等多参数条件。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `query` | `Query mut& query` | 需要配置或生成语句的查询对象引用。 | 无 |
| `field` | `String field` | 模型中声明的字段名或列名。 | 无 |
| `values` | `List<V> values` | 参与操作的元素序列 | 无 |
| `operation` | `int64 operation` | 动态 SQL 操作名称或 ipage 操作符编号，按入口声明区分。 | `11` |

**返回值**

`Result<bool,String>`。为动态 SQL 添加多参数或空值条件。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::filterMany<V>(query, field, values, operation);
```

**源码**：[source/std/sql.chl:612](../../source/std/sql.chl#L612)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-53"></a>

### insert（54）

**声明**

```chali
Result<SqlStatement,String> insert<T>(Query mut& query, T const& value);
```

**行为**

按反射模型值生成参数化 INSERT，返回 SQL 与有序参数列表。

使用模型值生成参数化 INSERT。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `query` | `Query mut& query` | 需要配置或生成语句的查询对象引用。 | 无 |
| `value` | `T const& value` | 输入值或写入的新值 | 无 |

**返回值**

`Result<SqlStatement,String>`。按反射模型值生成参数化 INSERT，返回 SQL 与有序参数列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::insert<T>(query, value);
```

**源码**：[source/std/sql.chl:620](../../source/std/sql.chl#L620)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。

<a id="api-54"></a>

### update（55）

**声明**

```chali
Result<SqlStatement,String> update<T>(Query mut& query, T const& value);
```

**行为**

按反射模型值生成参数化更新。

使用模型值生成参数化 UPDATE，主键不会被修改。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `query` | `Query mut& query` | 需要配置或生成语句的查询对象引用。 | 无 |
| `value` | `T const& value` | 输入值或写入的新值 | 无 |

**返回值**

`Result<SqlStatement,String>`。按反射模型值生成参数化更新。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::sql::update<T>(query, value);
```

**源码**：[source/std/sql.chl:626](../../source/std/sql.chl#L626)。

**完整示例**：[doc/examples/library/sqlite.chl](../../doc/examples/library/sqlite.chl)。
