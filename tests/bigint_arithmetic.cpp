/**
 * @file bigint_arithmetic.cpp
 * @brief 供 Python 独立整数参考实现驱动的乘除法差分入口。
 */
#include "../source/bignum/bigint.hpp"

#include <iostream>
#include <limits>
#include <string>

/**
 * @brief 按输入指定算法求乘积、商和余数，并检查输入输出别名。
 * @return 全部运算及别名检查成功时返回 0，失败时返回非零值。
 */
int main()
{
    using bigint::BigInt;
    const std::size_t defaultMultiplication = BigInt::multiplicationThreshold();
    const std::size_t defaultDivision = BigInt::divisionThreshold();
    int mode = 0;
    std::string leftText, rightText;
    while (std::cin >> mode >> leftText >> rightText)
    {
        BigInt::setMultiplicationThreshold(mode == 1 ? 1u : mode == 2 ? std::numeric_limits<std::size_t>::max() :
                                                                        defaultMultiplication);
        BigInt::setDivisionThreshold(mode == 2 ? std::numeric_limits<std::size_t>::max() : mode == 1 ? 32u :
                                                                                                       defaultDivision);
        const BigInt left = BigInt::fromString(Memory::String(leftText.c_str()), 16);
        const BigInt right = BigInt::fromString(Memory::String(rightText.c_str()), 16);
        BigInt quotient, remainder;
        if (!left.divmod(right, &quotient, &remainder)) return 2;
        BigInt aliasLeft = left;
        BigInt aliasRight = right;
        if (!aliasLeft.divmod(aliasRight, &aliasLeft, &aliasRight)) return 3;
        if (aliasLeft != quotient || aliasRight != remainder) return 4;
        BigInt square = left;
        square *= square;
        if (square != left * left) return 5;
        std::cout << (left * right).toString(16).c_str() << ' '
                  << quotient.toString(16).c_str() << ' '
                  << remainder.toString(16).c_str() << '\n';
    }
    return 0;
}
