# std::sql — 数据库

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::sql;
```

## 公共约定与错误边界

SQLite 静态内置，无需 sqlite3.dll，忽略 SQLITE_LIBRARY。MySQL/MariaDB 使用 MYSQL_LIBRARY；PostgreSQL 使用 POSTGRES_LIBRARY；ODBC 需要相应驱动。mysql/maria 默认端口 3306，connect 已移除。exec 返回最后一条语句的结果，失败检查 ok/error；结果已复制，不借用客户端缓冲区。事务由 SQL BEGIN/COMMIT/ROLLBACK 控制。拼字符串字面量使用 db.quote；它不转义表名/列名。目标宿主必须提供相应数据库驱动。 DataCell.asInt/asReal 只转换数值类型，其他类型返回 0/0.0；text 对 NULL 返回 "NULL"。ResultSet.scalar/count 在空结果上返回空串/0，at/intAt/realAt/textAt 的行列索引必须有效。

## 参考代码

完整程序：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

```powershell
magel run doc/examples/library/sqlite.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::sql;

/** @brief 使用内置 SQLite，在内存库中演示执行、转义、结果与错误处理。 */
int64 main() {
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
    return 0;
}
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-440"></a>`struct DataCell` | 结果集里的一个格子，保留类型与文本两种形态。 |
| DataCell | <a id="symbol-441"></a>`int64 kind` | 单元格种类：NULL、整数、浮点或文本。 |
| DataCell | <a id="symbol-442"></a>`int64 intValue` | 整数单元格保存的值。 |
| DataCell | <a id="symbol-443"></a>`float64 realValue` | 浮点单元格保存的值。 |
| DataCell | <a id="symbol-444"></a>`std::lang::String textValue` | 文本单元格保存的字符串。 |
| 模块 | <a id="symbol-445"></a>`struct DataRow` | 结果集的一行。 |
| DataRow | <a id="symbol-446"></a>`std::lang::List<DataCell> cells` | 该行按列顺序保存的单元格。 |
| 模块 | <a id="symbol-447"></a>`struct ResultSet` | 一次 exec 的结果：状态、错误、影响行数、结果集。 |
| ResultSet | <a id="symbol-448"></a>`bool ok` | 当前解析或请求是否成功。 |
| ResultSet | <a id="symbol-449"></a>`std::lang::String error` | 最近操作的错误说明。 |
| ResultSet | <a id="symbol-450"></a>`int64 affected` | 操作影响的记录数量。 |
| ResultSet | <a id="symbol-451"></a>`std::lang::List<std::lang::String> columns` | 结果集按顺序保存的列名。 |
| ResultSet | <a id="symbol-452"></a>`std::lang::List<DataRow> rows` | 按查询顺序保存的数据行。 |
| 模块 | <a id="symbol-453"></a>`enum Driver` | std::sql 支持的数据库引擎。 |
| Driver | `MySQL, SQLite, PostgreSQL, SqlServer, Odbc, MariaDB` | std::sql 支持的数据库引擎。 变体及载荷见声明。 |
| 模块 | <a id="symbol-454"></a>`struct Database` | 一个 SQL 数据库连接（MySQL / SQLite / PostgreSQL / SQL Server / ODBC）。 |
| Database | <a id="symbol-455"></a>`int32 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |
| Database | <a id="symbol-456"></a>`bool opened` | 外部资源是否处于打开状态。 |
| Database | <a id="symbol-457"></a>`Driver driver` | 数据库后端驱动。 |
| Database | <a id="symbol-458"></a>`std::lang::String error` | 最近操作的错误说明。 |
| Database | <a id="symbol-459"></a>`int64 affected` | 操作影响的记录数量。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [DataCell::isNull](#api-0) | `bool isNull()` |
| [DataCell::isNumber](#api-1) | `bool isNumber()` |
| [DataCell::text](#api-2) | `std::lang::String text()` |
| [DataCell::asInt](#api-3) | `int64 asInt()` |
| [DataCell::asReal](#api-4) | `float64 asReal()` |
| [DataRow::size](#api-5) | `int64 size()` |
| [DataRow::at](#api-6) | `DataCell at(int64 index)` |
| [ResultSet::rowCount](#api-7) | `int64 rowCount()` |
| [ResultSet::colCount](#api-8) | `int64 colCount()` |
| [ResultSet::isEmpty](#api-9) | `bool isEmpty()` |
| [ResultSet::columnName](#api-10) | `std::lang::String columnName(int64 index)` |
| [ResultSet::columnIndex](#api-11) | `int64 columnIndex(std::lang::String name)` |
| [ResultSet::at](#api-12) | `DataCell at(int64 row, int64 column)` |
| [ResultSet::textAt](#api-13) | `std::lang::String textAt(int64 row, int64 column)` |
| [ResultSet::intAt](#api-14) | `int64 intAt(int64 row, int64 column)` |
| [ResultSet::realAt](#api-15) | `float64 realAt(int64 row, int64 column)` |
| [ResultSet::scalar](#api-16) | `std::lang::String scalar()` |
| [ResultSet::count](#api-17) | `int64 count()` |
| [ResultSet::pad](#api-18) | `std::lang::String pad(std::lang::String text, int64 width)` |
| [ResultSet::render](#api-19) | `std::lang::String render()` |
| [Database::mysql](#api-20) | `static Database mysql(std::lang::String host, std::lang::String user, std::lang::String password, std::lang::String database, int32 port)` |
| [Database::sqlite](#api-21) | `static Database sqlite(std::lang::String path)` |
| [Database::postgres](#api-22) | `static Database postgres(std::lang::String options)` |
| [Database::odbc](#api-23) | `static Database odbc(std::lang::String options)` |
| [Database::sqlServer](#api-24) | `static Database sqlServer(std::lang::String options)` |
| [Database::quote](#api-25) | `std::lang::String quote(std::lang::String text)` |
| [Database::backendName](#api-26) | `std::lang::String backendName()` |
| [Database::mysql](#api-27) | `static Database mysql(std::lang::String host, std::lang::String user, std::lang::String password, std::lang::String database)` |
| [Database::maria](#api-28) | `static Database maria(std::lang::String host, std::lang::String user, std::lang::String password, std::lang::String database, int32 port = (int32)3306)` |
| [Database::local](#api-29) | `static Database local(std::lang::String database)` |
| [Database::isOpen](#api-30) | `bool isOpen()` |
| [Database::lastError](#api-31) | `std::lang::String lastError()` |
| [Database::affectedRows](#api-32) | `int64 affectedRows()` |
| [Database::close](#api-33) | `void close()` |
| [Database::exec](#api-34) | `ResultSet exec(std::lang::String sql)` |
| [Database::execOk](#api-35) | `bool execOk(std::lang::String sql)` |
| [Database::scalar](#api-36) | `std::lang::String scalar(std::lang::String sql)` |
| [Database::count](#api-37) | `int64 count(std::lang::String sql)` |
| [Database::hasTable](#api-38) | `bool hasTable(std::lang::String tableName)` |
| [quote](#api-39) | `std::lang::String quote(std::lang::String text, bool escapeBackslash)` |
| [quote](#api-40) | `std::lang::String quote(std::lang::String text)` |
| [intLiteral](#api-41) | `std::lang::String intLiteral(int64 value)` |
| [realLiteral](#api-42) | `std::lang::String realLiteral(float64 value)` |
| [displayWidth](#api-43) | `int64 displayWidth(std::lang::String text)` |

<a id="api-0"></a>

### DataCell::isNull（1）

**声明**

```magel
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

```magel
obj.isNull();
```

**源码**：[source/std/sql.mgl:63](../../source/std/sql.mgl#L63)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-1"></a>

### DataCell::isNumber（2）

**声明**

```magel
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

```magel
obj.isNumber();
```

**源码**：[source/std/sql.mgl:64](../../source/std/sql.mgl#L64)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-2"></a>

### DataCell::text（3）

**声明**

```magel
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

```magel
obj.text();
```

**源码**：[source/std/sql.mgl:67](../../source/std/sql.mgl#L67)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-3"></a>

### DataCell::asInt（4）

**声明**

```magel
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

```magel
obj.asInt();
```

**源码**：[source/std/sql.mgl:72](../../source/std/sql.mgl#L72)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-4"></a>

### DataCell::asReal（5）

**声明**

```magel
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

```magel
obj.asReal();
```

**源码**：[source/std/sql.mgl:78](../../source/std/sql.mgl#L78)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-5"></a>

### DataRow::size（6）

**声明**

```magel
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

```magel
obj.size();
```

**源码**：[source/std/sql.mgl:89](../../source/std/sql.mgl#L89)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-6"></a>

### DataRow::at（7）

**声明**

```magel
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

```magel
obj.at(index);
```

**源码**：[source/std/sql.mgl:90](../../source/std/sql.mgl#L90)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-7"></a>

### ResultSet::rowCount（8）

**声明**

```magel
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

```magel
obj.rowCount();
```

**源码**：[source/std/sql.mgl:101](../../source/std/sql.mgl#L101)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-8"></a>

### ResultSet::colCount（9）

**声明**

```magel
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

```magel
obj.colCount();
```

**源码**：[source/std/sql.mgl:102](../../source/std/sql.mgl#L102)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-9"></a>

### ResultSet::isEmpty（10）

**声明**

```magel
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

```magel
obj.isEmpty();
```

**源码**：[source/std/sql.mgl:103](../../source/std/sql.mgl#L103)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-10"></a>

### ResultSet::columnName（11）

**声明**

```magel
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

```magel
obj.columnName(index);
```

**源码**：[source/std/sql.mgl:104](../../source/std/sql.mgl#L104)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-11"></a>

### ResultSet::columnIndex（12）

**声明**

```magel
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

```magel
obj.columnIndex(name);
```

**源码**：[source/std/sql.mgl:107](../../source/std/sql.mgl#L107)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-12"></a>

### ResultSet::at（13）

**声明**

```magel
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

```magel
obj.at(row, column);
```

**源码**：[source/std/sql.mgl:120](../../source/std/sql.mgl#L120)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-13"></a>

### ResultSet::textAt（14）

**声明**

```magel
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

```magel
obj.textAt(row, column);
```

**源码**：[source/std/sql.mgl:121](../../source/std/sql.mgl#L121)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-14"></a>

### ResultSet::intAt（15）

**声明**

```magel
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

```magel
obj.intAt(row, column);
```

**源码**：[source/std/sql.mgl:122](../../source/std/sql.mgl#L122)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-15"></a>

### ResultSet::realAt（16）

**声明**

```magel
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

```magel
obj.realAt(row, column);
```

**源码**：[source/std/sql.mgl:123](../../source/std/sql.mgl#L123)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-16"></a>

### ResultSet::scalar（17）

**声明**

```magel
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

```magel
obj.scalar();
```

**源码**：[source/std/sql.mgl:126](../../source/std/sql.mgl#L126)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-17"></a>

### ResultSet::count（18）

**声明**

```magel
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

```magel
obj.count();
```

**源码**：[source/std/sql.mgl:132](../../source/std/sql.mgl#L132)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-18"></a>

### ResultSet::pad（19）

**声明**

```magel
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

```magel
obj.pad(text, width);
```

**源码**：[source/std/sql.mgl:138](../../source/std/sql.mgl#L138)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-19"></a>

### ResultSet::render（20）

**声明**

```magel
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

```magel
obj.render();
```

**源码**：[source/std/sql.mgl:152](../../source/std/sql.mgl#L152)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-20"></a>

### Database::mysql（21）

**声明**

```magel
static Database mysql(std::lang::String host, std::lang::String user, std::lang::String password, std::lang::String database, int32 port);
```

**行为**

连接 MySQL，返回带打开状态及错误信息的 Database。

连接指定主机与库；database 为空表示先不选库（用 `USE` 选）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `std::lang::String host` | 主机名或 IP 地址 | 无 |
| `user` | `std::lang::String user` | 数据库用户名 | 无 |
| `password` | `std::lang::String password` | 数据库连接密码 | 无 |
| `database` | `std::lang::String database` | 数据库名 | 无 |
| `port` | `int32 port` | 端口号 | 无 |

**返回值**

`Database`。连接 MySQL，返回带打开状态及错误信息的 Database。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::sql::Database::mysql(host, user, password, database, port);
```

**源码**：[source/std/sql.mgl:254](../../source/std/sql.mgl#L254)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-21"></a>

### Database::sqlite（22）

**声明**

```magel
static Database sqlite(std::lang::String path);
```

**行为**

打开文件 SQLite 或 :memory: 内存库。

打开 SQLite 文件；:memory: 创建不落盘的内存库。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `std::lang::String path` | 文件/数据库路径 | 无 |

**返回值**

`Database`。打开文件 SQLite 或 :memory: 内存库。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::sql::Database::sqlite(path);
```

**源码**：[source/std/sql.mgl:268](../../source/std/sql.mgl#L268)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-22"></a>

### Database::postgres（23）

**声明**

```magel
static Database postgres(std::lang::String options);
```

**行为**

使用 libpq 连接串/URI 打开 PostgreSQL。

通过 libpq 连接串或 URI 连接 PostgreSQL。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `options` | `std::lang::String options` | 后端连接选项/连接串 | 无 |

**返回值**

`Database`。使用 libpq 连接串/URI 打开 PostgreSQL。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::sql::Database::postgres(options);
```

**源码**：[source/std/sql.mgl:280](../../source/std/sql.mgl#L280)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-23"></a>

### Database::odbc（24）

**声明**

```magel
static Database odbc(std::lang::String options);
```

**行为**

使用 Unicode ODBC 连接选项打开数据源。

通过 ODBC Unicode 连接串连接通用数据源。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `options` | `std::lang::String options` | 后端连接选项/连接串 | 无 |

**返回值**

`Database`。使用 Unicode ODBC 连接选项打开数据源。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::sql::Database::odbc(options);
```

**源码**：[source/std/sql.mgl:292](../../source/std/sql.mgl#L292)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-24"></a>

### Database::sqlServer（25）

**声明**

```magel
static Database sqlServer(std::lang::String options);
```

**行为**

使用 ODBC 连接选项打开 SQL Server。

通过已安装的 Microsoft ODBC 驱动连接 SQL Server。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `options` | `std::lang::String options` | 后端连接选项/连接串 | 无 |

**返回值**

`Database`。使用 ODBC 连接选项打开 SQL Server。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::sql::Database::sqlServer(options);
```

**源码**：[source/std/sql.mgl:304](../../source/std/sql.mgl#L304)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-25"></a>

### Database::quote（26）

**声明**

```magel
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

```magel
obj.quote(text);
```

**源码**：[source/std/sql.mgl:311](../../source/std/sql.mgl#L311)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-26"></a>

### Database::backendName（27）

**声明**

```magel
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

```magel
obj.backendName();
```

**源码**：[source/std/sql.mgl:321](../../source/std/sql.mgl#L321)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-27"></a>

### Database::mysql（28）

**声明**

```magel
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

```magel
std::sql::Database::mysql(host, user, password, database);
```

**源码**：[source/std/sql.mgl:334](../../source/std/sql.mgl#L334)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-28"></a>

### Database::maria（29）

**声明**

```magel
static Database maria(std::lang::String host, std::lang::String user, std::lang::String password, std::lang::String database, int32 port = (int32)3306);
```

**行为**

连接 MariaDB，驱动标识为 mariadb，客户端 ABI 与 MySQL 兼容。

使用兼容的 MySQL/MariaDB 客户端连接 MariaDB，默认端口 3306。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `std::lang::String host` | 主机名或 IP 地址 | 无 |
| `user` | `std::lang::String user` | 数据库用户名 | 无 |
| `password` | `std::lang::String password` | 数据库连接密码 | 无 |
| `database` | `std::lang::String database` | 数据库名 | 无 |
| `port` | `int32 port` | 端口号 | `(int32)3306` |

**返回值**

`Database`。连接 MariaDB，驱动标识为 mariadb，客户端 ABI 与 MySQL 兼容。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::sql::Database::maria(host, user, password, database, port);
```

**源码**：[source/std/sql.mgl:339](../../source/std/sql.mgl#L339)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-29"></a>

### Database::local（30）

**声明**

```magel
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

```magel
std::sql::Database::local(database);
```

**源码**：[source/std/sql.mgl:346](../../source/std/sql.mgl#L346)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-30"></a>

### Database::isOpen（31）

**声明**

```magel
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

```magel
obj.isOpen();
```

**源码**：[source/std/sql.mgl:350](../../source/std/sql.mgl#L350)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-31"></a>

### Database::lastError（32）

**声明**

```magel
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

```magel
obj.lastError();
```

**源码**：[source/std/sql.mgl:351](../../source/std/sql.mgl#L351)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-32"></a>

### Database::affectedRows（33）

**声明**

```magel
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

```magel
obj.affectedRows();
```

**源码**：[source/std/sql.mgl:352](../../source/std/sql.mgl#L352)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-33"></a>

### Database::close（34）

**声明**

```magel
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

```magel
obj.close();
```

**源码**：[source/std/sql.mgl:355](../../source/std/sql.mgl#L355)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-34"></a>

### Database::exec（35）

**声明**

```magel
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

```magel
obj.exec(sql);
```

**源码**：[source/std/sql.mgl:368](../../source/std/sql.mgl#L368)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-35"></a>

### Database::execOk（36）

**声明**

```magel
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

```magel
obj.execOk(sql);
```

**源码**：[source/std/sql.mgl:432](../../source/std/sql.mgl#L432)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-36"></a>

### Database::scalar（37）

**声明**

```magel
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

```magel
obj.scalar(sql);
```

**源码**：[source/std/sql.mgl:438](../../source/std/sql.mgl#L438)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-37"></a>

### Database::count（38）

**声明**

```magel
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

```magel
obj.count(sql);
```

**源码**：[source/std/sql.mgl:445](../../source/std/sql.mgl#L445)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-38"></a>

### Database::hasTable（39）

**声明**

```magel
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

```magel
obj.hasTable(tableName);
```

**源码**：[source/std/sql.mgl:452](../../source/std/sql.mgl#L452)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-39"></a>

### quote（40）

**声明**

```magel
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

```magel
std::sql::quote(text, escapeBackslash);
```

**源码**：[source/std/sql.mgl:479](../../source/std/sql.mgl#L479)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-40"></a>

### quote（41）

**声明**

```magel
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

```magel
std::sql::quote(text);
```

**源码**：[source/std/sql.mgl:502](../../source/std/sql.mgl#L502)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-41"></a>

### intLiteral（42）

**声明**

```magel
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

```magel
std::sql::intLiteral(value);
```

**源码**：[source/std/sql.mgl:505](../../source/std/sql.mgl#L505)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-42"></a>

### realLiteral（43）

**声明**

```magel
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

```magel
std::sql::realLiteral(value);
```

**源码**：[source/std/sql.mgl:508](../../source/std/sql.mgl#L508)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。

<a id="api-43"></a>

### displayWidth（44）

**声明**

```magel
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

```magel
std::sql::displayWidth(text);
```

**源码**：[source/std/sql.mgl:511](../../source/std/sql.mgl#L511)。

**完整示例**：[doc/examples/library/sqlite.mgl](../../doc/examples/library/sqlite.mgl)。
