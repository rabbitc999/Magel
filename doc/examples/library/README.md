# 库函数参考配套程序

完整 API 见 [库函数参考](../../library/README.md)。各模块页还链接已有的 `example/samples` 完整程序及预期输出。

| 程序 | 运行命令 | 验证内容 |
| --- | --- | --- |
| [io.chl](io.chl) | `chali run doc/examples/library/io.chl -- build/library-example.txt` | 文件创建、写入、刷新、定位和读取；先创建 build，给定路径会覆盖 |
| [sqlite.chl](sqlite.chl) | `chali run doc/examples/library/sqlite.chl` | 内置 SQLite 内存库、Unicode 字符串、查询与错误分支 |
| [watchdog.chl](watchdog.chl) | `chali run doc/examples/library/watchdog.chl` | 未启用与错误水位分支、启用及状态查询 |
| [reflect.chl](reflect.chl) | `chali run doc/examples/library/reflect.chl reflect` | 类型与结构字段查询；需前端和 VM 均编入反射支持 |

`python tests/chali_library_reference.py` 核对声明、链接并执行 23 个完整程序。可用反射工具链通过 `--reflection-compiler PATH` 指定，随后也执行反射示例。
