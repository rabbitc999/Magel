#ifndef MAGEL_MACRO_SOURCE_HPP
#define MAGEL_MACRO_SOURCE_HPP
#include "allocator/magel_allocator.hpp"

namespace magel_macro
{
/**
 * @brief 为宏声明及匹配分支标记头部换行边界，不展开宏或替换任何实参。
 * @param source 送入生成解析器的源码，保持行数及非头部片段原文。
 * @note 标记是词法归一化，与插值及泛型标点归一化采用相同接入层。
 */
void markHeaderBoundaries(Memory::String &source);
}  // namespace magel_macro
#endif
