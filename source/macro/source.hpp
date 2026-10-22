#ifndef CHALI_MACRO_SOURCE_HPP
#define CHALI_MACRO_SOURCE_HPP
#include "allocator/chali_allocator.hpp"
#include <string>
#include <vector>

namespace chali_macro
{
/**
 * @brief 为宏声明及匹配分支标记头部换行边界，不展开宏或替换任何实参。
 * @param source 送入生成解析器的源码，保持行数及非头部片段原文。
 * @note 标记是词法归一化，与插值及泛型标点归一化采用相同接入层。
 */
void markHeaderBoundaries(Memory::String &source);
/**
 * @brief 为单行宏标记头部与行末边界，正文仍交给生成解析器建立 AST。
 * @param source 原始源码。
 * @param error 接收缺少正文或不平衡形参组错误。
 * @return 所有单行宏边界有效时返回真。
 */
bool markReplacementBoundaries(Memory::String &source, std::string &error);
/**
 * @brief 临时隐藏宏定义，避免普通闭包和泛型的源码归一化提前处理未选中模板。
 */
class ProtectedDefinitions
{
public:
    /**
     * @brief 将完整定义替换为保留行结构的注释占位。
     * @param source 待归一化源码。
     * @param error 接收不完整或嵌套定义诊断。
     * @return 隐藏成功时返回真。
     */
    bool hide(Memory::String &source, std::string &error);
    /**
     * @brief 在普通源码归一化完成后恢复相同的已解析模板输入。
     * @param source 包含占位的源码。
     * @param error 接收占位被破坏的错误。
     * @return 恢复成功时返回真。
     */
    bool restore(Memory::String &source, std::string &error) const;

private:
    std::vector<std::pair<Memory::String, Memory::String>> definitions_;
};
}  // namespace chali_macro
#endif
