---
name: magel-programming
description: 编写、修改与调试 Magel (.mgl) 程序，依据实际文法和标准库选择语法、宏、值语义、借用与错误处理方式，并使用 Magel 工具链验证。用于 Magel 应用代码和示例，不替代编译器或 VM 内部实现的仓库规范。
---

# 编写 Magel 程序

目标是交付能被实际 Magel 编译器检查、运行的 `.mgl` 程序。Magel 使用 C 风格声明和块，但不要照搬 C++、Rust 或 JavaScript 的泛型、所有权和函数语法。

## 先查询随程序分发的手册

定位已安装的 `magel`（Windows 为 `magel.exe`）。语言用法优先通过程序内嵌手册查询，无需查找仓库 `doc/`，也不依赖 Python 或网络。先执行 `magel man help` 确认可用命令，按任务读取相关条目：

| 任务 | 命令 |
| --- | --- |
| 导出书稿与查看章节 | `magel man book`、`magel man book 6` |
| 关键字语法和用法 | `magel man keyword break`、`magel man keyword fn` |
| 标准库模块 | `magel man api std::text` |
| 类型、方法及参数 | `magel man api std::text::Text::split` |
| 搜索 API | `magel man api --search delimiter` |
| 机器可读结果 | `magel man keyword break --json` |
| 获取这份 AI SKILL | `magel man skill` |
| 工具链选项 | `magel --help` |

`man book` 是导出命令，会写入当前工作目录的 `book/`；它更新书稿同名文件，保留其他文件。只需查询时使用 `man book 章号`。书稿可按章号、文件名或标题查找；附录使用 `A`、`B`。手册中的示例可能是片段，不把它们未经检查当作完整程序。中英文界面使用 `--lang zh` 或 `--lang en`，书稿保留实际原文语言。若安装版本缺少对应手册，明确说明工具版本边界；只有任务本身涉及编译器实现或修改文档时，再查实际仓库的文法、源码、回归和 `AGENTS.md`。

本 Skill 的权威文件为 `skills/magel-programming/SKILL.md`，同一内容由 `man skill` 和 VS Code 插件分发；无需为了使用它克隆 Magel 仓库。

## 常见决策

- 程序入口支持 `int64 main()` 或 `void main()`；void 正常完成的退出码为 0，panic 仍失败。结构体、枚举和接口声明在 `}` 结束，尾分号可选。普通函数写 `int32 add(int32 a, int32 b) { return a + b; }`。
- 核心 `std::lang` 自动加载，可直接使用 `String`、`List<T>`、`println`、`assert`。其他模块按实际文档导入，查到真实 API 再调用。
- `int32`、`int64`、`uint64`、`float32` 等是具体类型。未标记整数字面量可按上下文定型；浮点无后缀默认 `float64`，`1.25f` 为 `float32`，`1u` 为无符号整数。`-1u` 和 `-(1u)` 是编译错误，不用它们表示回绕；按需求选择有符号值、显式转换或无符号减法。
- 普通赋值和按值传参复制值；字符串、列表和聚合也遵守值语义。避免不必要的大对象复制时查引用参数规则，明确需要转移资源时才使用 `move`。移动后原绑定禁止读取、写入或重新绑定；需要继续使用同名变量时，必须二次声明建立新绑定，普通赋值不能恢复。引用的可变性与生命周期受检查，不能返回局部变量的引用。
- 用 `if (condition) { value } else { value }` 取条件值，简单分支的值不带分号。多语句分支用 `yield value;`。不要生成 `?:`、`choose` 或 `chooseLazy`。
- 非空独立块可作为表达式：`int32 a = { int32 b = 3; yield b; };`，以及 `a = { int32 b = 4; yield b; };`。每条正常路径必须给出值；空 `{}` 是集合字面量。`yield value;` 穿透无值循环，交给最近有接收者的表达式；内部可写变量优先移动，外部变量复制。无接收者时报错。`break level N;` 退出第 N 层循环，`break all;` 退出当前函数所有包围循环。块本身不计循环层数。
- 用 `println("value={}", value)` 做格式输出，或直接 `println(f"value={value}")`。后者可包含表达式、嵌套插值，使用 `\{`、`\}` 表示字面花括号。不要给插值输出附加传统格式参数。
- `List<int32> values;` 已经是空列表，`values.push(element)` 追加，`len(values)` 取长度。窄类型元素需要合适的类型上下文，可先声明 `int32 element = 1;` 再 push，不假设所有容器方法都会替字面量推导类型。
- 函数类型及匿名函数返回类型前置，例如 `int64 fn(int64) callback`、`int64 fn(int64 x) { return x; }`；匿名函数只用 `fn`，旧 `lambda` 语法已移除，捕获修饰和列表不变。不要生成 `->` 返回类型写法。
- 按目标泛型与接口的实际声明填写类型参数。处理可能失败的操作时查 `Option`、`Result` 的当前接口；不要自行发明 Rust 风格的方法名或泛型默认参数。

## 可复用的列表宏

Magel 宏使用 `$macro`、`$end_macro`、`$for`、`$end_for`；尾参声明为 `...values`，调用带 `$`。使用 `$0`、`$1` 等卫生名称防止与调用点局部变量冲突。

```magel
$macro vec(type, ...values)
{
    List<type> $0;
$for item in values
    { type $1 = item; $0.push($1); }
$end_for
    yield $0;
}
$end_macro

int64 main() {
    List<int32> values = $vec(int32, 1, 2, 3);
    values = $vec(int32, 4, 5);
    assert(len(values) == 2 && values[0] == 4);
    println(f"count={len(values)}");
    return 0;
}
```

暂存元素使类型上下文明确，每个实参只求值一次，局部小块允许每轮复用卫生名称。`$vec(int32)` 生成空列表。宏仅在定义或显式导入后可用，不把示例宏当成自动加载的标准 API。

## 验证交付

对新增或修改的程序执行 `magel check path/to/program.mgl`，再 `magel run path/to/program.mgl`，比较输出并检查断言。命令以安装工具链的实际 `--help` 为准；Windows 仓库可用 `./magel.exe`。无法执行时说明未验证，不能把推测写成实测结果。

涉及所有权、分支退出、宏副作用时，覆盖空输入、多个元素、嵌套作用域与重复求值风险。AOT 默认 checked 和 O3。目标是 AOT 时再执行 `magel aot path/to/program.mgl --out <artifact>` 并运行产物；构建成功不能代替运行验证。版本支持不足时按用户目标提供兼容写法或说明最低能力需求。

在 Magel 仓库内，默认编入前端与运行时反射，IR（四元式）和 VM 使用最高可用优化；只有调试或差分验证才显式关闭。编写文档时使用功能说明，发布变更放根目录 `CHANGELOG.md`，`doc/` 不新增版本发布页。

单独执行 `magel man` 与 `magel man help` 均显示用法；`magel man api` 显示 API 分类，`magel man api XXX` 查询内嵌 `doc/library/` 的 API。
