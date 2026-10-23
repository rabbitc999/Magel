/**
 * @file toml_source.cpp
 * @brief 在值编辑与结构变动后保存 TOML 注释，保持重复保存幂等。
 */
#include "toml_source.hpp"
#include <algorithm>

namespace toml::source_detail
{
/**
 * @brief 逐层比较值树，对未改变容器形状的编辑生成原文替换。
 * @param before 原始值及原文位置。
 * @param after 当前值。
 * @param changes 接收互不嵌套的修改。
 * @return 能保留原文布局时返回真。
 */
bool Document::collect(const Value &before, const Value &after, std::vector<Replacement> &changes)
{
    if (before.inlineDump() == after.inlineDump()) return true;
    if (before.isTable() && after.isTable())
    {
        const auto &oldTable = *before.asTable();
        const auto &newTable = *after.asTable();
        if (oldTable.size() != newTable.size()) return false;
        auto updated = newTable.begin();
        for (const auto &member : oldTable)
        {
            if (updated->key != member.key || !collect(member.value, updated->value, changes)) return false;
            ++updated;
        }
        return true;
    }
    if (before.isArray() && after.isArray())
    {
        const auto &oldItems = *before.asArray();
        const auto &newItems = *after.asArray();
        if (oldItems.size() != newItems.size()) return false;
        for (std::size_t i = 0; i < oldItems.size(); ++i)
            if (!collect(oldItems[i], newItems[i], changes)) return false;
        return true;
    }
    if (before.sourceBegin_ >= before.sourceEnd_) return false;
    changes.push_back({ before.sourceBegin_, before.sourceEnd_, after.inlineDump() });
    return true;
}

/**
 * @brief 保留未修改文档的全部原文，并在安全的值区间内更新内容。
 * @param value 当前值树。
 * @return 保留注释内容且可重复解析保存的文档。
 */
std::string Document::render(const Value &value) const
{
    std::vector<Replacement> changes;
    bool patchable = collect(original, value, changes);
    for (const auto &change : changes)
    {
        if (change.end > text.size()) patchable = false;
        for (const auto &comment : comments)
            if (comment.begin >= change.begin && comment.begin < change.end) patchable = false;
    }
    if (patchable)
    {
        std::sort(changes.begin(), changes.end(), [](const Replacement &a, const Replacement &b) { return a.begin > b.begin; });
        std::string result = text;
        for (const auto &change : changes) result.replace(change.begin, change.end - change.begin, change.text);
        return result;
    }
    Value canonical = value;
    canonical.sourceDocument_.reset();
    std::string result;
    for (const auto &comment : comments)
    {
        result.append(text, comment.begin, comment.end - comment.begin);
        result += '\n';
    }
    result += canonical.dump();
    return result;
}
}  // namespace toml::source_detail
