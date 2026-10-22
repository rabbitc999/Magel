#include "std_signal.hpp"

#include "../chali_glue.hpp"
#include <cmath>
#include <complex>
#include <vector>

namespace std_signal_glue
{
using Context = chali_glue::Context;

inline constexpr double kPi = 3.14159265358979323846;

/* 读 `std::lang::List<Complex<float64>>`：每个元素按 2 个 double（re, im）展平。 */
/**
 * @brief 从复数列表的平铺字段读取双精度宿主复数数组。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param list 语言侧复数列表句柄。
 * @return 与列表存储独立的复数数组。
 */
inline std::vector<std::complex<double>> readComplexes(Context &ctx, std::int64_t list)
{
    Memory::Vector<double> flat;
    std::vector<std::complex<double>> out;
    if (!collection_glue::readListStruct(ctx, list, 2u, flat))
    {
        ctx.fail("读取 List<Complex> 失败");
        return out;
    }
    out.reserve(flat.size() / 2u);
    for (std::size_t i = 0; i + 1u < flat.size(); i += 2u)
        out.emplace_back(flat[i], flat[i + 1u]);
    return out;
}

/* 把 `std::vector<std::complex<double>>` 写成 `std::lang::List<Complex<float64>>`。 */
/**
 * @brief 将宿主复数数组复制为拥有型语言列表。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param values 需要复制到新运行时存储的宿主数值数组。
 * @return 新列表的运行时句柄。
 */
inline std::int64_t writeComplexes(Context &ctx, const std::vector<std::complex<double>> &values)
{
    const std::int64_t list = collection_glue::makeList(ctx);
    for (const std::complex<double> &value : values)
    {
        const std::int64_t address = chali_glue::makeStruct(ctx, { value.real(), value.imag() });
        collection_glue::listPushStruct(ctx, list, address, 2u);
    }
    return list;
}

/**
 * @brief 按负指数定义计算离散傅里叶变换。
 * @param in 待变换的原始复数数组。
 * @return 与输入等长的频域复数数组。
 */
inline std::vector<std::complex<double>> dft(const std::vector<std::complex<double>> &in)
{
    const int n = static_cast<int>(in.size());
    std::vector<std::complex<double>> out(static_cast<std::size_t>(n));
    for (int k = 0; k < n; ++k)
    {
        std::complex<double> sum(0.0, 0.0);
        for (int j = 0; j < n; ++j)
        {
            const double angle = -2.0 * kPi * static_cast<double>(k) * static_cast<double>(j) / static_cast<double>(n);
            const std::complex<double> w(std::cos(angle), std::sin(angle));
            sum += in[static_cast<std::size_t>(j)] * w;
        }
        out[static_cast<std::size_t>(k)] = sum;
    }
    return out;
}

/**
 * @brief 按正指数定义计算逆离散傅里叶变换并归一化。
 * @param in 待变换的原始复数数组。
 * @return 与输入等长的时域复数数组。
 */
inline std::vector<std::complex<double>> idft(const std::vector<std::complex<double>> &in)
{
    const int n = static_cast<int>(in.size());
    std::vector<std::complex<double>> out(static_cast<std::size_t>(n));
    for (int k = 0; k < n; ++k)
    {
        std::complex<double> sum(0.0, 0.0);
        for (int j = 0; j < n; ++j)
        {
            const double angle = 2.0 * kPi * static_cast<double>(k) * static_cast<double>(j) / static_cast<double>(n);
            const std::complex<double> w(std::cos(angle), std::sin(angle));
            sum += in[static_cast<std::size_t>(j)] * w;
        }
        out[static_cast<std::size_t>(k)] = sum / static_cast<double>(n);
    }
    return out;
}

/**
 * @brief 对复数数组计算快速傅里叶变换。
 * @param a 运算左侧的输入矩阵或复数数组。
 * @return 频域数组；不满足快速路径条件时使用离散变换。
 */
inline std::vector<std::complex<double>> fft(std::vector<std::complex<double>> a)
{
    const int n = static_cast<int>(a.size());
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(j)]);
    }
    for (int length = 2; length <= n; length <<= 1)
    {
        const double angle = -2.0 * kPi / static_cast<double>(length);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (int start = 0; start < n; start += length)
        {
            std::complex<double> twiddle(1.0, 0.0);
            for (int offset = 0; offset < length / 2; ++offset)
            {
                const std::complex<double> even = a[static_cast<std::size_t>(start + offset)];
                const std::complex<double> odd = a[static_cast<std::size_t>(start + offset + length / 2)] * twiddle;
                a[static_cast<std::size_t>(start + offset)] = even + odd;
                a[static_cast<std::size_t>(start + offset + length / 2)] = even - odd;
                twiddle *= step;
            }
        }
    }
    return a;
}

/**
 * @brief 对复数数组计算逆快速傅里叶变换并归一化。
 * @param a 运算左侧的输入矩阵或复数数组。
 * @return 与输入等长的时域复数数组。
 */
inline std::vector<std::complex<double>> ifft(std::vector<std::complex<double>> a)
{
    for (std::complex<double> &value : a) value = std::conj(value);
    a = fft(std::move(a));
    for (std::complex<double> &value : a) value = std::conj(value);
    const double scale = 1.0 / static_cast<double>(a.size());
    for (std::complex<double> &value : a) value *= scale;
    return a;
}

/**
 * @brief 为宿主注册 std_signal 模块的标准库接口。
 * @param registry 接收类型化函数绑定的宿主注册表。
 * @param error 绑定失败时接收原始诊断，可为空。
 * @return 全部入口绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTypedWithContext<std::int64_t, std::int64_t>(
               "std::signal::signalDft",
               [](Context &ctx, std::int64_t input) { return writeComplexes(ctx, dft(readComplexes(ctx, input))); }, error)
           && registry.bindTypedWithContext<std::int64_t, std::int64_t>(
               "std::signal::signalIdft",
               [](Context &ctx, std::int64_t input) { return writeComplexes(ctx, idft(readComplexes(ctx, input))); }, error)
           && registry.bindTypedWithContext<std::int64_t, std::int64_t>(
               "std::signal::signalFft",
               [](Context &ctx, std::int64_t input) {
                   const auto values = readComplexes(ctx, input);
                   const int n = static_cast<int>(values.size());
                   if (!(n > 0 && (n & (n - 1)) == 0))
                   {
                       ctx.fail("fft 长度必须是正的 2 的幂");
                       return std::int64_t{ 0 };
                   }
                   return writeComplexes(ctx, fft(values));
               },
               error)
           && registry.bindTypedWithContext<std::int64_t, std::int64_t>(
               "std::signal::signalIfft",
               [](Context &ctx, std::int64_t input) {
                   const auto values = readComplexes(ctx, input);
                   const int n = static_cast<int>(values.size());
                   if (!(n > 0 && (n & (n - 1)) == 0))
                   {
                       ctx.fail("ifft 长度必须是正的 2 的幂");
                       return std::int64_t{ 0 };
                   }
                   return writeComplexes(ctx, ifft(values));
               },
               error);
}
}  // namespace std_signal_glue
