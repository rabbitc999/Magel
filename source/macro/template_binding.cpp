#include "template_binding.hpp"
#include "../magel_ast.hpp"
#include <stdexcept>

namespace magel_macro
{
/**
 * @brief 保存节点池及卫生解析器的引用。
 * @param nodes 编译器节点池。
 * @param resolver 本次编译共用的卫生解析器。
 */
TemplateBinding::TemplateBinding(Memory::Deque<MNode> &nodes, HygieneResolver &resolver) : nodes_(nodes), resolver_(resolver) {}

/**
 * @brief 克隆宏体并建立词法绑定，错误时回滚克隆节点。
 * @param definition 宏定义编号。
 * @param definitionScope 宏定义所在作用域。
 * @param result 接收独立模板及形参映射。
 * @param error 接收错误文本。
 * @return 成功返回真。
 */
bool TemplateBinding::bind(int definition, ScopeId definitionScope, BoundTemplate &result, std::string &error)
{
    const auto start = nodes_.size();
    result = {};
    error.clear();
    parameters_.clear();
    try
    {
        if (definition < 0 || static_cast<std::size_t>(definition) >= start || nodes_[definition].kind != MNode::MacroDefinition || nodes_[definition].kids.size() != 3)
            throw std::runtime_error("宏定义结构无效");
        const auto definitionNode = nodes_[definition];
        const auto scope = resolver_.createScope(definitionScope);
        const int group = definitionNode.kids[0];
        if (group >= 0)
        {
            if (static_cast<std::size_t>(group) >= start || nodes_[group].kind != MNode::MacroParameters)
                throw std::runtime_error("宏参数组结构无效");
            for (const int parameter : nodes_[group].kids)
            {
                if (parameter < 0 || static_cast<std::size_t>(parameter) >= start || nodes_[parameter].kind != MNode::MacroParameter)
                    throw std::runtime_error("宏形参结构无效");
                const auto &name = nodes_[parameter].s;
                const auto identity = resolver_.declare(std::string_view(name.data(), name.size()), scope, definitionNode.syntaxContext);
                if (!identity) throw std::runtime_error("宏形参重复：" + std::string(name.data(), name.size()));
                result.parameters.emplace(std::string(name.data(), name.size()), identity);
                parameters_.insert(identity);
            }
        }
        AstSubstitution substitution(nodes_);
        result.root = substitution.instantiate(definitionNode.kids[1], {}, definitionNode.syntaxContext, error);
        if (result.root < 0) throw std::runtime_error(error);
        visit(result.root, resolver_.createScope(scope), 0);
        return true;
    } catch (const std::exception &failure)
    {
        nodes_.resize(start);
        result = {};
        error = failure.what();
        return false;
    }
}

/**
 * @brief 登记声明身份，保留用户名称用于诊断。
 * @param id 声明节点。
 * @param scope 词法作用域。
 */
void TemplateBinding::declare(int id, ScopeId scope)
{
    auto &node = nodes_[id];
    node.bindingIdentity = resolver_.declare(std::string_view(node.s.data(), node.s.size()), scope, node.syntaxContext);
    if (!node.bindingIdentity) throw std::runtime_error("宏体同一作用域重复声明：" + std::string(node.s.data(), node.s.size()));
}

/**
 * @brief 根据 AST 的真实结构边绑定形参引用和局部变量。
 * @param id 当前节点。
 * @param scope 当前作用域。
 * @param depth 遍历深度。
 */
void TemplateBinding::visit(int id, ScopeId scope, std::size_t depth)
{
    if (id < 0) return;
    if (static_cast<std::size_t>(id) >= nodes_.size() || depth >= 256) throw std::runtime_error("宏绑定遇到无效或过深的语法节点");
    auto &node = nodes_[id];
    if (node.kind == MNode::MacroDefinition) throw std::runtime_error("嵌套宏定义尚未接入模板绑定");
    if (node.kind == MNode::Var)
    {
        node.bindingIdentity = resolver_.resolve(std::string_view(node.s.data(), node.s.size()), scope, node.syntaxContext);
        if (parameters_.count(node.bindingIdentity)) node.kind = MNode::MacroHole;
        return;
    }
    if (node.kind == MNode::Block)
    {
        visit(static_cast<int>(node.i), resolver_.createScope(scope), depth + 1);
        return;
    }
    if (node.kind == MNode::FuncDef || node.kind == MNode::MethodDef || node.kind == MNode::For)
        scope = resolver_.createScope(scope);
    if (node.kind == MNode::Foreach)
    {
        if (node.kids.size() != 2) throw std::runtime_error("宏体遍历循环结构无效");
        visit(node.kids[0], scope, depth + 1);
        scope = resolver_.createScope(scope);
        declare(id, scope);
        visit(node.kids[1], scope, depth + 1);
        return;
    }
    const bool declarationGroup = node.kind == MNode::Declare && node.kids.size() == 1 && node.kids[0] >= 0 && nodes_[node.kids[0]].kind == MNode::List;
    for (const int child : node.kids) visit(child, scope, depth + 1);
    for (const int annotation : node.annotations) visit(annotation, scope, depth + 1);
    visit(node.guard, scope, depth + 1);
    const bool indexedChild = node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !declarationGroup && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__magel_bare_index");
    if (indexedChild) visit(static_cast<int>(node.i), scope, depth + 1);
    if ((node.kind == MNode::Declare && !declarationGroup) || node.kind == MNode::Params) declare(id, scope);
}
}  // namespace magel_macro
