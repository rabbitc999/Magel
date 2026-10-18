# `magel-lsp` —— Magel 宏 / 块的编辑期语言服务器

Magel 的 `$macro` / `$replace` / `$for` 在**编辑期**就能查到配对、
参数与调用问题，另外给出语义高亮 —— 不调编译器，离线可用。

与工具链自带的 [`magel lsp`](../tools/mageldev.py)（内嵌在编译器里的 Python 服务器，
跑真前端 `magel --check --diagnostics=json`）**分工不同**：

| | `magel-lsp`（本工程） | `magel lsp`（编译器内嵌） |
| --- | --- | --- |
| 检查范围 | 宏 / 块的配对、参数、调用与嵌套 | 完整前端（类型、语义、宏展开后的规则） |
| 依赖 | 无（C++17，只用仓库里的最小 JSON 与共用类型） | 需要 `magel` 编译器（含 Python 运行时） |
| 额外能力 | 语义高亮（semantic tokens） | 真实错误码 + 精确位置 |

VS Code 那边两个插件分工也一致：[`magel/vscode/`](../vscode/README.md) 用 `magel lsp`
拿真诊断；本工程可以在需要语义高亮 / 不装编译器时单独接。

## 能力

| 能力 | 说明 |
| --- | --- |
| 诊断 | 14 个稳定短码：`magel-macro-duplicate` / `magel-macro-header` / `magel-macro-params` / `magel-macro-arity` / `magel-macro-unclosed` / `magel-macro-end` / `magel-macro-end-legacy` / `magel-nested-macro` / `magel-replace-header` / `magel-delete-macro` / `magel-macro-object-call` / `magel-match-outside` / `magel-match-end-outside` / `magel-end-for` |
| 悬停 | 宏声明（含形参、`expr` / 参数括号修饰的说明） |
| 补全 | 当前文档里可见的宏（插入 `NAME(x, y)`），按块作用域过滤 |
| 跳转定义 | 宏调用 → 定义处 |
| 文档大纲 | 宏声明 |
| 语义高亮 | `$macro` / `$replace` / `$for` 等指令、宏名 |
| 格式化 | 保守：原文不动（本模型只做扫描，不改写源码） |

## 构建与自检

```sh
make -C magel/source/lsp            # 构建 build/magel-lsp/magel-lsp 与 magel-lsp-tests
make -C magel/source/lsp check      # 单元测试 + 用 library/lsp 的 examples/client.py 驱动真服务器
```

命令行：

```text
magel-lsp --stdio            在标准输入输出上跑 LSP（编辑器默认用法）
magel-lsp --check FILE...    直接校验，输出 FILE:行:列: 级别: 消息 [码]
```

## 与 `library/lsp` 的关系

两个共用件**直接编进本工程**（不复制，免得漂移）：

* [`library/lsp/json.cpp`](../../library/lsp/json.cpp)：最小 JSON；
* [`library/lsp/types.hpp`](../../library/lsp/types.hpp)：位置 / 区间 / 诊断 / 悬停等头文件级共用类型。

模型各管各的：`.gram` 的文法模型在 [`library/lsp`](../../library/lsp/README.md)，Magel 的宏模型
就是本目录的 `mc.hpp` / `mc.cpp`（原来住在 `library/lsp`，2026-10 跟着 Magel 搬过来）。
