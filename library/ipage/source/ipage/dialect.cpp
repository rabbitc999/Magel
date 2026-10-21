/**
 * @file dialect.cpp
 * @brief `Dialect` 的列类型映射与方言注册表。
 *
 * @author 小兔糖糖OvO
 * @version 1.0
 * @since 1.0
 */

#include "dialect.hpp"

#include "field.hpp"

#include <algorithm>
#include <cctype>

namespace ipage
{

/**
 * @brief 根据字段语义和标量分类选择标准 SQL 列类型。
 * @param fieldType 字段语义枚举值。
 * @param memberKind 标量存储分类。
 * @return 标准列类型名称。
 */
std::string Dialect::columnType(int fieldType, int memberKind) const
{
    const auto type = static_cast<FieldType>(fieldType);
    const auto kind = static_cast<ColumnKind>(memberKind);
    switch (type)
    {
    case FieldType::ModelId:
    case FieldType::AssociationId:
        return "INTEGER";
    case FieldType::Boolean:
        return "INTEGER";
    case FieldType::Date:
    case FieldType::String:
    case FieldType::Enum:
        return "TEXT";
    case FieldType::AssociationModel:
    case FieldType::Aggregation:
        // 关联 / 聚合字段不入库，调用方不应走到这里；给个安全的兜底。
        return "TEXT";
    case FieldType::Number:
        break;
    }
    switch (kind)
    {
    case ColumnKind::Integer:
    case ColumnKind::Boolean:
        return "INTEGER";
    case ColumnKind::Real:
        return "REAL";
    case ColumnKind::Text:
        break;
    }
    return "TEXT";
}

/**
 * @brief 取得数据库无关的默认标准 SQL 方言。
 * @return 静态生命周期的只读方言引用。
 */
const Dialect &defaultDialect()
{
    static const DefaultDialect dialect;
    return dialect;
}

/**
 * @brief 查询标准 SQL 方言别名。
 * @param name 不区分大小写的 default、standard 或 ansi。
 * @return 标准方言指针；未知名称返回空指针。
 */
const Dialect *findDialect(std::string_view name) noexcept
{
    static const DefaultDialect standard;

    std::string lowered(name);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (lowered == "default" || lowered == "standard" || lowered == "ansi")
    {
        return &standard;
    }
    return nullptr;
}

}  // namespace ipage
