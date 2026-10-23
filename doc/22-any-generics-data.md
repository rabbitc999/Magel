# Any、显式泛型与通用数据工具

[返回文档目录](README.md) · [Any、显式泛型与通用数据工具](en/22-any-generics-data.md)

编译器与运行时资产必须匹配；修改类型契约后需重新编译源码、生成宿主胶水并重建缓存。

## 集合存储与遍历顺序

公开 API 仍为 `List<T>`、`Set<T>` 和 `Map<K,V>`，类型参数、字面量及值语义保持一致。
`List<T>` 采用 ArrayList，按下标和插入位置保存元素，复制时产生独立的拥有值。
`Map` 与 `Set` 根据键协议选择存储：内建标量键具有内建哈希；自定义键实现 `Hash<T>` 时
使用 HashMap / HashSet，哈希相同仍通过 `Equal<bool,T>` 判断是否为同一个键。
自定义键只有 `Equal<bool,T>` 而没有 `Hash<T>` 时自动使用 ArrayMap / ArraySet，按数组线性查找。
不具有 Hash 的复合 Set 元素同样使用数组存储，沿用其已有的值或身份比较规则。

Map、Set 不保证键排序、插入顺序或删除前后的遍历顺序；`keyAt`、`valAt`、`at`、for-in
与默认 `toString()` 使用当前存储的遍历位置。一次未修改的 Map 中，相同位置的 `keyAt`
和 `valAt` 对应同一个条目。需要固定展示顺序时，应先收集到 List 并显式排序。
复制、清空、字段默认初始化与消费式迭代保留键策略；元素借用期间仍禁止移动或删除列表元素。
实现与回归见 [集合存储样例](../example/samples/collection_storage.chl)。

## Any 与类型判断

公开类型 `Object` 改为 `Any`，核心模块自动加载，通常直接写 `Any`。装箱的复制、析构、引用和接口转换继续沿用值语义与借用检查；更名不允许把局部对象地址保存到更长寿命的盒子。

`nameof` 与盒子的 `.name()` 不属于应用 API。运行时内部标签只在一次编译及其匹配运行时内有意义，不应写入文件、网络协议或数据库。

```chali
void main() {
    Any value;
    assert(value.isEmpty());
    value.set((int64)7);
    assert(isType<int64>(&value));
    Option<int64> number = tryGet<int64>(&value);
    assert(number.unwrap() == 7);
    assert(!tryGet<String>(&value).isValue());
    println("Any: {}", number.unwrap());

}
```

`isType<T>` 判断保存的精确类型，不隐式进行数值转换。`tryGet<T>` 返回拥有值的副本；只读／可写引用使用对应的借用接口，引用不能超过盒子和原始对象的寿命。接口借用可以接受实现该接口的目标，遵守现有接收者可变性规则。已知提取一定成功时仍可使用 `getValue`／`getReference`；不匹配会报告运行错误。

需要稳定的元数据身份、字段、方法或注解时使用 `std::reflect`。其 `getTypeId<T>()` 仍要求反射构建和 `--reflection`，与 Any 的内部运行期分派标签分开。私有 `__typeTag` 仅供核心库和反射生成代码使用，应用不能直接调用。

## Any 目标转换与异构容器

目标明确为 `Any` 时，编译器自动装箱具体值。这条规则统一用于函数实参与返回值、显式 `Any` 泛型实参、泛型字段、枚举载荷，以及容器的元素／键／值；无须先声明一个 `Any` 中转变量。已有 `Any` 值按值复制，不会再嵌套装箱。用户声明的直接 `ImplicitConvert<Any>` 优先于默认装箱。

```chali
T identity<T>(T value) { return value; }
void main()
{
    List<Any> values = [1, "two", true];
    values.push(1.5);
    Any value = identity<Any>("generic");
    Map<String, Any> record = {"count": 3, "name": "ChaLi"};
    Set<Any> unique;
    Any key = "key";
    unique.add(key);
    unique.add(key);
    assert(unique.len() == 1);
    assert(isType<String>(&value));
    value.clear();
    assert(value.isEmpty());

}
```

`List<Any>`、`Set<Any>`、`Map<Any, V>`、`Map<K, Any>` 和 `Map<Any, Any>` 均可显式声明；容器字面量仍由目标类型确定种类。普通 `List<int64>` 不会因此接收字符串，省略泛型实参也不会自动推导成 `Any`。返回块／if／match 的各分支仍先确定一致的具体结果类型，再对整个结果装箱；不能用返回类型 `Any` 绕过分支一致性检查。

`Any.clear()` 需要可写接收者，释放当前拥有的载荷、恢复空状态，之后仍可 `set` 或重新赋值。装箱保留精确源类型，`int32` 与 `int64`、整数与浮点不会因装箱被合并；提取使用 `isType<T>`、`tryGet<T>` 或借用接口。临时值只求值一次，字符串和容器保存拥有值副本，清理沿用完整表达式及作用域规则，没有 GC。

**Any 键按身份比较。** `Any` 复制保留身份，所以向 `Set` 重复插入同一个盒子会去重，`Map` 用它的副本可以取回原条目。两次独立装箱同一个整数或字符串产生不同身份；`map.put("key", value)` 自动装箱的键不能靠另一次独立装箱的 `"key"` 查找。需要按字符串内容查询时优先写 `Map<String, Any>`；需要异构键时保存 `Any key` 并用它查询。

装箱不延长借用寿命。现有多槽结构体／枚举／数组按地址装箱，局部值和临时值不能借装箱逃出作用域；显式引用也受来源与逃逸检查约束。不能把 `void` 或不支持复制的宿主资源强行装箱。

## 接口引用直接绑定

```chali
interface Readable { int64 read(); };
struct Counter : Readable
{
    int64 value;
    int64 read() { return self.value; }
};
int64 consume(Readable const& view) { return view.read(); }
void main()
{
    Counter counter;
    counter.value = 7;
    Readable const& view = &counter;
    println("{} {}", view.read(), consume(&counter));

}
```

声明初始化、赋值、实参和返回值的目标为接口引用时，可直接接收实现该接口的具体类型引用，不需要 `Any.set`／`getReference` 中转。编译器直接生成已有的“地址＋具体类型号”接口视图，沿用运行时方法分派。只读来源只能绑定只读视图，可写来源可绑定只读或可写视图；接口值仍不能任意强转或访问具体字段。

视图借用原对象。复制视图只复制接口句柄，不复制具体对象；原对象必须活得更久，局部对象的接口视图不能从函数返回，也不能保存到更外层作用域。没有实现接口的类型在编译期报错；同名方法并不代替显式的接口实现。

## 泛型参数必须明确

省略类型实参不再自动补成 Any。允许函数类型推导，也允许模板声明显式给出默认类型。

```chali
struct Box<T> { T value; };
struct ExplicitDefault<T = Any> { T value; };
void main() {
    Box<int64> number;
    number.value = 7;
    Box<Any> dynamic;
    dynamic.value.set("explicit");
    ExplicitDefault<> chosen;
    chosen.value.set(true);

}
```

例如 `Pair<int64>` 不能再省略第二个类型参数，应写成 `Pair<int64, String>` 或确实需要类型擦除时写成 `Pair<int64, Any>`。

## 新模块

| 模块 | 首批功能 | 错误边界与平台 |
| --- | --- | --- |
| [`std::encoding`](library/std-encoding.md) | Base64、Base64URL、十六进制 | 严格解码返回 `Result`，拒绝非法填充与非零尾部位；纯 ChaLi |
| [`std::csv`](library/std-csv.md) | 文本读写、转义、分隔符、位置诊断 | 引号字段可含换行；位置为一基行号和字节列；纯 ChaLi |
| [`std::datetime`](library/std-datetime.md) | Unix 秒、UTC 日期、解析格式化、日期运算 | 公历年份 1..9999，ISO 秒精度 UTC；换算为纯 ChaLi，当前时间需要宿主时钟 |
| [`std::io`](library/std-io.md) | 字节流、整文件、目录、文件信息、创建与重命名 | `FileStream` 字节接口走 FileApi；路径操作使用桌面宿主文件系统 |
| [`std::cli`](library/std-cli.md) | 长短选项、位置参数、默认值、帮助、校验 | 不自动打印或退出，应用读取解析结果；纯 ChaLi |
| [`std::utf8`](library/std-utf8.md) | 校验、码点编解码、数量、截取 | 严格 UTF-8；码点不是字素簇；字节接口完整支持 U+0000 |

没有名称冲突时示例优先使用简写。完整可运行程序和断言见各模块参考页。

```chali
import std::encoding;
import std::datetime;
import std::utf8;
void main() {
    List<uint8> bytes = decodeHex("666f6f").unwrap();
    println("{}", encodeBase64(bytes));
    DateTime utc = parseUtc("2024-02-29T12:34:56Z").unwrap();
    println("{}", formatUtc(addDays(utc, 1).unwrap()).unwrap());
    println("{}", sliceCodePoints("A中文😀", 1, 2).unwrap());

}
```

## 文件和目标宿主边界

`FileStream.readBytes(count)` 在 EOF 返回短列表，`writeBytes` 保留零字节；失败返回 `IoError`。操作系统宿主提供字节读取；自定义旧 FileApi 后端未实现字节读取时明确报告不支持。

`readAllBytes` 默认预算为 16 MiB，可显式传入上限。`readAllText` 读取完整内容而非一行，拒绝内嵌零字节；二进制文件必须使用字节接口。`listDirectory` 只枚举一层并排序；`fileInfo` 跟随符号链接；`renamePath` 的目标覆盖规则遵从平台。文件操作不承诺事务或原子写入。

自定义宿主的文件系统能力由 FileApi 注入，目录操作需相应系统服务。日期换算不依赖操作系统时区，`nowUtc` 的宿主必须提供真实 UTC 时钟。

## 二进制中间产物与体积实测

默认 `emit` 输出 `.quad`（MGQI v1）、`.layout`（CHLY v1）与 `.data.bin`。四元式的操作符、参数和源文件名使用共享字符串表；数量、索引、位置使用无符号 LEB128。布局只保存运行所需的符号、偏移、容量和步长。`.data.bin` 保留原始内存映像，不再重复写 `.data` 文本或 `.data.h` C 数组；重新生成同前缀产物时会清理旧副本。

VM 导出器自动识别新格式，并继续接受旧文本四元式和布局。格式带版本号，解码器检查整数溢出、截断、越界字符串引用及尾部数据。二进制编码不包含加密，也不作为保护源码的承诺。

2026-10-13 在 Windows / MinGW `-O2` 下，对同一份内容编码前后比较，单位为字节：

| 示例 | 文本 quad → 二进制 quad | 文本 layout → 二进制 layout | 原四文件总量 → 新三文件总量 | 减少 |
| --- | ---: | ---: | ---: | ---: |
| std_io_extended | 903,013 → 216,295 | 33,548 → 2,038 | 976,988 → 223,797 | 77.1% |
| std_cli | 764,630 → 184,923 | 26,979 → 2,574 | 822,671 → 191,457 | 76.7% |

原总量包括 `.quad`、`.layout`、`.data`、`.data.bin`，新总量包括 `.quad`、`.layout`、`.data.bin`；`.data.bin` 分别为 5,464 / 3,960 字节，未改变。未计入旧 `.data.h`，因此没有夸大删除重复数组带来的收益。

`source/stackvm/test_artifacts.cpp` 在内存中反复解码至少 250 ms，包含结构分配和释放，排除磁盘、源码编译与 C++ 编译。首轮均次结果：

| 示例 | quad 文本 / 二进制（µs） | layout 文本 / 二进制（µs） |
| --- | ---: | ---: |
| std_io_extended | 19,213.7 / 2,766.85（约 6.9 倍） | 300.935 / 3.969（约 75.8 倍） |
| std_cli | 16,294.9 / 2,398.31（约 6.8 倍） | 284.044 / 4.734（约 60.0 倍） |

这些数据描述解码环节，不能直接解释为整体编译速度。CMake 的 `chali-artifact-tests` 目标默认检查小型夹具；传入旧文本 `.quad` 和 `.layout` 路径可复测其他内容。测试同时核对四元式、源码位置、组装指令及布局往返等价性。

Map 的 `.iter()` 和 `.intoIter()` 只产生 `Pair<K, V>`，`first` 是键、`second` 是值，不产生元组或单独的键/值。

`Pair<K, V>::create((key, value))` 从二元组构造 Pair，`pair.toTuple()` 返回 `(K, V)`；两个方向均遵循值语义，独立复制拥有字段。

`Pair<K, V>::create(key, value)` 与 `Pair<K, V>::create((key, value))` 两个重载等价；不提供 `from` 方法。

类型参数可从实参推导：`Pair::create(key, value)` 与 `Pair::create((key, value))` 均可直接使用；显式指定 `Pair<K, V>` 时仍按该类型检查两个字段。
