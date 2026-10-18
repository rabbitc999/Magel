# LaTeX 书本

本目录维护《玩转 Magel 语言》的 [PDF](book.pdf)，语言版本以根目录 [`VERSION`](../../../VERSION) 为准。章节源稿在 [doc/book](../README.md)，正文章节由 `tools/md2tex.py` 生成；`book.tex`、`preamble.tex` 和封面源文件手写维护。

在仓库根运行：

```sh
python doc/book/latex/tools/md2tex.py
python tests/magel_book_updates.py
```

然后进入 `doc/book/latex`，使用已安装的 TeX Live 与中文字体先生成封面，再构建正文两次，使目录和页码稳定：

```sh
xelatex -interaction=nonstopmode -halt-on-error -output-directory=figure figure/cover.tex
xelatex -interaction=nonstopmode -halt-on-error book.tex
xelatex -interaction=nonstopmode -halt-on-error book.tex
```

生成的 `.tex` 章节和 `book.pdf` 入库，编译日志及辅助文件不入库。完整程序用 `<!-- verify-book -->` 标记，紧接 Magel 代码和预期输出；[书稿回归](../../../tests/magel_book_updates.py) 执行这些程序并检查全部生成章节一致。

正文包括表达式、插值、关联类型、整数泛型、作用域清理、宏补全、捕获错误诊断、行输入、文件文本读取和终端手册，调整表格列宽及行内代码换行，避免长签名挤出页边。


## 维护与校验

读者入口只说明阅读方式；生成流程和回归命令在本页维护。书稿完整程序及 Markdown 与 LaTeX 的一致性由 [书稿回归](../../../tests/magel_book_updates.py) 检查。相关功能回归还包括[返回表达式、具名闭包与派生](../../../tests/magel_declaration_sugars.py)、[多括号宏与源码映射](../../../tests/magel_macro_groups.py)、[泛型单态化](../../../tests/magel_generic_monomorphization.py)和[文本输入](../../../tests/magel_text_readers.py)。


## 示例排版

Magel 代码使用四空格缩进，赋值及二元运算符两侧、逗号后保留空格；函数体和控制分支展开为多行，避免将声明、赋值与退出语句压在同一行。插值字符串的文字部分及宏的原始文本须保持原意。PDF 代码样式使用 `columns=fullflexible` 和 `keepspaces=true` 保留可见的缩进与词间空格，调整后须检查实际页面，并执行书稿程序回归。

所有 `lstlisting` 允许自动换行和跨页，不用 `minipage`／`samepage` 整块保留短示例。列表项后的代码独立排版，不嵌套在 `enumerate` 或 `itemize` 中；拆开后的有序列表保留原编号。书稿回归同时检查这些结构约束。
