# std::complex — 泛型复数

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::complex;
```

## 公共约定与错误边界

Complex<T> 保存 real/imag；T 应支持所用的四则运算。leftAdd/leftSub/leftMul/leftDiv 是运算符接口，支持 +、-、*、/。整数 T 的除法仍采用整数运算规则。

## 参考代码

完整程序：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

```powershell
magel run example/samples/std_complex_fft.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::complex;
import std::signal;

import std::math;

/** @brief 浮点比较结果通过函数指针仍须保持布尔类型。 */
bool lessFloat(float64 left, float64 right) { return left < right; }


int64 main() {
    bool fn(float64, float64) compare = lessFloat;
    assert(compare(1.25, 1.75) && !compare(2.0, 1.0));
    std::complex::Complex<float64> a=std::complex::Complex<float64>::of(3.0,4.0);
    std::complex::Complex<float64> b=std::complex::Complex<float64>::of(1.0,-2.0);
    std::complex::Complex<float64> sum=a+b, product=a*b, quotient=a/b;
    std::complex::Complex<float64> difference=a-b, conjugate=a.conjugate();
    assert(difference.re==2.0 && difference.im==6.0);
    assert(conjugate.re==3.0 && conjugate.im==-4.0 && a.normSquared()==25.0);
    std::complex::Complex<float32> small=std::complex::Complex<float32>::of((float32)1.0,(float32)2.0);
    std::complex::Complex<float32> doubled=small+small;
    assert(doubled.re==(float32)2.0 && doubled.im==(float32)4.0);
    println("complex {} {} {} {} {} {}",sum.re,sum.im,product.re,product.im,
    quotient.re,quotient.im);
    List<std::complex::Complex<float64>> input;
    input.push(std::complex::Complex<float64>::of(1.0,0.0));
    input.push(std::complex::Complex<float64>::of(2.0,0.0));
    input.push(std::complex::Complex<float64>::of(3.0,0.0));
    input.push(std::complex::Complex<float64>::of(4.0,0.0));
    List<std::complex::Complex<float64>> spectrum=std::signal::fft(input);
    List<std::complex::Complex<float64>> restored=std::signal::ifft(spectrum);
    List<std::complex::Complex<float64>> slow=std::signal::dft(input);
    int64 i=0;
    while (i<4) {
        std::complex::Complex<float64> fastValue=spectrum.at(i), slowValue=slow.at(i);
        std::complex::Complex<float64> back=restored.at(i), original=input.at(i);
        assert(std::math::abs(fastValue.re-slowValue.re)<0.00000001 &&
        std::math::abs(fastValue.im-slowValue.im)<0.00000001);
        assert(std::math::abs(back.re-original.re)<0.00000001 &&
        std::math::abs(back.im-original.im)<0.00000001);
        i=i+1;
    }
    List<std::complex::Complex<float64>> oddLength;
    oddLength.push(a); oddLength.push(b); oddLength.push(sum);
    List<std::complex::Complex<float64>> oddSpectrum=std::signal::dft(oddLength);
    List<std::complex::Complex<float64>> oddBack=std::signal::idft(oddSpectrum);
    assert(std::math::abs(oddBack.at(0).re-a.re)<0.00000001 &&
    std::math::abs(oddBack.at(2).im-sum.im)<0.00000001);
    println("fft {} {} {} {}",spectrum.at(0).re,spectrum.at(1).re,
    spectrum.at(2).re,spectrum.at(3).re);
    println("inverse {} {} {} {}",restored.at(0).re,restored.at(1).re,
    restored.at(2).re,restored.at(3).re);
    println("dft {}",slow.at(0).re);
    delete oddLength;
    delete oddSpectrum;
    delete oddBack;
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
complex 4 2 11 -2 -1 2
fft 10 -2 -2 -2
inverse 1 2 3 4
dft 10
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-34"></a>`struct Complex<T>: Add<Complex<T>, Complex<T>>, Subtract<Complex<T>, Complex<T>>, Multiply<Complex<T>, Complex<T>>, Divide<Complex<T>, Complex<T>>` | 复数的 T 必须支持普通数值加减乘除；FFT 使用 Complex<float64>。 |
| Complex | <a id="symbol-35"></a>`T re` | 复数的实部。 |
| Complex | <a id="symbol-36"></a>`T im` | 复数的虚部。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Complex::of](#api-0) | `static Complex<T> of(T real, T imaginary)` |
| [Complex::conjugate](#api-1) | `Complex<T> conjugate()` |
| [Complex::normSquared](#api-2) | `T normSquared()` |
| [Complex::scale](#api-3) | `Complex<T> scale(T factor)` |
| [Complex::leftAdd](#api-4) | `Complex<T> leftAdd(Complex<T> right)` |
| [Complex::leftSub](#api-5) | `Complex<T> leftSub(Complex<T> right)` |
| [Complex::leftMul](#api-6) | `Complex<T> leftMul(Complex<T> right)` |
| [Complex::leftDiv](#api-7) | `Complex<T> leftDiv(Complex<T> right)` |

<a id="api-0"></a>

### Complex::of（1）

**声明**

```magel
static Complex<T> of(T real, T imaginary);
```

**行为**

按 real、imag 创建复数值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `real` | `T real` | 复数实部 | 无 |
| `imaginary` | `T imaginary` | 复数虚部 | 无 |

**返回值**

`Complex<T>`。按 real、imag 创建复数值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::complex::Complex<T>::of(real, imaginary);
```

**源码**：[source/std/complex.mgl:12](../../source/std/complex.mgl#L12)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-1"></a>

### Complex::conjugate（2）

**声明**

```magel
Complex<T> conjugate();
```

**行为**

返回虚部取反的共轭复数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Complex<T>`。返回虚部取反的共轭复数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.conjugate();
```

**源码**：[source/std/complex.mgl:17](../../source/std/complex.mgl#L17)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-2"></a>

### Complex::normSquared（3）

**声明**

```magel
T normSquared();
```

**行为**

返回实部平方加虚部平方。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。返回实部平方加虚部平方。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.normSquared();
```

**源码**：[source/std/complex.mgl:18](../../source/std/complex.mgl#L18)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-3"></a>

### Complex::scale（4）

**声明**

```magel
Complex<T> scale(T factor);
```

**行为**

返回各分量乘以 factor 的新值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `factor` | `T factor` | 缩放因子 | 无 |

**返回值**

`Complex<T>`。返回各分量乘以 factor 的新值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.scale(factor);
```

**源码**：[source/std/complex.mgl:19](../../source/std/complex.mgl#L19)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-4"></a>

### Complex::leftAdd（5）

**声明**

```magel
Complex<T> leftAdd(Complex<T> right);
```

**行为**

实现左操作数加法接口。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `right` | `Complex<T> right` | 右操作数或区间右端 | 无 |

**返回值**

`Complex<T>`。实现左操作数加法接口。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.leftAdd(right);
```

**源码**：[source/std/complex.mgl:20](../../source/std/complex.mgl#L20)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-5"></a>

### Complex::leftSub（6）

**声明**

```magel
Complex<T> leftSub(Complex<T> right);
```

**行为**

实现左操作数减法接口。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `right` | `Complex<T> right` | 右操作数或区间右端 | 无 |

**返回值**

`Complex<T>`。实现左操作数减法接口。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.leftSub(right);
```

**源码**：[source/std/complex.mgl:23](../../source/std/complex.mgl#L23)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-6"></a>

### Complex::leftMul（7）

**声明**

```magel
Complex<T> leftMul(Complex<T> right);
```

**行为**

实现左操作数乘法接口。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `right` | `Complex<T> right` | 右操作数或区间右端 | 无 |

**返回值**

`Complex<T>`。实现左操作数乘法接口。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.leftMul(right);
```

**源码**：[source/std/complex.mgl:26](../../source/std/complex.mgl#L26)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-7"></a>

### Complex::leftDiv（8）

**声明**

```magel
Complex<T> leftDiv(Complex<T> right);
```

**行为**

实现左操作数除法接口。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `right` | `Complex<T> right` | 右操作数或区间右端 | 无 |

**返回值**

`Complex<T>`。实现左操作数除法接口。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`denominator != (T)0, "Complex 除数不能为零"`。

**调用形式**

```magel
obj.leftDiv(right);
```

**源码**：[source/std/complex.mgl:30](../../source/std/complex.mgl#L30)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。
