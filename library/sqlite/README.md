# 内置 SQLite

官方 SQLite 3.54.0 amalgamation 原样副本（公共领域）。
来源：https://www.sqlite.org/2026/sqlite-amalgamation-3540000.zip
归档 SHA3-256：`7b670a62fdfbd672b75fef004cb703c8a3e87d3a5cc7d675b4a08337004a2d93`。

以 C 编译，启用线程安全、禁用动态扩展加载。桌面工具链静态链接；AOT 按 SQL 模块引用编译并静态链接。源码随工具链内嵌，导出不依赖仓库或网络。
