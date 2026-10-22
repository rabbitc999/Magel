#include "std_string_methods.hpp"
#include "../chali_glue.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace std_string_methods_glue
{
namespace
{
using Text = Memory::String;
/**
 * @brief 解码严格 UTF-8 并记录每个字符及尾部的字节偏移。
 * @param text 待解码文本。
 * @return 包含尾部偏移的边界数组；编码非法时抛出诊断。
 */
std::vector<std::size_t> boundaries(const Text &text)
{
    std::vector<std::size_t> result;
    std::size_t at = 0;
    while (at < text.size())
    {
        result.push_back(at);
        auto first = static_cast<unsigned char>(text[at++]);
        unsigned remaining = 0;
        std::uint32_t code = first, minimum = 0;
        if (first < 0x80) continue;
        if (first >= 0xc2 && first <= 0xdf)
        {
            remaining = 1;
            code &= 0x1f;
            minimum = 0x80;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            remaining = 2;
            code &= 0x0f;
            minimum = 0x800;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            remaining = 3;
            code &= 7;
            minimum = 0x10000;
        }
        else
            throw std::invalid_argument("String：非法 UTF-8 起始字节");
        if (remaining > text.size() - at) throw std::invalid_argument("String：不完整的 UTF-8 字符");
        while (remaining--)
        {
            auto next = static_cast<unsigned char>(text[at++]);
            if ((next & 0xc0) != 0x80) throw std::invalid_argument("String：非法 UTF-8 延续字节");
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) throw std::invalid_argument("String：非法 Unicode 码点");
    }
    result.push_back(text.size());
    return result;
}
/**
 * @brief 将有符号字节位置限制在文本范围内。
 * @param position 输入字节位置；负值按零处理。
 * @param size 文本字节长度。
 * @return 零至 size 之间的位置。
 */
std::size_t clamp(std::int64_t position, std::size_t size)
{
    return position <= 0 ? 0 : std::min(static_cast<std::uint64_t>(position), static_cast<std::uint64_t>(size));
}
/**
 * @brief 判断 ASCII 空白，不依赖宿主区域设置。
 * @param value 要判断的字节。
 * @return 空格、制表或换行类字节时返回 true。
 */
bool space(unsigned char value)
{
    return value == ' ' || (value >= '\t' && value <= '\r');
}
/**
 * @brief 执行按字节定义的字符串查询和 ASCII 分类。
 * @param text 原文本。
 * @param needle 搜索文本。
 * @param operation 操作编号。
 * @param from 搜索开始位置或字节下标。
 * @return 索引、计数、比较值或布尔结果。
 */
std::int64_t query(Text text, Text needle, std::int64_t operation, std::int64_t from)
{
    auto start = clamp(from, text.size());
    auto result = Text::npos;
    switch (operation)
    {
    case 0: result = text.find(needle, start); break;
    case 1: result = text.rfind(needle, from < 0 ? Text::npos : start); break;
    case 2: return text.find(needle) != Text::npos;
    case 3: return text.size() >= needle.size() && text.compare(0, needle.size(), needle) == 0;
    case 4: return text.size() >= needle.size() && text.compare(text.size() - needle.size(), needle.size(), needle) == 0;
    case 5:
    {
        if (needle.empty()) return static_cast<std::int64_t>(text.size()) + 1;
        std::int64_t count = 0;
        while ((start = text.find(needle, start)) != Text::npos)
        {
            ++count;
            start += needle.size();
        }
        return count;
    }
    case 6:
    {
        auto comparison = text.compare(needle);
        return (comparison > 0) - (comparison < 0);
    }
    case 7: return std::all_of(text.begin(), text.end(), [](unsigned char c) { return space(c); });
    case 8: return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
    case 9: return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); });
    case 10: return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
    case 11: return std::all_of(text.begin(), text.end(), [](unsigned char c) { return c < 128; });
    case 12:
        if (from < 0 || static_cast<std::uint64_t>(from) >= text.size()) throw std::out_of_range("String：字节下标越界");
        return static_cast<unsigned char>(text[static_cast<std::size_t>(from)]);
    case 13: return static_cast<std::int64_t>(boundaries(text).size()) - 1;
    default: throw std::invalid_argument("String：查询操作编号无效");
    }
    return result == Text::npos ? -1 : static_cast<std::int64_t>(result);
}
/**
 * @brief 生成切片、替换、大小写、填充或重复后的独立字符串。
 * @param text 原文本。
 * @param first 搜索文本、填充文本或待连接文本。
 * @param second 替换文本。
 * @param operation 操作编号。
 * @param offset 起点或目标长度。
 * @param count 长度或重复次数；切片长度负一表示余下全部。
 * @return 新字符串，不修改输入值。
 */
Text transform(Text text, Text first, Text second, std::int64_t operation, std::int64_t offset, std::int64_t count)
{
    switch (operation)
    {
    case 0:
    {
        auto start = clamp(offset, text.size());
        return text.substr(start, count < 0 ? Text::npos : clamp(count, text.size() - start));
    }
    case 1:
    case 2:
    case 3:
    {
        std::size_t begin = 0, end = text.size();
        if (operation != 3)
            while (begin < end && space(static_cast<unsigned char>(text[begin]))) ++begin;
        if (operation != 2)
            while (end > begin && space(static_cast<unsigned char>(text[end - 1]))) --end;
        return text.substr(begin, end - begin);
    }
    case 4:
    case 5:
        for (auto &c : text)
        {
            if (operation == 4 && c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (operation == 5 && c >= 'a' && c <= 'z') c -= 'a' - 'A';
        }
        return text;
    case 6:
    case 7:
    {
        if (first.empty()) return text;
        Text result;
        std::size_t at = 0, found;
        while ((found = text.find(first, at)) != Text::npos)
        {
            result.append(text, at, found - at);
            result += second;
            at = found + first.size();
            if (operation == 6) break;
        }
        result.append(text, at, Text::npos);
        return result;
    }
    case 8:
    {
        if (count < 0) throw std::invalid_argument("String：重复次数不能为负");
        if (text.empty() || count == 0) return {};
        if (static_cast<std::uint64_t>(count) > text.max_size() / text.size()) throw std::length_error("String：重复结果过长");
        Text result;
        result.reserve(text.size() * static_cast<std::size_t>(count));
        for (std::int64_t i = 0; i < count; ++i) result += text;
        return result;
    }
    case 9:
    {
        auto points = boundaries(text);
        Text result;
        result.reserve(text.size());
        for (auto i = points.size() - 1; i > 0; --i) result.append(text, points[i - 1], points[i] - points[i - 1]);
        return result;
    }
    case 10:
    case 11:
    {
        if (offset < 0) throw std::invalid_argument("String：填充长度不能为负");
        auto target = static_cast<std::uint64_t>(offset);
        if (target <= text.size() || first.empty()) return text;
        if (target > text.max_size()) throw std::length_error("String：填充结果过长");
        Text padding;
        auto needed = static_cast<std::size_t>(target) - text.size();
        padding.reserve(needed);
        while (padding.size() < needed) padding.append(first, 0, std::min(first.size(), needed - padding.size()));
        return operation == 10 ? padding + text : text + padding;
    }
    case 12: return text.size() >= first.size() && text.compare(0, first.size(), first) == 0 ? text.substr(first.size()) : text;
    case 13: return text.size() >= first.size() && text.compare(text.size() - first.size(), first.size(), first) == 0 ? text.substr(0, text.size() - first.size()) : text;
    case 14:
    {
        auto start = clamp(offset, text.size());
        text.insert(start, first);
        return text;
    }
    case 15:
    {
        auto start = clamp(offset, text.size());
        text.erase(start, count < 0 ? Text::npos : clamp(count, text.size() - start));
        return text;
    }
    case 16: return text.substr(text.size() - clamp(count, text.size()));
    case 17:
    {
        auto points = boundaries(text);
        auto start = clamp(offset, points.size() - 1);
        auto end = count < 0 ? points.size() - 1 : start + clamp(count, points.size() - 1 - start);
        return text.substr(points[start], points[end] - points[start]);
    }
    default: throw std::invalid_argument("String：转换操作编号无效");
    }
}
/**
 * @brief 按分隔符、空白、行或 UTF-8 字符生成独立字符串列表。
 * @param text 原文本。
 * @param separator 分隔文本，空分隔符按 UTF-8 字符分割。
 * @param operation 零为精确分隔，一为 ASCII 空白，二为行，三为字符。
 * @param keepEmpty 是否保留精确分隔及行分割产生的空项。
 * @return 拥有独立字符串值的列表。
 */
Memory::Vector<Text> split(Text text, Text separator, std::int64_t operation, bool keepEmpty)
{
    Memory::Vector<Text> result;
    if (operation == 3 || (operation == 0 && separator.empty()))
    {
        auto points = boundaries(text);
        for (std::size_t i = 1; i < points.size(); ++i) result.push_back(text.substr(points[i - 1], points[i] - points[i - 1]));
        return result;
    }
    std::size_t begin = 0, at = 0;
    while (at < text.size())
    {
        std::size_t width = 0;
        if (operation == 0 && text.compare(at, separator.size(), separator) == 0) width = separator.size();
        if (operation == 1 && space(static_cast<unsigned char>(text[at]))) width = 1;
        if (operation == 2 && (text[at] == '\r' || text[at] == '\n')) width = text[at] == '\r' && at + 1 < text.size() && text[at + 1] == '\n' ? 2 : 1;
        if (!width)
        {
            ++at;
            continue;
        }
        if (at > begin || (operation != 1 && keepEmpty)) result.push_back(text.substr(begin, at - begin));
        at += width;
        begin = at;
    }
    if (begin < text.size() || (operation != 1 && keepEmpty)) result.push_back(text.substr(begin));
    return result;
}
/**
 * @brief 预计算长度后一次分配连接文本列表。
 * @param separator 项目间的分隔文本。
 * @param items 待连接的独立字符串值。
 * @return 连接结果；空列表返回空字符串。
 */
Text join(Text separator, Memory::Vector<Text> items)
{
    Text result;
    std::size_t total = 0;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (items[i].size() > result.max_size() - total) throw std::length_error("String：连接结果过长");
        total += items[i].size();
        if (i)
        {
            if (separator.size() > result.max_size() - total) throw std::length_error("String：连接结果过长");
            total += separator.size();
        }
    }
    result.reserve(total);
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (i) result += separator;
        result += items[i];
    }
    return result;
}
}  // namespace
/**
 * @brief 注册 String 的独立值操作，不保存全局字符串状态。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有入口绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTyped<std::int64_t, Text, Text, std::int64_t, std::int64_t>("std::lang::__stringQuery", query, error)
           && registry.bindTyped<Text, Text, Text, Text, std::int64_t, std::int64_t, std::int64_t>("std::lang::__stringTransform", transform, error)
           && registry.bindTyped<Memory::Vector<Text>, Text, Text, std::int64_t, bool>("std::lang::__stringSplit", split, error)
           && registry.bindTyped<Text, Text, Memory::Vector<Text>>("std::lang::__stringJoinParts", join, error);
}
}  // namespace std_string_methods_glue
