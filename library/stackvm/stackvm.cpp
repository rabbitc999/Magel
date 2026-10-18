#include <iomanip>
#include <array>
/**
 * @file stackvm.cpp
 * @brief stackvm.hpp 的实现：四元式解析、组装、栈式虚拟机执行。
 */

#include "stackvm.hpp"
#include "value_text.hpp"
#include "allocation_index.hpp"
#include "execution_plan.hpp"
#include "data_capacity_index.hpp"
#include "register_core.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#if STACKVM_WITH_HOST_IO
#include <fstream>
#endif
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

/* 大整数（`bint`）是**附加功能**：`STACKVM_WITH_BIGINT`（见 stackvm.hpp）为 1 才编进来。
 * 编进来时用仓库里的 library/big_number（`bigint::BigInt`）：加 / 减 / 乘 / 除 / 取余
 * 与十进制串互转都在那边，本库只负责"把它接到虚拟机的值语义上"（见运行期对象表）。 */
#if STACKVM_WITH_BIGINT
#include "bigint.hpp"
#endif
#if STACKVM_WITH_BIGFLOAT
#include "bigfloat.hpp"
#endif

/* 板端输入钩子（串口）。必须放在**第一个 namespace 之前**：下面的 parts 嵌在命名空间里，
 * 在那里 include 会各自造一份 mcuConsole，宿主装的钩子就看不见了。 */
#if STACKVM_EMBEDDED
#include "../../source/stackvm_mcu/mcu_console.hpp"
#endif

namespace stackvm
{
/**
 * @brief 查询实际编译进运行时的能力。
 * @return 能力位集合。
 */
std::uint32_t supportedProgramFeatures()
{
#if STACKVM_WITH_REFLECTION
    return kFeatureReflection;
#else
    return 0;
#endif
}

/**
 * @brief 验证二进制、组装及直接执行共用的能力契约。
 * @param required 程序能力位。
 * @param diagnostics 可选诊断输出。
 * @return 校验成功返回 true。
 */
bool validateProgramFeatures(std::uint32_t required,
                             Memory::Vector<Memory::String> *diagnostics)
{
    const char *error = nullptr;
    if ((required & ~kKnownProgramFeatures) != 0)
        error = "不支持的程序特征标志";
    else if ((required & ~supportedProgramFeatures()) != 0)
        error = "程序要求反射支持，但运行时未启用 STACKVM_WITH_REFLECTION=1";
    if (error == nullptr) return true;
    if (diagnostics != nullptr) diagnostics->emplace_back(error);
    return false;
}

/**
 * @brief 验证访问完全位于同一个活跃分配中，避免宽访问跨过块尾。
 * @param heap 分配器。
 * @param address 地址。
 * @param count 字节数。
 * @return 零长度或有效分配范围返回 true。
 */
static bool allocatedHeapRange(const HeapAllocator &heap, std::size_t address, std::size_t count)
{
#if STACKVM_WITH_ALLOCATION_GUARDS
    std::size_t capacity = 0;
    return count == 0 || (heap.capacityAt(address, capacity) && count <= capacity);
#else
    (void)heap;
    (void)address;
    (void)count;
    return true;
#endif
}

/**
 * @brief 读 8 字节小端。
 * @param address 地址（数据区内的偏移）。
 * @param out 结果。
 * @return 越界返回 false。
 */
bool HeapAllocator::readInt(std::size_t address, std::int64_t &out) const
{
    return allocatedHeapRange(*this, address, sizeof(out)) && readBytes(address, reinterpret_cast<char *>(&out), sizeof(out));
}

/**
 * @brief 写 8 字节小端。
 * @param address 地址。
 * @param value 值。
 * @return 越界返回 false。
 */
bool HeapAllocator::writeInt(std::size_t address, std::int64_t value)
{
    return allocatedHeapRange(*this, address, sizeof(value)) && writeBytes(address, reinterpret_cast<const char *>(&value), sizeof(value));
}

/**
 * @brief 读 8 字节小端，按 IEEE-754 双精度解释（`(load:f64 …)` 用它）。
 * @param address 地址。
 * @param out 结果。
 * @return 越界返回 false。
 */
bool HeapAllocator::readDouble(std::size_t address, double &out) const
{
    return allocatedHeapRange(*this, address, sizeof(out)) && readBytes(address, reinterpret_cast<char *>(&out), sizeof(out));
}

/**
 * @brief 把双精度写成 8 字节小端（`(store:f64 …)` 用它）。
 * @param address 地址。
 * @param value 值。
 * @return 越界返回 false。
 */
bool HeapAllocator::writeDouble(std::size_t address, double value)
{
    return allocatedHeapRange(*this, address, sizeof(value)) && writeBytes(address, reinterpret_cast<const char *>(&value), sizeof(value));
}

/**
 * @brief 将整数封装为宿主原生值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
NativeValue NativeValue::fromInt(std::int64_t value)
{
    NativeValue result;
    result.integer = value;
    return result;
}

/**
 * @brief 将浮点数封装为宿主原生值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
NativeValue NativeValue::fromFloat(double value)
{
    NativeValue result;
    result.kind = Kind::Floating;
    result.floating = value;
    return result;
}

/**
 * @brief 将文本封装为宿主原生值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
NativeValue NativeValue::fromText(Memory::String value)
{
    NativeValue result;
    result.kind = Kind::Text;
    result.text = std::move(value);
    return result;
}

/**
 * @brief 尝试将当前值读取为整数。
 * @retval 尝试将当前值读取为整数。
 */
std::int64_t NativeValue::asInt() const
{
    if (isText()) return 0;
    return isFloat() ? static_cast<std::int64_t>(floating) : integer;
}

/**
 * @brief 将当前运行时值转换为浮点数。
 * @retval 保存将当前运行时值转换为浮点数结果的对象。
 */
double NativeValue::asFloat() const
{
    if (isText()) return 0.0;
    return isFloat() ? floating : static_cast<double>(integer);
}

/**
 * @brief 登记原生函数的名称、参数数量与实现。
 * @param name 名称文本。
 * @param arity 原生函数接收的参数数量。
 * @param callback 接收事件或执行操作的回调函数。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeRegistry::add(const Memory::String &name, std::size_t arity, NativeCallback callback,
                         Memory::String *error)
{
    if (name.empty())
    {
        if (error != nullptr) *error = "原生函数名不能为空";
        return false;
    }
    if (!callback)
    {
        if (error != nullptr) *error = "原生函数 " + name + " 没有 C++ 回调";
        return false;
    }
    if (entries_.count(name) != 0u)
    {
        if (error != nullptr) *error = "原生函数重复注册：" + name;
        return false;
    }
    const auto key = symbols::key(symbols::logicalId(name));
    if (compactNames_.count(key))
    {
        if (error) *error = "原生函数稳定编号碰撞";
        return false;
    }
    compactNames_.emplace(key, name);
    entries_.emplace(name, Entry{ arity, std::move(callback) });
    ++revision_;
    return true;
}

/**
 * @brief 移除指定名称的原生函数绑定。
 * @param name 名称文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeRegistry::remove(const Memory::String &name)
{
    compactNames_.erase(symbols::key(symbols::logicalId(name)));
    const bool removed = entries_.erase(name) != 0u;
    if (removed) ++revision_;
    return removed;
}

/**
 * @brief 将名字解析为稳定整数身份，动态回调临时名称不自动驻留。
 * @param name 名字。
 * @return 已登记身份或 absent。
 */
const NativeRegistry::Entry *NativeRegistry::find(const Memory::String &name) const
{
    const auto alias = compactNames_.find(name);
    const auto found = entries_.find(alias == compactNames_.end() ? name : alias->second);
    return found == entries_.end() ? nullptr : &found->second;
}

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
 * @brief 从**内存映像 + 布局文本**加载数据区（板端：两者都在 flash 里，没有文件系统）。
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

namespace
{

constexpr std::uint8_t kProgramMagic[] = { 'P', 'E', 'S', 'V', 'M', 'B', '\r', '\n' };
/**
 * 当前写出的版本。
 */
constexpr std::uint16_t kProgramFormatVersion = 5u;
constexpr std::uint8_t kSymbolMagic[] = { 'M', 'C', 'V', 'M', '0', '0', '0', '5' };
constexpr std::size_t kSymbolHeaderSize = 44u;
/**
 * 从这一版起 payload 末尾有"导出标签"节。
 */
constexpr std::uint16_t kProgramExportsFormatVersion = 3u;
/**
 * 从这一版起 payload 末尾（导出标签之后）还有"调试信息"节。
 */
constexpr std::uint16_t kProgramDebugFormatVersion = 4u;
constexpr std::uint16_t kProgramCompactFormatVersion = 2u;
constexpr std::uint16_t kProgramLegacyFormatVersion = 1u;
constexpr std::size_t kProgramHeaderSize = 24u;
constexpr std::size_t kMaximumBinarySize = 512u * 1024u * 1024u;
constexpr std::uint32_t kMaximumItemCount = 10u * 1000u * 1000u;
constexpr std::uint32_t kMaximumStringSize = 64u * 1024u * 1024u;

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

struct BinaryWriter
{
    Memory::Vector<std::uint8_t> bytes;
    Memory::String error;
    const Memory::Map<Memory::String, std::uint64_t> *symbolIndices = nullptr;
    /**
     * @brief 读写二进制中间产物的符号字段。
     * @param value 待转换、保存或参与计算的值。
     */
    void symbol(const Memory::String &value)
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
    void number(std::uint64_t value)
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
    void raw(const std::uint8_t *data, std::size_t size)
    {
        if (size) bytes.insert(bytes.end(), data, data + size);
    }

    /**
     * @brief 读写一个 8 位无符号整数。
     * @param value 待转换、保存或参与计算的值。
     */
    void u8(std::uint8_t value) { bytes.push_back(value); }

    /**
     * @brief 读写一个 16 位无符号整数。
     * @param value 待转换、保存或参与计算的值。
     */
    void u16(std::uint16_t value)
    {
        for (unsigned shift = 0u; shift < 16u; shift += 8u)
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }

    /**
     * @brief 读写一个 32 位无符号整数。
     * @param value 待转换、保存或参与计算的值。
     */
    void u32(std::uint32_t value)
    {
        for (unsigned shift = 0u; shift < 32u; shift += 8u)
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }

    /**
     * @brief 读写一个 64 位无符号整数。
     * @param value 待转换、保存或参与计算的值。
     */
    void u64(std::uint64_t value)
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
    void varUInt(std::uint64_t value)
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
    void varInt(std::int64_t value)
    {
        const std::uint64_t zigzag = (static_cast<std::uint64_t>(value) << 1u) ^ (0u - static_cast<std::uint64_t>(value < 0));
        varUInt(zigzag);
    }

    /**
     * @brief 读写带变长长度前缀的文本。
     * @param value 待转换、保存或参与计算的值。
     */
    void varText(const Memory::String &value)
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
    void text(const Memory::String &value)
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
    bool count(std::size_t value, const char *what)
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
};

struct BinaryReader
{
    const Memory::Vector<std::uint8_t> &bytes;
    std::size_t position = 0u;
    Memory::String error;

    const Memory::Vector<SymbolRecord> *symbolRecords = nullptr;
    /**
     * @brief 读写二进制中间产物的符号字段。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool symbol(Memory::String *out)
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
    bool number(std::uint64_t *out) { return symbolRecords ? varUInt(out) : u64(out); }
    /**
     * @brief 初始化二进制中间产物读取器。
     * @param input 程序输入或待解码的字节流。
     */
    explicit BinaryReader(const Memory::Vector<std::uint8_t> &input) : bytes(input) {}

    /**
     * @brief 检查并消耗指定长度的二进制数据。
     * @param size 输入数据或目标区域的大小。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool take(std::size_t size, const std::uint8_t **out)
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
    bool u8(std::uint8_t *out)
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
    bool u16(std::uint16_t *out)
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
    bool u32(std::uint32_t *out)
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
    bool u64(std::uint64_t *out)
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
    bool varUInt(std::uint64_t *out)
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
    bool varInt(std::int64_t *out)
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
    bool varText(Memory::String *out)
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
    bool text(Memory::String *out)
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
    bool count(std::uint32_t *out, const char *what)
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
};

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

}  // namespace

#include "parts/symbol_build.cpp"
#include "parts/symbol_container.cpp"

/**
 * @brief 识别旧格式与无名格式魔数。
 * @param path 路径。
 * @return 是否为字节码。
 */
bool isProgramBinary(const Memory::String &path)
{
#if STACKVM_WITH_HOST_IO

    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::uint8_t magic[sizeof(kProgramMagic)] = {};
    stream.read(reinterpret_cast<char *>(magic), static_cast<std::streamsize>(sizeof(magic)));
    return stream.gcount() == static_cast<std::streamsize>(sizeof(magic)) && (std::equal(std::begin(magic), std::end(magic), std::begin(kProgramMagic)) || std::equal(std::begin(magic), std::end(magic), std::begin(kSymbolMagic)));

#else
    return false;
#endif
}

/**
 * @brief 序列化旧格式或无名变长格式。
 * @param program 程序。
 * @param path 输出。
 * @param diagnostics 诊断。
 * @param compressSymbols 无名符号开关。
 * @return 写入成功返回 true。
 */
bool saveProgramBinary(const Program &program, const Memory::String &path,
                       Memory::Vector<Memory::String> *diagnostics, bool compressSymbols)
{
#if STACKVM_WITH_HOST_IO

    if (!validateProgramMetadata(program, diagnostics)) return false;
    if (!program.ok())
    {
        binaryDiagnostic(diagnostics, "不能序列化带组装诊断的程序");
        return false;
    }
    if (program.compactSymbols)
    {
        binaryDiagnostic(diagnostics, "压缩输入不能再次压缩");
        return false;
    }
    Memory::Vector<SymbolRecord> records;
    Memory::Map<Memory::String, std::uint64_t> indices;
    if (compressSymbols && !buildSymbols(program, records, indices, diagnostics)) return false;
    BinaryWriter payload;
    if (compressSymbols) payload.symbolIndices = &indices;
    if (compressSymbols)
        payload.varUInt(symbols::logicalId(program.name));
    else
        payload.text(program.name);
    if (!payload.count(program.code.size(), "指令数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (std::size_t instructionIndex = 0u; instructionIndex < program.code.size(); ++instructionIndex)
    {
        const Instruction &instruction = program.code[instructionIndex];
        if (static_cast<std::uint16_t>(instruction.op) > static_cast<std::uint16_t>(Op::StoreAt) || static_cast<std::uint8_t>(instruction.format) > static_cast<std::uint8_t>(PrintFormat::Text))
        {
            binaryDiagnostic(diagnostics, "程序含无法序列化的指令枚举值");
            return false;
        }
        std::uint64_t doubleBits = 0u;
        std::memcpy(&doubleBits, &instruction.doubleValue, sizeof(doubleBits));
        std::uint8_t operands = 0u;
        if (instruction.format != PrintFormat::Signed) operands |= 1u << 0u;
        if (instruction.unsignedOp || instruction.isFloat) operands |= 1u << 1u;
        if (instruction.intValue != 0) operands |= 1u << 2u;
        if (doubleBits != 0u) operands |= 1u << 3u;
        if (instruction.quadIndex != instructionIndex) operands |= 1u << 4u;
        if (!instruction.name.empty()) operands |= 1u << 5u;
        if (!instruction.name2.empty()) operands |= 1u << 6u;
        if (!instruction.textValue.empty()) operands |= 1u << 7u;

        payload.u8(static_cast<std::uint8_t>(instruction.op));
        payload.u8(operands);
        if ((operands & (1u << 0u)) != 0u)
            payload.u8(static_cast<std::uint8_t>(instruction.format));
        if ((operands & (1u << 1u)) != 0u)
            payload.u8(static_cast<std::uint8_t>((instruction.unsignedOp ? 1u : 0u) | (instruction.isFloat ? 2u : 0u)));
        if ((operands & (1u << 2u)) != 0u) payload.varInt(instruction.intValue);
        if ((operands & (1u << 3u)) != 0u) payload.u64(doubleBits);
        if ((operands & (1u << 4u)) != 0u) payload.varUInt(instruction.quadIndex);
        if ((operands & (1u << 5u)) != 0u) payload.symbolIndices ? payload.symbol(instruction.name) : payload.varText(instruction.name);
        if ((operands & (1u << 6u)) != 0u) payload.symbolIndices ? payload.symbol(instruction.name2) : payload.varText(instruction.name2);
        if ((operands & (1u << 7u)) != 0u) payload.varText(instruction.textValue);
    }

    if (!payload.count(program.labels.size(), "标签数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &[name, index] : program.labels)
    {
        if (index > program.code.size())
        {
            binaryDiagnostic(diagnostics, "程序含越过指令末尾的标签：" + name);
            return false;
        }
        payload.symbol(name);
        payload.number(static_cast<std::uint64_t>(index));
    }

    if (!payload.count(program.types.size(), "类型数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &[variable, type] : program.types)
    {
        payload.symbol(variable);
        payload.symbol(type.name);
        compressSymbols ? payload.varInt(type.bits) : payload.u32(static_cast<std::uint32_t>(static_cast<std::int32_t>(type.bits)));
        payload.u8(static_cast<std::uint8_t>((type.isUnsigned ? 1u : 0u) | (type.isFloat ? 2u : 0u)));
        payload.number(static_cast<std::uint64_t>(type.size));
    }

    if (!payload.count(program.parameters.size(), "形参表数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &[label, parameters] : program.parameters)
    {
        payload.symbol(label);
        if (!payload.count(parameters.size(), "形参数"))
        {
            binaryDiagnostic(diagnostics, payload.error);
            return false;
        }
        for (const Memory::String &parameter : parameters) payload.symbol(parameter);
    }
    if (!payload.count(program.exports.size(), "导出标签数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const Memory::String &label : program.exports)
    {
        if (program.labels.find(label) == program.labels.end())
        {
            binaryDiagnostic(diagnostics, "导出项不是程序标签：" + label);
            return false;
        }
        payload.symbol(label);
    }
    /* 调试信息节（v4）：指令下标 → 源码位置。没有调试信息也写 0 条，节始终在场。 */
    if (!payload.count(compressSymbols ? 0u : program.debug.entries.size(), "调试信息条数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &entry : (compressSymbols ? DebugInfo{}.entries : program.debug.entries))
    {
        payload.varUInt(static_cast<std::uint64_t>(entry.first));
        payload.varText(entry.second.function);
        payload.varText(entry.second.file);
        payload.varUInt(entry.second.line > 0 ? static_cast<std::uint64_t>(entry.second.line) : 0u);
        payload.varUInt(static_cast<std::uint64_t>(entry.second.quadLine));
    }
    // 新版产物直接携带内部执行指令；原始 VM 指令作为精确观察与回退序列。
    auto prepared = execution_detail::lowerRegisters(program);
    if (!payload.count(prepared.size(), "内部指令数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &internal : prepared)
    {
        payload.varUInt(internal.left);
        payload.u8(static_cast<std::uint8_t>(internal.operation));
        payload.u8(static_cast<std::uint8_t>(internal.kind));
        payload.varUInt(internal.span);
        payload.varInt(internal.immediate);
        payload.varInt(internal.bits);
        payload.u8((internal.literal ? 1 : 0) | (internal.unsignedResult ? 2 : 0) | (internal.jumpOnZero ? 4 : 0) | (internal.storeResult ? 8 : 0));
        payload.varUInt(internal.target == execution_detail::absent ? 0 : internal.target + 1);
    }
    if (program.requiredFeatures & kFeatureReflection)
    {
        Memory::Vector<std::uint8_t> metadata;
        Memory::String error;
        if (!reflection::encode(program.reflection, metadata, &error))
        {
            binaryDiagnostic(diagnostics, error);
            return false;
        }
        payload.raw(metadata.data(), metadata.size());
    }
    if (!payload.error.empty())
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    if (compressSymbols) return writeSymbolContainer(program, payload.bytes, records, path, diagnostics);
    const std::uint16_t flags = static_cast<std::uint16_t>(program.requiredFeatures);
    const auto &stored = payload.bytes;
    if (stored.size() > kMaximumBinarySize - kProgramHeaderSize)
    {
        binaryDiagnostic(diagnostics, "二进制程序过大（上限 512 MiB）");
        return false;
    }

    BinaryWriter file;
    file.raw(kProgramMagic, sizeof(kProgramMagic));
    file.u16(kProgramFormatVersion);
    file.u16(flags);
    file.u64(static_cast<std::uint64_t>(stored.size()));
    file.u32(binaryChecksum(stored.data(), stored.size()));
    file.raw(stored.data(), stored.size());

    std::ofstream stream(path, std::ios::binary);
    if (!stream)
    {
        binaryDiagnostic(diagnostics, "写不了二进制程序：" + path);
        return false;
    }
    stream.write(reinterpret_cast<const char *>(file.bytes.data()),
                 static_cast<std::streamsize>(file.bytes.size()));
    if (!stream)
    {
        binaryDiagnostic(diagnostics, "写二进制程序失败：" + path);
        return false;
    }
    return true;

#else
    return false;
#endif
}

/**
 * @brief 路径版与内存版共用的解码实现（定义在本文件下方）。
 */
static bool decodeProgramBinary(const Memory::Vector<std::uint8_t> &fileBytes,
                                const Memory::String &label, Program &program,
                                Memory::Vector<Memory::String> *diagnostics);

/**
 * @brief 从内存字节加载程序；声明与文档见 `stackvm.hpp`。
 */
bool loadProgramBinary(const void *bytes, std::size_t size, Program &program,
                       Memory::Vector<Memory::String> *diagnostics)
{
    if (bytes == nullptr || size == 0u)
    {
        binaryDiagnostic(diagnostics, "空的内存程序映像");
        return false;
    }
    const std::uint8_t *first = static_cast<const std::uint8_t *>(bytes);
    Memory::Vector<std::uint8_t> fileBytes(first, first + size);
    return decodeProgramBinary(fileBytes, "<内存映像>", program, diagnostics);
}

/**
 * @brief 校验并加载兼容格式（按路径），失败不修改输出程序。
 * @param path 输入。
 * @param program 程序。
 * @param diagnostics 诊断。
 * @return 成功返回 true。
 */
bool loadProgramBinary(const Memory::String &path, Program &program,
                       Memory::Vector<Memory::String> *diagnostics)
{
    Memory::Vector<std::uint8_t> fileBytes;
    if (!readBinaryFile(path, &fileBytes, diagnostics)) return false;
    return decodeProgramBinary(fileBytes, path, program, diagnostics);
}

/**
 * @brief 路径版与内存版共用的解码实现。
 */
static bool decodeProgramBinary(const Memory::Vector<std::uint8_t> &fileBytes,
                                const Memory::String &label, Program &program,
                                Memory::Vector<Memory::String> *diagnostics)
{
    const bool compressed = fileBytes.size() >= 8 && std::equal(std::begin(kSymbolMagic), std::end(kSymbolMagic), fileBytes.begin());
    Program decoded;
    Memory::Vector<std::uint8_t> payloadBytes;
    if (compressed)
    {
        if (!readSymbolContainer(fileBytes, decoded, payloadBytes, diagnostics)) return false;
    }
    if (!compressed)
    {
        if (fileBytes.size() < kProgramHeaderSize || !std::equal(std::begin(kProgramMagic), std::end(kProgramMagic), fileBytes.begin()))
        {
            binaryDiagnostic(diagnostics, "不是 stackvm 二进制程序：" + label);
            return false;
        }

        BinaryReader header{ fileBytes };
        const std::uint8_t *ignoredMagic = nullptr;
        std::uint16_t version = 0u;
        std::uint16_t flags = 0u;
        std::uint64_t payloadSize = 0u;
        std::uint32_t checksum = 0u;
        if (!header.take(sizeof(kProgramMagic), &ignoredMagic) || !header.u16(&version) || !header.u16(&flags) || !header.u64(&payloadSize) || !header.u32(&checksum))
        {
            binaryDiagnostic(diagnostics, header.error);
            return false;
        }
        if (version != kProgramFormatVersion || (flags & ~static_cast<std::uint16_t>(kKnownProgramFeatures)) != 0u)
        {
            binaryDiagnostic(diagnostics, "不支持的 stackvm 二进制版本或标志");
            return false;
        }
        decoded.requiredFeatures = flags & kKnownProgramFeatures;
        if (!validateProgramFeatures(decoded.requiredFeatures, diagnostics)) return false;
        if (payloadSize != fileBytes.size() - kProgramHeaderSize)
        {
            binaryDiagnostic(diagnostics, "二进制程序长度字段不匹配");
            return false;
        }
        const std::uint8_t *payloadBegin = fileBytes.data() + kProgramHeaderSize;
        if (binaryChecksum(payloadBegin, static_cast<std::size_t>(payloadSize)) != checksum)
        {
            binaryDiagnostic(diagnostics, "二进制程序校验和不匹配");
            return false;
        }

        const std::uint8_t *payloadEnd = fileBytes.data() + fileBytes.size();
        payloadBytes.assign(payloadBegin, payloadEnd);
    }
    BinaryReader reader{ payloadBytes };
    if (compressed) reader.symbolRecords = &decoded.symbols;
    std::uint16_t version = compressed ? kProgramFormatVersion : static_cast<std::uint16_t>(fileBytes[8] | (fileBytes[9] << 8u));
    std::uint64_t moduleId = 0;
    if (compressed ? !reader.varUInt(&moduleId) : !reader.text(&decoded.name))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }

    if (compressed) decoded.name = symbols::key(moduleId);
    std::uint32_t count = 0u;
    if (!reader.count(&count, "指令数"))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }
    decoded.code.reserve(count);
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        Instruction instruction;
        std::uint16_t op = 0u;
        std::uint8_t format = 0u;
        std::uint8_t instructionFlags = 0u;
        std::uint64_t doubleBits = 0u;
        std::uint64_t quadIndex = index;
        std::int64_t intValue = 0;
        if (version == kProgramLegacyFormatVersion)
        {
            std::uint64_t legacyIntBits = 0u;
            if (!reader.u16(&op) || !reader.u8(&format) || !reader.u8(&instructionFlags) || !reader.u64(&legacyIntBits) || !reader.u64(&doubleBits) || !reader.u64(&quadIndex) || !reader.text(&instruction.name) || !reader.text(&instruction.name2) || !reader.text(&instruction.textValue))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            std::memcpy(&intValue, &legacyIntBits, sizeof(intValue));
        }
        else
        {
            std::uint8_t byteOp = 0u;
            std::uint8_t operands = 0u;
            if (!reader.u8(&byteOp) || !reader.u8(&operands))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            op = byteOp;
            std::uint64_t varQuadIndex = index;
            if (((operands & (1u << 0u)) != 0u && !reader.u8(&format)) || ((operands & (1u << 1u)) != 0u && !reader.u8(&instructionFlags)) || ((operands & (1u << 2u)) != 0u && !reader.varInt(&intValue)) || ((operands & (1u << 3u)) != 0u && !reader.u64(&doubleBits)) || ((operands & (1u << 4u)) != 0u && !reader.varUInt(&varQuadIndex)) || ((operands & (1u << 5u)) != 0u && !(compressed ? reader.symbol(&instruction.name) : reader.varText(&instruction.name))) || ((operands & (1u << 6u)) != 0u && !(compressed ? reader.symbol(&instruction.name2) : reader.varText(&instruction.name2))) || ((operands & (1u << 7u)) != 0u && !reader.varText(&instruction.textValue)))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            quadIndex = varQuadIndex;
        }
        if (op > static_cast<std::uint16_t>(Op::StoreAt) || format > static_cast<std::uint8_t>(PrintFormat::Text) || (instructionFlags & ~3u) != 0u || quadIndex > kMaximumItemCount)
        {
            binaryDiagnostic(diagnostics, "二进制程序含非法指令字段");
            return false;
        }
        instruction.op = static_cast<Op>(op);
        instruction.format = static_cast<PrintFormat>(format);
        instruction.unsignedOp = (instructionFlags & 1u) != 0u;
        instruction.isFloat = (instructionFlags & 2u) != 0u;
        instruction.intValue = intValue;
        std::memcpy(&instruction.doubleValue, &doubleBits, sizeof(doubleBits));
        instruction.quadIndex = static_cast<std::size_t>(quadIndex);
        decoded.code.push_back(std::move(instruction));
    }

    if (!reader.count(&count, "标签数"))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        Memory::String name;
        std::uint64_t target = 0u;
        if (!reader.symbol(&name) || !reader.number(&target) || target > decoded.code.size() || !decoded.labels.emplace(std::move(name), static_cast<std::size_t>(target)).second)
        {
            binaryDiagnostic(diagnostics, reader.error.empty() ? "二进制程序含非法或重复标签" : reader.error);
            return false;
        }
    }

    if (!reader.count(&count, "类型数"))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        Memory::String variable;
        TypeInfo type;
        std::uint32_t bits = 0u;
        std::int64_t varBits = 0;
        std::uint8_t typeFlags = 0u;
        std::uint64_t size = 0u;
        if (!reader.symbol(&variable) || !reader.symbol(&type.name) || !(compressed ? reader.varInt(&varBits) : reader.u32(&bits)) || !reader.u8(&typeFlags) || !reader.number(&size) || (typeFlags & ~3u) != 0u || size > kMaximumBinarySize)
        {
            binaryDiagnostic(diagnostics, reader.error.empty() ? "二进制程序含非法类型字段" : reader.error);
            return false;
        }
        if (compressed && (varBits < -1 || varBits > 65536))
        {
            binaryDiagnostic(diagnostics, "类型位宽越界");
            return false;
        }
        type.bits = compressed ? static_cast<std::int32_t>(varBits) : static_cast<std::int32_t>(bits);
        type.isUnsigned = (typeFlags & 1u) != 0u;
        type.isFloat = (typeFlags & 2u) != 0u;
        type.size = static_cast<std::size_t>(size);
        if (!decoded.types.emplace(std::move(variable), std::move(type)).second)
        {
            binaryDiagnostic(diagnostics, "二进制程序含重复类型项");
            return false;
        }
    }

    if (!reader.count(&count, "形参表数"))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        Memory::String label;
        std::uint32_t parameterCount = 0u;
        if (!reader.symbol(&label) || !reader.count(&parameterCount, "形参数"))
        {
            binaryDiagnostic(diagnostics, reader.error);
            return false;
        }
        Memory::Vector<Memory::String> parameters;
        parameters.reserve(parameterCount);
        for (std::uint32_t parameter = 0u; parameter < parameterCount; ++parameter)
        {
            Memory::String name;
            if (!reader.symbol(&name))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            parameters.push_back(std::move(name));
        }
        if (!decoded.parameters.emplace(std::move(label), std::move(parameters)).second)
        {
            binaryDiagnostic(diagnostics, "二进制程序含重复形参表");
            return false;
        }
    }
    if (version >= kProgramExportsFormatVersion)
    {
        if (!reader.count(&count, "导出标签数"))
        {
            binaryDiagnostic(diagnostics, reader.error);
            return false;
        }
        for (std::uint32_t index = 0u; index < count; ++index)
        {
            Memory::String label;
            if (!reader.symbol(&label) || decoded.labels.find(label) == decoded.labels.end() || !decoded.exports.insert(std::move(label)).second)
            {
                binaryDiagnostic(diagnostics, reader.error.empty() ? "二进制程序含非法或重复导出项" : reader.error);
                return false;
            }
        }
    }
    else
    {
        for (const auto &[label, ignored] : decoded.labels)
        {
            (void)ignored;
            decoded.exports.insert(label);
        }
    }
    /* 调试信息节（v4 起才有）。旧版本没有这一段，读到时直接跳过。 */
    if (version >= kProgramDebugFormatVersion)
    {
        if (!reader.count(&count, "调试信息条数"))
        {
            binaryDiagnostic(diagnostics, reader.error);
            return false;
        }
        if (compressed && count != 0)
        {
            binaryDiagnostic(diagnostics, "无名格式不能携带原始调试节");
            return false;
        }
        std::size_t previous = 0u;
        bool first = true;
        for (std::uint32_t index = 0u; index < count; ++index)
        {
            std::uint64_t instruction = 0u;
            std::uint64_t line = 0u;
            std::uint64_t quadLine = 0u;
            SourceLocation location;
            if (!reader.varUInt(&instruction) || !reader.varText(&location.function) || !reader.varText(&location.file) || !reader.varUInt(&line) || !reader.varUInt(&quadLine))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            if (instruction >= decoded.code.size() || (!first && instruction < previous) || line > kMaximumItemCount || quadLine > kMaximumItemCount)
            {
                binaryDiagnostic(diagnostics, "二进制程序含非法调试信息");
                return false;
            }
            location.line = static_cast<std::int32_t>(line);
            location.quadLine = static_cast<std::size_t>(quadLine);
            decoded.debug.entries.emplace_back(static_cast<std::size_t>(instruction),
                                               std::move(location));
            previous = static_cast<std::size_t>(instruction);
            first = false;
        }
    }
    // 校验存储的内部序列与基础 VM 指令完全一致，再供执行器直接绑定。
    auto expected = execution_detail::lowerRegisters(decoded);
    if (!reader.count(&count, "内部指令数") || count != expected.size())
    {
        binaryDiagnostic(diagnostics, "内部指令数量非法");
        return false;
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        std::uint64_t origin = 0, span = 0, target = 0;
        std::uint8_t op = 0, kind = 0, flags = 0;
        std::int64_t immediate = 0, bits = 0;
        if (!reader.varUInt(&origin) || !reader.u8(&op) || !reader.u8(&kind) || !reader.varUInt(&span) || !reader.varInt(&immediate) || !reader.varInt(&bits) || !reader.u8(&flags) || !reader.varUInt(&target))
        {
            binaryDiagnostic(diagnostics, reader.error);
            return false;
        }
        const auto &internal = expected[index];
        const auto expectedFlags = (internal.literal ? 1 : 0) | (internal.unsignedResult ? 2 : 0) | (internal.jumpOnZero ? 4 : 0) | (internal.storeResult ? 8 : 0);
        if (origin != internal.left || op != static_cast<std::uint8_t>(internal.operation) || kind != static_cast<std::uint8_t>(internal.kind) || span != internal.span || immediate != internal.immediate || bits != internal.bits || flags != expectedFlags || target != (internal.target == execution_detail::absent ? 0 : internal.target + 1))
        {
            binaryDiagnostic(diagnostics, "内部指令与基础指令不一致");
            return false;
        }
    }
    decoded.internalCode = std::move(expected);
    decoded.preparedInternal = true;

    if ((decoded.requiredFeatures & kFeatureReflection) && reader.position < payloadBytes.size())
    {
        Memory::String error;
        if (!reflection::decode(payloadBytes.data() + reader.position,
                                payloadBytes.size() - reader.position, decoded.reflection, &error))
        {
            binaryDiagnostic(diagnostics, error);
            return false;
        }
        reader.position = payloadBytes.size();
    }
    if (reader.position != payloadBytes.size())
    {
        binaryDiagnostic(diagnostics, "二进制程序尾部有未识别数据");
        return false;
    }
    if (compressed && !validateSymbolProgram(fileBytes, decoded, diagnostics)) return false;
    program = std::move(decoded);
    return true;
}

#include "parts/machine_natives.cpp"
#include "parts/artifact_binary.cpp"
#include "parts/quads.cpp"
#include "parts/assembly.cpp"
#include "parts/execution.cpp"
#if STACKVM_WITH_AOT
#include "parts/aot_bridge.cpp"
#endif
}  // namespace stackvm
