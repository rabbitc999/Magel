# 借用控制流与算法验证

借用检查分为两部分：`source/magel_borrow_checker.hpp` 检查引用逃逸，
`source/magel_borrow_flow.hpp` 检查函数内的释放后访问和移动后访问。
前端适配层 `source/epilogue/borrow_flow.cpp` 记录已绑定的变量身份、字段、下标和控制流；
所有函数及泛型实例降级完成后，独立检查器才运行，不访问语义分析符号表。

实现参考 [Rust MIR 数据流分析](https://rustc-dev-guide.rust-lang.org/mir/dataflow.html)
和 [NLL RFC 2094](https://github.com/rust-lang/rfcs/blob/main/text/2094-nll.md)
的控制流分析思路：分支合流合并可能来源，循环通过工作队列迭代到不动点，
再统一诊断可达操作。`break`、`continue`、`return` 和短路表达式保留实际控制流边；
常量布尔条件裁剪不可达边。

每个引用槽和它指向的对象分开建模。别名追踪字段和下标，常量下标可区分，
动态下标按可能重叠处理。`delete` 使堆对象失效，`move` 和显式 `drop()`
使资源对象失效；数值的 `move` 仍按语言既有规则将来源清零。
移动后原绑定禁止读取、写入和重新绑定；只有新声明可以建立可用绑定，不能通过写悬空引用复活被释放的堆对象。
同一分配点再次执行时采用最近实例与更早实例两个抽象身份，新分配不会复活旧别名。
多个独立违规均通过 `MGL1205` 报告，并保留源码位置。

这不是完整的 Rust 借用规则：Magel 仍允许既有的多个可写引用，并未引入独占借用；
逃逸检查仍采用保守的词法作用域规则。控制流检查限于函数内部，普通函数的引用返回
保守关联到引用实参，可见方法体会提取实际返回的字段、形参或新分配来源；不为回调或外部函数自动推导释放副作用。
未建立绑定事实的引用字段不会被当成其父对象；这类未知来源交给运行时检查，避免链表前进时误把下一节点判为已释放的上一节点。
容器内部引用、动态索引及递归字段的复杂别名仍有精度限制；递归路径超过 16 段时加宽，
保证循环分析收敛。运行时边界和引用检查继续保留。

## 数据结构测试

[`borrow_algorithms.mgl`](../tests/fixtures/borrow_algorithms.mgl) 使用实际堆分配节点，
而非用数组模拟指针：

- 链表使用 `Option<Link mut&>`，执行构建、原地反转、逐项验证及迭代释放。
- 二叉搜索树使用 `Option<Tree mut&>`，执行插入、重复值计数、中序验证及后序释放。
- 图使用前向星表示，包含自环、重边和不可达节点；从所有起点交叉检查 DFS 与 BFS。
- 节点析构计数必须等于分配数，标准错误不得出现泄漏提示。

Python 独立生成相同随机输入，以 `deque` BFS 核对距离校验和，
以集合核对 BST 实际分配数，覆盖空结构、单节点、重复值和不同随机种子。

## NOIP 2015《运输计划》

综合测试选用 NOIP 2015 提高组 Day 2 第三题，
[原始题面镜像](https://huang.lt/download.php?id=15296&type=statement)。
目标是把树上的一条边权清零，使给定路径的最长长度尽可能小。
[`noip2015_transport.mgl`](../tests/fixtures/noip2015_transport.mgl) 为独立实现：
迭代树遍历、扁平倍增 LCA、树上差分、二分答案。
图与倍增表使用一块运行时数组，查询使用聚合数组，并与小型泛型容器副本交叉核对，避免深链递归和大表容器扩容的内存峰值。
时间复杂度为 `O((n+m) log n + (n+m) log W)`，其中 `W` 为最长原始路径长度。

测试驱动 [`magel_algorithm_robustness.py`](../tests/magel_algorithm_robustness.py)
在小树上枚举被清零的每一条边，并独立遍历每条查询路径求最优值，
不复用 LCA、差分或二分算法；覆盖链、星、随机树、零权、等权、反向和重复查询，
同时核对原题样例答案 `11`。大规模等权深链使用可直接证明的答案 `(n-2)*1000`。

```sh
python tests/magel_borrow_flow.py --magel magel.exe
g++ -std=c++17 -I. tests/magel_borrow_checker.cpp -o build/borrow-checker-test
python tests/magel_algorithm_robustness.py --magel magel.exe
python tests/magel_algorithm_robustness.py --magel magel.exe --stress-size 300000
```

本次验证已通过原题样例、60 组随机小树的独立暴力对照，以及 AOT 的 `n=m=300000` 等权深链（输出 `299998000`，标准错误为空）。压力测试用于正确性和资源清理验证，不作为原赛事时限的性能承诺。

默认用例分别运行 VM 和 AOT；超过 10000 节点的压力数据只运行 AOT。
`--skip-aot` 可用于单独诊断 VM，`--random-cases` 控制随机小数据数量。
失败时保留 `build/algorithm-robustness/<compiler-name>/last-input.txt`，便于复现。

`drop()` 和 `delete` 均为幂等终结操作：重复调用不再析构或释放，释放后读写仍拒绝。默认堆复用物理内存，同时为每次分配提供不复用的逻辑身份，防止旧引用删除新对象；这项约束同时适用于 VM 与 AOT。默认堆按物理块首保存最新代次和清扫边界，同址复用不追加墓碑存储；逻辑历史身份通过代次范围识别。`sweep()` 显式撤销已释放身份的幂等保证，此后旧引用再次 delete 报 MGL1205；调用者保证清扫时不再需要这些旧引用。完整 watchdog 重启清空墓碑和 arena。

板端 `source/stackvm_mcu/` 使用共享 `library/stackvm/` 核心的生命周期、压缩分配身份及幂等释放。新增 VM/板端算法对照可通过 `--skip-aot --mcu-host build/stackvm_mcu/mcu-host.exe` 运行；桌面与板端的执行优化及内存预算见 [VM 执行优化](12-vm-performance.md)。

局部开发按模块运行门禁，复测保存固定随机种子及独立日志：

```sh
python tools/run_module_gates.py language --random-recheck 1 --seed 20261013
python tools/run_module_gates.py borrow
python tools/run_module_gates.py memory
python tools/run_module_gates.py generics tools docs
```

先用 `make` 构建当前编译器。模块门禁只运行所选范围，不代替发布时各模块及底层 C/C++、VM、板端检查的完整覆盖。
