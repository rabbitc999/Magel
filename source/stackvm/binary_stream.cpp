/**
 * @file binary_stream.cpp
 * @brief 有界小端二进制读写、校验与宿主文件访问。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 将二进制解析错误加入诊断列表。
 * @param diagnostics 接收处理诊断的列表。
 * @param message 待报告或输出的信息文本。
 */
void binaryDiagnostic(Memory::Vector<Memory::String> *diagnostics, const Memory::String &message)
{
    if (diagnostics != nullptr) diagnostics->push_back(message);
}

/**
 * @brief 计算二进制中间产物的完整性校验值。
 * @param data 输入或待写入的字节数据。
 * @param size 输入数据或目标区域的大小。
 * @retval 二进制数据的完整性校验值。
 */
std::uint32_t binaryChecksum(const std::uint8_t *data, std::size_t size)
{
    std::uint32_t hash = 2166136261u;
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= data[index];
        hash *= 16777619u;
    }
    return hash;
}

/**
 * @brief 将二进制文件内容读取到字节数组。
 * @param path 目标文件或目录的路径。
 * @param bytes 内存区域或数据的字节数。
 * @param diagnostics 接收处理诊断的列表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool readBinaryFile(const Memory::String &path, Memory::Vector<std::uint8_t> *bytes,
                    Memory::Vector<Memory::String> *diagnostics)
{
#if STACKVM_WITH_HOST_IO

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        binaryDiagnostic(diagnostics, "打不开二进制程序：" + path);
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    if (length < 0 || static_cast<std::uint64_t>(length) > kMaximumBinarySize)
    {
        binaryDiagnostic(diagnostics, "二进制程序过大（上限 512 MiB）：" + path);
        return false;
    }
    stream.seekg(0, std::ios::beg);
    bytes->assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (!stream.eof() && stream.fail())
    {
        binaryDiagnostic(diagnostics, "读取二进制程序失败：" + path);
        return false;
    }
    return true;

#else
    return false;
#endif
}

/**
 * @brief 读写二进制中间产物的符号字段。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::symbol(const Memory::String &value)

{
    if (!symbolIndices)
    {
        text(value);
        return;
    }
    const auto found = symbolIndices->find(value);
    if (found == symbolIndices->end())
    {
        error = "未登记的符号";
        return;
    }
    varUInt(found->second);
}

/**
 * @brief 读写二进制中间产物的数值字段。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::number(std::uint64_t value)

{
    if (symbolIndices)
        varUInt(value);
    else
        u64(value);
}

/**
 * @brief 追加未经编码的原始字节。
 * @param data 输入或待写入的字节数据。
 * @param size 输入数据或目标区域的大小。
 */
void BinaryWriter::raw(const std::uint8_t *data, std::size_t size)

{
    if (size) bytes.insert(bytes.end(), data, data + size);
}

/**
 * @brief 读写一个 8 位无符号整数。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::u8(std::uint8_t value)
{ bytes.push_back(value); }

/**
 * @brief 读写一个 16 位无符号整数。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::u16(std::uint16_t value)

{
    for (unsigned shift = 0u; shift < 16u; shift += 8u)
        u8(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

/**
 * @brief 读写一个 32 位无符号整数。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::u32(std::uint32_t value)

{
    for (unsigned shift = 0u; shift < 32u; shift += 8u)
        u8(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

/**
 * @brief 读写一个 64 位无符号整数。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::u64(std::uint64_t value)

{
    for (unsigned shift = 0u; shift < 64u; shift += 8u)
        u8(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

/**
 * @brief 读取变长编码的无符号整数。
 *
 * ULEB128：小整数只占一个字节。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::varUInt(std::uint64_t value)

{
    do
    {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7fu);
        value >>= 7u;
        if (value != 0u) byte |= 0x80u;
        u8(byte);
    } while (value != 0u);
}

/**
 * @brief 读取变长编码的有符号整数。
 *
 * ZigZag + ULEB128：正负小整数都紧凑。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::varInt(std::int64_t value)

{
    const std::uint64_t zigzag = (static_cast<std::uint64_t>(value) << 1u) ^ (0u - static_cast<std::uint64_t>(value < 0));
    varUInt(zigzag);
}

/**
 * @brief 读写带变长长度前缀的文本。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::varText(const Memory::String &value)

{
    if (value.size() > kMaximumStringSize)
    {
        error = "字符串超过二进制格式的 64 MiB 上限";
        return;
    }
    varUInt(value.size());
    raw(reinterpret_cast<const std::uint8_t *>(value.data()), value.size());
}

/**
 * @brief 取得文本字段的字符串内容。
 * @param value 待转换、保存或参与计算的值。
 */
void BinaryWriter::text(const Memory::String &value)

{
    if (value.size() > kMaximumStringSize)
    {
        error = "字符串超过二进制格式的 64 MiB 上限";
        return;
    }
    u32(static_cast<std::uint32_t>(value.size()));
    raw(reinterpret_cast<const std::uint8_t *>(value.data()), value.size());
}

/**
 * @brief 读写二进制记录的条目数量。
 * @param value 待转换、保存或参与计算的值。
 * @param what 测试条件的说明文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryWriter::count(std::size_t value, const char *what)

{
    if (value > kMaximumItemCount)
    {
        error = Memory::String(what) + "超过二进制格式的数量上限";
        return false;
    }
    if (symbolIndices)
        varUInt(value);
    else
        u32(static_cast<std::uint32_t>(value));
    return true;
}

/**
 * @brief 读写二进制中间产物的符号字段。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::symbol(Memory::String *out)

{
    if (!symbolRecords) return text(out);
    std::uint64_t index = 0;
    if (!varUInt(&index)) return false;
    if (index >= symbolRecords->size())
    {
        error = "符号物理索引越界";
        return false;
    }
    *out = (*symbolRecords)[static_cast<std::size_t>(index)].original;
    return true;
}

/**
 * @brief 读写二进制中间产物的数值字段。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::number(std::uint64_t *out)
{ return symbolRecords ? varUInt(out) : u64(out); }

/**
 * @brief 初始化二进制中间产物读取器。
 * @param input 程序输入或待解码的字节流。
 */
BinaryReader::BinaryReader(const Memory::Vector<std::uint8_t> &input) : bytes(input)
{
}

/**
 * @brief 检查并消耗指定长度的二进制数据。
 * @param size 输入数据或目标区域的大小。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::take(std::size_t size, const std::uint8_t **out)

{
    if (size > bytes.size() - std::min(position, bytes.size()))
    {
        error = "二进制程序被截断";
        return false;
    }
    *out = bytes.data() + position;
    position += size;
    return true;
}

/**
 * @brief 读写一个 8 位无符号整数。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::u8(std::uint8_t *out)

{
    const std::uint8_t *data = nullptr;
    if (!take(1u, &data)) return false;
    *out = data[0];
    return true;
}

/**
 * @brief 读写一个 16 位无符号整数。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::u16(std::uint16_t *out)

{
    const std::uint8_t *data = nullptr;
    if (!take(2u, &data)) return false;
    *out = static_cast<std::uint16_t>(data[0]) | static_cast<std::uint16_t>(data[1] << 8u);
    return true;
}

/**
 * @brief 读写一个 32 位无符号整数。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::u32(std::uint32_t *out)

{
    const std::uint8_t *data = nullptr;
    if (!take(4u, &data)) return false;
    *out = 0u;
    for (unsigned index = 0u; index < 4u; ++index)
        *out |= static_cast<std::uint32_t>(data[index]) << (index * 8u);
    return true;
}

/**
 * @brief 读写一个 64 位无符号整数。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::u64(std::uint64_t *out)

{
    const std::uint8_t *data = nullptr;
    if (!take(8u, &data)) return false;
    *out = 0u;
    for (unsigned index = 0u; index < 8u; ++index)
        *out |= static_cast<std::uint64_t>(data[index]) << (index * 8u);
    return true;
}

/**
 * @brief 读取变长编码的无符号整数。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::varUInt(std::uint64_t *out)

{
    const auto result = symbols::readVarUInt(bytes.data(), bytes.size(), position, *out);
    if (result == symbols::Error::Ok) return true;
    error = "非法变长整数，错误码=" + Memory::toString(static_cast<unsigned>(result));
    return false;
}

/**
 * @brief 读取变长编码的有符号整数。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::varInt(std::int64_t *out)

{
    std::uint64_t zigzag = 0u;
    if (!varUInt(&zigzag)) return false;
    *out = static_cast<std::int64_t>((zigzag >> 1u) ^ (0u - static_cast<std::uint64_t>(zigzag & 1u)));
    return true;
}

/**
 * @brief 读写带变长长度前缀的文本。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::varText(Memory::String *out)

{
    std::uint64_t size = 0u;
    if (!varUInt(&size)) return false;
    if (size > kMaximumStringSize)
    {
        error = "二进制程序里的字符串超过 64 MiB 上限";
        return false;
    }
    const std::uint8_t *data = nullptr;
    if (!take(static_cast<std::size_t>(size), &data)) return false;
    out->assign(reinterpret_cast<const char *>(data), static_cast<std::size_t>(size));
    return true;
}

/**
 * @brief 取得文本字段的字符串内容。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::text(Memory::String *out)

{
    std::uint32_t size = 0u;
    if (!u32(&size)) return false;
    if (size > kMaximumStringSize)
    {
        error = "二进制程序里的字符串超过 64 MiB 上限";
        return false;
    }
    const std::uint8_t *data = nullptr;
    if (!take(size, &data)) return false;
    out->assign(reinterpret_cast<const char *>(data), size);
    return true;
}

/**
 * @brief 读写二进制记录的条目数量。
 * @param out 接收处理结果的输出对象。
 * @param what 测试条件的说明文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BinaryReader::count(std::uint32_t *out, const char *what)

{
    if (symbolRecords)
    {
        std::uint64_t value = 0;
        if (!varUInt(&value)) return false;
        if (value > kMaximumItemCount || value > bytes.size() - position)
        {
            error = "数量字段超过上限或剩余字节数";
            return false;
        }
        *out = static_cast<std::uint32_t>(value);
    }
    else if (!u32(out))
        return false;
    if (*out > kMaximumItemCount || *out > bytes.size() - position)
    {
        error = Memory::String(what) + "超过二进制格式的数量上限";
        return false;
    }
    return true;
}
}  // namespace runtimeDetail
}  // namespace stackvm
