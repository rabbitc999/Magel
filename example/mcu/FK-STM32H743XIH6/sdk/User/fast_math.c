#include "fast_math.h"
#include <stdint.h>
#include <math.h>
#include <float.h>

static float betainv(float p, float a, float b);
static float betainc(float x, float a, float b);
static float beta(float z, float w);
static float ln_gamma(float xx);
static float beta_cf(float a, float b, float x);
static float gammainc(float x, float a);
static float gammaincc(float x, float a);

/* 三角函数表格，包含0-90度，步长0.3度 */
#define QUARTER_TABLE_SIZE 300 /* 90/0.3 + 1 = 301 */
#define DEGREE_STEP        0.3f

float sinQuarterTable[QUARTER_TABLE_SIZE + 1] = { 0 };

/**
 * @brief  初始化数学库，预计算正弦查找表
 */
void Math_Init(void)
{
    for (int i = 0; i <= QUARTER_TABLE_SIZE; i++)
    {
        float angle_rad = i * DEGREE_STEP * M_PI / 180.0f;
        sinQuarterTable[i] = sinf(angle_rad);
    }
}

/**
 * @brief  将角度归一化到0-360度
 * @param  x 输入角度
 * @retval 归一化后的角度
 */
static float Math_NormalizeAngle(float x)
{
    if (!isfinite(x))
    {
        return 0.0f;
    }

    /* 使用fmodf将角度转换到0-360度 */
    x = fmodf(x, 360.0f);
    if (x < 0.0f)
    {
        x += 360.0f;
    }
    return x;
}

/**
 * @brief  通过查表法计算正弦值
 * @param  x 输入角度值，单位为度
 * @retval 计算得到的正弦值
 */
float Math_Sin(float x)
{
    if (!isfinite(x))
    {
        return 0.0f;
    }

    /* 归一化到0-360度 */
    x = Math_NormalizeAngle(x);

    /* 利用对称性 */
    float sign = 1.0f;

    if (x > 270.0f)
    { /* 270-360度 */
        x = 360.0f - x;
        sign = -1.0f;
    }
    else if (x > 180.0f)
    { /* 180-270度 */
        x = x - 180.0f;
        sign = -1.0f;
    }
    else if (x > 90.0f)
    { /* 90-180度 */
        x = 180.0f - x;
    }

    /* 0-90度：直接使用 */
    float index_float = x / DEGREE_STEP;

    /* sin(90°) = 1 */
    if (x >= 90.0f)
    {
        return 1.0f;
    }

    int32_t i = (int32_t)index_float;
    if (i >= QUARTER_TABLE_SIZE - 1)
    {
        i = QUARTER_TABLE_SIZE - 2;
    }

    float fraction = index_float - i;
    return sign * (sinQuarterTable[i] + (sinQuarterTable[i + 1] - sinQuarterTable[i]) * fraction);
}

/**
 * @brief  通过查表法计算余弦值
 * @param  x 输入角度值，单位为度
 * @retval 计算得到的余弦值
 * @note   利用三角函数关系 cos(x) = sin(x + 90°)
 */
float Math_Cos(float x)
{
    return Math_Sin(x + 90.0f);
}

/**
 * @brief  通过查表法计算正切值
 * @param  x 输入角度值，单位为度
 * @retval 计算得到的正切值
 * @note   增加除零保护
 */
float Math_Tan(float x)
{
    /* 处理NaN和无穷大 */
    if (!isfinite(x)) { return 0.0f; }
    if (isnan(x)) { return 0.0f; }

    float sinVal = Math_Sin(x);
    float cosVal = Math_Cos(x);

    /* 除零保护 */
    if (fabsf(cosVal) < 1e-10f)
    {
        return (sinVal > 0) ? FLT_MAX : -FLT_MAX;
    }
    return sinVal / cosVal;
}

/**
 * @brief  计算指数函数值
 * @param  x 输入值
 * @retval 输入浮点数的指数函数值
 * @note   使用floor分离整数和小数部分，并用预计算表避免循环
 */
float Math_Exp(float x)
{
    /* 处理NaN和无穷大 */
    if (!isfinite(x)) { return 0.0f; }
    if (isnan(x)) { return 0.0f; }
    if (x == 0.0f) return 1.0f;

    /* 处理非常大的负数，防止下溢 */
    if (x < -87.0f) return 0.0f;
    /* 处理非常大的正数，防止上溢 */
    if (x > 88.0f) return 1e38f;

    /* 处理负数: exp(-x) = 1 / exp(x) */
    if (x < 0.0f)
    {
        return 1.0f / Math_Exp(-x);
    }

    /* 1. 范围缩减: x = k * ln(2) + r */
    int k = (int)floorf(x / LN_2);
    float r = x - k * LN_2;

    /* 2. 对小数部分 r 使用帕德逼近 */
    /* exp(r) ≈ (1 + r/2 + r?/10 + r?/120) / (1 - r/2 + r?/10 - r?/120) */
    float r2 = r * r;
    float r3 = r2 * r;

    /* 为了提高精度和稳定性，可以分解计算 */
    /* 分子: 1 + r/2 + r?/10 + r?/120 */
    float numerator = 1.0f + r * (0.5f + r * (0.1f + r * (1.0f / 120.0f)));
    /* 分母: 1 - r/2 + r?/10 - r?/120 */
    float denominator = 1.0f - r * (0.5f - r * (0.1f - r * (1.0f / 120.0f)));

    float expR = numerator / denominator;

    /* 3. 合并结果: exp(x) = 2^k * exp(r) */
    return ldexpf(expR, k); /* ldexpf(a, b) 等价于 a * 2^b */
}

/**
 * @brief  计算自然对数函数值
 * @param  x 输入值，需为正数
 * @retval 输入浮点数的自然对数值
 * @note   使用除法将输入值转换到1-2范围内，避免递归调用
 */
float Math_Log(float x)
{
    /* 处理NaN和无穷大 */
    if (!isfinite(x)) { return 0.0f; }
    if (isnan(x)) { return 0.0f; }

    if (x <= 0.0f) return -1e38f; /* 无效输入 */
    if (x == 1.0f) return 0;

    /* 1. 范围缩减: x = m * 2^p */
    int p;
    float m = frexpf(x, &p); /* frexpf返回 m in [0.5, 2) */
                             /* 为了方便，我们调整到 m in [0.5, 1) */
    if (m >= 1.0f)
    {
        m /= 2.0f;
        p++;
    }

    /* 现在 m 在 [0.5, 1) 区间, p 是整数 */
    /* log(x) = log(m) + p * ln(2) */

    /* 2. 对尾数 m 使用帕德逼近 */
    /* 令 z = m - 1, z in [-0.5, 0) */
    float z = m - 1.0f;
    float z2 = z * z;

    /* log(1+z) ≈ z * (6 + z?) / (6 + 2*z?) */
    float logM = z * (6.0f + z2) / (6.0f + 2.0f * z2);

    /* 3. 合并结果 */
    return logM + p * LN_2;
}

/**
 * @brief  快速计算浮点数的倒数平方根
 * @param  x 输入的浮点数
 * @retval 输入浮点数的倒数平方根
 */
float Math_InvSqrt(float x)
{
    /* 处理NaN和无穷大 */
    if (!isfinite(x)) { return 0.0f; }
    if (isnan(x)) { return 0.0f; }

    if (x <= 0.0f) return 0; /* 返回0 */

    int32_t i;
    float x2, y;
    const float threehalfs = 1.5f;
    x2 = x * 0.5f;
    y = x;
    i = *(int32_t *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *)&i;
    y = y * (threehalfs - (x2 * y * y));
    return y;
}

/**
 * @brief  快速计算浮点数的平方根
 * @param  x 输入的浮点数
 * @retval 输入浮点数的平方根近似值
 */
float Math_Sqrt(float x)
{
    /* 处理NaN和无穷大 */
    if (!isfinite(x)) { return 0.0f; }
    if (isnan(x)) { return 0.0f; }

    return 1.0f / Math_InvSqrt(x);
}

/**
 * @brief  快速计算反正切函数 (y/x)
 * @param  y Y坐标分量
 * @param  x X坐标分量
 * @retval 角度值 [-90, 90]
 * @note   使用多项式近似实现
 */
float Math_Atan2(float y, float x)
{
    /* 处理NaN和无穷大 */
    if (!isfinite(x)) { return 0.0f; }
    if (isnan(x)) { return 0.0f; }

    if (x == 0.0f)
    {
        return (y > 0.0f) ? M_PI / 2 : -M_PI / 2;
    }

    float absY = fabsf(y) + 1e-10f; /* 防止除零 */
    float angle;

    if (x >= 0.0f)
    {
        float r = (x - absY) / (x + absY);
        angle = 0.1963f * r * r * r - 0.9817f * r + M_PI / 4;
    }
    else
    {
        float r = (x + absY) / (absY - x);
        angle = 0.1963f * r * r * r - 0.9817f * r + 3 * M_PI / 4;
    }

    /* 判断输出的合法性 */
    if (!isnan(angle))
    {
        return ((y < 0.0f) ? -angle : angle) * (180 / M_PI);
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  快速计算反正弦函数
 * @param  x 输入值，范围[-1, 1]
 * @retval 弧度值，范围[-π/2, π/2]
 * @note   使用关系式 asin(x) = atan2(x, sqrt(1 - x?))
 */
float Math_Asin(float x)
{
    /* 处理NaN和无穷大 */
    if (!isfinite(x)) { return 0.0f; }
    if (isnan(x)) { return 0.0f; }

    /* 输入范围检查 */
    if (x <= -1.0f) return -M_PI / 2.0f;
    if (x >= 1.0f) return M_PI / 2.0f;

    /* 边界值处理 */

    /* 计算 sqrt(1 - x?) 并组合 atan2 */
    float sqrtVal = Math_Sqrt(1.0f - x * x);
    return Math_Atan2(x, sqrtVal);
}

/**
 * @brief  快速计算反余弦函数
 * @param  x 输入值，范围[-1, 1]
 * @retval 弧度值，范围[0, π]
 * @note   使用关系式 acos(x) = atan2(sqrt(1 - x?), x)
 */
float Math_Acos(float x)
{
    /* 处理NaN和无穷大 */
    if (!isfinite(x)) { return 0.0f; }
    if (isnan(x)) { return 0.0f; }

    /* 边界值处理 */
    if (x >= 1.0f) return 0.0f;
    if (x <= -1.0f) return M_PI;

    /* 计算 sqrt(1 - x?) 并组合 atan2 */
    float sqrtVal = Math_Sqrt(1.0f - x * x);
    return Math_Atan2(sqrtVal, x);
}

/**
 * @brief  计算F分布的右尾概率
 * @param  F F统计量的值
 * @param  m 自由度1
 * @param  n 自由度2
 * @retval 右尾概率p值
 */
float Math_FDist(float F, float m, float n)
{
    if (m <= 0 || n <= 0) { return -1; }
    if (F <= 0) { return 0.0f; }  // F<=0时概率为0

    // 修正公式：使用第二种形式
    float xx = (m * F) / (m * F + n);
    return betainc(xx, m / 2.0f, n / 2.0f);
}

/**
 * @brief  计算T分布的右尾概率
 * @param  t T统计量的值
 * @param  n 自由度
 * @retval 右尾概率p值
 */
float Math_TDist(float t, float n)
{
    if (n <= 0) { return -1; }

    float x = n / (n + t * t);

    if (t >= 0) { return 1 - 0.5f * betainc(x, n / 2.0, 0.5f); }
    else
    {
        return 0.5f * betainc(x, n / 2.0, 0.5f);
    }
}

/**
 * @brief  计算卡方分布的右尾概率
 * @param  x 卡方统计量的值
 * @param  n 自由度
 * @retval 右尾概率p值
 */
float Math_ChiDist(float x, float n)
{
    if (n <= 0 || x < 0) { return -1; }
    if (x == 0) { return 0.0f; }

    return gammainc(x / 2.0, n / 2.0);
}

/**
 * @brief  计算标准正态分布的右尾概率
 * @param  x 正态统计量的值
 * @retval 右尾概率p值
 */
float Math_NormDist(float x)
{
    // 使用误差函数计算标准正态分布的CDF
    if (x == 0) { return 0.5f; }

    // 近似计算公式
    const float a1 = 0.31938153f;
    const float a2 = -0.356563782f;
    const float a3 = 1.781477937f;
    const float a4 = -1.821255978f;
    const float a5 = 1.330274429f;
    const float gamma = 0.2316419f;

    float t = 1.0 / (1.0 + gamma * fabs(x));
    float n = (1.0 / sqrt(2 * M_PI)) * exp(-0.5f * x * x) * (a1 * t + a2 * t * t + a3 * t * t * t + a4 * t * t * t * t + a5 * t * t * t * t * t);

    if (x >= 0) { return 1.0 - n; }
    else
    {
        return n;
    }
}

/**
 * @brief  计算gamma函数的对数值
 * @param  x 自变量x
 * @retval gamma函数的对数值
 */
float Math_LnGamma(float x)
{
    return ln_gamma(x);
}

/**
 * @brief  计算gamma函数值
 * @param  x 自变量x
 * @retval gamma函数值
 */
float Math_Gamma(float x)
{
    return Math_Exp(ln_gamma(x));
}

/**
 * @brief  计算Beta函数值
 * @param  z 参数z
 * @param  w 参数w
 * @retval Beta函数值
 */
float Math_Beta(float z, float w)
{
    return beta(z, w);
}

/**
 * @brief  计算上不完全Gamma函数（正则化）
 * @param  x 下限
 * @param  a 形状参数
 * @retval Q(a,x) = Γ(a,x)/Γ(a)
 */
static float gammaincc(float x, float a)
{
    return 1.0 - gammainc(x, a);
}

/**
 * @brief  使用级数展开计算下不完全Gamma函数
 * @param  x 上限
 * @param  a 形状参数
 * @retval P(a,x)
 */
static float gammainc_series(float x, float a)
{
    int n;
    float sum, term, ap;

    ap = a;
    term = 1.0 / a;
    sum = term;

    for (n = 1; n <= 100; n++)
    {
        ap += 1.0;
        term *= x / ap;
        sum += term;
        if (fabs(term) < fabs(sum) * 1e-7) break;
    }

    return sum * exp(-x + a * log(x) - ln_gamma(a));
}

/**
 * @brief  使用连分式计算上不完全Gamma函数
 * @param  x 下限
 * @param  a 形状参数
 * @retval Q(a,x)
 */
static float gammaincc_contfrac(float x, float a)
{
    int n;
    float an, b, c, d, h, del;

    b = x + 1.0 - a;
    c = 1.0 / 1e-30;
    d = 1.0 / b;
    h = d;

    for (n = 1; n <= 100; n++)
    {
        an = n * (a - n);
        b += 2.0;
        d = an * d + b;
        if (fabs(d) < 1e-30) d = 1e-30;
        c = b + an / c;
        if (fabs(c) < 1e-30) c = 1e-30;
        d = 1.0 / d;
        del = d * c;
        h *= del;
        if (fabs(del - 1.0) < 1e-7) break;
    }

    return h * exp(-x + a * log(x) - ln_gamma(a));
}

/**
 * @brief  计算下不完全Gamma函数（正则化）
 * @param  x 上限
 * @param  a 形状参数
 * @retval P(a,x) = γ(a,x)/Γ(a)
 */
static float gammainc(float x, float a)
{
    if (x < 0 || a <= 0) return 0.0;
    if (x == 0) return 0.0;
    if (x < a + 1.0) { return gammainc_series(x, a); }
    else
    {
        return 1.0 - gammaincc_contfrac(x, a);
    }
}

/**
 * @brief  计算不完全Beta函数的反函数
 * @param  p 不完全Beta函数的值
 * @param  a 参数a
 * @param  b 参数b
 * @retval 使得betainc(x,a,b)=p的x值
 */
static float betainv(float p, float a, float b)
{
    const int count_max_limit = 100;
    const float crit = 6.4155306e-6f;  // 单精度时的设置，双精度时的设置1.818989403545857e-12
    int count_max = 0;
    float x, xnew, y, h, pbeta, logkerna, logkernb;

    if (p == 0) { x = 0; }
    if (p == 1) { x = 1; }

    x = a / (a + b);
    if (x < crit) { x = crit; }
    if (x > 1 - crit) { x = 1 - crit; }
    h = 1;

    while ((h > (crit * fabs(x))) && (h > crit) && (count_max < count_max_limit))
    {
        count_max = count_max + 1;

        if (x > 1) { p = 1; }
        pbeta = betainc(x, a, b);
        if (pbeta > 1) { pbeta = 1; }

        logkerna = (a - 1) * log(x);
        if ((a == 1) && (x == 0)) { logkerna = 0; }
        logkernb = (b - 1) * log(1 - x);
        if ((b == 1) && (x == 1)) { logkernb = 0; }
        y = exp(logkerna + logkernb - log(beta(a, b)));

        h = (pbeta - p) / y;
        xnew = x - h;

        if (xnew <= 0) { xnew = x * 0.1f; }
        if (xnew >= 1) { xnew = 1 - (1 - x) * 0.1f; }

        x = xnew;
    }
    return x;
}

/**
 * @brief  计算Beta函数值
 * @param  z 参数z
 * @param  w 参数w
 * @retval Beta函数值
 */
static float beta(float z, float w)
{
    int m, n, k, l, j;
    float h[10], bb[10], hh, t1, s1, ep, s, x, t2, g;
    float eps = 1e-10f;

    m = 1;
    n = 1;
    hh = 1;
    h[0] = hh;
    t1 = 0;
    s1 = t1;
    bb[0] = s1;
    ep = 1.0f + eps;

    while ((ep >= eps) && (m <= 9))
    {
        s = 0.0f;
        for (k = 0; k <= n - 1; k++)
        {
            x = (k + 0.5f) * hh;
            s = s + pow(x, z - 1) * pow(1 - x, w - 1);  // 对f(x)=pow(x,z-1)*pow(1-x,w-1)的积分
        }

        t2 = (t1 + hh * s) / 2.0f;
        m = m + 1;
        h[m - 1] = h[m - 2] / 2.0f;
        g = t2;
        l = 0;
        for (j = 2; j <= m; j++)
        {
            s = g - bb[j - 2];
            if (l == 0)
            {
                if (fabs(s) + 1.0f == 1.0f) { l = 1; }
                else
                {
                    g = (h[m - 1] - h[j - 2]) / s;
                }
            }
        }
        bb[m - 1] = g;
        if (l != 0) bb[m - 1] = 1.0e+35f;
        g = bb[m - 1];
        for (j = m; j >= 2; j--) g = bb[j - 2] - h[j - 2] / g;
        ep = fabs(g - s1);
        s1 = g;
        t1 = t2;
        hh = hh / 2.0;
        n = n + n;
    }
    return (g);
}

/**
 * @brief  计算不完全Beta函数
 * @param  x 自变量x
 * @param  a 参数a
 * @param  b 参数b
 * @retval 不完全Beta函数值
 */
static float betainc(float x, float a, float b) /* 不完全Beta函数 */
{
    float y, BT, AAA;

    if (x == 0 || x == 1) { BT = 0; }
    else
    {
        AAA = ln_gamma(a + b) - ln_gamma(a) - ln_gamma(b);
        BT = exp(AAA + a * log(x) + b * log(1 - x));
    }
    if (x < (a + 1) / (a + b + 2)) { y = BT * beta_cf(a, b, x) / a; }
    else
    {
        y = 1 - BT * beta_cf(b, a, 1 - x) / b;
    }

    return y;
}

/**
 * @brief  计算Beta函数的连分式部分
 * @param  a 参数a
 * @param  b 参数b
 * @param  x 自变量x
 * @retval 连分式部分值
 */
static float beta_cf(float a, float b, float x)
{
    const int count_max = 100;
    const float eps = 1e-6f;
    int count;
    float AM = 1;
    float BM = 1;
    float AZ = 1;
    float QAB;
    float QAP;
    float QAM;
    float BZ, EM, TEM, D, AP, BP, AAP, BPP, AOLD;

    QAB = a + b;
    QAP = a + 1;
    QAM = a - 1;
    BZ = 1 - QAB * x / QAP;

    for (count = 1; count <= count_max; count++)
    {
        EM = count;
        TEM = EM + EM;
        D = EM * (b - count) * x / ((QAM + TEM) * (a + TEM));
        AP = AZ + D * AM;
        BP = BZ + D * BM;
        D = -(a + EM) * (QAB + EM) * x / ((a + TEM) * (QAP + TEM));
        AAP = AP + D * AZ;
        BPP = BP + D * BZ;
        AOLD = AZ;
        AM = AP / BPP;
        BM = BP / BPP;
        AZ = AAP / BPP;
        BZ = 1;
        if (fabs(AZ - AOLD) < eps * fabs(AZ)) return (AZ);
    }
    return AZ;
}

/**
 * @brief  计算gamma函数的对数值
 * @param  x 自变量x
 * @retval gamma函数的对数值
 */
static float ln_gamma(float x)
{
    const float coef_const[7] = { 0, 76.18009173f, -86.50532033f, 24.01409822f, -1.231739516f, 0.00120858003f, -0.00000536382f };
    const float step = 2.50662827465f;
    float SER, temp, y;
    int j;

    x = x - 1;
    temp = x + 5.5f;
    temp = (x + 0.5f) * log(temp) - temp;
    SER = 1;
    for (j = 1; j <= 6; j++)
    {
        x = x + 1;
        SER = SER + coef_const[j] / x;
    }
    y = temp + log(step * SER);

    return y;
}
