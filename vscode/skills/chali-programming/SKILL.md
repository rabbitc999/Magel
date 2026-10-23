---
name: chali-programming
description: 编写、修改与调试 ChaLi (.chl) 程序，依据实际文法和标准库选择语法、宏、值语义、借用与错误处理方式，并使用 ChaLi 工具链验证。用于 ChaLi 应用代码和示例，不替代编译器或 VM 内部实现的仓库规范。
---

# 编写 ChaLi 程序

目标是交付能被实际 ChaLi 编译器检查、运行的 `.chl` 程序。ChaLi 使用 C 风格声明和块，但类型、所有权与运行时行为由 ChaLi 自己定义，不要照搬 C++、Rust 或 JavaScript 的泛型、所有权与函数语法。

## 先查询随程序分发的手册

定位已安装的 `chali`（Windows 为 `ChaLi.exe`）。语言用法优先通过程序内嵌手册查询，无需查找仓库 `doc/`，也不依赖 Python 或网络。先执行 `chali man help` 确认可用命令，按任务读取相关条目：

| 任务 | 命令 |
| --- | --- |
| 导出书稿与查看章节 | `chali man book`、`chali man book 6` |
| 关键字语法和用法 | `chali man keyword break`、`chali man keyword move` |
| 标准库模块 | `chali man api std::text` |
| 类型、方法及参数 | `chali man api std::text::Text::split` |
| 搜索 API | `chali man api --search delimiter` |
| 机器可读结果 | `chali man keyword break --json` |
| 获取这份 AI SKILL | `chali man skill` |
| 工具链选项 | `chali --help` |

`man book` 是导出命令，会写入当前工作目录的 `book/`；它更新书稿同名文件，保留其他文件。只需查询时使用 `man book 章号`。书稿可按章号、文件名或标题查找；附录使用 `A`、`B`。手册中的示例可能是片段，不把它们未经检查当作完整程序。中英文界面使用 `--lang zh` 或 `--lang en`，书稿保留实际原文语言。若安装版本缺少对应手册，明确说明工具版本边界；只有任务本身涉及编译器实现或修改文档时，再查实际仓库的文法、源码、回归和 `AGENTS.md`。

本 Skill 的权威文件为 `skills/chali-programming/SKILL.md`，同一内容由 `man skill` 和 VS Code 插件分发；无需为了使用它克隆 ChaLi 仓库。

## 第一个能跑的程序

```chali
void main() {
    int64 total = 0;
    int64 i = 1;
    while (i <= 10) {
        total = total + i;
        i = i + 1;
    }
    assert(total == 55);
    println("total={}", total);
}
```

```text
total=55
```

程序入口只接受 `void main()`：写 `int32 main()` 会报「main 返回类型必须为 void」。结构体、枚举和接口声明在 `}` 结束，尾分号可选；普通函数写 `int32 add(int32 a, int32 b) { return a + b; }`。

## 常见决策

- 核心 `std::lang` 自动加载，可直接使用 `String`、`List<T>`、`println`、`assert`。其他模块按实际文档导入，查到真实 API 再调用；`chali man api` 没有的东西不要凭印象写。
- `int32`、`int64`、`uint64`、`float32` 等是具体类型。未标记整数字面量可按上下文定型；浮点无后缀默认 `float64`，`1.25f` 为 `float32`，`1u` 为无符号整数。`-1u` 和 `-(1u)` 是编译错误，不用它们表示回绕；按需求选择有符号值、显式转换或无符号减法。
- 普通赋值和按值传参复制值；字符串、列表和聚合也遵守值语义。要避免大对象复制就传引用（`T&` 只读、`T mut&` 可写），要转移资源就写 `move x`。
- 用 `if (condition) { value } else { value }` 取条件值，简单分支的值不带分号。多语句分支用 `yield value;`。不要生成 `?:`、`choose` 或 `chooseLazy`。
- 非空独立块可作为表达式：`int32 a = { int32 b = 3; yield b; };`，以及 `a = { int32 b = 4; yield b; };`。每条正常路径必须给出值；空 `{}` 是集合字面量。`yield value;` 穿透无值循环，交给最近有接收者的表达式；内部可写变量优先移动，外部变量复制。无接收者时报错。`break level N;` 退出第 N 层循环，`break all;` 退出当前函数所有包围循环。块本身不计循环层数。
- 用 `println("value={}", value)` 做格式输出，或直接 `println(f"value={value}")`。后者可包含表达式、嵌套插值，使用 `\{`、`\}` 表示字面花括号。不要给插值输出附加传统格式参数。
- `List<int32> values;` 已经是空列表，`values.push(element)` 追加，`len(values)` 取长度。窄类型元素需要合适的类型上下文，可先声明 `int32 element = 1;` 再 push，不假设所有容器方法都会替字面量推导类型。
- 函数类型及匿名函数返回类型前置，例如 `int64 fn(int64) callback`、`int64 fn(int64 x) { return x; }`；匿名函数只用 `fn`，捕获修饰和列表不变。不要生成 `->` 返回类型写法。
- 处理可能失败的操作时查 `Option`、`Result` 的当前接口；不要自行发明 Rust 风格的方法名或泛型默认参数。

## 值语义与 `move`

赋值是**深拷贝**：两份各持一块，谁也不影响谁。只有确实要把资源交出去时才用 `move`，它不复制、直接把源槽清空：

```chali
struct Buffer : Drop {
    String text;
    void drop() { println("释放 {}", self.text); }
}

void main() {
    Buffer first;
    first.text = "alpha";
    Buffer second = first;
    println("{}/{}", first.text, second.text);

    Buffer third = move second;
    println("third={}", third.text);
}
```

```text
alpha/alpha
third=alpha
释放 alpha
释放 alpha
```

只有两行「释放」：`second` 的值已经被 `move` 走，它的析构被跳过。几条硬规矩：

- `move` 是**一元运算符**，写 `move x`。
- 操作数是**位置表达式**：变量、`self`、成员 `move obj.field`、下标 `move list[i]`，或者指针 / 引用本身。指针与引用**自动解引用一层**，所以写 `move p` 而不是 `move *p`（后者在文法上就是语法错误）。
- `move` 只做一层：`T**` 的 `move p` 移动的是那个 `T*`。字面量、临时值、右值表达式，以及 `Any`、未定型的泛型占位都没有具体布局，一律拒绝。
- 移动之后源绑定**禁止读写、禁止重绑定**；要复用同一个名字必须二次声明建立新绑定，普通赋值不能恢复。读它会得到 `CHL1205`。

## `Option` 与 `Result`

可能没有值用 `Option<T>`，可能失败用 `Result<T, E>`。枚举值写成 `Option<T>::Value(x)` / `Option<T>::None`、`Result<T, E>::Ok(x)` / `Result<T, E>::Err(e)`，判断与取值用 `isValue()` / `isNone()` / `isOk()` / `isErr()`，取值用 `take()`（转移载荷）、`unwrap()`、`unwrapOr(兜底)`、`takeError()`：

```chali
Option<int64> look(List<int64> values, int64 index) {
    if (index < 0 || index >= len(values)) {
        return Option<int64>::None;
    }
    return Option<int64>::Value(values[index]);
}

Result<int64, String> divide(int64 a, int64 b) {
    if (b == 0) {
        return Result<int64, String>::Err("除数为零");
    }
    return Result<int64, String>::Ok(a / b);
}

void main() {
    List<int64> values;
    values.push(7);
    values.push(9);

    match (look(values, 1)) {
        Option<int64>::Value(value) => { println("找到 {}", value); }
        Option<int64>::None => { println("越界"); }
    }

    Result<int64, String> ok = divide(9, 3);
    if (ok.isOk()) { println("商 {}", ok.take()); }
    Result<int64, String> bad = divide(1, 0);
    if (bad.isErr()) { println("错误 {}", bad.takeError()); }
}
```

```text
找到 9
商 3
错误 除数为零
```

## 卫生宏

宏是**编译期 AST 替换**，用 `$` 标记，和普通标识符不共享命名空间。类型参数放尖括号（`<T:first>` 表示类型由首个实参推导），值参数放方括号（`...rest` 是变长）：

```chali
$macro vec<T:first>[first,...rest]
{
    List<T> result;
    result.push(first);
    $for element in rest
    result.push(element);
    $end_for
    yield result;
}
$end_macro

void main() {
    List<int64> values = $vec[1, 2, 3];
    assert(values.len() == 3 && values.at(2) == 3);
    println("count={}", values.len());
}
```

```text
count=3
```

- 宏体里的名称是卫生的：调用方也有 `result` 也不会撞车；实参保留调用处的绑定。
- `$SQUARE(next())` 这类写法会把实参展开两次，需要只求值一次就在宏体里先落局部变量，或改用函数。
- 补全里宏与普通标识符同口径：输入 `$` 就出候选；只有一个模块可见时是 `$宏名`，跨模块同名时显示 `$模块::宏名` 并插入限定名，唯一的宏优先用不带模块链接的短名。

## 验证交付

对新增或修改的程序执行 `chali check path/to/program.chl`，再 `chali run path/to/program.chl`，比较输出并检查断言。命令以安装工具链的实际 `--help` 为准；Windows 仓库可用 `./ChaLi.exe`。无法执行时说明未验证，不能把推测写成实测结果。

在 ChaLi 仓库内，默认编入前端与运行时反射，IR（四元式）和 VM 使用最高可用优化；只有调试或差分验证才显式关闭。编写文档时使用功能说明，发布变更放根目录 `CHANGELOG.md`，`doc/` 不新增版本发布页。

单独执行 `chali man` 与 `chali man help` 均显示用法；`chali man api` 显示 API 分类，`chali man api XXX` 查询内嵌 `doc/library/` 的 API。
