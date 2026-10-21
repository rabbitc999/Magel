#include "ipage/model.hpp"
#include "ipage/sql.hpp"
#include "ipage/dialect.hpp"
#include <iostream>
#include <stdexcept>

/**
 * @brief 校验断言并在失败时报告原因。
 * @param condition 待校验条件。
 * @param message 失败说明。
 */
void expect(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
/**
 * @brief 验证参数保真、白名单、复制隔离和标准 SQL 方言。
 * @return 全部通过时返回零，失败时返回一。
 */
int main()
{
    try
    {
        ipage::ModelMeta model("Person", "people");
        ipage::FieldInfo id;
        id.name = "id";
        id.column = "person_id";
        id.type = ipage::FieldType::ModelId;
        model.add(id);
        ipage::FieldInfo name;
        name.name = "name";
        name.column = "display_name";
        name.type = ipage::FieldType::String;
        model.add(name);
        model.seal();
        const auto &dialect = ipage::defaultDialect();
        expect(dialect.name() == "default", "默认方言错误");
        expect(dialect.quote("people") == "\"people\"", "标识符引用错误");
        expect(ipage::findDialect("ANSI") != nullptr, "标准方言别名错误");
        expect(ipage::findDialect("unknown") == nullptr, "未知方言未拒绝");
        const std::string text = "张三'); DROP TABLE people; --";
        ipage::Query query(model, dialect);
        query.filter("name", ipage::OperatorType::Eq, { ipage::Cell(text) }).orderBy("id").limit(5).offset(2);
        const auto statement = query.build();
        expect(statement.sql().find(text) == std::string::npos, "参数被插入 SQL");
        expect(statement.sql().find("LIMIT 5 OFFSET 2") != std::string::npos, "分页错误");
        expect(statement.params().size() == 1 && statement.params()[0].asText().value() == text, "文本参数丢失");
        auto copy = query;
        copy.filter("id", ipage::OperatorType::Eq, { ipage::Cell(std::int64_t(9223372036854775807LL)) });
        expect(copy.build().params().size() == 2 && query.build().params().size() == 1, "查询复制未隔离");
        expect(copy.build().params()[1].asInt().value() == 9223372036854775807LL, "整数精度丢失");
        bool rejected = false;
        try
        {
            query.filter("missing", ipage::OperatorType::Eq, { ipage::Cell(1) });
        } catch (const std::invalid_argument &) { rejected = true; }
        expect(rejected, "未知列未拒绝");
        ipage::Query count(model, dialect);
        count.countOnly();
        expect(count.build().sql().find("COUNT(*)") != std::string::npos, "计数查询错误");
        std::cout << "ipage SQL core tests OK\n";
        return 0;
    } catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
