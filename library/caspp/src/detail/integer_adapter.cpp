#include "cas/detail/integer_adapter.hpp"
#include <limits>
#include <stdexcept>

namespace Cas::Detail
{
/**
 * @brief 构造值为零的共享后端整数。
 */
BigInt::BigInt() {}
/**
 * @brief 从有符号整数创建共享后端值。
 * @param value 需要精确保存的整数。
 */
BigInt::BigInt(long long value) : m_value(static_cast<std::int64_t>(value)) {}
/**
 * @brief 从宿主 int 精确构造整数。
 * @param value 需要精确保存的整数。
 */
BigInt::BigInt(int value) : BigInt(static_cast<long long>(value)) {}
/**
 * @brief 解析指定进制整数，非法文本抛出异常。
 * @param text 以零结尾的数字文本，不得为空。
 * @param base 数字进制，范围为 2..36。
 */
BigInt::BigInt(const char *text, int base) : BigInt(std::string(text ? text : ""), base) {}
/**
 * @brief 通过共享大整数解析器读取指定进制文本。
 * @param text 待解析的整数文本。
 * @param base 数字进制，范围为 2..36。
 */
BigInt::BigInt(const std::string &text, int base)
{
    bool ok = false;
    m_value = bigint::BigInt::fromString(::Memory::String(text.c_str()), static_cast<unsigned int>(base), &ok);
    if (!ok) throw std::invalid_argument("CAS 整数文本无效");
}
/**
 * @brief 保存现有大整数的值副本，不转换为浮点。
 * @param value 共享数值后端中的整数。
 */
BigInt::BigInt(const bigint::BigInt &value) : m_value(value) {}
/**
 * @brief 判断幅值是否为零。
 * @return 值为零时返回 true。
 */
bool BigInt::isZero() const { return m_value.isZero(); }
/**
 * @brief 判断整数是否为负数。
 * @return 值小于零时返回 true。
 */
bool BigInt::isNegative() const { return m_value.isNegative(); }
/**
 * @brief 判断整数是否为精确的正一。
 * @return 值恰好为正一时返回 true。
 */
bool BigInt::isOne() const { return m_value == bigint::BigInt(1); }
/**
 * @brief 检查整数幅值最低位。
 * @return 整数不能被二整除时返回 true。
 */
bool BigInt::isOdd() const { return m_value.isOdd(); }
/**
 * @brief 返回整数的符号。
 * @return 负数返回 -1，零返回 0，正数返回 1。
 */
int BigInt::sign() const { return m_value.sign(); }
/**
 * @brief 取得幅值的有效二进制位数。
 * @return 非零幅值的有效二进制位数，零返回零。
 */
std::size_t BigInt::bitLength() const { return m_value.bitLength(); }
/**
 * @brief 取得共享后端的 32 位块数。
 * @return 共享后端幅值所占的 32 位块数量。
 */
std::size_t BigInt::limbCount() const { return m_value.limbCount(); }
/**
 * @brief 显式转换为可能丢失精度的近似浮点值。
 * @return 转换为 double 的近似值，可能发生舍入或溢出。
 */
double BigInt::toDouble() const { return m_value.toDouble(); }
/**
 * @brief 从共享后端幅值恢复符号并按补码截断，供 CAS 小整数规则使用。
 * @return 原值的低 64 位补码；可表示的负数保留负号。
 */
long long BigInt::toLongLong() const
{
    const std::uint64_t magnitude = m_value.toU64();
    return static_cast<long long>(m_value.isNegative() ? std::uint64_t(0) - magnitude : magnitude);
}
/**
 * @brief 取得非负幅值的独立数值。
 * @return 保留原幅值且符号非负的整数。
 */
BigInt BigInt::abs() const { return m_value.abs(); }
/**
 * @brief 取得精确相反数。
 * @return 与原值相加为零的整数。
 */
BigInt BigInt::negated() const { return -m_value; }
/**
 * @brief 执行精确一元取负。
 * @return 原整数的精确相反数。
 */
BigInt BigInt::operator-() const { return -m_value; }
/**
 * @brief 通过共享后端计算非负整数平方根。
 * @return 不大于精确平方根的非负整数，负输入抛出异常。
 */
BigInt BigInt::isqrt() const { return bigint::integerSqrt(m_value); }
/**
 * @brief 借用共享大整数后端以供精确大浮点转换。
 * @return 与适配对象寿命相同的只读后端引用。
 */
const bigint::BigInt &BigInt::backend() const { return m_value; }
/**
 * @brief 输出精确十进制文本。
 * @return 不丢失任何有效数字的整数文本。
 */
std::string BigInt::toString() const
{
    const auto text = m_value.toString();
    return std::string(text.data(), text.size());
}
/**
 * @brief 输出精确十六进制文本。
 * @return 可按十六进制重新解析的整数文本。
 */
std::string BigInt::toStringHex() const
{
    const auto text = m_value.toString(16);
    return std::string(text.data(), text.size());
}
/**
 * @brief 检查能否无损转换为 long long。
 * @return 值处于有符号 64 位整数范围时为 true。
 */
bool BigInt::fitsLongLong() const { return m_value >= bigint::BigInt(std::numeric_limits<std::int64_t>::min()) && m_value <= bigint::BigInt(std::numeric_limits<std::int64_t>::max()); }
/**
 * @brief 通过共享后端位接口读取幅值块，供结构哈希使用。
 * @param index 从零开始的 32 位块序号。
 * @return 对应块；越界时为零。
 */
std::uint32_t BigInt::limbAt(std::size_t index) const
{
    if (index >= m_value.limbCount()) return 0u;
    std::uint32_t value = 0u;
    for (std::size_t bit = 0u; bit < 32u; ++bit)
        if (m_value.bitAt(index * 32u + bit)) value |= std::uint32_t(1u) << bit;
    return value;
}
/**
 * @brief 通过共享后端精确相加。
 * @param other 右操作数。
 * @return 运算后的独立整数。
 */
BigInt BigInt::operator+(const BigInt &other) const { return m_value + other.m_value; }
/**
 * @brief 通过共享后端精确相减。
 * @param other 右操作数。
 * @return 运算后的独立整数。
 */
BigInt BigInt::operator-(const BigInt &other) const { return m_value - other.m_value; }
/**
 * @brief 通过共享后端精确相乘。
 * @param other 右操作数。
 * @return 运算后的独立整数。
 */
BigInt BigInt::operator*(const BigInt &other) const { return m_value * other.m_value; }
/**
 * @brief 使用 CAS 显式向下整除约定计算商。
 * @param other 非零除数。
 * @return 商向下取整，余数跟随除数符号。
 */
BigInt BigInt::operator/(const BigInt &other) const
{
    BigInt quotient;
    divMod(*this, other, &quotient, nullptr);
    return quotient;
}
/**
 * @brief 使用 CAS 显式向下整除约定计算余数。
 * @param other 非零除数。
 * @return 商向下取整，余数跟随除数符号。
 */
BigInt BigInt::operator%(const BigInt &other) const
{
    BigInt remainder;
    divMod(*this, other, nullptr, &remainder);
    return remainder;
}
/**
 * @brief 原地保存精确算术结果并支持自别名。
 * @param other 参与运算的右值。
 * @return 更新后的当前整数引用。
 */
BigInt &BigInt::operator+=(const BigInt &other)
{
    *this = *this + other;
    return *this;
}
/**
 * @brief 原地保存精确算术结果并支持自别名。
 * @param other 参与运算的右值。
 * @return 更新后的当前整数引用。
 */
BigInt &BigInt::operator-=(const BigInt &other)
{
    *this = *this - other;
    return *this;
}
/**
 * @brief 原地保存精确算术结果并支持自别名。
 * @param other 参与运算的右值。
 * @return 更新后的当前整数引用。
 */
BigInt &BigInt::operator*=(const BigInt &other)
{
    *this = *this * other;
    return *this;
}
/**
 * @brief 原地保存精确算术结果并支持自别名。
 * @param other 参与运算的右值。
 * @return 更新后的当前整数引用。
 */
BigInt &BigInt::operator/=(const BigInt &other)
{
    *this = *this / other;
    return *this;
}
/**
 * @brief 原地保存精确算术结果并支持自别名。
 * @param other 参与运算的右值。
 * @return 更新后的当前整数引用。
 */
BigInt &BigInt::operator%=(const BigInt &other)
{
    *this = *this % other;
    return *this;
}
/**
 * @brief 通过共享后端执行幅值位移。
 * @param bits 非负移位位数。
 * @return 移位后的整数。
 */
BigInt BigInt::operator<<(int bits) const
{
    if (bits < 0) throw std::invalid_argument("CAS 位移量必须非负");
    bigint::BigInt value = m_value;
    value.shiftLeft(static_cast<std::size_t>(bits));
    return value;
}
/**
 * @brief 通过共享后端执行幅值位移。
 * @param bits 非负移位位数。
 * @return 移位后的整数。
 */
BigInt BigInt::operator>>(int bits) const
{
    if (bits < 0) throw std::invalid_argument("CAS 位移量必须非负");
    bigint::BigInt value = m_value;
    value.shiftRight(static_cast<std::size_t>(bits));
    return value;
}
/**
 * @brief 比较两个精确整数的大小。
 * @param other 被比较的右操作数。
 * @return 小于、相等或大于分别返回 -1、0、1。
 */
int BigInt::compare(const BigInt &other) const { return m_value.compare(other.m_value); }
/**
 * @brief 按共享整数值进行关系比较。
 * @param other 比较的右操作数。
 * @return 关系成立时为 true。
 */
bool BigInt::operator==(const BigInt &other) const { return m_value == other.m_value; }
/**
 * @brief 按共享整数值进行关系比较。
 * @param other 比较的右操作数。
 * @return 关系成立时为 true。
 */
bool BigInt::operator!=(const BigInt &other) const { return m_value != other.m_value; }
/**
 * @brief 按共享整数值进行关系比较。
 * @param other 比较的右操作数。
 * @return 关系成立时为 true。
 */
bool BigInt::operator<(const BigInt &other) const { return m_value < other.m_value; }
/**
 * @brief 按共享整数值进行关系比较。
 * @param other 比较的右操作数。
 * @return 关系成立时为 true。
 */
bool BigInt::operator<=(const BigInt &other) const { return m_value <= other.m_value; }
/**
 * @brief 按共享整数值进行关系比较。
 * @param other 比较的右操作数。
 * @return 关系成立时为 true。
 */
bool BigInt::operator>(const BigInt &other) const { return m_value > other.m_value; }
/**
 * @brief 按共享整数值进行关系比较。
 * @param other 比较的右操作数。
 * @return 关系成立时为 true。
 */
bool BigInt::operator>=(const BigInt &other) const { return m_value >= other.m_value; }
/**
 * @brief 在共享除法结果上显式调整为 CAS 所需的 floor 语义。
 * @param a 被除数。
 * @param b 非零除数。
 * @param quotient 非空时接收向下取整的商。
 * @param remainder 非空时接收与除数同号的余数。
 */
void BigInt::divMod(const BigInt &a, const BigInt &b, BigInt *quotient, BigInt *remainder)
{
    bigint::BigInt q, r;
    if (!a.m_value.divmod(b.m_value, &q, &r)) throw std::domain_error("CAS 除数不能为零");
    if (!r.isZero() && a.sign() != b.sign())
    {
        q -= bigint::BigInt(1);
        r += b.m_value;
    }
    if (quotient) *quotient = BigInt(q);
    if (remainder) *remainder = BigInt(r);
}
/**
 * @brief 通过共享后端取得非负模。
 * @param modulus 非零模数。
 * @return 位于零到模数幅值之间的余数。
 */
BigInt BigInt::mod(const BigInt &modulus) const { return m_value.modNonNegative(modulus.m_value); }
/**
 * @brief 通过共享后端求模逆并保留 CAS 无逆时返回零的约定。
 * @param modulus 非零模数。
 * @return 规范化模逆；不存在时为零。
 */
BigInt BigInt::modInverse(const BigInt &modulus) const
{
    if (bigint::BigInt::gcd(m_value, modulus.m_value) != bigint::BigInt(1)) return BigInt();
    return bigint::BigInt::modInverse(m_value, modulus.m_value);
}
/**
 * @brief 通过共享快速幂计算精确整数乘方。
 * @param base 底数。
 * @param exponent 非负整数指数。
 * @return 精确整数幂。
 */
BigInt BigInt::power(const BigInt &base, unsigned long long exponent) { return base.m_value.pow(exponent); }
/**
 * @brief 通过共享后端计算非负最大公约数。
 * @param a 第一个整数。
 * @param b 第二个整数。
 * @return 两个幅值的最大公约数。
 */
BigInt BigInt::gcd(const BigInt &a, const BigInt &b) { return bigint::BigInt::gcd(a.m_value, b.m_value); }
/**
 * @brief 通过共享后端计算非负整数的下取整 n 次根。
 * @param n 正整数根次。
 * @return 下取整整数根。
 */
BigInt BigInt::nthRoot(unsigned n) const { return bigint::integerRoot(m_value, n); }
/**
 * @brief 使用共享整数根判断精确完全平方数。
 * @return 非负且平方根的平方等于原值时为 true。
 */
bool BigInt::isPerfectSquare() const
{
    if (isNegative()) return false;
    const BigInt root = isqrt();
    return root * root == *this;
}
/**
 * @brief 使用共享整数根判断非负整数的完全幂。
 * @param n 正整数根次。
 * @return 整数根的 n 次幂精确等于原值时为 true。
 */
bool BigInt::isPerfectPower(unsigned n) const
{
    if (isNegative() || n == 0u) return false;
    return power(nthRoot(n), n) == *this;
}
}  // namespace Cas::Detail
