#include "ipage/model.hpp"
#include "ipage/sql.hpp"
#include "ipage/dialect.hpp"
#include <iostream>

/**
 * @brief 演示不依赖数据库连接的参数化查询。
 * @return 正常生成 SQL 时返回零。
 */
int main()
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
    ipage::Query query(model, ipage::defaultDialect());
    query.filter("name", ipage::OperatorType::Eq, { ipage::Cell("张三") }).orderBy("id").limit(10);
    const auto statement = query.build();
    std::cout << statement.sql() << '\n'
              << "参数数量：" << statement.params().size() << '\n';
    return 0;
}
