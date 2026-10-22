#include "template_binding.hpp"
#include "../chali_ast.hpp"
#include "../literals/string_escape.hpp"
#include <stdexcept>

namespace chali_macro
{
/**
 * @brief 保存节点池及卫生解析器的引用。
 * @param nodes 编译器节点池。
 * @param resolver 本次编译共用的卫生解析器。
 * @param types 可选的当前类型池。
 */
TemplateBinding::TemplateBinding(Memory::Deque<MNode> &nodes, HygieneResolver &resolver, Memory::Vector<std::shared_ptr<MType>> *types) : nodes_(nodes), resolver_(resolver), types_(types) {}

/**
 * @brief 克隆宏体并建立词法绑定，错误时回滚克隆节点。
 * @param definition 宏定义编号。
 * @param definitionScope 宏定义所在作用域。
 * @param result 接收独立模板及形参映射。
 * @param error 接收错误文本。
 * @param inputs 本次调用的形参到实参映射。
 * @param matcher 共享正则缓存。
 * @param bindBody 是否处理宏体。
 * @return 成功返回真。
 */
bool TemplateBinding::bind(int definition, ScopeId definitionScope, BoundTemplate &result, std::string &error,
                           const std::map<std::string, int> *inputs, RegexMatcher *matcher, bool bindBody)
{
    const auto start = nodes_.size();
    const auto typeStart = types_ ? types_->size() : 0;
    result = {};
    error.clear();
    parameters_.clear();
    inputs_ = inputs;
    result_ = &result;
    iterationInputs_ = inputs ? *inputs : std::map<std::string, int>{};
    if (inputs) inputs_ = &iterationInputs_;
    matcher_ = matcher;
    try
    {
        if (definition < 0 || static_cast<std::size_t>(definition) >= start || nodes_[definition].kind != MNode::MacroDefinition || nodes_[definition].kids.size() != 3)
            throw std::runtime_error("宏定义结构无效");
        const auto definitionNode = nodes_[definition];
        const auto scope = resolver_.createScope(definitionScope);
        parameterScope_ = scope;
        const int group = definitionNode.kids[0];
        if (group >= 0)
        {
            if (static_cast<std::size_t>(group) >= start || nodes_[group].kind != MNode::MacroParameters)
                throw std::runtime_error("宏参数组结构无效");
            Memory::Vector<int> groups = { group };
            if (!nodes_[group].kids.empty() && nodes_[nodes_[group].kids[0]].kind == MNode::MacroParameters) groups = nodes_[group].kids;
            for (const int parameterGroup : groups)
                for (const int parameter : nodes_[parameterGroup].kids)
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
        if (!bindBody)
        {
            result.root = definitionNode.kids[1];
            return true;
        }
        AstSubstitution substitution(nodes_, 1000000, types_);
        result.root = substitution.instantiate(definitionNode.kids[1], {}, definitionNode.syntaxContext, error);
        if (result.root < 0) throw std::runtime_error(error);
        visit(result.root, resolver_.createScope(scope), 0);
        return true;
    } catch (const std::exception &failure)
    {
        nodes_.resize(start);
        if (types_) types_->resize(typeStart);
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
    const auto parameter = resolver_.resolve(std::string_view(node.s.data(), node.s.size()), scope, node.syntaxContext);
    if (parameters_.count(parameter)) node.nameParameter = parameter;
    node.bindingIdentity = resolver_.declare(std::string_view(node.s.data(), node.s.size()), scope, node.syntaxContext);
    if (!node.bindingIdentity) throw std::runtime_error("宏体同一作用域重复声明：" + std::string(node.s.data(), node.s.size()));
}

/**
 * @brief 遍历模式中的绑定叶子，不将构造器或标量模式名称误当声明。
 * @param id 模式编号。
 * @param scope 分支或声明所在作用域。
 * @param depth 当前模式深度。
 */
void TemplateBinding::bindPattern(int id, ScopeId scope, std::size_t depth)
{
    if (id < 0) return;
    if (depth >= 256) throw std::runtime_error("宏模式绑定超过语法深度预算");
    auto &node = nodes_[id];
    if (node.kind == MNode::Var)
    {
        if (node.s != "_") declare(id, scope);
        return;
    }
    if (node.kind == MNode::List || node.kind == MNode::Tuple)
        for (const int child : node.kids) bindPattern(child, scope, depth + 1);
    else
        visit(id, scope, depth + 1);
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
    if (node.kind == MNode::MacroFor)
    {
        const auto source = std::string(node.s2.data(), node.s2.size());
        const auto variable = std::string(node.s.data(), node.s.size());
        if (!inputs_ || !inputs_->count(source) || nodes_[inputs_->at(source)].kind != MNode::MacroVariadic)
            throw std::runtime_error("宏循环必须遍历变长片段参数：" + source);
        const auto arguments = nodes_[inputs_->at(source)].kids;
        const int body = node.kids[0];
        MNode list;
        list.kind = MNode::List;
        const int items = static_cast<int>(nodes_.size());
        nodes_.push_back(std::move(list));
        const auto saved = iterationInputs_.find(variable);
        const int previous = saved == iterationInputs_.end() ? -1 : saved->second;
        for (const int argument : arguments)
        {
            const auto iterationScope = resolver_.createScope(scope);
            const auto identity = resolver_.declare(variable, iterationScope, node.syntaxContext);
            parameters_.insert(identity);
            result_->iterations.emplace(identity, argument);
            iterationInputs_[variable] = argument;
            AstSubstitution copy(nodes_, 1000000, types_);
            std::string error;
            const int expanded = copy.instantiate(body, {}, node.syntaxContext, error);
            if (expanded < 0) throw std::runtime_error(error);
            visit(expanded, iterationScope, depth + 1);
            nodes_[items].kids.push_back(expanded);
        }
        if (previous < 0)
            iterationInputs_.erase(variable);
        else
            iterationInputs_[variable] = previous;
        node.kind = MNode::MacroTemplate;
        node.kids = { items, -1 };
        node.s.clear();
        node.s2.clear();
        return;
    }
    if (node.kind == MNode::MacroMatch || node.kind == MNode::MacroRegex)
    {
        const auto name = std::string(node.s.data(), node.s.size());
        if (!inputs_ || !inputs_->count(name)) throw std::runtime_error("宏编译期分支引用了不存在的形参：" + name);
        const auto &argument = nodes_[inputs_->at(name)];
        int selected = -1;
        if (node.kind == MNode::MacroMatch)
        {
            for (const int branchId : nodes_[node.kids[0]].kids)
            {
                const auto &branch = nodes_[branchId];
                int category = branch.s == "expr" ? 0 : branch.s == "ident" ? 1 :
                                                    branch.s == "type"      ? 2 :
                                                    branch.s == "block"     ? 3 :
                                                    branch.s == "stmt"      ? 4 :
                                                                              -1;
                if (category < 0 || (argument.kind == MNode::MacroArgument && argument.kids.size() == 5 && argument.kids[category] >= 0))
                {
                    selected = branch.kids[0];
                    break;
                }
            }
        }
        else
        {
            if (!matcher_ || argument.kind != MNode::MacroArgument) throw std::runtime_error("宏正则匹配缺少原始实参或模式缓存");
            std::string pattern, error;
            if (!chali_literal::decodeString(std::string_view(node.s2.data(), node.s2.size()), pattern, error)) throw std::runtime_error(error);
            const auto result = matcher_->match(std::string_view(argument.s.data(), argument.s.size()), pattern, error);
            if (result == RegexResult::Invalid) throw std::runtime_error("宏正则模式非法：" + error);
            selected = node.kids[result == RegexResult::Matched ? 0 : 1];
        }
        if (selected >= 0)
            node = nodes_[selected];
        else
        {
            const int empty = static_cast<int>(nodes_.size());
            nodes_.push_back(MNode{});
            node.kind = MNode::MacroTemplate;
            node.kids = { empty, -1 };
            node.s.clear();
            node.s2.clear();
        }
    }
    if (types_ && node.type >= 0)
    {
        auto type = bindType(types_->at(node.type), scope, node.syntaxContext, 0);
        node.type = static_cast<int>(types_->size());
        types_->push_back(std::move(type));
    }
    if (node.kind == MNode::MacroArgument && node.kids.size() == 5 && node.kids[1] >= 0)
    {
        const auto &identifier = nodes_[node.kids[1]];
        const auto identity = resolver_.resolve(std::string_view(identifier.s.data(), identifier.s.size()), scope, node.syntaxContext);
        if (parameters_.count(identity)) node.bindingIdentity = identity;
    }
    if (node.kind == MNode::Var)
    {
        node.bindingIdentity = resolver_.resolve(std::string_view(node.s.data(), node.s.size()), scope, node.syntaxContext);
        if (parameters_.count(node.bindingIdentity)) node.kind = MNode::MacroHole;
        return;
    }
    if (node.kind == MNode::EnumCtor && types_)
    {
        const auto identity = resolver_.resolve(std::string_view(node.s.data(), node.s.size()), scope, node.syntaxContext);
        if (parameters_.count(identity))
        {
            auto type = Memory::makeShared<MType>();
            type->kind = MType::Struct;
            type->name = node.s;
            type->macroBinding = identity;
            node.type = static_cast<int>(types_->size());
            types_->push_back(std::move(type));
            node.kind = MNode::TypeQualifiedRef;
            node.s.clear();
        }
    }
    if (node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::EnumCtor || node.kind == MNode::TypeQualifiedRef)
    {
        const auto &name = node.kind == MNode::EnumCtor || node.kind == MNode::TypeQualifiedRef ? node.s2 : node.s;
        const auto identity = resolver_.resolve(std::string_view(name.data(), name.size()), scope, node.syntaxContext);
        if (parameters_.count(identity)) node.nameParameter = identity;
    }
    if (node.kind == MNode::Block)
    {
        visit(static_cast<int>(node.i), resolver_.createScope(scope), depth + 1);
        return;
    }
    if (node.kind == MNode::FuncDef || node.kind == MNode::MethodDef || node.kind == MNode::For || node.kind == MNode::GenericLambda)
        scope = resolver_.createScope(scope);
    if (node.kind == MNode::Destructure && !node.i)
    {
        visit(node.kids[1], scope, depth + 1);
        bindPattern(node.kids[0], scope, depth + 1);
        return;
    }
    if (node.kind == MNode::Case)
    {
        scope = resolver_.createScope(scope);
        if (node.i >= 0)
        {
            auto &pattern = nodes_[static_cast<int>(node.i)];
            if (pattern.kind == MNode::EnumCtor || pattern.kind == MNode::TypeQualifiedRef)
            {
                const auto payloads = pattern.kids;
                pattern.kids.clear();
                visit(static_cast<int>(node.i), scope, depth + 1);
                pattern.kids = payloads;
                for (const int payload : payloads) bindPattern(payload, scope, depth + 1);
            }
            else if (pattern.kind == MNode::Call && pattern.kids.size() == 2)
            {
                visit(pattern.kids[0], scope, depth + 1);
                bindPattern(pattern.kids[1], scope, depth + 1);
            }
            else
                visit(static_cast<int>(node.i), scope, depth + 1);
        }
        visit(node.guard, scope, depth + 1);
        for (const int child : node.kids) visit(child, scope, depth + 1);
        return;
    }
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
    if (node.kind == MNode::GenericLambda && node.type2 >= 0) visit(node.type2, scope, depth + 1);
    for (const int annotation : node.annotations) visit(annotation, scope, depth + 1);
    visit(node.guard, scope, depth + 1);
    const bool indexedChild = node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !declarationGroup && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__chali_bare_index");
    if (indexedChild) visit(static_cast<int>(node.i), scope, depth + 1);
    if ((node.kind == MNode::Declare && !declarationGroup) || node.kind == MNode::Params) declare(id, scope);
}

/**
 * @brief 标记类型语法中引用的宏形参，类型参数与表达式参数仍共用同一声明身份。
 * @param type 原始类型描述。
 * @param scope 当前词法作用域。
 * @param context 当前语法上下文。
 * @param depth 类型递归深度。
 * @return 独立的已绑定类型描述。
 */
std::shared_ptr<MType> TemplateBinding::bindType(const std::shared_ptr<MType> &type, ScopeId scope, ContextId context, std::size_t depth)
{
    if (!type) return nullptr;
    if (depth >= 256) throw std::runtime_error("宏类型绑定超过类型深度预算");
    auto result = Memory::makeShared<MType>(*type);
    if (type->kind == MType::Struct && type->genericBase.empty())
    {
        const auto identity = resolver_.resolve(std::string_view(type->name.data(), type->name.size()), scope, context);
        if (parameters_.count(identity))
            result->macroBinding = identity;
        else
        {
            const auto split = type->name.find("::");
            if (split != Memory::String::npos)
            {
                const auto parameter = resolver_.resolve(std::string_view(type->name.data(), split), scope, context);
                if (parameters_.count(parameter))
                {
                    result->macroBinding = parameter;
                    result->genericMember = type->name.substr(split + 2);
                }
            }
        }
    }
    result->elem = bindType(type->elem, scope, context, depth + 1);
    result->elem2 = bindType(type->elem2, scope, context, depth + 1);
    for (auto &parameter : result->paramTypes) parameter = bindType(parameter, scope, context, depth + 1);
    for (auto &argument : result->genericArguments) argument = bindType(argument, scope, context, depth + 1);
    return result;
}
}  // namespace chali_macro
