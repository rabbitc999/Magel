# 反射版学生管理系统

命令行版学生信息管理：模型用 `@Reflect` 与 SQL 模型注解声明，字段清单由反射读取，
JSON 编解码走反射序列化，数据库访问全部是参数化 SQLite。

已验证的命令：`schema`、`add`、`update`、`get`、`delete`、`list`（可按字段排序）、
`search`；以及学号、姓名、班级、年龄、成绩的业务校验，非法 JSON、未知排序字段、
错误排序方向和不存在的学号。

源码用法：`magel main.mgl 数据库路径 命令 [参数]`。支持 `add JSON`、`update JSON`、
`get 学号`、`delete 学号`、`list [字段 [asc|desc]]`、`search 姓名` 和 `schema`。
JSON 字段为 `id`、`name`、`age`、`className`、`score`。

入门讲解见书稿[第 23 章 学生信息管理系统](../../doc/book/23-学生信息管理系统.md)。
数据库与反射 SQL 的更多用法见[动态 SQL 说明](../../doc/36-reflection-serialization-sql.md)。
