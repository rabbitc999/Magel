# sql-magel —— Magel 连接外部 SQL 数据库的示例工程

本目录是 Magel 工程内的示例工程（`magel/example/sql-magel/`），演示**用 Magel 语言连外部 SQL
数据库**：SQL 的解析、执行、事务、索引全部交给外部数据库服务器，Magel 只负责把语句发过去、
把结果搬回来。

库本体不是独立 SDK，而是 **Magel 编译器自带的标准库模块** `std::sql`
（[`std/sql.mgl`](../std/sql.mgl) + 宿主胶水 [`glue/std_sql.hpp`](../glue/std_sql.hpp)），
任何 Magel 程序 `import std::sql;` 即可使用；本目录放的是**示例项目**：一个按实训需求实现的
学生信息管理系统，以及它用到的 `.sql` 脚本。

`std::sql` 现支持 MySQL、SQLite、PostgreSQL 与 Microsoft SQL Server / ODBC；学生管理示例
继续使用 MySQL。无需数据库服务的 SQLite 样例、各后端依赖与验证范围见
SQL 模块参考见 [`doc/library/std-sql.md`](../doc/library/std-sql.md)，完整接口见[接口文档](doc/01-sql-library.md)。

| 路径 | 内容 |
|---|---|
| `sql/schema.sql` | 学生 / 班级 / 成绩三张表的建表 SQL（原有表格的 SQL 代码，保留在这里） |
| `sql/seed.sql` | 初始演示数据（`INSERT IGNORE`，可反复执行） |
| `sql/queries.sql` | 各菜单真正发给 MySQL 的查询，便于对照与手工验证 |
| `example/student/main.mgl` | 学生信息管理系统（菜单、增删改查、统计） |
| `example/ipage-student/main.mgl` | 同一类需求、**换成外部工程的数据**：读写 ipage-student 的 `student` 库，带登录与四角色权限门限 |
| `example/sql_demo.mgl` | `std::sql` 最小用法演示 |
| `tests/check.sh` | 冒烟自检：跑一遍脚本化输入并对 stdout 逐行比对 |
| `tests/check-ipage.sh` | ipage-student 示例的冒烟自检：在独立测试库 `student_check` 上用夹具重建，不碰真实库 |
| `doc/01-sql-library.md` | 库的分层、接口清单与实现要点 |

## 前置条件

1. 本机 MySQL 在跑，默认连接参数 `127.0.0.1:3306`、用户 `root`、口令 `123456`
   （库 `student_db` 不存在会自动创建）。
2. 已经构建过 Magel：标准库模块在构建期内嵌进编译器，所以要先用
   [`build-all.sh`](../../build-all.sh) / `sh build.sh` 重新构建，`std::sql` 才在编译器里。

## 运行

在**仓库根目录**执行（示例按相对路径找 `magel/example/sql-magel/sql/*.sql`）：

```sh
# 浏览 + 增删改查 + 统计（脚本化回答，UTF-8）
magel/magel.exe magel/example/sql-magel/example/student/main.mgl -- --answers magel/example/sql-magel/tests/input_crud.txt

# 换一份外部工程的数据：读写 ipage-student 的 student 库（库不存在时先 --init 建库）
magel/magel.exe magel/example/sql-magel/example/ipage-student/main.mgl -- \
    --init ../ipage-student/student.sql

# 库用法最小演示
magel/magel.exe magel/example/sql-magel/example/sql_demo.mgl
```

自检：

```sh
sh magel/example/sql-magel/tests/check.sh        # 输出应为 ok crud / magel/example/sql-magel 检查通过
sh magel/example/sql-magel/tests/check-ipage.sh  # 输出应为 ok ipage / ipage-student 检查通过
```

## 关于输入

示例支持 `--answers 文件`，从 UTF-8 文件按空白读取脚本化回答，自检使用这条路径，避免交互输入影响回归结果。当前工具已经使用 Unicode 命令行参数，文件回答不再是规避 Windows ANSI 参数乱码的必要条件。直接文件运行时，文件后的参数属于应用程序。

## 已知边界

* 学生管理示例继续依赖本机 MySQL 服务及可加载的客户端库；客户端路径可通过 `MYSQL_LIBRARY` 指定。连接失败会显示原因并停止业务流程。
* 示例使用同步单连接，拼接值字面量时调用 `std::sql::quote()`；跨后端代码应选择 `db.quote()`。当前没有 SQL 参数占位符绑定接口。
* `DECIMAL` 的原始文本可通过 `textAt()` 读取，需要数值计算时使用 `realAt()`。整数结果及影响行数使用 `int64`，句柄仍为内部管理的整数。
* 字符串按值复制与临时实参清理遵循当前语言规则，可以用形参初始化局部字符串，也可以把返回字符串的调用作为按值实参；早期为规避所有权缺陷而写的中间变量不再是语言限制。
* 无需 MySQL 的完整 Web 学生系统见 [web-students](../example/web-students/README.md)，使用真实 SQLite 文件。两套学生系统的数据库和界面不同，验证结果需分别记录。
