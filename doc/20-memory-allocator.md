# 内存分配器

本篇从 heap_address_reuse.chl 的地址复用反例和 std_parse_random_path_memory.chl 的 Arena 使用出发，区分 ChaLi 值的清理、VM 堆与宿主资源。随后说明容量统计和外部缓冲区接入。

## 地址复用样例揭示什么

自定义 `stackvm::HeapAllocator` 必须准确实现 `capacityAt`：未分配或已释放地址返回 false，有效地址返回到所属块末尾的剩余容量。运行时复用这份分配元数据，先检查再调用底层读写，不额外维护位图。`contains` 只表示地址属于池，不能作为有效分配的依据。可以用位图实现分配器，但仍须识别块边界，不能把两个相邻分配拼成一次合法访问。

[heap_address_reuse.chl](../example/samples/heap_address_reuse.chl) 先分配整数并赋 11，释放后再分配同样大小的整数并赋 22，从新引用读取，正式预期输出为 [22](../example/expected/heap_address_reuse.txt)。物理块可以复用，逻辑身份不会复活；释放前的旧别名不能再解引用。拒绝旧别名的样例见 [heap_stale_alias.chl](../example/samples-error/heap_stale_alias.chl)，正常引用与所有权写法见[核心概念](03-core-concepts.md)。

## Arena 的 reset 与普通值清理不同

[std_parse_random_path_memory.chl](../example/samples/std_parse_random_path_memory.chl) 使用 Arena/Span 检查批量分配；[arena_use_after_reset.chl](../example/samples-error/arena_use_after_reset.chl) 故意在 reset 后访问旧 Span，应报告运行错误。reset 让该 Arena 的所有旧分配一起失效，不能把仍持有 Span 变量当作来源仍有效。

## 用样例观察内存统计

```sh
cd chali
mkdir -p ../build/doc-example
../build/work/chali/chali run example/samples/std_parse_random_path_memory.chl --stats --out ../build/doc-example/memory
```

ChaLi 分配器通过 Memory 命名空间管理容器和对象，默认池 512 MiB；全局 C++ new/delete 保留宿主行为。通用分配器实现在仓库根的 [`source/allocator/`](../source/allocator)（`chali_pool.c` 与 `source/chali_allocator.hpp`），ChaLi 导出使用同一份资产。接入与宿主边界如下。

## PC 容量与统计

PC 默认容量为 **512 MiB（536870912 字节）**。解释器与独立 exe 接受：

```sh
chali run app.chl --memory-size 16777216 --stats
./app --memory-size 16777216 --stats
```

## 外部缓冲区

缓冲区由宿主持有并覆盖整个调用生命周期。此路径不申请 PC 的后备池。
返回值 0 表示成功，1 表示 VM / 分配失败；`stepLimit` 限制失控循环，**传 0 表示不设上限**
（导出 exe 的 `--step-limit 0`、解释器的 `--step-limit 0` 同义）。
同一缓冲区可以反复调用。线程共用受锁保护的池，需等待全部工作线程退出后才能清零。

## C 分配器与 C++ 接入

[`chali_pool.c`](../source/allocator/chali_pool.c) 可独立按 C99 编译，块元数据位于缓冲区内，
支持二次幂对齐，无固定块数表，也不会调用全局 C++ 分配操作。底层实现由
[`source/chali_pool.h`](../source/allocator/chali_pool.h) 的**分配器类型宏** `CHALI_ALLOCATOR_TYPE` 在编译期选择
（用 `-DCHALI_ALLOCATOR_TYPE=…` 覆盖，必须对所有翻译单元一致）：

- `CHALI_ALLOCATOR_LINKED`（缺省）：块头 + **显式空闲双向链表**首次适配，切块后维护相邻块信息，
  释放时直接合并相邻空闲块；**释放过的地址可以再分配出去**，长循环的“分配—释放—再分配”
  不会持续吃内存。
- `CHALI_ALLOCATOR_LINEAR`：线性（bump）分配，只向后切块，释放只把块记成墓碑，
  地址到 `reset` 前不复用。适合“地址一旦给出就不再复用”的护栏场景，代价是长循环占用只增不减。

两种实现共用 `McPool` 句柄与 `chali_pool_*` 接口，上层 arena 与容器无需改动。
`chali_pool` 本身不带锁：它是**单线程/调用方串行化**的；缺省 C++ 侧由
`BufferRuntimeArena` 与 `StaticRuntimeArena` 各自持 `std::mutex` 加锁（VM 线程共享同一个
`currentRuntimeAllocator()`，所以锁是必须的）。`CHALI_ALLOCATOR_TYPE` 当前编译进来的名字由
`Memory::allocatorTypeName` 暴露，便于诊断。

[`source/chali_allocator.hpp`](../source/allocator/chali_allocator.hpp) 提供
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

ChaLi 翻译单元及其依赖库必须一致定义 `CHALI_CUSTOM_ALLOCATOR=1`。
生成 C++ 解析器通过用户头的 `PARSER_MEMORY_NAMESPACE` 接入这一组类型与工厂。
普通通用库构建不定义该宏时，别名仍是标准 STL 类型。

## 宿主边界

文件流、正则引擎、操作系统线程栈和应用胶水自己创建的外部资源使用各自宿主设施。
这些资源不计入池容量，也不由池清零。这一接口支持提供缓冲区的 C++17 宿主，
没有承诺无标准库或无操作系统的裸机环境。

托管胶水文本和错误参数使用 `Memory::String` / `Memory::String *`。
普通 `std::string` 宿主签名可以使用 Codec 转换，其宿主字符串由宿主释放。

## 活跃分配与代次隔离

运行时只保存活跃逻辑区间与物理块首的最新代次，不保存释放墓碑或清扫边界。释放立即移除活跃记录；重复释放返回失败。自定义 `HeapAllocator` 保持相同契约，不再提供清扫和已释放身份查询接口。

物理空间可以复用，逻辑引用编码槽号、分配代次与块内偏移，旧引用不能命中新对象。每个曾使用的物理块首仅占一个身份槽，累计分配次数不增加槽数；代次耗尽时拒绝复用该身份，不回绕。身份槽是防止旧引用复活的分配元数据，不是已释放对象列表。

编译器为新建的借用实参生成完整表达式临时所有者，接管返回值时清空源槽。显式释放只改变实际执行路径上的拥有槽与初始化状态；其他分支仍执行自动清理。可证明的重复销毁由编译器合并，释放后访问报 `CHL1205`，跨调用及不透明宿主操作仍由运行时核验活跃分配。

## 统一内存池

不再支持 `CHALI_USE_MALLOC` 的逐次 malloc/free 模式，容量限制、统计和外部缓冲区清零均使用内存池口径。动态池在宿主侧预留和归还整块缓冲区仍使用宿主分配设施；ChaLi 的对象和容器不再转向逐次宿主堆分配。基准入口为 `benchmarks/compare-vm.ps1`，历史 malloc 对比报告仅用于存档。

## 局部值存储与统计观测

当前编译器为局部聚合、定长数组和引用隐藏槽分配每次调用独立的运行时存储，正常控制流下自动释放。它们可能计入 `Memory::stats()` 的活块与字节数；不能把尚存活的局部值所占空间误判为泄漏。比较某段代码的净分配时，应保持观测点的存活对象集合一致，例如把 `Memory::stats().liveBlocks` 保存为标量，避免“前后两个 MemoryStats 局部聚合”本身引入不同数量的存储。

单对象 `new` 与运行时数组的新存储先清零；结构体单对象还初始化默认容器字段。已释放引用仍然失效，清零不能替代生命周期检查或分配代次检查。递归、地址复用和清理回归见 [03 核心概念 §4](03-core-concepts.md#4-无垃圾回收确定性释放)。
