# 反射版学生管理系统草稿

本目录为未完成验收的交接草稿。模型使用 `@Reflect`、SQL 模型注解、反射 JSON 编解码和 SQLite 参数绑定。已通过共享 VM 验证空列表、新增、按学号查询和持久化列表；更新、删除、搜索、排序、异常输入与完整资源回收仍待验收。

源码用法：`magel main.mgl 数据库路径 命令 [参数]`。支持 `add JSON`、`update JSON`、`get 学号`、`delete 学号`、`list [字段 [asc|desc]]`、`search 姓名` 和 `schema`。JSON 字段为 `id`、`name`、`age`、`className`、`score`。

后续任务及实际验证记录见根目录 [TASK.md](../../TASK.md)。不要将该草稿视为已完成的示例项目。
