# 08 宿主集成与 C ABI

Magel 程序不是孤岛。这篇讲两件事：**Magel 怎么调宿主（C/C++）代码**，以及**宿主怎么把 Magel 程序跑起来**。

`export` 声明的 C 接口只支持 AOT。`magel run` 和 `magel svmb` 遇到它会报
“C 函数导出仅支持 AOT，VM 执行不支持 export 声明”；`--check` 仍可用于检查源码。
库源码至少包含一个 `export`，可省略 `main`。`magel aot app.mgl --out app.dll`
按后缀选择共享库（Linux 使用 `.so`），`--out app.o` 生成可重定位文件供宿主链接。
`.o` 不直接交给动态加载器；不支持 `.lib` / `.a` 静态库。选项及平台限制见[库输出](09-toolchain.md#aot-库输出)。
共享库同时生成 C 头文件，宿主通过 `magel_create` 创建独立上下文，调用
`magel_<函数名>`，最后调用 `magel_destroy`。完整例子见
[`C 导出及 ABI 合约示例`](examples/abi_contracts/)。
`abi "库路径" { ... }` 是从 Magel 调用 C 动态库，仍支持 VM 执行。

---

## 1. 三种集成方式

| 方式 | 用在哪 | 代价 |
| --- | --- | --- |
| `abi` 块 | 直接调用动态库里按 C ABI 导出的函数 | 需要一份 `.dll` / `.so` |
| 宿主胶水绑定 | 把 C++ 函数注册进编译器，Magel 侧像普通函数一样调 | 要写一点 C++ |
| `runMagel` 嵌入 | 宿主程序（含固件）调用编译好的 Magel 逻辑 | 需要 AOT 产物 |

---

## 2. `abi` 块：直接调 C 函数

```magel
abi "./native.dll" {
    int32 native_add(int32 left, int32 right) = "abi_add";
    String native_name() = "abi_name";
}

int64 main() {
    println("{} {}", native_add((int32)20, (int32)22), native_name());
    return 0;
}
```

语法是：

```
abi "<动态库路径>" {
    <返回类型> <Magel侧名字>(<形参表>) = "<库里的符号名>";
}
```

等号右边是库里真实导出的符号名，左边是你在 Magel 里用的名字——两者可以不同，方便给一批 C 函数起统一风格的 Magel 名。

完整例子见 [`doc/examples/abi_dynamic/`](../doc/examples/abi_dynamic/)（最小）和 [`doc/examples/abi_buffers/`](../doc/examples/abi_buffers/)（覆盖结构体、数组、可选引用）。

### 支持的类型

| Magel 写法 | 含义 |
| --- | --- |
| `int8` … `int64`、`uint8` … `uint64`、`float32`、`float64`、`bool` | 标量，按值传 |
| `String` | 字符串 |
| `T mut&` / `T const&` | 可变 / 只读引用 |
| `Option<T mut&>` / `Option<T const&>` | **可空引用**（宿主侧可传空） |
| `T[]` | 数组视图 |
| `T[] f(...) = "..." [count]` | 返回值长度由某个形参决定 |
| 结构体 | 按值传；布局默认按 C 规则 |
| `struct X packed { … }` | 紧凑布局（不对齐填充） |

```magel
abi "./native.dll" {
    void packed_update(Packed mut& value) = "packed_update";
    Option<Packed const&> packed_snapshot() = "packed_snapshot";
    Packed[] packed_numbers(int32 count) = "packed_numbers" [count];
    void packed_fill(Packed[] values, int32 count) = "packed_fill";
}
```

要点：

- 引用参数在调用处写 `&x`：`update(&q);`、`bump(&q.count);`。
- 可空引用走 `Option<Packet mut&>`：没有就传 `Option<Packet mut&>::None`，宿主侧收到空指针。这是**唯一让"空"跨过边界的方式**——Magel 内部的引用仍然非空。
- 数组用 `T[]`，长度另给；返回值长度用 `[count]` 标注由哪个形参决定。
- `packed` 结构体用于对接 C 侧 `#pragma pack` 的紧凑布局。

---

## 3. 宿主胶水绑定

### ABI 返回值的释放与同步回调

默认返回指针由原生库持有，Magel 复制所需内容，不释放原生内存。如果库要求调用者释放，
在声明中指定同一个库内的 `void release(void*)` 符号；复制结束或复制失败时都会调用它。

```magel
abi "./native.dll" {
    String text() = "owned_text" ["free_owned"];
    int32[] numbers(int32 count) = "owned_numbers" [count, "free_owned"];
    int32 apply(int32 fn(int32) callback, int32 value) = "apply";
}
```

字符串遵循 C 的 NUL 结尾约定，字符串参数不允许内嵌 NUL；二进制内容使用数组。
声明中的结构体布局会校验大小、对齐和字段边界，但不会从动态库反推出真实 C 签名。
声明必须与原生函数一致，错误指针无法通过布局校验变成安全指针。

`fn` 回调目前只支持标量参数和标量或 `void` 返回值，并且必须在本次原生调用返回前，
由同一线程同步调用。原生库不得保存回调指针或在其他线程调用它；需要异步回调时先写 C 包装层。
回调中的失败会先被截获，待原生函数返回后交给 Magel 报错，异常不会穿过 C 调用栈。

复杂 ABI 桥默认缓存到用户缓存目录。`--abi-cache DIR` 指定目录，`--abi-no-cache` 禁用缓存。
`magel run app.mgl --abi-prepare --abi-write-bridge bridge.dll` 只准备桥，不运行入口；
后续可用 `--abi-bridge bridge.dll` 显式加载，避免现场编译。桥的生成代码身份必须匹配，
只加载可信来源的本机动态库。`MAGEL_ABI_CXX` 可以指定生成桥时使用的 C++ 编译器。

`magel gen abi api.h --library ./native.dll --out bindings.mgl` 可以导入有限的 C 头文件：
定宽标量、简单结构体、整数枚举常量、标量 typedef 和同步回调 typedef。生成类型使用
`C_` 前缀，函数使用 `c_` 前缀。指针、所有权、复杂预处理、位域、联合体及可变参数须手写
包装；遇到不支持的声明会失败，不输出一份缺少声明的绑定。完整测试源码见
[`abi_contracts`](examples/abi_contracts/)。

想把 C++ 函数注册进来（而不是走动态库），写一个 `bindMagelGlue`：

```cpp
#include <cstdint>
#include <string>

struct ExportCounterTag;
using ExportCounter = magel_glue::Handle<ExportCounterTag, std::uint64_t>;

bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTyped<ExportCounter, std::uint64_t>(
               "demo_counter_from_uint",
               [](std::uint64_t arg0) { return ExportCounter{arg0}; }, error) &&
           registry.bindTyped<std::int32_t, Memory::String>(
               "demo_cpp_text_size",
               [](Memory::String arg0) { return static_cast<std::int32_t>(arg0.size()); }, error);
}
```

模板参数顺序是 `<返回值, 形参…>`，第一个字符串参数是**绑定键名**。

配套约定：

- **名义宿主类型**用 `magel_glue::Handle<Tag, AbiType>` 声明——不同 Tag 就是不同类型，不会互相混淆。
- **复杂宿主对象一律以整数句柄过边界**，不要把 C++ 对象本体塞进 Magel 值里。
- 泛型函数实例化后键名带后缀：上面的 `demo_cpp_identity<int32>` 对应键名 `demo_cpp_identity__int32`。

Magel 侧声明这些函数，就像声明普通函数：

```magel
type HostHandle<T> = uint64;

HostHandle<int32> demo_cpp_add(HostHandle<int32> left, HostHandle<int32> right);
int32 demo_cpp_text_size(String text);
T demo_cpp_identity<T>(T value);
```

想让宿主类型实现 Magel 的接口或转换 trait，用 `impl`：

```magel
impl HostHandle<int32> : HostValue = demo_counter_to_uint;
impl uint64 : ExplicitConvert<HostHandle<int32>> = demo_counter_from_uint;
```

转换用 `cast<T>(x)` 显式调用。

样板代码见 [`source/glue/cpp_glue.cpp`](../source/glue/cpp_glue.cpp)（应用胶水）与 [`source/glue/std_glue.cpp`](../source/glue/std_glue.cpp)（标准库胶水）。各标准库模块的绑定定义在 `source/glue/std_*.hpp`。

---

### 从胶水同步调用 Magel 函数

VM 与 AOT 共用 `magel_glue::Function<Result(Args...)>`：Magel 把具名函数、内联 `fn` 或捕获闭包
作为参数传给 C++，胶水直接调用包装。函数返回后继续当前程序，共享全局变量、堆和对象状态。

```magel
int64 hostApply(int64 fn(int64) function, int64 value);
int64 triple(int64 x) { return x * 3; }
int64 main() {
    assert(hostApply(triple, 7) == 21);
    assert(hostApply(int64 fn(int64 x) { x + 1 }, 7) == 8);
    return 0;
}
```

C++ 注册时的参数类型写成 `magel_glue::Function<std::int64_t(std::int64_t)>`，
`function(value)` 按签名完成编解码。`function.tryInvoke(rawResult, value)` 返回调用是否成功；
成功时 `rawResult` 是原始返回槽，可用 `Codec<Result>::decode(context, rawResult)` 解码。
底层入口为 `Context::invoke(functionAddress, encodedArguments, rawResult)`，板端无需依赖类型包装。

| 调用能力 | VM | AOT |
| --- | --- | --- |
| 具名函数、内联 `fn`、copy/move 捕获闭包 | 支持 | 支持 |
| 整数、浮点、布尔、void、引用及高阶函数参数 | 同一 Codec 接口 | 同一 Codec 接口 |
| Magel → 胶水 → Magel 多层同步重入 | 支持 | 支持 |
| panic、空函数和参数数量错误 | 终止当前执行并保留原始诊断 | 终止当前执行并保留原始诊断 |
| 自定义胶水的安装 | 宿主将 Registry 安装到 Machine | `--glue` 编入产物 |
| `export` C 接口 | 不支持 | 支持 |

地址是当前运行时的函数值或闭包句柄，不能强制转换成 C/C++ 机器函数指针。
包装与上下文只在所属宿主回调期间有效，只能同线程同步调用，不保存后异步使用；
函数包装不复制捕获环境。字符串、列表和聚合等资源值仍须按各自 Codec 管理所有权，
此接口不提供任意资源类型的自动转移。调用失败不是可恢复的普通返回值，不应忽略失败继续业务操作。

完整 C++ 注册代码、VM 宿主、运行命令及十种回调用例见
[`胶水回调示例`](../example/native-callbacks/README.md)。两端差分回归为
[`tests/magel_native_callbacks.py`](../tests/magel_native_callbacks.py)。

## 4. 把 Magel 嵌进宿主程序

AOT 生成的 C++ 里带着入口：

```cpp
static int runMagel(int argumentCount, const char *const *arguments,
                    void *buffer, std::size_t memoryBytes);
```

| 参数 | 含义 |
| --- | --- |
| `argumentCount` / `arguments` | 传给 Magel `main` 的参数（不含程序名） |
| `buffer` | 运行内存缓冲区；传 `nullptr` 由程序自行分配 |
| `memoryBytes` | 缓冲区大小，即运行内存上限 |

生成的文件用 `#ifndef MAGEL_NO_MAIN` 包着 `int main(...)`。**定义 `MAGEL_NO_MAIN` 后再编译，就只剩 `runMagel` 可供你的固件调用。**

典型用法：固件启动时划一块静态内存，反复调用 `runMagel` 执行业务逻辑；逻辑更新时替换数据，不动固件。详见[跨平台部署](06-deployment.md#4-嵌入到宿主程序)。

---

## 5. 类型映射速查

| Magel | C / C++ |
| --- | --- |
| `int8` / `int16` / `int32` / `int64` | `int8_t` / `int16_t` / `int32_t` / `int64_t` |
| `uint8` … `uint64` | `uint8_t` … `uint64_t` |
| `float32` / `float64` | `float` / `double` |
| `bool` | `bool` |
| `char` | `char` |
| `String` | `Memory::String`（胶水侧） |
| `T mut&` | `T*` |
| `T const&` | `const T*` |
| `Option<T mut&>` | `T*`，可为 null |
| `T[]` | `T*` + 长度形参 |
| 结构体 | 同字段顺序的 C struct |
| `struct X packed { … }` | `#pragma pack(1)` 的 C struct |

---

## 6. 边界上的注意事项

- **Magel 内部的引用永远非空**。只有 `Option<T mut&>` / `Option<T const&>` 能把"空"送过边界，而且只在 abi / 胶水这一层。
- **不要在宿主侧长期持有 Magel 传来的指针**。引用只在本次调用内有效。
- **复杂对象用句柄**。整数句柄比裸指针安全，也便于宿主管理生命周期。
- **嵌入式下网络、进程、SQL 等模块被禁**，胶水里对应绑定也就不可用——见[跨平台部署](06-deployment.md#3-嵌入式前端配置与独立-vm)。

---

## 7. 下一步

| 你想 | 去哪 |
| --- | --- |
| 看能跑起来的完整例子 | [`doc/examples/abi_dynamic/`](../doc/examples/abi_dynamic/)、[`abi_buffers/`](../doc/examples/abi_buffers/)、[`library/sqlite/`](../doc/examples/sqlite/) |
| 看标准库是怎么接宿主的 | [`source/glue/std_*.hpp`](../source/glue) |
| 把产物部署到板端 | [跨平台部署](06-deployment.md) |
| 查 AOT 与命令行选项 | [工具链使用](09-toolchain.md) |
