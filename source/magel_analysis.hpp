#pragma once
#include "magel_incremental.hpp"
#include "magel_diagnostics.hpp"
#include "json.hpp"

/**
 * @brief 编辑器检查的未保存源码与依赖记录，不修改工程文件。
 */
namespace magel_analysis
{
/**
 * @brief 读取未保存文档覆盖表。
 * @param path JSON 文件。
 * @return 规范路径到源码的映射。
 */
inline Memory::Map<Memory::String, Memory::String> overlays(const Memory::String &path)
{
    Memory::Map<Memory::String, Memory::String> result;
    if (path.empty()) return result;
    std::ifstream in(Memory::u8path(path), std::ios::binary);
    if (!in) throw std::runtime_error("无法读取编辑器源码覆盖表");
    const Memory::String text((std::istreambuf_iterator<char>(in)), {});
    const auto value = json::parse(text);
    if (!value.ok() || !value.value->asObject()) throw std::runtime_error("编辑器源码覆盖表必须是 JSON 对象");
    for (const auto &entry : value.value->asObject()->members())
    {
        if (!entry.value.asString()) throw std::runtime_error("编辑器源码覆盖项必须是字符串");
        result[magel_incremental::canonical(entry.key)] = *entry.value.asString();
    }
    return result;
}
/**
 * @brief 写出本次检查实际读取的依赖，供 LSP 复用诊断。
 * @param path 输出文件。
 * @param dependencies 依赖列表。
 * @param complete 模块装载是否完整。
 */
inline void dependencies(const Memory::String &path, const Memory::Vector<Memory::String> &dependencies,
                         bool complete)
{
    if (path.empty()) return;
    magel_incremental::replaceCacheFile(Memory::u8path(path), [&](std::ostream &out) {
        out << "{\"complete\":" << (complete ? "true" : "false") << ",\"dependencies\":[";
        bool first = true;
        for (const auto &file : dependencies)
        {
            if (!first) out << ',';
            first = false;
            out << magel_diagnostics::quote(file);
        }
        out << "]}";
    });
}
}  // namespace magel_analysis
