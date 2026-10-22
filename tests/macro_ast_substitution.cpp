#include "chali_ast.hpp"
#include "macro/ast_substitution.hpp"
#include <iostream>

/**
 * @brief 验证 AST 替换保留优先级、重复求值结构、卫生来源和失败回滚。
 * @return 所有结构及预算检查通过时返回零。
 */
int main()
{
    Memory::Deque<MNode> nodes;
    MNode argument;
    argument.kind = MNode::Bin;
    argument.s = "+";
    argument.syntaxContext = 3;
    argument.sourceLine = 12;
    argument.kids = { 1, 2 };
    nodes.push_back(argument);
    MNode left;
    left.kind = MNode::Var;
    left.s = "a";
    left.syntaxContext = 3;
    left.bindingIdentity = 10;
    nodes.push_back(left);
    MNode right = left;
    right.s = "b";
    right.bindingIdentity = 11;
    nodes.push_back(right);
    MNode hole;
    hole.kind = MNode::MacroHole;
    hole.bindingIdentity = 42;
    nodes.push_back(hole);
    MNode product;
    product.kind = MNode::Bin;
    product.s = "*";
    product.kids = { 3, 3 };
    nodes.push_back(product);
    chali_macro::AstSubstitution substitute(nodes);
    std::string error;
    const int expanded = substitute.instantiate(4, { { 42, 0 } }, 99, error);
    if (expanded < 0 || !error.empty()) return 1;
    const auto &result = nodes[expanded];
    if (result.kind != MNode::Bin || result.s != "*" || result.syntaxContext != 99) return 2;
    if (result.kids[0] == result.kids[1]) return 3;
    for (const auto child : result.kids)
    {
        const auto &value = nodes[child];
        if (value.kind != MNode::Bin || value.s != "+" || value.syntaxContext != 3 || value.sourceLine != 12) return 4;
        if (nodes[value.kids[0]].bindingIdentity != 10 || nodes[value.kids[1]].bindingIdentity != 11) return 5;
    }
    if (nodes[4].kids != product.kids || nodes[0].kids != argument.kids) return 6;
    const auto saved = nodes.size();
    if (substitute.instantiate(4, {}, 100, error) >= 0 || error.empty() || nodes.size() != saved) return 7;
    chali_macro::AstSubstitution limited(nodes, 2);
    if (limited.instantiate(4, { { 42, 0 } }, 100, error) >= 0 || error.empty() || nodes.size() != saved) return 8;
    if (substitute.instantiate(999999, {}, 100, error) >= 0 || error.empty() || nodes.size() != saved) return 9;
    MNode declarations;
    declarations.kind = MNode::List;
    const int list = static_cast<int>(nodes.size());
    nodes.push_back(declarations);
    MNode group;
    group.kind = MNode::Declare;
    group.kids = { list };
    const int groupId = static_cast<int>(nodes.size());
    nodes.push_back(group);
    MNode statements;
    statements.kind = MNode::List;
    statements.kids = { groupId };
    const int statementId = static_cast<int>(nodes.size());
    nodes.push_back(statements);
    MNode block;
    block.kind = MNode::Block;
    block.i = statementId;
    const int blockId = static_cast<int>(nodes.size());
    nodes.push_back(block);
    const int copied = substitute.instantiate(blockId, {}, 101, error);
    if (copied < 0 || !error.empty()) return 10;
    const int copiedStatements = static_cast<int>(nodes[copied].i);
    if (copiedStatements == statementId || nodes[copiedStatements].kids.size() != 1) return 11;
    const auto &copiedGroup = nodes[nodes[copiedStatements].kids[0]];
    if (copiedGroup.i != 0 || copiedGroup.kids[0] == list) return 12;
    const auto beforeFailure = nodes.size();
    if (substitute.instantiate(-1, {}, 102, error) >= 0 || error.empty() || nodes.size() != beforeFailure) return 13;
    MNode cycle;
    cycle.kind = MNode::List;
    const int cyclic = static_cast<int>(nodes.size());
    cycle.kids = { cyclic };
    nodes.push_back(cycle);
    const auto beforeCycle = nodes.size();
    if (substitute.instantiate(cyclic, {}, 102, error) >= 0 || error.empty() || nodes.size() != beforeCycle) return 14;
    std::cout << "macro AST substitution passed\n";
    return 0;
}
