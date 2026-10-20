#include "ast_substitution.hpp"
#include "../magel_ast.hpp"
#include <stdexcept>

namespace magel_macro
{
/**
 * @brief 保存节点池引用及单次展开节点预算。
 * @param nodes 编译器现有语法节点池。
 * @param nodeBudget 允许生成的节点上限。
 */
AstSubstitution::AstSubstitution(Memory::Deque<MNode> &nodes, std::size_t nodeBudget) : nodes_(nodes), budget_(nodeBudget) {}

/**
 * @brief 克隆模板并按绑定身份替换占位，失败时回滚节点池。
 * @param root 模板根编号。
 * @param arguments 形参绑定到实参节点的映射。
 * @param context 本次展开的上下文。
 * @param error 接收错误，成功时清空。
 * @return 实例化根编号，失败返回负一。
 */
int AstSubstitution::instantiate(int root, const std::map<BindingId, int> &arguments, ContextId context, std::string &error)
{
    start_ = nodes_.size();
    arguments_ = &arguments;
    context_ = context;
    error.clear();
    try
    {
        if (root < 0) throw std::runtime_error("宏模板没有有效的根节点");
        const auto result = clone(root, true, 0);
        arguments_ = nullptr;
        return result;
    } catch (const std::runtime_error &failure)
    {
        nodes_.resize(start_);
        arguments_ = nullptr;
        error = failure.what();
        return -1;
    } catch (...)
    {
        nodes_.resize(start_);
        arguments_ = nullptr;
        throw;
    }
}

/**
 * @brief 复制既有语法图的结构边，避免把整数值、类型或名称池编号误当子节点。
 * @param id 子树根编号。
 * @param substitute 是否替换宏模板占位。
 * @param depth 当前深度，用于阻止循环语法图或过深展开。
 * @return 新子树根编号。
 */
int AstSubstitution::clone(int id, bool substitute, std::size_t depth)
{
    if (id < 0) return id;
    if (static_cast<std::size_t>(id) >= start_) throw std::runtime_error("宏模板引用了无效的语法节点");
    if (depth >= 256) throw std::runtime_error("宏模板语法深度超过 256 层");
    const auto original = nodes_[id];
    if (substitute && original.kind == MNode::MacroHole)
    {
        const auto argument = arguments_->find(original.bindingIdentity);
        if (argument == arguments_->end()) throw std::runtime_error("宏形参没有绑定实参");
        return clone(argument->second, false, depth + 1);
    }
    if (nodes_.size() - start_ >= budget_) throw std::runtime_error("宏展开生成的语法节点超过预算");
    const int result = static_cast<int>(nodes_.size());
    nodes_.push_back(original);
    if (substitute) nodes_[result].syntaxContext = context_;
    for (auto &child : nodes_[result].kids) child = clone(child, substitute, depth + 1);
    for (auto &annotation : nodes_[result].annotations) annotation = clone(annotation, substitute, depth + 1);
    if (nodes_[result].guard >= 0) nodes_[result].guard = clone(original.guard, substitute, depth + 1);
    const bool declarationGroup = original.kind == MNode::Declare && original.kids.size() == 1 && original.kids[0] >= 0 && static_cast<std::size_t>(original.kids[0]) < start_ && nodes_[original.kids[0]].kind == MNode::List;
    const bool indexedChild = original.kind == MNode::Block || original.kind == MNode::Case || original.kind == MNode::NewPtr || ((original.kind == MNode::Declare || original.kind == MNode::Field) && !declarationGroup && original.i >= 0) || (original.kind == MNode::Index && original.s2 == "__magel_bare_index");
    if (indexedChild && original.i >= 0) nodes_[result].i = clone(static_cast<int>(original.i), substitute, depth + 1);
    return result;
}
}  // namespace magel_macro
