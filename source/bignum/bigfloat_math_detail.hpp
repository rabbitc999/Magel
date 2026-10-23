#pragma once
#include "bigfloat.hpp"

namespace bigfloat::mathDetail
{
/**
 * @brief 验证公开精度并为内部迭代增加保护位。
 * @param digits 请求的有效位数。
 * @param extra 约化或消去需要的额外位数。
 * @return 已检查资源预算的内部有效位数。
 */
int workPrecision(int digits, std::int64_t extra = 0);
/**
 * @brief 取得非零幅值最高位对应的十进制阶数。
 * @param value 待检查的十进制数。
 * @return 最高十进制位的指数；零返回 0。
 */
std::int64_t decimalOrder(const BigFloat &value);
}  // namespace bigfloat::mathDetail
