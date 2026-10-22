#include "registry.hpp"
#include "../chali_ast.hpp"

namespace chali_macro
{
/**
 * @brief 保存当前编译的节点池和卫生解析器。
 * @param nodes AST 节点池。
 * @param resolver 绑定和展开上下文解析器。
 * @param types 当前类型池。
 */
Registry::Registry(Memory::Deque<MNode> &nodes, HygieneResolver &resolver, Memory::Vector<std::shared_ptr<MType>> *types) : nodes_(nodes), resolver_(resolver), types_(types) {}

/**
 * @brief 保存直接导入关系，供宏的定义环境查找使用。
 * @param from 声明所在模块。
 * @param target 被导入模块。
 */
void Registry::addImport(const std::string &from, const std::string &target)
{
    imports_[from].insert(target);
}

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
    if (key.second == "line" || key.second == "file" || key.second == "module" || key.second == "concat" || key.second == "stringify")
    {
        error = "宏名称占用了编译期内建名称：" + key.second;
        return false;
    }
    Definition entry;
    entry.declaration = definition;
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
    const auto start = nodes_.size();
    TemplateBinding binder(nodes_, resolver_, types_);
    if (!binder.bind(definition, scope, entry.bound, error, nullptr, nullptr, false)) return false;
    if (entry.grouped)
    {
        Memory::Vector<int> groups = { node.kids[0] };
        if (!nodes_[groups[0]].kids.empty() && nodes_[nodes_[groups[0]].kids[0]].kind == MNode::MacroParameters) groups = nodes_[groups[0]].kids;
        for (const int group : groups)
        {
            Definition::Group signature;
            signature.opening = nodes_[group].i ? static_cast<int>(nodes_[group].i) : '(';
            signature.begin = entry.parameters.size();
            signature.count = nodes_[group].kids.size();
            for (const int parameter : nodes_[group].kids)
            {
                if (signature.variadic)
                {
                    error = "变长宏形参必须位于参数组末尾";
                    return false;
                }
                signature.variadic = nodes_[parameter].i != 0;
                const auto &name = nodes_[parameter].s;
                entry.parameters.push_back(entry.bound.parameters.at(std::string(name.data(), name.size())));
                entry.names.push_back(std::string(name.data(), name.size()));
            }
            entry.groups.push_back(signature);
        }
    }
    auto &overloads = definitions_[key];
    for (const auto &existing : overloads)
        if (existing.grouped == entry.grouped && existing.groups.size() == entry.groups.size() && std::equal(existing.groups.begin(), existing.groups.end(), entry.groups.begin(), [](const Definition::Group &a, const Definition::Group &b) {
                return a.opening == b.opening && a.count == b.count && a.variadic == b.variadic;
            }))
        {
            nodes_.resize(start);
            error = "宏重复定义：" + key.second;
            return false;
        }
    overloads.push_back(std::move(entry));
    return true;
}

/**
 * @brief 校验宏调用并克隆替换模板，实参保持原始卫生上下文。
 * @param call 调用节点编号。
 * @param error 接收错误，成功时清空。
 * @return 实例化模板编号，失败为负一。
 */
int Registry::instantiate(int call, std::string &error,
                          const std::function<int(int, int, int)> &resolveType,
                          const std::function<bool(int, const std::map<std::string, int> &, std::string &)> &applyTypes)
{
    error.clear();
    if (call < 0 || static_cast<std::size_t>(call) >= nodes_.size() || nodes_[call].kind != MNode::MacroCall || nodes_[call].kids.empty())
    {
        error = "宏展开需要有效的调用节点";
        return -1;
    }
    const auto node = nodes_[call];
    const auto name = std::string(node.s.data(), node.s.size());
    const auto callModule = std::string(node.module.data(), node.module.size());
    const auto key = std::make_pair(callModule, name);
    auto found = definitions_.end();
    /* 限定名调用（`$std::lang::assert`）：按「模块 + 宏名」直接定位。模块路径从最长的
     * 前缀往下试（`std::lang::assert` 先试 `std::lang` + `assert`），第一个命中的算数。 */
    for (std::size_t at = name.rfind("::"); at != std::string::npos && at != 0; at = name.rfind("::", at - 1))
    {
        const auto qualifier = name.substr(0, at);
        /* 限定调用也走可见性：`std::lang` 始终可见，其余必须先 import。 */
        if (qualifier != callModule && qualifier != "std::lang" && imports_[callModule].count(qualifier) == 0) continue;
        const auto candidate = definitions_.find({ qualifier, name.substr(at + 2) });
        if (candidate == definitions_.end()) continue;
        found = candidate;
        break;
    }
    if (found == definitions_.end()) found = definitions_.find(key);
    if (found == definitions_.end())
    {
        auto visible = imports_[key.first];
        visible.insert("std::lang");
        for (const auto &imported : visible)
        {
            const auto candidate = definitions_.find({ imported, key.second });
            if (candidate == definitions_.end()) continue;
            if (found != definitions_.end())
            {
                error = "宏名称存在多个可见定义：" + key.second;
                return -1;
            }
            found = candidate;
        }
    }
    if (found == definitions_.end())
    {
        error = "未定义宏：" + key.second;
        return -1;
    }
    const int group = node.kids[0];
    Memory::Vector<int> groups;
    if (group >= 0)
        groups = nodes_[group].kind == MNode::MacroArguments ? nodes_[group].kids : Memory::Vector<int>{ group };
    const Definition *selected = nullptr;
    bool sameGroup = false;
    for (const auto &candidate : found->second)
        if (candidate.groups.size() == groups.size())
        {
            bool shape = true, arity = true, variadic = false;
            for (std::size_t index = 0; index < groups.size(); ++index)
            {
                const auto &signature = candidate.groups[index];
                const auto &actual = nodes_[groups[index]];
                const int opening = actual.kind == MNode::List ? '(' : static_cast<int>(actual.i);
                shape = shape && signature.opening == opening;
                arity = arity && (signature.variadic ? actual.kids.size() + 1 >= signature.count : actual.kids.size() == signature.count);
                variadic = variadic || signature.variadic;
            }
            sameGroup = sameGroup || shape;
            if (shape && arity)
            {
                if (!selected || !variadic) selected = &candidate;
            }
        }
    if (!selected)
    {
        error = (sameGroup ? "宏实参数量与形参不一致：" : "宏调用的参数组与定义不一致：") + key.second;
        return -1;
    }
    const auto &entry = *selected;
    if (entry.generic && !resolveType) return -2;
    if (!entry.generic && node.kids.size() > 1)
    {
        error = "非泛型宏不能指定类型实参";
        return -1;
    }
    std::map<BindingId, int> arguments;
    std::map<std::string, int> inputs;
    if (remainingNodes_ == 0)
    {
        error = "宏展开生成的语法节点超过编译预算";
        return -1;
    }
    const auto start = nodes_.size();
    struct Rollback
    {
        Memory::Deque<MNode> &nodes;
        Memory::Vector<std::shared_ptr<MType>> *types;
        std::size_t nodeCount, typeCount;
        bool committed = false;
        /**
         * @brief 实例化失败或被类型请求中断时同步回滚语法池和类型池。
         */
        ~Rollback()
        {
            if (committed) return;
            nodes.resize(nodeCount);
            if (types) types->resize(typeCount);
        }
    } rollback{ nodes_, types_, start, types_ ? types_->size() : 0 };
    for (std::size_t index = 0; index < groups.size(); ++index)
    {
        const auto &signature = entry.groups[index];
        const auto &actuals = nodes_[groups[index]].kids;
        const auto fixed = signature.count - (signature.variadic ? 1 : 0);
        for (std::size_t item = 0; item < fixed; ++item)
            inputs.emplace(entry.names[signature.begin + item], actuals[item]);
        if (signature.variadic)
        {
            MNode rest;
            rest.kind = MNode::MacroVariadic;
            for (std::size_t item = fixed; item < actuals.size(); ++item)
            {
                if (item != fixed) rest.s += ",";
                rest.s += nodes_[actuals[item]].s;
                rest.kids.push_back(actuals[item]);
            }
            inputs.emplace(entry.names[signature.begin + fixed], static_cast<int>(nodes_.size()));
            nodes_.push_back(std::move(rest));
        }
    }
    ExpansionSite site;
    site.line = node.expansionLine ? node.expansionLine : node.sourceLine;
    site.column = node.expansionLine ? node.expansionColumn : node.sourceColumn;
    const auto &module = node.expansionLine ? node.expansionModule : node.module;
    site.module.assign(module.data(), module.size());
    MNode frame;
    frame.kind = MNode::MacroExpansionFrame;
    frame.s = node.s;
    frame.module = module;
    frame.sourceLine = site.line;
    frame.sourceColumn = site.column;
    frame.i = entry.declaration;
    frame.kids = { node.macroExpansion };
    site.frame = static_cast<int>(nodes_.size());
    nodes_.push_back(std::move(frame));
    TemplateBinding binder(nodes_, resolver_, types_);
    BoundTemplate bound;
    if (!binder.bind(entry.declaration, entry.scope, bound, error, &inputs, &matcher_))
    {
        nodes_.resize(start);
        return -1;
    }
    if (!normalizeTemplate(bound.root, 0, error))
    {
        nodes_.resize(start);
        return -1;
    }
    const auto bindingCost = nodes_.size() - start;
    if (bindingCost >= remainingNodes_)
    {
        nodes_.resize(start);
        error = "宏展开生成的语法节点超过编译预算";
        return -1;
    }
    for (const auto &input : inputs) arguments.emplace(bound.parameters.at(input.first), input.second);
    arguments.insert(bound.iterations.begin(), bound.iterations.end());
    std::map<std::string, int> types;
    if (entry.generic)
    {
        const auto &generics = nodes_[nodes_[entry.declaration].kids[2]].kids;
        const auto explicitCount = node.kids.size() > 1 ? nodes_[node.kids[1]].kids.size() : 0;
        if (explicitCount && explicitCount != generics.size())
        {
            nodes_.resize(start);
            error = "宏显式类型实参数量与泛型形参不一致";
            return -1;
        }
        for (std::size_t index = 0; index < generics.size(); ++index)
        {
            const auto &generic = nodes_[generics[index]];
            const auto source = std::string(generic.s2.data(), generic.s2.size());
            if (!source.empty() && !inputs.count(source))
            {
                nodes_.resize(start);
                error = "宏泛型推导来源不是形参：" + source;
                return -1;
            }
            const int resolved = resolveType(generics[index], source.empty() ? -1 : inputs.at(source),
                                             explicitCount ? nodes_[node.kids[1]].kids[index] : -1);
            if (resolved < 0)
            {
                nodes_.resize(start);
                error = "宏泛型类型无法推导：" + std::string(generic.s.data(), generic.s.size());
                return -1;
            }
            types.emplace(std::string(generic.s.data(), generic.s.size()), resolved);
        }
    }
    AstSubstitution substitution(nodes_, remainingNodes_ - bindingCost, types_);
    const int expanded = substitution.instantiate(bound.root, arguments, resolver_.createExpansion(entry.scope, entry.context), error, site);
    if (expanded >= 0 && entry.generic && (!applyTypes || !applyTypes(expanded, types, error)))
    {
        nodes_.resize(start);
        if (error.empty()) error = "宏类型参数未能替换到模板中";
        return -1;
    }
    if (expanded >= 0)
    {
        remainingNodes_ -= nodes_.size() - start;
        rollback.committed = true;
    }
    else
        nodes_.resize(start);
    return expanded;
}

/**
 * @brief 直接合并选中分支的语句与末尾表达式，不增加运行时块。
 * @param root 模板节点编号。
 * @param depth 当前递归深度。
 * @param error 接收诊断文本。
 * @return 检查成功返回真。
 */
bool Registry::normalizeTemplate(int root, std::size_t depth, std::string &error)
{
    if (root < 0 || depth >= 256 || static_cast<std::size_t>(root) >= nodes_.size() || nodes_[root].kind != MNode::MacroTemplate || nodes_[root].kids.size() != 2)
    {
        error = "宏模板结构无效或过深";
        return false;
    }
    auto &node = nodes_[root];
    const auto parts = nodes_[node.kids[0]].kids;
    Memory::Vector<int> statements;
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        const int part = parts[index];
        if (nodes_[part].kind != MNode::MacroTemplate)
        {
            statements.push_back(part);
            continue;
        }
        if (!normalizeTemplate(part, depth + 1, error)) return false;
        const auto &selected = nodes_[part];
        const auto &items = nodes_[selected.kids[0]].kids;
        statements.insert(statements.end(), items.begin(), items.end());
        if (selected.kids[1] >= 0)
        {
            if (index + 1 == parts.size() && node.kids[1] < 0)
                node.kids[1] = selected.kids[1];
            else
            {
                const int expression = selected.kids[1];
                nodes_[expression].checkedExpression = true;
                MNode statement;
                statement.kind = MNode::ExprStmt;
                statement.kids = { expression };
                statement.sourceLine = nodes_[expression].sourceLine;
                statement.sourceColumn = nodes_[expression].sourceColumn;
                statement.module = nodes_[expression].module;
                statements.push_back(static_cast<int>(nodes_.size()));
                nodes_.push_back(std::move(statement));
            }
        }
    }
    nodes_[node.kids[0]].kids = std::move(statements);
    return true;
}
}  // namespace chali_macro
