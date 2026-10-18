# Magel Web 学生信息管理系统

一个可直接运行的 Magel 示例：Magel 使用 `std::net` 监听 HTTP，通过 `std::sql` 访问真实 SQLite 文件，浏览器页面调用同源 JSON 接口。服务端业务代码在 [main.mgl](main.mgl)，页面在 [index.html](index.html)，无需安装 Web 框架或前端依赖。

支持新增、查看、编辑、删除学生；学号唯一，姓名、邮箱、年龄在服务端校验。页面可按学号、姓名或邮箱过滤当前列表，SQL 列表最多返回 200 条。数据库在启动时自动建表，重启保留已保存数据。

## 从仓库根目录运行

先构建仓库的 `magel`。SQLite 已静态内置，无需准备 DLL 或设置库路径。

```powershell
New-Item -ItemType Directory -Force build | Out-Null
.\magel\magel.exe .\magel\example\web-students\main.mgl .\build\students.db 8080
```

然后访问 **http://127.0.0.1:8080/**。源码文件后的参数直接传入 Magel 的 main。

Linux/macOS 同样使用内置 SQLite，命令为：

```sh
mkdir -p build
./magel/magel magel/example/web-students/main.mgl build/students.db 8080
```

入口参数依次为：

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| 数据库文件 | `students.db` | SQLite 文件；父目录须已存在 |
| 端口 | `8080` | `0` 由操作系统分配，范围 0–65535 |
| 请求上限 | `0` | 0 持续运行，正整数在处理指定数量的连接后正常关闭 |
| 页面路径 | `magel/example/web-students/index.html` | 相对当前工作目录，也可传绝对路径 |
| 就绪文件 | 无 | 可选测试参数；监听成功后写入实际端口 |

示例只监听 `127.0.0.1`，浏览器请使用该地址。Host 与可选 Origin 按实际监听端口校验。开发时可用 Ctrl+C 停止；自动测试通过请求上限让主循环正常返回，以检查监听器、数据库和 Magel 资源清理。

## 导出独立程序

仍从仓库根目录运行，使用简短场景指令：

```powershell
.\magel\magel.exe aot magel/example/web-students/main.mgl out build/students-aot.exe opt 0
.\build\students-aot.exe build/students.db 8080
```

导出程序内置 SQLite，仍需要页面文件。移到其他目录部署时，把 `index.html` 一同复制，并用第四个参数指定其路径，例如 `students-aot.exe students.db 8080 0 index.html`。

## HTTP 接口

每条连接处理一个请求，响应采用 UTF-8、字节 Content-Length 与 `Connection: close`。

| 方法与路径 | 行为 | 成功状态 |
| --- | --- | --- |
| `GET /` | 管理页面 | 200 |
| `GET /api/health` | 执行 `SELECT 1` 检查数据库 | 200 |
| `GET /api/students` | 按 id 排序，最多 200 条 | 200 |
| `GET /api/students/{id}` | 查询一个学生 | 200 |
| `POST /api/students` | 新增学生 | 201 |
| `PUT /api/students/{id}` | 完整替换学生的业务字段 | 200 |
| `DELETE /api/students/{id}` | 删除学生 | 200 |

新增和修改必须发送 `Content-Type: application/json`，正文恰好包含四个字段：

```json
{"studentNo":"S001","name":"张三","email":"zhangsan@example.com","age":20}
```

返回对象包含数据库生成的整数 `id`。学号 1–32 字节，姓名 1–120 字节，邮箱 3–160 字节且包含 `@`，年龄必须为 JSON 整数且在 0–150 之间。文本先去除首尾空白，不接受内嵌零字节。不进行字符串到数字的隐式转换，也不允许客户端指定主键。

错误采用 `{"error":"说明"}`：格式或字段错误 400，来源不符 403，记录/接口不存在 404，方法不支持 405，学号重复 409，缺少正文长度 411，正文超限 413，媒体类型错误 415，Expect 不支持 417，请求头超限 431，Transfer-Encoding 不支持 501，数据库执行失败 500。数据库内部诊断不会原样放入 HTTP 响应。

SQL 表名和列名固定；文本值统一使用当前连接的 `db.quote(...)`，id 和年龄先校验为有界整数。页面使用 `textContent` 展示用户字段，避免把姓名等数据当作 HTML 执行。

## 自动测试

以下命令在仓库根目录执行。测试使用静态内置 SQLite，外部 SQLite 动态库路径被设为无效以验证独立运行。数据库、端口就绪文件和构建产物隔离在临时目录中。

```powershell
python tests/magel_web_students.py --magel magel/magel.exe
python tests/magel_web_students.py --magel magel/magel.exe --backends aot --make make
python tests/magel_web_students_browser.py --magel magel/magel.exe --browser "C:/Program Files/Google/Chrome/Application/chrome.exe"
```

第一条验证解释执行和 AOT。每个后端处理 62 个真实 HTTP 请求，包括增删改查、重复学号、JSON 类型与范围错误、中文/引号/反斜杠/控制字符、SQL 注入形态的字面量、HTTP 报文长度、重复 Content-Length、错误 Host/Origin、分段正文与截断正文。随后用 Python sqlite3 独立查询落盘数据并执行 integrity_check，再启动服务器确认重启后的记录。正常退出要求标准错误为空。

第二条单独验证 VM 的 make 构建路径。第三条使用本机已安装的 Chrome 或 Edge，无第三方 Python 包，通过真实浏览器操作表单、查看重复学号提示、编辑、搜索和删除，并检查 HTML 注入防护与实际 SQL 行数；截图写入 `build/magel-web-students-browser.png`。

## 示例边界

这是本地、单连接顺序处理的文本 HTTP 示例。请求头上限 8192 字节，正文上限 16384 字节；不支持 TLS、分块传输、长连接、文件上传、登录与权限管理。底层 receive 是阻塞调用，未完成的客户端请求会阻塞后续处理；正式服务需要补充读取超时、并发、认证和部署方案。

本例的建表语句、自动主键和 `last_insert_rowid()` 使用 SQLite 方言。虽然 `std::sql` 也提供 MySQL/PostgreSQL/SQL Server，切换后端时应同时改写这些 SQL，不能只更换连接函数就声称已经兼容。

## 本次验收

验证（2026-10-10，Windows / GCC 11.2、真实 SQLite 3 动态库、本机 Chrome）：解释执行、C++ VM、AOT 各 62 个 HTTP 请求通过，SQL 落盘与重启核验通过；VM make 路径另 62 个请求通过；浏览器新增、重复提示、编辑、搜索、删除与 HTML 注入防护通过；正常服务器退出资源诊断为空。既有场景指令专项 3 组、CLI 参数单元测试及文档同步检查通过。

4.0.0 起仅提供 AOT 和 .svmb 导出；内嵌 VM 导出模式已删除。以上 make 命令验证 AOT 构建路径。3.2.3 验收记录是当时版本的历史结果。
