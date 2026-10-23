#include "std_sql_ipage.hpp"
#include "../chali_glue.hpp"
#include "../../library/ipage/source/ipage/sql.hpp"
#include "../../library/ipage/source/ipage/dialect.hpp"
#include "../../library/ipage/source/ipage/model.hpp"
#include "../../source/json/json.hpp"
#include <stdexcept>
#include <cmath>
#include <set>

namespace std_sql_ipage_glue
{
/**
 * @brief 为 ChaLi 内置数据库提供 SQLite SQL 适配，保持 ipage 核心独立于数据库实现。
 */
class SqliteDialect final : public ipage::Dialect
{
public:
    /**
     * @brief 返回宿主数据库方言名称。
     * @return SQLite 方言标识。
     */
    std::string_view name() const noexcept override { return "sqlite"; }
    /**
     * @brief 按现有动态 SQL 协议引用已校验的标识符。
     * @param identifier 已通过模型白名单校验的表名或列名。
     * @return 反引号包围的标识符。
     */
    std::string quote(std::string_view identifier) const override { return "`" + std::string(identifier) + "`"; }
    /**
     * @brief 将模型字段映射到 SQLite 存储类型。
     * @param fieldType ipage 字段语义分类。
     * @param memberKind 标量存储分类。
     * @return 建表使用的类型名称。
     */
    std::string columnType(int fieldType, int memberKind) const override
    {
        const auto type = static_cast<ipage::FieldType>(fieldType);
        if (type == ipage::FieldType::ModelId || type == ipage::FieldType::AssociationId || type == ipage::FieldType::Boolean) return "INTEGER";
        if (type == ipage::FieldType::Number) return memberKind == static_cast<int>(ipage::ColumnKind::Integer) ? "INTEGER" : "REAL";
        return "TEXT";
    }
};
/**
 * @brief 验证元数据标识符，拒绝改变 SQL 结构的标点和空白。
 * @param name 表名或列名。
 */
void validateIdentifier(const std::string &name)
{
    if (name.empty()) throw std::invalid_argument("SQL 标识符不能为空");
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        const auto ch = static_cast<unsigned char>(name[index]);
        if (ch >= 128 || ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (index && ch >= '0' && ch <= '9')) continue;
        throw std::invalid_argument("SQL 标识符包含不允许的字符：" + name);
    }
}
/**
 * @brief 校验 JSON 节点确实为数组后返回只读元素序列。
 * @param value 待校验的节点。
 * @return 数组元素引用。
 */
const json::Array &array(const json::Value &value)
{
    const auto *items = value.asArray();
    if (!items) throw std::invalid_argument("SQL 列表选项必须是数组");
    return *items;
}
/**
 * @brief 读取并验证必需的元数据属性。
 * @param object 元数据对象。
 * @param name 属性名称。
 * @return 属性值的只读引用。
 */
const json::Value &required(const json::Value &object, const char *name)
{
    const auto *value = object.find(name);
    if (!value) throw std::invalid_argument(std::string("缺少 SQL 元数据：") + name);
    return *value;
}
/**
 * @brief 提取严格的字符串属性。
 * @param object 元数据对象。
 * @param name 属性名称。
 * @return 字符串副本。
 */
std::string text(const json::Value &object, const char *name)
{
    const auto value = required(object, name).asString();
    if (!value) throw std::invalid_argument(std::string("SQL 属性必须是字符串：") + name);
    return std::string(*value);
}
/**
 * @brief 将参数节点转换为 ipage 的精确单元格类型。
 * @param value 参数节点。
 * @return 可绑定的标量值。
 */
ipage::Cell cell(const json::Value &value)
{
    if (value.isNull()) return ipage::Cell();
    if (value.isBool()) return ipage::Cell(*value.asBool());
    if (value.isInt()) return ipage::Cell(*value.asInt());
    if (value.isDouble() && std::isfinite(*value.asDouble())) return ipage::Cell(*value.asDouble());
    if (value.isString()) return ipage::Cell(std::string(*value.asString()));
    throw std::invalid_argument("SQL 参数只接受 null、布尔、整数、有限浮点数或字符串");
}
/**
 * @brief 将 ipage 参数恢复为 JSON 标量，保持整数及文本类型。
 * @param value 待导出的单元格。
 * @return JSON 参数值。
 */
json::Value parameter(const ipage::Cell &value)
{
    if (value.isNull()) return json::Value();
    if (value.isBool()) return json::Value(*value.asBool());
    if (value.isInt()) return json::Value(*value.asInt());
    if (value.isDouble()) return json::Value(*value.asDouble());
    return json::Value(std::string(*value.asText()));
}
/**
 * @brief 使用白名单元数据生成查询或写入语句，不拼接任何参数值。
 * @param modelText 反射模型元数据 JSON。
 * @param operation 查询、建表、插入、更新或删除操作。
 * @param optionsText 查询选项 JSON。
 * @param valuesText 按字段名组织的写入值 JSON。
 * @return 包含 SQL、参数和错误的 JSON 响应。
 */
Memory::String build(Memory::String modelText, Memory::String operation, Memory::String optionsText, Memory::String valuesText)
{
    json::Object response;
    try
    {
        auto model = json::parse(modelText.c_str()), options = json::parse(optionsText.c_str()), values = json::parse(valuesText.c_str());
        if (!model.ok() || !options.ok() || !values.ok()) throw std::invalid_argument("无效 SQL 请求 JSON");
        const auto &definition = *model.value;
        ipage::ModelMeta meta(text(definition, "name"), text(definition, "table"));
        validateIdentifier(meta.table());
        const auto *fields = required(definition, "fields").asArray();
        if (!fields || fields->empty()) throw std::invalid_argument("SQL 模型没有字段");
        std::vector<int> kinds;
        std::set<std::string> names, columns;
        std::size_t primaryCount = 0;
        for (const auto &field : *fields)
        {
            ipage::FieldInfo info;
            info.name = text(field, "name");
            info.column = text(field, "column");
            validateIdentifier(info.column);
            if (!names.insert(info.name).second || !columns.insert(info.column).second) throw std::invalid_argument("SQL 模型字段或列名重复");
            const auto kind = text(field, "kind");
            const bool primary = required(field, "id").asBool().value_or(false);
            if (primary) ++primaryCount;
            info.type = primary ? ipage::FieldType::ModelId : kind == "text" ? ipage::FieldType::String :
                                                          kind == "bool"     ? ipage::FieldType::Boolean :
                                                                               ipage::FieldType::Number;
            if (kind != "text" && kind != "bool" && kind != "int" && kind != "real") throw std::invalid_argument("不支持的 SQL 字段类型：" + kind);
            kinds.push_back(kind == "text" ? 2 : kind == "bool" ? 3 :
                                             kind == "real"     ? 1 :
                                                                  0);
            meta.add(std::move(info));
        }
        if (primaryCount != 1) throw std::invalid_argument("SQL 模型必须且只能有一个主键");
        meta.seal();
        SqliteDialect dialect;
        ipage::Query query(meta, dialect);
        if (const auto *filters = options.value->find("filters"))
            for (const auto &filter : array(*filters))
            {
                const auto op = required(filter, "op").asInt().value_or(-1);
                if (op < 0 || op > 12) throw std::invalid_argument("无效 SQL 过滤操作符");
                std::vector<ipage::Cell> parameters;
                for (const auto &value : array(required(filter, "values"))) parameters.push_back(cell(value));
                query.filter(text(filter, "field"), static_cast<ipage::OperatorType>(op), std::move(parameters));
            }
        if (const auto *orders = options.value->find("orders"))
            for (const auto &order : array(*orders)) query.orderBy(text(order, "field"), required(order, "desc").asBool().value_or(false) ? ipage::SortType::Desc : ipage::SortType::Asc);
        if (const auto *columns = options.value->find("columns"))
        {
            std::vector<std::string> selected;
            for (const auto &column : array(*columns)) selected.emplace_back(column.asString().value());
            if (!selected.empty()) query.select(std::move(selected));
        }
        if (const auto *limit = options.value->find("limit")) query.limit(limit->asInt().value_or(0));
        if (const auto *offset = options.value->find("offset")) query.offset(offset->asInt().value_or(0));
        if (operation == "count") query.countOnly();
        auto statement = query.build();
        std::string sql = statement.sql();
        std::vector<ipage::Cell> parameters = statement.params();
        if (operation == "create")
        {
            sql = "CREATE TABLE IF NOT EXISTS " + dialect.quote(meta.table()) + " (";
            std::size_t index = 0;
            for (const auto &field : meta.fields())
            {
                if (index) sql += ", ";
                sql += dialect.quote(field.column) + " " + dialect.columnType(static_cast<int>(field.type), kinds[index++]);
                if (field.type == ipage::FieldType::ModelId) sql += " PRIMARY KEY";
            }
            sql += ")";
            parameters.clear();
        }
        else if (operation == "insert" || operation == "update")
        {
            if (!values.value->isObject()) throw std::invalid_argument("写入值必须为字段对象");
            std::string columns, placeholders, assignments;
            std::vector<ipage::Cell> writes;
            for (const auto &member : *values.value->asObject())
            {
                const auto *found = meta.field(member.key);
                if (!found) throw std::invalid_argument("未知 SQL 字段：" + member.key);
                const auto &field = *found;
                if (operation == "update" && field.type == ipage::FieldType::ModelId) continue;
                if (!columns.empty())
                {
                    columns += ", ";
                    placeholders += ", ";
                    assignments += ", ";
                }
                columns += dialect.quote(field.column);
                placeholders += "?";
                assignments += dialect.quote(field.column) + " = ?";
                writes.push_back(cell(member.value));
            }
            if (writes.empty()) throw std::invalid_argument("写入没有可用字段");
            if (operation == "insert")
            {
                sql = "INSERT INTO " + dialect.quote(meta.table()) + " (" + columns + ") VALUES (" + placeholders + ")";
                parameters = std::move(writes);
            }
            else
            {
                const auto where = sql.find(" WHERE ");
                if (where == std::string::npos) throw std::invalid_argument("动态 UPDATE 必须提供过滤条件");
                sql = "UPDATE " + dialect.quote(meta.table()) + " AS " + dialect.quote("t_") + " SET " + assignments + sql.substr(where);
                writes.insert(writes.end(), parameters.begin(), parameters.end());
                parameters = std::move(writes);
            }
        }
        else if (operation == "delete")
        {
            const auto where = sql.find(" WHERE ");
            if (where == std::string::npos) throw std::invalid_argument("动态 DELETE 必须提供过滤条件");
            sql = "DELETE FROM " + dialect.quote(meta.table()) + " AS " + dialect.quote("t_") + sql.substr(where);
        }
        else if (operation != "select" && operation != "count")
            throw std::invalid_argument("未知动态 SQL 操作");
        json::Array params;
        for (const auto &value : parameters) params.push_back(parameter(value));
        response.insert("sql", json::Value(sql));
        response.insert("params", json::Value(std::move(params)));
        response.insert("error", json::Value(""));
    } catch (const std::exception &failure) { response.insert("error", json::Value(failure.what())); }
    return Memory::String(json::Value(std::move(response)).dump().c_str());
}
/**
 * @brief 将 ipage 动态生成函数注册到 std::sql。
 * @param registry 宿主注册表。
 * @param error 注册失败诊断。
 * @return 注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTyped<Memory::String, Memory::String, Memory::String, Memory::String, Memory::String>("std::sql::__ipageBuild", build, error);
}
}  // namespace std_sql_ipage_glue
