/**
 * @file toml_source.hpp
 * @brief TOML 原文、注释和可替换值区间的内部保存协议。
 */
#pragma once
#include "toml.hpp"

namespace toml::source_detail
{
/**
 * @brief 原文中真实注释的字节区间，不包含行结束符。
 */
struct Comment
{
    std::size_t begin;
    std::size_t end;
};
/**
 * @brief 将一个旧值区间替换为新值文本。
 */
struct Replacement
{
    std::size_t begin;
    std::size_t end;
    std::string text;
};
/**
 * @brief 保存解析原文和独立值快照，支持修改后的注释保留。
 */
struct Document
{
    std::string text;
    Value original;
    std::vector<Comment> comments;
    /**
     * @brief 使用原文区间保存修改后的文档；结构变动时保留全部注释内容。
     * @param value 修改后的文档。
     * @return 可再次解析且注释不会重复的 TOML 文本。
     */
    std::string render(const Value &value) const;
    /**
     * @brief 在形状相同的值树中收集互不重叠的修改区间。
     * @param before 原始值及原文位置。
     * @param after 当前值。
     * @param changes 接收修改区间。
     * @return 可直接替换原文时返回真。
     */
    static bool collect(const Value &before, const Value &after, std::vector<Replacement> &changes);
};
}  // namespace toml::source_detail
