#ifndef CHALI_MACRO_FRAGMENTS_HPP
#define CHALI_MACRO_FRAGMENTS_HPP
#include "allocator/chali_allocator.hpp"
#include <string>
#include <vector>
#include <string_view>

namespace chali_macro
{
/**
 * @brief 保留完整实参原文及其在输入源码中的起点。
 */
struct SourceFragment
{
    std::string text;
    std::size_t line = 1;
    std::size_t column = 1;
};
/**
 * @brief 记录一个显式实参组的括号种类和独立实参。
 */
struct SourceGroup
{
    char opening = '(';
    std::vector<SourceFragment> arguments;
};
/**
 * @brief 保存尚未进行语法分类的宏调用，不把实参当文本展开。
 */
struct SourceInvocation
{
    std::string name;
    std::vector<SourceGroup> groups;
    std::vector<SourceFragment> types;
    std::size_t line = 1;
    std::size_t column = 1;
    bool explicitTypes = false;
};
/**
 * @brief 在词法归一化阶段收集宏调用原文，实际片段语法由生成解析器检查。
 */
class SourceFragments
{
public:
    /**
     * @brief 清除上次编译的调用记录。
     */
    void clear();
    /**
     * @brief 结束当前输入的词素坐标查找，保留片段记录供后续语义阶段使用。
     */
    void finishParsing();
    /**
     * @brief 将调用词素归一化为内部引用，保留原始组内容、行数及后续列号。
     * @param source 待处理源码，仅将调用区间改为内部词法标记。
     * @param error 接收括号及词法错误，成功时清空。
     * @param firstLine 当前片段原文在完整输入中的起始行。
     * @param firstColumn 起始行的原始列号。
     * @param anchored 是否在词素中记录稳定调用编号，供后续普通泛型归一化复制。
     * @return 捕获成功时返回真。
     */
    bool capture(Memory::String &source, std::string &error, std::size_t firstLine = 1, std::size_t firstColumn = 1, bool anchored = false);
    /**
     * @brief 将已捕获来源移到追加的原文映射区，不受后续源码归一化行号变化影响。
     * @param offset 原文映射追加到当前映射中的起始偏移。
     */
    void rebase(std::size_t offset);
    /**
     * @brief 校验内部调用词素中的稳定编号及原宏名。
     * @param token 含美元前缀及内部编号的完整词素。
     * @return 对应调用编号，词素无效时返回 size_t 最大值。
     */
    std::size_t resolveMarker(std::string_view token) const;
    /**
     * @brief 根据内部编号取得完整调用记录。
     * @param index 调用编号。
     * @return 对应记录，无效编号抛出 out_of_range。
     */
    const SourceInvocation &at(std::size_t index) const;
    /**
     * @brief 在当前解析输入的捕获批次内查找调用词素。
     * @param line 原始输入行。
     * @param column 原始输入列。
     * @return 对应调用编号，不存在时返回 size_t 的最大值。
     */
    std::size_t find(std::size_t line, std::size_t column) const;

private:
    std::vector<SourceInvocation> invocations_;
    std::size_t activeBegin_ = 0;
    bool active_ = false;
};

/**
 * @brief 当前前端编译会话的原始片段池，递归片段解析共用同一编号空间。
 */
extern SourceFragments sourceFragments;
}  // namespace chali_macro
#endif
