# 自定义错误传播与 if 表达式

## 条件表达式

`if` 可用于初始化、赋值、函数实参、返回值及嵌套表达式。简单分支只包含一个无分号表达式；复杂分支通过 `yield 值;` 给出结果。

```magel
int32 p = 1;
bool odd = if (p % 2 == 1) { true } else { false };
int32 value;
value = if (p % 3 == 1) { 1 } elif (p % 3 == 2) { 2 } else {
    int32 fallback = 3;
    yield fallback;
};
```

条件从上到下判断，命中后不再计算后续条件或其他分支。作为值使用时必须有 `else`，每条正常结束路径必须给出结果。`{ value; }` 是丢弃值的语句，不能替代 `{ value }`。复杂分支不接受隐式末尾表达式。

目标类型从初始化、赋值、实参或返回位置传递到分支；无上下文的整数字面量仍默认 `int64`。分支类型不兼容时报错，不自动装入 Any。

`yield 值;` 结束最近接收结果的块、if、match 或 loop 表达式，可以穿透中间的无值循环。`yield (a, b);` 以一个元组作为结果。用于初始化、赋值、实参或返回值的内层表达式仍接收自己的带值退出；`break;` 和 `continue;` 保持循环控制含义。`return` 退出当前函数。分支结果在清理局部资源前保存，禁止借用已结束的分支局部变量。

## 用户自定义错误传播

核心协议均位于自动导入的 `std::lang`：

```magel
enum TryBranch<T,R> { Continue(T value), Break(R residual); };
interface Try<T,R> { TryBranch<T,R> branch(); };
interface TryMove<T,R> { TryBranch<T,R> intoBranch(); };
interface FromResidual<F,R> { static F fromResidual(R residual); };
```

来源显式实现唯一 `Try<T,R>`，成功分支给出 T；失败分支携带 R。当前函数返回类型 F 必须实现 `FromResidual<F,R>`。协议精确匹配，不搜索隐式转换链。静态接口方法属于编译期契约，不通过接口对象动态分派。

普通变量 `value?` 使用只读 `branch()`；显式 `move(&value)?` 使用 `intoBranch()`；拥有型临时值优先选择移动协议，没有移动协议时可使用读取协议。来源只求值一次，失败时执行正常作用域清理后退出当前函数或 lambda。

标准 `Result<T,E>` 使用 `ErrorResidual<E>`，`Option<T>` 使用 `NoneResidual`，因此不同成功类型可共享失败出口，缺失与错误不会自动混合。

用户类型可返回 `TryBranch<T,ErrorResidual<AppError>>`，直接向标准 `Result<U,AppError>` 传播；自定义返回类型可实现 `FromResidual<F,ErrorResidual<AppError>>` 接收标准 Result。完整程序见 [自定义传播样例](../example/samples/try_custom.mgl)。

## 错误适配

- `result.mapErr<F>(mapper)`：仅在 Err 时调用映射函数，得到 `Result<T,F>`。
- `option.okOr<E>(error)`：缺失时生成 Err，error 按普通实参规则求值。
- `option.okOrElse<E>(factory)`：只在缺失时调用错误工厂。
- `result.ok()`：明确丢弃错误信息，得到 Option。

```magel
auto value = operation().mapErr<AppError>(toAppError)?;
auto user = findUser(id).okOr<AppError>(AppError::UserMissing)?;
```

`?` 可以写在 if 分支及条件中，其失败出口仍然是当前函数，不是 if 表达式。完整回归见 [条件表达式](../example/samples/if_expression.mgl) 与 [错误适配](../example/samples/error_adapters.mgl) 两个样例（由 `sh check.sh` 跑 VM 与 AOT 双后端）。

## 迁移与工具

移除标准库 `choose` 和 `chooseLazy`，改用 `if (condition) { a } else { b }`。新写法只计算被选中的分支；迁移旧 choose 时，需要确认原来依赖两侧提前执行的副作用。用户自己声明的同名函数仍是普通函数。

LSP 显示 if 和传播表达式的成功类型，并支持 auto 悬停与成员补全。悬停 `?` 可查看来源、成功类型、失败载荷和当前函数返回出口。

交互终端的程序文本实时输出，`std::lang::pause()` 在提示前刷新 stdout 与 stderr；非交互输入不等待回车。管道输出仍按宿主既有收集方式返回。
