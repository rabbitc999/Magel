# panic、源码行号与隐藏符号策略

先运行 samples-error/assert_failure.mgl 看一次预期失败，再比较源码位置、函数栈和压缩隐藏策略。错误样例应以失败退出；它不是正常程序回归中的成功用例。

## 一次最小失败

[`assert_failure.mgl`](../example/samples-error/assert_failure.mgl) 的完整输入为：

```mc
int64 main() {
    assert(2 < 1);
    return 0;
}
```

从仓库根目录执行，命令预期返回非零：

```sh
mkdir -p build/doc-example
build/work/magel/magel run magel/example/samples-error/assert_failure.mgl --debug --out build/doc-example/assert-failure
```

[正式错误期望](../example/expected-error/assert_failure.txt) 的消息为 `magel：运行期错误：panic：assert(x) 的 x 为假`。--debug 还会输出执行轨迹、失败位置和可用调用记录，所以不能把完整 stderr 与这一行直接做严格相等比较；专项测试分别检查消息、位置与帧顺序。

## 有消息、有函数栈和有源码位置是三件事

默认开启 STACKVM_WITH_PANIC_TRACE=1，普通程序 panic 可记录函数栈；保存字节码与导出程序须使用 --debug 才保留源码位置。没有调试表时仍可有函数/指令快照，但不能伪造文件行号。压缩符号策略优先隐藏路径、调用记录和编号栈，即使同时有 --debug；用户主动写入错误消息的文本仍原样保留。

| 配置 | 消息 | 自动调用栈与位置 |
| --- | --- | --- |
| 普通程序，默认栈宏 | 保留 | 可记录函数栈，源码位置取决于调试信息 |
| STACKVM_WITH_PANIC_TRACE=0 / NO_MAGEL_PANIC_TRACE | 保留 | 关闭自动失败栈，显式调试设施独立 |
| --compress-symbols 或 magel 压缩 | 保留 | 隐藏路径、函数名、调用记录与编号栈 |

宏/include/import/嵌套调用和多线程终止不靠这个最小程序推断；分别由 test_magel_source_lines.py 与 test_magel_panic.py 的专用输入检查。下面给出其运行时边界、宿主查询和构建方式。

## 整个程序停止

VM 和 AOT 的线程组共享首次 panic 信号：主线程、忙循环兄弟线程及等待消息/线程结束的任务都会在执行检查点停止，等待者同时被唤醒。程序退出时自动回收未显式等待的线程，并返回失败；关闭调用栈宏或压缩符号不会关闭这个终止行为。重新运行或 `begin()` 会创建新的线程组，清空上次失败信号。

这是运行时协作停止：Magel 指令与 AOT 基本块检查取消信号。宿主提供的原生函数若永久阻塞且不返回，运行时无法安全强制杀死该 C++ 线程；原生接口需要可取消或有界的阻塞机制。嵌入宿主收到失败结果，可查询快照；独立可执行程序返回非零状态。

## 编译宏

```sh
cmake -S magel/library/stackvm -B build/vm-small -DSTACKVM_WITH_PANIC_TRACE=OFF
cmake --build build/vm-small
# 直接编译库或生成的单文件 C++：库与调用方应使用一致的宏。
g++ -DSTACKVM_WITH_PANIC_TRACE=0 ...
g++ -DNO_MAGEL_PANIC_TRACE ...
```

关闭宏后，VM 每个函数帧的额外函数符号字段、AOT 活动调用栈/错误快照存储及自动捕获分支不编译进去；既有执行所必需的返回地址、局部变量和显式调试设施仍保留。关闭后保留空查询接口，使用方不必换 API。公开 `DebugFrame` 新增 `instruction`、`threadId` 和 `coroutineId`，更新库与调用方后须重新编译，不混用旧对象文件。

`magel` 已继承 magel 的压缩默认值及 CLI 覆盖。直接解释运行会通过 `setPanicDetailsEnabled(false)` 禁止详情；VM/AOT 可执行导出自动向 magel-emit 转发 `--no-panic-details`，在生成代码里定义 `NO_MAGEL_PANIC_TRACE` 并关闭调试表/行信息，因此即使同时给 `--debug` 也不打印 panic 调用记录。独立 magel-emit 可显式使用这个开关；它是诊断隐藏选项，不意味着 C++/AOT 产物采用压缩字节码格式。

## 宿主查询接口

```cpp
stackvm::Machine machine;
const auto result = machine.run(program);
if (!result.ok) {
    // result.error 保留 panic 消息；压缩产物的下面两个查询均为空。
    const auto& trace = result.sourceTrace;
    const auto snapshot = machine.callStack();
}
```

VM 帧提供 `function`、`instruction` 与 `location`；递归每层、直接/间接调用和当前协程共享同一份真实执行帧，采集发生在退栈之前。每次 `run()` 清空上次快照，线程子 VM 继承隐藏策略。无调试表时不会伪造源码行号。

AOT 生成器在每个函数作用域插入可裁剪的 `RuntimeCallFrame`，首次错误在最内层函数退出之前保存快照。`RuntimeBridge::callStack()` 可在 `end()` 后读取，下一次 `begin()` 清空。活动帧按宿主线程隔离，并以互斥锁保护快照；任一线程触发 panic 后，上层保留最先触发线程的真实调用栈及线程编号，不将不同线程的栈拼接。内存不足时 AOT 析构捕获为尽力提供，保留原始错误。

编译器的错误首行继续是 `magel：运行期错误：panic：…`，后续行才附位置和调用栈；`M_LastError()` 也包含这份多行诊断。正常程序的输出与返回值不变。

## 回归

| 用例 | 自动验证 |
| --- | --- |
| 嵌套函数 panic | 三帧按外到内排列，最内层为 `inner` |
| 去调试表 | 仍有函数和指令快照，不伪造路径 |
| 压缩容器 + 外部调试表 | 消息保留，路径/调用记录/宿主快照均为空 |
| 关闭宏 | 错误语义相同，VM/AOT 栈查询为空 |
| 多次运行与 AOT 生命周期 | 失败栈可查询，新运行清空，正常返回不产生失败快照 |
| magel 默认与 CLI 覆盖 | 默认压缩隐藏，`--no-compress-symbols` 恢复栈 |
| VM/AOT 可执行导出 | 普通版显示栈，压缩选项版即使有 `--debug` 也只显示消息 |
| 多线程全局停止 | 主线程/子线程 panic、忙循环兄弟、消息等待、显式等待及自动回收均停止；保留首个 panic 线程栈 |
| 原有 VM 回归 | 开/关两种配置的 312 项断言通过 |

```sh
ctest --test-dir build/symbols --output-on-failure
python magel/tests/test_magel_panic.py --magel magel/magel --exports
```

源码入口：[VM 头文件](../library/stackvm/stackvm.hpp)、[VM 执行](../library/stackvm/parts/execution.cpp)、[AOT 桥](../library/stackvm/parts/aot_bridge.cpp)、[AOT 生成器](../source/magel-emit/aot.cpp)。

## Magel 行级来源

前端记录语法节点起始行，降级与发射期间将原始位置写成 `.quad` 的 `# magel-source: 行号 "文件"` 元数据。组装器将其写入已有 SVMB 调试节，因此运行时直接显示 `.mgl:行号`；没有来源元数据的旧四元式仍使用 `.quad` 行号。

`%include` 与 `import` 使用各自文件中的原始行号。宏正文指向定义文件的正文行，并显示 `[宏 名称]`；嵌套宏指向实际执行的内层宏。退出展开后恢复调用文件行号，`$line` / `$file` 等宏元变量的既有调用处语义保持不变。

解释运行与 `--debug` 执行轨迹使用这份映射。保存字节码或导出时以 `--debug`（或 magel 的 `debug=true`）保留行信息；AOT 同时生成对应的 `#line` 和运行时帧位置。`--compress-symbols` 仍优先隐藏 panic 路径与调用记录。

验证：`python magel/tests/test_magel_source_lines.py --magel magel/magel --exports`。
