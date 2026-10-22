#include "macro/hygiene.hpp"
#include <iostream>
#include <stdexcept>

/**
 * @brief 验证卫生名字身份、定义处查找、调用处参数、嵌套展开和词法遮蔽。
 * @return 全部绑定规则通过时返回零。
 */
int main()
{
    chali_macro::HygieneResolver resolver;
    const auto definition = resolver.createScope(0);
    const auto helper = resolver.declare("helper", definition, 0);
    const auto caller = resolver.createScope(0);
    const auto callerHelper = resolver.declare("helper", caller, 0);
    const auto argument = resolver.declare("temporary", caller, 0);
    const auto expansion = resolver.createExpansion(definition, 0);
    const auto temporary = resolver.declare("temporary", caller, expansion);
    if (!helper || !callerHelper || !argument || !temporary || argument == temporary) return 1;
    if (resolver.resolve("temporary", caller, 0) != argument) return 2;
    if (resolver.resolve("temporary", caller, expansion) != temporary) return 3;
    if (resolver.resolve("helper", caller, expansion) != helper) return 4;
    if (resolver.resolve("helper", caller, 0) != callerHelper) return 5;
    const auto nestedScope = resolver.createScope(caller);
    const auto shadow = resolver.declare("temporary", nestedScope, expansion);
    if (resolver.resolve("temporary", nestedScope, expansion) != shadow) return 6;
    if (resolver.resolve("temporary", caller, expansion) != temporary) return 7;
    if (resolver.resolve("temporary", nestedScope, 0) != argument) return 8;
    const auto second = resolver.createExpansion(definition, 0);
    const auto other = resolver.declare("temporary", caller, second);
    if (!other || other == temporary || resolver.resolve("temporary", caller, second) != other) return 9;
    const auto nestedExpansion = resolver.createExpansion(nestedScope, expansion);
    if (resolver.resolve("temporary", caller, nestedExpansion) != shadow) return 10;
    if (resolver.resolve("helper", caller, nestedExpansion) != helper) return 11;
    if (resolver.resolve("callerOnly", caller, expansion) != 0) return 12;
    if (!resolver.declare("callerOnly", caller, 0)) return 13;
    if (resolver.resolve("callerOnly", caller, expansion) != 0) return 14;
    if (resolver.declare("temporary", caller, expansion) != 0) return 15;
    if (resolver.resolve("missing", caller, nestedExpansion) != 0) return 16;
    bool invalidRejected = false;
    try
    {
        resolver.resolve("helper", caller, 9999);
    } catch (const std::out_of_range &)
    {
        invalidRejected = true;
    }
    if (!invalidRejected) return 17;
    std::cout << "macro hygiene binding passed\n";
    return 0;
}
