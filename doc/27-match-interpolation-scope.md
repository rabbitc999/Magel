# 匹配、插值、泛型与作用域清理

本文说明匹配表达式、字符串插值、泛型推导与作用域清理的用法及约束。

## match 表达式

匹配可用于初始化、赋值、实参、返回值和嵌套表达式。简单分支写 `{ value }`，其中的值不加分号；多条语句的分支用 `yield value;` 给出结果。外层声明、赋值和 return 仍以分号结束。

```chali
enum State { Ready(int64 value), Missing };
State state = State::Ready(3);
int64 result = match (state) {
    State::Ready(value) => { value + 1 }
    State::Missing => { int64 fallback = 0; yield fallback; }
};
match (state) {
    State::Ready(value) => { println("{}", value); }
    State::Missing => { println("missing"); }
};
```

分支结果类型必须一致。没有 `_` 兜底时，枚举必须覆盖全部变体；带守卫的分支不能单独证明该变体已覆盖。整数、字符串等非枚举的有值匹配需要兜底分支。只执行命中的分支。

表达式匹配的值载荷绑定是独立副本，字符串和带拥有资源的结构体按值复制，修改副本不影响主对象。引用载荷仍保持引用身份，不能借此修改主对象或返回已结束分支的局部引用。语句匹配的原有绑定规则继续有效，支持结尾分号并保留已有不带分号写法。

## f-string 插值

```chali
int64 f = 3;
String text = f"value={f + 1}, nested={f"inner {f}"}, \{literal\}";
```

只有 `f` 后紧跟 `"` 才识别插值；独立的 f 是普通标识符。每个大括号内写完整表达式，实参从左到右各求值一次。词法层只拆分字符串片段与表达式边界，表达式由生成解析器读取。插值复用 `std::lang::format` 的格式化与类型规则，不增加运行时函数。

普通字符串的转义继续有效；`\{`、`\}` 写出字面大括号，普通字符串中的 `{...}` 不插值。嵌套插值先展开内层。空的 `{}`、缺失引号或未配对的大括号在编译时报告。

## 关联类型

```chali
interface Container {
    type Item;
    Item get(int64 index);
};
struct Numbers : Container {
    type Item = int64;
    Item get(int64 index) { return index; }
};
C.Item first<C: Container>(C container) { return container.get(0); }
```

接口体内的 `type Item;` 声明关联类型，实现类型在类型体内提供 `type Item = T;`。泛型参数 C 受 Container 约束后，可用 C.Item 表示该实现的具体类型。关联类型参与方法签名检查和接口继承；缺失定义、循环定义、返回类型不匹配都报告错误。模块层 type 保持已有类型别名规则。

## const 泛型与定长数组字段

```chali
struct Buffer<T, N: int64> {
    T data[N];
    int64 len() { return N; }
};
struct Matrix<T, rows: int64, cols: int64> {
    T data[rows][cols];
};
Buffer<int64, 4> values;
values.data[3] = 7;
Matrix<int64, 2, 3> matrix;
matrix.data[1][2] = 9;
```

N、rows、cols 是编译期整数，不是运行时函数实参。整数表达式先求值并规范化，同样的类型与数值复用一个实例，不同尺寸产生不同实例。支持 `N: int64 = 4` 默认值；运行时变量不能作为整数实参。

定长数组字段在结构体内部连续存放，二维数组按行存放；尺寸必须是正整数，并受当前内存预算约束。结构体复制会复制数组元素，拥有资源的元素按原有值语义复制和清理。运行时数组的已有用法不变。

## defer 与 scope(exit)

```chali
void main() {
    int64 value = 1;
    defer println("last {}", value);
    scope(exit) { println("first"); }
    value = 2;

}
```

defer 登记执行语句，scope(exit) 登记语句块；登记时不执行正文。作用域正常结束、return、panic、break、continue 和 `?` 提前返回都会执行已登记回调。同作用域按登记逆序执行，每个登记只执行一次。正文读取退出时的变量值，所借用的变量必须仍处于有效作用域。

回调先于该作用域的自动资源清理执行。defer 用于非所有权资源的收尾，Drop、Closeable 继续按已有规则自动执行。

## 卫生宏展开

宏由生成解析器建立 AST，在语义分析中展开。`$match value is expr` 仅识别完整的 `expr`、`ident`、`type`、`block`、`stmt`，`$else is` 按顺序继续，`$else` 兜底。`$regex value is "模式"` 对原文执行完整 ECMAScript 正则匹配。未选分支不进行最终语义检查；模板本身仍须语法合法。

普通局部声明自动卫生，实参保留调用者绑定，不增加自动括号或缓存。详细规则与可运行示例见 [卫生宏](24-literals-operators-macros.md#编译期卫生-ast-宏)。
