# ipage-student-server —— 学生成绩信息管理系统（前后端分离版）

把同目录的[控制台版](../ipage-student/)（`magel.exe sql-magel/example/ipage-student/main.mgl`）
升格成**前后端分离**的应用：Magel 只管 HTTP + JSON 接口，页面是另一套纯静态资源，
两者**不同源**、各自独立部署，浏览器靠 CORS 访问接口。

数据仍然直接读写 ipage-student 自己的 `student` 库（表结构与初始数据来自该工程的
`student.sql`），业务规则、权限门限、校验分支与控制台版保持一致。

```
main.mgl        2174 行，后端：HTTP 收发、路由、权限判定、五模块 SQL 与 JSON 输出
web/           1121 行，前端：原生 HTML/CSS/JS，无构建步骤、无第三方依赖
```

## 目录结构

```
ipage-student-server/
├── main.mgl          后端：JSON API 服务（不返回任何页面）
├── web/              前端：可直接丢给任意静态服务器
│   ├── index.html    登录卡片 + 应用外壳（顶栏 / 侧栏 / 数据面板）
│   ├── app.js        取数、渲染、表单；模块与字段描述表
│   ├── style.css     单页应用样式
│   └── config.js     前端连哪个后端 —— 分离的唯一耦合点
├── README.md
└── ../../tests/      自检脚本在仓库的 tests/ 下（见「自检」）
```

## 前后端是怎么分开的

| 关注点 | 做法 |
|---|---|
| 页面归属 | 后端只认 `/api/*`；`/`、`/index.html`、`/web/app.js` 一律 **404**，静态资源由另一个进程托管 |
| 跨源 | 后端每个响应（含 `OPTIONS` 预检）都带 CORS 头：`Allow-Origin`（默认 `*`，可用 `--allow-origin` 收窄）、`Allow-Methods`、`Allow-Headers: Content-Type, Authorization`、`Max-Age: 600` |
| 身份 | 登录返回 `Authorization: Bearer <64 位十六进制令牌>`；会话表放在后端内存里，**服务重启即全部失效** |
| 权限 | 权限表是**唯一来源**，后端按 `用户等级 >= 门槛等级` 逐动作判定；前端菜单与按钮的可见性由 `/api/session` 返回的 `modules[].actions[].needLevel` 驱动，前端**不做**权限决定 |
| 注入 | 前端一律用 `textContent` 写用户数据，不拼 HTML |

前端地址写在 `web/config.js` 的 `window.MAGEL_API_BASE`（默认 `http://127.0.0.1:8080`），
也可以在打开页面时用查询串覆盖：`http://127.0.0.1:5500/?api=http://127.0.0.1:8080`。

## 数据模型

使用 ipage-student 现有的五张表，不改表结构：

| 表 | 主键 | 关键列 | 外键行为 |
|---|---|---|---|
| `user` | `user_id` | `user_name`(≤18) / `user_password`(≤18) / `user_permission`(enum) | — |
| `sclass` | `sclass_id` | `sclass_name`(≤36) / `sclass_administrator`(≤24) / `sclass_grade`(enum ONE/TWO/THREE) | — |
| `course` | `course_id` | `course_name`(≤36) / `course_state`(enum START/COMPLETION) | — |
| `student` | `student_id` | `student_name`(≤24) / `student_sex`(enum MALE/FEMALE) / `sclass_id` | `sclass_id` → `sclass` ON DELETE **SET NULL** |
| `selection` | `selection_id` | `student_id` / `course_id` / `test_date` / `grade_number` | → `student`、`course` 均 ON DELETE **CASCADE** |

删除学生 / 课程会连坐删掉对应的 `selection` 行，删除班级只把学生置为「未分班」；
接口把影响面放进返回体（`cascade` / `detached` 计数），前端在确认框里告知用户。

列宽是**字符数**而不是字节数：`student_name` 的 24 指的是 24 个字符，24 个汉字
（72 字节）能进库。接口在写库前按同一张表拦一道超长入参，返回 400 加中文说明，
而不是漏出 MySQL 的 `Data too long` 500。

## 权限模型

等级值与 Java 端 `UserPermission` 的 `value` 对齐（`STUDENT=0`、`TEACHER=2`、
`ADMINISTRATOR=3`、`SUPER_ADMINISTRATOR=4`），判定一律是 `用户等级 >= 门槛等级`。
门限来自各模型 `@ModelClass` 的 `look` / `edit` / `delete`：

| 模块 | 查看 | 编辑 | 删除 |
|---|---|---|---|
| 学生 `student` | 学生 | 教师 | 管理员 |
| 班级 `sclass` | 教师 | 教师 | 管理员 |
| 课程 `course` | 教师 | 教师 | 管理员 |
| 成绩 `selection` | 学生 | 教师 | 管理员 |
| 用户 `user` | 超级管理员 | 超级管理员 | 超级管理员 |

四种账号登录后可见的模块数：学生 3、教师 5、管理员 5、超级管理员 6。

## 功能

* **登录 / 注销**：核对 `user_name` + `user_password`；成功后按权限下发模块树。
  控制台版的「最多 3 次」是界面循环，无状态 HTTP 上改由前端计数，服务端只如实回答
  成功或失败（口令错返回 401，前端自己减次数）。
* **学生**：列表（带课程数与平均分）/ 按姓名查 / 按班级查 / 新增 / 修改 / 删除。
* **班级**：列表（带学生数与班级平均分）/ 按名称查 / 新增 / 修改 / 删除。
* **课程**：列表（带选课人数与平均分）/ 按名称查 / 新增 / 修改 / 删除。
* **成绩**：列表（前 200 条，含等级）/ 按学生查 / 按课程查 / 录入 / 改分 / 删除 /
  按课程统计（人数、平均、最高、最低、及格率）/ 按班级统计。
* **用户**：列表（**不返回口令**）/ 新增 / 删除 / 改权限 / 重置口令。
* **修改我的密码**：任何登录用户可用，先验旧口令。

## HTTP 接口

每条连接处理一个请求，响应 UTF-8、字节 `Content-Length`、`Connection: close`、
`Cache-Control: no-store`。除健康检查与登录外，都要 `Authorization: Bearer <令牌>`。

### 公开

| 方法与路径 | 行为 | 成功状态 |
|---|---|---|
| `GET /api/health` | `SELECT 1` 探活，返回 `{"status":"ok","database":"mysql"}` | 200 |
| `POST /api/login` | 登录，返回 `token` / `user` / `modules` | 200 |

### 会话

| 方法与路径 | 行为 | 门槛 |
|---|---|---|
| `POST /api/logout` | 丢弃当前令牌 | 已登录 |
| `GET /api/session` | 当前用户 + 可见模块树 | 已登录 |
| `POST /api/password` | 改自己的口令（要旧口令） | 已登录 |

### 业务（`module.action` 为权限表里的动作键）

| 方法与路径 | 行为 | 动作门槛 |
|---|---|---|
| `GET /api/students` | 列表，最多 200 条 | `student.list` 学生 |
| `POST /api/students` | 新增 | `student.add` 教师 |
| `POST /api/students/query` | 按 `mode`（`name`/`class`）+ `keyword` 查 | `student.query` 学生 |
| `PUT /api/students/{id}` | 全量替换（不给班级就是不分班） | `student.update` 教师 |
| `DELETE /api/students/{id}` | 删除（连带成绩） | `student.delete` 管理员 |
| `GET` / `POST` / `POST query` / `PUT {id}` / `DELETE {id}` `/api/sclasses` | 班级同构 | `sclass.*` 教师 / 教师 / 教师 / 教师 / 管理员 |
| `GET` / `POST` / `POST query` / `PUT {id}` / `DELETE {id}` `/api/courses` | 课程同构 | `course.*` 教师 / 教师 / 教师 / 教师 / 管理员 |
| `GET /api/selections` | 成绩列表 | `selection.list` 学生 |
| `POST /api/selections` | 录入（同一学生同一课程重复录入给 409） | `selection.add` 教师 |
| `POST /api/selections/query` | 按 `student` / `course` 查 | `selection.query` 学生 |
| `PUT /api/selections/{id}` | 只改分数 | `selection.update` 教师 |
| `DELETE /api/selections/{id}` | 删除 | `selection.delete` 管理员 |
| `GET /api/selections/stats/course?courseId=N` | 按课程统计（`courseId=0` 为全部） | `selection.statCourse` 学生 |
| `GET /api/selections/stats/class` | 按班级统计 | `selection.statClass` 学生 |
| `GET /api/users` | 用户列表 | `user.list` 超管 |
| `POST /api/users` | 新增用户 | `user.add` 超管 |
| `PUT /api/users/{id}/permission` | 改权限 | `user.permission` 超管 |
| `PUT /api/users/{id}/password` | 重置口令 | `user.password` 超管 |
| `DELETE /api/users/{id}` | 删除（`admin` 与当前登录人不许删） | `user.delete` 超管 |
| `GET /api/options/sclasses` | 班级下拉（新增/编辑学生用） | `sclass.list` 教师 |
| `GET /api/options/courses` | 课程下拉（录入成绩用） | `course.list` 教师 |

### 报文约定

列表统一包一层信封：

```json
{"items":[...],"count":3,"total":3,"limit":200}
```

成功动作用 `{"message":"..."}`（新增类另带 `id`，删除类另带计数）说明结果；
失败一律 `{"error":"中文说明"}`。

| 状态 | 含义 |
|---|---|
| 400 | 字段格式 / 取值 / 列宽不符，主键不是正整数 |
| 401 | 未登录、令牌失效，或用户名口令不匹配 |
| 403 | 已登录但等级不够，提示「需要 XX 及以上权限」 |
| 404 | 记录或接口不存在 |
| 405 | 方法不支持 |
| 409 | 用户名被占用；同一学生同一课程已有成绩 |
| 411 / 413 / 415 / 417 / 431 / 501 | 缺 `Content-Length` / 正文超限 / 非 `application/json` / 不支持 `Expect` / 请求头超 8192 字节 / 不支持 `Transfer-Encoding` |
| 500 / 503 | 服务端执行失败（如权限表缺动作，失败关闭）/ 数据库不可用 |

`POST` 与 `PUT` 必须带 `Content-Type: application/json`，正文上限 64 KiB。
请求头的 `Host` 必须是 `127.0.0.1` 或 `localhost` 且端口等于实际监听端口
（比 web-students 示例放宽一处：前端另起一个源，两种写法都要能用）。

## 运行

先让本机 MySQL 跑着。**在 magel 仓库根目录**开两个终端：

```sh
# ① 后端：库已存在（自己导入过 ipage-student/student.sql）
./magel.exe run sql-magel/example/ipage-student-server/main.mgl --out build/ipage-server.exe

# 库还不存在：用 --init 就地建库并灌入初始数据
./magel.exe run sql-magel/example/ipage-student-server/main.mgl --out build/ipage-server.exe -- \
    --init ../ipage-student/student.sql

# ② 前端：起一个静态服务器托管 web/（哪个都行，这里用 Python 自带）
python -m http.server 5500 --directory sql-magel/example/ipage-student-server/web
```

然后访问 **http://127.0.0.1:5500/**。若后端不在默认的 8080，用
`http://127.0.0.1:5500/?api=http://127.0.0.1:9000` 覆盖。

后端不提供页面，所以直接访问 `http://127.0.0.1:8080/` 只会拿到 404 —— 这是有意的。

### 后端开关

| 开关 | 默认值 | 含义 |
|---|---|---|
| `--listen-host` | `127.0.0.1` | HTTP 监听地址 |
| `--listen-port` | `8080` | HTTP 监听端口；`0` 由系统分配 |
| `--mysql-host` / `--mysql-port` | `127.0.0.1` / `3306` | MySQL 地址 |
| `--mysql-user` / `--mysql-password` | `root` / `123456` | MySQL 账号 |
| `--db` | `student` | 库名 |
| `--init FILE` | 无 | 库里缺表时执行该 `.sql` 建库灌数据 |
| `--reinit` | 关 | 无论表在不在都重新执行 `--init`（自检用，保证夹具确定） |
| `--allow-origin` | `*` | CORS 响应头里的允许源 |
| `--ready FILE` | 无 | 监听成功后把实际端口写进该文件（自检用） |
| `--max-requests N` | `0` | 处理 N 个请求后正常退出，`0` 不限（自检用） |

## 自检

两套脚本，用法与 ipage 自检一致（在仓库根目录执行）：

```sh
# 接口本身：run 与 aot 两种后端各跑一遍，每种 207 个真实 HTTP 请求
python tests/magel_ipage_student_server.py

# 前端：真浏览器跨源跑一遍完整用户流程
python tests/magel_ipage_student_server_browser.py \
    --browser "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
```

两个脚本都**不碰真实库**：在独立测试库 `ipage_server_check` 里用
`tests/fixture_ipage.sql`（五张表 + 5 学生 / 3 班级 / 3 课程 / 6 成绩 / 4 用户）
配合 `--reinit` 重建，因此自增主键每次从 1 开始。接口脚本按模块分 5 组、
**每组独立重建一次夹具**，避免用例之间互相踩数据。

* 接口脚本覆盖：登录 / 注销 / 坏令牌；四种账号的模块可见性与动作门槛；五模块增删查改、
  聚合统计、校验分支与 `admin` 约束；CORS 预检与实际响应头；**后端不返回页面**；
  HTTP 报文边界（错 `Host`、重复 `Content-Length`、分段正文、头部超长、截断正文）。
* 浏览器脚本用本机已装的 Chrome / Edge 加标准库 CDP 客户端（无第三方 Python 包），
  页面由 `python -m http.server` 之外的一个线程内静态服务器托管，**真的跨源**：
  校验按权限收放侧栏与按钮、新增 / 编辑 / 查询 / 删除、后端聚合统计页、
  以及用 `<img src=x onerror=...>` 当姓名验证不做 HTML 注入。截图写入
  `build/magel-ipage-server-ui.png`。

## 与控制台版的有意差异

* **形态**：控制台版是一问一答的菜单循环，这里是无状态 HTTP 接口 + 浏览器单页。
  菜单、校验规则、权限门限、SQL 语义、`admin` 约束逐条对齐，交互层不搬运。
* **「最多 3 次」的归属**：控制台版在进程里循环；HTTP 无状态，服务端如实答对错，
  由前端计数（`web/app.js` 的 `MAX_ATTEMPTS`）。绕过前端直连接口就没有次数限制——
  这是分离架构的固有取舍，真要硬限得在服务端或网关做限流。
* **会话**：控制台版没有会话概念；这里用 `Bearer` 令牌 + 内存会话表，**重启即失效**，
  也没有过期时间与刷新机制。
* **认证**：沿用明文口令比对（`student.sql` 里就是明文），**样式对齐优先于安全增强**；
  真要上线必须改成哈希 + 参数化语句，并加上 TLS 与会话过期。
* **超长入参**：控制台版只拦空值，超长会撞出数据库报错；这里在写库前按列宽拦成 400。
  控制台版的同一处缺口没有一并修改，保持它现有的行为。
* **前端权限只是提示**：菜单和按钮的可见性来自后端返回的门槛值，但**真正的判定在后端**；
  前端查表只负责不显示不该有的入口。

## 本次验收

2026-10-12，Windows / GCC 11.2 / 本机 MySQL 8.0 / 本机 Edge 无头模式：

* 接口：`run` 与 `aot` 两种后端 × 5 组场景全部通过（82 / 31 / 28 / 25 / 41 个真实
  HTTP 请求），正常退出且标准错误为空、资源诊断为空。
* 前端：跨源登录、四种权限的侧栏收放、学生看不到新增/删除、新增 / 编辑 / 查询 / 删除、
  成绩聚合统计页、HTML 注入防护全部通过。
* 静态检查：`magel check` 无诊断；`node --check web/app.js` 通过。
