#pragma once
#include "bigfloat.hpp"

namespace bigfloat
{
/**
 * @brief 计算圆周率，结果按指定有效位数舍入。
 * @param digits 有效十进制位数，范围为 1..100000。
 * @return 使用 AGM 计算的圆周率。
 */
BigFloat pi(int digits);
/**
 * @brief 计算自然指数，使用十进制级数和缩放平方。
 * @param value 实数指数。
 * @param digits 结果的有效十进制位数。
 * @return 舍入后的正实数；指数超出表示范围时抛出异常。
 */
BigFloat exp(const BigFloat &value, int digits);
/**
 * @brief 计算正实数的自然对数。
 * @param value 严格为正的输入。
 * @param digits 结果的有效十进制位数。
 * @return 舍入后的自然对数；非正输入抛出定义域异常。
 */
BigFloat ln(const BigFloat &value, int digits);
/**
 * @brief 计算正实数的常用对数。
 * @param value 严格为正的输入。
 * @param digits 结果的有效十进制位数。
 * @return 以 10 为底的对数。
 */
BigFloat log10(const BigFloat &value, int digits);
/**
 * @brief 计算弧度角的正弦，使用高精度参数约化。
 * @param value 有限实数弧度角。
 * @param digits 结果的有效十进制位数。
 * @return 舍入后的正弦值。
 */
BigFloat sin(const BigFloat &value, int digits);
/**
 * @brief 计算弧度角的余弦，使用高精度参数约化。
 * @param value 有限实数弧度角。
 * @param digits 结果的有效十进制位数。
 * @return 舍入后的余弦值。
 */
BigFloat cos(const BigFloat &value, int digits);
/**
 * @brief 计算弧度角的正切。
 * @param value 有限实数弧度角。
 * @param digits 结果的有效十进制位数。
 * @return 正弦与余弦的高精度商；检测到零余弦时抛出异常。
 */
BigFloat tan(const BigFloat &value, int digits);
/**
 * @brief 计算实数的反正切主值。
 * @param value 有限实数。
 * @param digits 结果的有效十进制位数。
 * @return 位于负二分之一圆周率与正二分之一圆周率之间的弧度角。
 */
BigFloat atan(const BigFloat &value, int digits);
/**
 * @brief 计算闭区间负一到一上的反正弦主值。
 * @param value 位于 [-1,1] 的实数。
 * @param digits 结果的有效十进制位数。
 * @return 反正弦弧度角；越界输入抛出定义域异常。
 */
BigFloat asin(const BigFloat &value, int digits);
/**
 * @brief 计算闭区间负一到一上的反余弦主值。
 * @param value 位于 [-1,1] 的实数。
 * @param digits 结果的有效十进制位数。
 * @return 位于零与圆周率之间的弧度角。
 */
BigFloat acos(const BigFloat &value, int digits);
/**
 * @brief 计算实数幂，整数指数保持负底数的实数语义。
 * @param value 底数；非整数指数要求底数为正。
 * @param exponent 实数指数。
 * @param digits 结果的有效十进制位数。
 * @return 舍入后的实数幂；未定义的实数运算抛出异常。
 */
BigFloat pow(const BigFloat &value, const BigFloat &exponent, int digits);
/**
 * @brief 精确计算向零整除对应的十进制余数。
 * @param value 被除数。
 * @param divisor 非零除数。
 * @return 与被除数同号且幅值小于除数的余数。
 */
BigFloat fmod(const BigFloat &value, const BigFloat &divisor);
}  // namespace bigfloat
