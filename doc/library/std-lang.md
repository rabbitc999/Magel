# std::lang — 核心库

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
// std::lang 自动加载
```

## 公共约定与错误边界

自动加载，无需 import。String 提供字节查找、切片、替换、修剪、填充、连接、分割、ASCII 大小写与分类，以及严格 UTF-8 码点操作；普通方法返回独立值，字节索引与码点索引明确区分，详情见字符串方法文档。Option/Result 通过 match 或 isValue/isOk 分支处理；unwrap/expect 在失败分支触发 panic。浮点转整数向零截断，范围检查见各重载；字节编码默认小端。拥有值按值复制，引用的生命周期不得超过来源。 List.pop 空列表报错，列表/集合/映射的按位置访问需有效下标。Set/Map 的 has/contains 返回 int64 的 0/1；List 的同名接口返回 bool。Any.getReference/getOptional 的 const/mut 由接收上下文决定，返回引用受盒子寿命限制。BigDecimal 精度范围 1..100000。内存内建接受有效存储引用，字节计数不是元素数量。$assert(condition) 和 $assert(condition,message) 为宏调用，自动添加调用点行号，宏体自带分号。

## 参考代码

完整程序：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

```powershell
chali run example/samples/std_collections_algorithms.chl
```

<details>
<summary>展开完整参考代码</summary>

```chali
import std::collections;

import std::memory;

import std::algorithms;

bool positive(int64 value) { return value > 0; }
int64 addValues(int64 left, int64 right) { return left + right; }

void main() {
    List<int64> values;
    values.push(4);
    values.push(1);
    values.push(3);
    List<int64> sorted = std::algorithms::sortValue<int64>(values);
    println("{} {} {} {} {}", sorted[0], sorted[1], sorted[2],
    std::algorithms::binarySearchValue<int64>(sorted, 3),
    std::algorithms::foldValue<int64>(sorted, 0, addValues));
    println("{} {} {}", values[0], values[1], values[2]);

    List<float64> stableValues;
    stableValues.push(-0.0);
    stableValues.push(2.0);
    stableValues.push(0.0);
    stableValues.push(1.0);
    List<float64> stableSorted = std::algorithms::sortValue<float64>(stableValues);
    println("{1:1F} {2:1F} {3:1F} {4:1F}", stableSorted[0], stableSorted[1],
    stableSorted[2], stableSorted[3]);
    println("{} {} {} {} {}", std::algorithms::findValue<int64>(sorted, 4),
    std::algorithms::minValue<int64>(9, 2), std::algorithms::maxValue<int64>(9, 2),
    std::algorithms::clampValue<int64>(12, 0, 10),
    std::algorithms::allValue<int64>(sorted, positive));
    println("{} {} {} {}", std::algorithms::minValue<int64>(9, 2, 7, -1),
    std::algorithms::maxValue<int64>(9, 2, 12, 7),
    std::algorithms::minValue<int64>(5), std::algorithms::maxValue<int64>(6));

    std::collections::Deque<int64> deque = std::collections::Deque<int64>::create();
    deque.pushBack(2);
    deque.pushFront(1);
    println("{} {} {}", deque.len(), deque.popFront().unwrap(),
    deque.popBack().unwrap());

    std::collections::Queue<int64> queue = std::collections::Queue<int64>::create();
    queue.push(7);
    queue.push(8);
    println("{} {}", queue.peek().unwrap(), queue.pop().unwrap());

    std::collections::RingBuffer<int64> ring = std::collections::RingBuffer<int64>::withCapacity(2);
    println("{} {} {}", ring.push(5), ring.push(6), ring.push(7));
    println("{} {}", ring.pop().unwrap(), ring.available());

    std::collections::FixedVector<int64> fixed = std::collections::FixedVector<int64>::withCapacity(1);
    println("{} {} {}", fixed.push(9), fixed.push(10), fixed[0]);

    int64 raw[] = new[2];
    raw[0] = 11;
    raw[1] = 12;
    std::memory::Span<int64> span = std::memory::Span<int64>::from(&raw[0], len(raw));
    println("{} {} {}", span.len(), span[0], span[1]);

    std::collections::Channel<int64> channel = std::collections::Channel<int64>::create();
    channel.send(21);
    println("{} {}", channel.len(), channel.receive().unwrap());

    Pair<int64, String> pair = Pair<int64, String>::create(7, "seven");
    println("{} {}", pair.first, pair.second);
    raw.drop();

}
```

</details>

<details>
<summary>示例输出</summary>

```text
1 3 4 1 8
4 1 3
-0.0 0.0 1.0 2.0
2 2 9 10 1
-1 12 5 6
2 1 2
7 7
1 1 0
5 1
1 0 9
2 11 12
1 21
7 seven
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-216"></a>`opcode String` | 拥有 UTF-8 文本的 opcode 值，复制产生独立可写缓冲。 |
| 模块 | <a id="symbol-217"></a>`opcode BigInteger` | 任意精度有符号整数，算术由大数运行时实现。 |
| 模块 | <a id="symbol-218"></a>`opcode BigDecimal` | 可配置精度的十进制大数。 |
| 模块 | <a id="symbol-219"></a>`#NonCopyable opcode File` | 不可复制的文件 opcode，作用域退出释放文件资源。 |
| 模块 | <a id="symbol-220"></a>`#NonCopyable opcode Process` | 不可复制的进程 opcode，操作由宿主胶水提供。 |
| 模块 | <a id="symbol-221"></a>`enum TryBranch<T, R>` | 传播分支：Continue 产生成功值，Break 携带失败载荷退出当前函数。 |
| TryBranch | `Continue(T value), Break(R residual)` | 传播分支：Continue 产生成功值，Break 携带失败载荷退出当前函数。 变体及载荷见声明。 |
| 模块 | <a id="symbol-222"></a>`struct ErrorResidual<E>` | Result 的错误传播载荷，保留错误类型身份。 |
| ErrorResidual | <a id="symbol-223"></a>`E error` | 最近操作的错误说明。 |
| 模块 | <a id="symbol-224"></a>`enum NoneResidual` | Option 无值分支的传播载荷。 |
| NoneResidual | `None` | Option 无值分支的传播载荷。 变体及载荷见声明。 |
| 模块 | <a id="symbol-225"></a>`interface Try<T, R>` | 只读拆分传播值的显式编译期协议。 |
| 模块 | <a id="symbol-226"></a>`interface TryMove<T, R>` | 移动拥有值后拆分传播值的显式编译期协议。 |
| 模块 | <a id="symbol-227"></a>`interface FromResidual<F, R>` | 以静态方法从传播载荷构造函数返回值的编译期协议。 |
| 模块 | <a id="symbol-228"></a>`enum Option<T> : Try<T, NoneResidual>, TryMove<T, NoneResidual>, FromResidual<Option<T>, NoneResidual>` | 可选值：Value(T) 表示存在，None 表示缺失。 |
| Option | `Value(T value), None` | 可选值：Value(T) 表示存在，None 表示缺失。 变体及载荷见声明。 |
| 模块 | <a id="symbol-229"></a>`enum Result<T, E> : Try<T, ErrorResidual<E>>, TryMove<T, ErrorResidual<E>>, FromResidual<Result<T, E>, ErrorResidual<E>>` | 操作结果：Ok(T) 表示成功，Err(E) 保存错误。 |
| Result | `Ok(T value), Err(E error)` | 操作结果：Ok(T) 表示成功，Err(E) 保存错误。 变体及载荷见声明。 |
| 模块 | <a id="symbol-230"></a>`interface NextValue<T>` | 自定义区间的有状态前进协议。
@note nextValue 从接收者的游标状态产生下一项并推进状态；到达终点后返回 None。 |
| 模块 | <a id="symbol-231"></a>`struct Pair<K, V>` | 包含 first 和 second 的两个值，分别使用 K 与 V 类型。 |
| Pair | <a id="symbol-232"></a>`K first` | 第一项，类型为 K。 |
| Pair | <a id="symbol-233"></a>`V second` | 第二项，类型为 V。 |
| 模块 | <a id="symbol-234"></a>`interface Drop` | 显式释放拥有资源的析构协议。 |
| 模块 | <a id="symbol-235"></a>`interface Closeable` | 关闭外部资源的协议。 |
| 模块 | <a id="symbol-236"></a>`struct List<T>` | 可变长度的顺序值容器，支持按下标访问、追加、删除与复制。 |
| 模块 | <a id="symbol-237"></a>`struct Set<T>` | 保存唯一元素的值集合，支持存在检查和集合运算。 |
| 模块 | <a id="symbol-238"></a>`struct Map<K,V>` | 从唯一键到值的映射，支持键查询和按遍历位置读取。 |
| 模块 | <a id="symbol-239"></a>`struct Any` | 拥有一个装箱值的类型擦除容器；通过类型检查安全读取。 |
| 模块 | <a id="symbol-240"></a>`struct Math` | 核心数学常量和数值操作的静态入口。 |
| Math | <a id="symbol-241"></a>`static const float64 PI` | 圆周率常量。 |
| Math | <a id="symbol-242"></a>`static const float64 E` | 自然对数底数常量。 |
| 模块 | <a id="symbol-243"></a>`interface Add<R,T>` | 左操作数的加法重载接口。 |
| 模块 | <a id="symbol-244"></a>`interface RightAdd<R,T>` | 右操作数的加法重载接口。 |
| 模块 | <a id="symbol-245"></a>`interface Subtract<R,T>` | 左操作数的减法重载接口。 |
| 模块 | <a id="symbol-246"></a>`interface RightSubtract<R,T>` | 右操作数的减法重载接口。 |
| 模块 | <a id="symbol-247"></a>`interface Multiply<R,T>` | 左操作数的乘法重载接口。 |
| 模块 | <a id="symbol-248"></a>`interface RightMultiply<R,T>` | 右操作数的乘法重载接口。 |
| 模块 | <a id="symbol-249"></a>`interface Divide<R,T>` | 左操作数的除法重载接口。 |
| 模块 | <a id="symbol-250"></a>`interface RightDivide<R,T>` | 右操作数的除法重载接口。 |
| 模块 | <a id="symbol-251"></a>`interface Modulo<R,T>` | 左操作数的取模重载接口。 |
| 模块 | <a id="symbol-252"></a>`interface RightModulo<R,T>` | 右操作数的取模重载接口。 |
| 模块 | <a id="symbol-253"></a>`interface Equal<R,T>` | 左操作数的相等比较重载接口。 |
| 模块 | <a id="symbol-254"></a>`interface RightEqual<R,T>` | 右操作数的相等比较重载接口。 |
| 模块 | <a id="symbol-255"></a>`interface NotEqual<R,T>` | 左操作数的不等比较重载接口。 |
| 模块 | <a id="symbol-256"></a>`interface RightNotEqual<R,T>` | 右操作数的不等比较重载接口。 |
| 模块 | <a id="symbol-257"></a>`interface Less<R,T>` | 左操作数的小于比较重载接口。 |
| 模块 | <a id="symbol-258"></a>`interface RightLess<R,T>` | 右操作数的小于比较重载接口。 |
| 模块 | <a id="symbol-259"></a>`interface LessOrEqual<R,T>` | 左操作数的小于等于比较重载接口。 |
| 模块 | <a id="symbol-260"></a>`interface RightLessOrEqual<R,T>` | 右操作数的小于等于比较重载接口。 |
| 模块 | <a id="symbol-261"></a>`interface Greater<R,T>` | 左操作数的大于比较重载接口。 |
| 模块 | <a id="symbol-262"></a>`interface RightGreater<R,T>` | 右操作数的大于比较重载接口。 |
| 模块 | <a id="symbol-263"></a>`interface GreaterOrEqual<R,T>` | 左操作数的大于等于比较重载接口。 |
| 模块 | <a id="symbol-264"></a>`interface RightGreaterOrEqual<R,T>` | 右操作数的大于等于比较重载接口。 |
| 模块 | <a id="symbol-265"></a>`interface And<R,T>` | 左操作数的按位与重载接口。 |
| 模块 | <a id="symbol-266"></a>`interface RightAnd<R,T>` | 右操作数的按位与重载接口。 |
| 模块 | <a id="symbol-267"></a>`interface Or<R,T>` | 左操作数的按位或重载接口。 |
| 模块 | <a id="symbol-268"></a>`interface RightOr<R,T>` | 右操作数的按位或重载接口。 |
| 模块 | <a id="symbol-269"></a>`interface Xor<R,T>` | 左操作数的按位异或重载接口。 |
| 模块 | <a id="symbol-270"></a>`interface RightXor<R,T>` | 右操作数的按位异或重载接口。 |
| 模块 | <a id="symbol-271"></a>`interface ShiftLeft<R,T>` | 左操作数的左移重载接口。 |
| 模块 | <a id="symbol-272"></a>`interface RightShiftLeft<R,T>` | 右操作数的左移重载接口。 |
| 模块 | <a id="symbol-273"></a>`interface ShiftRight<R,T>` | 左操作数的右移重载接口。 |
| 模块 | <a id="symbol-274"></a>`interface RightShiftRight<R,T>` | 右操作数的右移重载接口。 |
| 模块 | <a id="symbol-275"></a>`interface Negate<R,T>` | 左操作数的取负重载接口。 |
| 模块 | <a id="symbol-276"></a>`interface BitNot<R,T>` | 左操作数的按位取反重载接口。 |
| 模块 | <a id="symbol-277"></a>`interface Positive<R,T>` | 左操作数的取正重载接口。 |
| 模块 | <a id="symbol-278"></a>`interface Hash<T>` | 为值提供哈希计算的接口。 |
| 模块 | <a id="symbol-279"></a>`interface ExplicitConvert<T>` | 显式类型转换的接口。 |
| 模块 | <a id="symbol-280"></a>`interface ImplicitConvert<T>` | 隐式类型转换的接口。 |
| 模块 | <a id="symbol-281"></a>`interface Index<R,T>` | 自定义下标访问接口；与小写 index 连续存储函数区分。 |
| 模块 | <a id="symbol-282"></a>`interface Iterator<R>` | 供 for-in 获取和推进迭代状态的接口。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [String::length](#api-0) | `int64 length()` |
| [String::size](#api-1) | `int64 size()` |
| [String::byteLength](#api-2) | `int64 byteLength()` |
| [String::isEmpty](#api-3) | `bool isEmpty()` |
| [String::isBlank](#api-4) | `bool isBlank()` |
| [String::isDigit](#api-5) | `bool isDigit()` |
| [String::isAlpha](#api-6) | `bool isAlpha()` |
| [String::isAlnum](#api-7) | `bool isAlnum()` |
| [String::isAscii](#api-8) | `bool isAscii()` |
| [String::charLength](#api-9) | `int64 charLength()` |
| [String::indexOf](#api-10) | `int64 indexOf(String text, int64 start = 0)` |
| [String::lastIndexOf](#api-11) | `int64 lastIndexOf(String text, int64 start = 0 - 1)` |
| [String::contains](#api-12) | `bool contains(String text)` |
| [String::startsWith](#api-13) | `bool startsWith(String text)` |
| [String::endsWith](#api-14) | `bool endsWith(String text)` |
| [String::count](#api-15) | `int64 count(String text)` |
| [String::compare](#api-16) | `int64 compare(String text)` |
| [String::byteAt](#api-17) | `Option<uint8> byteAt(int64 index)` |
| [String::substring](#api-18) | `String substring(int64 start, int64 count = 0 - 1)` |
| [String::slice](#api-19) | `String slice(int64 start, int64 end)` |
| [String::left](#api-20) | `String left(int64 count)` |
| [String::right](#api-21) | `String right(int64 count)` |
| [String::trim](#api-22) | `String trim()` |
| [String::trimStart](#api-23) | `String trimStart()` |
| [String::trimEnd](#api-24) | `String trimEnd()` |
| [String::toLower](#api-25) | `String toLower()` |
| [String::toUpper](#api-26) | `String toUpper()` |
| [String::replaceFirst](#api-27) | `String replaceFirst(String before, String after)` |
| [String::replaceAll](#api-28) | `String replaceAll(String before, String after)` |
| [String::replace](#api-29) | `String replace(String before, String after)` |
| [String::repeat](#api-30) | `String repeat(int64 count)` |
| [String::reverse](#api-31) | `String reverse()` |
| [String::padStart](#api-32) | `String padStart(int64 length, String fill = " ")` |
| [String::padEnd](#api-33) | `String padEnd(int64 length, String fill = " ")` |
| [String::removePrefix](#api-34) | `String removePrefix(String prefix)` |
| [String::removeSuffix](#api-35) | `String removeSuffix(String suffix)` |
| [String::insert](#api-36) | `String insert(int64 index, String text)` |
| [String::remove](#api-37) | `String remove(int64 start, int64 count = 0 - 1)` |
| [String::utf8Substring](#api-38) | `String utf8Substring(int64 start, int64 count = 0 - 1)` |
| [String::charAt](#api-39) | `Option<String> charAt(int64 index)` |
| [String::split](#api-40) | `List<String> split(String separator, bool keepEmpty = true)` |
| [String::splitWhitespace](#api-41) | `List<String> splitWhitespace()` |
| [String::lines](#api-42) | `List<String> lines(bool keepEmpty = true)` |
| [String::unicodeChars](#api-43) | `List<String> unicodeChars()` |
| [String::bytes](#api-44) | `List<uint8> bytes()` |
| [String::join](#api-45) | `String join(List<String> items)` |
| [String::concat](#api-46) | `String concat(String text)` |
| [String::prepend](#api-47) | `String prepend(String text)` |
| [File::print](#api-48) | `int64 print(String text)` |
| [File::println](#api-49) | `int64 println(String text = "")` |
| [readLine](#api-50) | `String readLine()` |
| [readLines](#api-51) | `String readLines(int32 N)` |
| [readLines](#api-52) | `String readLines(int64 N)` |
| [toInt32](#api-53) | `Option<int32> toInt32(float64 value)` |
| [toInt32](#api-54) | `Option<int32> toInt32(float32 value)` |
| [toInt64](#api-55) | `int64 toInt64(float64 value)` |
| [toInt64](#api-56) | `int64 toInt64(float32 value)` |
| [float32ToBytes](#api-57) | `std::lang::List<uint8> float32ToBytes(float32 value, bool bigEndian)` |
| [float32ToBytes](#api-58) | `std::lang::List<uint8> float32ToBytes(float32 value)` |
| [bytesToFloat32](#api-59) | `Option<float32> bytesToFloat32(std::lang::List<uint8> bytes, bool bigEndian)` |
| [bytesToFloat32](#api-60) | `Option<float32> bytesToFloat32(std::lang::List<uint8> bytes)` |
| [bytesToFloat32](#api-61) | `Option<float32> bytesToFloat32(uint8 const& bytes, int64 count, bool bigEndian)` |
| [bytesToFloat32](#api-62) | `Option<float32> bytesToFloat32(uint8 const& bytes, int64 count)` |
| [bytesToFloat32](#api-63) | `Option<float32> bytesToFloat32(uint8 bytes[], bool bigEndian)` |
| [bytesToFloat32](#api-64) | `Option<float32> bytesToFloat32(uint8 bytes[])` |
| [float32ToBytes](#api-65) | `bool float32ToBytes(float32 value, uint8 mut& bytes, int64 count, bool bigEndian)` |
| [float32ToBytes](#api-66) | `bool float32ToBytes(float32 value, uint8 mut& bytes, int64 count)` |
| [float32ToBytes](#api-67) | `bool float32ToBytes(float32 value, uint8[] mut& bytes, bool bigEndian)` |
| [float32ToBytes](#api-68) | `bool float32ToBytes(float32 value, uint8[] mut& bytes)` |
| [float64ToBytes](#api-69) | `std::lang::List<uint8> float64ToBytes(float64 value, bool bigEndian)` |
| [float64ToBytes](#api-70) | `std::lang::List<uint8> float64ToBytes(float64 value)` |
| [bytesToFloat64](#api-71) | `Option<float64> bytesToFloat64(std::lang::List<uint8> bytes, bool bigEndian)` |
| [bytesToFloat64](#api-72) | `Option<float64> bytesToFloat64(std::lang::List<uint8> bytes)` |
| [bytesToFloat64](#api-73) | `Option<float64> bytesToFloat64(uint8 const& bytes, int64 count, bool bigEndian)` |
| [bytesToFloat64](#api-74) | `Option<float64> bytesToFloat64(uint8 const& bytes, int64 count)` |
| [bytesToFloat64](#api-75) | `Option<float64> bytesToFloat64(uint8 bytes[], bool bigEndian)` |
| [bytesToFloat64](#api-76) | `Option<float64> bytesToFloat64(uint8 bytes[])` |
| [float64ToBytes](#api-77) | `bool float64ToBytes(float64 value, uint8 mut& bytes, int64 count, bool bigEndian)` |
| [float64ToBytes](#api-78) | `bool float64ToBytes(float64 value, uint8 mut& bytes, int64 count)` |
| [float64ToBytes](#api-79) | `bool float64ToBytes(float64 value, uint8[] mut& bytes, bool bigEndian)` |
| [float64ToBytes](#api-80) | `bool float64ToBytes(float64 value, uint8[] mut& bytes)` |
| [assert](#api-81) | `void assert(bool condition)` |
| [assert](#api-82) | `void assert(bool condition, std::lang::String message)` |
| [assert](#api-83) | `void assert(bool condition, int64 line)` |
| [assert](#api-84) | `void assert(bool condition, std::lang::String message, int64 line)` |
| [Try::branch](#api-85) | `TryBranch<T, R> branch()` |
| [TryMove::intoBranch](#api-86) | `TryBranch<T, R> intoBranch()` |
| [FromResidual::fromResidual](#api-87) | `static F fromResidual(R residual)` |
| [Option::branch](#api-88) | `TryBranch<T, NoneResidual> branch()` |
| [Option::intoBranch](#api-89) | `TryBranch<T, NoneResidual> intoBranch()` |
| [Option::fromResidual](#api-90) | `static Option<T> fromResidual(NoneResidual residual)` |
| [Option::isValue](#api-91) | `bool isValue()` |
| [Option::isNone](#api-92) | `bool isNone()` |
| [Option::take](#api-93) | `T take()` |
| [Option::unwrap](#api-94) | `T unwrap()` |
| [Option::unwrapOr](#api-95) | `T unwrapOr(T fallback)` |
| [Option::expect](#api-96) | `T expect(std::lang::String message)` |
| [Option::transform](#api-97) | `Option<T> transform(T fn(T) mapper)` |
| [Option::andThen](#api-98) | `Option<T> andThen(Option<T> fn(T) mapper)` |
| [Result::branch](#api-99) | `TryBranch<T, ErrorResidual<E>> branch()` |
| [Result::intoBranch](#api-100) | `TryBranch<T, ErrorResidual<E>> intoBranch()` |
| [Result::fromResidual](#api-101) | `static Result<T,E> fromResidual(ErrorResidual<E> residual)` |
| [Result::ok](#api-102) | `Option<T> ok()` |
| [Result::isOk](#api-103) | `bool isOk()` |
| [Result::isErr](#api-104) | `bool isErr()` |
| [Result::take](#api-105) | `T take()` |
| [Result::unwrap](#api-106) | `T unwrap()` |
| [Result::unwrapOr](#api-107) | `T unwrapOr(T fallback)` |
| [Result::expect](#api-108) | `T expect(std::lang::String message)` |
| [Result::takeError](#api-109) | `E takeError()` |
| [Result::error](#api-110) | `E error()` |
| [Result::transform](#api-111) | `Result<T, E> transform(T fn(T) mapper)` |
| [Result::andThen](#api-112) | `Result<T, E> andThen(Result<T, E> fn(T) mapper)` |
| [NextValue::nextValue](#api-113) | `Option<T> nextValue(T end, bool inclusive)` |
| [Pair::create](#api-114) | `static Pair<K, V> create(K first, V second)` |
| [Pair::create](#api-115) | `static Pair<K, V> create((K, V) value)` |
| [Pair::toTuple](#api-116) | `(K, V) toTuple()` |
| [tryGet](#api-117) | `Option<T> tryGet<T>(Any const& value)` |
| [tryGetReference](#api-118) | `Option<T const&> tryGetReference<T>(Any const& value)` |
| [tryGetMut](#api-119) | `Option<T mut&> tryGetMut<T>(Any mut& value)` |
| [stringBytes](#api-120) | `List<uint8> stringBytes(String text)` |
| [range](#api-121) | `__Range range(int64 start, int64 stop)` |
| [range](#api-122) | `__Range range(int64 start, int64 stop, int64 step)` |
| [rangeEq](#api-123) | `__Interval_int64 rangeEq(int64 start, int64 stop)` |
| [Drop::drop](#api-124) | `void drop()` |
| [Closeable::close](#api-125) | `void close()` |
| [print](#api-126) | `int64 print(String format, Any args...)` |
| [println](#api-127) | `int64 println(String format, Any args...)` |
| [eprint](#api-128) | `int64 eprint(String format, Any args...)` |
| [eprintln](#api-129) | `int64 eprintln(String format, Any args...)` |
| [format](#api-130) | `String format(String format, Any args...)` |
| [putChar](#api-131) | `int64 putChar(char value)` |
| [pause](#api-132) | `int64 pause()` |
| [inputLine](#api-133) | `String inputLine()` |
| [inputInt](#api-134) | `int64 inputInt()` |
| [inputUInt](#api-135) | `uint64 inputUInt()` |
| [inputFloat](#api-136) | `float64 inputFloat()` |
| [inputBool](#api-137) | `bool inputBool()` |
| [inputChar](#api-138) | `char inputChar()` |
| [inputBigInteger](#api-139) | `BigInteger inputBigInteger()` |
| [inputBigDecimal](#api-140) | `BigDecimal inputBigDecimal()` |
| [panic](#api-141) | `int64 panic(String message)` |
| [len](#api-142) | `int64 len(Any value)` |
| [move](#api-143) | `T move<T>(T mut& value)` |
| [toReference](#api-144) | `T mut& toReference<T>(Option<T mut&> value)` |
| [toOptional](#api-145) | `Option<T mut&> toOptional<T>(T mut& value)` |
| [memcpy](#api-146) | `int64 memcpy(char mut& destination, char const& source, int64 bytes)` |
| [memset](#api-147) | `int64 memset(char mut& destination, int64 value, int64 bytes)` |
| [List::copy](#api-148) | `List<T> copy()` |
| [List::push](#api-149) | `int64 push(T value)` |
| [List::insert](#api-150) | `int64 insert(int64 index, T value)` |
| [List::at](#api-151) | `T at(int64 index)` |
| [List::pop](#api-152) | `T pop()` |
| [List::remove](#api-153) | `int64 remove(int64 index)` |
| [List::indexOf](#api-154) | `int64 indexOf(T value)` |
| [List::contains](#api-155) | `bool contains(T value)` |
| [List::has](#api-156) | `bool has(T value)` |
| [List::clear](#api-157) | `int64 clear()` |
| [List::len](#api-158) | `int64 len()` |
| [Set::copy](#api-159) | `Set<T> copy()` |
| [Set::add](#api-160) | `int64 add(T value)` |
| [Set::has](#api-161) | `int64 has(T value)` |
| [Set::contains](#api-162) | `int64 contains(T value)` |
| [Set::remove](#api-163) | `int64 remove(T value)` |
| [Set::at](#api-164) | `T at(int64 index)` |
| [Set::clear](#api-165) | `int64 clear()` |
| [Set::len](#api-166) | `int64 len()` |
| [Map::copy](#api-167) | `Map<K,V> copy()` |
| [Map::put](#api-168) | `int64 put(K key,V value)` |
| [Map::has](#api-169) | `int64 has(K key)` |
| [Map::contains](#api-170) | `int64 contains(K key)` |
| [Map::remove](#api-171) | `int64 remove(K key)` |
| [Map::keyAt](#api-172) | `K keyAt(int64 index)` |
| [Map::valAt](#api-173) | `V valAt(int64 index)` |
| [Map::clear](#api-174) | `int64 clear()` |
| [Map::len](#api-175) | `int64 len()` |
| [BigInteger::str](#api-176) | `String str()` |
| [BigInteger::copy](#api-177) | `BigInteger copy()` |
| [BigInteger::pow](#api-178) | `BigInteger pow(int64 exponent)` |
| [BigInteger::powmod](#api-179) | `BigInteger powmod(BigInteger exponent, BigInteger modulus)` |
| [BigInteger::gcd](#api-180) | `BigInteger gcd(BigInteger other)` |
| [BigInteger::lcm](#api-181) | `BigInteger lcm(BigInteger other)` |
| [BigInteger::mod](#api-182) | `BigInteger mod(BigInteger modulus)` |
| [BigInteger::modInverse](#api-183) | `BigInteger modInverse(BigInteger modulus)` |
| [BigInteger::isProbablePrime](#api-184) | `bool isProbablePrime(int64 rounds)` |
| [BigInteger::nextProbablePrime](#api-185) | `BigInteger nextProbablePrime()` |
| [BigInteger::bitLength](#api-186) | `int64 bitLength()` |
| [BigInteger::bitAnd](#api-187) | `BigInteger bitAnd(BigInteger other)` |
| [BigInteger::bitOr](#api-188) | `BigInteger bitOr(BigInteger other)` |
| [BigInteger::bitXor](#api-189) | `BigInteger bitXor(BigInteger other)` |
| [BigInteger::strRadix](#api-190) | `String strRadix(int64 base)` |
| [BigInteger::iroot](#api-191) | `BigInteger iroot(int64 k)` |
| [BigDecimal::str](#api-192) | `String str()` |
| [BigDecimal::copy](#api-193) | `BigDecimal copy()` |
| [BigDecimal::setPrecision](#api-194) | `int64 setPrecision(int64 digits)` |
| [BigDecimal::pow](#api-195) | `BigDecimal pow(int64 exponent)` |
| [BigDecimal::abs](#api-196) | `BigDecimal abs()` |
| [BigDecimal::isInteger](#api-197) | `bool isInteger()` |
| [BigDecimal::floor](#api-198) | `BigInteger floor()` |
| [BigDecimal::ceil](#api-199) | `BigInteger ceil()` |
| [BigDecimal::trunc](#api-200) | `BigInteger trunc()` |
| [BigDecimal::sqrt](#api-201) | `BigDecimal sqrt(int64 digits)` |
| [BigDecimal::root](#api-202) | `BigDecimal root(int64 k, int64 digits)` |
| [Any::copy](#api-203) | `Any copy()` |
| [Any::isEmpty](#api-204) | `bool isEmpty()` |
| [Any::clear](#api-205) | `void clear()` |
| [Any::set](#api-206) | `int64 set(T value)` |
| [Any::getValue](#api-207) | `T getValue()` |
| [Any::getReference](#api-208) | `T mut& getReference()` |
| [Any::getOptional](#api-209) | `Option<T mut&> getOptional()` |
| [eq](#api-210) | `bool eq(T left,T right)` |
| [ne](#api-211) | `bool ne(T left,T right)` |
| [lt](#api-212) | `bool lt(T left,T right)` |
| [le](#api-213) | `bool le(T left,T right)` |
| [gt](#api-214) | `bool gt(T left,T right)` |
| [ge](#api-215) | `bool ge(T left,T right)` |
| [add](#api-216) | `R add(T left,U right)` |
| [sub](#api-217) | `R sub(T left,U right)` |
| [mul](#api-218) | `R mul(T left,U right)` |
| [div](#api-219) | `R div(T left,U right)` |
| [mod](#api-220) | `R mod(T left,U right)` |
| [index](#api-221) | `T index(T const& data,int64 index)` |
| [Add::leftAdd](#api-222) | `R leftAdd(T value)` |
| [RightAdd::rightAdd](#api-223) | `R rightAdd(T value)` |
| [Subtract::leftSub](#api-224) | `R leftSub(T value)` |
| [RightSubtract::rightSub](#api-225) | `R rightSub(T value)` |
| [Multiply::leftMul](#api-226) | `R leftMul(T value)` |
| [RightMultiply::rightMul](#api-227) | `R rightMul(T value)` |
| [Divide::leftDiv](#api-228) | `R leftDiv(T value)` |
| [RightDivide::rightDiv](#api-229) | `R rightDiv(T value)` |
| [Modulo::leftMod](#api-230) | `R leftMod(T value)` |
| [RightModulo::rightMod](#api-231) | `R rightMod(T value)` |
| [Equal::leftEqual](#api-232) | `R leftEqual(T value)` |
| [RightEqual::rightEqual](#api-233) | `R rightEqual(T value)` |
| [NotEqual::leftNotEqual](#api-234) | `R leftNotEqual(T value)` |
| [RightNotEqual::rightNotEqual](#api-235) | `R rightNotEqual(T value)` |
| [Less::leftLess](#api-236) | `R leftLess(T value)` |
| [RightLess::rightLess](#api-237) | `R rightLess(T value)` |
| [LessOrEqual::leftLessEqual](#api-238) | `R leftLessEqual(T value)` |
| [RightLessOrEqual::rightLessEqual](#api-239) | `R rightLessEqual(T value)` |
| [Greater::leftGreater](#api-240) | `R leftGreater(T value)` |
| [RightGreater::rightGreater](#api-241) | `R rightGreater(T value)` |
| [GreaterOrEqual::leftGreaterEqual](#api-242) | `R leftGreaterEqual(T value)` |
| [RightGreaterOrEqual::rightGreaterEqual](#api-243) | `R rightGreaterEqual(T value)` |
| [And::leftBitAnd](#api-244) | `R leftBitAnd(T value)` |
| [RightAnd::rightBitAnd](#api-245) | `R rightBitAnd(T value)` |
| [Or::leftBitOr](#api-246) | `R leftBitOr(T value)` |
| [RightOr::rightBitOr](#api-247) | `R rightBitOr(T value)` |
| [Xor::leftBitXor](#api-248) | `R leftBitXor(T value)` |
| [RightXor::rightBitXor](#api-249) | `R rightBitXor(T value)` |
| [ShiftLeft::leftShiftLeft](#api-250) | `R leftShiftLeft(T value)` |
| [RightShiftLeft::rightShiftLeft](#api-251) | `R rightShiftLeft(T value)` |
| [ShiftRight::leftShiftRight](#api-252) | `R leftShiftRight(T value)` |
| [RightShiftRight::rightShiftRight](#api-253) | `R rightShiftRight(T value)` |
| [Negate::negate](#api-254) | `R negate(T value)` |
| [BitNot::bitNot](#api-255) | `R bitNot(T value)` |
| [Positive::positive](#api-256) | `R positive(T value)` |
| [Hash::hash](#api-257) | `uint64 hash(T value)` |
| [ExplicitConvert::explicitConvert](#api-258) | `T explicitConvert()` |
| [ImplicitConvert::implicitConvert](#api-259) | `T implicitConvert()` |
| [Index::index](#api-260) | `R index(T key)` |
| [Iterator::iterator](#api-261) | `Option<R> iterator(int64 index)` |
| [Result::mapErr](#api-262) | `Result<T,F> mapErr<F>(F fn(E) mapper)` |
| [Option::okOr](#api-263) | `Result<T,E> okOr<E>(E error)` |
| [Option::okOrElse](#api-264) | `Result<T,E> okOrElse<E>(E fn() factory)` |

<a id="api-0"></a>

### String::length（1）

**声明**

```chali
int64 length();
```

**行为**

返回 UTF-8 文本的字节长度。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。不包括终止零字节的长度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.length();
```

**源码**：[source/std/lang.chl:15](../../source/std/lang.chl#L15)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-1"></a>

### String::size（2）

**声明**

```chali
int64 size();
```

**行为**

返回存储的总长度。

返回 UTF-8 字节数，与 length 一致。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回存储的总长度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.size();
```

**源码**：[source/std/lang.chl:17](../../source/std/lang.chl#L17)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-2"></a>

### String::byteLength（3）

**声明**

```chali
int64 byteLength();
```

**行为**

返回字节数，不包含终止零字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回字节数，不包含终止零字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.byteLength();
```

**源码**：[source/std/lang.chl:20](../../source/std/lang.chl#L20)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-3"></a>

### String::isEmpty（4）

**声明**

```chali
bool isEmpty();
```

**行为**

判断 Any 是否为空盒子，不公开内部执行期标签。

是否为空字符串。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 Any 是否为空盒子，不公开内部执行期标签。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isEmpty();
```

**源码**：[source/std/lang.chl:23](../../source/std/lang.chl#L23)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-4"></a>

### String::isBlank（5）

**声明**

```chali
bool isBlank();
```

**行为**

是否仅含 ASCII 空白；空字符串也返回 true。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。是否仅含 ASCII 空白；空字符串也返回 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isBlank();
```

**源码**：[source/std/lang.chl:26](../../source/std/lang.chl#L26)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-5"></a>

### String::isDigit（6）

**声明**

```chali
bool isDigit();
```

**行为**

是否为非空 ASCII 十进制数字串。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。是否为非空 ASCII 十进制数字串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isDigit();
```

**源码**：[source/std/lang.chl:29](../../source/std/lang.chl#L29)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-6"></a>

### String::isAlpha（7）

**声明**

```chali
bool isAlpha();
```

**行为**

是否为非空 ASCII 字母串。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。是否为非空 ASCII 字母串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isAlpha();
```

**源码**：[source/std/lang.chl:32](../../source/std/lang.chl#L32)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-7"></a>

### String::isAlnum（8）

**声明**

```chali
bool isAlnum();
```

**行为**

是否为非空 ASCII 字母数字串。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。是否为非空 ASCII 字母数字串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isAlnum();
```

**源码**：[source/std/lang.chl:35](../../source/std/lang.chl#L35)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-8"></a>

### String::isAscii（9）

**声明**

```chali
bool isAscii();
```

**行为**

所有字节是否均小于 128；空串返回 true。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。所有字节是否均小于 128；空串返回 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isAscii();
```

**源码**：[source/std/lang.chl:38](../../source/std/lang.chl#L38)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-9"></a>

### String::charLength（10）

**声明**

```chali
int64 charLength();
```

**行为**

严格解码 UTF-8 后的 Unicode 码点数，非字素簇数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。严格解码 UTF-8 后的 Unicode 码点数，非字素簇数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.charLength();
```

**源码**：[source/std/lang.chl:41](../../source/std/lang.chl#L41)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-10"></a>

### String::indexOf（11）

**声明**

```chali
int64 indexOf(String text, int64 start = 0);
```

**行为**

返回首次出现的下标；未找到返回 -1。

从 start 字节起搜索，未找到返回 -1；负起点按零处理。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |
| `start` | `int64 start` | 起始值 | `0` |

**返回值**

`int64`。返回首次出现的下标；未找到返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.indexOf(text, start);
```

**源码**：[source/std/lang.chl:44](../../source/std/lang.chl#L44)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-11"></a>

### String::lastIndexOf（12）

**声明**

```chali
int64 lastIndexOf(String text, int64 start = 0 - 1);
```

**行为**

从 start 字节向前搜索，负起点表示从末尾开始。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |
| `start` | `int64 start` | 起始值 | `0 - 1` |

**返回值**

`int64`。从 start 字节向前搜索，负起点表示从末尾开始。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.lastIndexOf(text, start);
```

**源码**：[source/std/lang.chl:47](../../source/std/lang.chl#L47)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-12"></a>

### String::contains（13）

**声明**

```chali
bool contains(String text);
```

**行为**

判断是否包含目标值或匹配文本。

是否包含指定字节序列；空搜索串总是匹配。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`bool`。判断是否包含目标值或匹配文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.contains(text);
```

**源码**：[source/std/lang.chl:50](../../source/std/lang.chl#L50)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-13"></a>

### String::startsWith（14）

**声明**

```chali
bool startsWith(String text);
```

**行为**

判断文本是否以 prefix 开始。

是否以指定文本开头；空前缀总是匹配。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`bool`。判断文本是否以 prefix 开始。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.startsWith(text);
```

**源码**：[source/std/lang.chl:53](../../source/std/lang.chl#L53)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-14"></a>

### String::endsWith（15）

**声明**

```chali
bool endsWith(String text);
```

**行为**

判断文本是否以 suffix 结束。

是否以指定文本结尾；空后缀总是匹配。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`bool`。判断文本是否以 suffix 结束。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.endsWith(text);
```

**源码**：[source/std/lang.chl:56](../../source/std/lang.chl#L56)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-15"></a>

### String::count（16）

**声明**

```chali
int64 count(String text);
```

**行为**

读取结果首行首列的整数表示。

统计不重叠匹配数；空搜索串返回字节数加一。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`int64`。读取结果首行首列的整数表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.count(text);
```

**源码**：[source/std/lang.chl:59](../../source/std/lang.chl#L59)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-16"></a>

### String::compare（17）

**声明**

```chali
int64 compare(String text);
```

**行为**

按字典序比较，返回 -1、0 或 1。

按字节字典序比较，返回 -1、0 或 1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`int64`。按字典序比较，返回 -1、0 或 1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.compare(text);
```

**源码**：[source/std/lang.chl:62](../../source/std/lang.chl#L62)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-17"></a>

### String::byteAt（18）

**声明**

```chali
Option<uint8> byteAt(int64 index);
```

**行为**

读取字节，越界返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<uint8>`。读取字节，越界返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.byteAt(index);
```

**源码**：[source/std/lang.chl:65](../../source/std/lang.chl#L65)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-18"></a>

### String::substring（19）

**声明**

```chali
String substring(int64 start, int64 count = 0 - 1);
```

**行为**

按字节起点及长度截取，越界截断，负起点按零，负长度表示至末尾。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | `0 - 1` |

**返回值**

`String`。按字节起点及长度截取，越界截断，负起点按零，负长度表示至末尾。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.substring(start, count);
```

**源码**：[source/std/lang.chl:68](../../source/std/lang.chl#L68)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-19"></a>

### String::slice（20）

**声明**

```chali
String slice(int64 start, int64 end);
```

**行为**

按半开字节范围截取；结束不大于起点时返回空串。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |
| `end` | `int64 end` | 积分终点 | 无 |

**返回值**

`String`。按半开字节范围截取；结束不大于起点时返回空串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.slice(start, end);
```

**源码**：[source/std/lang.chl:71](../../source/std/lang.chl#L71)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-20"></a>

### String::left（21）

**声明**

```chali
String left(int64 count);
```

**行为**

取前 count 字节，负数按零。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`String`。取前 count 字节，负数按零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.left(count);
```

**源码**：[source/std/lang.chl:74](../../source/std/lang.chl#L74)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-21"></a>

### String::right（22）

**声明**

```chali
String right(int64 count);
```

**行为**

取后 count 字节，负数按零，超出长度取全部。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`String`。取后 count 字节，负数按零，超出长度取全部。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.right(count);
```

**源码**：[source/std/lang.chl:77](../../source/std/lang.chl#L77)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-22"></a>

### String::trim（23）

**声明**

```chali
String trim();
```

**行为**

去掉两端 ASCII 空白。

去除两端 ASCII 空白，保留内部文本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。去掉两端 ASCII 空白。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.trim();
```

**源码**：[source/std/lang.chl:80](../../source/std/lang.chl#L80)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-23"></a>

### String::trimStart（24）

**声明**

```chali
String trimStart();
```

**行为**

去除起始 ASCII 空白。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。去除起始 ASCII 空白。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.trimStart();
```

**源码**：[source/std/lang.chl:83](../../source/std/lang.chl#L83)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-24"></a>

### String::trimEnd（25）

**声明**

```chali
String trimEnd();
```

**行为**

去除末尾 ASCII 空白。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。去除末尾 ASCII 空白。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.trimEnd();
```

**源码**：[source/std/lang.chl:86](../../source/std/lang.chl#L86)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-25"></a>

### String::toLower（26）

**声明**

```chali
String toLower();
```

**行为**

将 ASCII 大写字母转换成小写。

将 ASCII 大写字母转小写，其他字节保持原样。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。将 ASCII 大写字母转换成小写。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.toLower();
```

**源码**：[source/std/lang.chl:89](../../source/std/lang.chl#L89)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-26"></a>

### String::toUpper（27）

**声明**

```chali
String toUpper();
```

**行为**

将 ASCII 小写字母转换成大写。

将 ASCII 小写字母转大写，其他字节保持原样。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。将 ASCII 小写字母转换成大写。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.toUpper();
```

**源码**：[source/std/lang.chl:92](../../source/std/lang.chl#L92)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-27"></a>

### String::replaceFirst（28）

**声明**

```chali
String replaceFirst(String before, String after);
```

**行为**

替换首次正则匹配。

替换第一个精确匹配；空搜索串保持原值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `before` | `String before` | 待替换的精确匹配文本 | 无 |
| `after` | `String after` | 替换文本 | 无 |

**返回值**

`String`。替换首次正则匹配。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.replaceFirst(before, after);
```

**源码**：[source/std/lang.chl:95](../../source/std/lang.chl#L95)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-28"></a>

### String::replaceAll（29）

**声明**

```chali
String replaceAll(String before, String after);
```

**行为**

替换全部正则匹配。

替换全部不重叠精确匹配；空搜索串保持原值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `before` | `String before` | 待替换的精确匹配文本 | 无 |
| `after` | `String after` | 替换文本 | 无 |

**返回值**

`String`。替换全部正则匹配。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.replaceAll(before, after);
```

**源码**：[source/std/lang.chl:98](../../source/std/lang.chl#L98)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-29"></a>

### String::replace（30）

**声明**

```chali
String replace(String before, String after);
```

**行为**

将所有 from 子串替换为 to；空 from 保留副本。

替换全部不重叠匹配，与 replaceAll 一致。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `before` | `String before` | 待替换的精确匹配文本 | 无 |
| `after` | `String after` | 替换文本 | 无 |

**返回值**

`String`。将所有 from 子串替换为 to；空 from 保留副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.replace(before, after);
```

**源码**：[source/std/lang.chl:101](../../source/std/lang.chl#L101)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-30"></a>

### String::repeat（31）

**声明**

```chali
String repeat(int64 count);
```

**行为**

重复给定值或文本；文本次数非正时为空串，迭代器版本为无限源。

重复 count 次，零次返回空串，负数报错。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`String`。重复给定值或文本；文本次数非正时为空串，迭代器版本为无限源。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.repeat(count);
```

**源码**：[source/std/lang.chl:104](../../source/std/lang.chl#L104)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-31"></a>

### String::reverse（32）

**声明**

```chali
String reverse();
```

**行为**

按 Unicode 码点反转，保持每个 UTF-8 编码完整。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。按 Unicode 码点反转，保持每个 UTF-8 编码完整。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.reverse();
```

**源码**：[source/std/lang.chl:107](../../source/std/lang.chl#L107)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-32"></a>

### String::padStart（33）

**声明**

```chali
String padStart(int64 length, String fill = " ");
```

**行为**

在左侧以 fill 补到 width 字节。

按字节目标长度向左循环填充；空填充串保持原值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `length` | `int64 length` | 有效长度 | 无 |
| `fill` | `String fill` | 填充字节 | `" "` |

**返回值**

`String`。在左侧以 fill 补到 width 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.padStart(length, fill);
```

**源码**：[source/std/lang.chl:110](../../source/std/lang.chl#L110)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-33"></a>

### String::padEnd（34）

**声明**

```chali
String padEnd(int64 length, String fill = " ");
```

**行为**

在右侧以 fill 补到 width 字节。

按字节目标长度向右循环填充；空填充串保持原值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `length` | `int64 length` | 有效长度 | 无 |
| `fill` | `String fill` | 填充字节 | `" "` |

**返回值**

`String`。在右侧以 fill 补到 width 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.padEnd(length, fill);
```

**源码**：[source/std/lang.chl:113](../../source/std/lang.chl#L113)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-34"></a>

### String::removePrefix（35）

**声明**

```chali
String removePrefix(String prefix);
```

**行为**

仅去除匹配的起始前缀一次。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `prefix` | `String prefix` | 前缀 | 无 |

**返回值**

`String`。仅去除匹配的起始前缀一次。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.removePrefix(prefix);
```

**源码**：[source/std/lang.chl:116](../../source/std/lang.chl#L116)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-35"></a>

### String::removeSuffix（36）

**声明**

```chali
String removeSuffix(String suffix);
```

**行为**

仅去除匹配的末尾后缀一次。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `suffix` | `String suffix` | 后缀 | 无 |

**返回值**

`String`。仅去除匹配的末尾后缀一次。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.removeSuffix(suffix);
```

**源码**：[source/std/lang.chl:119](../../source/std/lang.chl#L119)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-36"></a>

### String::insert（37）

**声明**

```chali
String insert(int64 index, String text);
```

**行为**

在 index 之前插入 value。

在截断到有效范围的字节位置插入文本，返回新值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`String`。在 index 之前插入 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.insert(index, text);
```

**源码**：[source/std/lang.chl:122](../../source/std/lang.chl#L122)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-37"></a>

### String::remove（38）

**声明**

```chali
String remove(int64 start, int64 count = 0 - 1);
```

**行为**

删除指定位置或键的元素。

移除指定字节段；负长度表示删除余下全部。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | `0 - 1` |

**返回值**

`String`。删除指定位置或键的元素。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.remove(start, count);
```

**源码**：[source/std/lang.chl:125](../../source/std/lang.chl#L125)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-38"></a>

### String::utf8Substring（39）

**声明**

```chali
String utf8Substring(int64 start, int64 count = 0 - 1);
```

**行为**

按 Unicode 码点截取，严格验证 UTF-8，不按字素簇处理。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | `0 - 1` |

**返回值**

`String`。按 Unicode 码点截取，严格验证 UTF-8，不按字素簇处理。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.utf8Substring(start, count);
```

**源码**：[source/std/lang.chl:128](../../source/std/lang.chl#L128)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-39"></a>

### String::charAt（40）

**声明**

```chali
Option<String> charAt(int64 index);
```

**行为**

读取 index 位置的字节，需处于有效 UTF-8 边界。

读取一个完整 Unicode 码点，越界返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<String>`。读取 index 位置的字节，需处于有效 UTF-8 边界。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.charAt(index);
```

**源码**：[source/std/lang.chl:131](../../source/std/lang.chl#L131)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-40"></a>

### String::split（41）

**声明**

```chali
List<String> split(String separator, bool keepEmpty = true);
```

**行为**

按 separator 分割，空分隔符把整串作为一项。

按精确分隔文本分割，默认保留空项；空分隔符按 UTF-8 码点分割。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `separator` | `String separator` | 分隔文本 | 无 |
| `keepEmpty` | `bool keepEmpty` | 是否保留分割产生的空项 | `true` |

**返回值**

`List<String>`。按 separator 分割，空分隔符把整串作为一项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.split(separator, keepEmpty);
```

**源码**：[source/std/lang.chl:134](../../source/std/lang.chl#L134)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-41"></a>

### String::splitWhitespace（42）

**声明**

```chali
List<String> splitWhitespace();
```

**行为**

按连续 ASCII 空白分词并忽略空项。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<String>`。按连续 ASCII 空白分词并忽略空项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.splitWhitespace();
```

**源码**：[source/std/lang.chl:137](../../source/std/lang.chl#L137)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-42"></a>

### String::lines（43）

**声明**

```chali
List<String> lines(bool keepEmpty = true);
```

**行为**

按 LF、CRLF 或 CR 分行，默认保留空行和末尾空项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `keepEmpty` | `bool keepEmpty` | 是否保留分割产生的空项 | `true` |

**返回值**

`List<String>`。按 LF、CRLF 或 CR 分行，默认保留空行和末尾空项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.lines(keepEmpty);
```

**源码**：[source/std/lang.chl:140](../../source/std/lang.chl#L140)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-43"></a>

### String::unicodeChars（44）

**声明**

```chali
List<String> unicodeChars();
```

**行为**

严格按 UTF-8 Unicode 码点拆分，空串产生空列表。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<String>`。严格按 UTF-8 Unicode 码点拆分，空串产生空列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.unicodeChars();
```

**源码**：[source/std/lang.chl:143](../../source/std/lang.chl#L143)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-44"></a>

### String::bytes（45）

**声明**

```chali
List<uint8> bytes();
```

**行为**

导出独立字节列表，修改列表不影响原字符串。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<uint8>`。导出独立字节列表，修改列表不影响原字符串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.bytes();
```

**源码**：[source/std/lang.chl:146](../../source/std/lang.chl#L146)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-45"></a>

### String::join（46）

**声明**

```chali
String join(List<String> items);
```

**行为**

用 separator 连接 parts。

以当前字符串为分隔符连接值列表，空列表返回空串。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<String> items` | 参与处理的列表元素 | 无 |

**返回值**

`String`。用 separator 连接 parts。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.join(items);
```

**源码**：[source/std/lang.chl:149](../../source/std/lang.chl#L149)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-46"></a>

### String::concat（47）

**声明**

```chali
String concat(String text);
```

**行为**

返回两段文本的连接，不修改当前值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`String`。返回两段文本的连接，不修改当前值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.concat(text);
```

**源码**：[source/std/lang.chl:152](../../source/std/lang.chl#L152)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-47"></a>

### String::prepend（48）

**声明**

```chali
String prepend(String text);
```

**行为**

返回在当前值前添加文本的新字符串。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`String`。返回在当前值前添加文本的新字符串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.prepend(text);
```

**源码**：[source/std/lang.chl:155](../../source/std/lang.chl#L155)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-48"></a>

### File::print（49）

**声明**

```chali
int64 print(String text);
```

**行为**

写入文本，格式参数沿用 print 的规则。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 待写入文本，支持由格式化调用生成。 | 无 |

**返回值**

`int64`。成功写入的字节数，失败沿用文件写入诊断。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.print(text);
```

**源码**：[source/std/lang.chl:178](../../source/std/lang.chl#L178)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-49"></a>

### File::println（50）

**声明**

```chali
int64 println(String text = "");
```

**行为**

写入文本并追加一个 LF 换行。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 待写入文本，省略时只写换行。 | `""` |

**返回值**

`int64`。包括换行的写入字节数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.println(text);
```

**源码**：[source/std/lang.chl:184](../../source/std/lang.chl#L184)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-50"></a>

### readLine（51）

**声明**

```chali
String readLine();
```

**行为**

读取一行标准输入，保留空格，去掉 LF/CRLF；EOF 返回空串，空行也返回空串。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。读取的行文本；String 不允许内嵌零字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
readLine();
```

**源码**：[source/std/lang.chl:217](../../source/std/lang.chl#L217)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-51"></a>

### readLines（52）

**声明**

```chali
String readLines(int32 N);
```

**行为**

最多读取 N 行，以 LF 连接，结果不额外追加换行；EOF 提前结束。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `N` | `int32 N` | 非负行数；零返回空串且不读取输入，负数触发 panic。 | 无 |

**返回值**

`String`。拼接后的文本，空行保留；显式 --input 的每个输入项视为一行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
readLines(N);
```

**源码**：[source/std/lang.chl:221](../../source/std/lang.chl#L221)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-52"></a>

### readLines（53）

**声明**

```chali
String readLines(int64 N);
```

**行为**

接受默认整数实参的多行输入重载，检查后转为 int32 行数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `N` | `int64 N` | 0 至 2147483647 的行数；超界触发 panic。 | 无 |

**返回值**

`String`。最多 N 行，以 LF 连接，EOF 提前结束。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
readLines(N);
```

**源码**：[source/std/lang.chl:225](../../source/std/lang.chl#L225)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-53"></a>

### toInt32（54）

**声明**

```chali
Option<int32> toInt32(float64 value);
```

**行为**

向零截断为 int32；非有限数或截断后越界返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`Option<int32>`。向零截断为 int32；非有限数或截断后越界返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
toInt32(value);
```

**源码**：[source/std/lang.chl:231](../../source/std/lang.chl#L231)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-54"></a>

### toInt32（55）

**声明**

```chali
Option<int32> toInt32(float32 value);
```

**行为**

向零截断为 int32；非有限数或截断后越界返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float32 value` | 输入值或写入的新值 | 无 |

**返回值**

`Option<int32>`。向零截断为 int32；非有限数或截断后越界返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
toInt32(value);
```

**源码**：[source/std/lang.chl:237](../../source/std/lang.chl#L237)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-55"></a>

### toInt64（56）

**声明**

```chali
int64 toInt64(float64 value);
```

**行为**

向零截断为 int64，非有限数或截断后越界时报错。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。向零截断为 int64，非有限数或截断后越界时报错。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
toInt64(value);
```

**源码**：[source/std/lang.chl:239](../../source/std/lang.chl#L239)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-56"></a>

### toInt64（57）

**声明**

```chali
int64 toInt64(float32 value);
```

**行为**

向零截断为 int64，非有限数或截断后越界时报错。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float32 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。向零截断为 int64，非有限数或截断后越界时报错。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
toInt64(value);
```

**源码**：[source/std/lang.chl:241](../../source/std/lang.chl#L241)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-57"></a>

### float32ToBytes（58）

**声明**

```chali
std::lang::List<uint8> float32ToBytes(float32 value, bool bigEndian);
```

**行为**

按 IEEE 754 binary32 输出 4 个字节。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float32 value` | 输入值或写入的新值 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`std::lang::List<uint8>`。按 IEEE 754 binary32 输出 4 个字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float32ToBytes(value, bigEndian);
```

**源码**：[source/std/lang.chl:248](../../source/std/lang.chl#L248)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-58"></a>

### float32ToBytes（59）

**声明**

```chali
std::lang::List<uint8> float32ToBytes(float32 value);
```

**行为**

默认小端编码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float32 value` | 输入值或写入的新值 | 无 |

**返回值**

`std::lang::List<uint8>`。默认小端编码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float32ToBytes(value);
```

**源码**：[source/std/lang.chl:250](../../source/std/lang.chl#L250)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-59"></a>

### bytesToFloat32（60）

**声明**

```chali
Option<float32> bytesToFloat32(std::lang::List<uint8> bytes, bool bigEndian);
```

**行为**

读取列表前 4 字节，不足则返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `std::lang::List<uint8> bytes` | 输入或输出字节存储 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`Option<float32>`。读取列表前 4 字节，不足则返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat32(bytes, bigEndian);
```

**源码**：[source/std/lang.chl:252](../../source/std/lang.chl#L252)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-60"></a>

### bytesToFloat32（61）

**声明**

```chali
Option<float32> bytesToFloat32(std::lang::List<uint8> bytes);
```

**行为**

默认小端列表解码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `std::lang::List<uint8> bytes` | 输入或输出字节存储 | 无 |

**返回值**

`Option<float32>`。默认小端列表解码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat32(bytes);
```

**源码**：[source/std/lang.chl:257](../../source/std/lang.chl#L257)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-61"></a>

### bytesToFloat32（62）

**声明**

```chali
Option<float32> bytesToFloat32(uint8 const& bytes, int64 count, bool bigEndian);
```

**行为**

从固定数组或指针读取前 4 字节；调用者保证 count 为有效长度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 const& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`Option<float32>`。从固定数组或指针读取前 4 字节；调用者保证 count 为有效长度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat32(bytes, count, bigEndian);
```

**源码**：[source/std/lang.chl:259](../../source/std/lang.chl#L259)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-62"></a>

### bytesToFloat32（63）

**声明**

```chali
Option<float32> bytesToFloat32(uint8 const& bytes, int64 count);
```

**行为**

默认小端固定数组解码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 const& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`Option<float32>`。默认小端固定数组解码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat32(bytes, count);
```

**源码**：[source/std/lang.chl:264](../../source/std/lang.chl#L264)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-63"></a>

### bytesToFloat32（64）

**声明**

```chali
Option<float32> bytesToFloat32(uint8 bytes[], bool bigEndian);
```

**行为**

从运行时数组读取，自动获取长度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 bytes[]` | 输入或输出字节存储 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`Option<float32>`。从运行时数组读取，自动获取长度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat32(bytes, bigEndian);
```

**源码**：[source/std/lang.chl:266](../../source/std/lang.chl#L266)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-64"></a>

### bytesToFloat32（65）

**声明**

```chali
Option<float32> bytesToFloat32(uint8 bytes[]);
```

**行为**

默认小端运行时数组解码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 bytes[]` | 输入或输出字节存储 | 无 |

**返回值**

`Option<float32>`。默认小端运行时数组解码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat32(bytes);
```

**源码**：[source/std/lang.chl:268](../../source/std/lang.chl#L268)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-65"></a>

### float32ToBytes（66）

**声明**

```chali
bool float32ToBytes(float32 value, uint8 mut& bytes, int64 count, bool bigEndian);
```

**行为**

写入数组前 4 项；容量不足返回 false，数组保持不变。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float32 value` | 输入值或写入的新值 | 无 |
| `bytes` | `uint8 mut& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`bool`。写入数组前 4 项；容量不足返回 false，数组保持不变。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float32ToBytes(value, bytes, count, bigEndian);
```

**源码**：[source/std/lang.chl:270](../../source/std/lang.chl#L270)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-66"></a>

### float32ToBytes（67）

**声明**

```chali
bool float32ToBytes(float32 value, uint8 mut& bytes, int64 count);
```

**行为**

默认小端固定数组编码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float32 value` | 输入值或写入的新值 | 无 |
| `bytes` | `uint8 mut& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`bool`。默认小端固定数组编码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float32ToBytes(value, bytes, count);
```

**源码**：[source/std/lang.chl:275](../../source/std/lang.chl#L275)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-67"></a>

### float32ToBytes（68）

**声明**

```chali
bool float32ToBytes(float32 value, uint8[] mut& bytes, bool bigEndian);
```

**行为**

编码到运行时数组的可变引用，自动获取容量；调用时传入 &array。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float32 value` | 输入值或写入的新值 | 无 |
| `bytes` | `uint8[] mut& bytes` | 输入或输出字节存储 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`bool`。编码到运行时数组的可变引用，自动获取容量；调用时传入 &array。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float32ToBytes(value, bytes, bigEndian);
```

**源码**：[source/std/lang.chl:277](../../source/std/lang.chl#L277)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-68"></a>

### float32ToBytes（69）

**声明**

```chali
bool float32ToBytes(float32 value, uint8[] mut& bytes);
```

**行为**

默认小端运行时数组编码；调用时传入 &array。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float32 value` | 输入值或写入的新值 | 无 |
| `bytes` | `uint8[] mut& bytes` | 输入或输出字节存储 | 无 |

**返回值**

`bool`。默认小端运行时数组编码；调用时传入 &array。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float32ToBytes(value, bytes);
```

**源码**：[source/std/lang.chl:282](../../source/std/lang.chl#L282)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-69"></a>

### float64ToBytes（70）

**声明**

```chali
std::lang::List<uint8> float64ToBytes(float64 value, bool bigEndian);
```

**行为**

按 IEEE 754 binary64 输出 8 个字节。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`std::lang::List<uint8>`。按 IEEE 754 binary64 输出 8 个字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float64ToBytes(value, bigEndian);
```

**源码**：[source/std/lang.chl:289](../../source/std/lang.chl#L289)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-70"></a>

### float64ToBytes（71）

**声明**

```chali
std::lang::List<uint8> float64ToBytes(float64 value);
```

**行为**

默认小端编码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`std::lang::List<uint8>`。默认小端编码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float64ToBytes(value);
```

**源码**：[source/std/lang.chl:291](../../source/std/lang.chl#L291)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-71"></a>

### bytesToFloat64（72）

**声明**

```chali
Option<float64> bytesToFloat64(std::lang::List<uint8> bytes, bool bigEndian);
```

**行为**

读取列表前 8 字节，不足则返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `std::lang::List<uint8> bytes` | 输入或输出字节存储 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`Option<float64>`。读取列表前 8 字节，不足则返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat64(bytes, bigEndian);
```

**源码**：[source/std/lang.chl:293](../../source/std/lang.chl#L293)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-72"></a>

### bytesToFloat64（73）

**声明**

```chali
Option<float64> bytesToFloat64(std::lang::List<uint8> bytes);
```

**行为**

默认小端列表解码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `std::lang::List<uint8> bytes` | 输入或输出字节存储 | 无 |

**返回值**

`Option<float64>`。默认小端列表解码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat64(bytes);
```

**源码**：[source/std/lang.chl:298](../../source/std/lang.chl#L298)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-73"></a>

### bytesToFloat64（74）

**声明**

```chali
Option<float64> bytesToFloat64(uint8 const& bytes, int64 count, bool bigEndian);
```

**行为**

从固定数组或指针读取前 8 字节；调用者保证 count 为有效长度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 const& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`Option<float64>`。从固定数组或指针读取前 8 字节；调用者保证 count 为有效长度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat64(bytes, count, bigEndian);
```

**源码**：[source/std/lang.chl:300](../../source/std/lang.chl#L300)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-74"></a>

### bytesToFloat64（75）

**声明**

```chali
Option<float64> bytesToFloat64(uint8 const& bytes, int64 count);
```

**行为**

默认小端固定数组解码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 const& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`Option<float64>`。默认小端固定数组解码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat64(bytes, count);
```

**源码**：[source/std/lang.chl:305](../../source/std/lang.chl#L305)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-75"></a>

### bytesToFloat64（76）

**声明**

```chali
Option<float64> bytesToFloat64(uint8 bytes[], bool bigEndian);
```

**行为**

从运行时数组读取，自动获取长度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 bytes[]` | 输入或输出字节存储 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`Option<float64>`。从运行时数组读取，自动获取长度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat64(bytes, bigEndian);
```

**源码**：[source/std/lang.chl:307](../../source/std/lang.chl#L307)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-76"></a>

### bytesToFloat64（77）

**声明**

```chali
Option<float64> bytesToFloat64(uint8 bytes[]);
```

**行为**

默认小端运行时数组解码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 bytes[]` | 输入或输出字节存储 | 无 |

**返回值**

`Option<float64>`。默认小端运行时数组解码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
bytesToFloat64(bytes);
```

**源码**：[source/std/lang.chl:309](../../source/std/lang.chl#L309)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-77"></a>

### float64ToBytes（78）

**声明**

```chali
bool float64ToBytes(float64 value, uint8 mut& bytes, int64 count, bool bigEndian);
```

**行为**

写入数组前 8 项；容量不足返回 false，数组保持不变。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |
| `bytes` | `uint8 mut& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`bool`。写入数组前 8 项；容量不足返回 false，数组保持不变。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float64ToBytes(value, bytes, count, bigEndian);
```

**源码**：[source/std/lang.chl:311](../../source/std/lang.chl#L311)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-78"></a>

### float64ToBytes（79）

**声明**

```chali
bool float64ToBytes(float64 value, uint8 mut& bytes, int64 count);
```

**行为**

默认小端固定数组编码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |
| `bytes` | `uint8 mut& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`bool`。默认小端固定数组编码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float64ToBytes(value, bytes, count);
```

**源码**：[source/std/lang.chl:316](../../source/std/lang.chl#L316)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-79"></a>

### float64ToBytes（80）

**声明**

```chali
bool float64ToBytes(float64 value, uint8[] mut& bytes, bool bigEndian);
```

**行为**

编码到运行时数组的可变引用，自动获取容量；调用时传入 &array。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |
| `bytes` | `uint8[] mut& bytes` | 输入或输出字节存储 | 无 |
| `bigEndian` | `bool bigEndian` | true 为大端，false 为小端 | 无 |

**返回值**

`bool`。编码到运行时数组的可变引用，自动获取容量；调用时传入 &array。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float64ToBytes(value, bytes, bigEndian);
```

**源码**：[source/std/lang.chl:318](../../source/std/lang.chl#L318)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-80"></a>

### float64ToBytes（81）

**声明**

```chali
bool float64ToBytes(float64 value, uint8[] mut& bytes);
```

**行为**

默认小端运行时数组编码；调用时传入 &array。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |
| `bytes` | `uint8[] mut& bytes` | 输入或输出字节存储 | 无 |

**返回值**

`bool`。默认小端运行时数组编码；调用时传入 &array。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
float64ToBytes(value, bytes);
```

**源码**：[source/std/lang.chl:323](../../source/std/lang.chl#L323)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-81"></a>

### assert（82）

**声明**

```chali
void assert(bool condition);
```

**行为**

condition 为真时继续，为假时以指定诊断文本和可选行号触发 panic。

中止统一走 std::lang::panic：编译器与虚拟机里只有 `__std_panic` 这一个停机原语， `exit(n)` 与内建 `assert(x)` 都已删除。需要断言时用下面这几个函数 —— 它们自己 调 `std::lang::panic`，所以停机清理只有一条入口；带消息的重载让标准库自己的前置检查 也能给出人话（例如 range 的步长）。 带 `int64 line` 的重载给文件末尾的系统宏 `$assert(...)` 用：宏把调用点的 `$line` 传进来，失败时就地报出**源码行号**，不必每处自己写文案。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `condition` | `bool condition` | 待验证的布尔条件 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
assert(condition);
```

**源码**：[source/std/lang.chl:336](../../source/std/lang.chl#L336)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-82"></a>

### assert（83）

**声明**

```chali
void assert(bool condition, std::lang::String message);
```

**行为**

condition 为真时继续，为假时以指定诊断文本和可选行号触发 panic。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `condition` | `bool condition` | 待验证的布尔条件 | 无 |
| `message` | `std::lang::String message` | 诊断文本或消息载荷 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
assert(condition, message);
```

**源码**：[source/std/lang.chl:340](../../source/std/lang.chl#L340)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-83"></a>

### assert（84）

**声明**

```chali
void assert(bool condition, int64 line);
```

**行为**

condition 为真时继续，为假时以指定诊断文本和可选行号触发 panic。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `condition` | `bool condition` | 待验证的布尔条件 | 无 |
| `line` | `int64 line` | 用于诊断的源码行号 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
assert(condition, line);
```

**源码**：[source/std/lang.chl:344](../../source/std/lang.chl#L344)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-84"></a>

### assert（85）

**声明**

```chali
void assert(bool condition, std::lang::String message, int64 line);
```

**行为**

condition 为真时继续，为假时以指定诊断文本和可选行号触发 panic。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `condition` | `bool condition` | 待验证的布尔条件 | 无 |
| `message` | `std::lang::String message` | 诊断文本或消息载荷 | 无 |
| `line` | `int64 line` | 用于诊断的源码行号 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
assert(condition, message, line);
```

**源码**：[source/std/lang.chl:348](../../source/std/lang.chl#L348)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-85"></a>

### Try::branch（86）

**声明**

```chali
TryBranch<T, R> branch();
```

**行为**

只读拆分传播值，Continue 产生成功载荷，Break 携带失败载荷。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TryBranch<T, R>`。只读拆分传播值，Continue 产生成功载荷，Break 携带失败载荷。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.branch();
```

**源码**：[source/std/lang.chl:359](../../source/std/lang.chl#L359)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-86"></a>

### TryMove::intoBranch（87）

**声明**

```chali
TryBranch<T, R> intoBranch();
```

**行为**

在移动拥有值后拆分传播值，结果遵守成功与失败载荷的所有权规则。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TryBranch<T, R>`。在移动拥有值后拆分传播值，结果遵守成功与失败载荷的所有权规则。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.intoBranch();
```

**源码**：[source/std/lang.chl:361](../../source/std/lang.chl#L361)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-87"></a>

### FromResidual::fromResidual（88）

**声明**

```chali
static F fromResidual(R residual);
```

**行为**

静态地将匹配的传播载荷构造为当前函数的返回类型。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `residual` | `R residual` | 向当前函数返回类型传递的失败载荷。 | 无 |

**返回值**

`F`。静态地将匹配的传播载荷构造为当前函数的返回类型。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
FromResidual<F, R>::fromResidual(residual);
```

**源码**：[source/std/lang.chl:363](../../source/std/lang.chl#L363)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-88"></a>

### Option::branch（89）

**声明**

```chali
TryBranch<T, NoneResidual> branch();
```

**行为**

只读拆分传播值，Continue 产生成功载荷，Break 携带失败载荷。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TryBranch<T, NoneResidual>`。只读拆分传播值，Continue 产生成功载荷，Break 携带失败载荷。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.branch();
```

**源码**：[source/std/lang.chl:368](../../source/std/lang.chl#L368)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-89"></a>

### Option::intoBranch（90）

**声明**

```chali
TryBranch<T, NoneResidual> intoBranch();
```

**行为**

在移动拥有值后拆分传播值，结果遵守成功与失败载荷的所有权规则。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TryBranch<T, NoneResidual>`。在移动拥有值后拆分传播值，结果遵守成功与失败载荷的所有权规则。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.intoBranch();
```

**源码**：[source/std/lang.chl:374](../../source/std/lang.chl#L374)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-90"></a>

### Option::fromResidual（91）

**声明**

```chali
static Option<T> fromResidual(NoneResidual residual);
```

**行为**

静态地将匹配的传播载荷构造为当前函数的返回类型。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `residual` | `NoneResidual residual` | 向当前函数返回类型传递的失败载荷。 | 无 |

**返回值**

`Option<T>`。静态地将匹配的传播载荷构造为当前函数的返回类型。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
Option<T>::fromResidual(residual);
```

**源码**：[source/std/lang.chl:378](../../source/std/lang.chl#L378)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-91"></a>

### Option::isValue（92）

**声明**

```chali
bool isValue();
```

**行为**

判断 Option 是否含 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 Option 是否含 Value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isValue();
```

**源码**：[source/std/lang.chl:379](../../source/std/lang.chl#L379)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-92"></a>

### Option::isNone（93）

**声明**

```chali
bool isNone();
```

**行为**

判断 Option 是否为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 Option 是否为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isNone();
```

**源码**：[source/std/lang.chl:388](../../source/std/lang.chl#L388)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-93"></a>

### Option::take（94）

**声明**

```chali
T take();
```

**行为**

转移整个结果并取出成功载荷；源结果随后不可使用。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。转移整个结果并取出成功载荷；源结果随后不可使用。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.take();
```

**源码**：[source/std/lang.chl:391](../../source/std/lang.chl#L391)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-94"></a>

### Option::unwrap（95）

**声明**

```chali
T unwrap();
```

**行为**

提取成功载荷；None/Err 时 panic。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。提取成功载荷；None/Err 时 panic。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.unwrap();
```

**源码**：[source/std/lang.chl:401](../../source/std/lang.chl#L401)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-95"></a>

### Option::unwrapOr（96）

**声明**

```chali
T unwrapOr(T fallback);
```

**行为**

提取成功载荷；None/Err 时返回 fallback。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `fallback` | `T fallback` | 失败/空值分支的替代值 | 无 |

**返回值**

`T`。提取成功载荷；None/Err 时返回 fallback。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.unwrapOr(fallback);
```

**源码**：[source/std/lang.chl:410](../../source/std/lang.chl#L410)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-96"></a>

### Option::expect（97）

**声明**

```chali
T expect(std::lang::String message);
```

**行为**

提取成功载荷；None/Err 时以 message 触发 panic。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `message` | `std::lang::String message` | 诊断文本或消息载荷 | 无 |

**返回值**

`T`。提取成功载荷；None/Err 时以 message 触发 panic。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.expect(message);
```

**源码**：[source/std/lang.chl:419](../../source/std/lang.chl#L419)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-97"></a>

### Option::transform（98）

**声明**

```chali
Option<T> transform(T fn(T) mapper);
```

**行为**

成功时调用 mapper 转换载荷，失败分支保持不变。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `mapper` | `T fn(T) mapper` | 转换回调，签名决定输入和输出类型 | 无 |

**返回值**

`Option<T>`。成功时调用 mapper 转换载荷，失败分支保持不变。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.transform(mapper);
```

**源码**：[source/std/lang.chl:428](../../source/std/lang.chl#L428)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-98"></a>

### Option::andThen（99）

**声明**

```chali
Option<T> andThen(Option<T> fn(T) mapper);
```

**行为**

成功时调用返回同族 Option/Result 的 mapper，将结果展平。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `mapper` | `Option<T> fn(T) mapper` | 转换回调，签名决定输入和输出类型 | 无 |

**返回值**

`Option<T>`。成功时调用返回同族 Option/Result 的 mapper，将结果展平。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.andThen(mapper);
```

**源码**：[source/std/lang.chl:437](../../source/std/lang.chl#L437)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-99"></a>

### Result::branch（100）

**声明**

```chali
TryBranch<T, ErrorResidual<E>> branch();
```

**行为**

只读拆分传播值，Continue 产生成功载荷，Break 携带失败载荷。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TryBranch<T, ErrorResidual<E>>`。只读拆分传播值，Continue 产生成功载荷，Break 携带失败载荷。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.branch();
```

**源码**：[source/std/lang.chl:458](../../source/std/lang.chl#L458)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-100"></a>

### Result::intoBranch（101）

**声明**

```chali
TryBranch<T, ErrorResidual<E>> intoBranch();
```

**行为**

在移动拥有值后拆分传播值，结果遵守成功与失败载荷的所有权规则。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TryBranch<T, ErrorResidual<E>>`。在移动拥有值后拆分传播值，结果遵守成功与失败载荷的所有权规则。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.intoBranch();
```

**源码**：[source/std/lang.chl:468](../../source/std/lang.chl#L468)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-101"></a>

### Result::fromResidual（102）

**声明**

```chali
static Result<T,E> fromResidual(ErrorResidual<E> residual);
```

**行为**

静态地将匹配的传播载荷构造为当前函数的返回类型。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `residual` | `ErrorResidual<E> residual` | 向当前函数返回类型传递的失败载荷。 | 无 |

**返回值**

`Result<T,E>`。静态地将匹配的传播载荷构造为当前函数的返回类型。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
Result<T, E>::fromResidual(residual);
```

**源码**：[source/std/lang.chl:474](../../source/std/lang.chl#L474)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-102"></a>

### Result::ok（103）

**声明**

```chali
Option<T> ok();
```

**行为**

成功时保留值，失败时明确丢弃错误信息并返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。成功时保留值，失败时明确丢弃错误信息并返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.ok();
```

**源码**：[source/std/lang.chl:475](../../source/std/lang.chl#L475)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-103"></a>

### Result::isOk（104）

**声明**

```chali
bool isOk();
```

**行为**

判断 Result 是否为 Ok。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 Result 是否为 Ok。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isOk();
```

**源码**：[source/std/lang.chl:481](../../source/std/lang.chl#L481)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-104"></a>

### Result::isErr（105）

**声明**

```chali
bool isErr();
```

**行为**

判断 Result 是否为 Err。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 Result 是否为 Err。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isErr();
```

**源码**：[source/std/lang.chl:490](../../source/std/lang.chl#L490)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-105"></a>

### Result::take（106）

**声明**

```chali
T take();
```

**行为**

转移整个结果并取出成功载荷；源结果随后不可使用。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。转移整个结果并取出成功载荷；源结果随后不可使用。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.take();
```

**源码**：[source/std/lang.chl:493](../../source/std/lang.chl#L493)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-106"></a>

### Result::unwrap（107）

**声明**

```chali
T unwrap();
```

**行为**

提取成功载荷；None/Err 时 panic。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。提取成功载荷；None/Err 时 panic。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.unwrap();
```

**源码**：[source/std/lang.chl:503](../../source/std/lang.chl#L503)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-107"></a>

### Result::unwrapOr（108）

**声明**

```chali
T unwrapOr(T fallback);
```

**行为**

提取成功载荷；None/Err 时返回 fallback。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `fallback` | `T fallback` | 失败/空值分支的替代值 | 无 |

**返回值**

`T`。提取成功载荷；None/Err 时返回 fallback。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.unwrapOr(fallback);
```

**源码**：[source/std/lang.chl:512](../../source/std/lang.chl#L512)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-108"></a>

### Result::expect（109）

**声明**

```chali
T expect(std::lang::String message);
```

**行为**

提取成功载荷；None/Err 时以 message 触发 panic。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `message` | `std::lang::String message` | 诊断文本或消息载荷 | 无 |

**返回值**

`T`。提取成功载荷；None/Err 时以 message 触发 panic。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.expect(message);
```

**源码**：[source/std/lang.chl:521](../../source/std/lang.chl#L521)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-109"></a>

### Result::takeError（110）

**声明**

```chali
E takeError();
```

**行为**

转移整个结果并取出错误载荷；源结果随后不可使用。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`E`。转移整个结果并取出错误载荷；源结果随后不可使用。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.takeError();
```

**源码**：[source/std/lang.chl:531](../../source/std/lang.chl#L531)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-110"></a>

### Result::error（111）

**声明**

```chali
E error();
```

**行为**

提取 Err 载荷；Ok 时 panic。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`E`。提取 Err 载荷；Ok 时 panic。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.error();
```

**源码**：[source/std/lang.chl:541](../../source/std/lang.chl#L541)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-111"></a>

### Result::transform（112）

**声明**

```chali
Result<T, E> transform(T fn(T) mapper);
```

**行为**

成功时调用 mapper 转换载荷，失败分支保持不变。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `mapper` | `T fn(T) mapper` | 转换回调，签名决定输入和输出类型 | 无 |

**返回值**

`Result<T, E>`。成功时调用 mapper 转换载荷，失败分支保持不变。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.transform(mapper);
```

**源码**：[source/std/lang.chl:550](../../source/std/lang.chl#L550)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-112"></a>

### Result::andThen（113）

**声明**

```chali
Result<T, E> andThen(Result<T, E> fn(T) mapper);
```

**行为**

成功时调用返回同族 Option/Result 的 mapper，将结果展平。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `mapper` | `Result<T, E> fn(T) mapper` | 转换回调，签名决定输入和输出类型 | 无 |

**返回值**

`Result<T, E>`。成功时调用返回同族 Option/Result 的 mapper，将结果展平。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.andThen(mapper);
```

**源码**：[source/std/lang.chl:559](../../source/std/lang.chl#L559)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-113"></a>

### NextValue::nextValue（114）

**声明**

```chali
Option<T> nextValue(T end, bool inclusive);
```

**行为**

使用接收者的游标状态、end 与 inclusive 产生下一项，并推进游标；返回 None 结束区间。

取得下一项，由实现判断边界并维护游标状态。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `end` | `T end` | 区间终点。 | 无 |
| `inclusive` | `bool inclusive` | 是否包含终点。 | 无 |

**返回值**

`Option<T>`。下一项，或表示遍历结束的 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.nextValue(end, inclusive);
```

**源码**：[source/std/lang.chl:580](../../source/std/lang.chl#L580)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-114"></a>

### Pair::create（115）

**声明**

```chali
static Pair<K, V> create(K first, V second);
```

**行为**

创建Pair，返回可供后续操作的值。

从两个值复制构造键值对。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `first` | `K first` | 键或第一项。 | 无 |
| `second` | `V second` | 值或第二项。 | 无 |

**返回值**

`Pair<K, V>`。拥有独立字段值的 Pair。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
Pair<K, V>::create(first, second);
```

**源码**：[source/std/lang.chl:636](../../source/std/lang.chl#L636)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-115"></a>

### Pair::create（116）

**声明**

```chali
static Pair<K, V> create((K, V) value);
```

**行为**

创建Pair，返回可供后续操作的值。

从二元组复制构造键值对。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `(K, V) value` | 提供 first 与 second 的两个值。 | 无 |

**返回值**

`Pair<K, V>`。拥有独立字段值的 Pair。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
Pair<K, V>::create(value);
```

**源码**：[source/std/lang.chl:646](../../source/std/lang.chl#L646)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-116"></a>

### Pair::toTuple（117）

**声明**

```chali
(K, V) toTuple();
```

**行为**

将键值对的两个字段复制为二元组。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(K, V)`。按 first、second 顺序保存独立值的二元组。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.toTuple();
```

**源码**：[source/std/lang.chl:655](../../source/std/lang.chl#L655)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-117"></a>

### tryGet（118）

**声明**

```chali
Option<T> tryGet<T>(Any const& value);
```

**行为**

类型匹配时复制拥有值；不匹配或空盒子返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `Any const& value` | 输入值或写入的新值 | 无 |

**返回值**

`Option<T>`。类型匹配时复制拥有值；不匹配或空盒子返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
tryGet<T>(value);
```

**源码**：[source/std/lang.chl:679](../../source/std/lang.chl#L679)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-118"></a>

### tryGetReference（119）

**声明**

```chali
Option<T const&> tryGetReference<T>(Any const& value);
```

**行为**

安全借用只读值或兼容接口，引用不得超过盒子的寿命。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `Any const& value` | 输入值或写入的新值 | 无 |

**返回值**

`Option<T const&>`。安全借用只读值或兼容接口，引用不得超过盒子的寿命。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
tryGetReference<T>(value);
```

**源码**：[source/std/lang.chl:685](../../source/std/lang.chl#L685)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-119"></a>

### tryGetMut（120）

**声明**

```chali
Option<T mut&> tryGetMut<T>(Any mut& value);
```

**行为**

安全借用可写值或兼容接口，要求盒子可写。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `Any mut& value` | 输入值或写入的新值 | 无 |

**返回值**

`Option<T mut&>`。安全借用可写值或兼容接口，要求盒子可写。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
tryGetMut<T>(value);
```

**源码**：[source/std/lang.chl:690](../../source/std/lang.chl#L690)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-120"></a>

### stringBytes（121）

**声明**

```chali
List<uint8> stringBytes(String text);
```

**行为**

返回字符串的完整 UTF-8 字节副本，不执行字符边界索引。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`List<uint8>`。返回字符串的完整 UTF-8 字节副本，不执行字符边界索引。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
stringBytes(text);
```

**源码**：[source/std/lang.chl:697](../../source/std/lang.chl#L697)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-121"></a>

### range（122）

**声明**

```chali
__Range range(int64 start, int64 stop);
```

**行为**

创建半开整数区间；正步长小于 stop，负步长大于 stop，步长不能为零。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |
| `stop` | `int64 stop` | 不包含的终止值 | 无 |

**返回值**

`__Range`。创建半开整数区间；正步长小于 stop，负步长大于 stop，步长不能为零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
range(start, stop);
```

**源码**：[source/chali_generic.cpp:1839](../../source/chali_generic.cpp#L1839)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-122"></a>

### range（123）

**声明**

```chali
__Range range(int64 start, int64 stop, int64 step);
```

**行为**

创建半开整数区间；正步长小于 stop，负步长大于 stop，步长不能为零。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |
| `stop` | `int64 stop` | 不包含的终止值 | 无 |
| `step` | `int64 step` | 步长或时间增量 | 无 |

**返回值**

`__Range`。创建半开整数区间；正步长小于 stop，负步长大于 stop，步长不能为零。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`step != 0, "range 的步长不能为 0"`。

**调用形式**

```chali
range(start, stop, step);
```

**源码**：[source/chali_generic.cpp:1847](../../source/chali_generic.cpp#L1847)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-123"></a>

### rangeEq（124）

**声明**

```chali
__Interval_int64 rangeEq(int64 start, int64 stop);
```

**行为**

构造包含起点和终点的值区间；整数和 char 直接计算，其他类型通过 NextValue<T> 迭代。match 区间模式仅支持整数和 char。

构造包含起点和终点的整数区间。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 区间起点。 | 无 |
| `stop` | `int64 stop` | 包含在区间内的终点。 | 无 |

**返回值**

`__Interval_int64`。保存独立游标的闭区间值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
rangeEq(start, stop);
```

**源码**：[source/chali_generic.cpp:1862](../../source/chali_generic.cpp#L1862)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-124"></a>

### Drop::drop（125）

**声明**

```chali
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.drop();
```

**源码**：[source/chali_generic.cpp:2069](../../source/chali_generic.cpp#L2069)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-125"></a>

### Closeable::close（126）

**声明**

```chali
void close();
```

**行为**

关闭底层资源；文档节点会使同文档所有视图失效。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.close();
```

**源码**：[source/chali_generic.cpp:2077](../../source/chali_generic.cpp#L2077)。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-126"></a>

### print（127）

**声明**

```chali
int64 print(String format, Any args...);
```

**行为**

输出格式化文本，返回过程状态；占位符和变长实参由编译器检查。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `format` | `String format` | 格式模板，使用 {} 占位符 | 无 |
| `args` | `Any args...` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`int64`。输出格式化文本，返回过程状态；占位符和变长实参由编译器检查。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
print(format, args);
```

**源码**：[source/semantics/boxing.cpp:1281](../../source/semantics/boxing.cpp#L1281)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-127"></a>

### println（128）

**声明**

```chali
int64 println(String format, Any args...);
```

**行为**

输出格式化文本并换行。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `format` | `String format` | 格式模板，使用 {} 占位符 | 无 |
| `args` | `Any args...` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`int64`。输出格式化文本并换行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
println(format, args);
```

**源码**：[source/semantics/boxing.cpp:1281](../../source/semantics/boxing.cpp#L1281)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-128"></a>

### eprint（129）

**声明**

```chali
int64 eprint(String format, Any args...);
```

**行为**

格式化写入标准错误流，不追加换行。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `format` | `String format` | 格式模板，使用 {} 占位符 | 无 |
| `args` | `Any args...` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`int64`。格式化写入标准错误流，不追加换行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
eprint(format, args);
```

**源码**：[source/semantics/formatted_output.cpp:15](../../source/semantics/formatted_output.cpp#L15)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-129"></a>

### eprintln（130）

**声明**

```chali
int64 eprintln(String format, Any args...);
```

**行为**

格式化写入标准错误流并追加换行。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `format` | `String format` | 格式模板，使用 {} 占位符 | 无 |
| `args` | `Any args...` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`int64`。格式化写入标准错误流并追加换行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
eprintln(format, args);
```

**源码**：[source/semantics/formatted_output.cpp:15](../../source/semantics/formatted_output.cpp#L15)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-130"></a>

### format（131）

**声明**

```chali
String format(String format, Any args...);
```

**行为**

返回格式化的新字符串。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `format` | `String format` | 格式模板，使用 {} 占位符 | 无 |
| `args` | `Any args...` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`String`。返回格式化的新字符串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
format(format, args);
```

**源码**：[source/semantics/boxing.cpp:1285](../../source/semantics/boxing.cpp#L1285)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-131"></a>

### putChar（132）

**声明**

```chali
int64 putChar(char value);
```

**行为**

输出一个字符。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `char value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。输出一个字符。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
putChar(value);
```

**源码**：[source/semantics/boxing.cpp:1331](../../source/semantics/boxing.cpp#L1331)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-132"></a>

### pause（133）

**声明**

```chali
int64 pause();
```

**行为**

调试暂停，提示写入标准错误；非交互输入立即返回。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。调试暂停，提示写入标准错误；非交互输入立即返回。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
pause();
```

**源码**：[source/semantics/boxing.cpp:1292](../../source/semantics/boxing.cpp#L1292)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-133"></a>

### inputLine（134）

**声明**

```chali
String inputLine();
```

**行为**

读取下一项原文，输入项按空白分隔，不是整行。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。读取下一项原文，输入项按空白分隔，不是整行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
inputLine();
```

**源码**：[source/semantics/boxing.cpp:1341](../../source/semantics/boxing.cpp#L1341)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-134"></a>

### inputInt（135）

**声明**

```chali
int64 inputInt();
```

**行为**

读取下一项有符号整数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。读取下一项有符号整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
inputInt();
```

**源码**：[source/semantics/boxing.cpp:1341](../../source/semantics/boxing.cpp#L1341)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-135"></a>

### inputUInt（136）

**声明**

```chali
uint64 inputUInt();
```

**行为**

读取下一项无符号整数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint64`。读取下一项无符号整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
inputUInt();
```

**源码**：[source/semantics/boxing.cpp:1341](../../source/semantics/boxing.cpp#L1341)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-136"></a>

### inputFloat（137）

**声明**

```chali
float64 inputFloat();
```

**行为**

读取下一项浮点数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`float64`。读取下一项浮点数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
inputFloat();
```

**源码**：[source/semantics/boxing.cpp:1341](../../source/semantics/boxing.cpp#L1341)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-137"></a>

### inputBool（138）

**声明**

```chali
bool inputBool();
```

**行为**

读取下一项布尔值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。读取下一项布尔值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
inputBool();
```

**源码**：[source/semantics/boxing.cpp:1341](../../source/semantics/boxing.cpp#L1341)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-138"></a>

### inputChar（139）

**声明**

```chali
char inputChar();
```

**行为**

读取下一项字符。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`char`。读取下一项字符。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
inputChar();
```

**源码**：[source/semantics/boxing.cpp:1341](../../source/semantics/boxing.cpp#L1341)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-139"></a>

### inputBigInteger（140）

**声明**

```chali
BigInteger inputBigInteger();
```

**行为**

读取下一项任意精度整数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigInteger`。读取下一项任意精度整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
inputBigInteger();
```

**源码**：[source/semantics/boxing.cpp:1341](../../source/semantics/boxing.cpp#L1341)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-140"></a>

### inputBigDecimal（141）

**声明**

```chali
BigDecimal inputBigDecimal();
```

**行为**

读取下一项任意精度小数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigDecimal`。读取下一项任意精度小数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
inputBigDecimal();
```

**源码**：[source/semantics/boxing.cpp:1341](../../source/semantics/boxing.cpp#L1341)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-141"></a>

### panic（142）

**声明**

```chali
int64 panic(String message);
```

**行为**

终止当前执行并报告 message。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `message` | `String message` | 诊断文本或消息载荷 | 无 |

**返回值**

`int64`。终止当前执行并报告 message。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
panic(message);
```

**源码**：[source/semantics/boxing.cpp:1360](../../source/semantics/boxing.cpp#L1360)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-142"></a>

### len（143）

**声明**

```chali
int64 len(Any value);
```

**行为**

返回当前元素数量。

返回字符串字节长度、数组元素数量或容器长度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `Any value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
len(value);
```

**源码**：[source/semantics/boxing.cpp:711](../../source/semantics/boxing.cpp#L711)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-143"></a>

### move（144）

**声明**

```chali
T move<T>(T mut& value);
```

**行为**

转移 value 的资源所有权并清空源槽，不复制资源、不执行源值析构；目标必须是有效可写引用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T mut& value` | 输入值或写入的新值 | 无 |

**返回值**

`T`。转移 value 的资源所有权并清空源槽，不复制资源、不执行源值析构；目标必须是有效可写引用。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
move<T>(value);
```

**源码**：[source/semantics/boxing.cpp:635](../../source/semantics/boxing.cpp#L635)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-144"></a>

### toReference（145）

**声明**

```chali
T mut& toReference<T>(Option<T mut&> value);
```

**行为**

将可选可写引用转换成可写引用；None 无法转为有效引用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `Option<T mut&> value` | 输入值或写入的新值 | 无 |

**返回值**

`T mut&`。将可选可写引用转换成可写引用；None 无法转为有效引用。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
toReference<T>(value);
```

**源码**：[source/chali_generic.cpp:1776](../../source/chali_generic.cpp#L1776)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-145"></a>

### toOptional（146）

**声明**

```chali
Option<T mut&> toOptional<T>(T mut& value);
```

**行为**

将可写引用封装成可选引用，不复制指向的对象。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T mut& value` | 输入值或写入的新值 | 无 |

**返回值**

`Option<T mut&>`。将可写引用封装成可选引用，不复制指向的对象。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
toOptional<T>(value);
```

**源码**：[source/chali_generic.cpp:1782](../../source/chali_generic.cpp#L1782)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-146"></a>

### memcpy（147）

**声明**

```chali
int64 memcpy(char mut& destination, char const& source, int64 bytes);
```

**行为**

按字节复制内存；调用者保证源与目标存储范围有效。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `destination` | `char mut& destination` | 可写目标存储首地址 | 无 |
| `source` | `char const& source` | 输入游标、存储或事件来源值 | 无 |
| `bytes` | `int64 bytes` | 输入或输出字节存储 | 无 |

**返回值**

`int64`。按字节复制内存；调用者保证源与目标存储范围有效。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
memcpy(destination, source, bytes);
```

**源码**：[source/semantics/boxing.cpp:1377](../../source/semantics/boxing.cpp#L1377)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-147"></a>

### memset（148）

**声明**

```chali
int64 memset(char mut& destination, int64 value, int64 bytes);
```

**行为**

按字节填充存储；调用者保证目标范围有效。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `destination` | `char mut& destination` | 可写目标存储首地址 | 无 |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |
| `bytes` | `int64 bytes` | 输入或输出字节存储 | 无 |

**返回值**

`int64`。按字节填充存储；调用者保证目标范围有效。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
memset(destination, value, bytes);
```

**源码**：[source/semantics/boxing.cpp:1377](../../source/semantics/boxing.cpp#L1377)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-148"></a>

### List::copy（149）

**声明**

```chali
List<T> copy();
```

**行为**

返回独立的值副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<T>`。返回独立的值副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.copy();
```

**源码**：[source/semantics/aggregate_ownership.cpp:256](../../source/semantics/aggregate_ownership.cpp#L256)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-149"></a>

### List::push（150）

**声明**

```chali
int64 push(T value);
```

**行为**

将 value 追加到列表尾部。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.push(value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:257](../../source/semantics/aggregate_ownership.cpp#L257)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-150"></a>

### List::insert（151）

**声明**

```chali
int64 insert(int64 index, T value);
```

**行为**

在 index 之前插入 value。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.insert(index, value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:258](../../source/semantics/aggregate_ownership.cpp#L258)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-151"></a>

### List::at（152）

**声明**

```chali
T at(int64 index);
```

**行为**

读取从 0 开始的指定位置。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`T`。读取从 0 开始的指定位置。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.at(index);
```

**源码**：[source/semantics/aggregate_ownership.cpp:259](../../source/semantics/aggregate_ownership.cpp#L259)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-152"></a>

### List::pop（153）

**声明**

```chali
T pop();
```

**行为**

移除列表尾元素并返回值，空列表时报错。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。移除列表尾元素并返回值，空列表时报错。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.pop();
```

**源码**：[source/semantics/aggregate_ownership.cpp:260](../../source/semantics/aggregate_ownership.cpp#L260)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-153"></a>

### List::remove（154）

**声明**

```chali
int64 remove(int64 index);
```

**行为**

删除指定位置或键的元素。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`int64`。成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.remove(index);
```

**源码**：[source/semantics/aggregate_ownership.cpp:261](../../source/semantics/aggregate_ownership.cpp#L261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-154"></a>

### List::indexOf（155）

**声明**

```chali
int64 indexOf(T value);
```

**行为**

返回首次出现的下标；未找到返回 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。返回首次出现的下标；未找到返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.indexOf(value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:262](../../source/semantics/aggregate_ownership.cpp#L262)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-155"></a>

### List::contains（156）

**声明**

```chali
bool contains(T value);
```

**行为**

判断是否包含目标值或匹配文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。判断是否包含目标值或匹配文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.contains(value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:263](../../source/semantics/aggregate_ownership.cpp#L263)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-156"></a>

### List::has（157）

**声明**

```chali
bool has(T value);
```

**行为**

检查是否包含指定值或键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。检查是否包含指定值或键。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.has(value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:264](../../source/semantics/aggregate_ownership.cpp#L264)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-157"></a>

### List::clear（158）

**声明**

```chali
int64 clear();
```

**行为**

移除全部元素。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.clear();
```

**源码**：[source/semantics/aggregate_ownership.cpp:265](../../source/semantics/aggregate_ownership.cpp#L265)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-158"></a>

### List::len（159）

**声明**

```chali
int64 len();
```

**行为**

返回当前元素数量。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.len();
```

**源码**：[source/semantics/aggregate_ownership.cpp:266](../../source/semantics/aggregate_ownership.cpp#L266)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-159"></a>

### Set::copy（160）

**声明**

```chali
Set<T> copy();
```

**行为**

返回独立的值副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Set<T>`。返回独立的值副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.copy();
```

**源码**：[source/semantics/aggregate_ownership.cpp:256](../../source/semantics/aggregate_ownership.cpp#L256)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-160"></a>

### Set::add（161）

**声明**

```chali
int64 add(T value);
```

**行为**

计算逐项相加或追加元素，具体操作取决于所属类型。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.add(value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:274](../../source/semantics/aggregate_ownership.cpp#L274)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-161"></a>

### Set::has（162）

**声明**

```chali
int64 has(T value);
```

**行为**

检查是否包含指定值或键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。存在返回 1，不存在返回 0。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.has(value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:264](../../source/semantics/aggregate_ownership.cpp#L264)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-162"></a>

### Set::contains（163）

**声明**

```chali
int64 contains(T value);
```

**行为**

判断是否包含目标值或匹配文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。存在返回 1，不存在返回 0。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.contains(value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:263](../../source/semantics/aggregate_ownership.cpp#L263)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-163"></a>

### Set::remove（164）

**声明**

```chali
int64 remove(T value);
```

**行为**

删除指定位置或键的元素。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。找到并删除返回 1，键不存在返回 0。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.remove(value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:261](../../source/semantics/aggregate_ownership.cpp#L261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-164"></a>

### Set::at（165）

**声明**

```chali
T at(int64 index);
```

**行为**

读取从 0 开始的指定位置。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`T`。读取从 0 开始的指定位置。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.at(index);
```

**源码**：[source/semantics/aggregate_ownership.cpp:259](../../source/semantics/aggregate_ownership.cpp#L259)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-165"></a>

### Set::clear（166）

**声明**

```chali
int64 clear();
```

**行为**

移除全部元素。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.clear();
```

**源码**：[source/semantics/aggregate_ownership.cpp:265](../../source/semantics/aggregate_ownership.cpp#L265)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-166"></a>

### Set::len（167）

**声明**

```chali
int64 len();
```

**行为**

返回当前元素数量。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.len();
```

**源码**：[source/semantics/aggregate_ownership.cpp:266](../../source/semantics/aggregate_ownership.cpp#L266)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-167"></a>

### Map::copy（168）

**声明**

```chali
Map<K,V> copy();
```

**行为**

返回独立的值副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Map<K,V>`。返回独立的值副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.copy();
```

**源码**：[source/semantics/aggregate_ownership.cpp:256](../../source/semantics/aggregate_ownership.cpp#L256)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-168"></a>

### Map::put（169）

**声明**

```chali
int64 put(K key,V value);
```

**行为**

写入 key 对应的 value，已有键被替换。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `K key` | 查找键或下标 | 无 |
| `value` | `V value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.put(key, value);
```

**源码**：[source/semantics/aggregate_ownership.cpp:288](../../source/semantics/aggregate_ownership.cpp#L288)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-169"></a>

### Map::has（170）

**声明**

```chali
int64 has(K key);
```

**行为**

检查是否包含指定值或键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `K key` | 查找键或下标 | 无 |

**返回值**

`int64`。存在返回 1，不存在返回 0。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.has(key);
```

**源码**：[source/semantics/aggregate_ownership.cpp:264](../../source/semantics/aggregate_ownership.cpp#L264)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-170"></a>

### Map::contains（171）

**声明**

```chali
int64 contains(K key);
```

**行为**

判断是否包含目标值或匹配文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `K key` | 查找键或下标 | 无 |

**返回值**

`int64`。存在返回 1，不存在返回 0。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.contains(key);
```

**源码**：[source/semantics/aggregate_ownership.cpp:263](../../source/semantics/aggregate_ownership.cpp#L263)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-171"></a>

### Map::remove（172）

**声明**

```chali
int64 remove(K key);
```

**行为**

删除指定位置或键的元素。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `K key` | 查找键或下标 | 无 |

**返回值**

`int64`。找到并删除返回 1，键不存在返回 0。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.remove(key);
```

**源码**：[source/semantics/aggregate_ownership.cpp:261](../../source/semantics/aggregate_ownership.cpp#L261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-172"></a>

### Map::keyAt（173）

**声明**

```chali
K keyAt(int64 index);
```

**行为**

按遍历位置读取键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`K`。按遍历位置读取键。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.keyAt(index);
```

**源码**：[source/semantics/aggregate_ownership.cpp:292](../../source/semantics/aggregate_ownership.cpp#L292)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-173"></a>

### Map::valAt（174）

**声明**

```chali
V valAt(int64 index);
```

**行为**

按遍历位置读取值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`V`。按遍历位置读取值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.valAt(index);
```

**源码**：[source/semantics/aggregate_ownership.cpp:293](../../source/semantics/aggregate_ownership.cpp#L293)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-174"></a>

### Map::clear（175）

**声明**

```chali
int64 clear();
```

**行为**

移除全部元素。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.clear();
```

**源码**：[source/semantics/aggregate_ownership.cpp:265](../../source/semantics/aggregate_ownership.cpp#L265)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-175"></a>

### Map::len（176）

**声明**

```chali
int64 len();
```

**行为**

返回当前元素数量。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.len();
```

**源码**：[source/semantics/aggregate_ownership.cpp:266](../../source/semantics/aggregate_ownership.cpp#L266)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-176"></a>

### BigInteger::str（177）

**声明**

```chali
String str();
```

**行为**

生成大数的十进制文本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。生成大数的十进制文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.str();
```

**源码**：[source/semantics/boxing.cpp:814](../../source/semantics/boxing.cpp#L814)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-177"></a>

### BigInteger::copy（178）

**声明**

```chali
BigInteger copy();
```

**行为**

返回独立的值副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigInteger`。返回独立的值副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.copy();
```

**源码**：[source/semantics/boxing.cpp:804](../../source/semantics/boxing.cpp#L804)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-178"></a>

### BigInteger::pow（179）

**声明**

```chali
BigInteger pow(int64 exponent);
```

**行为**

计算当前值的 exponent 次非负整数次幂（快速幂）；exponent 为负时报运行错误。

非负整数次幂（快速幂）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `exponent` | `int64 exponent` | 指数 | 无 |

**返回值**

`BigInteger`。计算当前值的 exponent 次非负整数次幂（快速幂）；exponent 为负时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.pow(exponent);
```

**源码**：[source/semantics/boxing.cpp:829](../../source/semantics/boxing.cpp#L829)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-179"></a>

### BigInteger::powmod（180）

**声明**

```chali
BigInteger powmod(BigInteger exponent, BigInteger modulus);
```

**行为**

通过快速幂计算 this^exponent mod modulus；指数可以为大整数，modulus 为零或 exponent 为负时报运行错误。

模幂：`this^exponent mod modulus`（指数可以是大整数）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `exponent` | `BigInteger exponent` | 指数 | 无 |
| `modulus` | `BigInteger modulus` | 模数 | 无 |

**返回值**

`BigInteger`。通过快速幂计算 this^exponent mod modulus；指数可以为大整数，modulus 为零或 exponent 为负时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.powmod(exponent, modulus);
```

**源码**：[source/semantics/boxing.cpp:835](../../source/semantics/boxing.cpp#L835)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-180"></a>

### BigInteger::gcd（181）

**声明**

```chali
BigInteger gcd(BigInteger other);
```

**行为**

返回两数绝对值的最大公约数。

最大公约数 / 最小公倍数（取绝对值）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `BigInteger other` | 另一操作数 | 无 |

**返回值**

`BigInteger`。返回两数绝对值的最大公约数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.gcd(other);
```

**源码**：[source/semantics/boxing.cpp:841](../../source/semantics/boxing.cpp#L841)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-181"></a>

### BigInteger::lcm（182）

**声明**

```chali
BigInteger lcm(BigInteger other);
```

**行为**

返回两数绝对值的最小公倍数；任一操作数为零给零。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `BigInteger other` | 另一操作数 | 无 |

**返回值**

`BigInteger`。返回两数绝对值的最小公倍数；任一操作数为零给零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.lcm(other);
```

**源码**：[source/semantics/boxing.cpp:841](../../source/semantics/boxing.cpp#L841)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-182"></a>

### BigInteger::mod（183）

**声明**

```chali
BigInteger mod(BigInteger modulus);
```

**行为**

非负取模：结果落在 [0, |modulus|)；modulus 为零时报运行错误。

非负取模：结果落在 [0, |modulus|)。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `modulus` | `BigInteger modulus` | 模数 | 无 |

**返回值**

`BigInteger`。非负取模：结果落在 [0, |modulus|)；modulus 为零时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.mod(modulus);
```

**源码**：[source/semantics/boxing.cpp:849](../../source/semantics/boxing.cpp#L849)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-183"></a>

### BigInteger::modInverse（184）

**声明**

```chali
BigInteger modInverse(BigInteger modulus);
```

**行为**

计算模乘逆元，结果落在 [0, |modulus|)；模数绝对值不大于 1 或两数不互素时报运行错误。

模逆元；两数不互素时运行期报错。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `modulus` | `BigInteger modulus` | 模数 | 无 |

**返回值**

`BigInteger`。计算模乘逆元，结果落在 [0, |modulus|)；模数绝对值不大于 1 或两数不互素时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.modInverse(modulus);
```

**源码**：[source/semantics/boxing.cpp:855](../../source/semantics/boxing.cpp#L855)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-184"></a>

### BigInteger::isProbablePrime（185）

**声明**

```chali
bool isProbablePrime(int64 rounds);
```

**行为**

Miller-Rabin 概率素性测试，固定随机种子可复现；rounds 为测试轮数（非负）。

Miller-Rabin 概率素性测试（固定种子，可复现）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `rounds` | `int64 rounds` | Miller-Rabin 素性测试轮数 | 无 |

**返回值**

`bool`。Miller-Rabin 概率素性测试，固定随机种子可复现；rounds 为测试轮数（非负）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isProbablePrime(rounds);
```

**源码**：[source/semantics/boxing.cpp:861](../../source/semantics/boxing.cpp#L861)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-185"></a>

### BigInteger::nextProbablePrime（186）

**声明**

```chali
BigInteger nextProbablePrime();
```

**行为**

返回严格大于当前值的下一个概率素数（25 轮测试）。

严格大于当前值的下一个（概率）素数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigInteger`。返回严格大于当前值的下一个概率素数（25 轮测试）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.nextProbablePrime();
```

**源码**：[source/semantics/boxing.cpp:867](../../source/semantics/boxing.cpp#L867)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-186"></a>

### BigInteger::bitLength（187）

**声明**

```chali
int64 bitLength();
```

**行为**

返回当前值（绝对值）的二进制位长。

二进制位长。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前值（绝对值）的二进制位长。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.bitLength();
```

**源码**：[source/semantics/boxing.cpp:873](../../source/semantics/boxing.cpp#L873)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-187"></a>

### BigInteger::bitAnd（188）

**声明**

```chali
BigInteger bitAnd(BigInteger other);
```

**行为**

按位与；仅支持非负操作数，负数报运行错误。

按位与 / 或 / 异或（仅非负数；负数运行期报错）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `BigInteger other` | 另一操作数 | 无 |

**返回值**

`BigInteger`。按位与；仅支持非负操作数，负数报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.bitAnd(other);
```

**源码**：[source/semantics/boxing.cpp:879](../../source/semantics/boxing.cpp#L879)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-188"></a>

### BigInteger::bitOr（189）

**声明**

```chali
BigInteger bitOr(BigInteger other);
```

**行为**

按位或；仅支持非负操作数，负数报运行错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `BigInteger other` | 另一操作数 | 无 |

**返回值**

`BigInteger`。按位或；仅支持非负操作数，负数报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.bitOr(other);
```

**源码**：[source/semantics/boxing.cpp:879](../../source/semantics/boxing.cpp#L879)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-189"></a>

### BigInteger::bitXor（190）

**声明**

```chali
BigInteger bitXor(BigInteger other);
```

**行为**

按位异或；仅支持非负操作数，负数报运行错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `BigInteger other` | 另一操作数 | 无 |

**返回值**

`BigInteger`。按位异或；仅支持非负操作数，负数报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.bitXor(other);
```

**源码**：[source/semantics/boxing.cpp:879](../../source/semantics/boxing.cpp#L879)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-190"></a>

### BigInteger::strRadix（191）

**声明**

```chali
String strRadix(int64 base);
```

**行为**

生成 base（2..36）进制文本，数字用 0-9a-z，负数带前导 -；越界进制报运行错误。

2..36 进制字符串（数字用 0-9a-z，负数带 -）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `base` | `int64 base` | 幂底数 | 无 |

**返回值**

`String`。生成 base（2..36）进制文本，数字用 0-9a-z，负数带前导 -；越界进制报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.strRadix(base);
```

**源码**：[source/semantics/boxing.cpp:890](../../source/semantics/boxing.cpp#L890)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-191"></a>

### BigInteger::iroot（192）

**声明**

```chali
BigInteger iroot(int64 k);
```

**行为**

整数 k 次方根，向下取整；k 小于 1 报运行错误。

整数 k 次方根（向下取整）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `k` | `int64 k` | 方根次数 | 无 |

**返回值**

`BigInteger`。整数 k 次方根，向下取整；k 小于 1 报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.iroot(k);
```

**源码**：[source/semantics/boxing.cpp:896](../../source/semantics/boxing.cpp#L896)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-192"></a>

### BigDecimal::str（193）

**声明**

```chali
String str();
```

**行为**

生成大数的十进制文本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。生成大数的十进制文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.str();
```

**源码**：[source/semantics/boxing.cpp:814](../../source/semantics/boxing.cpp#L814)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-193"></a>

### BigDecimal::copy（194）

**声明**

```chali
BigDecimal copy();
```

**行为**

返回独立的值副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigDecimal`。返回独立的值副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.copy();
```

**源码**：[source/semantics/boxing.cpp:804](../../source/semantics/boxing.cpp#L804)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-194"></a>

### BigDecimal::setPrecision（195）

**声明**

```chali
int64 setPrecision(int64 digits);
```

**行为**

设置此 BigDecimal 的十进制精度；要求有效正精度，操作修改当前值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `digits` | `int64 digits` | 十进制有效数字精度 | 无 |

**返回值**

`int64`。设置此 BigDecimal 的十进制精度；要求有效正精度，操作修改当前值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.setPrecision(digits);
```

**源码**：[source/semantics/boxing.cpp:793](../../source/semantics/boxing.cpp#L793)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-195"></a>

### BigDecimal::pow（196）

**声明**

```chali
BigDecimal pow(int64 exponent);
```

**行为**

计算当前值的 exponent 次非负整数次幂，保持当前精度；exponent 为负时报运行错误。

精确非负整数次幂。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `exponent` | `int64 exponent` | 指数 | 无 |

**返回值**

`BigDecimal`。计算当前值的 exponent 次非负整数次幂，保持当前精度；exponent 为负时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.pow(exponent);
```

**源码**：[source/semantics/boxing.cpp:829](../../source/semantics/boxing.cpp#L829)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-196"></a>

### BigDecimal::abs（197）

**声明**

```chali
BigDecimal abs();
```

**行为**

返回绝对值。

绝对值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigDecimal`。返回绝对值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.abs();
```

**源码**：[source/semantics/boxing.cpp:912](../../source/semantics/boxing.cpp#L912)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-197"></a>

### BigDecimal::isInteger（198）

**声明**

```chali
bool isInteger();
```

**行为**

判断当前值是否为整数。

是不是整数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断当前值是否为整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isInteger();
```

**源码**：[source/semantics/boxing.cpp:918](../../source/semantics/boxing.cpp#L918)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-198"></a>

### BigDecimal::floor（199）

**声明**

```chali
BigInteger floor();
```

**行为**

向负无穷取整，返回大整数。

向负无穷 / 向正无穷 / 向零取整，返回大整数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigInteger`。向负无穷取整，返回大整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.floor();
```

**源码**：[source/semantics/boxing.cpp:924](../../source/semantics/boxing.cpp#L924)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-199"></a>

### BigDecimal::ceil（200）

**声明**

```chali
BigInteger ceil();
```

**行为**

向正无穷取整，返回大整数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigInteger`。向正无穷取整，返回大整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.ceil();
```

**源码**：[source/semantics/boxing.cpp:924](../../source/semantics/boxing.cpp#L924)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-200"></a>

### BigDecimal::trunc（201）

**声明**

```chali
BigInteger trunc();
```

**行为**

向零截断小数部分，返回大整数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigInteger`。向零截断小数部分，返回大整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.trunc();
```

**源码**：[source/semantics/boxing.cpp:924](../../source/semantics/boxing.cpp#L924)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-201"></a>

### BigDecimal::sqrt（202）

**声明**

```chali
BigDecimal sqrt(int64 digits);
```

**行为**

计算平方根，保留 digits 位有效数字。

平方根，保留 digits 位有效数字。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `digits` | `int64 digits` | 十进制有效数字精度 | 无 |

**返回值**

`BigDecimal`。计算平方根，保留 digits 位有效数字。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.sqrt(digits);
```

**源码**：[source/semantics/boxing.cpp:934](../../source/semantics/boxing.cpp#L934)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-202"></a>

### BigDecimal::root（203）

**声明**

```chali
BigDecimal root(int64 k, int64 digits);
```

**行为**

计算 k 次方根，保留 digits 位有效数字；k 小于 1 或 digits 越界时报运行错误。

k 次方根，保留 digits 位有效数字。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `k` | `int64 k` | 方根次数 | 无 |
| `digits` | `int64 digits` | 十进制有效数字精度 | 无 |

**返回值**

`BigDecimal`。计算 k 次方根，保留 digits 位有效数字；k 小于 1 或 digits 越界时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.root(k, digits);
```

**源码**：[source/semantics/boxing.cpp:940](../../source/semantics/boxing.cpp#L940)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-203"></a>

### Any::copy（204）

**声明**

```chali
Any copy();
```

**行为**

返回独立的值副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Any`。返回独立的值副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.copy();
```

**源码**：[source/semantics/boxing.cpp:804](../../source/semantics/boxing.cpp#L804)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-204"></a>

### Any::isEmpty（205）

**声明**

```chali
bool isEmpty();
```

**行为**

判断 Any 是否为空盒子，不公开内部执行期标签。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 Any 是否为空盒子，不公开内部执行期标签。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isEmpty();
```

**源码**：[source/semantics/boxing.cpp:966](../../source/semantics/boxing.cpp#L966)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-205"></a>

### Any::clear（206）

**声明**

```chali
void clear();
```

**行为**

释放盒子拥有的载荷并恢复空状态；保留盒子自身的键身份。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.clear();
```

**源码**：[source/semantics/boxing.cpp:973](../../source/semantics/boxing.cpp#L973)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-206"></a>

### Any::set（207）

**声明**

```chali
int64 set(T value);
```

**行为**

以 value 的值副本和类型编号替换盒子内容。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。以 value 的值副本和类型编号替换盒子内容。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.set(value);
```

**源码**：[source/semantics/boxing.cpp:989](../../source/semantics/boxing.cpp#L989)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-207"></a>

### Any::getValue（208）

**声明**

```chali
T getValue();
```

**行为**

按接收上下文要求的类型取出拥有值，类型不匹配时报错。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。按接收上下文要求的类型取出拥有值，类型不匹配时报错。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.getValue();
```

**源码**：[source/semantics/boxing.cpp:134](../../source/semantics/boxing.cpp#L134)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-208"></a>

### Any::getReference（209）

**声明**

```chali
T mut& getReference();
```

**行为**

按上下文要求的类型借用盒子内的值，引用受盒子生命周期约束。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T mut&`。按上下文要求的类型借用盒子内的值，引用受盒子生命周期约束。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.getReference();
```

**源码**：[source/semantics/boxing.cpp:134](../../source/semantics/boxing.cpp#L134)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-209"></a>

### Any::getOptional（210）

**声明**

```chali
Option<T mut&> getOptional();
```

**行为**

按上下文要求的类型获取可选引用，类型不匹配为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T mut&>`。按上下文要求的类型获取可选引用，类型不匹配为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.getOptional();
```

**源码**：[source/semantics/boxing.cpp:134](../../source/semantics/boxing.cpp#L134)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-210"></a>

### eq（211）

**声明**

```chali
bool eq(T left,T right);
```

**行为**

内建运算函数，等价于 left == right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `T right` | 右操作数或区间右端 | 无 |

**返回值**

`bool`。内建运算函数，等价于 left == right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
eq(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-211"></a>

### ne（212）

**声明**

```chali
bool ne(T left,T right);
```

**行为**

内建运算函数，等价于 left != right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `T right` | 右操作数或区间右端 | 无 |

**返回值**

`bool`。内建运算函数，等价于 left != right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
ne(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-212"></a>

### lt（213）

**声明**

```chali
bool lt(T left,T right);
```

**行为**

内建运算函数，等价于 left < right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `T right` | 右操作数或区间右端 | 无 |

**返回值**

`bool`。内建运算函数，等价于 left < right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
lt(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-213"></a>

### le（214）

**声明**

```chali
bool le(T left,T right);
```

**行为**

内建运算函数，等价于 left <= right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `T right` | 右操作数或区间右端 | 无 |

**返回值**

`bool`。内建运算函数，等价于 left <= right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
le(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-214"></a>

### gt（215）

**声明**

```chali
bool gt(T left,T right);
```

**行为**

内建运算函数，等价于 left > right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `T right` | 右操作数或区间右端 | 无 |

**返回值**

`bool`。内建运算函数，等价于 left > right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
gt(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-215"></a>

### ge（216）

**声明**

```chali
bool ge(T left,T right);
```

**行为**

内建运算函数，等价于 left >= right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `T right` | 右操作数或区间右端 | 无 |

**返回值**

`bool`。内建运算函数，等价于 left >= right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
ge(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-216"></a>

### add（217）

**声明**

```chali
R add(T left,U right);
```

**行为**

计算逐项相加或追加元素，具体操作取决于所属类型。

内建运算函数，等价于 left + right，结果由运算符解析决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `U right` | 右操作数或区间右端 | 无 |

**返回值**

`R`。计算逐项相加或追加元素，具体操作取决于所属类型。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
add(left, right);
```

**源码**：[source/semantics/boxing.cpp:1146](../../source/semantics/boxing.cpp#L1146)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-217"></a>

### sub（218）

**声明**

```chali
R sub(T left,U right);
```

**行为**

内建运算函数，等价于 left - right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `U right` | 右操作数或区间右端 | 无 |

**返回值**

`R`。内建运算函数，等价于 left - right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
sub(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-218"></a>

### mul（219）

**声明**

```chali
R mul(T left,U right);
```

**行为**

内建运算函数，等价于 left * right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `U right` | 右操作数或区间右端 | 无 |

**返回值**

`R`。内建运算函数，等价于 left * right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
mul(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-219"></a>

### div（220）

**声明**

```chali
R div(T left,U right);
```

**行为**

内建运算函数，等价于 left / right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `U right` | 右操作数或区间右端 | 无 |

**返回值**

`R`。内建运算函数，等价于 left / right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
div(left, right);
```

**源码**：[source/semantics/boxing.cpp:1](../../source/semantics/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-220"></a>

### mod（221）

**声明**

```chali
R mod(T left,U right);
```

**行为**

内建运算函数，等价于 left % right。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `U right` | 右操作数或区间右端 | 无 |

**返回值**

`R`。内建运算函数，等价于 left % right。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
mod(left, right);
```

**源码**：[source/semantics/boxing.cpp:849](../../source/semantics/boxing.cpp#L849)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-221"></a>

### index（222）

**声明**

```chali
T index(T const& data,int64 index);
```

**行为**

读取 key 对应的元素，供下标语法调用。

读取连续存储的第 index 项，相当于下标操作。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T const& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`T`。读取 key 对应的元素，供下标语法调用。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
index(data, index);
```

**源码**：[source/semantics/boxing.cpp:1732](../../source/semantics/boxing.cpp#L1732)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-222"></a>

### Add::leftAdd（223）

**声明**

```chali
R leftAdd(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftAdd；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftAdd；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftAdd(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-223"></a>

### RightAdd::rightAdd（224）

**声明**

```chali
R rightAdd(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightAdd；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightAdd；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightAdd(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-224"></a>

### Subtract::leftSub（225）

**声明**

```chali
R leftSub(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftSub；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftSub；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftSub(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-225"></a>

### RightSubtract::rightSub（226）

**声明**

```chali
R rightSub(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightSub；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightSub；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightSub(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-226"></a>

### Multiply::leftMul（227）

**声明**

```chali
R leftMul(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftMul；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftMul；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftMul(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-227"></a>

### RightMultiply::rightMul（228）

**声明**

```chali
R rightMul(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightMul；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightMul；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightMul(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-228"></a>

### Divide::leftDiv（229）

**声明**

```chali
R leftDiv(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftDiv；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftDiv；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftDiv(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-229"></a>

### RightDivide::rightDiv（230）

**声明**

```chali
R rightDiv(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightDiv；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightDiv；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightDiv(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-230"></a>

### Modulo::leftMod（231）

**声明**

```chali
R leftMod(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftMod；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftMod；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftMod(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-231"></a>

### RightModulo::rightMod（232）

**声明**

```chali
R rightMod(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightMod；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightMod；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightMod(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-232"></a>

### Equal::leftEqual（233）

**声明**

```chali
R leftEqual(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftEqual(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-233"></a>

### RightEqual::rightEqual（234）

**声明**

```chali
R rightEqual(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightEqual(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-234"></a>

### NotEqual::leftNotEqual（235）

**声明**

```chali
R leftNotEqual(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftNotEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftNotEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftNotEqual(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-235"></a>

### RightNotEqual::rightNotEqual（236）

**声明**

```chali
R rightNotEqual(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightNotEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightNotEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightNotEqual(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-236"></a>

### Less::leftLess（237）

**声明**

```chali
R leftLess(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftLess；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftLess；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftLess(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-237"></a>

### RightLess::rightLess（238）

**声明**

```chali
R rightLess(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightLess；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightLess；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightLess(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-238"></a>

### LessOrEqual::leftLessEqual（239）

**声明**

```chali
R leftLessEqual(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftLessEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftLessEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftLessEqual(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-239"></a>

### RightLessOrEqual::rightLessEqual（240）

**声明**

```chali
R rightLessEqual(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightLessEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightLessEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightLessEqual(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-240"></a>

### Greater::leftGreater（241）

**声明**

```chali
R leftGreater(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftGreater；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftGreater；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftGreater(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-241"></a>

### RightGreater::rightGreater（242）

**声明**

```chali
R rightGreater(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightGreater；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightGreater；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightGreater(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-242"></a>

### GreaterOrEqual::leftGreaterEqual（243）

**声明**

```chali
R leftGreaterEqual(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftGreaterEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftGreaterEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftGreaterEqual(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-243"></a>

### RightGreaterOrEqual::rightGreaterEqual（244）

**声明**

```chali
R rightGreaterEqual(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightGreaterEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightGreaterEqual；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightGreaterEqual(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-244"></a>

### And::leftBitAnd（245）

**声明**

```chali
R leftBitAnd(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftBitAnd；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftBitAnd；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftBitAnd(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-245"></a>

### RightAnd::rightBitAnd（246）

**声明**

```chali
R rightBitAnd(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightBitAnd；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightBitAnd；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightBitAnd(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-246"></a>

### Or::leftBitOr（247）

**声明**

```chali
R leftBitOr(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftBitOr；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftBitOr；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftBitOr(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-247"></a>

### RightOr::rightBitOr（248）

**声明**

```chali
R rightBitOr(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightBitOr；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightBitOr；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightBitOr(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-248"></a>

### Xor::leftBitXor（249）

**声明**

```chali
R leftBitXor(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftBitXor；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftBitXor；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftBitXor(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-249"></a>

### RightXor::rightBitXor（250）

**声明**

```chali
R rightBitXor(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightBitXor；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightBitXor；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightBitXor(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-250"></a>

### ShiftLeft::leftShiftLeft（251）

**声明**

```chali
R leftShiftLeft(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftShiftLeft；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftShiftLeft；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftShiftLeft(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-251"></a>

### RightShiftLeft::rightShiftLeft（252）

**声明**

```chali
R rightShiftLeft(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightShiftLeft；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightShiftLeft；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightShiftLeft(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-252"></a>

### ShiftRight::leftShiftRight（253）

**声明**

```chali
R leftShiftRight(T value);
```

**行为**

用户类型实现此运算符接口时提供 leftShiftRight；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 leftShiftRight；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.leftShiftRight(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-253"></a>

### RightShiftRight::rightShiftRight（254）

**声明**

```chali
R rightShiftRight(T value);
```

**行为**

用户类型实现此运算符接口时提供 rightShiftRight；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 rightShiftRight；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.rightShiftRight(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-254"></a>

### Negate::negate（255）

**声明**

```chali
R negate(T value);
```

**行为**

用户类型实现此运算符接口时提供 negate；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 negate；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.negate(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-255"></a>

### BitNot::bitNot（256）

**声明**

```chali
R bitNot(T value);
```

**行为**

用户类型实现此运算符接口时提供 bitNot；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 bitNot；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.bitNot(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-256"></a>

### Positive::positive（257）

**声明**

```chali
R positive(T value);
```

**行为**

用户类型实现此运算符接口时提供 positive；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`R`。用户类型实现此运算符接口时提供 positive；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.positive(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-257"></a>

### Hash::hash（258）

**声明**

```chali
uint64 hash(T value);
```

**行为**

返回输入值的哈希值，由实现者保持与相等性的一致性。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`uint64`。返回输入值的哈希值，由实现者保持与相等性的一致性。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.hash(value);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-258"></a>

### ExplicitConvert::explicitConvert（259）

**声明**

```chali
T explicitConvert();
```

**行为**

由用户类型提供显式目标类型转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。由用户类型提供显式目标类型转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.explicitConvert();
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-259"></a>

### ImplicitConvert::implicitConvert（260）

**声明**

```chali
T implicitConvert();
```

**行为**

由用户类型提供隐式目标类型转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。由用户类型提供隐式目标类型转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.implicitConvert();
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-260"></a>

### Index::index（261）

**声明**

```chali
R index(T key);
```

**行为**

读取 key 对应的元素，供下标语法调用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `T key` | 查找键或下标 | 无 |

**返回值**

`R`。读取 key 对应的元素，供下标语法调用。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.index(key);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-261"></a>

### Iterator::iterator（262）

**声明**

```chali
Option<R> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<R>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.iterator(index);
```

**源码**：[source/chali_generic.cpp:827](../../source/chali_generic.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-262"></a>

### Result::mapErr（263）

**声明**

```chali
Result<T,F> mapErr<F>(F fn(E) mapper);
```

**行为**

仅在失败时调用 mapper 转换错误类型，成功值保持不变。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `mapper` | `F fn(E) mapper` | 转换回调，签名决定输入和输出类型 | 无 |

**返回值**

`Result<T,F>`。仅在失败时调用 mapper 转换错误类型，成功值保持不变。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.mapErr<F>(mapper);
```

**源码**：[source/std/lang.chl:2](../../source/std/lang.chl#L2)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-263"></a>

### Option::okOr（264）

**声明**

```chali
Result<T,E> okOr<E>(E error);
```

**行为**

将 None 转为给定错误，error 按普通实参规则求值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `error` | `E error` | 缺失时产生的错误值。 | 无 |

**返回值**

`Result<T,E>`。将 None 转为给定错误，error 按普通实参规则求值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.okOr<E>(error);
```

**源码**：[source/std/lang.chl:3](../../source/std/lang.chl#L3)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

<a id="api-264"></a>

### Option::okOrElse（265）

**声明**

```chali
Result<T,E> okOrElse<E>(E fn() factory);
```

**行为**

只在 None 时调用错误工厂，成功分支不调用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `factory` | `E fn() factory` | 缺失时才调用的无参数错误工厂。 | 无 |

**返回值**

`Result<T,E>`。只在 None 时调用错误工厂，成功分支不调用。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.okOrElse<E>(factory);
```

**源码**：[source/std/lang.chl:3](../../source/std/lang.chl#L3)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.chl](../../example/samples/std_collections_algorithms.chl)。

## 补充完整示例

[example/samples/std_read_lines.chl](../../example/samples/std_read_lines.chl)。

```chali
void main() {
    String first=std::lang::readLine();
    String rest=std::lang::readLines(3);
    println("[{}]",first);
    println("[{}]",rest);
    assert(std::lang::readLines(0)=="");
    assert(std::lang::readLine()=="");

}
```
