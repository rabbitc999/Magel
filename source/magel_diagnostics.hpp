#pragma once
#include "allocator/magel_allocator.hpp"
#include <cstdio>
#include <fstream>
/**
 * @brief 编译器诊断的 JSON 编码及稳定错误码。
 */
namespace magel_diagnostics
{
/**
 * @brief 标准 JSON 字符串转义。
 * @param text 原文。
 * @return JSON 字面量。
 */
inline Memory::String quote(const Memory::String &text)
{
    Memory::String out = "\"";
    const char *hex = "0123456789abcdef";
    for (unsigned char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += char(c);
        }
        else if (c < 32)
        {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        }
        else
            out += char(c);
    }
    return out + "\"";
}
/**
 * @brief 旧语义错误归入稳定类别，新检查可直接提供专用错误码。
 * @param message 错误文本。
 * @return 稳定错误码。
 */
inline const char *code(const Memory::String &message)
{
    if (message.find("[MGL1601]") != Memory::String::npos) return "MGL1601";
    if (message.find("[MGL1205]") != Memory::String::npos) return "MGL1205";
    if (message.find("[MGL1301]") != Memory::String::npos) return "MGL1301";
    if (message.find("[MGL1204]") != Memory::String::npos) return "MGL1204";
    if (message.find("[MGL") != Memory::String::npos) return "MGL1300";
    if ((message.find("只读") != Memory::String::npos || message.find("read-only") != Memory::String::npos || message.find("読み取り専用") != Memory::String::npos) || message.find("const") != Memory::String::npos) return "MGL1203";
    if (message.find("格式") != Memory::String::npos || message.find("占位符") != Memory::String::npos) return "MGL1400";
    if (message.find("泛型") != Memory::String::npos || message.find("类型实参") != Memory::String::npos) return "MGL1500";
    if (message.find("match") != Memory::String::npos || message.find("模式") != Memory::String::npos) return "MGL1600";
    return "MGL1200";
}
/**
 * @brief 可序列化的前端诊断，文件和位置已映射到原始源码。
 */
struct Diagnostic
{
    Memory::String message, file;
    std::size_t line = 1, column = 1;
    bool warning = false;
};
/**
 * @brief 将全部诊断写入一个 JSON 文档。
 * @param diagnostics 原始源码诊断列表。
 * @return 无；写入 stdout。
 */
inline void reportMany(const Memory::Vector<Diagnostic> &diagnostics)
{
    std::fputs("{\"schemaVersion\":1,\"diagnostics\":[", stdout);
    bool first = true;
    for (const auto &diagnostic : diagnostics)
    {
        const auto &message = diagnostic.message;
        const auto line = diagnostic.line ? diagnostic.line : 1u, column = diagnostic.column ? diagnostic.column : 1u;
        Memory::String source;
        std::ifstream input(Memory::u8path(diagnostic.file));
        for (std::size_t i = 0; i < line; ++i)
            if (!std::getline(input, source))
            {
                source.clear();
                break;
            }
        if (!source.empty() && source.back() == '\r') source.pop_back();
        const auto note = message.find("\nnote: ");
        const auto notes = note == Memory::String::npos ? Memory::String("[]") : "[" + quote(message.substr(note + 7)) + "]";
        if (!first) std::fputc(',', stdout);
        first = false;
        std::printf("{\"code\":%s,\"severity\":%s,\"message\":%s,\"span\":{\"file\":%s,\"line\":%zu,\"column\":%zu,\"endLine\":%zu,\"endColumn\":%zu},\"source\":%s,\"notes\":%s}",
                    quote(code(message)).c_str(), diagnostic.warning ? "\"warning\"" : "\"error\"", quote(note == Memory::String::npos ? message : message.substr(0, note)).c_str(),
                    quote(diagnostic.file).c_str(), line, column, line, source.size() >= column ? source.size() + 1u : column + 1u,
                    quote(source).c_str(), notes.c_str());
    }
    std::puts("]}");
}
/**
 * @brief 输出单条兼容诊断。
 * @param id 错误码。
 * @param message 消息。
 * @param file 文件。
 * @param line 行。
 * @param column 列。
 * @return 无。
 */
inline void report(const char *id, const Memory::String &message, Memory::String file,
                   std::size_t line = 1, std::size_t column = 1)
{
    // 解析和预处理阶段的专用错误码也必须保留。
    if (id && Memory::String(id) != code(message))
    {
        Memory::String source;
        std::ifstream input(Memory::u8path(file));
        if (!line) line = 1;
        if (!column) column = 1;
        for (std::size_t i = 0; i < line; ++i)
            if (!std::getline(input, source))
            {
                source.clear();
                break;
            }
        if (!source.empty() && source.back() == '\r') source.pop_back();
        std::printf("{\"schemaVersion\":1,\"diagnostics\":[{\"code\":%s,\"severity\":\"error\",\"message\":%s,\"span\":{\"file\":%s,\"line\":%zu,\"column\":%zu,\"endLine\":%zu,\"endColumn\":%zu},\"source\":%s,\"notes\":[]}]}\n",
                    quote(id).c_str(), quote(message).c_str(), quote(file).c_str(), line, column, line,
                    source.size() >= column ? source.size() + 1u : column + 1u, quote(source).c_str());
        return;
    }
    reportMany({ { message, file, line, column } });
}
/**
 * @brief 成功检查的稳定空诊断输出。
 * @return 无；写入 stdout。
 */
inline void success() { std::puts("{\"schemaVersion\":1,\"diagnostics\":[]}"); }
/**
 * @brief 编码工具命令诊断，保持编译检查的 code、span、source、notes 字段。
 * @param id 稳定错误码。
 * @param message 双语选择后的消息。
 * @param notes 候选或补充说明。
 * @return 可放入 diagnostics 数组的 JSON 对象。
 */
inline Memory::String toolDiagnostic(const char *id, const Memory::String &message,
                                     const Memory::Vector<Memory::String> &notes = {})
{
    Memory::String text = "{\"code\":" + quote(id) + ",\"severity\":\"error\",\"message\":" + quote(message) + ",\"span\":{\"file\":\"<命令行>\",\"line\":1,\"column\":1,\"endLine\":1,\"endColumn\":1},\"source\":\"\",\"notes\":[";
    for (std::size_t at = 0; at < notes.size(); ++at)
    {
        if (at) text += ',';
        text += quote(notes[at]);
    }
    return text + "]}";
}
}  // namespace magel_diagnostics
