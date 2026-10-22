# 精确符号计算与高精度数学

`import std::cas;` 提供 `Expr`。表达式从字符串解析为不可变 AST，整数和有理数使用项目已有的大整数后端；CAS 不再维护另一套大整数乘除实现。十进制字面量也按精确有理数解析，例如 `0.1 + 0.2` 化简为 `3/10`。

```chali
import std::cas;
import std::math;

int64 main()
{
    Expr x = Expr::symbol("x");
    Expr expression = Expr::parse("x^3 + 2*x");
    println("{}", expression.differentiate("x").str());
    println("{}", Expr::parse("1/3") + Expr::parse("1/6"));
    println("{}", sin(x).differentiate().str());
    BigDecimal number = BigDecimal(1);
    number.setPrecision(100);
    println("{}", sin(number).str());
    println("{}", pi(100).str());
    return 0;
}
```

`+`、`-`、`*`、`/`、一元负号和 `pow` 直接组合 AST，避免先转宿主二进制浮点造成精度损失。`Expr::fromInteger` 和 `Expr::fromDecimal` 精确保留已有大数的值。`decimal(digits)` 将精确数值节点转换为指定位数的 `BigDecimal`；含自由变量的节点需要先 `substitute`。

`ast()` 返回独立持有的根，`kind()` 区分整数、有理数、符号、加法、乘法、幂、函数和常数。`name()` 返回符号或函数名称，`childCount()`、`child(index)` 和 `children()` 遍历直接子节点。下标从零开始并检查范围。父表达式释放后，保留的根和子树仍然有效。AST 已规范化，减法和除法通常表示为加法、乘法和幂，不能用于恢复源码的空白和括号。

支持 `simplify`、`expand`、`factor`、`together`、`differentiate`、`integrate`、`integrateDefinite`、`substitute`、`limit` 和 `taylor`。导数与展开阶数为 0..1000。不定积分无法求出时保留 `integrate` 函数节点，不能求值的定积分报告错误。结构相等不代表能够证明所有数学恒等式。CAS 保留其底层算法的支持范围，不能承诺任意表达式均能求出闭式结果。

`std::math` 的三角、反三角、指数、对数、平方根和舍入函数使用泛型入口。普通整数计算结果为 `float64`，`float32` 保留单精度返回类型，`float64` 使用宿主数学库；大整数数学计算转为 `BigDecimal`，大十进制沿用其有效位数，符号表达式生成相应 AST。角度以弧度计。

高精度数学由共享十进制整数后端实现：圆周率使用 AGM，指数采用级数与缩放平方，对数采用平方根约化与级数，三角函数使用高精度周期约化、半角约化和级数。计算增加保护位后按请求有效位数舍入，不经过 `double`。`pi(digits)` 支持 1..100000 位；参数约化另有中间工作精度预算，过大的输入会报告资源范围错误。`BigDecimal` 不表示 NaN 和无穷，非法实数定义域通过宿主诊断报告。普通浮点沿用宿主 NaN/无穷行为。`round` 中点远离零，`trunc` 向零，`fmod` 余数符号沿用被除数。

CAS 的节点引用计数、符号表和内存池由进程级锁串行保护。表达式可以复制给线程，但单次 CAS 调用不能并行执行。持有的精确数值缓冲由进程级分配器管理，不借用创建它的 VM 内存池。裁剪 `NO_CHALI_STD_CAS` 可移除 CAS 实现；CAS 需要标准库和大整数后端，小数转换另需大十进制后端。

参考：[CAS API](library/std-cas.md)、[标准库索引](library/README.md)、[完整示例](../example/samples/cas.chl)、[高精度数学示例](../example/samples/math_generic.chl)。高精度独立差分测试为 `tests/bigfloat_math.py`，使用 mpmath 作为参考，比较多个精度、大角度、接近端点与定义域错误；CAS 底层测试由 `tests/cas_library.py` 运行。
