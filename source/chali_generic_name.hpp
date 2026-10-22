#include "allocator/chali_allocator.hpp"
#ifndef CHALI_GENERIC_NAME_HPP
#define CHALI_GENERIC_NAME_HPP

#include <cctype>
#include <cstdint>

namespace chali_generic_name
{
/**
 * @brief 为归一化类型实参生成工具链和宿主胶水共用的稳定符号。
 * @param argument 完整的类型拼写。
 * @return 简单类型保留原符号，复杂类型使用长度受控的前缀和完整类型指纹。
 */
inline Memory::String argumentSymbol(const Memory::String &argument)
{
    const Memory::String marker = "__chl_";
    bool simple = !argument.empty() && argument.size() <= 48u && argument.compare(0, marker.size(), marker) != 0;
    for (unsigned char value : argument) simple = simple && (std::isalnum(value) || value == '_');
    if (simple) return argument;
    std::uint64_t fingerprint = UINT64_C(14695981039346656037);
    Memory::String readable;
    for (unsigned char value : argument)
    {
        fingerprint = (fingerprint ^ value) * UINT64_C(1099511628211);
        if (readable.size() < 48u) readable += std::isalnum(value) || value == '_' ? static_cast<char>(value) : '_';
    }
    Memory::String result = marker;
    const char *digits = "0123456789abcdef";
    for (int shift = 60; shift >= 0; shift -= 4) result += digits[(fingerprint >> shift) & 15u];
    return result + "_" + readable;
}
}  // namespace chali_generic_name
#endif
