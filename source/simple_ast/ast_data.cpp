/**
 * @file ast_data.cpp
 * @brief 数据区二进制、布局与 C 头导出。
 */
#include "ast_storage_detail.hpp"
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
namespace simple_ast
{
namespace
{
using Kind = Type::Kind;
// ---- .data 二进制导出的小工具 --------------------------------------------

/**
 * @brief 从 meta 里取 `key=数字`；没有就返回 fallback。
 * @param meta 类型布局的附加元数据。
 * @param key 查找或写入使用的键。
 * @param fallback 无法取得目标值时使用的默认值。
 * @retval 元数据中指定键的整数值；缺失时为 fallback。
 */
int metaInt(const Memory::String &meta, const Memory::String &key, int fallback)
{
    const std::size_t at = meta.find(key + "=");
    if (at == Memory::String::npos) return fallback;
    return std::atoi(meta.c_str() + at + key.size() + 1u);
}

/**
 * @brief 追加小端整数（8 字节）。
 * @param out 接收处理结果的输出对象。
 * @param value 待转换、保存或参与计算的值。
 */
void putInt(Memory::String &out, std::int64_t value)
{
    const std::uint64_t bits = static_cast<std::uint64_t>(value);
    for (int index = 0; index < 8; ++index) out.push_back(static_cast<char>((bits >> (index * 8)) & 0xFFu));
}

/**
 * @brief 追加小端 32 位无符号（4 字节）。
 * @param out 接收处理结果的输出对象。
 * @param value 待转换、保存或参与计算的值。
 */
void putU32(Memory::String &out, std::uint32_t value)
{
    for (int index = 0; index < 4; ++index) out.push_back(static_cast<char>((value >> (index * 8)) & 0xFFu));
}

/**
 * @brief 追加小端双精度（8 字节）。
 * @param out 接收处理结果的输出对象。
 * @param value 待转换、保存或参与计算的值。
 */
void putDouble(Memory::String &out, double value)
{
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double 必须是 8 字节");
    std::memcpy(&bits, &value, sizeof(bits));
    for (int index = 0; index < 8; ++index) out.push_back(static_cast<char>((bits >> (index * 8)) & 0xFFu));
}

/**
 * @brief 文本是不是带引号的字符串。
 * @param text 待处理的文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool isQuoted(const Memory::String &text)
{
    return text.size() >= 2u && text.front() == '"' && text.back() == '"';
}

/**
 * @brief 切顶层逗号（去括号，尊重引号）。
 * @param body 待处理的函数体或循环体。
 * @retval 生成或读取的结果文本。
 */
Memory::Vector<Memory::String> splitTop(const Memory::String &body)
{
    // 先把最外层的 [] / {} / () 剥掉：它们让内容停在 depth=1，
    // 而下面的分隔判断只在 depth==0 生效——不剥就会把 `[10, 20, 30, 40]`
    // 当成一个元素（这正是数组在 .data 里只写 4 字节的原因）。
    Memory::String text = body;
    if (text.size() >= 2u)
    {
        const char front = text.front();
        const char back = text.back();
        const bool wrapped = (front == '[' && back == ']') || (front == '{' && back == '}') || (front == '(' && back == ')');
        if (wrapped) text = text.substr(1u, text.size() - 2u);
    }

    Memory::Vector<Memory::String> parts;
    Memory::String current;
    int depth = 0;
    bool quoted = false;
    for (const char ch : text)
    {
        if (ch == '"') quoted = !quoted;
        if (!quoted)
        {
            if (ch == '[' || ch == '{' || ch == '(')
            {
                ++depth;
                continue;
            }
            if (ch == ']' || ch == '}' || ch == ')')
            {
                --depth;
                continue;
            }
            if (ch == ',' && depth == 0)
            {
                parts.push_back(current);
                current.clear();
                continue;
            }
        }
        current += ch;
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

/**
 * @brief 解析数字文本。
 * @param text 待处理的文本。
 * @param isFloat 是否按浮点数表示读写数据。
 * @param intValue 整数形式的数值。
 * @param doubleValue 浮点形式的数值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool numberText(const Memory::String &text, bool &isFloat, std::int64_t &intValue, double &doubleValue)
{
    if (text.empty()) return false;
    char *end = nullptr;
    isFloat = text.find_first_of(".eE") != Memory::String::npos;
    if (isFloat)
    {
        doubleValue = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0' || !std::isfinite(doubleValue)) return false;
        intValue = doubleValue >= -9223372036854775808.0 && doubleValue < 9223372036854775808.0 ? static_cast<std::int64_t>(doubleValue) : 0;
        return true;
    }
    errno = 0;
    if (text.front() == '-')
        intValue = std::strtoll(text.c_str(), &end, 10);
    else
    {
        const std::uint64_t bits = std::strtoull(text.c_str(), &end, 10);
        std::memcpy(&intValue, &bits, sizeof(intValue));
    }
    return end != text.c_str() && *end == '\0' && errno != ERANGE;
}

}  // namespace
namespace
{

/**
 * @brief 把数据条目铺成二进制；可选输出布局表。
 * @param entries 条目。
 * @param layout 非空时写入 `名字 offset=… size=… type=…`。
 * @return 二进制内容。
 */
Memory::String buildDataBinary(const Memory::Vector<CodegenResult::Entry> &entries,
                               Memory::Vector<Memory::String> *layout)
{
    // 1. 句柄表：数组元素里出现字符串 / 复合对象时写的是"条目序号"。
    Memory::Map<Memory::String, std::uint32_t> handles;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        handles[entries[index].name] = static_cast<std::uint32_t>(index);
    }

    Memory::String out;
    // **地址 0 留给"空指针"**：数据区从偏移 8 开始排，这样任何真实对象的地址都不为 0，
    // `p == null` / 函数指针判空才有意义（堆那边本来也是从数据区之后开始长的）。
    out.assign(8u, '\0');
    for (const CodegenResult::Entry &entry : entries)
    {
        const auto element = entry.type->kind == Type::Kind::Array ? entry.type->element : entry.type;
        const bool byteStorage = scalarStorageBytes(element) == 1u && entry.type->kind != Type::Kind::Struct;
        // u8 数据按字节对齐；显式内部对齐元数据仍被保留。
        const int align = metaInt(entry.type->meta, "align", byteStorage ? 1 : 8);
        while ((out.size() % static_cast<std::size_t>(align > 0 ? align : 1)) != 0u) out.push_back('\0');
        // 数组地址指向首元素，前八字节记录逻辑长度；布局表不把长度头计入载荷。
        if (entry.type->kind == Type::Kind::Array)
        {
            const int count = metaInt(entry.type->meta, "length", metaInt(entry.type->meta, "count", static_cast<int>(splitTop(entry.body).size())));
            putInt(out, count);
        }
        const std::size_t start = out.size();

        /**
         * 写一个值：字符串 → 字节 + `\0`；数字 → 8 字节；已知标签 → 4 字节句柄。
         */
        const Memory::Function<void(const Memory::String &)> writeValue = [&](const Memory::String &token) {
            Memory::String text = token;
            while (!text.empty() && (text.front() == ' ')) text.erase(text.begin());
            while (!text.empty() && (text.back() == ' ')) text.pop_back();
            if (isQuoted(text))
            {
                out += text.substr(1u, text.size() - 2u);
                out.push_back('\0');
                return;
            }
            bool isFloat = false;
            std::int64_t intValue = 0;
            double doubleValue = 0.0;
            if (numberText(text, isFloat, intValue, doubleValue))
            {
                if (byteStorage)
                    out.push_back(static_cast<char>(static_cast<std::uint8_t>(intValue)));
                else if (isFloat)
                    putDouble(out, doubleValue);
                else
                    putInt(out, intValue);
                return;
            }
            const auto found = handles.find(text);
            putU32(out, found == handles.end() ? 0u : found->second);
        };

        // 3. 按类型写内容。
        if (entry.type->kind == Type::Kind::String)
        {
            writeValue(entry.body);
        }
        else if (entry.type->kind == Type::Kind::Array || entry.type->kind == Type::Kind::Struct)
        {
            for (const Memory::String &item : splitTop(entry.body)) writeValue(item);
        }
        else if (entry.type->kind == Type::Kind::Enum)
        {
            // 枚举：4 字节 tag（分支序号，按类型的 variants 顺序）+ 载荷。
            const std::size_t open = entry.body.find('(');
            const Memory::String variant =
                (open == Memory::String::npos) ? entry.body : entry.body.substr(0u, open);
            std::uint32_t tag = 0u;
            for (std::size_t index = 0; index < entry.type->variants.size(); ++index)
            {
                if (entry.type->variants[index].first == variant) tag = static_cast<std::uint32_t>(index);
            }
            putU32(out, tag);
            if (open != Memory::String::npos)
            {
                const std::size_t close = entry.body.rfind(')');
                const Memory::String payload = entry.body.substr(open + 1u, close - open - 1u);
                for (const Memory::String &item : splitTop(payload)) writeValue(item);
            }
        }
        else
        {
            writeValue(entry.body);
        }

        if (layout != nullptr)
        {
            // 把物理步长写入布局，与下标寻址和边界检查共用口径。
            layout->push_back(entry.name + " offset=" + Memory::toString(start) + " size=" + Memory::toString(out.size() - start) + " stride=" + Memory::toString(byteStorage ? 1 : 8) + " type=" + entry.type->describe());
        }
    }
    // 5. 尾部补 8 个零字节：数据区是按地址按 8 字节整块读的（`load`），
    //    最后一个条目末尾也要有"可读满 8 字节"的余量，逐字节扫字符串才不会越界。
    while ((out.size() % 8u) != 0u) out.push_back('\0');
    out.append(8u, '\0');
    return out;
}

}  // namespace

/**
 * @brief 导出 `.data` 的**二进制**形态（小端）。
 *
 * 布局规则（简单、够嵌入式用）：
 *
 *   - 每个条目按 `align=` 对齐（缺省 8 字节），不足补 0；
 *   - 字符串：UTF-8 字节 + `\0`；
 *   - u8：一个字节，数组中的 u8 连续保存；
 *   - 其他数值：8 字节有符号 / 浮点（f* 用 IEEE-754 双精度）；
 *   - 数组：八字节逻辑长度头后紧接元素，符号地址指向首元素；
 *   - 结构体：字段依次排布；
 *   - 枚举：4 字节 tag（分支序号）+ 载荷；
 *   - 引用 / 字符串 / 复合对象出现在数组里时写 4 字节"条目序号"当句柄。
 * @return 二进制内容。
 */
Memory::String CodegenResult::exportDataBinary() const
{
    return buildDataBinary(entries, nullptr);
}

/**
 * @brief 每个条目在二进制里的偏移与大小（便于查表 / 测试）。
 * @return 一行一条：`名字 offset=… size=… type=…`。
 */
Memory::Vector<Memory::String> CodegenResult::exportDataLayout() const
{
    Memory::Vector<Memory::String> layout;
    (void)buildDataBinary(entries, &layout);
    return layout;
}

/**
 * @brief 导出配套的 C 头文件（偏移表 + 字节数组）。
 * @param moduleName 模块名（写进注释与宏前缀）。
 * @return 头文件文本。
 */
Memory::String CodegenResult::exportDataHeader(const Memory::String &moduleName) const
{
    const Memory::String binary = buildDataBinary(entries, nullptr);
    Memory::OutputStringStream header;
    header << "/* " << moduleName << " 的 .data 二进制（小端）。由 simple_ast 生成。 */\n";
    header << "#ifndef SIMPLE_AST_DATA_" << moduleName << "_H\n";
    header << "#define SIMPLE_AST_DATA_" << moduleName << "_H\n\n";
    header << "#include <stdint.h>\n\n";
    for (const Memory::String &line : exportDataLayout())
    {
        const std::size_t space = line.find(' ');
        const Memory::String name = line.substr(0u, space);
        const std::size_t offsetAt = line.find("offset=");
        header << "#define DATA_" << name << "_OFFSET "
               << line.substr(offsetAt + 7u, line.find(' ', offsetAt) - offsetAt - 7u) << "u\n";
    }
    header << "\nstatic const unsigned char " << moduleName << "_data[] = {";
    for (std::size_t index = 0; index < binary.size(); ++index)
    {
        if (index % 12u == 0u) header << "\n    ";
        header << static_cast<int>(static_cast<unsigned char>(binary[index])) << ",";
    }
    header << "\n};\n\n#endif\n";
    return header.str();
}

}  // namespace simple_ast
