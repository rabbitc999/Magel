# panic、源码行号与隐藏符号策略

先运行 samples-error/assert_failure.chl 看一次预期失败，再比较源码位置、函数栈和压缩隐藏策略。错误样例应以失败退出；它不是正常程序回归中的成功用例。

## 一次最小失败

[`assert_failure.chl`](../example/samples-error/assert_failure.chl) 的完整输入为：

```mc
void main() {
    assert(2 < 1);

}
```

从仓库根目录执行，命令预期返回非零：

```sh
mkdir -p build/doc-example
build/work/chali/chali run chali/example/samples-error/assert_failure.chl --debug --out build/doc-example/assert-failure
```

[正式错误期望](../example/expected-error/assert_failure.txt) 的消息为 `chali: 运行期错误：panic：assert(x) 的 x 为假`。--debug 还会输出执行轨迹、失败位置和可用调用记录，所以不能把完整 stderr 与这一行直接做严格相等比较；专项测试分别检查消息、位置与帧顺序。

## 有消息、有函数栈和有源码位置是三件事

默认开启 STACKVM_WITH_PANIC_TRACE=1，普通程序 panic 可记录函数栈；保存字节码与导出程序须使用 --debug 才保留源码位置。没有调试表时仍可有函数/指令快照，但不能伪造文件行号。压缩符号策略优先隐藏路径、调用记录和编号栈，即使同时有 --debug；用户主动写入错误消息的文本仍原样保留。

| 配置 | 消息 | 自动调用栈与位置 |
| --- | --- | --- |
| 普通程序，默认栈宏 | 保留 | 可记录函数栈，源码位置取决于调试信息 |
| STACKVM_WITH_PANIC_TRACE=0 / NO_CHALI_PANIC_TRACE | 保留 | 关闭自动失败栈，显式调试设施独立 |
| --compress-symbols 或 chali 压缩 | 保留 | 隐藏路径、函数名、调用记录与编号栈 |

宏/include/import/嵌套调用和多线程终止不靠这个最小程序推断；分别由 test_chali_source_lines.py 与 test_chali_panic.py 的专用输入检查。下面给出其运行时边界、宿主查询和构建方式。

## 整个程序停止

VM 的线程组共享首次 panic 信号：主线程、忙循环兄弟线程及等待消息/线程结束的任务都会在执行检查点停止，等待者同时被唤醒。程序退出时自动回收未显式等待的线程，并返回失败；关闭调用栈宏或压缩符号不会关闭这个终止行为。重新运行或 `begin()` 会创建新的线程组，清空上次失败信号。

## 编译宏

```sh
cmake -S chali/source/stackvm -B build/vm-small -DSTACKVM_WITH_PANIC_TRACE=OFF
cmake --build build/vm-small
# 直接编译库或生成的单文件 C++：库与调用方应使用一致的宏。
g++ -DSTACKVM_WITH_PANIC_TRACE=0 ...
g++ -DNO_CHALI_PANIC_TRACE ...
```

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

VM 帧提供 `function`、`instruction` 与 `location`；递归每层、直接/间接调用和当前执行上下文共享同一份真实执行帧，采集发生在退栈之前。每次 `run()` 清空上次快照，线程子 VM 继承隐藏策略。无调试表时不会伪造源码行号。

编译器的错误首行继续是 `chali: 运行期错误：panic：…`，后续行才附位置和调用栈；`M_LastError()` 也包含这份多行诊断。正常程序的输出与返回值不变。

## 回归

| 用例 | 自动验证 |
| --- | --- |
| 嵌套函数 panic | 三帧按外到内排列，最内层为 `inner` |
| 去调试表 | 仍有函数和指令快照，不伪造路径 |
| 压缩容器 + 外部调试表 | 消息保留，路径/调用记录/宿主快照均为空 |
| 关闭宏 | 错误语义相同，VM 栈查询为空 |
| chali 默认与 CLI 覆盖 | 默认压缩隐藏，`--no-compress-symbols` 恢复栈 |
| VM 可执行导出 | 普通版显示栈，压缩选项版即使有 `--debug` 也只显示消息 |
| 多线程全局停止 | 主线程/子线程 panic、忙循环兄弟、消息等待、显式等待及自动回收均停止；保留首个 panic 线程栈 |
| 原有 VM 回归 | 开/关两种配置的 312 项断言通过 |

```sh
ctest --test-dir build/symbols --output-on-failure
python chali/tests/test_chali_panic.py --chali chali/chali --exports
```

## ChaLi 行级来源

前端记录语法节点起始行，降级与发射期间将原始位置写成 `.quad` 的 `# chali-source: 行号 "文件"` 元数据。组装器将其写入已有 SVMB 调试节，因此运行时直接显示 `.chl:行号`；没有来源元数据的旧四元式仍使用 `.quad` 行号。

`import` 引入的模块 使用各自文件中的原始行号。宏正文指向定义文件的正文行，并显示 `[宏 名称]`；嵌套宏指向实际执行的内层宏。退出展开后恢复调用文件行号，`$line` / `$file` 等宏元变量的既有调用处语义保持不变。

验证：`python chali/tests/test_chali_source_lines.py --chali chali/chali --exports`。
