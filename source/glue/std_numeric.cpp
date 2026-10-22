#include "std_numeric.hpp"

#include "../chali_glue.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

/**
 * @brief lang 基础数值转换，不受可选标准库模块裁剪影响。
 */
namespace lang_numeric_glue
{
/**
 * @brief 先向零截断，再检查有符号整数范围。
 * @param ctx 调用上下文。
 * @param value 浮点值。
 * @param bound 目标正上界（不包含）。
 * @param message 错误说明。
 * @return 整数结果，失败时报告运行错误。
 */
template <class T>
T toInteger(chali_glue::Context &ctx, double value, double bound, const char *message)
{
    const double truncated = std::trunc(value);
    if (!std::isfinite(value) || truncated < -bound || truncated >= bound)
    {
        ctx.fail(message);
        return T{};
    }
    return static_cast<T>(truncated);
}

/**
 * @brief 按指定字节序编码 IEEE 754。
 * @param value 浮点值。
 * @param bigEndian 是否大端。
 * @return 固定长度字节列表。
 */
template <class T, class U>
Memory::Vector<std::uint8_t> floatBytes(T value, bool bigEndian)
{
    static_assert(sizeof(T) == sizeof(U) && std::numeric_limits<T>::is_iec559,
                  "IEEE 754 layout required");
    U bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    Memory::Vector<std::uint8_t> bytes(sizeof(T));
    for (std::size_t i = 0; i < sizeof(T); ++i)
        bytes[i] = static_cast<std::uint8_t>(bits >> ((bigEndian ? sizeof(T) - 1 - i : i) * 8));
    return bytes;
}

/**
 * @brief 解码已验证长度的 IEEE 754 字节。
 * @param bytes 输入地址。
 * @param bigEndian 是否大端。
 * @return 浮点值。
 */
template <class T, class U>
T bytesFloat(const std::uint8_t *bytes, bool bigEndian)
{
    U bits = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i)
        bits |= static_cast<U>(bytes[i]) << ((bigEndian ? sizeof(T) - 1 - i : i) * 8);
    T value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

/**
 * @brief 读取 VM 数组的连续字节，拒绝空指针、地址溢出及越界。
 * @param ctx 上下文。
 * @param address 数组地址。
 * @param bytes 接收字节的定长缓冲区。
 * @return 是否成功。
 */
template <std::size_t N>
bool readArrayBytes(chali_glue::Context &ctx, std::int64_t address, std::uint8_t (&bytes)[N])
{
    if (address <= 0 || address > std::numeric_limits<std::int64_t>::max() - static_cast<std::int64_t>(sizeof(bytes)) || !ctx.readBytes(address, reinterpret_cast<char *>(bytes), sizeof(bytes)))
    {
        ctx.fail("IEEE 754 转换：数组地址无效或越界");
        return false;
    }
    return true;
}

/**
 * @brief 注册一种浮点宽度的列表和数组双向转换。
 * @param registry 注册表。
 * @param suffix 位宽字符串。
 * @param error 注册错误。
 * @return 是否成功。
 */
template <class T, class U>
bool bindFloatBytes(chali_glue::Registry &registry, const char *suffix, Memory::String *error)
{
    using Bytes = Memory::Vector<std::uint8_t>;
    using Reference = chali_glue::Reference<std::uint8_t>;
    using Context = chali_glue::Context;
    const Memory::String number = suffix;
    return registry.bindTyped<Bytes, T, bool>("std::lang::__float" + number + "ToBytes", [](T value, bool bigEndian) { return floatBytes<T, U>(value, bigEndian); }, error) && registry.bindTypedWithContext<T, Bytes, bool>("std::lang::__bytesToFloat" + number, [](Context &ctx, Bytes bytes, bool bigEndian) {
            if (bytes.size() < sizeof(T)) { ctx.fail("IEEE 754 转换：字节不足"); return T{}; }
            return bytesFloat<T, U>(bytes.data(), bigEndian); }, error) && registry.bindTypedWithContext<T, Reference, bool>("std::lang::__arrayToFloat" + number, [](Context &ctx, Reference pointer, bool bigEndian) {
            std::uint8_t bytes[sizeof(T)]{};
            if (!readArrayBytes(ctx, pointer.address, bytes)) return T{};
            return bytesFloat<T, U>(bytes, bigEndian); }, error) && registry.bindTypedWithContext<bool, T, Reference, bool>("std::lang::__float" + number + "ToArray", [](Context &ctx, T value, Reference pointer, bool bigEndian) {
            std::uint8_t previous[sizeof(T)]{};
            if (!readArrayBytes(ctx, pointer.address, previous)) return false;
            const auto bytes = floatBytes<T, U>(value, bigEndian);
            if (!ctx.writeBytes(pointer.address, reinterpret_cast<const char *>(bytes.data()), bytes.size())) {
                ctx.fail("IEEE 754 转换：无法写入数组"); return false;
            }
            return true; }, error);
}

/**
 * @brief 注册核心数值和 IEEE 754 字节转换。
 * @param registry 注册表。
 * @param error 注册错误。
 * @return 是否成功。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    // int64 中的 2^31 作为失败哨兵，与所有有效 int32 结果严格区分。
    return registry.bindTyped<std::int64_t, double>("std::lang::__tryToInt32", [](double value) -> std::int64_t {
            const double truncated = std::trunc(value);
            if (!std::isfinite(value) || truncated < -2147483648.0 || truncated >= 2147483648.0)
                return INT64_C(2147483648);
            return static_cast<std::int64_t>(truncated); }, error) && registry.bindTypedWithContext<std::int64_t, double>("std::lang::__toInt64", [](chali_glue::Context &ctx, double value) { return toInteger<std::int64_t>(ctx, value, 9223372036854775808.0,
                                                                                                                                                                                                                                                                                       "std::lang::toInt64：浮点值不是有限数或超出 int64 范围"); }, error) && bindFloatBytes<float, std::uint32_t>(registry, "32", error) && bindFloatBytes<double, std::uint64_t>(registry, "64", error);
}
}  // namespace lang_numeric_glue
