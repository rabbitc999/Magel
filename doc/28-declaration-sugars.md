# 返回表达式、具名闭包与字段派生

[文档索引](README.md) · [English](en/28-declaration-sugars.md)

权威产生式在 [`source/magel.gram`](../source/magel.gram)。ParseEngine 只负责由文法生成解析器；语义动作调用 Magel 自己的 AST 构造与降级函数。以下 IR 均指四元式。

## return 接控制表达式

```magel
String make() {
    return {
        String local = "owned";
        yield local;
    };
}
int32 choose(bool yes) { return if (yes) { 1 } else { 2 }; }
int64 count() { return loop { yield 3; }; }
```

`return` 仍接受普通表达式。块、`loop`、`if`、`match` 复用已有值出口；复杂分支使用 `yield value;`，单表达式分支已有的隐式值出口继续有效。返回控制表达式的所有正常完成路径必须给出值，分支最终类型必须完全一致，不能靠分支之间的数值提升或用户转换消除差异。整数字面量仍接受函数返回类型提供的上下文。

语法入口与 AST 脱糖：

```text
stmt := RETURN expr SEMI { M_ReturnValue(expr) }

Return(ControlExpr E)
  => Block(
       Declare hidden : FunctionResult = E,
       Return(hidden))
```

AST 阶段创建独立且不可被用户命名碰撞的结果绑定，声明组和控制表达式标记 `returnExpression`。`LowerLocalDecl` 把当前函数返回类型传入隐藏声明，`LoopValueType` 检查各分支的精确类型；控制表达式本身仍由 `LowerLoop` 分配单独的汇合槽。表达式只求值一次，先保存结果，再清理来源作用域，最后交给普通返回路径。

返回控制表达式内禁止嵌套 `return`、`break level N`、`break all`；会提前返回的 `?` 传播也要求改为显式匹配后 `break`。检查停在新的 `fn` 或具名函数边界，因此表达式内部声明的独立闭包可以拥有自己的 `return`。不会把内部函数的出口错误归给外层函数。


泛型函数使用同一条路径：`T take<T>(T value) { return { T local = value; yield local; }; }`。loop、if 和 match 同样可返回 T；单态化先把 T 替换为具体类型，之后按该类型建立隐藏槽、检查分支并执行移动或复制。泛型参数本身不会豁免分支类型一致、借用与清理约束。示例覆盖整数、String 及包含 String 的结构体。

## 编译期所有权与状态追踪

不增加 GC，也不把普通赋值改成移动。移动判定针对 `break` 实际交给哪个值出口，而不是仅看语法上最近的大括号。

| 信息 | 当前实现中的载体 | 用途 |
| --- | --- | --- |
| 唯一存储身份与字段投影 | `MVar.emitted`、`Place.root/path` | 区分同名重声明、字段、数组下标及引用别名 |
| 声明所属作用域 | `g_scope` 与 `MBlockContext.valueScopeDepth` | 判断变量位于当前接收结果表达式内部还是外部 |
| 拥有者与待清理项 | `g_ownedScopes` | 维护 Drop、Closeable、字段及延迟清理顺序 |
| 引用来源集合 | 借用控制流的 Bind/Allocate 与 `Fact` | 阻止结果、字段或闭包把局部引用带出作用域 |
| 移动与释放状态 | `FlowState.invalid` 的 Moved/Freed/Dropped | 检查移动后读取、写入、别名访问和重复销毁 |
| 分支与循环合流 | `checkFlow` 的不动点分析 | 任意到达路径上失效的存储都不能被后续路径无条件访问 |

`PrepareBreakValue` 对元组逐项处理。对拥有值的直接局部变量，在当前接收结果的表达式内部声明时生成编译器消费操作；外部变量保持复制，临时表达式由接收方接管。内部只读拥有绑定在退出处也可被编译器消费，这个内部标记不开放给源码；普通用户 `move` 仍要求 `T mut&`。引用不是被引用资源的拥有者，不能借此消费外部资源；所有引用结果继续经过来源寿命检查。

控制流伪代码：

```text
lowerBreak(value, receiver):
    for each leaf in tuple-or-value:
        binding = resolveDirectBinding(leaf)
        if binding owns value and declaredInside(binding, receiver):
            read binding exactly once
            transfer storage into receiver.joinSlot
            mark source Moved; clear source storage without Drop
        else:
            copy value into receiver.joinSlot
        check referenced sources outlive receiver
    cleanup scopes above receiver in reverse order
    jump receiver.exit

lowerReturnControl(E):
    hidden = evaluate E exactly once through its joinSlot
    transfer hidden to ordinary return slot
    cleanup remaining function scopes, retaining the returned owner
    return ordinary return slot
```

这里的 transfer/copy 是编译器伪操作，不新增 VM 操作码。已有 `LowerLanguageMove` 使用存活内存检查、地址读取、源槽清零及聚合移动标记；复制仍使用现有字符串、对象与结构体复制路径。Drop 保留在相应控制流上，由移动状态保护，不能在遇到一条移动分支时全局删除另一条分支的清理。清理回调若仍访问已消费的来源，必须由借用检查拒绝，不能靠悬垂别名访问搬走的资源。

核心代码见 [`M_ReturnValue`](../source/epilogue/return_expression_ast.cpp)、[`PrepareBreakValue` 与 `LowerLanguageMove`](../source/epilogue/boxing.cpp)、[`LowerLoop`](../source/epilogue/lowering.cpp) 和[借用控制流](../source/magel_borrow_flow.hpp)。

运行时数组也沿用该规则：`return { String values[] = new[2]; ... yield values; };` 的内部数组连同长度头一起转移，源句柄清零，清理时跳过源元素。外部运行时数组则重新分配长度相同的存储，字符串、对象和聚合元素逐项深复制。隐藏结果槽接受带长度的块、if、match 与 loop 结果，不再只接受 new 或函数调用。此编译器内部消费不开放为源码中的 `move(&array)`；定长数组仍按原有存储和返回类型约束处理，不透明宿主类型也不会被推断成拥有资源。

## 语句级具名函数

```magel
void main() {
    int64 value = 2;
    int64 read(int64 amount) { return value + amount; }
    int64 change(int64 amount) mut { value += amount; return value; }
    int64 snapshot() copy { return value; }
    int64 owned() move { return value; }
    int64 later(int64 amount);
    later = int64 fn(int64 amount) { return amount * 2; };
}
```

局部定义只加入 `decl_stmt`，不加入 `primary`、`unary` 或其他表达式产生式。`a + int64 f() { ... }` 因此是语法错误。

```text
decl_stmt := type name '(' param_list ')' block
           | type name '(' param_list ')' MUT block
           | type name '(' param_list ')' COPY block
           | type name '(' param_list ')' MOVE block
           | type name '(' param_list ')' ';'

NamedFunction(T, name, params, body, capture)
  => Declare name : FnType(T, params)
       = Lambda(T, params, body, capture)
```

返回类型是类型锚点，不从调用处猜测。默认捕获 const&；`mut` 显式允许修改借用来源，`copy` 建立独立状态，`move` 接管状态所有权。闭包构造结果由新绑定接管，后续普通函数值赋值仍遵守值复制规则。没有函数体的局部声明是函数值变量，可随后赋值；顶层 `T f(...);` 继续表示已有宿主函数声明。

闭包捕获与函数值生命周期复用已有 `fn` 实现。返回或保存一个仍借用局部变量的具名闭包不会延长来源寿命，必须编译报错。核心入口为 [`M_NamedFunction`](../source/epilogue/named_function_ast.cpp)；没有另建一套闭包运行时。

## 字段派生指令

```magel
#derive(Equal, Hash, Clone)
struct Point { int64 x; String label; }

#derive(Equal, Hash, Clone)
enum Choice { None, Value(Point point) }
```

```text
derive_directive := DERIVE '(' iface_list ')'
top_decl := derive_directive struct_decl type_decl_end
          | derive_directive enum_decl type_decl_end

DerivedDecl(traits, declaration)
  => declaration.derives = traits
```

`#derive` 是独立登记的编译器指令 token。指令只修饰结构体和枚举声明。白名单为 Equal、Hash、Clone；Drop、Closeable、未知或重复的请求直接报错，不生成析构协议，也不覆盖已有同名方法。

派生分为两个互相隔离的步骤：

1. 语义层从已解析声明读取字段和枚举载荷元数据，把字段名或每字段代码片段交给现有 `$macro`、`$for`、`$concat` 文本模板。宏引擎只接收文本，不解析类型、不构建 AST，也不调用 Parser。
2. 宏展开完毕后，派生语义层使用同一个生成解析器读取生成的普通方法，附加到原类型 AST，并实例化正常的 `Equal<bool,T>`、`Hash<T>`、`Clone<T>` 接口。之后统一进行接口、类型、借用和四元式降级检查。

Equal 按字段生成短路比较，枚举先匹配变体再比较对应载荷。Hash 把变体标签和字段哈希组合；字符串按 UTF-8 字节处理，浮点零做一致化，确保相等值具有相同哈希。Clone 逐字段复制内建值，调用嵌套自定义字段的 Clone 接口；枚举只克隆当前活动变体。生成的方法执行正常的比较、哈希和复制，不包含派生解释器、运行时字段枚举或反射查找。

字段协议在全部类型和接口登记之后检查，未被调用的派生类型同样检查。标量和 String 有内建语义；嵌套自定义结构体或枚举必须已经实现对应接口，或自身也请求并完成了该接口派生。引用、函数值、原始数组及其他没有相应语义协议的字段不会按地址、内存字节或自动析构规则偷偷推导，必须明确提供合适的封装和接口。

核心框架在 [`derive.cpp`](../source/epilogue/derive.cpp)：

```text
ExpandDerivedDeclarations():
    for each declared type with pending derives:
        reject traits outside Equal / Hash / Clone
        source = expand existing text macros over field metadata
        generated = generatedParser.parse(source)
        attach generated methods and concrete interfaces to the original AST

RegisterTop()
ValidateDerivedFields()  // missing nested interface is always a compilation error
LowerProgram()          // ordinary typed methods -> ordinary quadruples
```

## 回归入口

- [返回表达式与资源清理](../example/samples/return_expression.mgl)、[具名闭包](../example/samples/named_function.mgl)、[结构体和枚举派生](../example/samples/derive.mgl)。
- [语义及生命周期门禁](../tests/magel_declaration_sugars.py)检查分支类型、嵌套返回、层级跳转、只读捕获、引用逃逸、表达式位置声明、Drop 派生和缺失字段协议。
