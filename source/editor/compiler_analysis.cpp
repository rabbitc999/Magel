#include "compiler_analysis.hpp"
#include <filesystem>
#include <fstream>
#include <utility>

namespace chali_editor
{
CompilerAnalysis compilerAnalysis;

/**
 * @brief 转义 JSON 必须转义的字节，不改变其他 UTF-8 内容。
 * @param text 原始文本。
 * @return 带引号的字符串。
 */
std::string jsonString(std::string_view text)
{
    static const char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (const unsigned char ch : text)
        if (ch == '"' || ch == '\\')
        {
            result += '\\';
            result += static_cast<char>(ch);
        }
        else if (ch < 32)
        {
            result += "\\u00";
            result += hex[ch >> 4];
            result += hex[ch & 15];
        }
        else
            result += static_cast<char>(ch);
    return result + "\"";
}

/**
 * @brief 重置一次完整编译的数据并设置收集开关。
 * @param enabled 是否启用分析输出。
 */
void CompilerAnalysis::begin(bool enabled)
{
    enabled_ = enabled;
    previewBytes_ = 0;
    types_.clear();
    macros_.clear();
}

/**
 * @brief 语义重放时撤销尚未完成一轮中的声明类型。
 */
void CompilerAnalysis::resetPass() { types_.clear(); }

/**
 * @brief 查询调用方是否请求了真实语义数据。
 * @return 分析开关的当前值。
 */
bool CompilerAnalysis::enabled() const { return enabled_; }

/**
 * @brief 保存类型提示，禁止将普通编译过程中的临时数据无条件缓存。
 * @param hint 类型及其原始声明位置。
 */
void CompilerAnalysis::addType(TypeHint hint)
{
    if (enabled_ && types_.size() < 100000) types_.push_back(std::move(hint));
}

/**
 * @brief 保存结构化宏预览，去重语义重放并限制总输出大小。
 * @param expansion 调用来源及模板树。
 */
void CompilerAnalysis::addMacro(MacroExpansion expansion)
{
    if (!enabled_ || macros_.size() >= 2048 || previewBytes_ + expansion.tree.size() > 16u * 1024u * 1024u) return;
    for (const auto &item : macros_)
        if (item.file == expansion.file && item.name == expansion.name && item.line == expansion.line && item.column == expansion.column && item.tree == expansion.tree) return;
    previewBytes_ += expansion.tree.size();
    macros_.push_back(std::move(expansion));
}

/**
 * @brief 写出类型提示与结构化展开记录，编译失败时明确标记数据不完整。
 * @param path 请求方提供的输出文件。
 * @param complete 最终语义检查是否通过。
 * @param error 接收文件错误。
 * @return 写入成功时返回真。
 */
bool CompilerAnalysis::write(const std::string &path, bool complete, std::string &error) const
{
    error.clear();
    std::ofstream out(std::filesystem::u8path(path), std::ios::binary);
    if (!out)
    {
        error = "无法写入编辑器编译分析文件";
        return false;
    }
    out << "{\"schemaVersion\":1,\"complete\":" << (complete ? "true" : "false") << ",\"typeHints\":[";
    bool first = true;
    for (const auto &hint : types_)
    {
        if (!first) out << ',';
        first = false;
        out << "{\"file\":" << jsonString(hint.file) << ",\"name\":" << jsonString(hint.name) << ",\"type\":" << jsonString(hint.type)
            << ",\"line\":" << hint.line << ",\"column\":" << hint.column << '}';
    }
    out << "],\"macros\":[";
    first = true;
    for (const auto &macro : macros_)
    {
        if (!first) out << ',';
        first = false;
        out << "{\"file\":" << jsonString(macro.file) << ",\"name\":" << jsonString(macro.name)
            << ",\"line\":" << macro.line << ",\"column\":" << macro.column << ",\"tree\":" << macro.tree << '}';
    }
    out << "]}\n";
    if (!out)
    {
        error = "编辑器编译分析文件写入未完成";
        return false;
    }
    return true;
}
}  // namespace chali_editor
