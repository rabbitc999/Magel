# 39 语法冻结清单

本页列**已经冻住、可以依赖**的源码写法，以及改语法时必须一起动的地方。
权威定义始终是 [`source/chali.gram`](../source/chali.gram) 与词法声明
[`source/chali_token.gram`](../source/chali_token.gram)；语义规则见[语义契约与兼容边界](21-semantics-frozen.md)，
逐条 EBNF 见[语法参考](14-grammar-ebnf.md)。三者冲突时以文法文件为准，本页只做冻结范围的索引与验收清单。

## 1 "冻结"指什么

- **冻结**：已写进文法、样例、书稿与语言参考的写法，形状与含义不再变。已有程序不用改写就能继续编译；
  新增能力只能加在未被占用的位置（新关键字、新后缀、新指令），不重新解释已有 token。
- **不冻结**：诊断文案与列号细节（只保证码值与位置）、内部节点名与四元式文本、生成器内部临时名
  （`__temp_t<n>`、`__chali_*`）、宏展开后的内部名字、优化后的指令序列。依赖这些的输出随时可能变。
- **分工**：本页管"字怎么拼、结构长什么样"；[21 语义契约](21-semantics-frozen.md) 管"值怎么流、什么时候释放"。

## 2 词法层

| 项目 | 冻结写法 | 位置 |
| --- | --- | --- |
| 标识符 | `[A-Za-z_][A-Za-z0-9_]*`，区分大小写，不支持 `$`（`$` 只属于宏调用名） | `%identifier` |
| 空白 | 空格、制表、`\r`、`\n`；不缩进敏感 | `%space` |
| 注释 | `// 行注释`、`/* 块注释 */`；注释与字符串里的文字不当作关键字 | `%trivia` |
| 十进制整数 | `[0-9]+`；超出 `int64` 报越界，唯一例外是紧跟在 `-` 之后的 `9223372036854775808` | `NUMBER` |
| 浮点 / 无符号 / 十六进制 / 二进制 | `1.5` / `1.5e3` / `1.5f` / `42u` / `0xFF` / `b1010` | `FLIT` `ULIT` `HEXLIT` `BINLIT` |
| 字符串 / 字符 | `"..."`（支持转义）、`'x'`；`f"..."` 在词法期归一化为插值标记 | `STRING` `CHRLIT` `FSTRING` |
| 泛型实参边界 | 源码写 `List<int64>`，词法期由归一化层改写成内部标记 `@<` `@>`；原始 `<` `>` 始终是比较 | `TYPE_ARGS_BEGIN` / `TYPE_ARGS_END` |

内部标记（`@annotation`、`@!macrobody`、`@!macroparse_*`、`@!<`、`__chali_fstring`、`__typeTag`）
由预处理与归一化层产生，**不是**用户可以书写的语法。

## 3 关键字冻结表

关键字逐个登记为 `%token 名字 "字面量"`，共四组同步维护（文法、原生语言服务、`chalidev` 补全、VS Code 高亮）。

| 组 | 词 |
| --- | --- |
| 内建类型 | `int8` `int16` `int32` `int64` `uint8` `uint16` `uint32` `uint64` `float32` `float64` `bool` `char` `void` |
| 字面量与通配 | `true` `false` `_` |
| 声明与修饰 | `let` `const` `mut` `static` `inline` `copy` `move` |
| 类型定义 | `struct` `enum` `interface` `opcode` `impl` `type` `abi` |
| 模块 | `module` `import` |
| 控制流 | `if` `elif` `else` `while` `for` `in` `loop` `match` `break` `continue` `return` `yield` `defer` `scope` |
| 内存与资源 | `new` `heap` `drop` |
| 函数值 | `fn` |
| 编译期指令 | `#NonCopyable` `#derive` `#No<接口>Replace` `#NoRight<接口>` |
| 宏语法词 | `$macro` `$replace` `$end_macro` `$match` `$else` `$end_match` `$regex` `$end_regex` `$for` `$end_for`，匹配类别 `is` `expr` `ident` `block` `stmt` |

约定：

- `let` 是**唯一**的类型推导声明关键字；`auto` 不是关键字，按普通标识符处理。
- `heap` 只在分配位置有意义，不能当变量名；`_` 只作匹配兜底与占位，不参与命名。
- 宏调用名保留 `$` 前缀且**只认小写**：`$Macro` 不是宏调用，按语法错误处理。
- `is` / `expr` / `ident` / `block` / `stmt` 只在 `$macro … $match … is` 结构里是关键字。

## 4 标点与优先级

运算符优先级由 `%left` / `%right` 的**声明顺序**决定（先声明先结合），可书写运算符从低到高：

```text
||  &&  |   ^   &   == !=   < <= > >=   << >>   + -   * / %   ~   !
```

`%left AT ANNOTATION` / `%left LPAREN` / `%left LBRACKET LBRACE` 这几条只用于消解归约冲突，
不是用户可以书写的优先级；注解 `@名字` 与括号、字面量的位置由产生式约束。

其余冻结的记号：`++` `--` `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=` `->` `...` `..` `.`
`::` `=>` `?` `(` `)` `{` `}` `[` `]` `;` `,` `:`。

- `->` 只用于函数类型与 lambda 返回类型，不用于成员访问（成员是 `.`）。
- `...` 用于变长形参、函数类型参数与宏参数片段；`..` 只在宏参数里作变长片段标记，两者不能互换。
- `?` 是**后缀**传播运算符，必须紧跟表达式，不能前置。
- `::` 用于模块与类型限定（`Result<int32, int32>::Ok`、`std::text::Text`）。

## 5 冻住的结构形状

| 结构 | 冻结写法 | 备注 |
| --- | --- | --- |
| 入口 | `void main()`，可无参或接收 `List<String>` | 返回类型必须是 `void` |
| 模块与导入 | `module 名字;` / `import 模块::名字;` | 每条独立一行 |
| 结构体 | `struct 名字 { … }`，尾分号可选 | 定义时写 `struct`，引用类型时只写名字 |
| 枚举 | `enum 名字 { 变体, 变体(类型) }` | 带载荷变体用括号 |
| 接口 | `interface 名字 { 函数声明; }`，可 `: 基接口` | 字段声明被拒绝 |
| 不透明对象 | `opcode 名字 { new = 模块符号; }` | `#NonCopyable` 写在 `opcode` 前 |
| 宿主类型 / 函数 | `type 名字 = 类型;` / `返回类型 名字(形参);` | 只有声明，没有函数体 |
| C ABI | `abi "库" { 返回类型 名字(形参) = "符号" ["头文件"]; }` | 头文件用方括号包一个字符串，必填 |
| 接口绑定 | `impl 类型 : 接口 = 符号;` | 宿主实现登记 |
| 函数 | `返回类型 名字(形参) { … }`，可 `inline` | 运算符重载写 `返回类型 operator+(左, 右) { … }` |
| 全局变量 | `类型 名字;` / `类型 名字 = 表达式;` | 顶层、不在函数内 |
| 局部声明 | `let 名字 = 表达式;`、`类型 名字 = 表达式;`、`let (a, b) = 表达式;` | 解构只能用 `let (...)` |
| 具名函数 | `返回类型 名字(形参) { … }`，可与 `let` 声明同处语句位置 | 返回类型是参数与函数体的类型锚点 |
| 引用 | `T&`（只读）、`T const&`、`T mut&` | 引用非空；`Option<T &>` 表示可能没有 |
| 数组 | `T[]`（运行时长度，中括号写在类型一侧） | 元素个数由字面量或实参决定 |
| 函数类型 | `fn(参数) -> 返回` | 不含参数名 |
| 语句 | 块、`if/elif/else`、`while`、`loop`、`for (x in 源)`、`match`、`return`、`break`、`continue`、`yield`、`defer`、`scope(exit)`、表达式语句 | 表达式语句必须以 `;` 结束 |
| 层级退出 | `break level N;` / `break all;` / `break level all;` / `yield 值;` | 层级退出只能这样写 |
| 堆分配 | `heap T;` / `new T(实参);` / `heap new T(实参);` | 分配一律写出类型 |
| 集合字面量 | `[a, b]`（List）、`{a, b}`（Set）、`{k: v}`（Map） | 空 `[]` / `{}` 需要上下文 |
| match 分支 | `模式 => { … }`，`_` 兜底，`|` 连接备选模式 | 不穿透、不写 `break` |
| 值表达式 | `if` / `match` / `loop` / 块可作表达式，用 `yield` 出值 | `yield (a, b);` 产生元组 |
| 宏定义 | `$macro 名字(组…) @!macrobody … $end_macro` | 参数组分 `()` `[]` `{}` 三种，组内可写变长片段 |
| 编译期指令 | `#derive(Equal, Hash, Clone)`、`#NoAddReplace`、`#NoRightAdd` | 写在类型声明前 |

## 6 改语法时必须同步的地方

按顺序做完，缺一项门禁就会红：

1. 改 `source/chali_token.gram`（词法）或 `source/chali.gram`（产生式），语义动作只调用语义层。
2. 重新生成解析器并重建编译器：
   ```sh
   ENGINE=/g/Desktop/ParseEngine/build/bin/ParseEngine.exe sh gen.sh
   make -j4
   ```
   只改 `chali_semantics.cpp` 而跳过 `gen.sh`，用的仍是旧解析器快照，改动不生效。
3. 重建 EBNF 附录：`python tools/export_chali_ebnf.py`（`--check` 只比对）。
4. 关键字四路同步：`python tools/check_keywords.py`（文法、原生语言服务、`chalidev` 补全、VS Code 高亮）。
5. 编辑器侧：`vscode/syntaxes/chali.tmLanguage.json`、`vscode/src/macros.js`、语言服务的悬停与诊断。
6. 样例与期望：`example/samples/`、`example/samples-error/` 及对应 `expected/`、`expected-error/`；
   关键字长度变化会移动诊断列号，期望文件要按实际输出更新。
7. 文档：本页与[语言参考](04-language-reference.md)、[语法参考](14-grammar-ebnf.md)、书稿对应章节、
   `tools/changelog_data.json`（再 `python tools/changelog.py --build`）、`VERSION` 与各处版本号。
8. 门禁：`sh check.sh`（样例与泄漏文案）、`sh ci.sh`（快速）/ `sh ci.sh --full`（发版）。

## 8 核对命令

```sh
python tools/export_chali_ebnf.py --check   # EBNF 附录与文法一致
python tools/check_keywords.py              # 关键字四处一致
python tools/check_doc_links.py             # 文档相对链接完整
python tools/changelog.py --check           # CHANGELOG 与 JSON / VERSION 对齐
sh check.sh                                 # 样例输出与错误位置
```

## 9 不在这份清单里的东西

- 诊断码的含义与归类见[错误速查](book/A-常见错误速查.md)与[编译器安全](37-compiler-safety.md)；
  文案措辞不冻结。
- 标准库 API、模块划分与重载集合见 [API 索引](library/README.md)，属于库契约，不是语法契约。
- 生成的四元式与 VM 指令序列见[执行方式与部署边界](25-execution-delivery.md)；优化会改变指令数量与临时名。
