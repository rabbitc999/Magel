# 17 当前能力与实现边界

循环语法使用 `while`、`loop` 与 `for (... in ...)`；`for-in` 和 `while` 支持带 `else` 的值表达式，以 `yield` 产生结果。List 字面量使用 `[a, b, c]`，Set 使用 `{a, b, c}`。区间使用 `std::lang::range(a, b)`（左闭右开）和 `rangeEq(a, b)`（闭区间），整数和 char 直接支持，自定义遍历类型实现 `NextValue<T>`；区间 match 仅支持整数和 char，普通自定义值匹配使用 `Equal<T>`。

[文档索引](README.md) · [English](en/17-capabilities.md)

本页统一描述当前检出的实现。程序版本读取根目录 [`VERSION`](../VERSION)，变更历史见
[`CHANGELOG.md`](../CHANGELOG.md)。功能以权威文法、标准库、构建配置与回归测试为依据；
历史性能数据保留实际被测版本、配置和时间，不自动代表当前版本或其他平台。

## 语言与资源

支持[返回控制表达式、语句级具名闭包和字段派生](28-declaration-sugars.md)：隐藏结果绑定保留内部移动、外部复制及借用清理，具名函数复用 fn 捕获，`#derive` 仅生成 Equal/Hash/Clone 并检查字段协议，不生成 Drop。

泛型闭包支持 `fn<T>`、实参类型推导、完整特化及唯一最具体的偏特化；可以直接作为结构体成员，并作为泛型类型参数 `F` 的值传递或保存。不同类型调用共享同一闭包的捕获状态，复制与借用规则沿用普通闭包。见[语言参考](04-language-reference.md)。

源码与宏通过 `module` / `import` 复用。生成解析器建立宏 AST，语义分析进行卫生替换与静态类型推导，四元式生成前完成。支持严格有序的三种参数组、类型参数、变长片段、五类完整语法匹配及 ECMAScript 正则。详见[卫生 AST 宏](24-literals-operators-macros.md#编译期卫生-ast-宏)。

入口支持 `int64 main` 和 `void main`，无参或接收 `List<String>`；void 入口正常退出码为 0，panic 仍失败。`struct`、`enum`、`interface` 声明的尾分号可选。

| 范围 | 当前能力与约束 |
| --- | --- |
| 类型 | 定宽整数、float32/float64、bool、char、String、大数；数组、元组、结构体、带载荷枚举、容器及函数类型 |
| 抽象 | 重载、泛型单态化、接口约束、关联类型、常量泛型、默认类型参数、接口默认方法和运行期接口分派 |
| 函数值 | 匿名函数只使用 `fn`；支持内联实参、嵌套高阶函数、const/mut/copy/move 捕获 |
| 控制流 | match 与守卫、整数单值／区间重叠警告、解构、区间迭代、if 表达式、`yield value;` 块表达式、`yield (a, b);` 元组退出、`break level N;` / `break all;` 层级循环退出、直接变量默认移动与带值退出穿透无值循环、宏与条件编译 |
| 错误与清理 | Option、Result、自定义错误传播、`?`、panic、defer、scope(exit)、Drop 与 Closeable |
| 所有权 | 普通拥有值赋值和按值传参复制；`move` 转移资源。移动后的原绑定禁止读写或重绑，完整移动后可重新声明同名变量 |
| 引用 | 非空的只读/可变引用；`Option<T const&>` / `Option<T mut&>` 表示可能没有的引用，None 不构造空引用 |
| 检查 | 独立的借用来源与控制流检查跟踪局部变量、字段、别名、移动、释放及闭包捕获；调用前检查捕获来源，复制、解构和聚合传播保持关联；允许别名共享，不实行全局独占借用规则 |

语言无 GC，拥有值按作用域确定性清理。引用、资源句柄及含引用的聚合仍受各自契约约束；
手动 `new` 的存储需要按接口释放。静态检查不覆盖任意原始内存操作、外部宿主行为或所有并发失效。
字符串下标和切片以 UTF-8 字节为单位，码点操作使用 `std::utf8`。
普通字符串字面量按字节内容合并生成期数据，字面量不能直接修改或可变借用；独立 String 值仍按值复制。
运行时数组形参 `T[]` 复制长度及元素；`T[] &` / `T[] const&` 只读借用，
`T[] mut&` 可变借用。数组引用保留元素布局、运行时长度及来源生命周期。
详见[语言参考](04-language-reference.md)、[核心概念](03-core-concepts.md)与[移动规则](23-ownership-move.md)。

明确的 `Any` 目标支持自动装箱，包括泛型实参、字段、返回值及异构 List/Set/Map；精确解包、清空与借用检查沿用值语义。接口引用可直接绑定实现类型引用，无需 Any 中转；视图仍借用原对象。详见 [Any 与接口绑定](22-any-generics-data.md)。

所有非 void 值支持默认 `toString()` 和字符串转换；结构体 `{...}`、List/数组 `[...]`、Set/Map `{...}`、元组 `(...)`，嵌套字符串加引号并转义。用户 `toString` 与 `ImplicitConvert<String>` 互补缺失入口，专用显式转换保持优先；无反射配置同样可用。详见[值文本规则](04-language-reference.md#默认值文本与字符串协议)。

## 执行、优化与交付

| 方式 | 实现 | 部署要求 |
| --- | --- | --- |
| 源码运行 | 前处理、解析、类型与借用检查、四元式降级后进入 VM | 编译器和实际使用的宿主能力 |
| 字节码 | 共享 VM 校验并加载 `.svmb` | 兼容特征位的 VM、胶水及配套数据 |
| 宿主嵌入 | 宿主加载字节码，以 `stackvm::Machine` 执行 | 目标平台的 VM 宿主和资源适配 |

IR 指四元式，默认执行完整优化流程；VM 默认使用最高可用执行优化，包括寄存器核心、执行准备与融合、跳转折叠及整数专用路径。源码与字节码共用 VM。

交付可使用字节码与宿主，符号压缩不提供加密、完整沙箱或防逆向保证。程序更新不自动迁移正在运行的栈、闭包或全局状态。

## 宿主与反射

Magel 可通过 `abi` 调用 C 动态库，或调用 Registry 注册的 C++ 胶水。引用、结构体、数组、
可选引用、返回内存释放及有限头文件绑定均按 ABI 契约处理；宿主声明必须匹配真实 C 签名。
胶水使用 `NativeCallContext::invoke` 或 `magel_glue::Function` 同步调用 Magel 具名函数、
内联 fn 和闭包，VM 共用接口并检查参数数量、返回值和错误传播。
函数包装与上下文仅在所属宿主回调期间、同一线程同步使用；资源值仍按 Codec 管理所有权。
详见[宿主集成](08-host-integration.md)和[回调示例](../example/native-callbacks/README.md)。

根目录工具链默认同时启用 `MAGEL_WITH_REFLECTION=1` 与 `STACKVM_WITH_REFLECTION=1`，
单次编译也默认启用反射；`--no-reflection` 显式关闭。独立生成编译器工程与目标宿主的默认值
按各自构建配置描述，不能从根目录默认值推断所有消费者。
反射支持注解、显式暴露声明的元数据及受限动态调用，不支持任意动态类型创建、一般字段写入
或任意资源类型的动态调用；不承诺硬实时。完整范围见[反射说明](19-reflection.md)。

## 标准库、目标宿主与工具

标准库覆盖容器算法、迭代器、数学与数值分析、文本与编码、JSON/TOML/CSV、文件与目录、
TCP/基础 HTTP、SQL 驱动、隔离线程、有界通道、消息、显式共享值、显式事件和看门狗。
模块与重载清单以[API 索引](library/README.md)为准，不在总览手工维护模块总数。
HTTP 接口不包含 TLS；外部数据库需要相应客户端和服务。线程使用 `std::thread::Thread`，支持复制初始参数、取消、状态、完成结果、线程组和固定线程池；跨线程通信有容量与等待预算，事件仅显式接收。
宿主事件投递器使用弱引用，旧协程与中断回调已删除；阻塞宿主调用的取消受其超时约束。详见[并发模型](29-thread-concurrency.md)。

桌面与嵌入式 Linux 共用 `library/stackvm/` VM 核心。目标需具备 C++17 工具链、足够的内存和
所需系统服务；原生胶水与依赖库必须按目标架构构建。Linux 设备使用普通宿主，
没有单独的嵌入式部署模式或前端模块禁用配置。内存与指令限制通过通用运行选项设置。
支持范围见[部署说明](06-deployment.md)。不提供单片机、裸机固件或板级 SDK。

统一 CLI 提供工程、检查、格式化、测试、依赖锁定与缓存。LSP 提供诊断、补全、悬停、参数提示
和跨文件跳转，并提供完整 `auto` 类型悬停、可选类型内联提示、多括号宏签名、展开来源导航、
引用查找及基于唯一绑定的安全重命名；所有权错误可附带关联声明和清理位置。重载或宏模板绑定不明确时
拒绝重构，详见 [LSP 说明](man/lsp.md)。DAP 提供断点、步进和调用栈，变量查看与表达式求值尚未实现。
包来源支持本地目录、固定 Git 提交和带摘要的 HTTP/HTTPS 压缩包，仍需持续建设第三方生态。
功能存在、回归通过与长期生产验证分别描述，不把其中一项当作另外两项的证明。

工具界面仅支持中文、英文，其他系统区域回退英文，显式日语选项被拒绝。`man book`、`man keyword` 与 `man skill` 从可执行文件内嵌数据查询书稿、关键字和 AI SKILL；VS Code 包携带同一份 SKILL。

API 手册通过 `magel man api` 查询，省略 `api` 会给出用法错误；`magel man macro` 查看内嵌宏帮助。

`opcode T { new = create; drop = destroy; copy = clone; };` 声明不透明胶水对象。
复制经声明的胶水函数完成；`#NonCopyable` 类型禁止复制，并省略 copy。
struct 可以拥有多个 opcode 字段。BigInteger、BigDecimal、File、Process 的记录及操作位于
标准库胶水，VM 核心只管理通用 NativeRecord 壳及宿主服务表。File、Process 不可复制；
公开进程类型名为 Process。List、Map、Set 使用参数化 opcode 类型和标准库不透明记录，
前端不再声明 Container Kind，VM 不再声明集合专用 Kind 或实现集合方法。List 使用 ArrayList；
内建标量键与实现 Hash<T> 的自定义键使用 HashMap / HashSet，只有 Equal<bool,T> 而没有 Hash<T>
的自定义键退化为线性查找的 ArrayMap / ArraySet。哈希碰撞通过 Equal 区分。
Map、Set 的 keyAt、valAt、at、遍历及默认文本均不承诺排序或插入顺序。String 的完整隔离仍在推进。


Image 使用可深复制的 opcode，宽高与连续 RGBA 像素由胶水拥有；width()/height() 查询尺寸，pixels() 导出独立列表快照。TcpSocket、TcpListener 使用不可复制的 opcode，作用域退出自动关闭，显式 move 转移所有权，各连接独立保存读状态与错误。线性代数 Vector<T>、Matrix<T> 保留泛型聚合外观，ScalarArray<T> 的 ScalarStorage opcode 保存内建整数与浮点数值，复制缓冲互不影响。字符串拼接、切片、比较、查找及数值转文本由标准胶水实现；内部入口统一使用 __std_ 前缀，VM 保留执行控制、内存安全与闭包等核心机制。

网络异步入口使用非阻塞轮询和 NET_PENDING，未就绪时采用可取消线程睡眠或在专用线程使用同步 I/O，避免忙轮询；connectAsync 首批仅接受数值 IPv4，域名解析保留在同步 connect。sendAsync 返回部分发送字节数并接受 offset；receiveLineAsync 保留未完成行，缓冲上限为 1 MiB。

隔离线程的 `waitFor(thread, millis)` 支持非负毫秒超时，`tryWait(thread)` 提供零超时等待；成功完成并回收返回 `true`，超时返回 `false` 且不取消目标。目标失败仍传播原始诊断；句柄成功回收后不能再次等待。

## VM 模块与 TOML 保存

共享 VM 按四元式、二进制容器、执行器、原生服务与并发通信组织独立编译单元；构建和导出使用 library/stackvm/core_sources.txt。公开接口由 stackvm.hpp 汇总，非模板实现独立链接。

std::toml 文档根节点保存时保留原文注释。底层 C++ 值模型的等形编辑保留布局；结构编辑保留注释内容并可调整位置。重复解析保存不丢失或重复注释，详见 [TOML 接口](library/std-toml.md)。
