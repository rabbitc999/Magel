#include "std_linalg.hpp"

#include "../chali_glue.hpp"
#include "std_scalar_buffer.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <limits>

/* `std::linalg` 的 LU 求解与顶层分解算法下沉到宿主 C++。
 *
 * 泛型类型定义与基础运算符保留在 CHALI；Matrix.solve 按内置数值类型注册特化，
 * 顶层分解函数采用 float64。求解保留逐步运算顺序与标量精度。
 *
 * 结构体是「摊平槽布局」：值字段（嵌套结构体）内联展开，每个标量/ScalarStorage 字段占一个 8 字节槽。
 *   Vector<float64>            = { ScalarStorage values }                          → 1 槽
 *   Matrix<float64>            = { int rows, int cols, ScalarStorage values }      → 3 槽
 *   LuFactorization            = { Matrix×3 }                             → 9 槽
 *   EigenDecomposition         = { Vector, Matrix }                       → 4 槽
 *   SingularValueDecomposition = { Matrix, Vector, Matrix }               → 7 槽
 *   JordanDecomposition        = { Matrix, Matrix }                       → 6 槽
 */
namespace std_linalg_glue
{

using Context = chali_glue::Context;
using Value = chali_glue::Value;

struct MatrixTag
{
};
struct VectorTag
{
};
using Matrix = chali_glue::Record<MatrixTag>;
using Vector = chali_glue::Record<VectorTag>;

struct MatrixData
{
    int rows = 0;
    int cols = 0;
    std::vector<double> values;
};
struct VectorData
{
    std::vector<double> values;
};
struct LUResult
{
    MatrixData lower, upper, permutation;
};
struct EigenResult
{
    VectorData eigenvalues;
    MatrixData eigenvectors;
};
struct SvdResult
{
    MatrixData u;
    VectorData singularValues;
    MatrixData vTranspose;
};
struct JordanResult
{
    MatrixData basis;
    MatrixData jordan;
};

/* ---- 槽读写 ---- */
/**
 * @brief 从结构体的指定八字节槽读取整数位模式。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param address 目标结构体或数组的运行时起始地址。
 * @param slot 从零开始的八字节槽下标。
 * @return 读取到的整数位模式；访问失败由上下文记录诊断。
 */
inline std::int64_t readSlot(Context &ctx, std::int64_t address, std::size_t slot)
{
    std::int64_t raw = 0;
    ctx.readBytes(address + static_cast<std::int64_t>(slot) * 8,
                  reinterpret_cast<char *>(&raw), 8);
    return raw;
}
/**
 * @brief 将整数位模式写入结构体指定槽。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param address 目标结构体或数组的运行时起始地址。
 * @param slot 从零开始的八字节槽下标。
 * @param value 需要写入该槽的数值或位模式。
 */
inline void writeSlot(Context &ctx, std::int64_t address, std::size_t slot, std::int64_t value)
{
    ctx.writeBytes(address + static_cast<std::int64_t>(slot) * 8,
                   reinterpret_cast<const char *>(&value), 8);
}
/**
 * @brief 将双精度浮点位模式写入结构体指定槽。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param address 目标结构体或数组的运行时起始地址。
 * @param slot 从零开始的八字节槽下标。
 * @param value 需要写入该槽的数值或位模式。
 */
inline void writeSlotF64(Context &ctx, std::int64_t address, std::size_t slot, double value)
{
    ctx.writeBytes(address + static_cast<std::int64_t>(slot) * 8,
                   reinterpret_cast<const char *>(&value), 8);
}

/* ---- ScalarStorage<float64> 读写 ---- */
/**
 * @brief 将标量 opcode 存储复制为双精度宿主数组。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param handle 当前模块中的资源句柄。
 * @return 与原记录独立的数值副本。
 */
inline std::vector<double> readListF64(Context &ctx, std::int64_t handle)
{
    const auto values = scalar_buffer::read<double>(ctx, handle);
    return std::vector<double>(values.begin(), values.end());
}
/**
 * @brief 为双精度数值创建拥有独立存储的标量记录。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param values 需要复制到新运行时存储的宿主数值数组。
 * @return 新建标量记录的运行时句柄。
 */
inline std::int64_t makeListF64(Context &ctx, const std::vector<double> &values)
{
    return scalar_buffer::make(ctx, values);
}

/* ---- Matrix / Vector 结构体读写（摊平槽） ---- */
/**
 * @brief 读取矩阵视图的尺寸和标量存储副本。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param m 作为输入或输出的行优先矩阵。
 * @return 按行优先保存元素的宿主矩阵。
 */
inline MatrixData readMatrix(Context &ctx, const Matrix &m)
{
    MatrixData out;
    out.rows = static_cast<int>(m.field<std::int64_t>(0u));
    out.cols = static_cast<int>(m.field<std::int64_t>(1u));
    out.values = readListF64(ctx, m.field<std::int64_t>(2u));
    return out;
}
/**
 * @brief 将矩阵尺寸及独立标量记录写入聚合结果的指定位置。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param address 目标结构体或数组的运行时起始地址。
 * @param base 目标聚合结构体内的起始槽下标。
 * @param m 作为输入或输出的行优先矩阵。
 */
inline void writeMatrixInto(Context &ctx, std::int64_t address, std::size_t base, const MatrixData &m)
{
    writeSlot(ctx, address, base + 0u, static_cast<std::int64_t>(m.rows));
    writeSlot(ctx, address, base + 1u, static_cast<std::int64_t>(m.cols));
    writeSlot(ctx, address, base + 2u, makeListF64(ctx, m.values));
}
/**
 * @brief 为矩阵结果创建临时结构体及独立标量存储。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param m 作为输入或输出的行优先矩阵。
 * @return 临时矩阵结构体的运行时地址。
 */
inline std::int64_t writeMatrix(Context &ctx, const MatrixData &m)
{
    const std::int64_t address = ctx.makeTemporary(3u * 8u);
    writeMatrixInto(ctx, address, 0u, m);
    return address;
}
/**
 * @brief 将向量的独立标量存储写入聚合结果指定槽。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param address 目标结构体或数组的运行时起始地址。
 * @param base 目标聚合结构体内的起始槽下标。
 * @param v 需要保存的数值。
 */
inline void writeVectorInto(Context &ctx, std::int64_t address, std::size_t base, const VectorData &v)
{
    writeSlot(ctx, address, base, makeListF64(ctx, v.values));
}

/* ---- 矩阵运算（照抄 CHALI 的 float64 语义，row-major） ---- */
/**
 * @brief 读取已验证行列范围内的矩阵元素。
 * @param m 作为输入或输出的行优先矩阵。
 * @param i 从零开始的行下标。
 * @param j 从零开始的列下标。
 * @return 对应行列的双精度数值。
 */
inline double at(const MatrixData &m, int i, int j) { return m.values[static_cast<std::size_t>(i * m.cols + j)]; }
/**
 * @brief 写入已验证行列范围内的矩阵元素。
 * @param m 作为输入或输出的行优先矩阵。
 * @param i 从零开始的行下标。
 * @param j 从零开始的列下标。
 * @param v 写入指定矩阵元素的数值。
 */
inline void set(MatrixData &m, int i, int j, double v) { m.values[static_cast<std::size_t>(i * m.cols + j)] = v; }
/**
 * @brief 创建按行优先保存的全零矩阵。
 * @param rows 输出矩阵的行数。
 * @param cols 输出矩阵的列数。
 * @return 具有指定行列数且独占元素数组的矩阵。
 */
inline MatrixData zerosMatrix(int rows, int cols) { return { rows, cols, std::vector<double>(static_cast<std::size_t>(rows * cols), 0.0) }; }
/**
 * @brief 创建指定长度的全零向量。
 * @param n 输出向量长度或方阵阶数。
 * @return 具有独立元素数组的零向量。
 */
inline VectorData zerosVector(int n) { return { std::vector<double>(static_cast<std::size_t>(n), 0.0) }; }
/**
 * @brief 创建指定阶数的单位矩阵。
 * @param n 输出向量长度或方阵阶数。
 * @return 主对角线为一、其余元素为零的方阵。
 */
inline MatrixData identityMatrix(int n)
{
    MatrixData m = zerosMatrix(n, n);
    for (int i = 0; i < n; ++i) set(m, i, i, 1.0);
    return m;
}
/**
 * @brief 交换矩阵行列并复制元素。
 * @param m 作为输入或输出的行优先矩阵。
 * @return 与输入存储独立的转置矩阵。
 */
inline MatrixData transpose(const MatrixData &m)
{
    MatrixData r = zerosMatrix(m.cols, m.rows);
    for (int i = 0; i < m.rows; ++i)
        for (int j = 0; j < m.cols; ++j) set(r, j, i, at(m, i, j));
    return r;
}
/**
 * @brief 按行优先布局计算矩阵乘积。
 * @param a 运算左侧的输入矩阵或复数数组。
 * @param b 运算右侧的输入矩阵。
 * @return 行数取自左矩阵、列数取自右矩阵的乘积。
 */
inline MatrixData multiply(const MatrixData &a, const MatrixData &b)
{
    MatrixData r = zerosMatrix(a.rows, b.cols);
    for (int i = 0; i < a.rows; ++i)
        for (int j = 0; j < b.cols; ++j)
        {
            double sum = 0.0;
            for (int k = 0; k < a.cols; ++k) sum += at(a, i, k) * at(b, k, j);
            set(r, i, j, sum);
        }
    return r;
}

/* ---- 6 个顶层算法 ---- */
/**
 * @brief 执行带主元选择的 LU 分解并构造排列信息。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param input 需要分解、求解或取列的输入矩阵。
 * @return 分解所得上下三角矩阵及排列结果。
 */
inline LUResult luDecomposeImpl(Context &ctx, const MatrixData &input)
{
    LUResult bad;
    if (input.rows != input.cols)
    {
        ctx.fail("LU 分解要求方阵");
        return bad;
    }
    const int n = input.rows;
    MatrixData lower = identityMatrix(n);
    MatrixData upper = input;
    MatrixData permutation = identityMatrix(n);
    for (int k = 0; k < n; ++k)
    {
        int pivot = k;
        double largest = std::fabs(at(upper, k, k));
        for (int row = k + 1; row < n; ++row)
        {
            const double candidate = std::fabs(at(upper, row, k));
            if (candidate > largest)
            {
                largest = candidate;
                pivot = row;
            }
        }
        if (!(largest > 0.000000000001))
        {
            ctx.fail("LU 分解遇到奇异或近奇异矩阵");
            return bad;
        }
        if (pivot != k)
        {
            for (int col = 0; col < n; ++col)
            {
                const double tmp = at(upper, k, col);
                set(upper, k, col, at(upper, pivot, col));
                set(upper, pivot, col, tmp);
            }
            for (int col = 0; col < n; ++col)
            {
                const double tmp = at(permutation, k, col);
                set(permutation, k, col, at(permutation, pivot, col));
                set(permutation, pivot, col, tmp);
            }
            for (int col = 0; col < k; ++col)
            {
                const double tmp = at(lower, k, col);
                set(lower, k, col, at(lower, pivot, col));
                set(lower, pivot, col, tmp);
            }
        }
        for (int row = k + 1; row < n; ++row)
        {
            const double factor = at(upper, row, k) / at(upper, k, k);
            set(lower, row, k, factor);
            set(upper, row, k, 0.0);
            for (int col = k + 1; col < n; ++col)
                set(upper, row, col, at(upper, row, col) - factor * at(upper, k, col));
        }
    }
    return { lower, upper, permutation };
}

/**
 * @brief 对实对称矩阵迭代求特征值和正交特征向量。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param input 需要分解、求解或取列的输入矩阵。
 * @return 特征值对角矩阵及对应特征向量矩阵。
 */
inline EigenResult symmetricEigenImpl(Context &ctx, const MatrixData &input)
{
    EigenResult bad;
    if (!(input.rows == input.cols && input.rows > 0))
    {
        ctx.fail("特征分解要求非空方阵");
        return bad;
    }
    const int n = input.rows;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (!(std::fabs(at(input, i, j) - at(input, j, i)) < 0.00000001))
            {
                ctx.fail("symmetricEigen 要求对称矩阵");
                return bad;
            }
    MatrixData a = input;
    MatrixData v = identityMatrix(n);
    int iteration = 0;
    while (iteration < 64 * n * n)
    {
        int p = 0, q = 0;
        double largest = 0.0;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
            {
                const double magnitude = std::fabs(at(a, i, j));
                if (magnitude > largest)
                {
                    largest = magnitude;
                    p = i;
                    q = j;
                }
            }
        if (largest < 0.0000000001) break;
        const double apq = at(a, p, q);
        const double tau = (at(a, q, q) - at(a, p, p)) / (2.0 * apq);
        double t = 1.0 / (std::fabs(tau) + std::sqrt(1.0 + tau * tau));
        if (tau < 0.0) t = -t;
        const double c = 1.0 / std::sqrt(1.0 + t * t);
        const double s = t * c;
        const double app = at(a, p, p), aqq = at(a, q, q);
        set(a, p, p, app - t * apq);
        set(a, q, q, aqq + t * apq);
        set(a, p, q, 0.0);
        set(a, q, p, 0.0);
        for (int i = 0; i < n; ++i)
        {
            if (i != p && i != q)
            {
                const double aip = at(a, i, p), aiq = at(a, i, q);
                set(a, i, p, c * aip - s * aiq);
                set(a, p, i, c * aip - s * aiq);
                set(a, i, q, s * aip + c * aiq);
                set(a, q, i, s * aip + c * aiq);
            }
            const double vip = at(v, i, p), viq = at(v, i, q);
            set(v, i, p, c * vip - s * viq);
            set(v, i, q, s * vip + c * viq);
        }
        ++iteration;
    }
    if (!(iteration < 64 * n * n))
    {
        ctx.fail("Jacobi 特征分解未收敛");
        return bad;
    }
    VectorData eigenvalues = zerosVector(n);
    MatrixData eigenvectors = v;
    for (int i = 0; i < n; ++i) eigenvalues.values[static_cast<std::size_t>(i)] = at(a, i, i);
    for (int i = 0; i < n; ++i)
    {
        int best = i;
        for (int j = i + 1; j < n; ++j)
            if (eigenvalues.values[static_cast<std::size_t>(j)] > eigenvalues.values[static_cast<std::size_t>(best)])
                best = j;
        if (best != i)
        {
            std::swap(eigenvalues.values[static_cast<std::size_t>(i)],
                      eigenvalues.values[static_cast<std::size_t>(best)]);
            for (int row = 0; row < n; ++row)
                std::swap(eigenvectors.values[static_cast<std::size_t>(row * n + i)],
                          eigenvectors.values[static_cast<std::size_t>(row * n + best)]);
        }
    }
    return { eigenvalues, eigenvectors };
}

/**
 * @brief 通过对称特征分解计算实矩阵的奇异值分解。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param input 需要分解、求解或取列的输入矩阵。
 * @return 左右奇异向量及奇异值矩阵。
 */
inline SvdResult svdImpl(Context &ctx, const MatrixData &input)
{
    SvdResult bad;
    if (!(input.rows > 0 && input.cols > 0))
    {
        ctx.fail("SVD 要求非空矩阵");
        return bad;
    }
    const int m = input.rows, n = input.cols;
    const int r = m < n ? m : n;
    const MatrixData inputTranspose = transpose(input);
    const MatrixData gram = multiply(inputTranspose, input);
    const EigenResult eigen = symmetricEigenImpl(ctx, gram);
    if (eigen.eigenvalues.values.empty()) return bad; /* ctx.fail 已置 */
    MatrixData u = zerosMatrix(m, r);
    VectorData singularValues = zerosVector(r);
    MatrixData vTranspose = zerosMatrix(r, n);
    for (int j = 0; j < r; ++j)
    {
        double value = eigen.eigenvalues.values[static_cast<std::size_t>(j)];
        if (value < 0.0) value = 0.0;
        const double sigma = std::sqrt(value);
        singularValues.values[static_cast<std::size_t>(j)] = sigma;
        for (int col = 0; col < n; ++col)
            set(vTranspose, j, col, at(eigen.eigenvectors, col, j));
        if (sigma > 0.0000000001)
        {
            for (int row = 0; row < m; ++row)
            {
                double sum = 0.0;
                for (int col = 0; col < n; ++col)
                    sum += at(input, row, col) * at(eigen.eigenvectors, col, j);
                set(u, row, j, sum / sigma);
            }
        }
        else
        {
            int basis = 0;
            while (basis < m)
            {
                for (int row = 0; row < m; ++row)
                    set(u, row, j, row == basis ? 1.0 : 0.0);
                for (int prev = 0; prev < j; ++prev)
                {
                    const double projection = at(u, basis, prev);
                    for (int row = 0; row < m; ++row)
                        set(u, row, j, at(u, row, j) - projection * at(u, row, prev));
                }
                double length = 0.0;
                for (int row = 0; row < m; ++row) length += at(u, row, j) * at(u, row, j);
                if (length > 0.000000000001)
                {
                    length = std::sqrt(length);
                    for (int row = 0; row < m; ++row) set(u, row, j, at(u, row, j) / length);
                    break;
                }
                ++basis;
            }
        }
    }
    return { u, singularValues, vTranspose };
}

/**
 * @brief 按当前支持的阶数计算实 Jordan 分解。
 * @param ctx 提供内存访问、资源创建和诊断的当前调用上下文。
 * @param input 需要分解、求解或取列的输入矩阵。
 * @return 基矩阵及 Jordan 形；不支持的输入通过上下文诊断。
 */
inline JordanResult jordan2Impl(Context &ctx, const MatrixData &input)
{
    JordanResult bad;
    if (!(input.rows == 2 && input.cols == 2))
    {
        ctx.fail("jordan2 只支持 2x2 矩阵");
        return bad;
    }
    const double a = at(input, 0, 0), b = at(input, 0, 1);
    const double c = at(input, 1, 0), d = at(input, 1, 1);
    double discriminant = (a - d) * (a - d) + 4.0 * b * c;
    if (!(discriminant >= -0.000000000001))
    {
        ctx.fail("jordan2 不支持复特征值");
        return bad;
    }
    if (discriminant < 0.0) discriminant = 0.0;
    const double root = std::sqrt(discriminant);
    const double first = (a + d + root) / 2.0, second = (a + d - root) / 2.0;
    MatrixData basis = identityMatrix(2);
    MatrixData jordan = zerosMatrix(2, 2);
    set(jordan, 0, 0, first);
    set(jordan, 1, 1, second);
    if (root > 0.00000001)
    {
        double x0 = b, y0 = first - a, x1 = b, y1 = second - a;
        if (std::fabs(x0) + std::fabs(y0) < 0.00000001)
        {
            x0 = first - d;
            y0 = c;
        }
        if (std::fabs(x1) + std::fabs(y1) < 0.00000001)
        {
            x1 = second - d;
            y1 = c;
        }
        set(basis, 0, 0, x0);
        set(basis, 1, 0, y0);
        set(basis, 0, 1, x1);
        set(basis, 1, 1, y1);
    }
    else if (std::fabs(a - first) + std::fabs(b) + std::fabs(c) + std::fabs(d - first) > 0.00000001)
    {
        set(jordan, 0, 1, 1.0);
        if (std::fabs(b) > 0.00000001)
        {
            set(basis, 0, 0, b);
            set(basis, 1, 0, d - first);
            set(basis, 0, 1, 0.0);
            set(basis, 1, 1, 1.0);
        }
        else
        {
            set(basis, 0, 0, a - first);
            set(basis, 1, 0, c);
            set(basis, 0, 1, 1.0);
            set(basis, 1, 1, 0.0);
        }
    }
    return { basis, jordan };
}

/**
 * @brief 计算消元差值，浮点逐步舍入，整数按位宽回绕，避免 C++ 有符号溢出。
 * @param value 被减数。
 * @param factor 乘数。
 * @param other 被乘数。
 * @return value - factor * other。
 */
template <typename Scalar>
inline Scalar subtractProduct(Scalar value, Scalar factor, Scalar other)
{
    if constexpr (std::is_integral_v<Scalar>)
    {
        using Unsigned = std::make_unsigned_t<Scalar>;
        const Unsigned bits = static_cast<Unsigned>(static_cast<std::uint64_t>(value) - static_cast<std::uint64_t>(factor) * static_cast<std::uint64_t>(other));
        Scalar result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
    else
    {
        const Scalar product = static_cast<Scalar>(factor * other);
        return static_cast<Scalar>(value - product);
    }
}

/**
 * @brief 标量除法，整数最小值除以负一时按原位宽回绕。
 * @param numerator 分子。
 * @param denominator 已检查非零的主元。
 * @return 标量商。
 */
template <typename Scalar>
inline Scalar divideScalar(Scalar numerator, Scalar denominator)
{
    if constexpr (std::is_integral_v<Scalar> && std::is_signed_v<Scalar>)
        if (numerator == std::numeric_limits<Scalar>::min() && denominator == -1)
            return numerator;
    return static_cast<Scalar>(numerator / denominator);
}

/**
 * @brief 按原标量精度进行部分主元 LU 求解，不修改输入矩阵与右端向量。
 * @param ctx 当前运行时上下文。
 * @param input 行优先矩阵视图。
 * @param rhs 右端向量视图。
 * @return 新向量的临时结构体地址；失败时返回零并设置运行时错误。
 */
template <typename Scalar>
inline std::int64_t matrixSolve(Context &ctx, Matrix input, Vector rhs)
{
    const auto rows = input.field<std::int64_t>(0);
    const auto cols = input.field<std::int64_t>(1);
    auto lu = scalar_buffer::read<Scalar>(ctx, input.field<std::int64_t>(2));
    auto b = scalar_buffer::read<Scalar>(ctx, rhs.field<std::int64_t>(0));
    if (rows < 0 || rows != cols || static_cast<std::uint64_t>(rows) != b.size() || (rows != 0 && (lu.size() / static_cast<std::uint64_t>(rows) != static_cast<std::uint64_t>(rows) || lu.size() % static_cast<std::uint64_t>(rows) != 0)) || (rows == 0 && !lu.empty()))
    {
        ctx.fail("Matrix.solve 维度不匹配");
        return 0;
    }
    const auto n = static_cast<std::size_t>(rows);
    auto at = [&](std::size_t row, std::size_t col) -> Scalar & { return lu[row * n + col]; };
    for (std::size_t k = 0; k < n; ++k)
    {
        std::size_t pivot = k;
        double largest = std::fabs(static_cast<double>(at(k, k)));
        for (std::size_t row = k + 1; row < n; ++row)
        {
            const double candidate = std::fabs(static_cast<double>(at(row, k)));
            if (candidate > largest)
            {
                largest = candidate;
                pivot = row;
            }
        }
        if (!(largest > 1e-12))
        {
            ctx.fail("Matrix.solve 奇异或近奇异矩阵");
            return 0;
        }
        if (pivot != k)
        {
            for (std::size_t col = 0; col < n; ++col) std::swap(at(k, col), at(pivot, col));
            std::swap(b[k], b[pivot]);
        }
        for (std::size_t row = k + 1; row < n; ++row)
        {
            const Scalar factor = divideScalar(at(row, k), at(k, k));
            at(row, k) = factor;
            for (std::size_t col = k + 1; col < n; ++col)
                at(row, col) = subtractProduct(at(row, col), factor, at(k, col));
            b[row] = subtractProduct(b[row], factor, b[k]);
        }
    }
    Memory::Vector<Scalar> answer(n, Scalar{});
    for (std::size_t remaining = n; remaining > 0; --remaining)
    {
        const auto i = remaining - 1;
        Scalar sum = b[i];
        for (std::size_t j = i + 1; j < n; ++j)
            sum = subtractProduct(sum, at(i, j), answer[j]);
        answer[i] = divideScalar(sum, at(i, i));
    }
    const auto address = ctx.makeTemporary(8);
    writeSlot(ctx, address, 0, scalar_buffer::make(ctx, answer));
    return address;
}

/**
 * @brief 从行优先矩阵复制指定列，返回独立向量。
 * @param ctx 当前运行时上下文。
 * @param input 矩阵视图。
 * @param key 零起始列索引。
 * @return 新向量地址；非法索引或尺寸时报告错误。
 */
template <typename Scalar>
inline std::int64_t matrixColumn(Context &ctx, Matrix input, std::int64_t key)
{
    const auto rows = input.field<std::int64_t>(0);
    const auto cols = input.field<std::int64_t>(1);
    if (rows < 0 || cols <= 0 || key < 0 || key >= cols)
    {
        ctx.fail("Matrix.column 列索引越界或维度无效");
        return 0;
    }
    auto values = scalar_buffer::read<Scalar>(ctx, input.field<std::int64_t>(2));
    if (values.size() / static_cast<std::uint64_t>(cols) != static_cast<std::uint64_t>(rows) || values.size() % static_cast<std::uint64_t>(cols) != 0)
    {
        ctx.fail("Matrix.column 存储长度与维度不匹配");
        return 0;
    }
    Memory::Vector<Scalar> column;
    column.reserve(static_cast<std::size_t>(rows));
    for (std::int64_t row = 0; row < rows; ++row)
        column.push_back(values[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(key)]);
    const auto address = ctx.makeTemporary(8);
    writeSlot(ctx, address, 0, scalar_buffer::make(ctx, column));
    return address;
}

/**
 * @brief 注册指定数值标量的泛型求解胶水。
 * @param registry 宿主函数注册表。
 * @param name CHALI 标量类型名。
 * @param error 绑定失败时的错误输出。
 * @return 是否注册成功。
 */
template <typename Scalar>
inline bool bindMatrixSolve(chali_glue::Registry &registry, const Memory::String &name,
                            Memory::String *error)
{
    return registry.bindTypedWithContext<std::int64_t, Matrix, Vector>(
               chali_glue::specialize("std::linalg::__matrixSolve", { name }),
               matrixSolve<Scalar>, error)
           && registry.bindTypedWithContext<std::int64_t, Matrix, std::int64_t>(
               chali_glue::specialize("std::linalg::__matrixColumn", { name }),
               matrixColumn<Scalar>, error);
}

/* ---- 绑定泛型求解与六个顶层分解函数 ---- */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return scalar_buffer::bind(registry, error) && bindMatrixSolve<float>(registry, "float32", error) && bindMatrixSolve<double>(registry, "float64", error) && bindMatrixSolve<std::int8_t>(registry, "int8", error) && bindMatrixSolve<std::int16_t>(registry, "int16", error) && bindMatrixSolve<std::int32_t>(registry, "int32", error) && bindMatrixSolve<std::int64_t>(registry, "int64", error) && bindMatrixSolve<std::uint8_t>(registry, "uint8", error) && bindMatrixSolve<std::uint16_t>(registry, "uint16", error) && bindMatrixSolve<std::uint32_t>(registry, "uint32", error) && bindMatrixSolve<std::uint64_t>(registry, "uint64", error) && registry.bindTypedWithContext<std::int64_t, Matrix>("std::linalg::luDecompose", [](Context &ctx, Matrix input) -> std::int64_t {
                   const LUResult r = luDecomposeImpl(ctx, readMatrix(ctx, input));
                   if (r.lower.values.empty()) return 0;
                   const std::int64_t address = ctx.makeTemporary(9u * 8u);
                   writeMatrixInto(ctx, address, 0u, r.lower);
                   writeMatrixInto(ctx, address, 3u, r.upper);
                   writeMatrixInto(ctx, address, 6u, r.permutation);
                   return address; }, error) && registry.bindTypedWithContext<std::int64_t, Matrix>("std::linalg::lurDecompose", [](Context &ctx, Matrix input) -> std::int64_t {
                   const LUResult r = luDecomposeImpl(ctx, readMatrix(ctx, input));
                   if (r.lower.values.empty()) return 0;
                   const std::int64_t address = ctx.makeTemporary(9u * 8u);
                   writeMatrixInto(ctx, address, 0u, r.lower);
                   writeMatrixInto(ctx, address, 3u, r.upper);
                   writeMatrixInto(ctx, address, 6u, r.permutation);
                   return address; }, error) && registry.bindTypedWithContext<std::int64_t, Matrix>("std::linalg::symmetricEigen", [](Context &ctx, Matrix input) -> std::int64_t {
                   const EigenResult r = symmetricEigenImpl(ctx, readMatrix(ctx, input));
                   if (r.eigenvalues.values.empty() && r.eigenvectors.values.empty()) return 0;
                   const std::int64_t address = ctx.makeTemporary(4u * 8u);
                   writeVectorInto(ctx, address, 0u, r.eigenvalues);
                   writeMatrixInto(ctx, address, 1u, r.eigenvectors);
                   return address; }, error) && registry.bindTypedWithContext<std::int64_t, Matrix>("std::linalg::svd", [](Context &ctx, Matrix input) -> std::int64_t {
                   const SvdResult r = svdImpl(ctx, readMatrix(ctx, input));
                   if (r.u.values.empty() && r.singularValues.values.empty() && r.vTranspose.values.empty())
                       return 0;
                   const std::int64_t address = ctx.makeTemporary(7u * 8u);
                   writeMatrixInto(ctx, address, 0u, r.u);
                   writeVectorInto(ctx, address, 3u, r.singularValues);
                   writeMatrixInto(ctx, address, 4u, r.vTranspose);
                   return address; }, error) && registry.bindTypedWithContext<std::int64_t, Matrix>("std::linalg::jordan2", [](Context &ctx, Matrix input) -> std::int64_t {
                   const JordanResult r = jordan2Impl(ctx, readMatrix(ctx, input));
                   if (r.basis.values.empty() && r.jordan.values.empty()) return 0;
                   const std::int64_t address = ctx.makeTemporary(6u * 8u);
                   writeMatrixInto(ctx, address, 0u, r.basis);
                   writeMatrixInto(ctx, address, 3u, r.jordan);
                   return address; }, error) && registry.bindTypedWithContext<std::int64_t, Matrix>("std::linalg::diagonalize2", [](Context &ctx, Matrix input) -> std::int64_t {
                   const JordanResult r = jordan2Impl(ctx, readMatrix(ctx, input));
                   if (r.basis.values.empty() && r.jordan.values.empty()) return 0;
                   if (!(std::fabs(at(r.jordan, 0, 1)) < 0.00000001)) { ctx.fail("矩阵不可对角化"); return 0; }
                   const std::int64_t address = ctx.makeTemporary(6u * 8u);
                   writeMatrixInto(ctx, address, 0u, r.basis);
                   writeMatrixInto(ctx, address, 3u, r.jordan);
                   return address; }, error);
}

}  // namespace std_linalg_glue
