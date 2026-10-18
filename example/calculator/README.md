# Magel 计算器工具链示例

这里包含两种 AST 表示的同一个表达式解释器：词法分析、递归下降解析、指令生成、栈机执行和逐行错误报告都用 Magel 编写。

| 文件 | 用途 |
| --- | --- |
| [calc.mgl](calc.mgl) | 扁平 AST：`List<Node>` 与子节点索引表，包含泛型接口约束、容器及迭代器组合 |
| [calc_nested.mgl](calc_nested.mgl) | 嵌套 AST：枚举载荷保存 `List<Expr>`，验证递归拥有值的复制与清理 |
| [repro_recursion.mgl](repro_recursion.mgl) | 最小递归遍历，子调用返回后父帧仍应保留 `kind`、子节点数量与字符串 |

从仓库根目录运行：

```powershell
magel/magel.exe run magel/example/calculator/calc.mgl
magel/magel.exe run magel/example/calculator/calc_nested.mgl
magel/magel.exe aot magel/example/calculator/calc.mgl --out build/calculator.exe
build/calculator.exe
```

默认输入包含 12 行。正确结果为 7 行成功、5 行应用错误；`min(x, y, z)` 因只接受两个参数而失败，另外四个错误涉及除零、语法错误和未定义变量。第五行 `sqrt(z) + abs(-4)` 约为 13.433981，第六行结果为 908，第八行结果为 1368。两种示例的输出精度和排版不同，数值含义一致。

扁平 AST 是一种数据组织选择，嵌套枚举与递归拥有局部变量也必须遵守值语义。原嵌套版本曾暴露编译器的递归存储问题；此处保留它作集成回归，文件名不再标为 buggy。扁平版本也直接保存局部 `Node`，不再通过每次回读下标规避递归问题。

回归入口为 `python tests/magel_recursive_ownership.py`，同时检查解释执行、AOT、两种表示的数值与错误报告，以及资源诊断。最小单元样例在 `example/samples/recursive_*`、`nested_enum_ownership.mgl` 和 `set_key_protocol.mgl`。
