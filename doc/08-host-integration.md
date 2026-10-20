# 08 宿主集成与 C ABI

Magel 与宿主通过共享 VM 和原生函数注册表集成。Magel 可调用宿主 C/C++ 函数，宿主可加载字节码并执行 Magel 程序。

## 宿主运行 Magel

编译源程序得到 `.svmb`、`.data.bin` 和 `.layout`。宿主用 `stackvm::loadProgramBinary` 加载程序，以 `DataArea::load` 加载数据及布局，创建 `stackvm::Machine` 并设置数据区和原生注册表，最后调用 `Machine::run`。

可运行入口见[同步回调示例](../example/native-callbacks/vm_host.cpp)。标准集合等宿主能力需要注册对应的标准库胶水；VM 核心不内置具体集合存储或方法。

宿主负责程序、数据区、注册表和所借用资源的生命周期。执行结果包含退出状态、错误、输出及活动资源统计。

## C++ 胶水与同步回调

胶水使用 `magel_glue::Registry::bindTyped` 或 `bindTypedWithContext` 绑定函数。
复杂宿主对象通过类型化句柄或通用 `NativeRecord` 跨越边界，不把 C++ 对象内存伪装为 Magel 结构体。

Magel 函数和闭包使用 `magel_glue::Function` 或 `NativeCallContext::invoke` 同步调用。运行时检查实参数量、返回值、闭包环境、重入和错误传播；不要把运行时函数地址强转为宿主机器函数指针。
函数包装和上下文在所属回调期间、同一线程同步使用；容器与字符串按 Codec 复制、借用和清理。

List 使用 ArrayList。Map、Set 由标准库胶水按键的 Hash 能力选择哈希或数组存储；Map 迭代元素为 `Pair<K,V>`。

## 从 Magel 调用 C 动态库

`abi "库路径" { ... }` 为 VM 声明 C 动态库符号。标量、引用、结构体、数组、可选引用、同步回调和返回内存释放按 ABI 契约转换；声明必须符合真实的 C 签名和布局。
复杂调用可能需要宿主 C++ 工具链准备桥接模块，该模块负责调用外部库，Magel 程序仍在 VM 中运行。

C ABI 导入例子见[ABI 合约目录](examples/abi_contracts/)。

## 调试和限制

反射默认同时编入前端与 VM。板端可明确裁剪反射、线程和堆；桌面构建不能据此假设所有目标支持相同宿主服务。
宿主异常转换、panic 和边界错误保留原始 VM 诊断。VM 不承诺完整沙箱或任意宿主函数的安全隔离。

参见[工具链](09-toolchain.md)、[运行边界](17-capabilities.md)与[回调示例](../example/native-callbacks/README.md)。
