#include "ast_substitution.hpp"
#include "../chali_ast.hpp"
#include <stdexcept>

namespace chali_macro
{
/**
 * @brief 保存节点池引用及单次展开节点预算。
 * @param nodes 编译器现有语法节点池。
 * @param nodeBudget 允许生成的节点上限。
 * @param types 可选的共享类型池。
 */
AstSubstitution::AstSubstitution(Memory::Deque<MNode> &nodes, std::size_t nodeBudget, Memory::Vector<std::shared_ptr<MType>> *types) : nodes_(nodes), budget_(nodeBudget), types_(types) {}

/**
 * @brief 克隆模板并按绑定身份替换占位，失败时回滚节点池。
 * @param root 模板根编号。
 * @param arguments 形参绑定到实参节点的映射。
 * @param context 本次展开的上下文。
 * @param error 接收错误，成功时清空。
 * @param site 调用处来源及展开帧。
 * @return 实例化根编号，失败返回负一。
 */
int AstSubstitution::instantiate(int root, const std::map<BindingId, int> &arguments, ContextId context, std::string &error, const ExpansionSite &site)
{
    start_ = nodes_.size();
    const auto typeStart = types_ ? types_->size() : 0;
    arguments_ = &arguments;
    context_ = context;
    site_ = site;
    names_.clear();
    preserveVariadicGroups_ = false;
    error.clear();
    try
    {
        if (root < 0) throw std::runtime_error("宏模板没有有效的根节点");
        collectNames(root, 0);
        const auto result = clone(root, true, 0);
        arguments_ = nullptr;
        return result;
    } catch (const std::runtime_error &failure)
    {
        nodes_.resize(start_);
        if (types_) types_->resize(typeStart);
        arguments_ = nullptr;
        error = failure.what();
        return -1;
    } catch (...)
    {
        nodes_.resize(start_);
        if (types_) types_->resize(typeStart);
        arguments_ = nullptr;
        throw;
    }
}

/**
 * @brief 绑定声明名的标识符实参，并将局部引用关联到同一调用者名字。
 * @param id 模板节点编号。
 * @param depth 当前深度。
 */
void AstSubstitution::collectNames(int id, std::size_t depth)
{
    if (id < 0) return;
    if (static_cast<std::size_t>(id) >= start_ || depth >= 256) throw std::runtime_error("宏名称占位结构无效或过深");
    const auto &node = nodes_[id];
    if (node.nameParameter && node.kind != MNode::Member && node.kind != MNode::Arrow && node.kind != MNode::EnumCtor && node.kind != MNode::TypeQualifiedRef)
    {
        const auto found = arguments_->find(node.nameParameter);
        if (found == arguments_->end()) throw std::runtime_error("宏声明名称没有绑定实参");
        int argument = found->second;
        if (argument >= 0 && static_cast<std::size_t>(argument) < start_ && nodes_[argument].kind == MNode::MacroArgument && nodes_[argument].kids.size() == 5)
            argument = nodes_[argument].kids[1];
        if (argument < 0 || static_cast<std::size_t>(argument) >= start_ || nodes_[argument].kind != MNode::Var || !nodes_[argument].kids.empty() || nodes_[argument].s.find("::") != Memory::String::npos)
            throw std::runtime_error("宏声明名称必须来自单个标识符实参");
        names_[node.bindingIdentity] = argument;
    }
    for (const int child : node.kids) collectNames(child, depth + 1);
    for (const int annotation : node.annotations) collectNames(annotation, depth + 1);
    collectNames(node.guard, depth + 1);
    const bool group = node.kind == MNode::Declare && node.kids.size() == 1 && node.kids[0] >= 0 && nodes_[node.kids[0]].kind == MNode::List;
    const bool indexed = node.kind == MNode::Block || node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !group && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__chali_bare_index");
    if (indexed) collectNames(static_cast<int>(node.i), depth + 1);
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
    if (substitute && original.kind == MNode::ExprStmt && original.kids.size() == 1 && original.kids[0] >= 0 && nodes_[original.kids[0]].kind == MNode::MacroHole)
    {
        const auto found = arguments_->find(nodes_[original.kids[0]].bindingIdentity);
        if (found != arguments_->end() && found->second >= 0 && static_cast<std::size_t>(found->second) < start_)
        {
            const auto &argument = nodes_[found->second];
            if (argument.kind == MNode::MacroArgument && argument.kids.size() == 5 && argument.kids[4] >= 0)
                return clone(argument.kids[4], false, depth + 1);
        }
    }
    if (substitute && (original.kind == MNode::MacroHole || (original.kind == MNode::MacroArgument && original.bindingIdentity && arguments_->count(original.bindingIdentity))))
    {
        const auto argument = arguments_->find(original.bindingIdentity);
        if (argument == arguments_->end()) throw std::runtime_error("宏形参没有绑定实参");
        int value = argument->second;
        if (original.kind == MNode::MacroHole && value >= 0 && static_cast<std::size_t>(value) < start_ && nodes_[value].kind == MNode::MacroArgument)
        {
            if (nodes_[value].kids.size() != 5 || nodes_[value].kids[0] < 0) throw std::runtime_error("宏形参在表达式位置需要完整表达式实参");
            value = nodes_[value].kids[0];
        }
        return clone(value, false, depth + 1);
    }
    if (nodes_.size() - start_ >= budget_) throw std::runtime_error("宏展开生成的语法节点超过预算");
    const int result = static_cast<int>(nodes_.size());
    nodes_.push_back(original);
    if (substitute && types_ && original.type >= 0)
    {
        auto type = cloneType(types_->at(original.type), depth);
        nodes_[result].type = static_cast<int>(types_->size());
        types_->push_back(std::move(type));
    }
    if (!substitute && nodes_[result].macroExpansion < 0 && site_.frame >= 0)
        nodes_[result].macroExpansion = site_.frame;
    if (substitute)
    {
        nodes_[result].syntaxContext = context_;
        if (site_.line)
        {
            nodes_[result].expansionLine = site_.line;
            nodes_[result].expansionColumn = site_.column;
            nodes_[result].sourceLine = site_.line;
            nodes_[result].sourceColumn = site_.column;
            nodes_[result].expansionModule.assign(site_.module.data(), site_.module.size());
            nodes_[result].macroExpansion = site_.frame;
        }
    }
    if (substitute && original.nameParameter && (original.kind == MNode::Member || original.kind == MNode::Arrow || original.kind == MNode::EnumCtor || original.kind == MNode::TypeQualifiedRef))
    {
        const auto actual = arguments_->find(original.nameParameter);
        if (actual == arguments_->end()) throw std::runtime_error("宏成员名称没有绑定实参");
        int value = actual->second;
        if (nodes_[value].kind == MNode::MacroArgument && nodes_[value].kids.size() == 5) value = nodes_[value].kids[1];
        if (value < 0 || nodes_[value].kind != MNode::Var || !nodes_[value].kids.empty() || nodes_[value].s.find("::") != Memory::String::npos)
            throw std::runtime_error("宏成员名称必须来自单个标识符实参");
        if (original.kind == MNode::EnumCtor || original.kind == MNode::TypeQualifiedRef)
            nodes_[result].s2 = nodes_[value].s;
        else
            nodes_[result].s = nodes_[value].s;
        nodes_[result].nameParameter = 0;
    }
    else if (substitute && (original.nameParameter || original.kind == MNode::Var))
    {
        const auto name = names_.find(original.bindingIdentity);
        if (name != names_.end())
        {
            const auto &argument = nodes_[name->second];
            nodes_[result].s = argument.s;
            nodes_[result].syntaxContext = argument.syntaxContext;
            nodes_[result].bindingContext = context_;
            nodes_[result].nameParameter = 0;
        }
    }
    const bool previousGroupPolicy = preserveVariadicGroups_;
    if (original.kind == MNode::MacroCall) preserveVariadicGroups_ = original.s == "stringify";
    for (auto &child : nodes_[result].kids) child = clone(child, substitute, depth + 1);
    if (nodes_[result].kind == MNode::List || nodes_[result].kind == MNode::Tuple || (nodes_[result].kind == MNode::MacroArgumentGroup && !preserveVariadicGroups_))
    {
        Memory::Vector<int> items;
        for (const int child : nodes_[result].kids)
            if (child >= 0 && nodes_[child].kind == MNode::MacroVariadic)
            {
                for (const int item : nodes_[child].kids)
                {
                    if ((nodes_[result].kind == MNode::List || nodes_[result].kind == MNode::Tuple) && item >= 0 && nodes_[item].kind == MNode::MacroArgument)
                    {
                        if (nodes_[item].kids.size() != 5 || nodes_[item].kids[0] < 0)
                            throw std::runtime_error("变长实参在表达式列表中需要完整表达式");
                        items.push_back(nodes_[item].kids[0]);
                    }
                    else
                        items.push_back(item);
                }
            }
            else
                items.push_back(child);
        nodes_[result].kids = std::move(items);
    }
    if (original.kind == MNode::GenericLambda && original.type2 >= 0)
        nodes_[result].type2 = clone(original.type2, substitute, depth + 1);
    for (auto &annotation : nodes_[result].annotations) annotation = clone(annotation, substitute, depth + 1);
    if (nodes_[result].guard >= 0) nodes_[result].guard = clone(original.guard, substitute, depth + 1);
    const bool declarationGroup = original.kind == MNode::Declare && original.kids.size() == 1 && original.kids[0] >= 0 && static_cast<std::size_t>(original.kids[0]) < start_ && nodes_[original.kids[0]].kind == MNode::List;
    const bool indexedChild = original.kind == MNode::Block || original.kind == MNode::Case || original.kind == MNode::NewPtr || ((original.kind == MNode::Declare || original.kind == MNode::Field) && !declarationGroup && original.i >= 0) || (original.kind == MNode::Index && original.s2 == "__chali_bare_index");
    if (indexedChild && original.i >= 0) nodes_[result].i = clone(static_cast<int>(original.i), substitute, depth + 1);
    preserveVariadicGroups_ = previousGroupPolicy;
    return result;
}

/**
 * @brief 在类型位置取用同一形参的完整类型候选，递归保留引用及嵌套类型应用。
 * @param type 模板类型。
 * @param depth 当前递归深度。
 * @return 克隆或替换后的类型。
 */
std::shared_ptr<MType> AstSubstitution::cloneType(const std::shared_ptr<MType> &type, std::size_t depth)
{
    if (!type) return nullptr;
    if (depth >= 256) throw std::runtime_error("宏类型片段超过类型深度预算");
    if (type->macroBinding)
    {
        const auto actual = arguments_->find(type->macroBinding);
        if (actual == arguments_->end()) throw std::runtime_error("宏类型形参没有绑定实参");
        const auto &fragment = nodes_[actual->second];
        if (fragment.kind != MNode::MacroArgument || fragment.kids.size() != 5 || fragment.kids[2] < 0)
            throw std::runtime_error("宏形参在类型位置需要完整类型实参");
        const int typeId = nodes_[fragment.kids[2]].type;
        if (typeId < 0) throw std::runtime_error("宏类型片段没有类型描述");
        if (type->genericMember.empty()) return types_->at(typeId);
        auto qualified = Memory::makeShared<MType>(*types_->at(typeId));
        qualified->genericMember = qualified->genericMember.empty() ? type->genericMember : qualified->genericMember + "::" + type->genericMember;
        return qualified;
    }
    auto result = Memory::makeShared<MType>(*type);
    result->elem = cloneType(type->elem, depth + 1);
    result->elem2 = cloneType(type->elem2, depth + 1);
    for (auto &parameter : result->paramTypes) parameter = cloneType(parameter, depth + 1);
    for (auto &argument : result->genericArguments) argument = cloneType(argument, depth + 1);
    return result;
}
}  // namespace chali_macro
