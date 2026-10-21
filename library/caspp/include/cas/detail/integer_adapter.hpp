#pragma once
#include "bigint.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace Cas::Detail
{
/**
 * @brief caspp 的数值接口适配器，全部存储及算法由共享 bigint 后端提供。
 * @note 不包含独立的乘除法、NTT、内存块或数值解析实现。
 */
class BigInt
{
public:
    /**
     * @brief 构造值为零的共享后端整数。
     */
    BigInt();
    /**
     * @brief 从有符号整数创建共享后端值。
     * @param value 需要精确保存的整数。
     */
    BigInt(long long value);
    /**
     * @brief 从宿主 int 精确构造整数。
     * @param value 需要精确保存的整数。
     */
    BigInt(int value);
    /**
     * @brief 解析指定进制整数，非法文本抛出异常。
     * @param text 以零结尾的数字文本，不得为空。
     * @param base 数字进制，范围为 2..36。
     */
    BigInt(const char *text, int base = 10);
    /**
     * @brief 通过共享大整数解析器读取指定进制文本。
     * @param text 待解析的整数文本。
     * @param base 数字进制，范围为 2..36。
     */
    BigInt(const std::string &text, int base = 10);
    /**
     * @brief 保存现有大整数的值副本，不转换为浮点。
     * @param value 共享数值后端中的整数。
     */
    BigInt(const bigint::BigInt &value);
    /**
     * @brief 判断幅值是否为零。
     * @return 值为零时返回 true。
     */
    bool isZero() const;
    /**
     * @brief 判断整数是否为负数。
     * @return 值小于零时返回 true。
     */
    bool isNegative() const;
    /**
     * @brief 判断整数是否为精确的正一。
     * @return 值恰好为正一时返回 true。
     */
    bool isOne() const;
    /**
     * @brief 检查整数幅值最低位。
     * @return 整数不能被二整除时返回 true。
     */
    bool isOdd() const;
    /**
     * @brief 返回整数的符号。
     * @return 负数返回 -1，零返回 0，正数返回 1。
     */
    int sign() const;
    /**
     * @brief 取得幅值的有效二进制位数。
     * @return 非零幅值的有效二进制位数，零返回零。
     */
    std::size_t bitLength() const;
    /**
     * @brief 取得共享后端的 32 位块数。
     * @return 共享后端幅值所占的 32 位块数量。
     */
    std::size_t limbCount() const;
    /**
     * @brief 显式转换为可能丢失精度的近似浮点值。
     * @return 转换为 double 的近似值，可能发生舍入或溢出。
     */
    double toDouble() const;
    /**
     * @brief 从共享后端幅值恢复符号并按补码截断，供 CAS 小整数规则使用。
     * @return 原值的低 64 位补码；可表示的负数保留负号。
     */
    long long toLongLong() const;
    /**
     * @brief 取得非负幅值的独立数值。
     * @return 保留原幅值且符号非负的整数。
     */
    BigInt abs() const;
    /**
     * @brief 取得精确相反数。
     * @return 与原值相加为零的整数。
     */
    BigInt negated() const;
    /**
     * @brief 执行精确一元取负。
     * @return 原整数的精确相反数。
     */
    BigInt operator-() const;
    /**
     * @brief 通过共享后端计算非负整数平方根。
     * @return 不大于精确平方根的非负整数，负输入抛出异常。
     */
    BigInt isqrt() const;
    /**
     * @brief 借用共享大整数后端以供精确大浮点转换。
     * @return 与适配对象寿命相同的只读后端引用。
     */
    const bigint::BigInt &backend() const;
    /**
     * @brief 输出精确十进制文本。
     * @return 不丢失任何有效数字的整数文本。
     */
    std::string toString() const;
    /**
     * @brief 输出精确十六进制文本。
     * @return 可按十六进制重新解析的整数文本。
     */
    std::string toStringHex() const;
    /**
     * @brief 检查能否无损转换为 long long。
     * @return 值处于有符号 64 位整数范围时为 true。
     */
    bool fitsLongLong() const;
    /**
     * @brief 通过共享后端位接口读取幅值块，供结构哈希使用。
     * @param index 从零开始的 32 位块序号。
     * @return 对应块；越界时为零。
     */
    std::uint32_t limbAt(std::size_t index) const;
    /**
     * @brief 通过共享后端精确相加。
     * @param other 右操作数。
     * @return 运算后的独立整数。
     */
    BigInt operator+(const BigInt &other) const;
    /**
     * @brief 通过共享后端精确相减。
     * @param other 右操作数。
     * @return 运算后的独立整数。
     */
    BigInt operator-(const BigInt &other) const;
    /**
     * @brief 通过共享后端精确相乘。
     * @param other 右操作数。
     * @return 运算后的独立整数。
     */
    BigInt operator*(const BigInt &other) const;
    /**
     * @brief 使用 CAS 显式向下整除约定计算商。
     * @param other 非零除数。
     * @return 商向下取整，余数跟随除数符号。
     */
    BigInt operator/(const BigInt &other) const;
    /**
     * @brief 使用 CAS 显式向下整除约定计算余数。
     * @param other 非零除数。
     * @return 商向下取整，余数跟随除数符号。
     */
    BigInt operator%(const BigInt &other) const;
    /**
     * @brief 原地保存精确算术结果并支持自别名。
     * @param other 参与运算的右值。
     * @return 更新后的当前整数引用。
     */
    BigInt &operator+=(const BigInt &other);
    /**
     * @brief 原地保存精确算术结果并支持自别名。
     * @param other 参与运算的右值。
     * @return 更新后的当前整数引用。
     */
    BigInt &operator-=(const BigInt &other);
    /**
     * @brief 原地保存精确算术结果并支持自别名。
     * @param other 参与运算的右值。
     * @return 更新后的当前整数引用。
     */
    BigInt &operator*=(const BigInt &other);
    /**
     * @brief 原地保存精确算术结果并支持自别名。
     * @param other 参与运算的右值。
     * @return 更新后的当前整数引用。
     */
    BigInt &operator/=(const BigInt &other);
    /**
     * @brief 原地保存精确算术结果并支持自别名。
     * @param other 参与运算的右值。
     * @return 更新后的当前整数引用。
     */
    BigInt &operator%=(const BigInt &other);
    /**
     * @brief 通过共享后端执行幅值位移。
     * @param bits 非负移位位数。
     * @return 移位后的整数。
     */
    BigInt operator<<(int bits) const;
    /**
     * @brief 通过共享后端执行幅值位移。
     * @param bits 非负移位位数。
     * @return 移位后的整数。
     */
    BigInt operator>>(int bits) const;
    /**
     * @brief 比较两个精确整数的大小。
     * @param other 被比较的右操作数。
     * @return 小于、相等或大于分别返回 -1、0、1。
     */
    int compare(const BigInt &other) const;
    /**
     * @brief 按共享整数值进行关系比较。
     * @param other 比较的右操作数。
     * @return 关系成立时为 true。
     */
    bool operator==(const BigInt &other) const;
    /**
     * @brief 按共享整数值进行关系比较。
     * @param other 比较的右操作数。
     * @return 关系成立时为 true。
     */
    bool operator!=(const BigInt &other) const;
    /**
     * @brief 按共享整数值进行关系比较。
     * @param other 比较的右操作数。
     * @return 关系成立时为 true。
     */
    bool operator<(const BigInt &other) const;
    /**
     * @brief 按共享整数值进行关系比较。
     * @param other 比较的右操作数。
     * @return 关系成立时为 true。
     */
    bool operator<=(const BigInt &other) const;
    /**
     * @brief 按共享整数值进行关系比较。
     * @param other 比较的右操作数。
     * @return 关系成立时为 true。
     */
    bool operator>(const BigInt &other) const;
    /**
     * @brief 按共享整数值进行关系比较。
     * @param other 比较的右操作数。
     * @return 关系成立时为 true。
     */
    bool operator>=(const BigInt &other) const;
    /**
     * @brief 在共享除法结果上显式调整为 CAS 所需的 floor 语义。
     * @param a 被除数。
     * @param b 非零除数。
     * @param quotient 非空时接收向下取整的商。
     * @param remainder 非空时接收与除数同号的余数。
     */
    static void divMod(const BigInt &a, const BigInt &b, BigInt *quotient, BigInt *remainder);
    /**
     * @brief 通过共享后端取得非负模。
     * @param modulus 非零模数。
     * @return 位于零到模数幅值之间的余数。
     */
    BigInt mod(const BigInt &modulus) const;
    /**
     * @brief 通过共享后端求模逆并保留 CAS 无逆时返回零的约定。
     * @param modulus 非零模数。
     * @return 规范化模逆；不存在时为零。
     */
    BigInt modInverse(const BigInt &modulus) const;
    /**
     * @brief 通过共享快速幂计算精确整数乘方。
     * @param base 底数。
     * @param exponent 非负整数指数。
     * @return 精确整数幂。
     */
    static BigInt power(const BigInt &base, unsigned long long exponent);
    /**
     * @brief 通过共享后端计算非负最大公约数。
     * @param a 第一个整数。
     * @param b 第二个整数。
     * @return 两个幅值的最大公约数。
     */
    static BigInt gcd(const BigInt &a, const BigInt &b);
    /**
     * @brief 通过共享后端计算非负整数的下取整 n 次根。
     * @param n 正整数根次。
     * @return 下取整整数根。
     */
    BigInt nthRoot(unsigned n) const;
    /**
     * @brief 使用共享整数根判断精确完全平方数。
     * @return 非负且平方根的平方等于原值时为 true。
     */
    bool isPerfectSquare() const;
    /**
     * @brief 使用共享整数根判断非负整数的完全幂。
     * @param n 正整数根次。
     * @return 整数根的 n 次幂精确等于原值时为 true。
     */
    bool isPerfectPower(unsigned n) const;

private:
    bigint::BigInt m_value;
};
}  // namespace Cas::Detail
