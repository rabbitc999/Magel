#include "registry.hpp"
#include "../magel_ast.hpp"

namespace magel_macro
{
/**
 * @brief 保存当前编译的节点池和卫生解析器。
 * @param nodes AST 节点池。
 * @param resolver 绑定和展开上下文解析器。
 */
Registry::Registry(Memory::Deque<MNode> &nodes, HygieneResolver &resolver) : nodes_(nodes), resolver_(resolver) {}

/**
 * @brief 登记模块内唯一的宏定义并预先绑定模板。
 * @param definition 定义节点编号。
 * @param scope 定义处词法作用域。
 * @param error 接收诊断，成功时清空。
 * @return 登记成功返回真。
 */
bool Registry::add(int definition, ScopeId scope, std::string &error)
{
    error.clear();
    if (definition < 0 || static_cast<std::size_t>(definition) >= nodes_.size() || nodes_[definition].kind != MNode::MacroDefinition || nodes_[definition].kids.size() != 3)
    {
        error = "宏注册需要有效的定义节点";
        return false;
    }
    const auto node = nodes_[definition];
    const auto key = std::make_pair(std::string(node.module.data(), node.module.size()), std::string(node.s.data(), node.s.size()));
    if (definitions_.count(key))
    {
        error = "宏重复定义：" + key.second;
        return false;
    }
    Definition entry;
    entry.scope = scope;
    entry.context = node.syntaxContext;
    entry.grouped = node.kids[0] >= 0;
    const int generics = node.kids[2];
    if (generics < 0 || static_cast<std::size_t>(generics) >= nodes_.size() || nodes_[generics].kind != MNode::List)
    {
        error = "宏泛型列表结构无效";
        return false;
    }
    entry.generic = !nodes_[generics].kids.empty();
    TemplateBinding binder(nodes_, resolver_);
    if (!binder.bind(definition, scope, entry.bound, error)) return false;
    if (entry.grouped)
        for (const int parameter : nodes_[node.kids[0]].kids)
        {
            const auto &name = nodes_[parameter].s;
            entry.parameters.push_back(entry.bound.parameters.at(std::string(name.data(), name.size())));
        }
    definitions_.emplace(key, std::move(entry));
    return true;
}

/**
 * @brief 校验宏调用并克隆替换模板，实参保持原始卫生上下文。
 * @param call 调用节点编号。
 * @param error 接收错误，成功时清空。
 * @return 实例化模板编号，失败为负一。
 */
int Registry::instantiate(int call, std::string &error)
{
    error.clear();
    if (call < 0 || static_cast<std::size_t>(call) >= nodes_.size() || nodes_[call].kind != MNode::MacroCall || nodes_[call].kids.empty())
    {
        error = "宏展开需要有效的调用节点";
        return -1;
    }
    const auto node = nodes_[call];
    const auto key = std::make_pair(std::string(node.module.data(), node.module.size()), std::string(node.s.data(), node.s.size()));
    const auto found = definitions_.find(key);
    if (found == definitions_.end())
    {
        error = "未定义宏：" + key.second;
        return -1;
    }
    const auto &entry = found->second;
    if (entry.generic || node.kids.size() > 1)
    {
        error = "宏泛型推导尚未接入语义展开";
        return -1;
    }
    const int group = node.kids[0];
    if (entry.grouped != (group >= 0))
    {
        error = "宏调用的参数组与定义不一致：" + key.second;
        return -1;
    }
    if (group >= 0 && (static_cast<std::size_t>(group) >= nodes_.size() || nodes_[group].kind != MNode::List))
    {
        error = "宏实参组结构无效";
        return -1;
    }
    if (entry.parameters.size() != (group < 0 ? 0 : nodes_[group].kids.size()))
    {
        error = "宏实参数量与形参不一致：" + key.second;
        return -1;
    }
    std::map<BindingId, int> arguments;
    if (!checkReferences(entry.bound.root, 0, error)) return -1;
    for (std::size_t index = 0; index < entry.parameters.size(); ++index)
        arguments.emplace(entry.parameters[index], nodes_[group].kids[index]);
    if (remainingNodes_ == 0)
    {
        error = "宏展开生成的语法节点超过编译预算";
        return -1;
    }
    AstSubstitution substitution(nodes_, remainingNodes_);
    const auto start = nodes_.size();
    const int expanded = substitution.instantiate(entry.bound.root, arguments, resolver_.createExpansion(entry.scope, entry.context), error);
    if (expanded >= 0) remainingNodes_ -= nodes_.size() - start;
    return expanded;
}

/**
 * @brief 拒绝尚不能通过绑定身份降级的辅助引用，避免退回调用处捕获。
 * @param root 模板节点编号。
 * @param depth 当前递归深度。
 * @param error 接收诊断文本。
 * @return 检查成功返回真。
 */
bool Registry::checkReferences(int root, std::size_t depth, std::string &error) const
{
    if (root < 0) return true;
    if (depth >= 256 || static_cast<std::size_t>(root) >= nodes_.size())
    {
        error = "宏引用结构无效或过深";
        return false;
    }
    const auto &node = nodes_[root];
    if (node.kind == MNode::Var || (node.kind == MNode::Call && !node.s.empty()) || node.kind == MNode::EnumCtor)
    {
        error = "宏体辅助名称的定义处绑定尚未接入：" + std::string(node.s.data(), node.s.size());
        return false;
    }
    for (const int child : node.kids)
        if (!checkReferences(child, depth + 1, error)) return false;
    for (const int annotation : node.annotations)
        if (!checkReferences(annotation, depth + 1, error)) return false;
    if (!checkReferences(node.guard, depth + 1, error)) return false;
    const bool declarationGroup = node.kind == MNode::Declare && node.kids.size() == 1 && node.kids[0] >= 0 && nodes_[node.kids[0]].kind == MNode::List;
    const bool indexedChild = node.kind == MNode::Block || node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !declarationGroup && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__magel_bare_index");
    return !indexedChild || checkReferences(static_cast<int>(node.i), depth + 1, error);
}
}  // namespace magel_macro
