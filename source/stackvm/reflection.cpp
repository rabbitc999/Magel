/**
 * @file reflection.cpp
 * @brief 反射元数据的校验、稳定身份和有界二进制序列化实现。
 */
#include "reflection.hpp"

namespace stackvm::reflection
{
/**
 * @brief 规范身份的稳定 FNV-1a 编号；身份需包含类别、模块和具体签名。
 * @param key 规范身份文本。
 * @return 稳定编号；碰撞由表校验拒绝。
 */
std::uint64_t stableId(std::string_view key)
{
    std::uint64_t id = 14695981039346656037ull;
    for (unsigned char ch : key)
    {
        id ^= ch;
        id *= 1099511628211ull;
    }
    return id;
}
namespace detail
{
/**
 * @brief 内部小端写入器，限制总长度及列表长度。
 */
struct Writer
{
    Memory::Vector<std::uint8_t> bytes;
    /**
     * @brief 按有界小端格式写入反射记录的字段。
     * @param values 依次编码的字段。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class... T>
    bool operator()(const T &...values)
    { return (write(values) && ...); }
    /**
     * @brief 按有界小端格式写入反射记录的字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    std::enable_if_t<std::is_unsigned_v<T>, bool> write(T value)
    {
        if (bytes.size() > kMaximumBytes - sizeof(T)) return false;
        for (std::size_t i = 0; i < sizeof(T); ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
        return true;
    }
    /**
     * @brief 按有界小端格式写入反射记录的字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool write(bool value) { return write(std::uint8_t(value)); }
    /**
     * @brief 按有界小端格式写入反射记录的字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool write(const Memory::String &value)
    {
        if (value.size() > 1024u * 1024u || !write(std::uint32_t(value.size())) || value.size() > kMaximumBytes - bytes.size()) return false;
        bytes.insert(bytes.end(), value.begin(), value.end());
        return true;
    }
    /**
     * @brief 按有界小端格式写入反射记录的字段。
     * @param values 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    bool write(const Memory::Vector<T> &values)
    {
        if (values.size() > kMaximumRecords || !write(std::uint32_t(values.size()))) return false;
        for (const auto &value : values)
            if (!write(value)) return false;
        return true;
    }
    /**
     * @brief 按有界小端格式写入反射记录的字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    bool write(const std::optional<T> &value)
    {
        return write(bool(value)) && (!value || write(*value));
    }
    /**
     * @brief 按有界小端格式写入反射记录的字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    auto write(const T &value) -> decltype(value.visit(*this))
    { return value.visit(*this); }
};
/**
 * @brief 内部有界读取器，在分配前检查输入剩余字节。
 */
struct Reader
{
    const std::uint8_t *data;
    std::size_t size, position = 0;
    /**
     * @brief 从有界小端输入读取并校验反射字段。
     * @param values 按格式顺序接收解码结果的字段。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class... T>
    bool operator()(T &...values)
    { return (read(values) && ...); }
    /**
     * @brief 从有界小端输入读取并校验反射字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    std::enable_if_t<std::is_unsigned_v<T>, bool> read(T &value)
    {
        if (sizeof(T) > size - position) return false;
        value = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) value |= T(data[position++]) << (8 * i);
        return true;
    }
    /**
     * @brief 从有界小端输入读取并校验反射字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool read(bool &value)
    {
        std::uint8_t raw = 0;
        if (!read(raw) || raw > 1) return false;
        value = raw != 0;
        return true;
    }
    /**
     * @brief 从有界小端输入读取并校验反射字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool read(Memory::String &value)
    {
        std::uint32_t count = 0;
        if (!read(count) || count > 1024u * 1024u || count > size - position) return false;
        value.assign(reinterpret_cast<const char *>(data + position), count);
        position += count;
        return true;
    }
    /**
     * @brief 从有界小端输入读取并校验反射字段。
     * @param values 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    bool read(Memory::Vector<T> &values)
    {
        std::uint32_t count = 0;
        if (!read(count) || count > kMaximumRecords || count > size - position) return false;
        // 本格式每个列表元素至少占一个字节；逐项构造避免长度前缀触发一次性巨额分配。
        for (std::uint32_t i = 0; i < count; ++i)
        {
            T value{};
            if (!read(value)) return false;
            values.push_back(std::move(value));
        }
        return true;
    }
    /**
     * @brief 从有界小端输入读取并校验反射字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    bool read(std::optional<T> &value)
    {
        bool present = false;
        if (!read(present)) return false;
        if (present)
        {
            T item{};
            if (!read(item)) return false;
            value = std::move(item);
        }
        return true;
    }
    /**
     * @brief 从有界小端输入读取并校验反射字段。
     * @param value 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    auto read(T &value) -> decltype(value.visit(*this))
    { return value.visit(*this); }
};
/**
 * @brief 内部表验证器，检查身份及所有交叉引用。
 */
struct Validator
{
    const Table &table;
    Memory::Set<std::uint64_t> ids, definitions;
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param id 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool reference(std::uint64_t id) const { return ids.count(id) != 0; }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param values 需要逐项校验的字段与记录。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class... T>
    bool operator()(const T &...values)
    { return (check(values) && ...); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const Constant &v) const { return reference(v.typeId); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const Parameter &v) const { return !v.name.empty() && reference(v.typeId); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const AnnotationParameter &v) const
    {
        return !v.name.empty() && reference(v.typeId) && (!v.defaultValue || (v.defaultValue->typeId == v.typeId && check(*v.defaultValue)));
    }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const Annotation &v) const
    {
        const auto info = findById(table.annotations, v.annotationId);
        if (!info || info->get().params.size() != v.args.size()) return false;
        for (std::size_t i = 0; i < v.args.size(); ++i)
            if (!check(v.args[i]) || v.args[i].typeId != info->get().params[i].typeId) return false;
        return true;
    }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param values 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    bool check(const Memory::Vector<T> &values) const
    {
        if (values.size() > kMaximumRecords) return false;
        for (const auto &value : values)
            if (!check(value)) return false;
        return true;
    }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param id 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(std::uint64_t id) const { return reference(id); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    bool named(const T &v)
    {
        return !v.name.empty() && reference(v.typeId) && definitions.insert(v.typeId).second;
    }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param values 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    template <class T>
    bool uniqueNames(const Memory::Vector<T> &values) const
    {
        Memory::Set<Memory::String> names;
        for (const auto &v : values)
            if (!names.insert(v.name).second) return false;
        return true;
    }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const FuncInfo &v) { return named(v) && reference(v.returnTypeId) && uniqueNames(v.params) && (*this)(v.params, v.annotations); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const MethodInfo &v)
    {
        // static T new 是无显式接收者的构造入口；构造体的 self 不属于公开签名。
        return !(v.isConst && (v.isStatic || v.isConstructor)) && check(v.function);
    }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const FieldInfo &v) const { return !v.name.empty() && reference(v.typeId) && check(v.annotations); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const StaticMemberInfo &v) const { return !v.name.empty() && reference(v.typeId) && check(v.annotations); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const VariantInfo &v) const { return !v.name.empty() && uniqueNames(v.payloads) && check(v.payloads); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const AnnotationInfo &v) { return named(v) && uniqueNames(v.params) && check(v.params); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const StructInfo &v)
    {
        return named(v) && uniqueNames(v.fields) && uniqueNames(v.staticMembers) && (*this)(v.fields, v.methods, v.staticMembers, v.implementedInterfaces, v.annotations);
    }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const EnumInfo &v)
    {
        Memory::Set<std::uint32_t> tags;
        for (const auto &variant : v.variants)
            if (!tags.insert(variant.tag).second) return false;
        return named(v) && uniqueNames(v.variants) && uniqueNames(v.staticMembers) && (*this)(v.variants, v.methods, v.staticMembers, v.implementedInterfaces, v.annotations);
    }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param v 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    bool check(const InterfaceInfo &v) { return named(v) && (*this)(v.methods, v.implementedBy, v.annotations); }
    /**
     * @brief 校验反射字段的类型身份、唯一性与交叉引用。
     * @param values 待处理的字段、记录或类型身份。
     * @return 满足格式及引用约束时返回真，否则返回假。
     */

    // 含定义的列表需要更新重复编号集合。
    template <class T>
    bool check(const Memory::Vector<T> &values)
    {
        if (values.size() > kMaximumRecords) return false;
        for (const auto &value : values)
            if (!check(value)) return false;
        return true;
    }
};
}  // namespace detail
/**
 * @brief 校验身份碰撞、声明重复和精确类型引用。
 * @param table 元数据。
 * @param error 失败时写入错误文本。
 * @return 合法返回真。
 */
bool validate(const Table &table, Memory::String *error)
{
    detail::Validator validator{ table, {}, {} };
    bool ok = table.identities.size() <= kMaximumRecords;
    for (const auto &identity : table.identities)
        ok = ok && !identity.key.empty() && identity.id != 0 && identity.id == stableId(identity.key) && validator.ids.insert(identity.id).second;
    ok = ok && validator(table.structs, table.enums, table.interfaces, table.functions, table.annotations, table.moduleAnnotations);
    if (!ok && error) *error = "反射元数据身份、签名或交叉引用无效";
    return ok;
}

/**
 * @brief 编码独立元数据段；失败不修改输出。
 * @param table 元数据。
 * @param output 字节输出。
 * @param error 错误文本。
 * @return 成功为真。
 */
bool encode(const Table &table, Memory::Vector<std::uint8_t> &output, Memory::String *error)
{
    if (!validate(table, error)) return false;
    detail::Writer writer;
    if (!writer(std::uint32_t(0x4652434d), kSchemaVersion, table))
    {
        if (error) *error = "反射元数据超过格式容量限制";
        return false;
    }
    output = std::move(writer.bytes);
    return true;
}

/**
 * @brief 解码并校验整个段；失败不修改现有表。
 * @param data 字节起点。
 * @param size 段长。
 * @param output 输出表。
 * @param error 错误文本。
 * @return 成功为真；不接受未知版本、尾部数据或截断输入。
 */
bool decode(const std::uint8_t *data, std::size_t size, Table &output, Memory::String *error)
{
    if (size > kMaximumBytes || size < 8 || data == nullptr)
    {
        if (error) *error = "反射元数据段长度无效";
        return false;
    }
    detail::Reader reader{ data, size };
    std::uint32_t magic = 0, version = 0;
    Table decoded;
    if (!reader(magic, version) || magic != 0x4652434d || version != kSchemaVersion || !reader(decoded) || reader.position != size)
    {
        if (error) *error = "反射元数据段损坏或版本不受支持";
        return false;
    }
    if (!validate(decoded, error)) return false;
    output = std::move(decoded);
    return true;
}

/**
 * @brief 将元数据编码为四元式注释所用的十六进制文本。
 * @param table 表。
 * @return 编码文本；非法表返回空。
 */
std::optional<Memory::String> toHex(const Table &table)
{
    Memory::Vector<std::uint8_t> bytes;
    if (!encode(table, bytes)) return std::nullopt;
    const char *digits = "0123456789abcdef";
    Memory::String result;
    result.reserve(bytes.size() * 2);
    for (auto byte : bytes)
    {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}

/**
 * @brief 解码四元式注释的十六进制元数据，失败不修改输出。
 * @param text 编码文本。
 * @param output 输出表。
 * @return 合法返回真。
 */
bool fromHex(std::string_view text, Table &output)
{
    if (text.size() > kMaximumBytes * 2 || text.size() % 2) return false;
    auto digit = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    Memory::Vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < text.size(); i += 2)
    {
        const int high = digit(text[i]), low = digit(text[i + 1]);
        if (high < 0 || low < 0) return false;
        bytes.push_back(static_cast<std::uint8_t>(high * 16 + low));
    }
    return decode(bytes.data(), bytes.size(), output);
}

/**
 * @brief 生成自包含导出的静态编码数据及拥有型表工厂。
 * @param table 表。
 * @return C++ 定义；非法表返回空。
 */
std::optional<Memory::String> cppDefinition(const Table &table)
{
    Memory::Vector<std::uint8_t> bytes;
    if (!encode(table, bytes)) return std::nullopt;
    Memory::String code = "\n/** @brief 在调用方分配域构造本产物的元数据。 @return 拥有型表，查询返回只读引用。 */\n"
                          "stackvm::reflection::Table chali_reflection_metadata() {\n"
                          "        static const std::uint8_t bytes[] = {\n";
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        code += Memory::toString(static_cast<unsigned>(bytes[i])) + ",";
        if (i % 24 == 23) code += "\n";
    }
    code += "\n        };\n        stackvm::reflection::Table result;\n"
            "        if (!stackvm::reflection::decode(bytes, sizeof(bytes), result)) std::abort();\n"
            "        return result;\n}\n";
    return code;
}
}  // namespace stackvm::reflection
