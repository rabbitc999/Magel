# std::signal — 复数信号变换

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::signal;
```

## 公共约定与错误边界

DFT 接受一般长度，FFT 要求长度为 2 的幂；非法长度由宿主检查。正变换采用负指数，逆变换归一化。输入输出为 Complex<float64> 列表，结果拥有独立存储。DFT 为 O(n²)，基 2 FFT 为 O(n log n)。 FFT/IFFT 的长度必须为正的 2 的幂。

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

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [signalDft](#api-0) | `std::lang::List<Complex<float64>> signalDft(std::lang::List<Complex<float64>> input)` |
| [signalIdft](#api-1) | `std::lang::List<Complex<float64>> signalIdft(std::lang::List<Complex<float64>> input)` |
| [signalFft](#api-2) | `std::lang::List<Complex<float64>> signalFft(std::lang::List<Complex<float64>> input)` |
| [signalIfft](#api-3) | `std::lang::List<Complex<float64>> signalIfft(std::lang::List<Complex<float64>> input)` |
| [dft](#api-4) | `std::lang::List<Complex<float64>> dft(std::lang::List<Complex<float64>> input)` |
| [idft](#api-5) | `std::lang::List<Complex<float64>> idft(std::lang::List<Complex<float64>> input)` |
| [fft](#api-6) | `std::lang::List<Complex<float64>> fft(std::lang::List<Complex<float64>> input)` |
| [ifft](#api-7) | `std::lang::List<Complex<float64>> ifft(std::lang::List<Complex<float64>> input)` |

<a id="api-0"></a>

### signalDft（1）

**声明**

```magel
std::lang::List<Complex<float64>> signalDft(std::lang::List<Complex<float64>> input);
```

**行为**

宿主入口：计算离散傅里叶正变换。

DFT/FFT 下沉到宿主 C++（glue/std_signal.hpp）。DFT 接受任意长度，基 2 FFT 只接受 2 的幂； 逆变换按 N 归一化；不修改输入，返回新列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `input` | `std::lang::List<Complex<float64>> input` | 输入矩阵、信号或数据值 | 无 |

**返回值**

`std::lang::List<Complex<float64>>`。宿主入口：计算离散傅里叶正变换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::signal::signalDft(input);
```

**源码**：[source/std/signal.mgl:8](../../source/std/signal.mgl#L8)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-1"></a>

### signalIdft（2）

**声明**

```magel
std::lang::List<Complex<float64>> signalIdft(std::lang::List<Complex<float64>> input);
```

**行为**

宿主入口：计算归一化离散傅里叶逆变换。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `input` | `std::lang::List<Complex<float64>> input` | 输入矩阵、信号或数据值 | 无 |

**返回值**

`std::lang::List<Complex<float64>>`。宿主入口：计算归一化离散傅里叶逆变换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::signal::signalIdft(input);
```

**源码**：[source/std/signal.mgl:9](../../source/std/signal.mgl#L9)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-2"></a>

### signalFft（3）

**声明**

```magel
std::lang::List<Complex<float64>> signalFft(std::lang::List<Complex<float64>> input);
```

**行为**

宿主入口：计算基 2 快速傅里叶正变换。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `input` | `std::lang::List<Complex<float64>> input` | 输入矩阵、信号或数据值 | 无 |

**返回值**

`std::lang::List<Complex<float64>>`。宿主入口：计算基 2 快速傅里叶正变换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::signal::signalFft(input);
```

**源码**：[source/std/signal.mgl:10](../../source/std/signal.mgl#L10)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-3"></a>

### signalIfft（4）

**声明**

```magel
std::lang::List<Complex<float64>> signalIfft(std::lang::List<Complex<float64>> input);
```

**行为**

宿主入口：计算归一化基 2 快速傅里叶逆变换。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `input` | `std::lang::List<Complex<float64>> input` | 输入矩阵、信号或数据值 | 无 |

**返回值**

`std::lang::List<Complex<float64>>`。宿主入口：计算归一化基 2 快速傅里叶逆变换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::signal::signalIfft(input);
```

**源码**：[source/std/signal.mgl:11](../../source/std/signal.mgl#L11)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-4"></a>

### dft（5）

**声明**

```magel
std::lang::List<Complex<float64>> dft(std::lang::List<Complex<float64>> input);
```

**行为**

计算离散傅里叶正变换。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `input` | `std::lang::List<Complex<float64>> input` | 输入矩阵、信号或数据值 | 无 |

**返回值**

`std::lang::List<Complex<float64>>`。计算离散傅里叶正变换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::signal::dft(input);
```

**源码**：[source/std/signal.mgl:13](../../source/std/signal.mgl#L13)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-5"></a>

### idft（6）

**声明**

```magel
std::lang::List<Complex<float64>> idft(std::lang::List<Complex<float64>> input);
```

**行为**

计算归一化离散傅里叶逆变换。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `input` | `std::lang::List<Complex<float64>> input` | 输入矩阵、信号或数据值 | 无 |

**返回值**

`std::lang::List<Complex<float64>>`。计算归一化离散傅里叶逆变换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::signal::idft(input);
```

**源码**：[source/std/signal.mgl:14](../../source/std/signal.mgl#L14)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-6"></a>

### fft（7）

**声明**

```magel
std::lang::List<Complex<float64>> fft(std::lang::List<Complex<float64>> input);
```

**行为**

计算基 2 快速傅里叶正变换。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `input` | `std::lang::List<Complex<float64>> input` | 输入矩阵、信号或数据值 | 无 |

**返回值**

`std::lang::List<Complex<float64>>`。计算基 2 快速傅里叶正变换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::signal::fft(input);
```

**源码**：[source/std/signal.mgl:15](../../source/std/signal.mgl#L15)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。

<a id="api-7"></a>

### ifft（8）

**声明**

```magel
std::lang::List<Complex<float64>> ifft(std::lang::List<Complex<float64>> input);
```

**行为**

计算归一化基 2 快速傅里叶逆变换。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `input` | `std::lang::List<Complex<float64>> input` | 输入矩阵、信号或数据值 | 无 |

**返回值**

`std::lang::List<Complex<float64>>`。计算归一化基 2 快速傅里叶逆变换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::signal::ifft(input);
```

**源码**：[source/std/signal.mgl:16](../../source/std/signal.mgl#L16)。

**完整示例**：[example/samples/std_complex_fft.mgl](../../example/samples/std_complex_fft.mgl)。
