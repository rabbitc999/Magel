/**
 * @file symbol_table.hpp
 * @brief 稳定符号身份与变长编码协议。
 */
#pragma once
#include "vm_config.hpp"

namespace stackvm
{
/**
 * @brief 注解与反射能力位；普通格式的最低位保留给载荷加密。
 */
inline constexpr std::uint32_t kFeatureReflection = 0x0002u;
/**
 * @brief 程序使用真正的单字节 u8 内存读写，旧运行时必须拒绝该能力位。
 */
inline constexpr std::uint32_t kFeatureByteMemory = 0x0004u;
/**
 * @brief 已定义的程序能力位，未知位必须拒绝。
 */
inline constexpr std::uint32_t kKnownProgramFeatures = kFeatureReflection | kFeatureByteMemory;
/**
 * @brief 查询实际链接的运行时能力，避免只检查调用者的头文件配置。
 * @return 当前运行时支持的能力位。
 */
[[nodiscard]] std::uint32_t supportedProgramFeatures();
/**
 * @brief 验证程序能力要求；未启用反射的普通程序可在两种运行时执行。
 * @param required 程序要求的能力位。
 * @param diagnostics 可选诊断输出。
 * @return 全部能力已定义且当前运行时支持时返回 true。
 */
[[nodiscard]] bool validateProgramFeatures(std::uint32_t required,
                                           Memory::Vector<Memory::String> *diagnostics = nullptr);

/**
 * @brief 稳定符号编号与严格 ULEB128 编解码。
 */
namespace symbols
{
/**
 * @brief 变长整数的明确错误码。
 */
enum class Error
{
    Ok,
    Truncated,
    Overflow,
    Overlong,
    NonCanonical
};
/**
 * @brief 写最短 ULEB128。
 * @param out 输出。
 * @param value 数值。
 */
template <class Vector>
void writeVarUInt(Vector &out, std::uint64_t value)
{
    do
    {
        auto byte = static_cast<std::uint8_t>(value & 127u);
        value >>= 7u;
        out.push_back(static_cast<std::uint8_t>(byte | (value ? 128u : 0u)));
    } while (value);
}
/**
 * @brief 严格读取 ULEB128，失败时保持游标与输出。
 * @param data 输入。
 * @param size 输入长度。
 * @param pos 游标。
 * @param value 结果。
 * @return 错误码。
 */
inline Error readVarUInt(const std::uint8_t *data, std::size_t size,
                         std::size_t &pos, std::uint64_t &value)
{
    std::size_t cursor = pos;
    std::uint64_t decoded = 0;
    for (unsigned i = 0; i < 10; ++i)
    {
        if (cursor >= size || data == nullptr) return Error::Truncated;
        const auto byte = data[cursor++];
        if (i == 9 && (byte & 128u)) return Error::Overlong;
        if (i == 9 && (byte & 126u)) return Error::Overflow;
        decoded |= std::uint64_t(byte & 127u) << (7u * i);
        if (!(byte & 128u))
        {
            if (i && (byte & 127u) == 0) return Error::NonCanonical;
            pos = cursor;
            value = decoded;
            return Error::Ok;
        }
    }
    return Error::Overlong;
}
/**
 * @brief 计算 UTF-8 名字的 FNV-1a 64 稳定编号。
 * @param name 原始名字。
 * @return 编号，编译端须检测碰撞。
 */
template <class String>
std::uint64_t logicalId(const String &name)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : name)
    {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}
/**
 * @brief CRC32/IEEE。
 * @param data 数据。
 * @param size 长度。
 * @return 校验值。
 */
inline std::uint32_t crc32(const std::uint8_t *data, std::size_t size)
{
    std::uint32_t crc = ~0u;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (unsigned j = 0; j < 8; ++j) crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
/**
 * @brief 运行时不含原名的编号键。
 * @param id 编号。
 * @return 内部键。
 */
inline Memory::String key(std::uint64_t id) { return "#" + Memory::toString(id); }
}  // namespace symbols
/**
 * @brief 紧凑符号条目；original 仅在编译端映射表存在，加载器填编号键。
 */
struct SymbolRecord
{
    std::uint64_t logicalId = 0, physicalIndex = 0, offset = 0, size = 0, refCount = 0;
    std::uint8_t kind = 0, flags = 0;
    std::int32_t line = 0;
    Memory::String original;
};
}  // namespace stackvm
