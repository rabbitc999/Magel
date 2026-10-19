std::lang::sweep() 不是垃圾回收器。它不做引用追踪、不判断对象存活、不自动调用。它只做一件事：清理已释放块的墓碑记录。

# std::lang — 核心库

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
// std::lang 自动加载
```

## 公共约定与错误边界

自动加载，无需 import。Option/Result 通过 match 或 isValue/isOk 分支处理；unwrap/expect 在失败分支触发 panic。浮点转整数向零截断，范围检查见各重载；字节编码默认小端。拥有值按值复制，引用的生命周期不得超过来源。 List.pop 空列表报错，列表/集合/映射的按位置访问需有效下标。Set/Map 的 has/contains 返回 int64 的 0/1；List 的同名接口返回 bool。Any.getReference/getOptional 的 const/mut 由接收上下文决定，返回引用受盒子寿命限制。BigDecimal 精度范围 1..100000。内存内建接受有效存储引用，字节计数不是元素数量。$assert(condition) 和 $assert(condition,message) 为宏调用，自动添加调用点行号，宏体自带分号。

## 参考代码

完整程序：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

```powershell
magel run example/samples/std_collections_algorithms.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::collections;

import std::memory;

import std::algorithms;

bool positive(int64 value) { return value > 0; }
int64 addValues(int64 left, int64 right) { return left + right; }

int64 main() {
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

    Pair<int64, String> pair = Pair<int64, String>::of(7, "seven");
    println("{} {}", pair.first, pair.second);
    delete raw;
    return 0;
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
| 模块 | <a id="symbol-230"></a>`struct Pair<K, V>` | 包含 first 和 second 的两个值，分别使用 K 与 V 类型。 |
| Pair | <a id="symbol-231"></a>`K first` | 第一项，类型为 K。 |
| Pair | <a id="symbol-232"></a>`V second` | 第二项，类型为 V。 |
| 模块 | <a id="symbol-233"></a>`interface Drop` | 显式释放拥有资源的析构协议。 |
| 模块 | <a id="symbol-234"></a>`interface Closeable` | 关闭外部资源的协议。 |
| 模块 | <a id="symbol-235"></a>`struct List<T>` | 可变长度的顺序值容器，支持按下标访问、追加、删除与复制。 |
| 模块 | <a id="symbol-236"></a>`struct Set<T>` | 保存唯一元素的值集合，支持存在检查和集合运算。 |
| 模块 | <a id="symbol-237"></a>`struct Map<K,V>` | 从唯一键到值的映射，支持键查询和按遍历位置读取。 |
| 模块 | <a id="symbol-238"></a>`struct BigInteger` | 任意精度有符号整数，算术由大数运行时实现。 |
| 模块 | <a id="symbol-239"></a>`struct BigDecimal` | 可配置精度的十进制大数。 |
| 模块 | <a id="symbol-240"></a>`struct Any` | 拥有一个装箱值的类型擦除容器；通过类型检查安全读取。 |
| 模块 | <a id="symbol-241"></a>`struct Math` | 核心数学常量和数值操作的静态入口。 |
| Math | <a id="symbol-242"></a>`static const float64 PI` | 圆周率常量。 |
| Math | <a id="symbol-243"></a>`static const float64 E` | 自然对数底数常量。 |
| 模块 | <a id="symbol-244"></a>`interface Add<R,T>` | 左操作数的加法重载接口。 |
| 模块 | <a id="symbol-245"></a>`interface RightAdd<R,T>` | 右操作数的加法重载接口。 |
| 模块 | <a id="symbol-246"></a>`interface Subtract<R,T>` | 左操作数的减法重载接口。 |
| 模块 | <a id="symbol-247"></a>`interface RightSubtract<R,T>` | 右操作数的减法重载接口。 |
| 模块 | <a id="symbol-248"></a>`interface Multiply<R,T>` | 左操作数的乘法重载接口。 |
| 模块 | <a id="symbol-249"></a>`interface RightMultiply<R,T>` | 右操作数的乘法重载接口。 |
| 模块 | <a id="symbol-250"></a>`interface Divide<R,T>` | 左操作数的除法重载接口。 |
| 模块 | <a id="symbol-251"></a>`interface RightDivide<R,T>` | 右操作数的除法重载接口。 |
| 模块 | <a id="symbol-252"></a>`interface Modulo<R,T>` | 左操作数的取模重载接口。 |
| 模块 | <a id="symbol-253"></a>`interface RightModulo<R,T>` | 右操作数的取模重载接口。 |
| 模块 | <a id="symbol-254"></a>`interface Equal<R,T>` | 左操作数的相等比较重载接口。 |
| 模块 | <a id="symbol-255"></a>`interface RightEqual<R,T>` | 右操作数的相等比较重载接口。 |
| 模块 | <a id="symbol-256"></a>`interface NotEqual<R,T>` | 左操作数的不等比较重载接口。 |
| 模块 | <a id="symbol-257"></a>`interface RightNotEqual<R,T>` | 右操作数的不等比较重载接口。 |
| 模块 | <a id="symbol-258"></a>`interface Less<R,T>` | 左操作数的小于比较重载接口。 |
| 模块 | <a id="symbol-259"></a>`interface RightLess<R,T>` | 右操作数的小于比较重载接口。 |
| 模块 | <a id="symbol-260"></a>`interface LessOrEqual<R,T>` | 左操作数的小于等于比较重载接口。 |
| 模块 | <a id="symbol-261"></a>`interface RightLessOrEqual<R,T>` | 右操作数的小于等于比较重载接口。 |
| 模块 | <a id="symbol-262"></a>`interface Greater<R,T>` | 左操作数的大于比较重载接口。 |
| 模块 | <a id="symbol-263"></a>`interface RightGreater<R,T>` | 右操作数的大于比较重载接口。 |
| 模块 | <a id="symbol-264"></a>`interface GreaterOrEqual<R,T>` | 左操作数的大于等于比较重载接口。 |
| 模块 | <a id="symbol-265"></a>`interface RightGreaterOrEqual<R,T>` | 右操作数的大于等于比较重载接口。 |
| 模块 | <a id="symbol-266"></a>`interface And<R,T>` | 左操作数的按位与重载接口。 |
| 模块 | <a id="symbol-267"></a>`interface RightAnd<R,T>` | 右操作数的按位与重载接口。 |
| 模块 | <a id="symbol-268"></a>`interface Or<R,T>` | 左操作数的按位或重载接口。 |
| 模块 | <a id="symbol-269"></a>`interface RightOr<R,T>` | 右操作数的按位或重载接口。 |
| 模块 | <a id="symbol-270"></a>`interface Xor<R,T>` | 左操作数的按位异或重载接口。 |
| 模块 | <a id="symbol-271"></a>`interface RightXor<R,T>` | 右操作数的按位异或重载接口。 |
| 模块 | <a id="symbol-272"></a>`interface ShiftLeft<R,T>` | 左操作数的左移重载接口。 |
| 模块 | <a id="symbol-273"></a>`interface RightShiftLeft<R,T>` | 右操作数的左移重载接口。 |
| 模块 | <a id="symbol-274"></a>`interface ShiftRight<R,T>` | 左操作数的右移重载接口。 |
| 模块 | <a id="symbol-275"></a>`interface RightShiftRight<R,T>` | 右操作数的右移重载接口。 |
| 模块 | <a id="symbol-276"></a>`interface Negate<R,T>` | 左操作数的取负重载接口。 |
| 模块 | <a id="symbol-277"></a>`interface BitNot<R,T>` | 左操作数的按位取反重载接口。 |
| 模块 | <a id="symbol-278"></a>`interface Positive<R,T>` | 左操作数的取正重载接口。 |
| 模块 | <a id="symbol-279"></a>`interface Hash<T>` | 为值提供哈希计算的接口。 |
| 模块 | <a id="symbol-280"></a>`interface ExplicitConvert<T>` | 显式类型转换的接口。 |
| 模块 | <a id="symbol-281"></a>`interface ImplicitConvert<T>` | 隐式类型转换的接口。 |
| 模块 | <a id="symbol-282"></a>`interface Index<R,T>` | 自定义下标访问接口；与小写 index 连续存储函数区分。 |
| 模块 | <a id="symbol-283"></a>`interface Iterator<R>` | 供 for-in 获取和推进迭代状态的接口。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [readLine](#api-0) | `String readLine()` |
| [readLines](#api-1) | `String readLines(int32 N)` |
| [readLines](#api-2) | `String readLines(int64 N)` |
| [toInt32](#api-3) | `Option<int32> toInt32(float64 value)` |
| [toInt32](#api-4) | `Option<int32> toInt32(float32 value)` |
| [toInt64](#api-5) | `int64 toInt64(float64 value)` |
| [toInt64](#api-6) | `int64 toInt64(float32 value)` |
| [float32ToBytes](#api-7) | `std::lang::List<uint8> float32ToBytes(float32 value, bool bigEndian)` |
| [float32ToBytes](#api-8) | `std::lang::List<uint8> float32ToBytes(float32 value)` |
| [bytesToFloat32](#api-9) | `Option<float32> bytesToFloat32(std::lang::List<uint8> bytes, bool bigEndian)` |
| [bytesToFloat32](#api-10) | `Option<float32> bytesToFloat32(std::lang::List<uint8> bytes)` |
| [bytesToFloat32](#api-11) | `Option<float32> bytesToFloat32(uint8 const& bytes, int64 count, bool bigEndian)` |
| [bytesToFloat32](#api-12) | `Option<float32> bytesToFloat32(uint8 const& bytes, int64 count)` |
| [bytesToFloat32](#api-13) | `Option<float32> bytesToFloat32(uint8 bytes[], bool bigEndian)` |
| [bytesToFloat32](#api-14) | `Option<float32> bytesToFloat32(uint8 bytes[])` |
| [float32ToBytes](#api-15) | `bool float32ToBytes(float32 value, uint8 mut& bytes, int64 count, bool bigEndian)` |
| [float32ToBytes](#api-16) | `bool float32ToBytes(float32 value, uint8 mut& bytes, int64 count)` |
| [float32ToBytes](#api-17) | `bool float32ToBytes(float32 value, uint8[] mut& bytes, bool bigEndian)` |
| [float32ToBytes](#api-18) | `bool float32ToBytes(float32 value, uint8[] mut& bytes)` |
| [float64ToBytes](#api-19) | `std::lang::List<uint8> float64ToBytes(float64 value, bool bigEndian)` |
| [float64ToBytes](#api-20) | `std::lang::List<uint8> float64ToBytes(float64 value)` |
| [bytesToFloat64](#api-21) | `Option<float64> bytesToFloat64(std::lang::List<uint8> bytes, bool bigEndian)` |
| [bytesToFloat64](#api-22) | `Option<float64> bytesToFloat64(std::lang::List<uint8> bytes)` |
| [bytesToFloat64](#api-23) | `Option<float64> bytesToFloat64(uint8 const& bytes, int64 count, bool bigEndian)` |
| [bytesToFloat64](#api-24) | `Option<float64> bytesToFloat64(uint8 const& bytes, int64 count)` |
| [bytesToFloat64](#api-25) | `Option<float64> bytesToFloat64(uint8 bytes[], bool bigEndian)` |
| [bytesToFloat64](#api-26) | `Option<float64> bytesToFloat64(uint8 bytes[])` |
| [float64ToBytes](#api-27) | `bool float64ToBytes(float64 value, uint8 mut& bytes, int64 count, bool bigEndian)` |
| [float64ToBytes](#api-28) | `bool float64ToBytes(float64 value, uint8 mut& bytes, int64 count)` |
| [float64ToBytes](#api-29) | `bool float64ToBytes(float64 value, uint8[] mut& bytes, bool bigEndian)` |
| [float64ToBytes](#api-30) | `bool float64ToBytes(float64 value, uint8[] mut& bytes)` |
| [assert](#api-31) | `void assert(bool condition)` |
| [assert](#api-32) | `void assert(bool condition, std::lang::String message)` |
| [assert](#api-33) | `void assert(bool condition, int64 line)` |
| [assert](#api-34) | `void assert(bool condition, std::lang::String message, int64 line)` |
| [Try::branch](#api-35) | `TryBranch<T, R> branch()` |
| [TryMove::intoBranch](#api-36) | `TryBranch<T, R> intoBranch()` |
| [FromResidual::fromResidual](#api-37) | `static F fromResidual(R residual)` |
| [Option::branch](#api-38) | `TryBranch<T, NoneResidual> branch()` |
| [Option::intoBranch](#api-39) | `TryBranch<T, NoneResidual> intoBranch()` |
| [Option::fromResidual](#api-40) | `static Option<T> fromResidual(NoneResidual residual)` |
| [Option::isValue](#api-41) | `bool isValue()` |
| [Option::isNone](#api-42) | `bool isNone()` |
| [Option::unwrap](#api-43) | `T unwrap()` |
| [Option::unwrapOr](#api-44) | `T unwrapOr(T fallback)` |
| [Option::expect](#api-45) | `T expect(std::lang::String message)` |
| [Option::transform](#api-46) | `Option<T> transform(T fn(T) mapper)` |
| [Option::andThen](#api-47) | `Option<T> andThen(Option<T> fn(T) mapper)` |
| [Result::branch](#api-48) | `TryBranch<T, ErrorResidual<E>> branch()` |
| [Result::intoBranch](#api-49) | `TryBranch<T, ErrorResidual<E>> intoBranch()` |
| [Result::fromResidual](#api-50) | `static Result<T,E> fromResidual(ErrorResidual<E> residual)` |
| [Result::ok](#api-51) | `Option<T> ok()` |
| [Result::isOk](#api-52) | `bool isOk()` |
| [Result::isErr](#api-53) | `bool isErr()` |
| [Result::unwrap](#api-54) | `T unwrap()` |
| [Result::unwrapOr](#api-55) | `T unwrapOr(T fallback)` |
| [Result::expect](#api-56) | `T expect(std::lang::String message)` |
| [Result::error](#api-57) | `E error()` |
| [Result::transform](#api-58) | `Result<T, E> transform(T fn(T) mapper)` |
| [Result::andThen](#api-59) | `Result<T, E> andThen(Result<T, E> fn(T) mapper)` |
| [Pair::of](#api-60) | `static Pair<K, V> of(K first, V second)` |
| [tryGet](#api-61) | `Option<T> tryGet<T>(Any const& value)` |
| [tryGetReference](#api-62) | `Option<T const&> tryGetReference<T>(Any const& value)` |
| [tryGetMut](#api-63) | `Option<T mut&> tryGetMut<T>(Any mut& value)` |
| [stringBytes](#api-64) | `List<uint8> stringBytes(String text)` |
| [range](#api-65) | `__Range range(int64 start, int64 stop)` |
| [range](#api-66) | `__Range range(int64 start, int64 stop, int64 step)` |
| [Drop::drop](#api-67) | `void drop()` |
| [Closeable::close](#api-68) | `void close()` |
| [print](#api-69) | `int64 print(String format, Any args...)` |
| [println](#api-70) | `int64 println(String format, Any args...)` |
| [format](#api-71) | `String format(String format, Any args...)` |
| [putChar](#api-72) | `int64 putChar(char value)` |
| [pause](#api-73) | `int64 pause()` |
| [sweep](#api-74) | `int64 sweep()` |
| [inputLine](#api-75) | `String inputLine()` |
| [inputInt](#api-76) | `int64 inputInt()` |
| [inputUInt](#api-77) | `uint64 inputUInt()` |
| [inputFloat](#api-78) | `float64 inputFloat()` |
| [inputBool](#api-79) | `bool inputBool()` |
| [inputChar](#api-80) | `char inputChar()` |
| [inputBigInteger](#api-81) | `BigInteger inputBigInteger()` |
| [inputBigDecimal](#api-82) | `BigDecimal inputBigDecimal()` |
| [panic](#api-83) | `int64 panic(String message)` |
| [len](#api-84) | `int64 len(Any value)` |
| [move](#api-85) | `T move<T>(T mut& value)` |
| [toReference](#api-86) | `T mut& toReference<T>(Option<T mut&> value)` |
| [toOptional](#api-87) | `Option<T mut&> toOptional<T>(T mut& value)` |
| [memcpy](#api-88) | `int64 memcpy(char mut& destination, char const& source, int64 bytes)` |
| [memset](#api-89) | `int64 memset(char mut& destination, int64 value, int64 bytes)` |
| [List::copy](#api-90) | `List<T> copy()` |
| [List::push](#api-91) | `int64 push(T value)` |
| [List::insert](#api-92) | `int64 insert(int64 index, T value)` |
| [List::at](#api-93) | `T at(int64 index)` |
| [List::pop](#api-94) | `T pop()` |
| [List::remove](#api-95) | `int64 remove(int64 index)` |
| [List::indexOf](#api-96) | `int64 indexOf(T value)` |
| [List::contains](#api-97) | `bool contains(T value)` |
| [List::has](#api-98) | `bool has(T value)` |
| [List::clear](#api-99) | `int64 clear()` |
| [List::len](#api-100) | `int64 len()` |
| [Set::copy](#api-101) | `Set<T> copy()` |
| [Set::add](#api-102) | `int64 add(T value)` |
| [Set::has](#api-103) | `int64 has(T value)` |
| [Set::contains](#api-104) | `int64 contains(T value)` |
| [Set::remove](#api-105) | `int64 remove(T value)` |
| [Set::at](#api-106) | `T at(int64 index)` |
| [Set::clear](#api-107) | `int64 clear()` |
| [Set::len](#api-108) | `int64 len()` |
| [Map::copy](#api-109) | `Map<K,V> copy()` |
| [Map::put](#api-110) | `int64 put(K key,V value)` |
| [Map::has](#api-111) | `int64 has(K key)` |
| [Map::contains](#api-112) | `int64 contains(K key)` |
| [Map::remove](#api-113) | `int64 remove(K key)` |
| [Map::keyAt](#api-114) | `K keyAt(int64 index)` |
| [Map::valAt](#api-115) | `V valAt(int64 index)` |
| [Map::clear](#api-116) | `int64 clear()` |
| [Map::len](#api-117) | `int64 len()` |
| [BigInteger::str](#api-118) | `String str()` |
| [BigInteger::copy](#api-119) | `BigInteger copy()` |
| [BigInteger::pow](#api-120) | `BigInteger pow(int64 exponent)` |
| [BigInteger::powmod](#api-121) | `BigInteger powmod(BigInteger exponent, BigInteger modulus)` |
| [BigInteger::gcd](#api-122) | `BigInteger gcd(BigInteger other)` |
| [BigInteger::lcm](#api-123) | `BigInteger lcm(BigInteger other)` |
| [BigInteger::mod](#api-124) | `BigInteger mod(BigInteger modulus)` |
| [BigInteger::modInverse](#api-125) | `BigInteger modInverse(BigInteger modulus)` |
| [BigInteger::isProbablePrime](#api-126) | `bool isProbablePrime(int64 rounds)` |
| [BigInteger::nextProbablePrime](#api-127) | `BigInteger nextProbablePrime()` |
| [BigInteger::bitLength](#api-128) | `int64 bitLength()` |
| [BigInteger::bitAnd](#api-129) | `BigInteger bitAnd(BigInteger other)` |
| [BigInteger::bitOr](#api-130) | `BigInteger bitOr(BigInteger other)` |
| [BigInteger::bitXor](#api-131) | `BigInteger bitXor(BigInteger other)` |
| [BigInteger::strRadix](#api-132) | `String strRadix(int64 base)` |
| [BigInteger::iroot](#api-133) | `BigInteger iroot(int64 k)` |
| [BigDecimal::str](#api-134) | `String str()` |
| [BigDecimal::copy](#api-135) | `BigDecimal copy()` |
| [BigDecimal::setPrecision](#api-136) | `int64 setPrecision(int64 digits)` |
| [BigDecimal::pow](#api-137) | `BigDecimal pow(int64 exponent)` |
| [BigDecimal::abs](#api-138) | `BigDecimal abs()` |
| [BigDecimal::isInteger](#api-139) | `bool isInteger()` |
| [BigDecimal::floor](#api-140) | `BigInteger floor()` |
| [BigDecimal::ceil](#api-141) | `BigInteger ceil()` |
| [BigDecimal::trunc](#api-142) | `BigInteger trunc()` |
| [BigDecimal::sqrt](#api-143) | `BigDecimal sqrt(int64 digits)` |
| [BigDecimal::root](#api-144) | `BigDecimal root(int64 k, int64 digits)` |
| [Any::copy](#api-145) | `Any copy()` |
| [Any::isEmpty](#api-146) | `bool isEmpty()` |
| [Any::clear](#api-147) | `void clear()` |
| [Any::set](#api-148) | `int64 set(T value)` |
| [Any::getValue](#api-149) | `T getValue()` |
| [Any::getReference](#api-150) | `T mut& getReference()` |
| [Any::getOptional](#api-151) | `Option<T mut&> getOptional()` |
| [eq](#api-152) | `bool eq(T left,T right)` |
| [ne](#api-153) | `bool ne(T left,T right)` |
| [lt](#api-154) | `bool lt(T left,T right)` |
| [le](#api-155) | `bool le(T left,T right)` |
| [gt](#api-156) | `bool gt(T left,T right)` |
| [ge](#api-157) | `bool ge(T left,T right)` |
| [add](#api-158) | `R add(T left,U right)` |
| [sub](#api-159) | `R sub(T left,U right)` |
| [mul](#api-160) | `R mul(T left,U right)` |
| [div](#api-161) | `R div(T left,U right)` |
| [mod](#api-162) | `R mod(T left,U right)` |
| [index](#api-163) | `T index(T const& data,int64 index)` |
| [Add::leftAdd](#api-164) | `R leftAdd(T value)` |
| [RightAdd::rightAdd](#api-165) | `R rightAdd(T value)` |
| [Subtract::leftSub](#api-166) | `R leftSub(T value)` |
| [RightSubtract::rightSub](#api-167) | `R rightSub(T value)` |
| [Multiply::leftMul](#api-168) | `R leftMul(T value)` |
| [RightMultiply::rightMul](#api-169) | `R rightMul(T value)` |
| [Divide::leftDiv](#api-170) | `R leftDiv(T value)` |
| [RightDivide::rightDiv](#api-171) | `R rightDiv(T value)` |
| [Modulo::leftMod](#api-172) | `R leftMod(T value)` |
| [RightModulo::rightMod](#api-173) | `R rightMod(T value)` |
| [Equal::leftEqual](#api-174) | `R leftEqual(T value)` |
| [RightEqual::rightEqual](#api-175) | `R rightEqual(T value)` |
| [NotEqual::leftNotEqual](#api-176) | `R leftNotEqual(T value)` |
| [RightNotEqual::rightNotEqual](#api-177) | `R rightNotEqual(T value)` |
| [Less::leftLess](#api-178) | `R leftLess(T value)` |
| [RightLess::rightLess](#api-179) | `R rightLess(T value)` |
| [LessOrEqual::leftLessEqual](#api-180) | `R leftLessEqual(T value)` |
| [RightLessOrEqual::rightLessEqual](#api-181) | `R rightLessEqual(T value)` |
| [Greater::leftGreater](#api-182) | `R leftGreater(T value)` |
| [RightGreater::rightGreater](#api-183) | `R rightGreater(T value)` |
| [GreaterOrEqual::leftGreaterEqual](#api-184) | `R leftGreaterEqual(T value)` |
| [RightGreaterOrEqual::rightGreaterEqual](#api-185) | `R rightGreaterEqual(T value)` |
| [And::leftBitAnd](#api-186) | `R leftBitAnd(T value)` |
| [RightAnd::rightBitAnd](#api-187) | `R rightBitAnd(T value)` |
| [Or::leftBitOr](#api-188) | `R leftBitOr(T value)` |
| [RightOr::rightBitOr](#api-189) | `R rightBitOr(T value)` |
| [Xor::leftBitXor](#api-190) | `R leftBitXor(T value)` |
| [RightXor::rightBitXor](#api-191) | `R rightBitXor(T value)` |
| [ShiftLeft::leftShiftLeft](#api-192) | `R leftShiftLeft(T value)` |
| [RightShiftLeft::rightShiftLeft](#api-193) | `R rightShiftLeft(T value)` |
| [ShiftRight::leftShiftRight](#api-194) | `R leftShiftRight(T value)` |
| [RightShiftRight::rightShiftRight](#api-195) | `R rightShiftRight(T value)` |
| [Negate::negate](#api-196) | `R negate(T value)` |
| [BitNot::bitNot](#api-197) | `R bitNot(T value)` |
| [Positive::positive](#api-198) | `R positive(T value)` |
| [Hash::hash](#api-199) | `uint64 hash(T value)` |
| [ExplicitConvert::explicitConvert](#api-200) | `T explicitConvert()` |
| [ImplicitConvert::implicitConvert](#api-201) | `T implicitConvert()` |
| [Index::index](#api-202) | `R index(T key)` |
| [Iterator::iterator](#api-203) | `Option<R> iterator(int64 index)` |
| [Result::mapErr](#api-204) | `Result<T,F> mapErr<F>(F fn(E) mapper)` |
| [Option::okOr](#api-205) | `Result<T,E> okOr<E>(E error)` |
| [Option::okOrElse](#api-206) | `Result<T,E> okOrElse<E>(E fn() factory)` |

<a id="api-0"></a>

### readLine（1）

**声明**

```magel
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

```magel
readLine();
```

**源码**：[source/std/lang.mgl:7](../../source/std/lang.mgl#L7)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-1"></a>

### readLines（2）

**声明**

```magel
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

```magel
readLines(N);
```

**源码**：[source/std/lang.mgl:11](../../source/std/lang.mgl#L11)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-2"></a>

### readLines（3）

**声明**

```magel
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

```magel
readLines(N);
```

**源码**：[source/std/lang.mgl:15](../../source/std/lang.mgl#L15)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-3"></a>

### toInt32（4）

**声明**

```magel
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

```magel
toInt32(value);
```

**源码**：[source/std/lang.mgl:21](../../source/std/lang.mgl#L21)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-4"></a>

### toInt32（5）

**声明**

```magel
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

```magel
toInt32(value);
```

**源码**：[source/std/lang.mgl:27](../../source/std/lang.mgl#L27)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-5"></a>

### toInt64（6）

**声明**

```magel
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

```magel
toInt64(value);
```

**源码**：[source/std/lang.mgl:29](../../source/std/lang.mgl#L29)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-6"></a>

### toInt64（7）

**声明**

```magel
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

```magel
toInt64(value);
```

**源码**：[source/std/lang.mgl:31](../../source/std/lang.mgl#L31)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-7"></a>

### float32ToBytes（8）

**声明**

```magel
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

```magel
float32ToBytes(value, bigEndian);
```

**源码**：[source/std/lang.mgl:38](../../source/std/lang.mgl#L38)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-8"></a>

### float32ToBytes（9）

**声明**

```magel
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

```magel
float32ToBytes(value);
```

**源码**：[source/std/lang.mgl:40](../../source/std/lang.mgl#L40)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-9"></a>

### bytesToFloat32（10）

**声明**

```magel
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

```magel
bytesToFloat32(bytes, bigEndian);
```

**源码**：[source/std/lang.mgl:42](../../source/std/lang.mgl#L42)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-10"></a>

### bytesToFloat32（11）

**声明**

```magel
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

```magel
bytesToFloat32(bytes);
```

**源码**：[source/std/lang.mgl:47](../../source/std/lang.mgl#L47)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-11"></a>

### bytesToFloat32（12）

**声明**

```magel
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

```magel
bytesToFloat32(bytes, count, bigEndian);
```

**源码**：[source/std/lang.mgl:49](../../source/std/lang.mgl#L49)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-12"></a>

### bytesToFloat32（13）

**声明**

```magel
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

```magel
bytesToFloat32(bytes, count);
```

**源码**：[source/std/lang.mgl:54](../../source/std/lang.mgl#L54)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-13"></a>

### bytesToFloat32（14）

**声明**

```magel
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

```magel
bytesToFloat32(bytes, bigEndian);
```

**源码**：[source/std/lang.mgl:56](../../source/std/lang.mgl#L56)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-14"></a>

### bytesToFloat32（15）

**声明**

```magel
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

```magel
bytesToFloat32(bytes);
```

**源码**：[source/std/lang.mgl:58](../../source/std/lang.mgl#L58)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-15"></a>

### float32ToBytes（16）

**声明**

```magel
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

```magel
float32ToBytes(value, bytes, count, bigEndian);
```

**源码**：[source/std/lang.mgl:60](../../source/std/lang.mgl#L60)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-16"></a>

### float32ToBytes（17）

**声明**

```magel
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

```magel
float32ToBytes(value, bytes, count);
```

**源码**：[source/std/lang.mgl:65](../../source/std/lang.mgl#L65)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-17"></a>

### float32ToBytes（18）

**声明**

```magel
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

```magel
float32ToBytes(value, bytes, bigEndian);
```

**源码**：[source/std/lang.mgl:67](../../source/std/lang.mgl#L67)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-18"></a>

### float32ToBytes（19）

**声明**

```magel
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

```magel
float32ToBytes(value, bytes);
```

**源码**：[source/std/lang.mgl:72](../../source/std/lang.mgl#L72)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-19"></a>

### float64ToBytes（20）

**声明**

```magel
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

```magel
float64ToBytes(value, bigEndian);
```

**源码**：[source/std/lang.mgl:79](../../source/std/lang.mgl#L79)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-20"></a>

### float64ToBytes（21）

**声明**

```magel
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

```magel
float64ToBytes(value);
```

**源码**：[source/std/lang.mgl:81](../../source/std/lang.mgl#L81)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-21"></a>

### bytesToFloat64（22）

**声明**

```magel
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

```magel
bytesToFloat64(bytes, bigEndian);
```

**源码**：[source/std/lang.mgl:83](../../source/std/lang.mgl#L83)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-22"></a>

### bytesToFloat64（23）

**声明**

```magel
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

```magel
bytesToFloat64(bytes);
```

**源码**：[source/std/lang.mgl:88](../../source/std/lang.mgl#L88)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-23"></a>

### bytesToFloat64（24）

**声明**

```magel
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

```magel
bytesToFloat64(bytes, count, bigEndian);
```

**源码**：[source/std/lang.mgl:90](../../source/std/lang.mgl#L90)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-24"></a>

### bytesToFloat64（25）

**声明**

```magel
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

```magel
bytesToFloat64(bytes, count);
```

**源码**：[source/std/lang.mgl:95](../../source/std/lang.mgl#L95)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-25"></a>

### bytesToFloat64（26）

**声明**

```magel
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

```magel
bytesToFloat64(bytes, bigEndian);
```

**源码**：[source/std/lang.mgl:97](../../source/std/lang.mgl#L97)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-26"></a>

### bytesToFloat64（27）

**声明**

```magel
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

```magel
bytesToFloat64(bytes);
```

**源码**：[source/std/lang.mgl:99](../../source/std/lang.mgl#L99)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-27"></a>

### float64ToBytes（28）

**声明**

```magel
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

```magel
float64ToBytes(value, bytes, count, bigEndian);
```

**源码**：[source/std/lang.mgl:101](../../source/std/lang.mgl#L101)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-28"></a>

### float64ToBytes（29）

**声明**

```magel
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

```magel
float64ToBytes(value, bytes, count);
```

**源码**：[source/std/lang.mgl:106](../../source/std/lang.mgl#L106)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-29"></a>

### float64ToBytes（30）

**声明**

```magel
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

```magel
float64ToBytes(value, bytes, bigEndian);
```

**源码**：[source/std/lang.mgl:108](../../source/std/lang.mgl#L108)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-30"></a>

### float64ToBytes（31）

**声明**

```magel
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

```magel
float64ToBytes(value, bytes);
```

**源码**：[source/std/lang.mgl:113](../../source/std/lang.mgl#L113)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-31"></a>

### assert（32）

**声明**

```magel
void assert(bool condition);
```

**行为**

condition 为真时继续，为假时以指定诊断文本和可选行号触发 panic。

中止统一走 std::lang::panic：编译器与虚拟机里只有 `__vm_panic` 这一个停机原语， `exit(n)` 与内建 `assert(x)` 都已删除。需要断言时用下面这几个函数 —— 它们自己 调 `std::lang::panic`，所以停机清理只有一条入口；带消息的重载让标准库自己的前置检查 也能给出人话（例如 range 的步长）。 带 `int64 line` 的重载给文件末尾的系统宏 `$assert(...)` 用：宏把调用点的 `$line` 传进来，失败时就地报出**源码行号**，不必每处自己写文案。

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

```magel
assert(condition);
```

**源码**：[source/std/lang.mgl:126](../../source/std/lang.mgl#L126)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-32"></a>

### assert（33）

**声明**

```magel
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

```magel
assert(condition, message);
```

**源码**：[source/std/lang.mgl:130](../../source/std/lang.mgl#L130)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-33"></a>

### assert（34）

**声明**

```magel
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

```magel
assert(condition, line);
```

**源码**：[source/std/lang.mgl:134](../../source/std/lang.mgl#L134)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-34"></a>

### assert（35）

**声明**

```magel
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

```magel
assert(condition, message, line);
```

**源码**：[source/std/lang.mgl:138](../../source/std/lang.mgl#L138)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-35"></a>

### Try::branch（36）

**声明**

```magel
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

```magel
obj.branch();
```

**源码**：[source/std/lang.mgl:149](../../source/std/lang.mgl#L149)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-36"></a>

### TryMove::intoBranch（37）

**声明**

```magel
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

```magel
obj.intoBranch();
```

**源码**：[source/std/lang.mgl:151](../../source/std/lang.mgl#L151)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-37"></a>

### FromResidual::fromResidual（38）

**声明**

```magel
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

```magel
FromResidual<F, R>::fromResidual(residual);
```

**源码**：[source/std/lang.mgl:153](../../source/std/lang.mgl#L153)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-38"></a>

### Option::branch（39）

**声明**

```magel
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

```magel
obj.branch();
```

**源码**：[source/std/lang.mgl:158](../../source/std/lang.mgl#L158)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-39"></a>

### Option::intoBranch（40）

**声明**

```magel
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

```magel
obj.intoBranch();
```

**源码**：[source/std/lang.mgl:164](../../source/std/lang.mgl#L164)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-40"></a>

### Option::fromResidual（41）

**声明**

```magel
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

```magel
Option<T>::fromResidual(residual);
```

**源码**：[source/std/lang.mgl:165](../../source/std/lang.mgl#L165)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-41"></a>

### Option::isValue（42）

**声明**

```magel
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

```magel
obj.isValue();
```

**源码**：[source/std/lang.mgl:166](../../source/std/lang.mgl#L166)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-42"></a>

### Option::isNone（43）

**声明**

```magel
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

```magel
obj.isNone();
```

**源码**：[source/std/lang.mgl:175](../../source/std/lang.mgl#L175)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-43"></a>

### Option::unwrap（44）

**声明**

```magel
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

```magel
obj.unwrap();
```

**源码**：[source/std/lang.mgl:177](../../source/std/lang.mgl#L177)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-44"></a>

### Option::unwrapOr（45）

**声明**

```magel
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

```magel
obj.unwrapOr(fallback);
```

**源码**：[source/std/lang.mgl:186](../../source/std/lang.mgl#L186)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-45"></a>

### Option::expect（46）

**声明**

```magel
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

```magel
obj.expect(message);
```

**源码**：[source/std/lang.mgl:195](../../source/std/lang.mgl#L195)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-46"></a>

### Option::transform（47）

**声明**

```magel
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

```magel
obj.transform(mapper);
```

**源码**：[source/std/lang.mgl:204](../../source/std/lang.mgl#L204)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-47"></a>

### Option::andThen（48）

**声明**

```magel
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

```magel
obj.andThen(mapper);
```

**源码**：[source/std/lang.mgl:213](../../source/std/lang.mgl#L213)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-48"></a>

### Result::branch（49）

**声明**

```magel
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

```magel
obj.branch();
```

**源码**：[source/std/lang.mgl:226](../../source/std/lang.mgl#L226)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-49"></a>

### Result::intoBranch（50）

**声明**

```magel
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

```magel
obj.intoBranch();
```

**源码**：[source/std/lang.mgl:236](../../source/std/lang.mgl#L236)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-50"></a>

### Result::fromResidual（51）

**声明**

```magel
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

```magel
Result<T, E>::fromResidual(residual);
```

**源码**：[source/std/lang.mgl:237](../../source/std/lang.mgl#L237)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-51"></a>

### Result::ok（52）

**声明**

```magel
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

```magel
obj.ok();
```

**源码**：[source/std/lang.mgl:238](../../source/std/lang.mgl#L238)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-52"></a>

### Result::isOk（53）

**声明**

```magel
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

```magel
obj.isOk();
```

**源码**：[source/std/lang.mgl:244](../../source/std/lang.mgl#L244)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-53"></a>

### Result::isErr（54）

**声明**

```magel
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

```magel
obj.isErr();
```

**源码**：[source/std/lang.mgl:253](../../source/std/lang.mgl#L253)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-54"></a>

### Result::unwrap（55）

**声明**

```magel
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

```magel
obj.unwrap();
```

**源码**：[source/std/lang.mgl:255](../../source/std/lang.mgl#L255)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-55"></a>

### Result::unwrapOr（56）

**声明**

```magel
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

```magel
obj.unwrapOr(fallback);
```

**源码**：[source/std/lang.mgl:264](../../source/std/lang.mgl#L264)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-56"></a>

### Result::expect（57）

**声明**

```magel
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

```magel
obj.expect(message);
```

**源码**：[source/std/lang.mgl:273](../../source/std/lang.mgl#L273)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-57"></a>

### Result::error（58）

**声明**

```magel
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

```magel
obj.error();
```

**源码**：[source/std/lang.mgl:282](../../source/std/lang.mgl#L282)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-58"></a>

### Result::transform（59）

**声明**

```magel
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

```magel
obj.transform(mapper);
```

**源码**：[source/std/lang.mgl:291](../../source/std/lang.mgl#L291)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-59"></a>

### Result::andThen（60）

**声明**

```magel
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

```magel
obj.andThen(mapper);
```

**源码**：[source/std/lang.mgl:300](../../source/std/lang.mgl#L300)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-60"></a>

### Pair::of（61）

**声明**

```magel
static Pair<K, V> of(K first, V second);
```

**行为**

按 first、second 创建二元值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `first` | `K first` | 第一分量 | 无 |
| `second` | `V second` | 第二分量 | 无 |

**返回值**

`Pair<K, V>`。按 first、second 创建二元值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
Pair<K, V>::of(first, second);
```

**源码**：[source/std/lang.mgl:314](../../source/std/lang.mgl#L314)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-61"></a>

### tryGet（62）

**声明**

```magel
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

```magel
tryGet<T>(value);
```

**源码**：[source/std/lang.mgl:341](../../source/std/lang.mgl#L341)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-62"></a>

### tryGetReference（63）

**声明**

```magel
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

```magel
tryGetReference<T>(value);
```

**源码**：[source/std/lang.mgl:347](../../source/std/lang.mgl#L347)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-63"></a>

### tryGetMut（64）

**声明**

```magel
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

```magel
tryGetMut<T>(value);
```

**源码**：[source/std/lang.mgl:352](../../source/std/lang.mgl#L352)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-64"></a>

### stringBytes（65）

**声明**

```magel
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

```magel
stringBytes(text);
```

**源码**：[source/std/lang.mgl:359](../../source/std/lang.mgl#L359)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-65"></a>

### range（66）

**声明**

```magel
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

```magel
range(start, stop);
```

**源码**：[source/magel_generic.hpp:1682](../../source/magel_generic.hpp#L1682)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-66"></a>

### range（67）

**声明**

```magel
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

```magel
range(start, stop, step);
```

**源码**：[source/magel_generic.hpp:1690](../../source/magel_generic.hpp#L1690)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-67"></a>

### Drop::drop（68）

**声明**

```magel
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

```magel
obj.drop();
```

**源码**：[source/magel_generic.hpp:1900](../../source/magel_generic.hpp#L1900)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-68"></a>

### Closeable::close（69）

**声明**

```magel
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

```magel
obj.close();
```

**源码**：[source/magel_generic.hpp:1908](../../source/magel_generic.hpp#L1908)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-69"></a>

### print（70）

**声明**

```magel
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

```magel
print(format, args);
```

**源码**：[source/epilogue/boxing.cpp:1196](../../source/epilogue/boxing.cpp#L1196)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-70"></a>

### println（71）

**声明**

```magel
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

```magel
println(format, args);
```

**源码**：[source/epilogue/boxing.cpp:1196](../../source/epilogue/boxing.cpp#L1196)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-71"></a>

### format（72）

**声明**

```magel
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

```magel
format(format, args);
```

**源码**：[source/epilogue/boxing.cpp:1200](../../source/epilogue/boxing.cpp#L1200)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-72"></a>

### putChar（73）

**声明**

```magel
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

```magel
putChar(value);
```

**源码**：[source/epilogue/boxing.cpp:1246](../../source/epilogue/boxing.cpp#L1246)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-73"></a>

### pause（74）

**声明**

```magel
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

```magel
pause();
```

**源码**：[source/epilogue/boxing.cpp:1207](../../source/epilogue/boxing.cpp#L1207)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-74"></a>

### sweep（75）

**声明**

```magel
int64 sweep();
```

**行为**

清理全部已释放块的墓碑身份，返回清理身份数。调用者必须保证不存在仍需使用的已释放块旧引用；清理后重复 delete 报 MGL1205。保留活跃分配及其未来的幂等释放保证。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。清理全部已释放块的墓碑身份，返回清理身份数。调用者必须保证不存在仍需使用的已释放块旧引用；清理后重复 delete 报 MGL1205。保留活跃分配及其未来的幂等释放保证。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
sweep();
```

**源码**：[source/epilogue/boxing.cpp:1256](../../source/epilogue/boxing.cpp#L1256)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-75"></a>

### inputLine（76）

**声明**

```magel
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

```magel
inputLine();
```

**源码**：[source/epilogue/boxing.cpp:1261](../../source/epilogue/boxing.cpp#L1261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-76"></a>

### inputInt（77）

**声明**

```magel
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

```magel
inputInt();
```

**源码**：[source/epilogue/boxing.cpp:1261](../../source/epilogue/boxing.cpp#L1261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-77"></a>

### inputUInt（78）

**声明**

```magel
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

```magel
inputUInt();
```

**源码**：[source/epilogue/boxing.cpp:1261](../../source/epilogue/boxing.cpp#L1261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-78"></a>

### inputFloat（79）

**声明**

```magel
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

```magel
inputFloat();
```

**源码**：[source/epilogue/boxing.cpp:1261](../../source/epilogue/boxing.cpp#L1261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-79"></a>

### inputBool（80）

**声明**

```magel
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

```magel
inputBool();
```

**源码**：[source/epilogue/boxing.cpp:1261](../../source/epilogue/boxing.cpp#L1261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-80"></a>

### inputChar（81）

**声明**

```magel
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

```magel
inputChar();
```

**源码**：[source/epilogue/boxing.cpp:1261](../../source/epilogue/boxing.cpp#L1261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-81"></a>

### inputBigInteger（82）

**声明**

```magel
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

```magel
inputBigInteger();
```

**源码**：[source/epilogue/boxing.cpp:1261](../../source/epilogue/boxing.cpp#L1261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-82"></a>

### inputBigDecimal（83）

**声明**

```magel
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

```magel
inputBigDecimal();
```

**源码**：[source/epilogue/boxing.cpp:1261](../../source/epilogue/boxing.cpp#L1261)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-83"></a>

### panic（84）

**声明**

```magel
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

```magel
panic(message);
```

**源码**：[source/epilogue/boxing.cpp:1280](../../source/epilogue/boxing.cpp#L1280)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-84"></a>

### len（85）

**声明**

```magel
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

```magel
len(value);
```

**源码**：[source/epilogue/boxing.cpp:626](../../source/epilogue/boxing.cpp#L626)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-85"></a>

### move（86）

**声明**

```magel
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

```magel
move<T>(value);
```

**源码**：[source/epilogue/boxing.cpp:555](../../source/epilogue/boxing.cpp#L555)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-86"></a>

### toReference（87）

**声明**

```magel
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

```magel
toReference<T>(value);
```

**源码**：[source/magel_generic.hpp:1630](../../source/magel_generic.hpp#L1630)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-87"></a>

### toOptional（88）

**声明**

```magel
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

```magel
toOptional<T>(value);
```

**源码**：[source/magel_generic.hpp:1636](../../source/magel_generic.hpp#L1636)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-88"></a>

### memcpy（89）

**声明**

```magel
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

```magel
memcpy(destination, source, bytes);
```

**源码**：[source/epilogue/boxing.cpp:1297](../../source/epilogue/boxing.cpp#L1297)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-89"></a>

### memset（90）

**声明**

```magel
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

```magel
memset(destination, value, bytes);
```

**源码**：[source/epilogue/boxing.cpp:1297](../../source/epilogue/boxing.cpp#L1297)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-90"></a>

### List::copy（91）

**声明**

```magel
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

```magel
obj.copy();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:856](../../source/epilogue/aggregate_ownership.cpp#L856)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-91"></a>

### List::push（92）

**声明**

```magel
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

```magel
obj.push(value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:857](../../source/epilogue/aggregate_ownership.cpp#L857)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-92"></a>

### List::insert（93）

**声明**

```magel
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

```magel
obj.insert(index, value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:858](../../source/epilogue/aggregate_ownership.cpp#L858)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-93"></a>

### List::at（94）

**声明**

```magel
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

```magel
obj.at(index);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:859](../../source/epilogue/aggregate_ownership.cpp#L859)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-94"></a>

### List::pop（95）

**声明**

```magel
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

```magel
obj.pop();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:860](../../source/epilogue/aggregate_ownership.cpp#L860)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-95"></a>

### List::remove（96）

**声明**

```magel
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

```magel
obj.remove(index);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:861](../../source/epilogue/aggregate_ownership.cpp#L861)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-96"></a>

### List::indexOf（97）

**声明**

```magel
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

```magel
obj.indexOf(value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:862](../../source/epilogue/aggregate_ownership.cpp#L862)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-97"></a>

### List::contains（98）

**声明**

```magel
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

```magel
obj.contains(value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:863](../../source/epilogue/aggregate_ownership.cpp#L863)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-98"></a>

### List::has（99）

**声明**

```magel
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

```magel
obj.has(value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:864](../../source/epilogue/aggregate_ownership.cpp#L864)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-99"></a>

### List::clear（100）

**声明**

```magel
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

```magel
obj.clear();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:865](../../source/epilogue/aggregate_ownership.cpp#L865)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-100"></a>

### List::len（101）

**声明**

```magel
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

```magel
obj.len();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:866](../../source/epilogue/aggregate_ownership.cpp#L866)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-101"></a>

### Set::copy（102）

**声明**

```magel
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

```magel
obj.copy();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:856](../../source/epilogue/aggregate_ownership.cpp#L856)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-102"></a>

### Set::add（103）

**声明**

```magel
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

```magel
obj.add(value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:874](../../source/epilogue/aggregate_ownership.cpp#L874)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-103"></a>

### Set::has（104）

**声明**

```magel
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

```magel
obj.has(value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:864](../../source/epilogue/aggregate_ownership.cpp#L864)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-104"></a>

### Set::contains（105）

**声明**

```magel
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

```magel
obj.contains(value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:863](../../source/epilogue/aggregate_ownership.cpp#L863)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-105"></a>

### Set::remove（106）

**声明**

```magel
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

```magel
obj.remove(value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:861](../../source/epilogue/aggregate_ownership.cpp#L861)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-106"></a>

### Set::at（107）

**声明**

```magel
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

```magel
obj.at(index);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:859](../../source/epilogue/aggregate_ownership.cpp#L859)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-107"></a>

### Set::clear（108）

**声明**

```magel
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

```magel
obj.clear();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:865](../../source/epilogue/aggregate_ownership.cpp#L865)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-108"></a>

### Set::len（109）

**声明**

```magel
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

```magel
obj.len();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:866](../../source/epilogue/aggregate_ownership.cpp#L866)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-109"></a>

### Map::copy（110）

**声明**

```magel
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

```magel
obj.copy();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:856](../../source/epilogue/aggregate_ownership.cpp#L856)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-110"></a>

### Map::put（111）

**声明**

```magel
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

```magel
obj.put(key, value);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:888](../../source/epilogue/aggregate_ownership.cpp#L888)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-111"></a>

### Map::has（112）

**声明**

```magel
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

```magel
obj.has(key);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:864](../../source/epilogue/aggregate_ownership.cpp#L864)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-112"></a>

### Map::contains（113）

**声明**

```magel
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

```magel
obj.contains(key);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:863](../../source/epilogue/aggregate_ownership.cpp#L863)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-113"></a>

### Map::remove（114）

**声明**

```magel
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

```magel
obj.remove(key);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:861](../../source/epilogue/aggregate_ownership.cpp#L861)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-114"></a>

### Map::keyAt（115）

**声明**

```magel
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

```magel
obj.keyAt(index);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:892](../../source/epilogue/aggregate_ownership.cpp#L892)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-115"></a>

### Map::valAt（116）

**声明**

```magel
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

```magel
obj.valAt(index);
```

**源码**：[source/epilogue/aggregate_ownership.cpp:893](../../source/epilogue/aggregate_ownership.cpp#L893)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-116"></a>

### Map::clear（117）

**声明**

```magel
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

```magel
obj.clear();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:865](../../source/epilogue/aggregate_ownership.cpp#L865)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-117"></a>

### Map::len（118）

**声明**

```magel
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

```magel
obj.len();
```

**源码**：[source/epilogue/aggregate_ownership.cpp:866](../../source/epilogue/aggregate_ownership.cpp#L866)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-118"></a>

### BigInteger::str（119）

**声明**

```magel
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

```magel
obj.str();
```

**源码**：[source/epilogue/boxing.cpp:729](../../source/epilogue/boxing.cpp#L729)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-119"></a>

### BigInteger::copy（120）

**声明**

```magel
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

```magel
obj.copy();
```

**源码**：[source/epilogue/boxing.cpp:719](../../source/epilogue/boxing.cpp#L719)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-120"></a>

### BigInteger::pow（121）

**声明**

```magel
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

```magel
obj.pow(exponent);
```

**源码**：[source/epilogue/boxing.cpp:744](../../source/epilogue/boxing.cpp#L744)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-121"></a>

### BigInteger::powmod（122）

**声明**

```magel
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

```magel
obj.powmod(exponent, modulus);
```

**源码**：[source/epilogue/boxing.cpp:750](../../source/epilogue/boxing.cpp#L750)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-122"></a>

### BigInteger::gcd（123）

**声明**

```magel
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

```magel
obj.gcd(other);
```

**源码**：[source/epilogue/boxing.cpp:756](../../source/epilogue/boxing.cpp#L756)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-123"></a>

### BigInteger::lcm（124）

**声明**

```magel
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

```magel
obj.lcm(other);
```

**源码**：[source/epilogue/boxing.cpp:756](../../source/epilogue/boxing.cpp#L756)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-124"></a>

### BigInteger::mod（125）

**声明**

```magel
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

```magel
obj.mod(modulus);
```

**源码**：[source/epilogue/boxing.cpp:764](../../source/epilogue/boxing.cpp#L764)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-125"></a>

### BigInteger::modInverse（126）

**声明**

```magel
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

```magel
obj.modInverse(modulus);
```

**源码**：[source/epilogue/boxing.cpp:770](../../source/epilogue/boxing.cpp#L770)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-126"></a>

### BigInteger::isProbablePrime（127）

**声明**

```magel
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

```magel
obj.isProbablePrime(rounds);
```

**源码**：[source/epilogue/boxing.cpp:776](../../source/epilogue/boxing.cpp#L776)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-127"></a>

### BigInteger::nextProbablePrime（128）

**声明**

```magel
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

```magel
obj.nextProbablePrime();
```

**源码**：[source/epilogue/boxing.cpp:782](../../source/epilogue/boxing.cpp#L782)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-128"></a>

### BigInteger::bitLength（129）

**声明**

```magel
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

```magel
obj.bitLength();
```

**源码**：[source/epilogue/boxing.cpp:788](../../source/epilogue/boxing.cpp#L788)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-129"></a>

### BigInteger::bitAnd（130）

**声明**

```magel
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

```magel
obj.bitAnd(other);
```

**源码**：[source/epilogue/boxing.cpp:794](../../source/epilogue/boxing.cpp#L794)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-130"></a>

### BigInteger::bitOr（131）

**声明**

```magel
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

```magel
obj.bitOr(other);
```

**源码**：[source/epilogue/boxing.cpp:794](../../source/epilogue/boxing.cpp#L794)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-131"></a>

### BigInteger::bitXor（132）

**声明**

```magel
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

```magel
obj.bitXor(other);
```

**源码**：[source/epilogue/boxing.cpp:794](../../source/epilogue/boxing.cpp#L794)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-132"></a>

### BigInteger::strRadix（133）

**声明**

```magel
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

```magel
obj.strRadix(base);
```

**源码**：[source/epilogue/boxing.cpp:805](../../source/epilogue/boxing.cpp#L805)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-133"></a>

### BigInteger::iroot（134）

**声明**

```magel
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

```magel
obj.iroot(k);
```

**源码**：[source/epilogue/boxing.cpp:811](../../source/epilogue/boxing.cpp#L811)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-134"></a>

### BigDecimal::str（135）

**声明**

```magel
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

```magel
obj.str();
```

**源码**：[source/epilogue/boxing.cpp:729](../../source/epilogue/boxing.cpp#L729)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-135"></a>

### BigDecimal::copy（136）

**声明**

```magel
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

```magel
obj.copy();
```

**源码**：[source/epilogue/boxing.cpp:719](../../source/epilogue/boxing.cpp#L719)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-136"></a>

### BigDecimal::setPrecision（137）

**声明**

```magel
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

```magel
obj.setPrecision(digits);
```

**源码**：[source/epilogue/boxing.cpp:708](../../source/epilogue/boxing.cpp#L708)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-137"></a>

### BigDecimal::pow（138）

**声明**

```magel
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

```magel
obj.pow(exponent);
```

**源码**：[source/epilogue/boxing.cpp:744](../../source/epilogue/boxing.cpp#L744)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-138"></a>

### BigDecimal::abs（139）

**声明**

```magel
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

```magel
obj.abs();
```

**源码**：[source/epilogue/boxing.cpp:827](../../source/epilogue/boxing.cpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-139"></a>

### BigDecimal::isInteger（140）

**声明**

```magel
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

```magel
obj.isInteger();
```

**源码**：[source/epilogue/boxing.cpp:833](../../source/epilogue/boxing.cpp#L833)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-140"></a>

### BigDecimal::floor（141）

**声明**

```magel
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

```magel
obj.floor();
```

**源码**：[source/epilogue/boxing.cpp:839](../../source/epilogue/boxing.cpp#L839)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-141"></a>

### BigDecimal::ceil（142）

**声明**

```magel
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

```magel
obj.ceil();
```

**源码**：[source/epilogue/boxing.cpp:839](../../source/epilogue/boxing.cpp#L839)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-142"></a>

### BigDecimal::trunc（143）

**声明**

```magel
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

```magel
obj.trunc();
```

**源码**：[source/epilogue/boxing.cpp:839](../../source/epilogue/boxing.cpp#L839)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-143"></a>

### BigDecimal::sqrt（144）

**声明**

```magel
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

```magel
obj.sqrt(digits);
```

**源码**：[source/epilogue/boxing.cpp:849](../../source/epilogue/boxing.cpp#L849)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-144"></a>

### BigDecimal::root（145）

**声明**

```magel
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

```magel
obj.root(k, digits);
```

**源码**：[source/epilogue/boxing.cpp:855](../../source/epilogue/boxing.cpp#L855)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-145"></a>

### Any::copy（146）

**声明**

```magel
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

```magel
obj.copy();
```

**源码**：[source/epilogue/boxing.cpp:719](../../source/epilogue/boxing.cpp#L719)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-146"></a>

### Any::isEmpty（147）

**声明**

```magel
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

```magel
obj.isEmpty();
```

**源码**：[source/epilogue/boxing.cpp:881](../../source/epilogue/boxing.cpp#L881)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-147"></a>

### Any::clear（148）

**声明**

```magel
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

```magel
obj.clear();
```

**源码**：[source/epilogue/boxing.cpp:888](../../source/epilogue/boxing.cpp#L888)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-148"></a>

### Any::set（149）

**声明**

```magel
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

```magel
obj.set(value);
```

**源码**：[source/epilogue/boxing.cpp:904](../../source/epilogue/boxing.cpp#L904)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-149"></a>

### Any::getValue（150）

**声明**

```magel
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

```magel
obj.getValue();
```

**源码**：[source/epilogue/boxing.cpp:130](../../source/epilogue/boxing.cpp#L130)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-150"></a>

### Any::getReference（151）

**声明**

```magel
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

```magel
obj.getReference();
```

**源码**：[source/epilogue/boxing.cpp:130](../../source/epilogue/boxing.cpp#L130)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-151"></a>

### Any::getOptional（152）

**声明**

```magel
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

```magel
obj.getOptional();
```

**源码**：[source/epilogue/boxing.cpp:130](../../source/epilogue/boxing.cpp#L130)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-152"></a>

### eq（153）

**声明**

```magel
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

```magel
eq(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-153"></a>

### ne（154）

**声明**

```magel
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

```magel
ne(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-154"></a>

### lt（155）

**声明**

```magel
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

```magel
lt(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-155"></a>

### le（156）

**声明**

```magel
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

```magel
le(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-156"></a>

### gt（157）

**声明**

```magel
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

```magel
gt(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-157"></a>

### ge（158）

**声明**

```magel
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

```magel
ge(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-158"></a>

### add（159）

**声明**

```magel
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

```magel
add(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1061](../../source/epilogue/boxing.cpp#L1061)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-159"></a>

### sub（160）

**声明**

```magel
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

```magel
sub(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-160"></a>

### mul（161）

**声明**

```magel
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

```magel
mul(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-161"></a>

### div（162）

**声明**

```magel
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

```magel
div(left, right);
```

**源码**：[source/epilogue/boxing.cpp:1](../../source/epilogue/boxing.cpp#L1)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-162"></a>

### mod（163）

**声明**

```magel
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

```magel
mod(left, right);
```

**源码**：[source/epilogue/boxing.cpp:764](../../source/epilogue/boxing.cpp#L764)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-163"></a>

### index（164）

**声明**

```magel
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

```magel
index(data, index);
```

**源码**：[source/epilogue/boxing.cpp:1643](../../source/epilogue/boxing.cpp#L1643)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-164"></a>

### Add::leftAdd（165）

**声明**

```magel
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

```magel
obj.leftAdd(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-165"></a>

### RightAdd::rightAdd（166）

**声明**

```magel
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

```magel
obj.rightAdd(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-166"></a>

### Subtract::leftSub（167）

**声明**

```magel
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

```magel
obj.leftSub(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-167"></a>

### RightSubtract::rightSub（168）

**声明**

```magel
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

```magel
obj.rightSub(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-168"></a>

### Multiply::leftMul（169）

**声明**

```magel
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

```magel
obj.leftMul(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-169"></a>

### RightMultiply::rightMul（170）

**声明**

```magel
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

```magel
obj.rightMul(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-170"></a>

### Divide::leftDiv（171）

**声明**

```magel
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

```magel
obj.leftDiv(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-171"></a>

### RightDivide::rightDiv（172）

**声明**

```magel
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

```magel
obj.rightDiv(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-172"></a>

### Modulo::leftMod（173）

**声明**

```magel
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

```magel
obj.leftMod(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-173"></a>

### RightModulo::rightMod（174）

**声明**

```magel
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

```magel
obj.rightMod(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-174"></a>

### Equal::leftEqual（175）

**声明**

```magel
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

```magel
obj.leftEqual(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-175"></a>

### RightEqual::rightEqual（176）

**声明**

```magel
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

```magel
obj.rightEqual(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-176"></a>

### NotEqual::leftNotEqual（177）

**声明**

```magel
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

```magel
obj.leftNotEqual(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-177"></a>

### RightNotEqual::rightNotEqual（178）

**声明**

```magel
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

```magel
obj.rightNotEqual(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-178"></a>

### Less::leftLess（179）

**声明**

```magel
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

```magel
obj.leftLess(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-179"></a>

### RightLess::rightLess（180）

**声明**

```magel
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

```magel
obj.rightLess(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-180"></a>

### LessOrEqual::leftLessEqual（181）

**声明**

```magel
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

```magel
obj.leftLessEqual(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-181"></a>

### RightLessOrEqual::rightLessEqual（182）

**声明**

```magel
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

```magel
obj.rightLessEqual(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-182"></a>

### Greater::leftGreater（183）

**声明**

```magel
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

```magel
obj.leftGreater(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-183"></a>

### RightGreater::rightGreater（184）

**声明**

```magel
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

```magel
obj.rightGreater(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-184"></a>

### GreaterOrEqual::leftGreaterEqual（185）

**声明**

```magel
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

```magel
obj.leftGreaterEqual(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-185"></a>

### RightGreaterOrEqual::rightGreaterEqual（186）

**声明**

```magel
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

```magel
obj.rightGreaterEqual(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-186"></a>

### And::leftBitAnd（187）

**声明**

```magel
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

```magel
obj.leftBitAnd(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-187"></a>

### RightAnd::rightBitAnd（188）

**声明**

```magel
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

```magel
obj.rightBitAnd(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-188"></a>

### Or::leftBitOr（189）

**声明**

```magel
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

```magel
obj.leftBitOr(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-189"></a>

### RightOr::rightBitOr（190）

**声明**

```magel
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

```magel
obj.rightBitOr(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-190"></a>

### Xor::leftBitXor（191）

**声明**

```magel
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

```magel
obj.leftBitXor(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-191"></a>

### RightXor::rightBitXor（192）

**声明**

```magel
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

```magel
obj.rightBitXor(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-192"></a>

### ShiftLeft::leftShiftLeft（193）

**声明**

```magel
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

```magel
obj.leftShiftLeft(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-193"></a>

### RightShiftLeft::rightShiftLeft（194）

**声明**

```magel
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

```magel
obj.rightShiftLeft(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-194"></a>

### ShiftRight::leftShiftRight（195）

**声明**

```magel
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

```magel
obj.leftShiftRight(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-195"></a>

### RightShiftRight::rightShiftRight（196）

**声明**

```magel
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

```magel
obj.rightShiftRight(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-196"></a>

### Negate::negate（197）

**声明**

```magel
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

```magel
obj.negate(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-197"></a>

### BitNot::bitNot（198）

**声明**

```magel
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

```magel
obj.bitNot(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-198"></a>

### Positive::positive（199）

**声明**

```magel
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

```magel
obj.positive(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-199"></a>

### Hash::hash（200）

**声明**

```magel
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

```magel
obj.hash(value);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-200"></a>

### ExplicitConvert::explicitConvert（201）

**声明**

```magel
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

```magel
obj.explicitConvert();
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-201"></a>

### ImplicitConvert::implicitConvert（202）

**声明**

```magel
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

```magel
obj.implicitConvert();
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-202"></a>

### Index::index（203）

**声明**

```magel
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

```magel
obj.index(key);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-203"></a>

### Iterator::iterator（204）

**声明**

```magel
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

```magel
obj.iterator(index);
```

**源码**：[source/magel_generic.hpp:827](../../source/magel_generic.hpp#L827)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-204"></a>

### Result::mapErr（205）

**声明**

```magel
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

```magel
obj.mapErr<F>(mapper);
```

**源码**：[source/std/lang.mgl:2](../../source/std/lang.mgl#L2)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-205"></a>

### Option::okOr（206）

**声明**

```magel
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

```magel
obj.okOr<E>(error);
```

**源码**：[source/std/lang.mgl:3](../../source/std/lang.mgl#L3)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-206"></a>

### Option::okOrElse（207）

**声明**

```magel
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

```magel
obj.okOrElse<E>(factory);
```

**源码**：[source/std/lang.mgl:3](../../source/std/lang.mgl#L3)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

## 补充完整示例

[example/samples/std_read_lines.mgl](../../example/samples/std_read_lines.mgl)。

```magel
int64 main() {
    String first=std::lang::readLine();
    String rest=std::lang::readLines(3);
    println("[{}]",first);
    println("[{}]",rest);
    assert(std::lang::readLines(0)=="");
    assert(std::lang::readLine()=="");
    return 0;
}
```
