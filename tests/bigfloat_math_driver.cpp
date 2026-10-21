#include "bigfloat_math.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

/**
 * @brief 通过逐行协议接受高精度数学测试向量并输出十进制结果。
 * @return 输入处理正常结束时返回零，协议字段不足时结束读取。
 */
int main()
{
    std::string operation, left, right;
    int digits;
    while (std::cin >> operation >> digits >> left >> right)
    {
        try
        {
            const bigfloat::BigFloat value(left.c_str()), other(right.c_str());
            bigfloat::BigFloat result;
            if (operation == "pi")
                result = bigfloat::pi(digits);
            else if (operation == "pow")
                result = bigfloat::pow(value, other, digits);
            else if (operation == "fmod")
                result = bigfloat::fmod(value, other);
#define MATH_CASE(name) else if (operation == #name) result = bigfloat::name(value, digits)
            MATH_CASE(sin);
            MATH_CASE(cos);
            MATH_CASE(tan);
            MATH_CASE(asin);
            MATH_CASE(acos);
            MATH_CASE(atan);
            MATH_CASE(exp);
            MATH_CASE(ln);
            MATH_CASE(log10);
#undef MATH_CASE
            else throw std::invalid_argument("未知测试操作");
            std::cout << result.toString(0).c_str() << '\n';
        } catch (const std::exception &error)
        {
            std::cout << "ERROR " << error.what() << '\n';
        }
    }
}
