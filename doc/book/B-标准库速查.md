# 附录 B 标准库速查

Magel 的「标准库」不是一堆函数，而是一组**模块**：除核心 `std::lang` 自动加载外，
其余都得先 `import`。这份附录按**能力域**把它们列一遍，让你知道「想干这件事，
该去 `import` 谁」；每个模块的完整签名与错误边界，去[库函数 API 参考](../library/README.md)查。

> 模块名与方法对齐 **Magel 1.0.0**。终端中可以直接用 `magel man` 查模块，用
> `magel man 模块::符号` 查签名、说明和示例；命令说明见 [doc/man](../man/README.md)。

---

## B.0 先看一个例子

```magel
import std::math;
import std::text;
import std::iter;

int64 main()
{
    println("sqrt={}", std::math::sqrt(144.0));
    println("upper={}", std::text::Text::toUpper("magel"));
    int64 sum = 0;
    for (int64 v in std::iter::range(1, 5))
    {
        sum += v;
    }
    println("sum={}", sum);
    return 0;
}
```

```text
sqrt=12
upper=MAGEL
sum=10
```

三行分别来自三个模块：数学、文本、迭代。注意 `range(1, 5)` 是**左闭右开**
（拿到 1、2、3、4），所以 `sum` 是 10——这是第 17 章的规矩，标准库里凡带
「起点、终点」的地方都一样。

---

## B.1 核心模块 `std::lang`（不用 import）

这几样任何程序里都能直接用：

| 能力 | 用法 |
| --- | --- |
| 输出 | `println(fmt, …)`、`format(fmt, …)` |
| 断言 | `assert(cond)` |
| 取长度 | `len(x)`（字符串 / 容器） |
| 字符串 | `std::lang::String` |
| 容器 | `List<T>`、`Map<K,V>`、`Set<T>` |
| 类型擦除盒子 | `std::lang::Any`（第 13 章的运行期多态） |
| 数学常数 | `std::lang::Math::PI`、`std::lang::Math::E` |
| 大数 | `BigInteger`、`BigDecimal` |
| 输入 | `input(variable)` |
| 整行输入 | `std::lang::readLine()`，保留空格，不含 LF/CRLF |
| 多行输入 | `std::lang::readLines(int32 N)`，LF 连接，EOF 提前结束 |

> **早年间它们不在 `lang` 里也能用**（靠 `source/std/lang.mgl` 注入正文）。现在除了上面这些，
> 其它能力一律要显式 `import`——这是第 18 章「模块与工程」讲过的变化。

---

## B.2 按能力域速查

### 数学与数值

| 模块 | 能干什么 |
| --- | --- |
| `std::math` | `sin` `cos` `sqrt` 等常用函数；`abs` `min` `max` 是泛型入口，`min`/`max` 可以给任意多个实参（常数见 `std::lang::Math`） |
| `std::numerics` | 标量数值方法：导数、辛普森积分、二分、RK4、牛顿迭代 |
| `std::numtheory` | 整数数论：模块级泛型 `gcd` `lcm`（任意多个实参）、`addMod` `mulMod` `powMod` `modInverse`，以及确定性的 `isPrime`（Miller-Rabin） |
| `std::complex` | 复数 `Complex<T>`，加减乘除都是一等公民 |
| `std::linalg` | 稠密向量与矩阵：`luDecompose` `symmetricEigen` `svd` `jordan2` `diagonalize2` |
| `std::random` | 确定性随机数（LCG）与熵播种；句柄型状态，实现 `Drop` |
| `std::signal` | DFT / FFT 与逆变换；基 2 的 FFT 只接受 2 的幂长度 |

### 文本与数据格式

| 模块 | 能干什么 |
| --- | --- |
| `std::text` | `Text::find` `split` `join` `replace` `trim` `toUpper` `padStart` …… |
| `std::json` | `parse` → `Result<JsonValue, String>`；再 `isInt` `isObject` `len` `asInt` `asString` 取值 |
| `std::toml` | 与 json 同构的 TOML 文档访问 |
| `std::regex` | 正则；**模式缓存在宿主侧**，Magel 侧只拿一个整数句柄 |
| `std::parse` | 字符串 → 整数，失败给 `ParseError` |
| `std::binary` | 定宽解码、字节序处理与边界检查 |
| `std::path` | 路径拼接、取文件名、取扩展名、规范化 |
| `std::compress` | 文本压缩 / 解压；压缩结果是**二进制** `List<uint8>` |

### 并发与协作

| 模块 | 能干什么 |
| --- | --- |
| `std::coroutine` | 共享协程 |
| `std::thread` | 隔离线程 `IsolatedThread` |
| `std::message` | 跨协程 / 跨线程的消息 |
| `std::concurrency` | 显式共享槽 `Shared<T>`（普通全局变量在线程间仍是隔离的） |
| `std::event` | 中断事件与软件定时器；`start()` 后按周期在指令边界触发 |

> `SharedCoroutine` / `IsolatedThread` / `Message` 是**三个独立模块**，不能合并——
> 它们各自有同名的宿主函数 `create` / `wait` / `receive`。

### 系统与环境

| 模块 | 能干什么 |
| --- | --- |
| `std::io` | 文件读写；错误类型 `IoError`（`OpenFailed` `ReadFailed` `WriteFailed` `SeekFailed` `Closed`） |
| `std::net` | TCP 与极简 HTTP 客户端；套接字在宿主胶水里，Magel 侧只拿 `int32` 句柄 |
| `std::sql` | 连 MySQL / SQLite / PostgreSQL / SQL Server；SQL 交给数据库引擎执行 |
| `std::process` | 进程与系统调用；命令管道由宿主管柄池持有 |
| `std::clock` | 单调时钟 `steady_clock`，只用来做差值，别当挂钟用 |
| `std::memory` | `Span<T>`（指针 + 长度视图）；它的迭代器给的是**元素指针**，能直接写 |
| `std::watchdog` | 软重启看门狗（水位与超时两个维度都默认关闭），桌面与板端共用一份实现 |

### 迭代

| 模块 | 能干什么 |
| --- | --- |
| `std::iter` | `range`、`map` / `filter` / `collect` 惰性链，以及 `IterNext<T>` 接口 |

> `std::probe` 是测试用的桩模块，**不是给应用用的**。

---

## B.3 三条容易踩的约定

**一、句柄型资源要显式关。** `std::json`、`std::regex`、`std::sql` 这类持有宿主资源的
值，用 `close()` 关，或者让它实现 `Closeable`、由作用域替你关（第 8 章）。
重复关闭返回 `false`，不会崩。

**二、压缩结果是二进制，不是文本。** `std::compress` 的输出含 `\0` 字节，类型是
`std::lang::List<uint8>`；**别拿它当字符串打印**，解压出来的才是 `std::lang::String`。

**三、前端嵌入式配置固定禁用部分模块。** `std::net`、`std::process`、`std::sql`、
`std::io`、`std::thread`、`std::coroutine`、`std::concurrency` 在 `[embedded]` 目标下
`import` 就会编译报错——不是你的代码写错了，是那一端不提供这个能力。见第 22 章。

---

## B.4 细节去哪儿查

| 你想查 | 去哪 |
| --- | --- |
| 某个函数的确切签名、默认值、返回与错误 | [库函数 API 参考](../library/README.md) 与[全部函数索引](../library/symbols.md) |
| 按能力域通读一遍标准库 | [07 标准库概览](../07-stdlib-overview.md) |
| 标准库源码（每个模块头部都有能力说明） | [`source/std/`](../../source/std) 下的 `source/std/<名字>.mgl` |
| 宿主侧绑定（`std::*` 的真正实现） | [`source/glue/`](../../source/glue) 下的 `source/glue/std_*.hpp` |
| 可直接运行的标准库样例 | [`example/samples/`](../../example/samples/) |

> **模块名怎么读**：`std::json` 对应源码 `source/std/json.mgl`、胶水 `source/glue/std_json.hpp`，
> 三处名字一对就找到了。

---

## B.5 下一步

标准库认完了，前面二十几章也读完了——接下来该自己写点东西了。

- 想先复习报错怎么读：翻回[附录 A](A-常见错误速查.md)；
- 想把整本书的东西拼成一个完整程序：[第 23 章 综合项目](23-综合项目.md)；
- 想把这门语言接到你自己的 C/C++ 代码上：[第 21 章 与 C 互操作](21-与C互操作.md)。
