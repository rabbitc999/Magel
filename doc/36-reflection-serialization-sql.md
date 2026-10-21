# 反射序列化与动态 SQL

`std::json`、`std::toml` 和 `std::serialization` 使用具体反射类型生成编解码桥接。
用户结构体和枚举使用 `@Reflect` 暴露。字段访问沿用普通语言的复制、释放和借用规则，
没有地址转整数、按偏移强制转换或绕过拥有关系的操作。

编译器裁剪配置未编入反射支持时，依赖反射的高级入口直接 `panic`。
模型类型未标记 `@Reflect` 时返回错误；它与编译器反射能力缺失是两种情况。

## 编解码

```mgl
import std::json;
import std::toml;
import std::serialization;

@Reflect
struct Person {
    String name;
    int64 age;
};
```

各模块提供 `serialize<T>(T const& value)` 和 `deserialize<T>(...)`。
JSON/TOML 使用文本，序列化模块使用 `List<uint8>`。调用编码函数时传入 `&value`。
所有接口返回 `Result<结果类型,String>`，格式错误、缺失字段、数值越界和不支持的类型
通过错误分支报告。解码创建独立存储，修改返回值不会修改输入或其他副本。

整数不经过浮点数。超过有符号 64 位范围的 `uint64` 使用十进制字符串；
`BigInteger` 使用精确十进制字符串，`BigDecimal` 使用包含十进制值和精度的对象。
小数精度为零时沿用默认值，显式精度为 1–100000 位；十进制指数绝对值沿用大数后端的一百万上限。
浮点数拒绝 NaN 和无穷大。枚举使用 `$variant` 与 `$values` 保存变体和载荷。
列表、集合、字符串键映射与嵌套反射结构递归处理，深度上限为 128。
拥有型资源、函数、裸指针及未支持的复合类型返回错误。

TOML 要求对象根节点；它使用相同的类型校验和精确数字表示。反射编码创建新文档；
保留已有 TOML 文档注释的操作仍是 `parse(text).unwrap().dump()`。
字节序列化格式包含 `MGS` 魔数、版本、64 位类型标识、64 位长度与 UTF-8 JSON 载荷；
整数头使用小端序。解码验证完整长度、版本和目标类型。格式不是原生内存快照。

## 反射字段

`std::reflect::getField<T,F>(&object, name)` 返回字段的独立值；
`setField<T,F>(&object, name, value)` 写入字段。`F` 必须与声明类型精确相同。
缺失字段、类型不匹配、未暴露类型和不支持的拥有资源通过 `ReflectError` 区分。

## 动态 SQL

`std::sql` 将反射元数据转换为 ipage 的 `ModelMeta` 和 `Query`。
源码在 `library/ipage/source/ipage` 直接维护，导出工程使用内嵌资产。

```mgl
import std::sql;

@Reflect
@std::sql::ModelClass("people")
struct Person {
    @std::sql::ModelField(column: "person_id", id: true)
    int64 id;
    String name;
};
```

`std::sql::query<Person>()` 返回查询对象。`ModelField` 的 `column` 指定列名，`id`
指定主键，`kind` 指定 `auto`、`int`、`real`、`text` 或 `bool` 存储类型。
默认从反射字段类型推导；模型必须有主键。

查询支持 `std::sql::filter<V>`、`std::sql::filterMany<V>`、`orderBy`、`select` 与 `page`。
`build()` 返回 `SqlStatement`，公开 `sql` 和 `parameters`；后者是 JSON 标量数组。
`build("create")` 生成建表，`build("count")` 生成计数，`std::sql::insert<T>` 和 `std::sql::update<T>`
从反射值生成写入，`build("delete")` 生成删除。更新和删除必须提供过滤条件。

过滤操作符使用 ipage 顺序：0 相等、1 不等、2 大于、3 小于、4 大于等于、
5 小于等于、6 包含、7 前缀、8 后缀、9 空值、10 非空、11 IN、12 NOT IN。
多参数和空值条件使用 `filterMany`。条件与排序字段必须属于模型白名单，标识符统一
由方言引用；参数值仅进入占位符与参数列表。

`Database.exec(statement)` 直接执行生成的参数化语句。当前动态生成和直接绑定执行
支持 SQLite，使用驱动绑定整数、浮点、文本、布尔和 NULL，严格检查参数数量且只允许
一条语句。其他已有数据库后端继续支持原来的文本执行接口；它们的参数化执行明确报错。
不会将参数插入 SQL 文本作为后备执行方式。

完整可运行样例见 [反射字段](../example/samples/reflect_fields.mgl)、
[反射序列化](../example/samples/reflect_serialization.mgl) 和
[动态 SQL](../example/samples/sql_ipage.mgl)。
