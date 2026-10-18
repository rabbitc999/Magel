/**
 * @file symbol_build.cpp
 * @brief 编译期符号收集、频率排序与 PC 映射输出。
 */
namespace
{
/**
 * @brief 收集全部语义名字，检查哈希碰撞并按引用次数降序排序。
 * @param program 原始程序。
 * @param records 条目。
 * @param indices 名字到物理索引。
 * @param diagnostics 诊断。
 * @return 成功与否。
 */
bool buildSymbols(const Program &program, Memory::Vector<SymbolRecord> &records,
                  Memory::Map<Memory::String, std::uint64_t> &indices,
                  Memory::Vector<Memory::String> *diagnostics)
{
    Memory::Map<Memory::String, SymbolRecord> names;
    auto add = [&](const Memory::String &name, std::uint8_t kind = 0u) -> SymbolRecord & {
        auto &s = names[name];
        s.original = name;
        s.kind |= kind;
        return s;
    };
    for (const auto &i : program.code)
    {
        if (!i.name.empty())
        {
            auto &s = add(i.name, (i.op == Op::Call && !program.labels.count(i.name)) ? 8u : 1u);
            ++s.refCount;
        }
        if (!i.name2.empty()) ++add(i.name2, 1u).refCount;
    }
    for (const auto &[name, offset] : program.labels)
    {
        auto &s = add(name, 2u);
        s.offset = offset;
        const auto *location = program.debug.lookup(offset);
        if (location) s.line = location->line > 0 ? location->line :
                                                    static_cast<std::int32_t>(std::min(location->quadLine, std::size_t(INT32_MAX)));
    }
    for (const auto &[name, type] : program.types)
    {
        add(name, 1u).size = type.size;
        add(type.name, 4u);
    }
    for (const auto &[name, params] : program.parameters)
    {
        add(name, 2u);
        for (const auto &p : params) add(p, 1u);
    }
    for (const auto &name : program.exports) add(name, 16u);
    if (names.size() > kMaximumItemCount)
    {
        binaryDiagnostic(diagnostics, "符号数超过格式上限");
        return false;
    }
    Memory::Map<std::uint64_t, Memory::String> ids;
    for (auto &[name, s] : names)
    {
        if (name.size() > kMaximumStringSize)
        {
            binaryDiagnostic(diagnostics, "符号名超过长度上限");
            return false;
        }
        s.logicalId = symbols::logicalId(name);
        if (!ids.emplace(s.logicalId, name).second)
        {
            binaryDiagnostic(diagnostics, "FNV-1a 符号编号碰撞");
            return false;
        }
        records.push_back(std::move(s));
    }
    std::sort(records.begin(), records.end(), [](const auto &a, const auto &b) {
        return a.refCount != b.refCount ? a.refCount > b.refCount : a.logicalId < b.logicalId;
    });
    for (std::size_t i = 0; i < records.size(); ++i)
    {
        records[i].physicalIndex = i;
        indices.emplace(records[i].original, i);
    }
    return true;
}
/**
 * @brief 按 JSON 标准转义字符串，不依赖运行时文件。
 * @param value 字符串。
 * @return 带双引号的 JSON 字面量。
 */
Memory::String symbolJsonString(const Memory::String &value)
{
    Memory::String out = "\"";
    const char *hex = "0123456789abcdef";
    for (unsigned char c : value)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += static_cast<char>(c);
        }
        else if (c < 32)
        {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        }
        else
            out += static_cast<char>(c);
    }
    return out + "\"";
}
}  // namespace
/**
 * @brief 将原始符号及频率排序写入 PC JSON 映射。
 * @param program 原始程序。
 * @param path 输出路径。
 * @param module 模块名。
 * @param version 版本。
 * @param diagnostics 错误诊断。
 * @return 成功返回 true。
 */
bool saveSymbolMap(const Program &program, const Memory::String &path,
                   const Memory::String &module, const Memory::String &version,
                   Memory::Vector<Memory::String> *diagnostics)
{
#if STACKVM_WITH_HOST_IO

    if (program.compactSymbols)
    {
        binaryDiagnostic(diagnostics, "无名输入无法恢复原始映射");
        return false;
    }
    Memory::Vector<SymbolRecord> records;
    Memory::Map<Memory::String, std::uint64_t> indices;
    if (!buildSymbols(program, records, indices, diagnostics)) return false;
    std::ofstream out(path, std::ios::binary);
    out << "{\"module\":" << symbolJsonString(module.empty() ? program.name : module)
        << ",\"version\":" << symbolJsonString(version) << ",\"symbols\":[";
    bool first = true;
    for (const auto &s : records)
    {
        if (!first) out << ',';
        first = false;
        // 64 位逻辑编号以十进制字符串表示，避免 JSON 消费者浮点精度损失。
        out << "{\"logical_id\":\"" << s.logicalId << "\",\"physical_idx\":" << s.physicalIndex
            << ",\"original\":" << symbolJsonString(s.original) << ",\"kind\":" << unsigned(s.kind)
            << ",\"line\":" << s.line << ",\"ref_count\":" << s.refCount << '}';
    }
    out << "]}\n";
    out.close();
    if (!out)
    {
        binaryDiagnostic(diagnostics, "写符号映射失败：" + path);
        return false;
    }
    return true;

#else
    return false;
#endif
}
