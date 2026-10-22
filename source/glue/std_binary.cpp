#include "std_binary.hpp"

#include "../chali_glue.hpp"
#include <cstring>
#include <limits>
#include <type_traits>

/**
 * @brief CHALI 二进制流的字节序解码与边界检查。
 */
namespace std_binary_glue
{
using Bytes = Memory::Vector<std::uint8_t>;
using Context = chali_glue::Context;

/**
 * @brief 校验读取范围，先除后乘避免长度溢出。
 * @param ctx 调用上下文。
 * @param data 输入字节。
 * @param offset 偏移。
 * @param count 元素数。
 * @param width 元素字节数。
 * @return 范围是否合法。
 */
inline bool checkRange(Context &ctx, const Bytes &data, std::int64_t offset,
                       std::int64_t count, std::size_t width)
{
    if (offset < 0 || count < 0)
    {
        ctx.fail("二进制读取：偏移和长度必须非负");
        return false;
    }
    if (static_cast<std::uint64_t>(offset) > data.size() || (width != 0 && static_cast<std::uint64_t>(count) > (data.size() - static_cast<std::size_t>(offset)) / width))
    {
        ctx.fail("二进制读取：数据不足");
        return false;
    }
    return true;
}

/**
 * @brief 解码已校验长度的数值。
 * @param data 输入字节。
 * @param bigEndian 是否大端。
 * @return 解码值。
 */
template <class T>
T decode(const std::uint8_t *data, bool bigEndian)
{
    constexpr std::size_t width = std::is_same_v<T, bool> ? 1 : sizeof(T);
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < width; ++i)
        bits = (bits << 8) | data[bigEndian ? i : width - 1 - i];
    if constexpr (std::is_same_v<T, bool>)
        return bits != 0;
    else if constexpr (std::is_floating_point_v<T>)
    {
        static_assert(std::numeric_limits<T>::is_iec559 && (sizeof(T) == 4 || sizeof(T) == 8),
                      "IEEE 754 binary32/binary64 required");
        using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
        const Bits raw = static_cast<Bits>(bits);
        T value;
        std::memcpy(&value, &raw, sizeof(T));
        return value;
    }
    else if constexpr (std::is_signed_v<T>)
    {
        using U = std::make_unsigned_t<T>;
        const U raw = static_cast<U>(bits);
        return raw <= static_cast<U>(std::numeric_limits<T>::max()) ? static_cast<T>(raw) : static_cast<T>(-1 - static_cast<T>(static_cast<U>(~raw)));
    }
    else
        return static_cast<T>(bits);
}

/**
 * @brief 注册一种数值的标量与数组读取。
 * @param registry 胶水注册表。
 * @param name CHALI 类型名。
 * @param error 注册错误。
 * @return 是否成功。
 */
template <class T>
bool bindNumber(chali_glue::Registry &registry, const char *name, Memory::String *error)
{
    constexpr std::size_t width = std::is_same_v<T, bool> ? 1 : sizeof(T);
    const auto scalar = chali_glue::specialize("std::binary::__read", { name });
    const auto array = chali_glue::specialize("std::binary::__readArray", { name });
    return registry.bindTypedWithContext<T, Bytes, std::int64_t, bool>(scalar, [width](Context &ctx, Bytes data, std::int64_t offset, bool bigEndian) -> T {
            if (!checkRange(ctx, data, offset, 1, width)) return T{};
            if constexpr (std::is_same_v<T, bool>) {
                if (data[static_cast<std::size_t>(offset)] > 1) {
                    ctx.fail("二进制读取：bool 字节必须为 0 或 1"); return false;
                }
            }
            return decode<T>(data.data() + offset, bigEndian); }, error) && registry.bindTypedWithContext<Memory::Vector<T>, Bytes, std::int64_t, std::int64_t, bool>(array, [width](Context &ctx, Bytes data, std::int64_t offset, std::int64_t count, bool bigEndian) {
            Memory::Vector<T> values;
            if (!checkRange(ctx, data, offset, count, width)) return values;
            values.reserve(static_cast<std::size_t>(count));
            for (std::int64_t i = 0; i < count; ++i) {
                const auto at = static_cast<std::size_t>(offset) + static_cast<std::size_t>(i) * width;
                if constexpr (std::is_same_v<T, bool>) {
                    if (data[at] > 1) { ctx.fail("二进制读取：bool 字节必须为 0 或 1"); return Memory::Vector<T>{}; }
                }
                values.push_back(decode<T>(data.data() + at, bigEndian));
            }
            return values; }, error);
}

/**
 * @brief 读取定长文本字段，首个零字节之后为填充。
 * @param data 字节数组。
 * @param offset 起点。
 * @param length 字段宽度。
 * @return 文本。
 */
inline Memory::String textAt(const Bytes &data, std::size_t offset, std::size_t length)
{
    std::size_t size = 0;
    while (size < length && data[offset + size] != 0) ++size;
    return size == 0 ? Memory::String() :
                       Memory::String(reinterpret_cast<const char *>(data.data() + offset), size);
}

/**
 * @brief 注册二进制流标准库。
 * @param registry 胶水注册表。
 * @param error 注册错误。
 * @return 是否成功。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    bool ok = bindNumber<std::int8_t>(registry, "int8", error) && bindNumber<std::uint8_t>(registry, "uint8", error) && bindNumber<std::int16_t>(registry, "int16", error) && bindNumber<std::uint16_t>(registry, "uint16", error) && bindNumber<std::int32_t>(registry, "int32", error) && bindNumber<std::uint32_t>(registry, "uint32", error) && bindNumber<std::int64_t>(registry, "int64", error) && bindNumber<std::uint64_t>(registry, "uint64", error) && bindNumber<float>(registry, "float32", error) && bindNumber<double>(registry, "float64", error) && bindNumber<bool>(registry, "bool", error);
    return ok && registry.bindTypedWithContext<Memory::String, Bytes, std::int64_t, std::int64_t>("std::binary::__readString", [](Context &ctx, Bytes data, std::int64_t offset, std::int64_t length) {
            if (!checkRange(ctx, data, offset, length, 1)) return Memory::String();
            return textAt(data, static_cast<std::size_t>(offset), static_cast<std::size_t>(length)); }, error) && registry.bindTypedWithContext<Memory::Vector<Memory::String>, Bytes, std::int64_t, std::int64_t, std::int64_t>("std::binary::__readStrings", [](Context &ctx, Bytes data, std::int64_t offset, std::int64_t count, std::int64_t width) {
            Memory::Vector<Memory::String> values;
            if (width < 0) { ctx.fail("二进制读取：偏移和长度必须非负"); return values; }
            if (!checkRange(ctx, data, offset, count, static_cast<std::size_t>(width))) return values;
            values.reserve(static_cast<std::size_t>(count));
            for (std::int64_t i = 0; i < count; ++i)
                values.push_back(textAt(data, static_cast<std::size_t>(offset) +
                    static_cast<std::size_t>(i) * static_cast<std::size_t>(width), static_cast<std::size_t>(width)));
            return values; }, error) && registry.bindTypedWithContext<Bytes, chali_glue::Reference<std::uint8_t>, std::int64_t>("std::binary::__fromArray", [](Context &ctx, chali_glue::Reference<std::uint8_t> data, std::int64_t count) {
            Bytes values;
            if (count < 0 ||
                data.address < 0 || (count > 0 && data.address == 0) ||
                data.address > std::numeric_limits<std::int64_t>::max() - count) {
                ctx.fail("二进制读取：数组长度或地址无效"); return values;
            }
            for (std::int64_t i = 0; i < count; ++i) {
                std::uint8_t value = 0;
                if (!ctx.readBytes(data.address + i, reinterpret_cast<char *>(&value), 1)) {
                    ctx.fail("二进制读取：数组地址越界"); return Bytes{};
                }
                values.push_back(static_cast<std::uint8_t>(value));
            }
            return values; }, error);
}
}  // namespace std_binary_glue
