# 40 名称冻结：ChaLi 五个字母

语言名 **ChaLi** 不是缩写拼出来的好听词，是五条契约的压缩写法：每个字母锁一条**不可回退**
的语言约定。本页把这五条定死，并给出每条可以当场验证的判据——判据拿不掉，字母就改名。

配套的两份冻结清单管另外两层：[21 语义契约](21-semantics-frozen.md) 管"值怎么流、什么时候释放"，
[39 语法冻结清单](39-syntax-frozen.md) 管"字怎么拼、结构长什么样"。本页管"这个名字承诺了什么"。

## 0 名字本身也一并冻结

| 项目 | 冻结写法 | 说明 |
| --- | --- | --- |
| 语言名 | `ChaLi`（C 与 L 大写） | 文档、帮助、手册里一律这个拼写 |
| 程序文件后缀 | `.chl` | 源码文件；工具链按后缀识别 |
| 诊断码前缀 | `CHL` | 形如 `CHL1204`；码值本身由诊断体系定，本页只锁前缀 |
| 可执行文件 | `ChaLi.exe`（Windows） | 命令行仍写小写 `chali`，Windows 下按文件名不区分大小写命中 |
| 工程配置 | `chali.toml` / `chali.lock` | 项目清单与依赖锁 |

历史名称只保留在 [`CHANGELOG.md`](../CHANGELOG.md) 里——那是变更史实，不回改。

## 1 C — Copy（深拷贝值语义）

**契约**：赋值就是彻底的复制，绝不共享内存。大对象必须显式用引用传。

**判据**

- 普通赋值与按值传参复制，修改副本不影响来源；字符串、容器与拥有字段都沿值语义处理
  （[03 核心概念 §1 值语义与显式移动](03-core-concepts.md)）。
- 可跑样例：`example/samples/struct_value.chl`、`example/samples/struct_container_field.chl`
  —— 改副本后来源保持原值。

**什么算违反**

- 赋值或按值传参之后，改副本连带改到来源（说明底下偷偷共享了内存）。
- 大对象默认按引用传递、不留显式的引用类型标记——这等于把"复制"变成可选的隐式行为。

## 2 H — Hygienic（卫生宏）

**契约**：在编译期做安全的代码替换，绝不意外捕获或污染外部变量。

**判据**

- 宏体里的名字经卫生改名，不会撞上调用处的同名变量：可跑样例 `example/samples/macro_gensym.chl`。
- 宏定义可跨模块按限定名调用（`$std::lang::assert(...)`），可见性沿用既有规则
  （`std::lang` 始终可见，其余模块须先 `import`）：可跑样例 `example/samples/macro_qualified.chl`，
  未定义时指向调用点报「未定义宏」。

**什么算违反**

- 展开结果捕获了调用处同名的变量，或把宏体内部的临时名泄漏进调用处的作用域。
- 限定名调用绕过可见性规则（`import` 之前就能点到别的模块的宏）。

## 3 A — Any（万能盒子 / 类型擦除）

**契约**：没有泛型约束时，可以装下任何类型的数据。

**判据**

- `Any` 装值时擦除具体类型，取回时按声明的类型校验：可跑样例 `example/samples/any_generics.chl`；
  规则见 [22 Any 与泛型数据](22-any-generics-data.md)。

**什么算违反**

- `Any` 只能装有限几种类型（那就退化成联合，不是万能盒子）。
- 取回时不校验、把装进去的类型和取出的类型对不上当成运行期未定义行为。

## 4 L — Lifetime（生命周期 / 借用检查器）

**契约**：引用永远不能活得比它的数据源更久，否则编译直接报错（`CHL1204`）。

**判据**

- 借用检查在编译期拦三类越界（实现见 `source/chali_borrow_checker.hpp`）：
  `保存的引用不能超过来源的生命周期`、`返回值中的引用不能超过局部来源的生命周期`、
  `按值实参中的 Any 不能隐藏非全局引用来源，请使用显式引用类型传参`。
- 合法用法可跑：`example/samples/borrowed_aggregate_return.chl`、
  `example/samples/borrowed_index_values.chl`；
  被拦下的写法见 `example/samples-error/iter_erase_borrowed.chl`、
  `example/samples-error/iter_transfer_borrowed.chl`。

**什么算违反**

- 让越界引用通过编译，把它推迟成运行期的野引用或未定义行为。
- 把 `CHL1204` 降级成警告——契约要求的是"编译直接报错"。

## 5 I — Interface（接口 / 运行期多态）

**契约**：定义一组共同的"契约"，运行期根据对象的实际类型进行分派。

**判据**

- 接口方法按接收者的实际类型在运行期分派：可跑样例 `example/samples/interface.chl`、
  `example/samples/interface_protocols.chl`、`example/samples/interface_view_ownership.chl`；
  默认实现与继承见 `example/samples/interface_defaults.chl`、
  `example/samples/interface_inherit.chl`。

**什么算违反**

- 退化成静态分派（编译期就把目标定死，运行期不再看实际类型）。
- 允许"实现了部分方法"的对象通过接口调用未实现的方法而不报错。

## 6 改名与验收

五个字母各自对应上面的判据；任何一条判据失效，就不是 ChaLi。改动语言名、程序后缀、诊断码前缀
或可执行文件名时，下面几处必须一起动，不能只改一半：

- 帮助与手册开头印出的五字母头图（`source/chali_driver.cpp` 的 `PrintUsage`、
  `tools/native_man.hpp` 的 `man` 用法），中英两版由 `source/i18n/messages.tsv` 提供。
- 版本串各处（`VERSION`、`source/chali_package.hpp`、语言服务器、`tools/chali_dev.py`、
  VS Code 扩展的 `chaliCompilerVersion`）。
- 工具链文档 [09 工具链](09-toolchain.md) 与 [01 快速开始](01-quickstart.md) 里的命令与后缀示例。
