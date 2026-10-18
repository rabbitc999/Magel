# 内存分配器

本篇从 heap_address_reuse.mgl 的地址复用反例和 std_parse_random_path_memory.mgl 的 Arena 使用出发，区分 Magel 值的清理、VM 堆与宿主资源。随后说明容量统计和外部缓冲区接入。

## 地址复用样例揭示什么

Magel 的解释器和 AOT 桥接层支持可选的分配范围检查。解释执行默认开启，AOT 默认 `unchecked`，用 `magel aot app.mgl checked` 开启。开启后，标量读写先验证整个 8 字节区间，字符、字符串和胶水字节访问按实际长度验证；区间必须完全落在同一个活跃分配中。检查失败进入语言级 `panic`，报告原始 Magel 文件、行号和函数调用栈；`checked` 无需加 `debug` 才能得到 panic 行号。

`magel aot app.mgl` 等价于 `magel aot app.mgl unchecked`：导出的运行时定义 `STACKVM_WITH_ALLOCATION_GUARDS=0` 和 `NO_MAGEL_PANIC_TRACE=1`，裁剪额外分配区域检查、逐条源码定位与调用记录。`checked` 恢复护栏和 panic 跟踪，二者同时出现时最后一个生效；`run` / `svmb` 不接受这两个选项。直接构建、make 构建和模块独立构建使用相同配置，模块缓存通过生成公共头的内容区分配置。`unchecked` 保留普通类型检查、引用生命周期规则、资源清理、显式 panic，以及分配器与容器原有底层检查；它不承诺所有边界检查归零，也不保证非法内存访问一定能变为语言 panic。`debug` / `release` 控制 C++ 调试与优化配置，不替代 `checked`。

自定义 `stackvm::HeapAllocator` 必须准确实现 `capacityAt`：未分配或已释放地址返回 false，有效地址返回到所属块末尾的剩余容量。运行时复用这份分配元数据，先检查再调用底层读写，不额外维护位图。`contains` 只表示地址属于池，不能作为有效分配的依据。可以用位图实现分配器，但仍须识别块边界，不能把两个相邻分配拼成一次合法访问。

`tests/magel_memory_guard.py` 使用故意放宽底层读写的自定义堆验证拦截，再验证真实 Magel 的整数读写、浮点和字符读写共五种情况在解释执行及 AOT checked 中的 panic、行号与调用栈；另验收默认 unchecked 的三种构建路径和显式 panic。开启的护栏覆盖 VM 地址访问及经 NativeCallContext 的胶水访问，不拦截宿主绕开接口直接解引用原生指针。显式 `--compress-symbols` / panic 跟踪裁剪会隐藏诊断详情，已经剥离的 `.svmb` 来源信息不能凭空恢复。

[heap_address_reuse.mgl](../example/samples/heap_address_reuse.mgl) 先分配整数并赋 11，释放后再分配同样大小的整数并赋 22，从新引用读取，正式预期输出为 [22](../example/expected/heap_address_reuse.txt)。物理块可以复用，逻辑身份不会复活；释放前的旧别名不能再解引用。拒绝旧别名的样例见 [heap_stale_alias.mgl](../example/samples-error/heap_stale_alias.mgl)，正常引用与所有权写法见[核心概念](03-core-concepts.md)。

## Arena 的 reset 与普通值清理不同

[std_parse_random_path_memory.mgl](../example/samples/std_parse_random_path_memory.mgl) 使用 Arena/Span 检查批量分配；[arena_use_after_reset.mgl](../example/samples-error/arena_use_after_reset.mgl) 故意在 reset 后访问旧 Span，应报告运行错误。reset 让该 Arena 的所有旧分配一起失效，不能把仍持有 Span 变量当作来源仍有效。

## 用样例观察内存统计

```sh
cd magel
mkdir -p ../build/doc-example
../build/work/magel/magel run example/samples/std_parse_random_path_memory.mgl --stats --out ../build/doc-example/memory
```

程序 stdout 仍与[正式 expected](../example/expected/std_parse_random_path_memory.txt) 比较，最后为 memory=ok；统计走 stderr。VM 堆峰值和整个运行池峰值不是同一个数字。AOT 接受 --stats 但不提供同一组 VM 堆统计，不能沿用 VM 的阈值判断。

Magel 分配器通过 Memory 命名空间管理容器和对象，默认池 512 MiB；全局 C++ new/delete 保留宿主行为。通用分配器实现在仓库根的 [`source/allocator/`](../source/allocator)（`magel_pool.c` 与 `source/magel_allocator.hpp`），Magel 导出使用同一份资产。接入与宿主边界如下。

## PC 容量与统计

PC 默认容量为 **512 MiB（536870912 字节）**。解释器与独立 exe 接受：

```sh
magel run app.mgl --memory-size 16777216 --stats
./app --memory-size 16777216 --stats
```

导出时 `magel-emit --runtime-arena 16777216` 可以修改 exe 的默认容量。
这限制的是池的总大小，块头、对齐空隙也消耗容量；统计的峰值包含这些开销。
池耗尽报告失败，不转向另一个宿主堆继续分配。成功和失败路径都在销毁运行对象后清零池。

## 外部缓冲区

使用 `magel aot app.mgl out app --cpp-output` 保留 `app.aot.cpp`。嵌入式宿主定义 `MAGEL_NO_MAIN`，调用统一入口：

```cpp
#define MAGEL_NO_MAIN
#include "app.aot.cpp"

alignas(std::max_align_t) static unsigned char buffer[1024 * 1024];

int execute() {
    const char *arguments[] = {"example"};
    std::size_t peak = 0;
    /* 第 4 个参数 stepLimit：0 = 不设指令上限；要防死循环可传正整数。 */
    int status = runMagel(1, arguments, false, 0,
                          buffer, sizeof(buffer), &peak);
    // 此时整个 buffer 为零；peak 是本次运行的最大池占用。
    return status;
}
```

缓冲区由宿主持有并覆盖整个调用生命周期。此路径不申请 PC 的后备池。
返回值 0 表示成功，1 表示 VM / 分配失败；`stepLimit` 限制失控循环，**传 0 表示不设上限**
（导出 exe 的 `--step-limit 0`、解释器的 `--step-limit 0` 同义）。
同一缓冲区可以反复调用。线程共用受锁保护的池，需等待全部工作线程退出后才能清零。

## C 分配器与 C++ 接入

[`magel_pool.c`](../source/allocator/magel_pool.c) 可独立按 C99 编译，块元数据位于缓冲区内，
支持二次幂对齐，无固定块数表，也不会调用全局 C++ 分配操作。底层实现由
[`source/magel_pool.h`](../source/allocator/magel_pool.h) 的**分配器类型宏** `MAGEL_ALLOCATOR_TYPE` 在编译期选择
（用 `-DMAGEL_ALLOCATOR_TYPE=…` 覆盖，必须对所有翻译单元一致）：

- `MAGEL_ALLOCATOR_LINKED`（缺省）：块头 + **显式空闲双向链表**首次适配，切块后维护相邻块信息，
  释放时直接合并相邻空闲块；**释放过的地址可以再分配出去**，长循环的“分配—释放—再分配”
  不会持续吃内存。
- `MAGEL_ALLOCATOR_LINEAR`：线性（bump）分配，只向后切块，释放只把块记成墓碑，
  地址到 `reset` 前不复用。适合“地址一旦给出就不再复用”的护栏场景，代价是长循环占用只增不减。

两种实现共用 `McPool` 句柄与 `magel_pool_*` 接口，上层 arena 与容器无需改动。
`magel_pool` 本身不带锁：它是**单线程/调用方串行化**的；缺省 C++ 侧由
`BufferRuntimeArena` 与 `StaticRuntimeArena` 各自持 `std::mutex` 加锁（VM 线程共享同一个
`currentRuntimeAllocator()`，所以锁是必须的）。`MAGEL_ALLOCATOR_TYPE` 当前编译进来的名字由
`Memory::allocatorTypeName` 暴露，便于诊断。

[`source/magel_allocator.hpp`](../source/allocator/magel_allocator.hpp) 提供
`Memory::Allocator<T>`、`Vector/Map/Set/Deque/String`、`makeShared/makeUnique` 和回调适配器。
每个 allocator 保存自己的分配器所有者，跨池复制、移动或切换当前池时仍向原所有者释放。
所有者必须比其分配的对象活得更久。

```cpp
alignas(std::max_align_t) unsigned char storage[65536];
Memory::BufferRuntimeArena arena(storage, sizeof(storage));
{
    Memory::RuntimeSession session(arena); // 必须比池内对象先构造
    Memory::Vector<Memory::String> names;
    names.emplace_back(1024, 'x');
} // names 先销毁，session 再清零
std::size_t peak = arena.peakBytes();
```

PC 也可使用 `DynamicRuntimeArena(bytes)`；外部缓冲区使用 `BufferRuntimeArena(address, bytes)`。
直接调用池接口而没有 `RuntimeSession` 时，调用方需要归还所有块后调用 `finish()`。
仍有活动块时清零会被拒绝，防止存活对象引用已失效的内存。

Magel 翻译单元及其依赖库必须一致定义 `MAGEL_CUSTOM_ALLOCATOR=1`。
生成 C++ 解析器通过用户头的 `PARSER_MEMORY_NAMESPACE` 接入这一组类型与工厂。
普通通用库构建不定义该宏时，别名仍是标准 STL 类型。

## 宿主边界

文件流、正则引擎、操作系统线程栈和应用胶水自己创建的外部资源使用各自宿主设施。
这些资源不计入池容量，也不由池清零。这一接口支持提供缓冲区的 C++17 宿主，
没有承诺无标准库或无操作系统的裸机环境。

托管胶水文本和错误参数使用 `Memory::String` / `Memory::String *`。
普通 `std::string` 宿主签名可以使用 Codec 转换，其宿主字符串由宿主释放。

回归：[`runtime_allocator.c`](../tests/runtime_allocator.c)、
[`runtime_allocator.cpp`](../tests/runtime_allocator.cpp)、[`magel_emit.py`](../tests/magel_emit.py)；
它们都挂在发布门禁 `sh ci.sh` 上（见[06 部署](06-deployment.md)），
修改分配器后先跑基础门禁，再按改动影响补充 ABI、逐库或其他专项；入口见 [`ci.sh`](../ci.sh) 的分配器小节。

## 墓碑压缩与手动清扫

sweep() 不是垃圾回收器。它不做引用追踪、不判断对象存活、不自动调用。它只做一件事：清理已释放块的墓碑记录。

运行时按曾使用的物理块首建立稳定槽，保存最新分配代次与清扫边界。物理复用更新原槽，拆分产生的新块首建立新槽，合并不删除已有槽；同一物理地址的累计分配次数不会增加槽数。存储上界取决于物理块首的高水位，而不是当前空闲链表节点数。统计中的逻辑墓碑身份数量仍可增长，但不再对应逐条存储。

`sweep()` 返回本次撤销幂等保证的已释放逻辑身份数量；重复清扫且无新释放时返回零。调用者必须保证没有仍需使用的旧引用指向已释放块。清扫保留活跃分配，以及它们随后释放时的幂等保证；已被清扫的引用再次显式 `delete` 报 `MGL1205`。清扫不释放活跃对象，不缩小物理池，不重置分配代次。自定义 `HeapAllocator` 须实现 `sweep()` 与 `retired()`；未实现清扫时显式报告运行错误，不伪装为成功清理零条。

桌面和板端共用 `library/stackvm/` 的压缩索引；`source/stackvm_mcu/` 注入静态字节池及无异常元数据 arena，通过宏裁剪默认堆、线程、宿主文件 I/O、异常保护及 AOT 桥。32 位板端使用同一编码算法的 32 位布局，代次与单块容量限制见上文。

内部逻辑地址编码槽号、分配代次和块内偏移，并携带槽号及偏移的位宽标签。旧引用自行解码稳定物理槽，不依赖最新分配的大小；不同槽的地址窗口互不重叠，合并、拆分后的旧引用不会命中新对象。槽号按实际位数编码，不预留固定数量的槽。64 位宿主的第一个槽对小于 256 字节的块可表示 4398046511103 个代次，对小于 4 MiB 的块可表示 268435455 个代次；较大的槽号或块会消耗更多地址位，代次上限相应降低。32 位宿主的第一个槽对小于 8 字节的块可表示 524287 个代次，单块请求须小于 262144 字节。编码空间耗尽时拒绝分配，不回绕或复活旧引用。内部逻辑地址与原生地址不等价，应用不应依赖地址的数值或不同地址的大小关系。

不会在函数返回、循环边界或语句块退出时自动清扫。最小活跃分配身份不能证明旧引用已消失，阈值也不能代替这一证明。手动清扫与完整 watchdog 重启是显式安全边界，见 [核心库](library/std-lang.md) 和 [看门狗](library/std-watchdog.md)。

## 统一内存池

不再支持 `MAGEL_USE_MALLOC` 的逐次 malloc/free 模式，容量限制、统计和外部缓冲区清零均使用内存池口径。动态池在宿主侧预留和归还整块缓冲区仍使用宿主分配设施；Magel 的对象和容器不再转向逐次宿主堆分配。基准入口为 `benchmarks/compare-vm.ps1`，历史 malloc 对比报告仅用于存档。

## 局部值存储与统计观测

当前编译器为局部聚合、定长数组和引用隐藏槽分配每次调用独立的运行时存储，正常控制流下自动释放。它们可能计入 `Memory::stats()` 的活块与字节数；不能把尚存活的局部值所占空间误判为泄漏。比较某段代码的净分配时，应保持观测点的存活对象集合一致，例如把 `Memory::stats().liveBlocks` 保存为标量，避免“前后两个 MemoryStats 局部聚合”本身引入不同数量的存储。

单对象 `new` 与运行时数组的新存储先清零；结构体单对象还初始化默认容器字段。已释放引用仍然失效，清零不能替代生命周期检查或分配代次检查。递归、地址复用和清理回归见 [03 核心概念 §4](03-core-concepts.md#4-无垃圾回收确定性释放)。
