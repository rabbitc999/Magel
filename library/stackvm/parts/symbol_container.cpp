/**
 * @file symbol_container.cpp
 * @brief 无名变长符号容器头部与边界校验。
 */
namespace
{
/**
 * @brief 写 MCVM0005 容器。
 * @param program 程序。
 * @param payload 指令及元数据。
 * @param records 符号。
 * @param path 路径。
 * @param diagnostics 诊断。
 * @return 成功与否。
 */
bool writeSymbolContainer(const Program &program, const Memory::Vector<std::uint8_t> &payload,
                          const Memory::Vector<SymbolRecord> &records, const Memory::String &path,
                          Memory::Vector<Memory::String> *diagnostics)
{
#if STACKVM_WITH_HOST_IO

    BinaryWriter table;
    std::uint32_t exports = 0, imports = 0;
    for (const auto &s : records)
    {
        table.varUInt(s.logicalId);
        table.varUInt(s.physicalIndex);
        table.u8(s.kind);
        table.u8(s.flags);
        table.varUInt(s.offset);
        table.varUInt(s.size);
        exports += (s.kind & 16u) != 0;
        imports += (s.kind & 8u) != 0;
    }
    if (payload.size() + table.bytes.size() > kMaximumBinarySize - kSymbolHeaderSize)
    {
        binaryDiagnostic(diagnostics, "紧凑程序超过 512 MiB 上限");
        return false;
    }
    BinaryWriter body;
    body.raw(payload.data(), payload.size());
    body.raw(table.bytes.data(), table.bytes.size());
    BinaryWriter file;
    file.raw(kSymbolMagic, sizeof(kSymbolMagic));
    file.u32(5);
    file.u32(program.requiredFeatures);
    file.u32(exports);
    file.u32(imports);
    file.u32(static_cast<std::uint32_t>(payload.size()));
    file.u32(0);
    file.u32(static_cast<std::uint32_t>(kSymbolHeaderSize + payload.size()));
    file.u32(static_cast<std::uint32_t>(table.bytes.size()));
    file.u32(symbols::crc32(body.bytes.data(), body.bytes.size()));
    file.raw(body.bytes.data(), body.bytes.size());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(file.bytes.data()), static_cast<std::streamsize>(file.bytes.size()));
    out.close();
    if (!out)
    {
        binaryDiagnostic(diagnostics, "写紧凑程序失败：" + path);
        return false;
    }
    return true;

#else
    return false;
#endif
}
/**
 * @brief 解码新头部及符号表；按物理编号生成运行时内部键。
 * @param bytes 文件。
 * @param decoded 程序。
 * @param payload 指令及元数据。
 * @param diagnostics 诊断。
 * @return 成功与否。
 */
bool readSymbolContainer(const Memory::Vector<std::uint8_t> &bytes, Program &decoded,
                         Memory::Vector<std::uint8_t> &payload,
                         Memory::Vector<Memory::String> *diagnostics)
{
    BinaryReader header(bytes);
    const std::uint8_t *magic = nullptr;
    std::uint32_t version = 0, flags = 0, exports = 0, imports = 0, code = 0, constants = 0;
    std::uint32_t offset = 0, size = 0, crc = 0;
    auto fail = [&]() { binaryDiagnostic(diagnostics, header.error.empty() ?
        Memory::String("无名符号容器字段非法") : header.error); return false; };
    if (!header.take(8, &magic) || !header.u32(&version) || !header.u32(&flags) || !header.u32(&exports) || !header.u32(&imports) || !header.u32(&code) || !header.u32(&constants) || !header.u32(&offset) || !header.u32(&size) || !header.u32(&crc)) return fail();
    if (version != 5 || (flags & ~kKnownProgramFeatures) != 0 || constants != 0 || exports > kMaximumItemCount || imports > kMaximumItemCount || offset < kSymbolHeaderSize || offset > bytes.size() || size != bytes.size() - offset || code != offset - kSymbolHeaderSize || symbols::crc32(bytes.data() + kSymbolHeaderSize, bytes.size() - kSymbolHeaderSize) != crc) return fail();
    decoded.requiredFeatures = flags;
    if (!validateProgramFeatures(flags, diagnostics)) return false;
    Memory::Vector<std::uint8_t> table(bytes.begin() + offset, bytes.end());
    BinaryReader reader(table);
    Memory::Set<std::uint64_t> ids;
    while (reader.position < table.size())
    {
        SymbolRecord s;
        if (decoded.symbols.size() >= kMaximumItemCount || !reader.varUInt(&s.logicalId) || !reader.varUInt(&s.physicalIndex) || !reader.u8(&s.kind) || !reader.u8(&s.flags) || !reader.varUInt(&s.offset) || !reader.varUInt(&s.size))
        {
            header.error = reader.error;
            return fail();
        }
        if (s.physicalIndex != decoded.symbols.size() || !ids.insert(s.logicalId).second || (s.kind & ~31u) || s.flags || s.offset > kMaximumItemCount || s.size > kMaximumBinarySize) return fail();
        s.original = symbols::key(s.logicalId);
        decoded.symbolKeys.insert(s.original);
        decoded.symbols.push_back(std::move(s));
    }
    decoded.compactSymbols = true;
    payload.assign(bytes.begin() + kSymbolHeaderSize, bytes.begin() + offset);
    return true;
}
/**
 * @brief 验证条目与程序元数据一致并统计真实指令引用。
 * @param bytes 文件。
 * @param program 程序。
 * @param diagnostics 诊断。
 * @return 成功与否。
 */
bool validateSymbolProgram(const Memory::Vector<std::uint8_t> &bytes, Program &program,
                           Memory::Vector<Memory::String> *diagnostics)
{
    std::uint32_t exports = 0, imports = 0;
    Memory::Map<Memory::String, std::size_t> indices;
    for (std::size_t i = 0; i < program.symbols.size(); ++i)
    {
        const auto &s = program.symbols[i];
        indices.emplace(s.original, i);
        exports += (s.kind & 16u) != 0;
        imports += (s.kind & 8u) != 0;
        if ((s.kind & 2u) && (s.offset > program.code.size() || !program.labels.count(s.original) || program.labels.at(s.original) != s.offset))
        {
            binaryDiagnostic(diagnostics, "符号标签偏移与程序不一致");
            return false;
        }
        if ((s.kind & 16u) && !program.exports.count(s.original))
        {
            binaryDiagnostic(diagnostics, "符号导出标志与程序不一致");
            return false;
        }
    }
    BinaryReader header(bytes);
    header.position = 16;
    std::uint32_t expectedExports = 0, expectedImports = 0;
    if (!header.u32(&expectedExports) || !header.u32(&expectedImports) || exports != expectedExports || imports != expectedImports || exports != program.exports.size())
    {
        binaryDiagnostic(diagnostics, "头部导入导出数量不一致");
        return false;
    }
    for (const auto &i : program.code)
        for (const auto *name : { &i.name, &i.name2 })
            if (!name->empty()) ++program.symbols[indices.at(*name)].refCount;
    return true;
}
}  // namespace
