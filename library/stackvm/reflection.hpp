/**
 * @file reflection.hpp
 * @brief 反射元数据的值模型、只读查询和有界二进制编解码。
 */
#ifndef CHALI_REFLECTION_HPP
#define CHALI_REFLECTION_HPP
#include "../../source/allocator/chali_allocator.hpp"
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
std::uint64_t stableId(std::string_view key);

/**
 * @brief 为所有记录提供统一字段访问，不存储函数指针或对象地址。
 */
#define CHALI_REFLECTION_FIELDS(...) \
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
    CHALI_REFLECTION_FIELDS(id, key)
};
/**
 * @brief 编译期常量，不含 VM 句柄；数字保存原始位模式。
 */
struct Constant
{
    std::uint64_t typeId = 0;  ///< 精确类型编号。
    std::uint64_t bits = 0;    ///< 整数或 IEEE 754 位模式。
    Memory::String text;       ///< 字符串内容，允许内嵌零字节。
    CHALI_REFLECTION_FIELDS(typeId, bits, text)
};
/**
 * @brief 一次注解使用，实参按声明顺序排列并包含默认值。
 */
struct Annotation
{
    std::uint64_t annotationId = 0;  ///< 注解定义编号。
    Memory::Vector<Constant> args;   ///< 已校验的参数值。
    CHALI_REFLECTION_FIELDS(annotationId, args)
};
/**
 * @brief 函数或枚举载荷的具名参数。
 */
struct Parameter
{
    Memory::String name;       ///< 源码名称。
    std::uint64_t typeId = 0;  ///< 精确类型编号。
    CHALI_REFLECTION_FIELDS(name, typeId)
};
/**
 * @brief 注解声明的参数与可选默认值。
 */
struct AnnotationParameter
{
    Memory::String name;                   ///< 参数名称。
    std::uint64_t typeId = 0;              ///< 精确类型编号。
    std::optional<Constant> defaultValue;  ///< 缺省参数值。
    CHALI_REFLECTION_FIELDS(name, typeId, defaultValue)
};
/**
 * @brief 注解定义，区别于某次使用的 Annotation。
 */
struct AnnotationInfo
{
    Memory::String name;                         ///< 完整声明名称。
    std::uint64_t typeId = 0;                    ///< 注解身份编号。
    Memory::Vector<AnnotationParameter> params;  ///< 参数声明顺序。
    CHALI_REFLECTION_FIELDS(name, typeId, params)
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
    CHALI_REFLECTION_FIELDS(name, typeId, params, returnTypeId, annotations)
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
    CHALI_REFLECTION_FIELDS(function, isStatic, isConstructor, isConst, hasDefaultBody)
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
    CHALI_REFLECTION_FIELDS(name, offset, typeId, isOwned, annotations)
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
    CHALI_REFLECTION_FIELDS(name, typeId, isOwned, annotations)
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
    CHALI_REFLECTION_FIELDS(name, typeId, fields, methods, staticMembers, implementedInterfaces, annotations)
};
/**
 * @brief 枚举变体，保留 CHALI 原生多个具名载荷。
 */
struct VariantInfo
{
    Memory::String name;                 ///< 变体名。
    std::uint32_t tag = 0;               ///< 编译期标签。
    Memory::Vector<Parameter> payloads;  ///< 空列表表示无载荷。
    CHALI_REFLECTION_FIELDS(name, tag, payloads)
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
    CHALI_REFLECTION_FIELDS(name, typeId, variants, methods, staticMembers, implementedInterfaces, annotations)
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
    CHALI_REFLECTION_FIELDS(name, typeId, methods, implementedBy, annotations)
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
    CHALI_REFLECTION_FIELDS(identities, structs, enums, interfaces, functions, annotations, moduleAnnotations)
    /**
     * @brief 判断是否完全没有元数据。
     * @return 所有列表为空时为真。
     */
    bool empty() const
    {
        return identities.empty() && structs.empty() && enums.empty() && interfaces.empty() && functions.empty() && annotations.empty() && moduleAnnotations.empty();
    }
};
#undef CHALI_REFLECTION_FIELDS
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

/**
 * @brief 校验身份碰撞、声明重复和精确类型引用。
 * @param table 元数据。
 * @param error 失败时写入错误文本。
 * @return 合法返回真。
 */
bool validate(const Table &table, Memory::String *error = nullptr);

/**
 * @brief 编码独立元数据段；失败不修改输出。
 * @param table 元数据。
 * @param output 字节输出。
 * @param error 错误文本。
 * @return 成功为真。
 */
bool encode(const Table &table, Memory::Vector<std::uint8_t> &output, Memory::String *error = nullptr);

/**
 * @brief 解码并校验整个段；失败不修改现有表。
 * @param data 字节起点。
 * @param size 段长。
 * @param output 输出表。
 * @param error 错误文本。
 * @return 成功为真；不接受未知版本、尾部数据或截断输入。
 */
bool decode(const std::uint8_t *data, std::size_t size, Table &output, Memory::String *error = nullptr);

/**
 * @brief 将元数据编码为四元式注释所用的十六进制文本。
 * @param table 表。
 * @return 编码文本；非法表返回空。
 */
std::optional<Memory::String> toHex(const Table &table);

/**
 * @brief 解码四元式注释的十六进制元数据，失败不修改输出。
 * @param text 编码文本。
 * @param output 输出表。
 * @return 合法返回真。
 */
bool fromHex(std::string_view text, Table &output);

/**
 * @brief 生成自包含导出的静态编码数据及拥有型表工厂。
 * @param table 表。
 * @return C++ 定义；非法表返回空。
 */
std::optional<Memory::String> cppDefinition(const Table &table);
}  // namespace stackvm::reflection
#endif
