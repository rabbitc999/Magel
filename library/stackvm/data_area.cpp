/**
 * @file data_area.cpp
 * @brief VM 数据区初始化、地址转换与边界检查。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 读一个以 `\0` 结尾的字符串。
 *
 * 函数指针就是"数据区里存着函数标签的那串字节的地址"，间接调用靠它拿标签。
 * @param address 地址（数据区内的偏移）。
 * @param limit 最多读多少字节（防越界）。
 * @return 字符串；地址越界返回空串。
 */
Memory::String DataArea::readCString(std::size_t address, std::size_t limit) const
{
    if (address >= binary.size()) return Memory::String();
    Memory::String out;
    for (std::size_t index = address; index < binary.size() && out.size() < limit; ++index)
    {
        const char ch = static_cast<char>(binary[index]);
        if (ch == '\0') break;
        out.push_back(ch);
    }
    return out;
}

/**
 * @brief 从**内存映像 + 布局文本**加载数据区（由宿主直接提供内存缓冲区）。
 *
 * 行格式与文件版一致：`符号 offset=0 size=8 type=… stride=8`。
 * @param bytes 数据区字节。
 * @param size 字节数。
 * @param layoutText 布局文本（可为空）。
 * @param diagnostics 诊断。
 * @return 成功返回 true。
 */
bool DataArea::loadFromBytes(const void *bytes, std::size_t size, const Memory::String &layoutText,
                             Memory::Vector<Memory::String> *diagnostics)
{
    if (bytes == nullptr || size == 0u)
    {
        if (diagnostics) diagnostics->push_back("空的数据区映像");
        return false;
    }
    const std::uint8_t *first = static_cast<const std::uint8_t *>(bytes);
    binary.assign(first, first + size);
    symbols.clear();
    sizes.clear();
    strides.clear();
    if (layoutText.size() >= 4 && layoutText.compare(0, 4, "MGLY") == 0)
    {
        Memory::Vector<LayoutRow> rows;
        if (!decodeDataLayout(layoutText, rows))
        {
            if (diagnostics) diagnostics->push_back("二进制数据布局损坏或版本不受支持");
            return false;
        }
        for (const auto &row : rows)
        {
            symbols[row.name] = row.offset;
            if (row.hasSize) sizes[row.name] = row.size;
            if (row.hasStride) strides[row.name] = row.stride;
        }
        return true;
    }
    std::size_t at = 0u;
    while (at <= layoutText.size())
    {
        const std::size_t end = layoutText.find('\n', at);
        Memory::String line =
            layoutText.substr(at, (end == Memory::String::npos ? layoutText.size() : end) - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        at = (end == Memory::String::npos) ? layoutText.size() + 1u : end + 1u;
        const std::size_t space = line.find(' ');
        const std::size_t offsetAt = line.find("offset=");
        if (space == Memory::String::npos || offsetAt == Memory::String::npos) continue;
        const Memory::String name = line.substr(0u, space);
        const std::size_t offsetEnd = line.find(' ', offsetAt);
        symbols[name] = static_cast<std::size_t>(std::strtoull(
            line.substr(offsetAt + 7u, offsetEnd - offsetAt - 7u).c_str(), nullptr, 10));
        const std::size_t sizeAt = line.find("size=");
        if (sizeAt != Memory::String::npos)
        {
            const std::size_t sizeEnd = line.find(' ', sizeAt);
            sizes[name] = static_cast<std::size_t>(std::strtoull(
                line.substr(sizeAt + 5u, sizeEnd - sizeAt - 5u).c_str(), nullptr, 10));
        }
        const std::size_t strideAt = line.find("stride=");
        if (strideAt != Memory::String::npos)
        {
            const std::size_t strideEnd = line.find(' ', strideAt);
            strides[name] = static_cast<std::size_t>(std::strtoull(
                line.substr(strideAt + 7u, strideEnd - strideAt - 7u).c_str(), nullptr, 10));
        }
    }
    return true;
}

/**
 * @brief 加载二进制与布局表。
 * @param binaryPath `.data.bin` 路径。
 * @param layoutPath 布局文件：一行 `符号 offset=0 size=8 type=…`。
 * @param diagnostics 出错信息。
 * @return 成功返回 true。
 */
bool DataArea::load(const Memory::String &binaryPath, const Memory::String &layoutPath,
                    Memory::Vector<Memory::String> *diagnostics)
{
#if STACKVM_WITH_HOST_IO

    // 1. 读二进制。
    std::ifstream binaryStream(binaryPath, std::ios::binary);
    if (!binaryStream)
    {
        if (diagnostics) diagnostics->push_back("打不开数据区文件：" + binaryPath);
        return false;
    }
    binary.assign(std::istreambuf_iterator<char>(binaryStream),
                  std::istreambuf_iterator<char>());

    // 2. 读布局：一行 `符号 offset=0 size=8 type=…`。
    symbols.clear();
    std::ifstream layoutStream(layoutPath, std::ios::binary);
    if (!layoutStream)
    {
        if (diagnostics) diagnostics->push_back("打不开布局文件：" + layoutPath);
        return false;
    }
    Memory::String layout{ std::istreambuf_iterator<char>(layoutStream), std::istreambuf_iterator<char>() };
    // 内存接口统一处理文本与二进制；先移出映像，避免赋值时自引用失效。
    const auto image = std::move(binary);
    return loadFromBytes(image.data(), image.size(), layout, diagnostics);

#else
    return false;
#endif
}

/**
 * @brief 读 8 字节小端。
 * @param address 地址（数据区内的偏移）。
 * @param out 结果。
 * @return 越界返回 false。
 */
bool DataArea::readInt(std::size_t address, std::int64_t &out) const
{
    if (address + 8u > binary.size()) return false;
    std::uint64_t bits = 0;
    for (int index = 7; index >= 0; --index)
    {
        bits = (bits << 8u) | static_cast<unsigned char>(binary[address + static_cast<std::size_t>(index)]);
    }
    out = static_cast<std::int64_t>(bits);
    return true;
}

/**
 * @brief 写 8 字节小端。
 * @param address 地址。
 * @param value 值。
 * @return 越界返回 false。
 */
bool DataArea::writeInt(std::size_t address, std::int64_t value)
{
    if (address + 8u > binary.size()) return false;
    const std::uint64_t bits = static_cast<std::uint64_t>(value);
    for (int index = 0; index < 8; ++index)
    {
        binary[address + static_cast<std::size_t>(index)] =
            static_cast<std::uint8_t>((bits >> (index * 8)) & 0xFFu);
    }
    return true;
}

/**
 * @brief 读 8 字节小端，按 IEEE-754 双精度解释（`(load:f64 …)` 用它）。
 * @param address 地址。
 * @param out 结果。
 * @return 越界返回 false。
 */
bool DataArea::readDouble(std::size_t address, double &out) const
{
    if (address + 8u > binary.size()) return false;
    std::uint64_t bits = 0;
    for (int index = 7; index >= 0; --index)
    {
        bits = (bits << 8u) | static_cast<unsigned char>(binary[address + static_cast<std::size_t>(index)]);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    out = value;
    return true;
}

/**
 * @brief 把双精度写成 8 字节小端（`(store:f64 …)` 用它）。
 * @param address 地址。
 * @param value 值。
 * @return 越界返回 false。
 */
bool DataArea::writeDouble(std::size_t address, double value)
{
    if (address + 8u > binary.size()) return false;
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int index = 0; index < 8; ++index)
    {
        binary[address + static_cast<std::size_t>(index)] =
            static_cast<std::uint8_t>((bits >> (index * 8)) & 0xFFu);
    }
    return true;
}

/**
 * @brief 按字节读一段（字符串是"以 `\0` 结尾的字节串"，跨 8 字节槽）。
 * @param address 地址（数据区内的偏移）。
 * @param out 输出缓冲（至少 count 字节）。
 * @param count 字节数。
 * @return 越界返回 false。
 */
bool DataArea::readBytes(std::size_t address, char *out, std::size_t count) const
{
    if (address + count > binary.size()) return false;
    if (count != 0u) std::memcpy(out, binary.data() + address, count);
    return true;
}

/**
 * @brief 按字节写一段。
 * @param address 地址。
 * @param in 输入缓冲（至少 count 字节）。
 * @param count 字节数。
 * @return 越界返回 false。
 */
bool DataArea::writeBytes(std::size_t address, const char *in, std::size_t count)
{
    if (address + count > binary.size()) return false;
    if (count != 0u) std::memcpy(binary.data() + address, in, count);
    return true;
}
}  // namespace stackvm
