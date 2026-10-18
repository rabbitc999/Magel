#pragma once
#include "../../source/allocator/magel_allocator.hpp"
#include <cstdint>
#include <limits>

namespace stackvm::value_text
{
/**
 * @brief 默认文本表示的类型布局，不包含运行时地址或反射元数据。
 * @note 字段偏移以字节计；子布局保持声明顺序。布局只读取值，不取得其所有权。
 */
struct Shape
{
    char kind = 'X';
    Memory::String name;
    std::size_t offset = 0;
    std::size_t count = 0;
    std::size_t width = 8;
    std::size_t formatter = 0;  ///< 只读自定义方法在调用表中的一基索引，最大值表示禁止可变方法。
    Memory::Vector<Shape> children;
};

/**
 * @brief 将类型布局编码为长度前缀文本，类型名中的标点不需要转义。
 * @param shape 待编码的布局。
 * @retval 与平台地址无关的布局文本。
 */
inline Memory::String encode(const Shape &shape)
{
    const auto number = [](std::size_t value) { return Memory::toString(value) + ":"; };
    Memory::String out(1, shape.kind);
    out += number(shape.name.size()) + shape.name + number(shape.offset) + number(shape.count) + number(shape.width) + number(shape.formatter) + number(shape.children.size());
    for (const auto &child : shape.children) out += encode(child);
    return out;
}

/**
 * @brief 安全读取无符号长度，拒绝溢出及不完整分隔符。
 * @param text 布局文本。
 * @param at 当前游标，成功时前移。
 * @param value 接收解析结果。
 * @retval 读取成功时返回 true。
 */
inline bool number(const Memory::String &text, std::size_t &at, std::size_t &value)
{
    value = 0;
    const auto start = at;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9')
    {
        const auto digit = static_cast<std::size_t>(text[at++] - '0');
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) return false;
        value = value * 10 + digit;
    }
    return at > start && at < text.size() && text[at++] == ':';
}

/**
 * @brief 解码单个布局节点并限制深度与节点数。
 * @param text 编码布局。
 * @param at 输入输出游标。
 * @param shape 接收布局。
 * @param remaining 剩余节点预算。
 * @param depth 当前嵌套深度。
 * @retval 布局完整且满足预算时返回 true。
 */
inline bool decode(const Memory::String &text, std::size_t &at, Shape &shape, std::size_t &remaining, std::size_t depth = 0)
{
    if (at >= text.size() || !remaining || depth > 64) return false;
    --remaining;
    shape.kind = text[at++];
    std::size_t length = 0, children = 0;
    if (!number(text, at, length) || length > text.size() - at) return false;
    shape.name = text.substr(at, length);
    at += length;
    if (!number(text, at, shape.offset) || !number(text, at, shape.count) || !number(text, at, shape.width) || !number(text, at, shape.formatter) || !number(text, at, children) || children > remaining) return false;
    shape.children.clear();
    for (std::size_t index = 0; index < children; ++index)
    {
        Shape child;
        if (!decode(text, at, child, remaining, depth + 1)) return false;
        shape.children.push_back(std::move(child));
    }
    return true;
}
}  // namespace stackvm::value_text
