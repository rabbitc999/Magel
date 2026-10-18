/**
 * @file reflection.hpp
 * @brief 反射元数据的值模型、只读查询和有界二进制编解码。
 */
#ifndef MAGEL_REFLECTION_HPP
#define MAGEL_REFLECTION_HPP
#include "../../source/allocator/magel_allocator.hpp"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>

namespace stackvm::reflection
{
/**
 * @brief 元数据格式版本；与指令容器版本独立演进。
 */
inline constexpr std::uint32_t kSchemaVersion = 1;
/**
 * @brief 单个元数据段最大字节数，读取前检查。
 */
inline constexpr std::size_t kMaximumBytes = 16u * 1024u * 1024u;
/**
 * @brief 每个列表最多记录数，避免损坏文件触发巨额分配。
 */
inline constexpr std::uint32_t kMaximumRecords = 4096;
/**
 * @brief 规范身份的稳定 FNV-1a 编号；身份需包含类别、模块和具体签名。
 * @param key 规范身份文本。
 * @return 稳定编号；碰撞由表校验拒绝。
 */
inline std::uint64_t stableId(std::string_view key)
{
    std::uint64_t id = 14695981039346656037ull;
    for (unsigned char ch : key)
    {
        id ^= ch;
        id *= 1099511628211ull;
    }
    return id;
}
/**
 * @brief 为所有记录提供统一字段访问，不存储函数指针或对象地址。
 */
#define MAGEL_REFLECTION_FIELDS(...) \
    template <class V>               \
    bool visit(V &v)                 \
    { return v(__VA_ARGS__); }       \
    template <class V>               \
    bool visit(V &v) const           \
    { return v(__VA_ARGS__); }
/**
 * @brief 编译器登记的具体身份；未暴露类型也可以只登记身份。
 */
struct Identity
{
    std::uint64_t id = 0;  ///< 稳定编号。
    Memory::String key;    ///< 带类别前缀的完整规范身份。
    MAGEL_REFLECTION_FIELDS(id, key)
};
/**
 * @brief 编译期常量，不含 VM 句柄；数字保存原始位模式。
 */
struct Constant
{
    std::uint64_t typeId = 0;  ///< 精确类型编号。
    std::uint64_t bits = 0;    ///< 整数或 IEEE 754 位模式。
    Memory::String text;       ///< 字符串内容，允许内嵌零字节。
    MAGEL_REFLECTION_FIELDS(typeId, bits, text)
};
/**
 * @brief 一次注解使用，实参按声明顺序排列并包含默认值。
 */
struct Annotation
{
    std::uint64_t annotationId = 0;  ///< 注解定义编号。
    Memory::Vector<Constant> args;   ///< 已校验的参数值。
    MAGEL_REFLECTION_FIELDS(annotationId, args)
};
/**
 * @brief 函数或枚举载荷的具名参数。
 */
struct Parameter
{
    Memory::String name;       ///< 源码名称。
    std::uint64_t typeId = 0;  ///< 精确类型编号。
    MAGEL_REFLECTION_FIELDS(name, typeId)
};
/**
 * @brief 注解声明的参数与可选默认值。
 */
struct AnnotationParameter
{
    Memory::String name;                   ///< 参数名称。
    std::uint64_t typeId = 0;              ///< 精确类型编号。
    std::optional<Constant> defaultValue;  ///< 缺省参数值。
    MAGEL_REFLECTION_FIELDS(name, typeId, defaultValue)
};
/**
 * @brief 注解定义，区别于某次使用的 Annotation。
 */
struct AnnotationInfo
{
    Memory::String name;                         ///< 完整声明名称。
    std::uint64_t typeId = 0;                    ///< 注解身份编号。
    Memory::Vector<AnnotationParameter> params;  ///< 参数声明顺序。
    MAGEL_REFLECTION_FIELDS(name, typeId, params)
};
/**
 * @brief 自由函数的声明信息；本模块仅描述签名，不提供调用入口。
 */
struct FuncInfo
{
    Memory::String name;                     ///< 完整名称。
    std::uint64_t typeId = 0;                ///< 包含签名的函数身份编号。
    Memory::Vector<Parameter> params;        ///< 不包含接收者。
    std::uint64_t returnTypeId = 0;          ///< void 也有独立身份。
    Memory::Vector<Annotation> annotations;  ///< 声明注解。
    MAGEL_REFLECTION_FIELDS(name, typeId, params, returnTypeId, annotations)
};
/**
 * @brief 方法的签名及接收者属性；调用桥接桩由后续模块生成。
 */
struct MethodInfo
{
    FuncInfo function;            ///< 方法签名与注解。
    bool isStatic = false;        ///< 无接收者。
    bool isConstructor = false;   ///< 构造函数。
    bool isConst = false;         ///< 只读接收者。
    bool hasDefaultBody = false;  ///< 接口默认实现存在，仅供查询。
    MAGEL_REFLECTION_FIELDS(function, isStatic, isConstructor, isConst, hasDefaultBody)
};
/**
 * @brief 实例字段，偏移按编译器实际布局以字节表示。
 */
struct FieldInfo
{
    Memory::String name;                     ///< 字段名。
    std::uint32_t offset = 0;                ///< 字节偏移。
    std::uint64_t typeId = 0;                ///< 精确类型编号。
    bool isOwned = false;                    ///< 是否拥有需要释放的资源。
    Memory::Vector<Annotation> annotations;  ///< 字段注解。
    MAGEL_REFLECTION_FIELDS(name, offset, typeId, isOwned, annotations)
};
/**
 * @brief 静态成员信息，不暴露运行时存储地址。
 */
struct StaticMemberInfo
{
    Memory::String name;                     ///< 成员名。
    std::uint64_t typeId = 0;                ///< 精确类型编号。
    bool isOwned = false;                    ///< 资源拥有标志。
    Memory::Vector<Annotation> annotations;  ///< 成员注解。
    MAGEL_REFLECTION_FIELDS(name, typeId, isOwned, annotations)
};
/**
 * @brief 结构体元数据。
 */
struct StructInfo
{
    Memory::String name;                                  ///< 完整类型名。
    std::uint64_t typeId = 0;                             ///< 具体类型身份。
    Memory::Vector<FieldInfo> fields;                     ///< 实例字段。
    Memory::Vector<MethodInfo> methods;                   ///< 实例、静态与构造方法。
    Memory::Vector<StaticMemberInfo> staticMembers;       ///< 静态成员。
    Memory::Vector<std::uint64_t> implementedInterfaces;  ///< 接口传递闭包。
    Memory::Vector<Annotation> annotations;               ///< 类型注解。
    MAGEL_REFLECTION_FIELDS(name, typeId, fields, methods, staticMembers, implementedInterfaces, annotations)
};
/**
 * @brief 枚举变体，保留 MAGEL 原生多个具名载荷。
 */
struct VariantInfo
{
    Memory::String name;                 ///< 变体名。
    std::uint32_t tag = 0;               ///< 编译期标签。
    Memory::Vector<Parameter> payloads;  ///< 空列表表示无载荷。
    MAGEL_REFLECTION_FIELDS(name, tag, payloads)
};
/**
 * @brief 枚举元数据。
 */
struct EnumInfo
{
    Memory::String name;                                  ///< 完整类型名。
    std::uint64_t typeId = 0;                             ///< 具体类型身份。
    Memory::Vector<VariantInfo> variants;                 ///< 编译期确定的变体。
    Memory::Vector<MethodInfo> methods;                   ///< 方法签名。
    Memory::Vector<StaticMemberInfo> staticMembers;       ///< 静态成员。
    Memory::Vector<std::uint64_t> implementedInterfaces;  ///< 接口传递闭包。
    Memory::Vector<Annotation> annotations;               ///< 类型注解。
    MAGEL_REFLECTION_FIELDS(name, typeId, variants, methods, staticMembers, implementedInterfaces, annotations)
};
/**
 * @brief 接口元数据，始终只记录方法。
 */
struct InterfaceInfo
{
    Memory::String name;                          ///< 完整接口名。
    std::uint64_t typeId = 0;                     ///< 接口身份。
    Memory::Vector<MethodInfo> methods;           ///< 方法及默认实现标志。
    Memory::Vector<std::uint64_t> implementedBy;  ///< 已单态化实现类型。
    Memory::Vector<Annotation> annotations;       ///< 接口注解。
    MAGEL_REFLECTION_FIELDS(name, typeId, methods, implementedBy, annotations)
};
/**
 * @brief 编译产物拥有的完整元数据；查询引用仅在表存活且不修改期间有效。
 */
struct Table
{
    Memory::Vector<Identity> identities;           ///< 签名中使用的具体身份。
    Memory::Vector<StructInfo> structs;            ///< 显式暴露的结构体。
    Memory::Vector<EnumInfo> enums;                ///< 显式暴露的枚举。
    Memory::Vector<InterfaceInfo> interfaces;      ///< 显式暴露的接口。
    Memory::Vector<FuncInfo> functions;            ///< 显式暴露的自由函数。
    Memory::Vector<AnnotationInfo> annotations;    ///< 注解声明。
    Memory::Vector<Annotation> moduleAnnotations;  ///< 模块注解实例。
    MAGEL_REFLECTION_FIELDS(identities, structs, enums, interfaces, functions, annotations, moduleAnnotations)
    /**
     * @brief 判断是否完全没有元数据。
     * @return 所有列表为空时为真。
     */
    bool empty() const
    {
        return identities.empty() && structs.empty() && enums.empty() && interfaces.empty() && functions.empty() && annotations.empty() && moduleAnnotations.empty();
    }
};
#undef MAGEL_REFLECTION_FIELDS
/**
 * @brief 查询唯一编号对应的只读记录。
 * @param values 记录表。
 * @param id 编号。
 * @return 匹配记录，缺失或重号返回空。
 */
template <class T>
inline std::optional<std::reference_wrapper<const T>>
findById(const Memory::Vector<T> &values, std::uint64_t id)
{
    std::optional<std::reference_wrapper<const T>> result;
    for (const auto &value : values)
        if (value.typeId == id)
        {
            if (result) return std::nullopt;
            result = std::cref(value);
        }
    return result;
}
/**
 * @brief 查询唯一名称对应的记录；重载必须改用编号查询。
 * @param values 记录表。
 * @param name 完整名称。
 * @return 唯一匹配或空。
 */
template <class T>
inline std::optional<std::reference_wrapper<const T>>
findByName(const Memory::Vector<T> &values, std::string_view name)
{
    std::optional<std::reference_wrapper<const T>> result;
    for (const auto &value : values)
        if (std::string_view(value.name.data(), value.name.size()) == name)
        {
            if (result) return std::nullopt;
            result = std::cref(value);
        }
    return result;
}
/**
 * @brief 按具体类型编号查询结构体。
 * @param table 元数据。
 * @param id 编号。
 * @return 只读记录或空。
 */
inline auto getStructInfo(const Table &table, std::uint64_t id) { return findById(table.structs, id); }
/**
 * @brief 按具体类型编号查询枚举。
 * @param table 元数据。
 * @param id 编号。
 * @return 只读记录或空。
 */
inline auto getEnumInfo(const Table &table, std::uint64_t id) { return findById(table.enums, id); }
/**
 * @brief 按接口编号查询接口。
 * @param table 元数据。
 * @param id 编号。
 * @return 只读记录或空。
 */
inline auto getInterfaceInfo(const Table &table, std::uint64_t id) { return findById(table.interfaces, id); }
/**
 * @brief 按完整名称查询唯一自由函数。
 * @param table 元数据。
 * @param name 名称。
 * @return 重载或缺失返回空。
 */
inline auto getFuncInfo(const Table &table, std::string_view name) { return findByName(table.functions, name); }
/**
 * @brief 按完整名称查询注解定义。
 * @param table 元数据。
 * @param name 名称。
 * @return 只读定义或空。
 */
inline auto getAnnotationInfo(const Table &table, std::string_view name) { return findByName(table.annotations, name); }
namespace detail
{
/**
 * @brief 内部小端写入器，限制总长度及列表长度。
 */
struct Writer
{
    Memory::Vector<std::uint8_t> bytes;
    template <class... T>
    bool operator()(const T &...values)
    { return (write(values) && ...); }
    template <class T>
    std::enable_if_t<std::is_unsigned_v<T>, bool> write(T value)
    {
        if (bytes.size() > kMaximumBytes - sizeof(T)) return false;
        for (std::size_t i = 0; i < sizeof(T); ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
        return true;
    }
    bool write(bool value) { return write(std::uint8_t(value)); }
    bool write(const Memory::String &value)
    {
        if (value.size() > 1024u * 1024u || !write(std::uint32_t(value.size())) || value.size() > kMaximumBytes - bytes.size()) return false;
        bytes.insert(bytes.end(), value.begin(), value.end());
        return true;
    }
    template <class T>
    bool write(const Memory::Vector<T> &values)
    {
        if (values.size() > kMaximumRecords || !write(std::uint32_t(values.size()))) return false;
        for (const auto &value : values)
            if (!write(value)) return false;
        return true;
    }
    template <class T>
    bool write(const std::optional<T> &value)
    {
        return write(bool(value)) && (!value || write(*value));
    }
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
    template <class... T>
    bool operator()(T &...values)
    { return (read(values) && ...); }
    template <class T>
    std::enable_if_t<std::is_unsigned_v<T>, bool> read(T &value)
    {
        if (sizeof(T) > size - position) return false;
        value = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) value |= T(data[position++]) << (8 * i);
        return true;
    }
    bool read(bool &value)
    {
        std::uint8_t raw = 0;
        if (!read(raw) || raw > 1) return false;
        value = raw != 0;
        return true;
    }
    bool read(Memory::String &value)
    {
        std::uint32_t count = 0;
        if (!read(count) || count > 1024u * 1024u || count > size - position) return false;
        value.assign(reinterpret_cast<const char *>(data + position), count);
        position += count;
        return true;
    }
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
    bool reference(std::uint64_t id) const { return ids.count(id) != 0; }
    template <class... T>
    bool operator()(const T &...values)
    { return (check(values) && ...); }
    bool check(const Constant &v) const { return reference(v.typeId); }
    bool check(const Parameter &v) const { return !v.name.empty() && reference(v.typeId); }
    bool check(const AnnotationParameter &v) const
    {
        return !v.name.empty() && reference(v.typeId) && (!v.defaultValue || (v.defaultValue->typeId == v.typeId && check(*v.defaultValue)));
    }
    bool check(const Annotation &v) const
    {
        const auto info = findById(table.annotations, v.annotationId);
        if (!info || info->get().params.size() != v.args.size()) return false;
        for (std::size_t i = 0; i < v.args.size(); ++i)
            if (!check(v.args[i]) || v.args[i].typeId != info->get().params[i].typeId) return false;
        return true;
    }
    template <class T>
    bool check(const Memory::Vector<T> &values) const
    {
        if (values.size() > kMaximumRecords) return false;
        for (const auto &value : values)
            if (!check(value)) return false;
        return true;
    }
    bool check(std::uint64_t id) const { return reference(id); }
    template <class T>
    bool named(const T &v)
    {
        return !v.name.empty() && reference(v.typeId) && definitions.insert(v.typeId).second;
    }
    template <class T>
    bool uniqueNames(const Memory::Vector<T> &values) const
    {
        Memory::Set<Memory::String> names;
        for (const auto &v : values)
            if (!names.insert(v.name).second) return false;
        return true;
    }
    bool check(const FuncInfo &v) { return named(v) && reference(v.returnTypeId) && uniqueNames(v.params) && (*this)(v.params, v.annotations); }
    bool check(const MethodInfo &v)
    {
        // static T new 是无显式接收者的构造入口；构造体的 self 不属于公开签名。
        return !(v.isConst && (v.isStatic || v.isConstructor)) && check(v.function);
    }
    bool check(const FieldInfo &v) const { return !v.name.empty() && reference(v.typeId) && check(v.annotations); }
    bool check(const StaticMemberInfo &v) const { return !v.name.empty() && reference(v.typeId) && check(v.annotations); }
    bool check(const VariantInfo &v) const { return !v.name.empty() && uniqueNames(v.payloads) && check(v.payloads); }
    bool check(const AnnotationInfo &v) { return named(v) && uniqueNames(v.params) && check(v.params); }
    bool check(const StructInfo &v)
    {
        return named(v) && uniqueNames(v.fields) && uniqueNames(v.staticMembers) && (*this)(v.fields, v.methods, v.staticMembers, v.implementedInterfaces, v.annotations);
    }
    bool check(const EnumInfo &v)
    {
        Memory::Set<std::uint32_t> tags;
        for (const auto &variant : v.variants)
            if (!tags.insert(variant.tag).second) return false;
        return named(v) && uniqueNames(v.variants) && uniqueNames(v.staticMembers) && (*this)(v.variants, v.methods, v.staticMembers, v.implementedInterfaces, v.annotations);
    }
    bool check(const InterfaceInfo &v) { return named(v) && (*this)(v.methods, v.implementedBy, v.annotations); }
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
inline bool validate(const Table &table, Memory::String *error = nullptr)
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
inline bool encode(const Table &table, Memory::Vector<std::uint8_t> &output, Memory::String *error = nullptr)
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
inline bool decode(const std::uint8_t *data, std::size_t size, Table &output, Memory::String *error = nullptr)
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
inline std::optional<Memory::String> toHex(const Table &table)
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
inline bool fromHex(std::string_view text, Table &output)
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
inline std::optional<Memory::String> cppDefinition(const Table &table)
{
    Memory::Vector<std::uint8_t> bytes;
    if (!encode(table, bytes)) return std::nullopt;
    Memory::String code = "\n/** @brief 在调用方分配域构造本产物的元数据。 @return 拥有型表，查询返回只读引用。 */\n"
                          "stackvm::reflection::Table magel_reflection_metadata() {\n"
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
#endif
