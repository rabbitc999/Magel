# ipage-student —— 学生成绩信息管理系统（Magel 版）

把 [`ipage-student`](https://gitee.com/rabbitc999/ipage-student)（Java + Spring MVC + MyBatis
的“Java 综合项目实训”工程）的**登录与增删查改**用 Magel 重写成一个控制台程序：
**不依赖 ipage 框架**，数据直接读写它自己的 `student` 库（表结构与初始数据来自该工程的
`student.sql`）。

```
main.mgl  ≈1540 行，一个文件；界面、校验、SQL 拼接、权限判定、夹具导入都在里面
```

## 数据模型

程序直接使用 ipage-student 现有的五张表，不改表结构：

| 表 | 主键 | 关键列 | 外键行为 |
|---|---|---|---|
| `user` | `user_id` | `user_name`(≤18) / `user_password` / `user_permission`(enum) | — |
| `sclass` | `sclass_id` | `sclass_name` / `sclass_administrator` / `sclass_grade`(enum ONE/TWO/THREE) | — |
| `course` | `course_id` | `course_name` / `course_state`(enum START/COMPLETION) | — |
| `student` | `student_id` | `student_name` / `student_sex`(enum MALE/FEMALE) / `sclass_id` | `sclass_id` → `sclass` ON DELETE **SET NULL** |
| `selection` | `selection_id` | `student_id` / `course_id` / `test_date` / `grade_number` | → `student`、`course` 均 ON DELETE **CASCADE** |

删除学生 / 课程会连坐删掉对应的 `selection` 行，删除班级只把学生置为“未分班”——程序在删除前
会先把影响面打印出来，避免误操作。

## 权限模型

等级值与 Java 端 `UserPermission` 的 `value` 逐一对齐（`STUDENT=0`、`TEACHER=2`、
`ADMINISTRATOR=3`、`SUPER_ADMINISTRATOR=4`），判定一律是 `用户等级 >= 门槛等级`，
与 `UserUtils.canUseOperator` 同口径。门限来自各模型 `@ModelClass` 的
`look` / `edit` / `delete`：

| 模块 | 查看 | 编辑 | 删除 |
|---|---|---|---|
| 学生 `student` | 学生 | 教师 | 管理员 |
| 班级 `sclass` | 教师 | 教师 | 管理员 |
| 课程 `course` | 教师 | 教师 | 管理员 |
| 成绩 `selection` | 学生 | 教师 | 管理员 |
| 用户 `user` | 超级管理员 | 超级管理员 | 超级管理员 |

菜单按上述门限**先过滤再显示**，选中后还会再判一次（`App.deny`），因此低权限账号既看不到
越权入口，也拿不到越权动作。四种账号登录后的主菜单项数：学生 3、教师 5、管理员 5、
超级管理员 6。

## 功能

* **登录**：核对 `user_name` + `user_password`，最多 3 次；成功后按权限决定菜单。
* **学生**：添加 / 删除 / 修改 / 按姓名查 / 按班级查 / 显示全部（带课程数与平均分）。
* **班级**：添加 / 删除 / 修改 / 按名称查 / 显示全部（带学生数与班级平均分）。
* **课程**：添加 / 删除 / 修改 / 按名称查 / 显示全部（带选课人数与平均分）。
* **成绩**：录入 / 修改 / 删除 / 按学生查 / 按课程查 / 显示全部（前 50 条，含等级）/
  按课程统计（人数、平均、最高、最低、及格率）/ 按班级统计。
* **修改我的密码**：任何登录用户可用，先验旧口令。

校验规则照搬 Java 端注解：用户名非空且 ≤18 字符（`@ValidatorLength`）、口令 ≥4 位字母数字
（`@ValidatorPattern("^[A-Za-z\d]{4,}")`）、成绩 0~100（`@ValidatorRange(max = 100)`）、
枚举列只接受合法字面量；`admin` 不允许删除、`admin` 只能是超级管理员、其他成员不能是
超级管理员（`User.checker`）。

## 运行

先让本机 MySQL 跑着，然后**在 magel 仓库根目录**执行：

```sh
# 库已存在（自己导入过 ipage-student/student.sql）
./magel.exe sql-magel/example/ipage-student/main.mgl

# 库还不存在：用 --init 就地建库并灌入初始数据
./magel.exe sql-magel/example/ipage-student/main.mgl -- \
    --init ../ipage-student/student.sql

# 脚本化回答（自检 / 演示用，UTF-8 文件按空白取词）
./magel.exe sql-magel/example/ipage-student/main.mgl -- \
    --answers sql-magel/tests/input_ipage.txt
```

可用开关：

| 开关 | 含义 |
|---|---|
| `--answers FILE` | 从 UTF-8 文件按空白读取界面回答；答案用尽时程序正常收尾退出 |
| `--db NAME` | 库名，默认 `student` |
| `--host` / `--user` / `--password` / `--port` | 连接参数，默认 `127.0.0.1:3306` / `root` / `123456` |
| `--init FILE` | 库里缺表时执行该 `.sql` 建库灌数据 |
| `--reinit` | 无论表在不在都重新执行 `--init`（自检用，保证夹具确定） |

## 自检

```sh
sh magel/example/sql-magel/tests/check-ipage.sh   # 输出应为 ok ipage / 检查通过
```

自检**不碰真实库**：它在独立测试库 `student_check` 里用 `tests/fixture_ipage.sql`
（五张表 + 5 学生 / 3 班级 / 3 课程 / 6 成绩 / 4 用户的确定性小夹具）配合 `--reinit`
重建一遍，因此自增主键每次从 1 开始，`input_ipage.txt` 里写死的 ID 才有确定含义。
脚本跑完后测试库会回到初始状态（新增的数据都被自己删掉、口令改回原值）。

## 与 Java 版的有意差异

* **形态**：Java 版是 Web 应用（HTML 视图 + Cookie 会话），这里是控制台程序；菜单、
  校验规则、权限门限、`admin` 约束保持一致，视图层不搬运。
* **权限数值**：Java 的 `UserPermission` 还有 `NORMAL` / `IMPOSSIBLE` / `LEAST` 三个不在
  数据库 enum 里的取值，这里只保留库里真有的四种。
* **认证**：Java 版把口令明文存库并按明文比对（`student.sql` 里就是明文），这里沿用同样的
  比对方式，未加哈希——**样式对齐优先于安全增强**，真要上线必须改成哈希 + 参数化语句。
* **同一学生同一课程重复录入**：原表没有唯一约束，Java 版靠 `AGGREGATION` 展示、不拦重复；
  这里在录入时先查一次并提示改用「修改成绩」。
* **学生看不到“自己的”成绩**：`user` 表没有指向 `student` 的外键，Java 版也做不到按登录人
  过滤；这里没有自行发明关联规则。
