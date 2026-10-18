# 墓碑机制调研报告

调研对象：工具链 `library/stackvm/`、内存分配器、核心库内建 lowering 与 watchdog 宿主清理路径。本文记录调研时的实现及最终改动位置；按用户后续要求，板端与桌面最终统一使用 `library/stackvm/`，板端通过宏裁剪线程、AOT、宿主文件 I/O 与异常保护。

## 原实现

- `library/stackvm/allocation_index.hpp` 的 `AllocationIndex` 用 `Memory::Map<std::size_t, Allocation> active_` 保存活跃逻辑区间，用 `Memory::Set<std::size_t> retired_` 保存已释放块首。`next_` 为每次分配生成新逻辑地址，物理偏移存于 `Allocation::offset`。
- `AllocationIndex::add()` 建立活跃记录；`release()` 将已释放块首插入 `retired_`，重复释放通过集合命中幂等返回；`retired()` 查询旧身份；`find()` 通过活跃区间及最近命中缓存验证读写。
- 默认物理堆在 `library/stackvm/parts/machine_natives.cpp` 的 `Heap` 中实现。`Heap::allocate()` 首次适配空闲块，切分余量；`Heap::release()` 释放并合并相邻空闲块。物理空间可以反复复用，但 `retired_` 每次释放追加一个新逻辑身份，因此历史记录随累计分配增长。

## 三条路径

| 操作 | 路径与边界 |
| --- | --- |
| 分配 | 语言 `new` lowering → VM/native 分配 → `Heap::allocate()` → `AllocationIndex::add()`；物理字节区与逻辑身份分离。 |
| 首次释放 | `LowerDelete()` / VM `free`、`drop` → 对应堆的 `release()` → `AllocationIndex::release()`；聚合释放先验证来源，再访问字段并执行析构。 |
| 重复释放 | 原实现先命中 `retired_`，不再释放物理字节；聚合旧引用原先通过 `__vm_heap_live` 跳过析构。物理复用后旧身份仍须幂等，不能释放新对象。 |

## 墓碑压缩的落点

- 修改 `AllocationIndex::add()`、`release()`、`retired()`、`find()`、`contains()`、`reset()`，增加 `sweep()`、槽数与身份数查询，以及空闲区间的可编码位置选择。
- 以固定槽向量和“物理块首 → 槽下标”映射替代逐身份墓碑集合。每槽保存最新代次、清扫边界及当前偏移位宽；活跃区间仍单独保存。
- 逻辑地址自身包含稳定槽号、分配代次，以及解码槽号和块内偏移所需的位宽标签。旧引用不依赖最新分配的大小。不同槽的逻辑窗口互不重叠，防止拆分、合并后旧字段引用命中新分配。
- 默认 `Heap::allocate()` 在空闲区间中优先复用可编码的已有槽；必要时切出新的物理块首，避免仅因旧槽位宽或代次约束持续扩展物理堆。
- 扩大的整数地址与对象句柄要求 `library/stackvm/parts/execution.cpp` 的寻址、释放、索引路径直接读取整数槽，不能先经过 `double`。

## sweep() 的插入点

- `source/epilogue/builtins.cpp` 登记核心内建的返回类型。
- `source/epilogue/boxing.cpp` 将 `sweep()` 降低到 `__vm_heap_sweep`，并让显式聚合 delete 使用 `__vm_heap_delete_live` 区分“未清扫旧身份”和“已经失去墓碑保证的身份”。
- `library/stackvm/parts/machine_natives.cpp` 登记两个 native 入口，调用当前堆的 `sweep()` / `retired()`。
- `library/stackvm/stackvm.hpp` 的 `HeapAllocator` 增加清扫和墓碑查询契约。未实现清扫的自定义堆显式报告不支持，不伪装成清理了零条。
- 清扫推进每槽边界，保留活跃分配及其未来释放的幂等保证；不释放活跃对象，不重置代次，不追踪引用。已清扫旧身份再次显式 delete 报 MGL1205。
- 核心库文档与索引由 `tools/library_reference_builtins.py`、`tools/library_reference_data.py` 和 `tools/library_reference.py` 生成，覆盖 `doc/library/std-lang.md`、`symbols.md`、`api-index.json`。

## watchdog 的插入点

- `source/std/watchdog.mgl` 提供请求重启的语言接口，运行时记录可恢复终止信号。
- `Machine::run()` 的收尾位于 `library/stackvm/parts/execution.cpp`；所有执行上下文停止后，为可恢复终止清扫当前堆。
- AOT 的对应收尾为 `library/stackvm/parts/aot_bridge.cpp` 的 `RuntimeBridge::end()`，在线程与协程收尾后处理重启信号并清扫。
- 宿主随后销毁旧执行状态、重置 arena，并重新运行逻辑。完整 watchdog 重启在墓碑清理效果上等价于完整 sweep()。
- 板端 `source/stackvm_mcu/mcu_host.cpp` 的重启收尾显式清扫并重置字节池和身份槽，再重置 arena；正常结束后的水位重启也经过该路径。
- 板端与桌面共用压缩身份索引，按宿主指针位宽选择编码；板端只保留 arena、输入输出和固定字节池适配。

## malloc/free 回退的移除范围

删除 `MAGEL_USE_MALLOC` 逐块宿主堆回退模式、C 的 `magel_heap_allocate/release` 包装入口及基准脚本开关。保留池本身的分配/释放 API，以及宿主为动态池一次性预留和归还整片缓冲区的底层设施。

## 不加入自动清扫的原因

“墓碑身份小于最小活跃分配身份”不能证明旧引用已经消失。例如 A 已释放但旧引用仍在，随后 B 成为最老的活跃分配；A 的身份比 B 小，旧引用却仍需要 A 的墓碑。函数返回或循环边界同样不能证明引用没有逃逸。阈值不能替代安全性证明，因此本次仅提供用户负责的手动边界与完整 watchdog 重启，不添加引用扫描或自动清扫策略。

## fn 闭包借用规则的参考与实现

参考 Rust 编译器 [upvar.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_hir_typeck/src/upvar.rs) 的捕获分析流程，以及 [constraint_conversion.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_borrowck/src/type_check/constraint_conversion.rs) 的寿命约束转换。Magel 使用自己的作用域来源事实与控制流检查器，不引入 Rust 的类型系统。0.15.0 默认捕获为 `const&`，显式 `mut` 使用可写借用；`copy` 和 `move` 持有内部状态。复制引用捕获的闭包不延长来源寿命。

捕获发现位于 `source/epilogue/generic_inference.cpp::CheckLambdaCaptures`；地址存储在 `source/epilogue/artifacts.cpp` 预登记，经 `source/epilogue/boxing.cpp` 建立和恢复闭包环境；来源寿命经 `source/epilogue/borrow_facts.cpp` 传播到赋值、返回与聚合字段。`LowerAssign` 必须先完成右值推导、发现 lambda 捕获，再记录保存约束。

异步事件回调可以被保存到全局登记表，因此单独要求静态来源；事件函数签名携带该契约，调用时拒绝借用局部环境的闭包，模块内部转送保留契约。含回调参数的事件登记入口不能转换成擦除该契约的普通函数指针。该限制不要求同步算法回调具有全局寿命。

## 0.15.0：借用检查与 Rust 源码对照

本轮参考 Rust 编译器的三个入口，采用适合 Magel 现有语义的规则：

| Rust 源码 | Magel 中的对应处理 |
| --- | --- |
| [move_paths/mod.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_mir_dataflow/src/move_paths/mod.rs)：记录存储位置、移动与初始化 | `Place` 使用变量存储身份加字段、下标和解引用路径；整体移动与局部字段移动分别影响对应路径 |
| [places_conflict.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_borrowck/src/places_conflict.rs)：逐层比较投影的重叠关系 | 不同结构体字段和已知不同下标互不影响；动态下标保守视为可能重叠 |
| [dataflow.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_borrowck/src/dataflow.rs)：借用与初始化状态的数据流分析 | 独立控制流图迭代到不动点；分支合并保留可能失效和借用来源的并集，同名重新定义要求所有到达路径均已完整移动 |

闭包建立时将捕获来源的读取和 move 捕获写入同一张控制流图。引用槽与其所指对象分别检查：移动引用变量使其槽失效；重新绑定只恢复该槽，不复活旧的已释放对象。copy/move 捕获的内部引用仍须通过寿命检查，拥有闭包不会延长外部被引用对象的寿命。

这是一套 Magel 自身的来源与移动分析，并未实现 Rust 的完整 NLL 区域推导、可变借用唯一性或 Polonius。引用资料用于规则对照，现有 Magel 允许的别名关系保持其语言契约。
