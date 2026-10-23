/**
 * @file program_binary.cpp
 * @brief 指令程序的二进制编码和解码。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 识别旧格式与无名格式魔数。
 * @param path 路径。
 * @return 是否为字节码。
 */
bool isProgramBinary(const Memory::String &path)
{
#if STACKVM_WITH_HOST_IO

    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::uint8_t magic[sizeof(kProgramMagic)] = {};
    stream.read(reinterpret_cast<char *>(magic), static_cast<std::streamsize>(sizeof(magic)));
    return stream.gcount() == static_cast<std::streamsize>(sizeof(magic)) && (std::equal(std::begin(magic), std::end(magic), std::begin(kProgramMagic)) || std::equal(std::begin(magic), std::end(magic), std::begin(kSymbolMagic)));

#else
    return false;
#endif
}

/**
 * @brief 序列化旧格式或无名变长格式。
 * @param program 程序。
 * @param path 输出。
 * @param diagnostics 诊断。
 * @param compressSymbols 无名符号开关。
 * @return 写入成功返回 true。
 */
bool saveProgramBinary(const Program &program, const Memory::String &path,
                       Memory::Vector<Memory::String> *diagnostics, bool compressSymbols)
{
#if STACKVM_WITH_HOST_IO

    if (!validateProgramMetadata(program, diagnostics)) return false;
    if (!program.ok())
    {
        binaryDiagnostic(diagnostics, "不能序列化带组装诊断的程序");
        return false;
    }
    if (program.compactSymbols)
    {
        binaryDiagnostic(diagnostics, "压缩输入不能再次压缩");
        return false;
    }
    Memory::Vector<SymbolRecord> records;
    Memory::Map<Memory::String, std::uint64_t> indices;
    if (compressSymbols && !buildSymbols(program, records, indices, diagnostics)) return false;
    BinaryWriter payload;
    if (compressSymbols) payload.symbolIndices = &indices;
    if (compressSymbols)
        payload.varUInt(symbols::logicalId(program.name));
    else
        payload.text(program.name);
    if (!payload.count(program.code.size(), "指令数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (std::size_t instructionIndex = 0u; instructionIndex < program.code.size(); ++instructionIndex)
    {
        const Instruction &instruction = program.code[instructionIndex];
        if (static_cast<std::uint16_t>(instruction.op) > static_cast<std::uint16_t>(Op::StoreAt) || static_cast<std::uint8_t>(instruction.format) > static_cast<std::uint8_t>(PrintFormat::Text))
        {
            binaryDiagnostic(diagnostics, "程序含无法序列化的指令枚举值");
            return false;
        }
        std::uint64_t doubleBits = 0u;
        std::memcpy(&doubleBits, &instruction.doubleValue, sizeof(doubleBits));
        std::uint8_t operands = 0u;
        if (instruction.format != PrintFormat::Signed) operands |= 1u << 0u;
        if (instruction.unsignedOp || instruction.isFloat) operands |= 1u << 1u;
        if (instruction.intValue != 0) operands |= 1u << 2u;
        if (doubleBits != 0u) operands |= 1u << 3u;
        if (instruction.quadIndex != instructionIndex) operands |= 1u << 4u;
        if (!instruction.name.empty()) operands |= 1u << 5u;
        if (!instruction.name2.empty()) operands |= 1u << 6u;
        if (!instruction.textValue.empty()) operands |= 1u << 7u;

        payload.u8(static_cast<std::uint8_t>(instruction.op));
        payload.u8(operands);
        if ((operands & (1u << 0u)) != 0u)
            payload.u8(static_cast<std::uint8_t>(instruction.format));
        if ((operands & (1u << 1u)) != 0u)
            payload.u8(static_cast<std::uint8_t>((instruction.unsignedOp ? 1u : 0u) | (instruction.isFloat ? 2u : 0u)));
        if ((operands & (1u << 2u)) != 0u) payload.varInt(instruction.intValue);
        if ((operands & (1u << 3u)) != 0u) payload.u64(doubleBits);
        if ((operands & (1u << 4u)) != 0u) payload.varUInt(instruction.quadIndex);
        if ((operands & (1u << 5u)) != 0u) payload.symbolIndices ? payload.symbol(instruction.name) : payload.varText(instruction.name);
        if ((operands & (1u << 6u)) != 0u) payload.symbolIndices ? payload.symbol(instruction.name2) : payload.varText(instruction.name2);
        if ((operands & (1u << 7u)) != 0u) payload.varText(instruction.textValue);
    }

    if (!payload.count(program.labels.size(), "标签数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &[name, index] : program.labels)
    {
        if (index > program.code.size())
        {
            binaryDiagnostic(diagnostics, "程序含越过指令末尾的标签：" + name);
            return false;
        }
        payload.symbol(name);
        payload.number(static_cast<std::uint64_t>(index));
    }

    if (!payload.count(program.types.size(), "类型数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &[variable, type] : program.types)
    {
        payload.symbol(variable);
        payload.symbol(type.name);
        compressSymbols ? payload.varInt(type.bits) : payload.u32(static_cast<std::uint32_t>(static_cast<std::int32_t>(type.bits)));
        payload.u8(static_cast<std::uint8_t>((type.isUnsigned ? 1u : 0u) | (type.isFloat ? 2u : 0u)));
        payload.number(static_cast<std::uint64_t>(type.size));
    }

    if (!payload.count(program.parameters.size(), "形参表数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &[label, parameters] : program.parameters)
    {
        payload.symbol(label);
        if (!payload.count(parameters.size(), "形参数"))
        {
            binaryDiagnostic(diagnostics, payload.error);
            return false;
        }
        for (const Memory::String &parameter : parameters) payload.symbol(parameter);
    }
    if (!payload.count(program.exports.size(), "导出标签数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const Memory::String &label : program.exports)
    {
        if (program.labels.find(label) == program.labels.end())
        {
            binaryDiagnostic(diagnostics, "导出项不是程序标签：" + label);
            return false;
        }
        payload.symbol(label);
    }
    /* 调试信息节（v4）：指令下标 → 源码位置。没有调试信息也写 0 条，节始终在场。 */
    if (!payload.count(compressSymbols ? 0u : program.debug.entries.size(), "调试信息条数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &entry : (compressSymbols ? DebugInfo{}.entries : program.debug.entries))
    {
        payload.varUInt(static_cast<std::uint64_t>(entry.first));
        payload.varText(entry.second.function);
        payload.varText(entry.second.file);
        payload.varUInt(entry.second.line > 0 ? static_cast<std::uint64_t>(entry.second.line) : 0u);
        payload.varUInt(static_cast<std::uint64_t>(entry.second.quadLine));
    }
    // 新版产物直接携带内部执行指令；原始 VM 指令作为精确观察与回退序列。
    auto prepared = execution_detail::lowerRegisters(program);
    if (!payload.count(prepared.size(), "内部指令数"))
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    for (const auto &internal : prepared)
    {
        payload.varUInt(internal.left);
        payload.u8(static_cast<std::uint8_t>(internal.operation));
        payload.u8(static_cast<std::uint8_t>(internal.kind));
        payload.varUInt(internal.span);
        payload.varInt(internal.immediate);
        payload.varInt(internal.bits);
        payload.u8((internal.literal ? 1 : 0) | (internal.unsignedResult ? 2 : 0) | (internal.jumpOnZero ? 4 : 0) | (internal.storeResult ? 8 : 0) | (internal.floatOperation ? 16 : 0));
        payload.varUInt(internal.target == execution_detail::absent ? 0 : internal.target + 1);
        payload.varUInt(internal.destination2 == execution_detail::absent ? 0 : internal.destination2 + 1);
    }
    if (program.requiredFeatures & kFeatureReflection)
    {
        Memory::Vector<std::uint8_t> metadata;
        Memory::String error;
        if (!reflection::encode(program.reflection, metadata, &error))
        {
            binaryDiagnostic(diagnostics, error);
            return false;
        }
        payload.raw(metadata.data(), metadata.size());
    }
    if (!payload.error.empty())
    {
        binaryDiagnostic(diagnostics, payload.error);
        return false;
    }
    if (compressSymbols) return writeSymbolContainer(program, payload.bytes, records, path, diagnostics);
    const std::uint16_t flags = static_cast<std::uint16_t>(program.requiredFeatures);
    const auto &stored = payload.bytes;
    if (stored.size() > kMaximumBinarySize - kProgramHeaderSize)
    {
        binaryDiagnostic(diagnostics, "二进制程序过大（上限 512 MiB）");
        return false;
    }

    BinaryWriter file;
    file.raw(kProgramMagic, sizeof(kProgramMagic));
    file.u16(kProgramFormatVersion);
    file.u16(flags);
    file.u64(static_cast<std::uint64_t>(stored.size()));
    file.u32(binaryChecksum(stored.data(), stored.size()));
    file.raw(stored.data(), stored.size());

    std::ofstream stream(path, std::ios::binary);
    if (!stream)
    {
        binaryDiagnostic(diagnostics, "写不了二进制程序：" + path);
        return false;
    }
    stream.write(reinterpret_cast<const char *>(file.bytes.data()),
                 static_cast<std::streamsize>(file.bytes.size()));
    if (!stream)
    {
        binaryDiagnostic(diagnostics, "写二进制程序失败：" + path);
        return false;
    }
    return true;

#else
    return false;
#endif
}

/**
 * @brief 路径版与内存版共用的解码实现（定义在本文件下方）。
 */
static bool decodeProgramBinary(const Memory::Vector<std::uint8_t> &fileBytes,
                                const Memory::String &label, Program &program,
                                Memory::Vector<Memory::String> *diagnostics);

/**
 * @brief 从内存字节加载程序；声明与文档见 `stackvm.hpp`。
 */
bool loadProgramBinary(const void *bytes, std::size_t size, Program &program,
                       Memory::Vector<Memory::String> *diagnostics)
{
    if (bytes == nullptr || size == 0u)
    {
        binaryDiagnostic(diagnostics, "空的内存程序映像");
        return false;
    }
    const std::uint8_t *first = static_cast<const std::uint8_t *>(bytes);
    Memory::Vector<std::uint8_t> fileBytes(first, first + size);
    return decodeProgramBinary(fileBytes, "<内存映像>", program, diagnostics);
}

/**
 * @brief 校验并加载兼容格式（按路径），失败不修改输出程序。
 * @param path 输入。
 * @param program 程序。
 * @param diagnostics 诊断。
 * @return 成功返回 true。
 */
bool loadProgramBinary(const Memory::String &path, Program &program,
                       Memory::Vector<Memory::String> *diagnostics)
{
    Memory::Vector<std::uint8_t> fileBytes;
    if (!readBinaryFile(path, &fileBytes, diagnostics)) return false;
    return decodeProgramBinary(fileBytes, path, program, diagnostics);
}

/**
 * @brief 路径版与内存版共用的解码实现。
 */
static bool decodeProgramBinary(const Memory::Vector<std::uint8_t> &fileBytes,
                                const Memory::String &label, Program &program,
                                Memory::Vector<Memory::String> *diagnostics)
{
    const bool compressed = fileBytes.size() >= 8 && std::equal(std::begin(kSymbolMagic), std::end(kSymbolMagic), fileBytes.begin());
    Program decoded;
    Memory::Vector<std::uint8_t> payloadBytes;
    if (compressed)
    {
        if (!readSymbolContainer(fileBytes, decoded, payloadBytes, diagnostics)) return false;
    }
    if (!compressed)
    {
        if (fileBytes.size() < kProgramHeaderSize || !std::equal(std::begin(kProgramMagic), std::end(kProgramMagic), fileBytes.begin()))
        {
            binaryDiagnostic(diagnostics, "不是 stackvm 二进制程序：" + label);
            return false;
        }

        BinaryReader header{ fileBytes };
        const std::uint8_t *ignoredMagic = nullptr;
        std::uint16_t version = 0u;
        std::uint16_t flags = 0u;
        std::uint64_t payloadSize = 0u;
        std::uint32_t checksum = 0u;
        if (!header.take(sizeof(kProgramMagic), &ignoredMagic) || !header.u16(&version) || !header.u16(&flags) || !header.u64(&payloadSize) || !header.u32(&checksum))
        {
            binaryDiagnostic(diagnostics, header.error);
            return false;
        }
        if (version != kProgramFormatVersion || (flags & ~static_cast<std::uint16_t>(kKnownProgramFeatures)) != 0u)
        {
            binaryDiagnostic(diagnostics, "不支持的 stackvm 二进制版本或标志");
            return false;
        }
        decoded.requiredFeatures = flags & kKnownProgramFeatures;
        if (!validateProgramFeatures(decoded.requiredFeatures, diagnostics)) return false;
        if (payloadSize != fileBytes.size() - kProgramHeaderSize)
        {
            binaryDiagnostic(diagnostics, "二进制程序长度字段不匹配");
            return false;
        }
        const std::uint8_t *payloadBegin = fileBytes.data() + kProgramHeaderSize;
        if (binaryChecksum(payloadBegin, static_cast<std::size_t>(payloadSize)) != checksum)
        {
            binaryDiagnostic(diagnostics, "二进制程序校验和不匹配");
            return false;
        }

        const std::uint8_t *payloadEnd = fileBytes.data() + fileBytes.size();
        payloadBytes.assign(payloadBegin, payloadEnd);
    }
    BinaryReader reader{ payloadBytes };
    if (compressed) reader.symbolRecords = &decoded.symbols;
    std::uint16_t version = compressed ? kProgramFormatVersion : static_cast<std::uint16_t>(fileBytes[8] | (fileBytes[9] << 8u));
    std::uint64_t moduleId = 0;
    if (compressed ? !reader.varUInt(&moduleId) : !reader.text(&decoded.name))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }

    if (compressed) decoded.name = symbols::key(moduleId);
    std::uint32_t count = 0u;
    if (!reader.count(&count, "指令数"))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }
    decoded.code.reserve(count);
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        Instruction instruction;
        std::uint16_t op = 0u;
        std::uint8_t format = 0u;
        std::uint8_t instructionFlags = 0u;
        std::uint64_t doubleBits = 0u;
        std::uint64_t quadIndex = index;
        std::int64_t intValue = 0;
        if (version == kProgramLegacyFormatVersion)
        {
            std::uint64_t legacyIntBits = 0u;
            if (!reader.u16(&op) || !reader.u8(&format) || !reader.u8(&instructionFlags) || !reader.u64(&legacyIntBits) || !reader.u64(&doubleBits) || !reader.u64(&quadIndex) || !reader.text(&instruction.name) || !reader.text(&instruction.name2) || !reader.text(&instruction.textValue))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            std::memcpy(&intValue, &legacyIntBits, sizeof(intValue));
        }
        else
        {
            std::uint8_t byteOp = 0u;
            std::uint8_t operands = 0u;
            if (!reader.u8(&byteOp) || !reader.u8(&operands))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            op = byteOp;
            std::uint64_t varQuadIndex = index;
            if (((operands & (1u << 0u)) != 0u && !reader.u8(&format)) || ((operands & (1u << 1u)) != 0u && !reader.u8(&instructionFlags)) || ((operands & (1u << 2u)) != 0u && !reader.varInt(&intValue)) || ((operands & (1u << 3u)) != 0u && !reader.u64(&doubleBits)) || ((operands & (1u << 4u)) != 0u && !reader.varUInt(&varQuadIndex)) || ((operands & (1u << 5u)) != 0u && !(compressed ? reader.symbol(&instruction.name) : reader.varText(&instruction.name))) || ((operands & (1u << 6u)) != 0u && !(compressed ? reader.symbol(&instruction.name2) : reader.varText(&instruction.name2))) || ((operands & (1u << 7u)) != 0u && !reader.varText(&instruction.textValue)))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            quadIndex = varQuadIndex;
        }
        if (op > static_cast<std::uint16_t>(Op::StoreAt) || format > static_cast<std::uint8_t>(PrintFormat::Text) || (instructionFlags & ~3u) != 0u || quadIndex > kMaximumItemCount)
        {
            binaryDiagnostic(diagnostics, "二进制程序含非法指令字段");
            return false;
        }
        instruction.op = static_cast<Op>(op);
        instruction.format = static_cast<PrintFormat>(format);
        instruction.unsignedOp = (instructionFlags & 1u) != 0u;
        instruction.isFloat = (instructionFlags & 2u) != 0u;
        instruction.intValue = intValue;
        std::memcpy(&instruction.doubleValue, &doubleBits, sizeof(doubleBits));
        instruction.quadIndex = static_cast<std::size_t>(quadIndex);
        decoded.code.push_back(std::move(instruction));
    }

    if (!reader.count(&count, "标签数"))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        Memory::String name;
        std::uint64_t target = 0u;
        if (!reader.symbol(&name) || !reader.number(&target) || target > decoded.code.size() || !decoded.labels.emplace(std::move(name), static_cast<std::size_t>(target)).second)
        {
            binaryDiagnostic(diagnostics, reader.error.empty() ? "二进制程序含非法或重复标签" : reader.error);
            return false;
        }
    }

    if (!reader.count(&count, "类型数"))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        Memory::String variable;
        TypeInfo type;
        std::uint32_t bits = 0u;
        std::int64_t varBits = 0;
        std::uint8_t typeFlags = 0u;
        std::uint64_t size = 0u;
        if (!reader.symbol(&variable) || !reader.symbol(&type.name) || !(compressed ? reader.varInt(&varBits) : reader.u32(&bits)) || !reader.u8(&typeFlags) || !reader.number(&size) || (typeFlags & ~3u) != 0u || size > kMaximumBinarySize)
        {
            binaryDiagnostic(diagnostics, reader.error.empty() ? "二进制程序含非法类型字段" : reader.error);
            return false;
        }
        if (compressed && (varBits < -1 || varBits > 65536))
        {
            binaryDiagnostic(diagnostics, "类型位宽越界");
            return false;
        }
        type.bits = compressed ? static_cast<std::int32_t>(varBits) : static_cast<std::int32_t>(bits);
        type.isUnsigned = (typeFlags & 1u) != 0u;
        type.isFloat = (typeFlags & 2u) != 0u;
        type.size = static_cast<std::size_t>(size);
        if (!decoded.types.emplace(std::move(variable), std::move(type)).second)
        {
            binaryDiagnostic(diagnostics, "二进制程序含重复类型项");
            return false;
        }
    }

    if (!reader.count(&count, "形参表数"))
    {
        binaryDiagnostic(diagnostics, reader.error);
        return false;
    }
    for (std::uint32_t index = 0u; index < count; ++index)
    {
        Memory::String label;
        std::uint32_t parameterCount = 0u;
        if (!reader.symbol(&label) || !reader.count(&parameterCount, "形参数"))
        {
            binaryDiagnostic(diagnostics, reader.error);
            return false;
        }
        Memory::Vector<Memory::String> parameters;
        parameters.reserve(parameterCount);
        for (std::uint32_t parameter = 0u; parameter < parameterCount; ++parameter)
        {
            Memory::String name;
            if (!reader.symbol(&name))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            parameters.push_back(std::move(name));
        }
        if (!decoded.parameters.emplace(std::move(label), std::move(parameters)).second)
        {
            binaryDiagnostic(diagnostics, "二进制程序含重复形参表");
            return false;
        }
    }
    if (version >= kProgramExportsFormatVersion)
    {
        if (!reader.count(&count, "导出标签数"))
        {
            binaryDiagnostic(diagnostics, reader.error);
            return false;
        }
        for (std::uint32_t index = 0u; index < count; ++index)
        {
            Memory::String label;
            if (!reader.symbol(&label) || decoded.labels.find(label) == decoded.labels.end() || !decoded.exports.insert(std::move(label)).second)
            {
                binaryDiagnostic(diagnostics, reader.error.empty() ? "二进制程序含非法或重复导出项" : reader.error);
                return false;
            }
        }
    }
    else
    {
        for (const auto &[label, ignored] : decoded.labels)
        {
            (void)ignored;
            decoded.exports.insert(label);
        }
    }
    /* 调试信息节（v4 起才有）。旧版本没有这一段，读到时直接跳过。 */
    if (version >= kProgramDebugFormatVersion)
    {
        if (!reader.count(&count, "调试信息条数"))
        {
            binaryDiagnostic(diagnostics, reader.error);
            return false;
        }
        if (compressed && count != 0)
        {
            binaryDiagnostic(diagnostics, "无名格式不能携带原始调试节");
            return false;
        }
        std::size_t previous = 0u;
        bool first = true;
        for (std::uint32_t index = 0u; index < count; ++index)
        {
            std::uint64_t instruction = 0u;
            std::uint64_t line = 0u;
            std::uint64_t quadLine = 0u;
            SourceLocation location;
            if (!reader.varUInt(&instruction) || !reader.varText(&location.function) || !reader.varText(&location.file) || !reader.varUInt(&line) || !reader.varUInt(&quadLine))
            {
                binaryDiagnostic(diagnostics, reader.error);
                return false;
            }
            if (instruction >= decoded.code.size() || (!first && instruction < previous) || line > kMaximumItemCount || quadLine > kMaximumItemCount)
            {
                binaryDiagnostic(diagnostics, "二进制程序含非法调试信息");
                return false;
            }
            location.line = static_cast<std::int32_t>(line);
            location.quadLine = static_cast<std::size_t>(quadLine);
            decoded.debug.entries.emplace_back(static_cast<std::size_t>(instruction),
                                               std::move(location));
            previous = static_cast<std::size_t>(instruction);
            first = false;
        }
    }
    // 校验存储的内部序列与基础 VM 指令完全一致，再供执行器直接绑定。
    auto expected = execution_detail::lowerRegisters(decoded);
    if (!reader.count(&count, "内部指令数") || count != expected.size())
    {
        binaryDiagnostic(diagnostics, "内部指令数量非法");
        return false;
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        std::uint64_t origin = 0, span = 0, target = 0, destination2 = 0;
        std::uint8_t op = 0, kind = 0, flags = 0;
        std::int64_t immediate = 0, bits = 0;
        if (!reader.varUInt(&origin) || !reader.u8(&op) || !reader.u8(&kind) || !reader.varUInt(&span) || !reader.varInt(&immediate) || !reader.varInt(&bits) || !reader.u8(&flags) || !reader.varUInt(&target) || !reader.varUInt(&destination2))
        {
            binaryDiagnostic(diagnostics, reader.error);
            return false;
        }
        const auto &internal = expected[index];
        const auto expectedFlags = (internal.literal ? 1 : 0) | (internal.unsignedResult ? 2 : 0) | (internal.jumpOnZero ? 4 : 0) | (internal.storeResult ? 8 : 0) | (internal.floatOperation ? 16 : 0);
        if (origin != internal.left || op != static_cast<std::uint8_t>(internal.operation) || kind != static_cast<std::uint8_t>(internal.kind) || span != internal.span || immediate != internal.immediate || bits != internal.bits || flags != expectedFlags || target != (internal.target == execution_detail::absent ? 0 : internal.target + 1) || destination2 != (internal.destination2 == execution_detail::absent ? 0 : internal.destination2 + 1))
        {
            binaryDiagnostic(diagnostics, "内部指令与基础指令不一致");
            return false;
        }
    }
    decoded.internalCode = std::move(expected);
    decoded.preparedInternal = true;

    if ((decoded.requiredFeatures & kFeatureReflection) && reader.position < payloadBytes.size())
    {
        Memory::String error;
        if (!reflection::decode(payloadBytes.data() + reader.position,
                                payloadBytes.size() - reader.position, decoded.reflection, &error))
        {
            binaryDiagnostic(diagnostics, error);
            return false;
        }
        reader.position = payloadBytes.size();
    }
    if (reader.position != payloadBytes.size())
    {
        binaryDiagnostic(diagnostics, "二进制程序尾部有未识别数据");
        return false;
    }
    if (!validateProgramMetadata(decoded, diagnostics)) return false;
    if (compressed && !validateSymbolProgram(fileBytes, decoded, diagnostics)) return false;
    program = std::move(decoded);
    return true;
}
}  // namespace stackvm
