/**
 * @file artifact_binary.cpp
 * @brief 编译期四元式与数据布局的二进制交换格式；不依赖对齐、字节序或异常。
 */
namespace artifact_binary
{
/**
 * @brief 写入无符号 LEB128。
 * @param out 输出。
 * @param value 数值。
 */
void number(Memory::String &out, std::uint64_t value)
{
    do
    {
        out += static_cast<char>((value & 127u) | (value >= 128u ? 128u : 0u));
        value >>= 7u;
    } while (value);
}
/**
 * @brief 写入长度前缀字节串。
 * @param out 输出。
 * @param value 字节串。
 */
void string(Memory::String &out, std::string_view value)
{
    number(out, value.size());
    out.append(value.data(), value.size());
}
/**
 * @brief 有界读取器；非法整数、截断或非规范编码使读取失败。
 */
struct Reader
{
    std::string_view bytes;
    std::size_t at = 0;
    /**
     * @brief 读取变长整数。
     * @param value 输出。
     * @return 是否成功。
     */
    bool number(std::uint64_t &value)
    {
        value = 0;
        for (unsigned shift = 0; shift < 64; shift += 7)
        {
            if (at == bytes.size()) return false;
            const auto ch = static_cast<unsigned char>(bytes[at++]);
            if (shift == 63 && ch > 1) return false;
            value |= std::uint64_t(ch & 127u) << shift;
            if (!(ch & 128u)) return shift == 0 || ch != 0;
        }
        return false;
    }
    /**
     * @brief 读取长度前缀字节串。
     * @param value 输出。
     * @return 是否成功。
     */
    bool string(Memory::String &value)
    {
        std::uint64_t size = 0;
        if (!number(size) || size > bytes.size() - at) return false;
        value.assign(bytes.data() + at, static_cast<std::size_t>(size));
        at += size;
        return true;
    }
};
}  // namespace artifact_binary

/**
 * @brief 编码带字符串表的二进制四元式，不写入宿主路径。
 * @param file 已解析的四元式。
 * @return 编码字节；输入含诊断时返回空。
 */
Memory::String encodeQuadBinary(const QuadFile &file)
{
    if (!file.ok()) return {};
    Memory::String out("MGQI\1", 5);
    artifact_binary::number(out, file.requiredFeatures);
    for (const auto *s : { &file.name, &file.expected, &file.expectedText, &file.input }) artifact_binary::string(out, *s);
    Memory::Vector<std::uint8_t> reflectionBytes;
    if (!file.reflection.empty() && !reflection::encode(file.reflection, reflectionBytes)) return {};
    artifact_binary::number(out, reflectionBytes.size());
    for (const auto b : reflectionBytes) out += static_cast<char>(b);
    Memory::Vector<Memory::String> strings;
    Memory::Map<Memory::String, std::size_t> indices;
    const auto intern = [&](const Memory::String &s) {
        auto inserted = indices.emplace(s, strings.size());
        if (inserted.second) strings.push_back(s);
        return inserted.first->second;
    };
    for (const auto &q : file.quads)
    {
        intern(q.sourceFile);
        intern(q.op);
        for (const auto &a : q.args) intern(a);
    }
    artifact_binary::number(out, strings.size());
    for (const auto &s : strings) artifact_binary::string(out, s);
    artifact_binary::number(out, file.quads.size());
    for (const auto &q : file.quads)
    {
        for (const auto n : { std::uint64_t(q.line), std::uint64_t(q.sourceLine), std::uint64_t(indices.at(q.sourceFile)),
                              std::uint64_t(indices.at(q.op)), std::uint64_t(q.args.size()) }) artifact_binary::number(out, n);
        for (const auto &a : q.args) artifact_binary::number(out, indices.at(a));
    }
    return out;
}

/**
 * @brief 解码二进制四元式，严格校验版本、引用和边界。
 * @param bytes 编码字节。
 * @param label 诊断路径。
 * @return 四元式或诊断。
 */
QuadFile decodeQuadBinary(std::string_view bytes, Memory::String label)
{
    QuadFile file;
    file.path = std::move(label);
    const auto bad = [&]() { file.quads.clear(); file.diagnostics.push_back("二进制四元式损坏或版本不受支持"); return file; };
    if (bytes.size() < 5 || bytes.substr(0, 5) != std::string_view("MGQI\1", 5)) return bad();
    artifact_binary::Reader r{ bytes, 5 };
    std::uint64_t value = 0;
    if (!r.number(value) || value > UINT32_MAX) return bad();
    file.requiredFeatures = static_cast<std::uint32_t>(value);
    for (auto *s : { &file.name, &file.expected, &file.expectedText, &file.input })
        if (!r.string(*s)) return bad();
    if (!r.number(value) || value > bytes.size() - r.at) return bad();
    if (value && (!reflection::decode(reinterpret_cast<const std::uint8_t *>(bytes.data() + r.at), value, file.reflection) || !(file.requiredFeatures & kFeatureReflection))) return bad();
    r.at += value;
    std::uint64_t count = 0;
    if (!r.number(count) || count > bytes.size() - r.at) return bad();
    Memory::Vector<Memory::String> strings;
    for (std::uint64_t i = 0; i < count; ++i)
    {
        Memory::String s;
        if (!r.string(s)) return bad();
        strings.push_back(std::move(s));
    }
    const auto reference = [&](Memory::String &s) { if (!r.number(value) || value >= strings.size()) return false; s = strings[value]; return true; };
    if (!r.number(count) || count > (bytes.size() - r.at) / 5) return bad();
    for (std::uint64_t i = 0; i < count; ++i)
    {
        Quad q;
        std::uint64_t line = 0, source = 0, args = 0;
        if (!r.number(line) || line > SIZE_MAX || !r.number(source) || source > INT32_MAX || !reference(q.sourceFile) || !reference(q.op) || !r.number(args) || args > bytes.size() - r.at) return bad();
        q.line = static_cast<std::size_t>(line);
        q.sourceLine = static_cast<std::int32_t>(source);
        for (std::uint64_t j = 0; j < args; ++j)
        {
            Memory::String s;
            if (!reference(s)) return bad();
            q.args.push_back(std::move(s));
        }
        file.quads.push_back(std::move(q));
    }
    if (r.at != bytes.size()) return bad();
    return file;
}

/**
 * @brief 将布局编码为变长整数与长度前缀字符串。
 * @param rows 布局记录。
 * @return 二进制字节。
 */
Memory::String encodeDataLayout(const Memory::Vector<LayoutRow> &rows)
{
    Memory::String out("MGLY\1", 5);
    artifact_binary::number(out, rows.size());
    for (const auto &row : rows)
    {
        artifact_binary::string(out, row.name);
        artifact_binary::number(out, row.offset);
        artifact_binary::number(out, (row.hasSize ? 1u : 0u) | (row.hasStride ? 2u : 0u));
        if (row.hasSize) artifact_binary::number(out, row.size);
        if (row.hasStride) artifact_binary::number(out, row.stride);
    }
    return out;
}

/**
 * @brief 解析文本或二进制布局，失败不修改输出。
 * @param bytes 布局数据。
 * @param rows 输出记录。
 * @return 是否成功。
 */
bool decodeDataLayout(std::string_view bytes, Memory::Vector<LayoutRow> &rows)
{
    Memory::Vector<LayoutRow> decoded;
    if (bytes.size() >= 4 && bytes.substr(0, 4) == "MGLY")
    {
        if (bytes.size() < 5 || bytes[4] != 1) return false;
        artifact_binary::Reader r{ bytes, 5 };
        std::uint64_t count = 0;
        if (!r.number(count) || count > (bytes.size() - r.at) / 3) return false;
        for (std::uint64_t i = 0; i < count; ++i)
        {
            LayoutRow row;
            std::uint64_t offset = 0, flags = 0, size = 0, stride = 0;
            if (!r.string(row.name) || row.name.empty() || !r.number(offset) || offset > SIZE_MAX || !r.number(flags) || flags > 3 || ((flags & 1) && (!r.number(size) || size > SIZE_MAX)) || ((flags & 2) && (!r.number(stride) || stride > SIZE_MAX))) return false;
            row.offset = offset;
            row.size = size;
            row.stride = stride;
            row.hasSize = flags & 1;
            row.hasStride = flags & 2;
            decoded.push_back(std::move(row));
        }
        if (r.at != bytes.size()) return false;
    }
    else
    {
        Memory::InputStringStream stream{ Memory::String(bytes) };
        Memory::String line;
        while (std::getline(stream, line))
        {
            if (trim(line).empty()) continue;
            LayoutRow row;
            Memory::InputStringStream fields(line);
            if (!(fields >> row.name)) return false;
            Memory::String field;
            bool hasOffset = false;
            while (fields >> field)
            {
                const auto eq = field.find('=');
                if (eq == Memory::String::npos) continue;
                const auto key = field.substr(0, eq);
                if (key != "offset" && key != "size" && key != "stride") continue;
                std::uint64_t value = 0;
                const auto part = std::string_view(field).substr(eq + 1);
                auto parsed = std::from_chars(part.data(), part.data() + part.size(), value);
                if (parsed.ec != std::errc() || parsed.ptr != part.data() + part.size() || value > SIZE_MAX) return false;
                if (key == "offset")
                {
                    row.offset = value;
                    hasOffset = true;
                }
                if (key == "size")
                {
                    row.size = value;
                    row.hasSize = true;
                }
                if (key == "stride")
                {
                    row.stride = value;
                    row.hasStride = true;
                }
            }
            if (!hasOffset) return false;
            decoded.push_back(std::move(row));
        }
    }
    rows = std::move(decoded);
    return true;
}
