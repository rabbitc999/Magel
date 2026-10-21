# 库函数 API 参考手册

采用 C/C++ 参考手册常见的组织方式：模块 → 类型 → 函数/重载，逐项列出声明、参数、返回值、行为、边界及调用形式。语法仍为 Magel，不是 C++。

覆盖工具链自带的 `std::lang`、`std`、`std::*` 与可选 `std::reflect`；编译器注入的算法和核心容器另有来源标记。`__` 前缀胶水是实现细节，不作为应用 API。底层 C++ 依赖库与宿主 C ABI 见[宿主集成](../08-host-integration.md)。

## 阅读约定

签名中 `static` 使用 `类型::方法`，实例方法使用 `对象.方法`。重载分别编号；泛型参数必须满足源码接口约束。`T`、`U`、`R` 等为类型参数，不能直接作为完整程序的具体类型。内建函数用等价调用签名表示，其参数检查由编译器完成。

参数为值时遵循值语义；`const&` 只读借用，`mut&` 可写借用。修改/释放资源会影响依赖它的借用；`const&` 不能调用 `drop`。显式释放统一使用 `value.drop();`，底层释放非幂等；编译器合并已知重复操作，释放后仍不能读取。

每项的“调用形式”展示如何排列参数，参数名代表调用者已准备的值，不能单独运行。模块页链接的 `.mgl` 是带入口的完整参考代码；有输入、线程、反射或外部服务条件时以示例说明为准。错误条件既包括库显式返回的 Option/Result，也包括断言、宿主检查和借用检查诊断。

## 模块目录

| 模块 | 内容 | 函数重载数 |
| --- | --- | ---: |
| [`std::binary`](std-binary.md) | 二进制读取 | 35 |
| [`std::cli`](std-cli.md) | 命令行参数 | 7 |
| [`std::clock`](std-clock.md) | 单调时钟 | 2 |
| [`std::collections`](std-collections.md) | 集合扩展 | 40 |
| [`std::complex`](std-complex.md) | 泛型复数 | 8 |
| [`std::compress`](std-compress.md) | 无损压缩 | 2 |
| [`std::concurrency`](std-concurrency.md) | 线程通信与共享 | 17 |
| [`std::csv`](std-csv.md) | CSV 读写 | 3 |
| [`std::datetime`](std-datetime.md) | UTC 日期时间 | 10 |
| [`std::encoding`](std-encoding.md) | 文本编码 | 7 |
| [`std::event`](std-event.md) | 显式事件与定时源 | 12 |
| [`std::graph`](std-graph.md) | 图论与并查集 | 18 |
| [`std::image`](std-image.md) | 图片处理 | 25 |
| [`std::io`](std-io.md) | 文件与流接口 | 37 |
| [`std::iter`](std-iter.md) | 惰性迭代器 | 109 |
| [`std::json`](std-json.md) | JSON 只读文档 | 27 |
| [`std::lang`](std-lang.md) | 核心库 | 218 |
| [`std::linalg`](std-linalg.md) | 向量与矩阵 | 56 |
| [`std::linkedlist`](std-linkedlist.md) | 双向链表 | 21 |
| [`std::math`](std-math.md) | 标量数学 | 23 |
| [`std::memory`](std-memory.md) | 内存视图与 arena | 9 |
| [`std::message`](std-message.md) | 线程邮箱 | 6 |
| [`std::net`](std-net.md) | TCP 与 HTTP | 37 |
| [`std::numerics`](std-numerics.md) | 数值分析 | 7 |
| [`std::numtheory`](std-numtheory.md) | 整数数论 | 7 |
| [`std::parse`](std-parse.md) | 严格文本解析 | 4 |
| [`std::path`](std-path.md) | 路径字符串 | 4 |
| [`std::probe`](std-probe.md) | 最小宿主集成探针 | 1 |
| [`std::process`](std-process.md) | 宿主进程 | 4 |
| [`std::random`](std-random.md) | 随机数 | 16 |
| [`std::reflect`](std-reflect.md) | 反射 | 37 |
| [`std::regex`](std-regex.md) | 正则表达式 | 7 |
| [`std::signal`](std-signal.md) | 复数信号变换 | 8 |
| [`std::sql`](std-sql.md) | 数据库 | 44 |
| [`std::text`](std-text.md) | UTF-8 文本工具 | 33 |
| [`std::thread`](std-thread.md) | 隔离线程 | 31 |
| [`std::toml`](std-toml.md) | TOML 只读文档 | 27 |
| [`std::tree`](std-tree.md) | 树与堆 | 45 |
| [`std::utf8`](std-utf8.md) | UTF-8 与码点 | 7 |
| [`std::watchdog`](std-watchdog.md) | 软重启看门狗 | 6 |
| [`std::algorithms`](std-algorithms.md) | 通用算法 | 28 |

## 查找与维护

[全部符号索引](symbols.md)按限定名排序，适合浏览器搜索；[API 清单](api-index.json)记录每个重载、源码和锚点。

参考页由 `python tools/library_reference.py` 根据声明和人工说明生成。`python tools/library_reference.py --check` 检查当前源码与入库页面是否一致；新增 API 后须同步补充行为、参数与示例。示例验证见 `python tests/magel_library_reference.py`。

[类型与字段清单](symbol-index.json)记录类型分类、字段类型、说明及源码位置，供编辑器与文档共用。
