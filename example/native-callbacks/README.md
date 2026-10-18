# 胶水同步调用 Magel 函数

同一份 [`glue.cpp`](glue.cpp) 用于 VM 与 AOT。Magel 侧把具名函数或 `fn` 值作为参数传给宿主，
C++ 侧接收 `magel_glue::Function<Result(Args...)>` 并直接调用。支持捕获闭包，不需要 `export`。
[`demo.mgl`](demo.mgl) 包含具名函数、内联函数、copy/move 捕获、float32、bool、void、
可变引用、高阶函数及 Magel → C++ → Magel 多层重入，并通过断言检查结果。

在仓库根目录构建并运行 AOT（Windows 的可执行文件使用 `.exe` 后缀）：

```sh
./magel aot example/native-callbacks/demo.mgl --glue example/native-callbacks/glue.cpp --out build/native-callbacks
./build/native-callbacks
```

VM 宿主显式安装相同注册表；编译器的 `run` 命令没有加载任意 C++ 源文件的入口。
下面用仓库共享核心构建最小宿主，再加载编译器输出的程序：

```sh
g++ -std=c++17 -O2 -pthread -DMAGEL_WITH_REFLECTION=1 -DSTACKVM_WITH_REFLECTION=1 -DSTACKVM_WITH_BIGINT=0 -DSTACKVM_WITH_BIGFLOAT=0 -Isource -Ilibrary -Ilibrary/stackvm example/native-callbacks/vm_host.cpp library/stackvm/stackvm.cpp library/stackvm/stackvm_glue.cpp -o build/native-callbacks-vm
./magel emit example/native-callbacks/demo.mgl --svmb --out build/native-callbacks-program
./build/native-callbacks-vm build/native-callbacks-program.svmb build/native-callbacks-program.data.bin build/native-callbacks-program.layout
```

这里仅裁剪该演示不使用的大数模块；VM 执行优化与反射保持开启。
[`vm_host.cpp`](vm_host.cpp) 在执行结束时还检查活动堆块和对象数，防止回调造成资源泄漏。
两条路径的输出均为：

```text
callbacks: named inline copy move float32 bool void reference higher-order reentry OK
```

`function(value)` 返回按签名解码的值。需要检查失败时使用
`function.tryInvoke(rawResult, value)`；成功后可按返回类型使用 `Codec<Result>::decode(context, rawResult)`。
原始 `Context::invoke(address, encodedArguments, rawResult)` 也适用于无需类型包装的板端胶水。
`void` 函数忽略返回槽。调用失败仍使所属运行时失败，不能以缺省返回值掩盖 `panic` 或参数错误。

函数地址和闭包句柄属于当前运行时，不能转换成宿主机器函数指针；包装和上下文只能在当前宿主
回调期间、同一执行线程同步使用，不能缓存后异步调用。`Function` 不复制或保活捕获环境。
数值、布尔、引用与函数值由相应 `Codec` 传递；字符串、列表、聚合等拥有资源的值还须遵守
其 Codec 和语言所有权约定，本例不提供任意资源类型的自动所有权转移。

回归命令 `python tests/magel_native_callbacks.py` 对 VM/AOT 的 IR O0/O2 分别执行成功程序，
并检查空函数、参数数量错误、Magel panic 和宿主错误传播。
