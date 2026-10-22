# 07 标准库概览

逐项查询函数的声明、参数、默认值、返回值、错误边界和完整程序，见 [库函数 API 参考](library/README.md) 与 [全部函数索引](library/symbols.md)。

按能力域列出 ChaLi 自带的模块。除核心模块 `std::lang` 自动加载外，其余都要 `import`。

```chali
import std::math;
import std::json;
void main() {
    println("sin0={} sqrt16={}", std::math::sin(0.0), std::math::sqrt(16.0));
    match (std::json::parse("{\"a\": 1}")) {
        Result<std::json::JsonValue, String>::Ok(doc) => {
            println("isObject={} len={}", doc.isObject(), doc.len());
        }
        Result<std::json::JsonValue, String>::Err(e) => { println("err={}", e); }
    }

}
```

```text
sin0=0 sqrt16=4
isObject=1 len=1
```

---

## 1. 核心模块 `std::lang`（自动加载）

无需 `import`，直接可用。

| 能力 | 例子 |
| --- | --- |
| 输出与格式化 | `println(fmt, …)`、`format(fmt, …)` |
| 断言 | `assert(cond)` |
| 长度 | `len(x)`（字符串 / 容器） |
| 字符串 | `std::lang::String`，下标与切片按 UTF-8 字节 |
| 容器 | `List<T>`、`Map<K,V>`、`Set<T>` |
| 类型擦除盒子 | `std::lang::Any`（运行期多态用） |
| 数学常数 | `std::lang::Math::PI`、`std::lang::Math::E` |
| 大数 | `BigInteger`、`BigDecimal` |

---

## 2. 按能力域速查

### 数学与数值

| 模块 | 内容 |
| --- | --- |
| `std::math` | `sin` `cos` `sqrt` 等常用函数；`abs` `min` `max` 是泛型入口（`min`/`max` 任意数量实参），`clamp` 分 `int64`/`float64` 两份；常数见 `std::lang::Math` |
| `std::numerics` | 标量数值方法：导数、辛普森积分、二分、RK4、牛顿 |
| `std::numtheory` | 整数数论：模块级泛型 `gcd` `lcm`（任意数量实参）、`addMod` `mulMod` `powMod` `modInverse`、确定性 Miller-Rabin `isPrime` |
| `std::complex` | 复数 `Complex<T>`，支持加减乘除 trait |
| `std::linalg` | 稠密向量与矩阵、`luDecompose` `symmetricEigen` `svd` `jordan2` `diagonalize2` |
| `std::random` | 确定性 LCG 与熵播种；句柄型状态，实现 `Drop` |
| `std::signal` | DFT / FFT / 逆变换；基 2 FFT 只接受 2 的幂 |

### 文本与数据格式

| 模块 | 内容 |
| --- | --- |
| `std::text` | `Text::find` `split` `join` `replace` `trim` `toUpper` `padStart` …；`kmpFind` `kmpFindAll` `kmpFailureTable` 是字节级 KMP |
| `std::json` | `parse` → `Result<JsonValue, String>`；`isInt` `isObject` `len` `asInt` `asString` … |
| `std::toml` | 与 json 同构的 TOML 文档访问 |
| `std::regex` | 模式在宿主侧按串缓存，ChaLi 侧只持整数句柄 |
| `std::parse` | 字符串 → 整数的解析，带 `ParseError` |
| `std::binary` | 定宽解码、字节序处理与边界检查 |
| `std::path` | 路径拼接、文件名、扩展名、规范化 |
| `std::compress` | 文本压缩 / 解压；压缩结果是**二进制** `List<uint8>`，不要当字符串打印 |
| `std::image` | RGBA 图片的加载 / 保存（BMP、PPM、PGM）、像素读写、卷积、池化、灰度 / ARGB8888 / RGB565 转换与大津法二值化 |

### 数据结构与算法

| 模块 | 内容 |
| --- | --- |
| `std::tree` | `BinarySearchTree`（增删查、三种遍历、min/max/height）、`RedBlackTree`（插入 + 双红修复）、`MinHeap`（可当优先队列）、`GeneralTree`（首个孩子 / 下一兄弟，可与二叉表示互转） |
| `std::graph` | `Graph` 链式前向星：`bfs` `dfs` `connectedComponents` `topologicalSort` `dijkstra` `hasCycle` `minimumSpanningTreeWeight`；`UnionFind` 路径压缩 + 按大小合并 |
| `std::linkedlist` | `LinkedList` 双向链表：首尾与游标处 O(1) 插删、`reverse` `toArray` `fromArray` `find`，实现 `Iterator<int64>` 与 `Drop` |

这三个模块的全部算法都在宿主胶水（`glue/std_tree.hpp`、`glue/std_graph.hpp`、`glue/std_linkedlist.hpp`）里，`.chl` 只做公开 API 壳；`std::text` 的 KMP 同理（`glue/std_kmp.hpp`）。裁剪宏分别是 `NO_CHALI_STD_TREE` / `NO_CHALI_STD_GRAPH` / `NO_CHALI_STD_LINKEDLIST` / `NO_CHALI_STD_KMP`。

### 并发与协作

| 模块 | 内容 |
| --- | --- |
| `std::thread` | 隔离线程 `Thread` |
| `std::message` | 跨线程消息 |
| `std::concurrency` | 显式共享槽 `Shared<T>`；普通全局变量在线程间仍隔离 |
| `std::event` | 显式事件与定时源；通过 receive 接收，周期积压合并 |

> 线程执行、通道通信和显式事件分别由 std::thread、std::concurrency 和 std::event 提供。

### 系统与环境

| 模块 | 内容 |
| --- | --- |
| `std::io` | 文件读写，错误类型 `IoError`（`OpenFailed` `ReadFailed` `WriteFailed` `SeekFailed` `Closed`） |
| `std::net` | TCP + 极简 HTTP 客户端；套接字在宿主胶水里，ChaLi 侧只拿 `int32` 句柄 |
| `std::sql` | 连接 MySQL / SQLite / PostgreSQL / SQL Server；SQL 交给数据库引擎，ChaLi 负责收发 |
| `std::process` | 进程与系统调用；命令管道由宿主管柄池持有 |
| `std::clock` | 单调时钟 `steady_clock`，只做差值，不当挂钟用 |
| `std::memory` | `Span<T>`（引用地址 + 长度视图）；它的迭代器返回**元素引用**，`for (T mut& p in span)` 可直接写 |
| `std::watchdog` | 软重启看门狗（水位与超时两个维度都默认关闭），桌面与目标宿主共用一份实现 |

### 迭代

| 模块 | 内容 |
| --- | --- |
| `std::iter` | `range`、`map` / `filter` / `collect` 惰性链、`IterNext<T>` 接口 |

`std::probe` 是测试用桩模块，不是给应用用的。

---

## 3. 使用约定

**句柄型资源要显式关闭。** `std::json`、`std::regex`、`std::sql` 等持有宿主资源的值，用 `close()` 或实现 `Closeable` 让作用域帮你关。重复关闭返回 `false`，不会崩。

**压缩结果是二进制。** `std::compress` 的输出含 NUL 字节，类型是 `std::lang::List<uint8>`；解压出来的才是 `std::lang::String`。

模块能力由目标宿主提供的胶水、系统库及实际构建配置决定。嵌入式 Linux 使用普通 Linux 宿主，不额外设置模块禁用模式。详见[部署说明](06-deployment.md)。

**模块迁移后必须 import。** 早期这些符号在 `source/std/lang.chl` 注入正文里自动可见；现在除核心 `std::lang` 外都要显式 `import`。

---

## 4. 完整 API 去哪查

标准库源码本身就是最好的参考，每个模块头部都有能力说明：

- [`source/std/`](../source/std) —— 全部模块源码，`source/std/<名字>.chl`
- [`source/glue/`](../source/glue) —— 宿主侧 C++ 胶水，`source/glue/std_*.hpp` 是各模块绑定的权威定义
- [`example/samples/`](../example/samples/) —— 按特性组织的可运行样例
- 各能力域的 API 参考：[`doc/library/`](library/README.md) 按模块分页（`std-json.md`、`std-toml.md`、`std-regex.md`、`std-linalg.md`、`std-complex.md`、`std-net.md`、`std-sql.md` …）

---

## 5. 下一步

| 你想 | 去哪 |
| --- | --- |
| 让 ChaLi 调你的 C/C++ 代码 | [宿主集成与 C ABI](08-host-integration.md) |
| 查标准库之外的能力边界 | [语言参考](04-language-reference.md) |
| 看某个特性的可运行样例 | [`example/samples/`](../example/samples/) |

`std::encoding`、`std::csv`、`std::datetime`、`std::cli`、`std::utf8` 与文件系统扩展，见 [Any、显式泛型与通用数据工具](22-any-generics-data.md)。
