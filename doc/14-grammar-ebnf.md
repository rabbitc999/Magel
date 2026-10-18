# Magel EBNF 语法参考

本文说明 Magel 的源码语法，包含词法、声明、类型、表达式、语句、lambda、注解、宿主接口及泛型入口。权威定义为 [magel.gram](../source/magel.gram)，具体类型和生命周期约束由编译器语义层、标准库与测试共同确定。本文使用 **EBNF**（扩展巴科斯范式）记法；不将历史语法作为兼容写法。

## 1 记号与阅读方法

| 记号 | 含义 |
| --- | --- |
| `name = ... ;` | 定义一个非终结符 |
| `"text"` | 字面终结符，如 `"fn"`、`"->"` |
| `a, b` | 按顺序连接 |
| `a \| b` | 选择一个分支 |
| `[ a ]` | 可选，零次或一次 |
| `{ a }` | 重复，零次或多次 |
| `( a )` | 文法分组，区别于带引号的源码圆括号 |
| `""` | 空产生式，不消费源码字符 |
| `? 说明 ?` | 特殊序列，由词法或语义层定义 |
| `VARIABLE` / `NUMBER` | 标识符 / 默认十进制整数字面量 Token |

附录保留生成器中的非终结符名称和递归形式，属于 EBNF 的基本子集，方便逐条与 `.gram` 核对；它不是可以直接交给 ParseEngine 的新 `.gram` 文件。附录的 `program` 是解析入口，输入还必须消费至文件末尾。

## 2 词法与处理阶段

关键字和标点按附录逐个登记，采取生成词法器的匹配规则；字符串、浮点、十六进制、二进制与字符字面量使用明确正则。标识符、十进制数字、空白以及行注释和块注释还使用生成词法器的默认处理。注释及字符串内的文字不会作为关键字解析。字符字面量的字节长度、数值范围与进制合法性仍须经过语义检查；正则识别成功不等于常量有效。

`std::lang::String` 是内建字符串类型，核心限定可省略为 `String`。它通过名称解析取得身份，不是独立的字符串类型关键字。`List` / `Set` / `Map` 有专用 Token；`Option` / `Result` 是标准库泛型枚举。预处理器的 `%INCLUDE` / `%IMPORT` / 条件与宏展开在语言解析前执行，不属于下面 `program` 的产生式。

源码首先经过预处理、模块装载和泛型模板收集与具体实例展开，再由生成解析器处理。泛型名称扫描与实例化不构成运行时类型声明。

## 3 类型与函数值速查

```ebnf
function_type = "fn", "(", [ function_type_parameters ], ")", "->", function_return ;
function_type_parameters = function_type_parameter, { ",", function_type_parameter } ;
function_type_parameter = type | ptr_type | array_type
                        | type, "..." | ptr_type, "..." ;
function_return = type | ptr_type ;
```

这是附录对应产生式的便于阅读的展开形式。函数类型不包含参数名，变量名位于完整类型之后：

```c
int64 fn(int64) callback = add;
int64 apply(int64 fn(int64) callback, int64 value) {
    return callback(value);
}
int64 const& fn(int64 const&) reader = borrow;
List<int64 fn(int64)> callbacks;
```

函数类型可以嵌套，`bool fn(int64 fn(int64))` 表示接收一个函数值并返回 bool。箭头右侧的引用限定符优先属于返回值：`int64 const& fn()` 返回只读引用。变长参数须放在末尾。`fn` 是关键字；旧函数指针声明和函数指针别名的关键字及产生式已删除，不提供函数类型别名。

引用函数值本身使用完整类型括号：`(int64 fn(int64)) const& ref = &callback;`。`(int64 fn(int64)) mut&` 可以替换被引用的函数值，替换沿用所有权与 Drop 清理规则；`int64 fn(int64) values[] = new[2];` 表示运行时函数值数组。与箭头右侧的引用返回值写法区分后，函数值可以参与枚举载荷、结构体字段、泛型、数组及容器组合。

数据引用写 `T &` / `T const&` / `T mut&`，前两种等价；引用非空且必须初始化，不能形成引用的引用，也不能把整数转换为引用。运行时数组写 `T[] value` 或声明名后的空方括号；定长数组的长度位于声明名之后。完整类型组合见附录 `type`、`ptr_type`、`array_type`、`element_type` 与 `declarator`。

## 4 泛型源码入口

泛型参数放在被声明的名称之后；下面是进入模板收集阶段的源码形式，模板展开后的声明再遵循附录产生式。

```ebnf
generic_parameters = "<", generic_parameter, { ",", generic_parameter }, ">" ;
generic_parameter = type_parameter | const_parameter ;
type_parameter = VARIABLE, [ ":", generic_constraint ], [ "=", concrete_type ] ;
const_parameter = VARIABLE, ":", "int64", [ "=", constant_expression ] ;
generic_constraint = ? 已声明接口约束，按现有模板约束检查规则解析 ? ;
generic_arguments = "<", generic_argument, { ",", generic_argument }, ">" ;
generic_argument = concrete_type | constant_expression ;
constant_expression = ? 可在编译期求值的 int64 表达式，不接受运行时变量 ? ;
concrete_type = ? 已解析并可单态化的具体类型，包含匿名函数类型 ? ;
generic_function_head = ( type | ptr_type ), VARIABLE, generic_parameters,
                        "(", param_list, ")" ;
generic_struct_head = "struct", VARIABLE, generic_parameters ;
generic_enum_head = "enum", VARIABLE, generic_parameters ;
generic_interface_head = "interface", VARIABLE, generic_parameters ;
generic_host_type_head = "type", VARIABLE, generic_parameters ;
```

以上 `*_head` 只描述声明头，后续继承列表、函数体或成员块分别沿用完整文法。实例化支持显式实参和可确定的函数参数推导。类型参数与整数值共同决定实例，整数表达式先求值；`N: int64` 可用于结构体数组字段的尺寸。受接口约束的类型参数可用 `C.Item` 表示关联类型，模板展开时将其绑定到具体实现。完整示例见 [匹配、插值、泛型与作用域清理](27-match-interpolation-scope.md)。未决定的泛型类型不具备运行时反射身份；`->` 中的 `>` 不结束泛型实参列表。例如 `Box<int64 fn(int64)>` 中只有末尾 `>` 结束 Box 的实参。

```c
T apply<T>(T fn(T) callback, T value) { return callback(value); }
struct Box<T> { T value; };
Box<int64 fn(int64)> box;
```

## 5 文法之外的检查

附录准确列出解析器的语法接受集合，部分分支为共享类型与表达式前缀，或为报告语义错误而存在；解析成功后还必须通过以下检查：

- 接口只包含方法；文法中能读取的接口字段会被语义层拒绝。接口实现及默认方法分派须满足编译期契约。
- `static T create(...)` 这类返回所属类型的具名静态方法是拥有值构造入口，提供 self；丢弃构造结果会报错。普通默认声明不强制调用构造函数。
- `auto`、fn 参数、枚举简写与空集合须有足够上下文。元组的共享类型项不能在值上下文冒充普通表达式。
- `=` 可以形成纯普通赋值链；复合赋值不能组成或混入赋值链。完整表达式按实际目标检查自增减的读写冲突，独立目标可组合；赋值目标只求值一次。
- 运算符重载通过接口实现。附录保留的 `operator_def` 名称是内部产生式名，其具体分支以 `.gram` 为准；它不代表允许动态声明运算协议。
- 引用可写性、逃逸、临时值存活、资源复制与 Drop 由类型与生命周期检查控制，不能由 EBNF 单独保证。
- 注解声明只在顶层；使用附着于允许的声明。参数、常量、顺序和名称冲突另外校验。根目录工具链默认同时编入前端与 VM 反射；单次编译用 `--no-reflection` 显式关闭，独立宿主按其构建配置。
- ABI 可空单对象指针通过 `Option<引用>` 映射；函数值的参数类型、返回类型和调用约定仍须符合宿主胶水支持范围。

运算优先级由附录从 `expr` 到 `primary` 的分层产生式表达；特殊前缀消歧另遵循 `.gram` 的优先级指令，包括函数返回引用限定符与共享前缀的消歧规则。

## 6 与权威文法同步的完整附录

运行 `python tools/export_magel_ebnf.py` 更新本节，运行 `python tools/export_magel_ebnf.py --check` 检查逐条同步。工具只导出文档，不参与语言解析，也不创建另一套解析实现。

<!-- BEGIN GENERATED MAGEL EBNF -->

以下 124 条产生式由 `source/magel.gram` 导出，语义动作及解析优先级指令省略。

### 终结符登记

| Token | 源码词素 |
| --- | --- |
| `ANNOTATION` | `@annotation` |
| `AT` | `@` |
| `STRUCT` | `struct` |
| `ENUM` | `enum` |
| `INT8` | `int8` |
| `INT16` | `int16` |
| `INT32` | `int32` |
| `INT64` | `int64` |
| `VOID` | `void` |
| `BOOL` | `bool` |
| `CHAR_T` | `char` |
| `TRUE` | `true` |
| `FALSE` | `false` |
| `CONST` | `const` |
| `MUT` | `mut` |
| `NEW` | `new` |
| `HEAP` | `heap` |
| `DELETE` | `delete` |
| `DROP` | `drop` |
| `STATIC` | `static` |
| `EXPORT` | `export` |
| `FN` | `fn` |
| `AUTO` | `auto` |
| `COPY` | `copy` |
| `MOVE` | `move` |
| `FSTRING` | `__magel_fstring` |
| `TYPECHECK` | `__magel_typecheck` |
| `INLINE` | `inline` |
| `IMPL` | `impl` |
| `TYPE_KW` | `type` |
| `ABI_KW` | `abi` |
| `MODULE` | `module` |
| `IMPORT` | `import` |
| `CAST_KW` | `cast` |
| `UINT8` | `uint8` |
| `UINT16` | `uint16` |
| `UINT32` | `uint32` |
| `UINT64` | `uint64` |
| `FLOAT32` | `float32` |
| `FLOAT64` | `float64` |
| `IF` | `if` |
| `ELIF` | `elif` |
| `ELSE` | `else` |
| `WHILE` | `while` |
| `FOR` | `for` |
| `IN` | `in` |
| `LOOP` | `loop` |
| `RETURN` | `return` |
| `MATCH` | `match` |
| `DO` | `do` |
| `BREAK` | `break` |
| `DERIVE` | `#derive` |
| `YIELD` | `yield` |
| `CONTINUE` | `continue` |
| `DEFER` | `defer` |
| `SCOPE_EXIT` | `scope` |
| `WILDCARD` | `_` |
| `INTERFACE` | `interface` |
| `TYPE_TAG` | `__typeTag` |
| `INCREMENT` | `++` |
| `DECREMENT` | `--` |
| `PLUS` | `+` |
| `MINUS` | `-` |
| `STAR` | `*` |
| `SLASH` | `/` |
| `PERCENT` | `%` |
| `ASSIGN` | `=` |
| `ADD_ASSIGN` | `+=` |
| `SUB_ASSIGN` | `-=` |
| `MUL_ASSIGN` | `*=` |
| `DIV_ASSIGN` | `/=` |
| `MOD_ASSIGN` | `%=` |
| `AND_ASSIGN` | `&=` |
| `OR_ASSIGN` | `&#124;=` |
| `XOR_ASSIGN` | `^=` |
| `SHL_ASSIGN` | `<<=` |
| `SHR_ASSIGN` | `>>=` |
| `EQ` | `==` |
| `NE` | `!=` |
| `LE` | `<=` |
| `GE` | `>=` |
| `LT` | `<` |
| `GT` | `>` |
| `AND` | `&&` |
| `OR` | `&#124;&#124;` |
| `NOT` | `!` |
| `ARROW` | `->` |
| `ELLIPSIS` | `...` |
| `DOT` | `.` |
| `LPAREN` | `(` |
| `RPAREN` | `)` |
| `LBRACE` | `{` |
| `RBRACE` | `}` |
| `LBRACKET` | `[` |
| `RBRACKET` | `]` |
| `SEMI` | `;` |
| `COMMA` | `,` |
| `COLON` | `:` |
| `SCOPE` | `::` |
| `AMP` | `&` |
| `PIPE` | `&#124;` |
| `CARET` | `^` |
| `TILDE` | `~` |
| `SHL` | `<<` |
| `SHR` | `>>` |
| `FATARROW` | `=>` |
| `QUESTION` | `?` |
| `TYPE_ARGS_BEGIN` | `@<` |
| `TYPE_ARGS_END` | `@>` |

### 显式正则词法规则

```ebnf
FLIT = ? 正则 "[0-9]+(\\.[0-9]+([eE][-+]?[0-9]+)?|[eE][-+]?[0-9]+)[f]?|[0-9]+f" ? ;
ULIT = ? 正则 "[0-9]+u" ? ;
HEXLIT = ? 正则 "0[xX][A-Za-z0-9_]*" ? ;
BINLIT = ? 正则 "[bB]_?[0-9][A-Za-z0-9_]*" ? ;
STRING = ? 正则 "\"([^\"\\\\]|\\\\.)*\"" ? ;
CHRLIT = ? 正则 "'([^'\\\\]|\\\\.)+'" ? ;
```

### 全部解析产生式

```ebnf
program = top_list ;

top_list = ""
    | top_list, top ;

top = annotation_decl
    | annotation_list, top_decl
    | top_decl ;

top_decl = struct_decl, type_decl_end
    | derive_directive, struct_decl, type_decl_end
    | derive_directive, enum_decl, type_decl_end
    | "module", module_name, ";"
    | "import", module_name, ";"
    | enum_decl, type_decl_end
    | interface_decl, type_decl_end
    | func_def
    | "export", func_def
    | "inline", func_def
    | host_func_decl
    | abi_block
    | host_type_decl
    | host_impl_decl
    | operator_def
    | global_decl ;

type_decl_end = ""
    | ";" ;

derive_directive = "#derive", "(", iface_list, ")" ;

annotation_decl = "@annotation", module_name
    | "@annotation", module_name, "(", annotation_params, ")" ;

annotation_params = ""
    | annotation_param_items ;

annotation_param_items = annotation_param
    | annotation_param_items, ",", annotation_param ;

annotation_param = name_token, ":", element_type
    | name_token, ":", element_type, "=", expr ;

annotation_list = annotation_use
    | annotation_list, annotation_use ;

annotation_use = "@", module_name
    | "@", module_name, "(", annotation_args, ")" ;

annotation_args = ""
    | annotation_arg_items ;

annotation_arg_items = annotation_arg
    | annotation_arg_items, ",", annotation_arg ;

annotation_arg = expr
    | name_token, ":", expr ;

module_name = name_token
    | module_name, "::", name_token ;

interface_decl = "interface", name_token, "{", iface_member_list, "}"
    | "interface", name_token, ":", iface_list, "{", iface_member_list, "}" ;

iface_member_list = ""
    | iface_member_list, iface_member ;

iface_member = annotation_list, iface_member_decl
    | iface_member_decl ;

iface_member_decl = "static", type, name_token, "(", param_list, ")", ";"
    | "type", name_token, ";"
    | type, name_token, "(", param_list, ")", ";"
    | type, name_token, "(", param_list, ")", block
    | type, "drop", "(", param_list, ")", ";"
    | type, "drop", "(", param_list, ")", block
    | ptr_type, name_token, "(", param_list, ")", ";"
    | ptr_type, name_token, "(", param_list, ")", block
    | type, name_token, ";"
    | ptr_type, name_token, ";" ;

iface_list = name_token
    | iface_list, ",", name_token ;

type = builtin_type
    | name_token
    | qualified_ref
    | container_type
    | tuple_type
    | function_type
    | "(", function_type, ")" ;

tuple_type = "(", tuple_items, ")" ;

tuple_items = tuple_item, ",", tuple_item
    | tuple_items, ",", tuple_item ;

tuple_item = expr
    | band_expr, "&"
    | builtin_type
    | container_type
    | function_type
    | ptr_type ;

element_type = type
    | type, "&"
    | type, "const", "&"
    | type, "mut", "&" ;

container_type = name_token, "@<", element_type, "@>"
    | name_token, "@<", element_type, ",", element_type, "@>"
    | qualified_ref, "@<", element_type, "@>"
    | qualified_ref, "@<", element_type, ",", element_type, "@>" ;

builtin_type = "int8"
    | "int16"
    | "int32"
    | "int64"
    | "uint8"
    | "uint16"
    | "uint32"
    | "uint64"
    | "float64"
    | "float32"
    | "bool"
    | "char"
    | "void" ;

ptr_type = type, "&"
    | type, "const", "&"
    | type, "mut", "&" ;

array_type = type, "[", "]" ;

struct_decl = "struct", name_token, "{", member_list, "}"
    | "struct", name_token, ":", iface_list, "{", member_list, "}"
    | "struct", name_token, name_token, "{", member_list, "}"
    | "struct", name_token, name_token, ":", iface_list, "{", member_list, "}" ;

enum_decl = "enum", name_token, "{", enum_variant_list, "}"
    | "enum", name_token, "{", enum_variant_list, ";", member_list, "}"
    | "enum", name_token, ":", iface_list, "{", enum_variant_list, ";", member_list, "}" ;

enum_variant_list = enum_variant
    | enum_variant_list, ",", enum_variant ;

enum_variant = name_token
    | name_token, "(", param_list, ")" ;

member_list = ""
    | member_list, member ;

member = annotation_list, member_decl
    | member_decl ;

member_decl = type, name_token, ";"
    | type, name_token, "[", expr, "]", ";"
    | type, name_token, "[", expr, "]", "[", expr, "]", ";"
    | "type", name_token, "=", element_type, ";"
    | ptr_type, name_token, ";"
    | "static", type, name_token, ";"
    | "static", ptr_type, name_token, ";"
    | type, name_token, "(", param_list, ")", block
    | ptr_type, name_token, "(", param_list, ")", block
    | array_type, name_token, "(", param_list, ")", block
    | type, "drop", "(", param_list, ")", block
    | "static", type, name_token, "(", param_list, ")", block
    | "static", ptr_type, name_token, "(", param_list, ")", block
    | "static", array_type, name_token, "(", param_list, ")", block
    | name_token, "(", param_list, ")", block ;

OPNAME = "+"
    | "-"
    | "*"
    | "/"
    | "%"
    | "=="
    | "!="
    | "<"
    | "<="
    | ">"
    | ">="
    | "&"
    | "|"
    | "^"
    | "<<"
    | ">>" ;

operator_def = type, name_token, OPNAME, "(", param, ",", param, ")", block ;

func_def = type, name_token, "(", param_list, ")", block
    | ptr_type, name_token, "(", param_list, ")", block
    | array_type, name_token, "(", param_list, ")", block
    | "auto", name_token, "(", param_list, ")", block ;

host_type_decl = "type", name_token, "=", type, ";"
    | "type", name_token, "=", ptr_type, ";" ;

host_func_decl = type, name_token, "(", param_list, ")", ";"
    | ptr_type, name_token, "(", param_list, ")", ";"
    | type, "yield", "(", param_list, ")", ";"
    | ptr_type, "yield", "(", param_list, ")", ";" ;

abi_block = "abi", STRING, "{", abi_func_list, "}" ;

abi_func_list = ""
    | abi_func_list, abi_func ;

abi_func = type, name_token, "(", param_list, ")", "=", STRING, "[", STRING, "]", ";"
    | ptr_type, name_token, "(", param_list, ")", "=", STRING, "[", STRING, "]", ";"
    | array_type, name_token, "(", param_list, ")", "=", STRING, "[", name_token, ",", STRING, "]", ";"
    | array_type, name_token, "(", param_list, ")", "=", STRING, "[", name_token, "]", ";"
    | array_type, name_token, "(", param_list, ")", "[", name_token, "]", ";"
    | type, name_token, "(", param_list, ")", "=", STRING, ";"
    | ptr_type, name_token, "(", param_list, ")", "=", STRING, ";"
    | type, name_token, "(", param_list, ")", ";"
    | ptr_type, name_token, "(", param_list, ")", ";" ;

host_impl_decl = "impl", type, ":", name_token, "=", name_token, ";"
    | "impl", ptr_type, ":", name_token, "=", name_token, ";" ;

function_type = fn_signature ;

fn_signature = type, "fn", "(", function_params, ")"
    | ptr_type, "fn", "(", function_params, ")"
    | "(", array_type, ")", "fn", "(", function_params, ")"
    | "(", function_type, ")", "fn", "(", function_params, ")" ;

function_params = ""
    | function_params_nonempty ;

function_params_nonempty = function_param
    | function_params_nonempty, ",", function_param ;

function_param = param
    | type
    | ptr_type
    | array_type
    | type, "..."
    | ptr_type, "..." ;

param_list = ""
    | param_list_nonempty ;

param_list_nonempty = param
    | param_list_nonempty, ",", param ;

param = type, name_token, "=", expr
    | ptr_type, name_token, "=", expr
    | array_type, name_token, "=", expr
    | type, name_token, "[", "]", "=", expr
    | type, name_token
    | type, "...", name_token
    | ptr_type, name_token
    | ptr_type, "...", name_token
    | array_type, name_token
    | type, name_token, "[", "]" ;

global_decl = type, declarator_list, ";"
    | ptr_type, declarator_list, ";"
    | "const", type, declarator_list, ";"
    | "const", ptr_type, declarator_list, ";"
    | "auto", declarator_list, ";" ;

declarator_list = declarator
    | declarator_list, ",", declarator ;

declarator = name_token
    | name_token, "[", expr, "]"
    | name_token, "[", expr, "]", "[", expr, "]"
    | name_token, "=", expr
    | name_token, "[", expr, "]", "=", expr
    | name_token, "[", expr, "]", "[", expr, "]", "=", expr
    | name_token, "[", "]"
    | name_token, "[", "]", "=", expr ;

block = "{", "}"
    | "{", nonempty_stmt_list, "}" ;

nonempty_stmt_list = stmt
    | nonempty_stmt_list, stmt ;

stmt_list = ""
    | stmt_list, stmt ;

stmt = block
    | decl_stmt
    | "defer", stmt
    | "scope", "(", name_token, ")", block
    | if_stmt
    | while_stmt
    | do_while_stmt
    | for_stmt
    | for_in_stmt
    | match_stmt
    | match_stmt, ";"
    | "return", expr, ";"
    | "return", tuple_items, ";"
    | "return", ";"
    | "delete", unary, ";"
    | "break", ";"
    | "yield", expr, ";"
    | "break", name_token, ";"
    | "break", name_token, NUMBER, ";"
    | "break", name_token, name_token, ";"
    | "continue", ";"
    | expression_items, ";" ;

do_while_stmt = "do", block, "while", "(", expr, ")", ";" ;

match_stmt = "match", "(", expr, ")", "{", match_item_list, "}" ;

match_item_list = ""
    | match_item_list, match_item ;

match_item = match_patterns, "if", expr, "=>", if_block
    | "_", "if", expr, "=>", if_block
    | match_patterns, "=>", if_block
    | "_", "=>", if_block ;

match_patterns = match_pattern
    | match_patterns, "|", match_pattern ;

match_pattern = range_pattern
    | bxor_expr ;

range_pattern = "[", expr, ",", expr, ")"
    | "[", expr, ",", expr, "]" ;

decl_stmt = "auto", "(", destructure_items, ")", "=", expr, ";"
    | type, name_token, "(", param_list, ")", block
    | type, name_token, "(", param_list, ")", "mut", block
    | type, name_token, "(", param_list, ")", "copy", block
    | type, name_token, "(", param_list, ")", "move", block
    | type, name_token, "(", param_list, ")", ";"
    | ptr_type, name_token, "(", param_list, ")", block
    | ptr_type, name_token, "(", param_list, ")", ";"
    | band_expr, "&", name_token, "=", expr, ";"
    | band_expr, "&", name_token, "=", expr, ",", declarator_list, ";"
    | type, declarator_list, ";"
    | ptr_type, declarator_list, ";"
    | "const", type, declarator_list, ";"
    | "const", ptr_type, declarator_list, ";"
    | "auto", declarator_list, ";" ;

destructure_items = destructure_binding, ",", destructure_binding
    | destructure_items, ",", destructure_binding ;

destructure_binding = name_token
    | "_"
    | "(", destructure_items, ")" ;

if_block = block
    | "{", expr, "}" ;

if_stmt = "if", "(", expr, ")", if_block, elif_list, else_part ;

elif_list = ""
    | elif_list, "elif", "(", expr, ")", if_block ;

else_part = ""
    | "else", if_block ;

while_stmt = "while", "(", expr, ")", block ;

loop_expr = "loop", block ;

for_stmt = "for", "(", for_init, expr, ";", for_inc, ")", block ;

for_in_stmt = "for", "(", name_token, "in", for_in_source, ")", block
    | "for", "(", "auto", name_token, "in", for_in_source, ")", block
    | "for", "(", type, name_token, "in", for_in_source, ")", block
    | "for", "(", ptr_type, name_token, "in", for_in_source, ")", block ;

for_in_source = expr
    | range_pattern ;

heap_type = name_token
    | builtin_type ;

for_init = ";"
    | decl_stmt
    | expr_stmt_simple ;

for_inc = ""
    | expr ;

expr_stmt_simple = expr, ";" ;

expr = assign_expr ;

assign_expr = or_expr
    | unary, "=", assign_expr
    | unary, "+=", assign_expr
    | unary, "-=", assign_expr
    | unary, "*=", assign_expr
    | unary, "/=", assign_expr
    | unary, "%=", assign_expr
    | unary, "&=", assign_expr
    | unary, "|=", assign_expr
    | unary, "^=", assign_expr
    | unary, "<<=", assign_expr
    | unary, ">>=", assign_expr ;

or_expr = or_expr, "||", and_expr
    | and_expr ;

and_expr = and_expr, "&&", bor_expr
    | bor_expr ;

bor_expr = bor_expr, "|", bxor_expr
    | bxor_expr ;

bxor_expr = bxor_expr, "^", band_expr
    | band_expr ;

band_expr = band_expr, "&", eq_expr
    | eq_expr ;

eq_expr = eq_expr, "==", rel_expr
    | eq_expr, "!=", rel_expr
    | rel_expr ;

rel_expr = rel_expr, "<", shift_expr
    | rel_expr, "<=", shift_expr
    | rel_expr, ">", shift_expr
    | rel_expr, ">=", shift_expr
    | shift_expr ;

shift_expr = shift_expr, "<<", add_expr
    | shift_expr, ">>", add_expr
    | add_expr ;

add_expr = add_expr, "+", mul_expr
    | add_expr, "-", mul_expr
    | mul_expr ;

mul_expr = mul_expr, "*", mul_expr2
    | mul_expr, "/", mul_expr2
    | mul_expr, "%", mul_expr2
    | mul_expr2 ;

mul_expr2 = unary ;

unary = "++", unary
    | "--", unary
    | "!", unary
    | "-", unary
    | "+", unary
    | "~", unary
    | "*", unary
    | "&", unary
    | "new", "[", expr, "]"
    | "new", heap_type, "(", call_args, ")"
    | "heap", heap_type
    | "heap", "new", heap_type, "(", call_args, ")"
    | cast_expr ;

cast_expr = "(", builtin_type, ")", unary
    | "(", band_expr, "&", ")", unary
    | "(", ptr_type, ")", unary
    | "cast", "<", type, ">", "(", expr, ")"
    | postfix ;

postfix = postfix, "++"
    | postfix, "--"
    | postfix, "[", expr, "]"
    | postfix, "?"
    | postfix, "[", expr, ":", expr, "]"
    | postfix, "[", expr, ":", "]"
    | postfix, ".", name_token
    | postfix, ".", NUMBER
    | postfix, "->", name_token
    | postfix, ".", "drop"
    | postfix, "->", "drop"
    | postfix, "(", call_args, ")"
    | primary ;

call_args = ""
    | call_args_nonempty ;

call_args_nonempty = expr
    | call_args_nonempty, ",", expr ;

primary = name_token
    | lambda_expr
    | qualified_ref
    | NUMBER
    | ULIT
    | HEXLIT
    | BINLIT
    | FLIT
    | string_literal
    | interpolated_string
    | "__magel_typecheck", "(", element_type, ",", expr, ")"
    | CHRLIT
    | "true"
    | "false"
    | loop_expr
    | z_block_value
    | z_if_value
    | z_match_value
    | "__typeTag", "(", element_type, ")"
    | collection_literal
    | "(", tuple_items, ")"
    | "(", expr, ")" ;

z_if_value = if_stmt ;

z_match_value = match_stmt ;

z_block_value = "{", nonempty_stmt_list, "}" ;

interpolated_string = "__magel_fstring", "(", call_args, ")" ;

name_token = VARIABLE
    | "copy"
    | "move" ;

lambda_capture = ""
    | "const"
    | "mut"
    | "copy"
    | "move" ;

lambda_keyword = "fn" ;

lambda_expr = fn_signature, lambda_body
    | lambda_keyword, lambda_capture, "(", lambda_params, ")", lambda_body
    | type, lambda_keyword, lambda_capture, "(", lambda_params, ")", lambda_body
    | ptr_type, lambda_keyword, lambda_capture, "(", lambda_params, ")", lambda_body
    | lambda_keyword, lambda_capture_groups, lambda_capture, "(", lambda_params, ")", lambda_body
    | type, lambda_keyword, lambda_capture_groups, lambda_capture, "(", lambda_params, ")", lambda_body
    | ptr_type, lambda_keyword, lambda_capture_groups, lambda_capture, "(", lambda_params, ")", lambda_body ;

lambda_capture_groups = lambda_capture_group
    | lambda_capture_groups, lambda_capture_group ;

lambda_capture_group = "const", "[", lambda_capture_names, "]"
    | "mut", "[", lambda_capture_names, "]"
    | "copy", "[", lambda_capture_names, "]"
    | "move", "[", lambda_capture_names, "]" ;

lambda_capture_names = ""
    | lambda_capture_names_nonempty ;

lambda_capture_names_nonempty = name_token
    | lambda_capture_names_nonempty, ",", name_token ;

lambda_params = ""
    | lambda_params_nonempty ;

lambda_params_nonempty = lambda_param
    | lambda_params_nonempty, ",", lambda_param ;

lambda_param = param
    | name_token ;

lambda_body = block
    | "{", expr, "}" ;

string_literal = STRING
    | string_literal, STRING ;

collection_literal = "{", "}"
    | "{", expression_items, "}"
    | "{", expression_items, ",", "}"
    | "{", map_literal_items, "}"
    | "{", map_literal_items, ",", "}" ;

expression_items = expr
    | expression_items, ",", expr ;

map_literal_items = expr, ":", expr
    | map_literal_items, ",", expr, ":", expr ;

qualified_ref = name_token, "::", name_token
    | name_token, "::", "yield"
    | "yield", "::", name_token
    | "yield", "::", "yield"
    | name_token, "::", "new"
    | "yield", "::", "new"
    | qualified_ref, "::", name_token
    | qualified_ref, "::", "yield" ;
```

<!-- END GENERATED MAGEL EBNF -->

## 7 验证入口

函数类型组合及旧语法拒绝见 `tests/magel_function_types.py`；lambda、泛型与引用分别见 `tests/magel_lambdas.py`、`tests/magel_generic_inference.py`、`tests/magel_reference_values.py`。正式语言规则还需由 [样例](../example/samples)和[错误样例](../example/samples-error)核对，文档不替代运行验收。

函数类型和匿名函数的返回类型统一前置：`int64 fn(int64, int64) callback`、`int64 fn(int64 x) { x + 1 }`。旧 `fn(int64, int64) -> int64` 与 `fn(int64 x) -> int64 { x + 1 }` 已移除并报错；推导返回类型使用 `fn(int64 x) { x + 1 }`，不写箭头。升级后重新生成泛型宿主胶水并重建旧缓存。
