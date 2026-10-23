# 线程、通道与显式事件

[文档索引](README.md) · [English](en/29-thread-concurrency.md)

## 创建与隔离

线程模块为 `std::thread`，公开句柄类型为 `Thread`。源码运行时传入 `--threads`，嵌入宿主调用 `Machine::setThreadsEnabled(true)`。每个工作线程有独立的 VM 栈、堆和全局变量快照；普通变量不会因名称相同而共享。线程数与内存受操作系统和宿主预算约束，不能把无限创建线程当作任务调度策略。

`create(task)` 接收无捕获的静态 `void fn()`；`createWith<T>(task, value)` 在创建时复制初始参数，入口为 `void fn(T)`。消息、通道和初始参数使用同一传值协议：支持标量、字符串及可递归复制的值聚合；引用、不可复制资源、含借用的对象和捕获闭包不能跨线程传递。静态函数值使用 VM 函数标识，不转换为机器函数指针。

`Thread` 是独立的名义类型，不能与普通整数混用，可保存在 `List<Thread>` 或结构体中。句柄只属于当前一次 VM 运行；不能跨运行保存或重新使用。

## 等待、取消与失败

`wait` 等待并回收目标，目标失败时传播原始诊断。`waitFor(thread, millis)` 和 `tryWait(thread)` 成功回收返回 true，超时返回 false；超时不取消目标、不消费句柄。完成回收后不能再次等待。

`waitResult` 和 `waitResultFor` 返回 `ThreadResult`，包含操作状态、原始错误和可用的源码调用栈。`state` 返回 RUNNING、SUCCEEDED、FAILED 或 CANCELLED，回收后仍可查询完成状态。符号压缩会限制可恢复的源码名称。

`requestStop` 是协作取消请求。VM 在执行边界及等待、接收、睡眠、通道操作中检查请求。`sleepFor` 提供可取消睡眠；`stopRequested` 可供任务查询。取消结束当前执行，返回 CANCELLED；它不会自动进入用户的取消处理分支。任意阻塞宿主 I/O 只有返回 VM 后才能观察停止请求，因此长时间 I/O 应设置宿主超时或使用异步轮询。

普通运行失败保留在目标结果中；显式等待可以传播它，线程组会取消其余任务并收集结果。panic 保留原有的运行级传播规则。线程退出释放所属 VM 存储和宿主资源记录；取消或失败不会执行任意用户 Drop、defer 或 scope(exit) 代码。需要执行用户收尾时，应先通过业务消息请求正常退出，再等待完成。主执行结束时请求停止并回收仍在运行的子线程。

## 有界通信

`std::concurrency` 定义统一状态：

| 常量 | 值 | 含义 |
| --- | --- | --- |
| READY | 0 | 本次操作完成 |
| TIMEOUT | 1 | 等待预算耗尽或非阻塞接收无值 |
| CLOSED | 2 | 队列关闭、目标结束或通道已回收 |
| CANCELLED | 3 | 执行被取消 |
| FULL | 4 | 非阻塞发送遇到满队列 |
| FAILED | 5 | 线程或任务失败 |
| INVALID | 6 | 无效句柄 |

`Receive<T>` 同时携带 status 和 value；仅在 `isReady()` 为 true 时读取 value。限时通信的 millis 为非负毫秒，-1 表示无限等待，低于 -1 报错。

线程邮箱容量为 256。`std::message::sendThread<T>` / `receiveThread<T>` 保留阻塞形式；`sendFor<T>`、`trySend<T>`、`receiveFor<T>`、`tryReceive<T>` 提供超时及非阻塞形式。`std::thread` 还提供字符串消息包装。`closeInbox()` 关闭当前邮箱，关闭后排空已有消息；发送方必须处理关闭或目标退出。

跨线程通道使用 **`std::concurrency::Channel<T>`**，区别于 `std::collections` 的本地队列。`create(capacity)` 的容量为 1..1048576；每次运行最多保留 1024 个活动通道。通道句柄可复制，所有副本指向同一队列，载荷仍按值复制。支持多个生产者和消费者，通过满队列等待提供背压。

通道的 send/receive、sendFor/receiveFor、trySend/tryReceive 分别用于阻塞、限时和非阻塞操作。`close()` 可重复调用；关闭后拒绝发送，接收方排空后得到 CLOSED，运行时回收通道槽。等待期间保持队列存储存活，关闭不会使等待者持有失效地址。默认零句柄无效；调用方显式关闭不再使用的通道，运行结束统一清理其余槽。

`Shared<T>` 的 get/set/swap 各自原子，但 get 后 set 不是原子事务。`AtomicInt64::fetchAdd(delta)` 返回更新前的值，溢出按 64 位环绕。不开放任意共享对象引用或持锁用户回调。

## 线程组与线程池

`ThreadGroup.start(task)` 创建并登记线程，`add(thread)` 接管已有线程的等待责任，`cancelAll()` 请求停止。`waitAll()` 等待全部目标，发现普通失败后取消其余任务，返回是否全部成功，并在 error 中保留首个失败诊断。重复 waitAll 返回缓存结果。结束后的组不能再添加任务；离开作用域时取消并等待尚未回收的任务。组拥有等待责任，调用方不要再单独等待其中的句柄；拒绝重复登记、自身和主线程。ThreadGroup 和 ThreadPool 的普通值复制不会克隆底层任务，调用方须使用引用或 move 保持单一收尾责任。Timer 经不可复制的私有 opcode 独占定时源，普通复制在编译期拒绝；id() 和 Event.timer 只作来源标识。

`ThreadPool::create(workers, capacity)` 建立固定工作线程和有界静态任务队列。`submit` / `submitFor` 提交无捕获的 `void fn()`。`closeAndWait()` 关闭提交、排空排队任务并回收线程；任务失败使组停止。`cancelAll()` 取消工作线程并丢弃未开始的任务，随后 closeAndWait 可收集结果。析构取消并回收。工作线程数量为 1..1024，实际创建还受宿主资源限制。

线程池适合短计算任务。长期阻塞 I/O 会占满固定工作线程，应使用专用线程、宿主超时或异步 I/O。池内任务不应等待同一池的未开始任务，也不应向已满的自身工作队列无限提交。

## 显式事件与定时源

`std::event` 的用户代码只在显式 receive / receiveFor / tryReceive 后处理事件。事件含 timer、tag、source 和 missed；宿主事件 timer 为 0。每个运行上下文的事件队列容量为 256，满时投递返回 FULL，调用方决定丢弃、合并或重试。

宿主通过 `NativeCallContext::eventPoster()` 或 `Machine::eventPoster()` 获取弱引用投递器。投递不执行用户回调，目标关闭、运行结束或销毁后返回 CLOSED；投递器可在宿主回调返回后继续使用。投递器和有界队列使用宿主存储，不延长 VM 内存池的生命周期。不得将原始 VM 指针当作跨线程异步调用入口。

`Timer::afterMillis/afterNanos` 创建一次性定时源，`everyMillis/everyNanos` 创建周期源，创建后立即运行。时间参数必须为正，失败得到无效定时源；每个上下文最多 1024 个定时源。共用队列在显式接收时按单调时钟计算到期，不为每个定时器创建 OS 线程，不承诺硬实时。积压周期合并为一次通知，missed 报告额外错过次数；stop 或作用域退出删除定时源及其待处理通知。event.close 关闭定时源，已排队事件仍可排空。

`std::watchdog` 直接在 VM 执行检查中维护超时和喂狗，不调用用户事件处理器。提供宿主纳秒时钟时预算单位为纳秒，否则为执行指令数；禁用时保留默认最高优化路径。

宿主事件回调写成显式接收循环，网络 NET_PENDING 轮询可用有界等待或 `sleepFor` 避免忙循环；语言块表达式的 `yield value;` 与层级 break 保持原有值语义。

完整可运行样例见 [通道](../example/samples/thread_channel.chl)、[线程池](../example/samples/thread_pool.chl) 和 [事件](../example/samples/event_timer.chl)，接口清单见 [线程 API](library/std-thread.md)、[通信 API](library/std-concurrency.md) 与 [事件 API](library/std-event.md)。
