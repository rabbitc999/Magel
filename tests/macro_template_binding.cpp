#include "chali_ast.hpp"
#include "macro/template_binding.hpp"
#include "macro/registry.hpp"
#include <iostream>

/**
 * @brief 验证形参占位、局部遮蔽、循环变量、成员名与失败回滚。
 * @return 全部绑定与替换检查通过时返回零。
 */
int main()
{
    Memory::Deque<MNode> nodes;
    auto add = [&](MNode::Kind kind, const char *name, Memory::Vector<int> children) {
        MNode node;
        node.kind = kind;
        node.s = name;
        node.kids = std::move(children);
        nodes.push_back(node);
        return static_cast<int>(nodes.size() - 1);
    };
    const int parameter = add(MNode::MacroParameter, "x", {});
    const int parameters = add(MNode::MacroParameters, "", { parameter });
    const int initial = add(MNode::Var, "x", {});
    const int declaration = add(MNode::Declare, "x", { initial });
    nodes[declaration].i = -1;
    const int localUse = add(MNode::Var, "x", {});
    const int member = add(MNode::Member, "x", { localUse });
    const int blockList = add(MNode::List, "", { declaration, member });
    const int block = add(MNode::Block, "", {});
    nodes[block].i = blockList;
    const int source = add(MNode::Var, "x", {});
    const int iterationUse = add(MNode::Var, "x", {});
    const int iteration = add(MNode::Foreach, "x", { source, iterationUse });
    const int statements = add(MNode::List, "", { block, iteration });
    const int tail = add(MNode::Var, "x", {});
    const int body = add(MNode::MacroTemplate, "", { statements, tail });
    const int generics = add(MNode::List, "", {});
    const int definition = add(MNode::MacroDefinition, "TEST", { parameters, body, generics });
    chali_macro::HygieneResolver resolver;
    chali_macro::TemplateBinding binder(nodes, resolver);
    chali_macro::BoundTemplate bound;
    std::string error;
    if (!binder.bind(definition, 0, bound, error) || !error.empty()) return 1;
    const auto identity = bound.parameters.at("x");
    const auto &copy = nodes[bound.root];
    if (nodes[copy.kids[1]].kind != MNode::MacroHole || nodes[copy.kids[1]].bindingIdentity != identity) return 2;
    const auto &boundStatements = nodes[copy.kids[0]];
    const auto &inner = nodes[static_cast<int>(nodes[boundStatements.kids[0]].i)];
    const auto &local = nodes[inner.kids[0]];
    if (!local.bindingIdentity || local.bindingIdentity == identity || nodes[local.kids[0]].kind != MNode::MacroHole) return 3;
    const auto &boundMember = nodes[inner.kids[1]];
    if (boundMember.kind != MNode::Member || boundMember.s != "x" || nodes[boundMember.kids[0]].kind != MNode::Var || nodes[boundMember.kids[0]].bindingIdentity != local.bindingIdentity) return 4;
    const auto &loop = nodes[boundStatements.kids[1]];
    if (nodes[loop.kids[0]].kind != MNode::MacroHole || nodes[loop.kids[1]].kind != MNode::Var || nodes[loop.kids[1]].bindingIdentity != loop.bindingIdentity) return 5;
    if (nodes[tail].kind != MNode::Var || nodes[initial].bindingIdentity || nodes[declaration].bindingIdentity) return 6;
    const int value = add(MNode::Int, "", {});
    nodes[value].i = 42;
    chali_macro::AstSubstitution substitution(nodes);
    const int expanded = substitution.instantiate(nodes[bound.root].kids[1], { { identity, value } }, 1, error);
    if (expanded < 0 || nodes[expanded].kind != MNode::Int) return 7;
    chali_macro::BoundTemplate second;
    if (!binder.bind(definition, 0, second, error) || second.parameters.at("x") == identity) return 8;
    nodes[parameters].kids.push_back(parameter);
    const auto saved = nodes.size();
    if (binder.bind(definition, 0, second, error) || error.empty() || second.root != -1 || !second.parameters.empty() || nodes.size() != saved) return 9;
    nodes[parameters].kids.pop_back();
    nodes[blockList].kids.push_back(declaration);
    if (binder.bind(definition, 0, second, error) || error.empty() || nodes.size() != saved) return 10;
    nodes[blockList].kids.pop_back();
    const int empty = add(MNode::List, "", {});
    const int expressionBody = add(MNode::MacroTemplate, "", { empty, tail });
    const int expressionDefinition = add(MNode::MacroDefinition, "IDENTITY", { parameters, expressionBody, generics });
    chali_macro::Registry registry(nodes, resolver);
    if (!registry.add(expressionDefinition, 0, error)) return 11;
    const auto registered = nodes.size();
    if (registry.add(expressionDefinition, 0, error) || error.empty() || nodes.size() != registered) return 12;
    const int caller = add(MNode::Var, "caller", {});
    nodes[caller].syntaxContext = 0;
    nodes[caller].bindingIdentity = 1000;
    const int arguments = add(MNode::List, "", { caller });
    const int call = add(MNode::MacroCall, "IDENTITY", { arguments });
    const int firstExpansion = registry.instantiate(call, error);
    if (firstExpansion < 0 || !error.empty()) return 13;
    const auto &actual = nodes[nodes[firstExpansion].kids[1]];
    if (actual.kind != MNode::Var || actual.s != "caller" || actual.bindingIdentity != 1000 || actual.syntaxContext != 0) return 14;
    const int nextExpansion = registry.instantiate(call, error);
    if (nextExpansion < 0 || nodes[firstExpansion].syntaxContext == nodes[nextExpansion].syntaxContext) return 15;
    nodes[call].kids[0] = -1;
    const auto beforeInvalid = nodes.size();
    if (registry.instantiate(call, error) >= 0 || error.empty() || nodes.size() != beforeInvalid) return 16;
    nodes[call].kids[0] = empty;
    if (registry.instantiate(call, error) >= 0 || error.empty() || nodes.size() != beforeInvalid) return 17;
    nodes[call].s = "MISSING";
    if (registry.instantiate(call, error) >= 0 || error.empty()) return 18;
    const int dimensionUse = add(MNode::Var, "x", {});
    const int dimensions = add(MNode::List, "", { dimensionUse });
    const int array = add(MNode::Declare, "x", { -1 });
    nodes[array].i = dimensions;
    const int arrayStatements = add(MNode::List, "", { array });
    const int arrayBody = add(MNode::MacroTemplate, "", { arrayStatements, -1 });
    const int arrayDefinition = add(MNode::MacroDefinition, "ARRAY", { parameters, arrayBody, generics });
    if (!binder.bind(arrayDefinition, 0, second, error)) return 19;
    const auto &boundArray = nodes[nodes[nodes[second.root].kids[0]].kids[0]];
    const auto &boundDimension = nodes[nodes[static_cast<int>(boundArray.i)].kids[0]];
    if (boundDimension.kind != MNode::MacroHole || boundDimension.bindingIdentity != second.parameters.at("x") || boundArray.bindingIdentity == boundDimension.bindingIdentity) return 20;
    std::cout << "macro template binding passed\n";
    return 0;
}
