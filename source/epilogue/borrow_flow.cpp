/* 语义状态到独立借用控制流的适配层；只提取存储身份和控制流，不检查合法性。 */
using MBorrowPlace = magel_borrow_checker::Place;
using MBorrowFlowNode = magel_borrow_checker::FlowNode;
/**
 * @brief 单个函数的已绑定源码事实，避免离开作用域后重新查找同名变量。
 */
struct MBorrowFlowCapture
{
    Memory::Map<int, MBorrowPlace> variables;
    Memory::Map<int, Memory::Vector<MBorrowPlace>> lambdaMoves;
    Memory::Map<int, Memory::Vector<MBorrowPlace>> lambdaReads;
    Memory::Map<int, bool> references;
    Memory::Map<int, std::shared_ptr<MType>> types;
    Memory::Map<int, Memory::String> calls;
    Memory::Map<int, bool> managed;
    Memory::Map<int, MBorrowPlace> declarations;
    Memory::Map<int, bool> declarationReferences;
    Memory::Map<int, MBorrowPlace> redeclarations;
    Memory::Map<int, MBorrowPlace> foreachBindings;
    Memory::Set<int> valueLoops;  ///< 实际分配了结果槽的循环，独立语句也可能接收并丢弃结果。
};
Memory::Vector<magel_borrow_checker::FlowGraph> g_borrowFlows;
MBorrowFlowCapture *g_borrowFlowCapture = nullptr;
/**
 * @brief 保存表达式变量身份及已推导类型，保留字段引用和影子变量区别。
 * @param id 源码节点。
 * @param type 已推导类型。
 * @return 无。
 */
void CaptureBorrowExpression(int id, const std::shared_ptr<MType> &type)
{
    if (!g_borrowFlowCapture || id < 0) return;
    if (type)
    {
        g_borrowFlowCapture->types[id] = type;
        g_borrowFlowCapture->references[id] = type->kind == MType::Ptr || IsRuntimeArray(type);
        const auto value = type->kind == MType::Ptr ? type->elem : type;
        g_borrowFlowCapture->managed[id] = value && (value->kind == MType::Struct || IsOwnedKind(value));
    }
    std::function<void(int)> capture = [&](int current) {
        if (current < 0) return;
        const auto node = N(current);
        if (node.kind == MNode::Block || node.kind == MNode::FuncDef) return;
        if (node.kind == MNode::Var)
        {
            const auto variable = FindIn(node.module, node.s);
            if (variable)
            {
                g_borrowFlowCapture->variables[current] = { variable->emitted, {} };
                g_borrowFlowCapture->references[current] = variable->type->kind == MType::Ptr || IsRuntimeArray(variable->type);
                const auto value = variable->type->kind == MType::Ptr ? variable->type->elem : variable->type;
                g_borrowFlowCapture->managed[current] = value && (value->kind == MType::Struct || IsOwnedKind(value));
            }
            else if (!g_owner.empty() && (node.s == "self" || FieldType(g_owner, node.s)))
            {
                g_borrowFlowCapture->variables[current] = { "self:" + g_owner,
                                                            node.s == "self" ? Memory::Vector<Memory::String>{} : Memory::Vector<Memory::String>{ "." + node.s } };
            }
            return;
        }
        for (const int child : node.kids) capture(child);
    };
    capture(id);
}
/**
 * @brief 保存局部声明最终存储槽，引用槽与被引用对象独立建模。
 * @param id 声明节点。
 * @param variable 已完成初始化的变量。
 * @return 无。
 */
void CaptureBorrowDeclaration(int id, const MVar *variable)
{
    if (!g_borrowFlowCapture || !variable) return;
    g_borrowFlowCapture->declarations[id] = { variable->emitted, {} };
    g_borrowFlowCapture->declarationReferences[id] = variable->type->kind == MType::Ptr || IsRuntimeArray(variable->type);
}
#include "borrow_return_sources.cpp"

/**
 * @brief 从绑定事实构造显式后继图，分支和循环不依赖语义分析的访问顺序。
 */
struct MBorrowFlowBuilder
{
    const MBorrowFlowCapture &capture;
    magel_borrow_checker::FlowGraph graph;
    mutable Memory::Map<Memory::String, MBorrowOrigins> summaries;
    Memory::Vector<std::size_t> loopStops;   ///< 按词法嵌套登记循环出口，供层级退出选择。
    Memory::Vector<std::size_t> valueStops;  ///< 值表达式出口独立于普通循环出口，带值退出可穿透循环。
    /**
     * @brief 将方法体返回来源替换为调用点的具体接收者或实参。
     * @param id 调用节点。
     * @return 来源集合，新分配保留专用标记。
     */
    MBorrowOrigins methodOrigins(int id) const
    {
        const auto call = capture.calls.find(id);
        if (call == capture.calls.end()) return {};
        const auto dot = call->second.rfind('.');
        if (dot == Memory::String::npos) return {};
        const auto owner = call->second.substr(0, dot);
        const auto definition = g_structs.find(owner);
        if (definition == g_structs.end()) return {};
        auto summary = summaries.find(call->second);
        if (summary == summaries.end())
        {
            MBorrowOrigins origins;
            for (const auto &method : definition->second.methods)
                if (!method.superseded && method.name == call->second.substr(dot + 1) && method.bodyId >= 0)
                {
                    MBorrowReturnSummary extractor(owner, method);
                    extractor.statement(method.bodyId);
                    origins = std::move(extractor.returned);
                    break;
                }
            summary = summaries.emplace(call->second, std::move(origins)).first;
        }
        const auto node = N(id);
        const auto callee = N(node.kids[0]);
        if (callee.kind != MNode::Member && callee.kind != MNode::Arrow) return {};
        const auto arguments = node.kids.size() > 1 ? N(node.kids[1]).kids : Memory::Vector<int>{};
        MBorrowOrigins result;
        for (const auto &origin : summary->second)
        {
            if (origin.root == "$fresh")
            {
                result.insert(origin);
                continue;
            }
            MBorrowPlace concrete;
            if (origin.root == "$self")
                concrete = place(callee.kids[0]);
            else if (origin.root.compare(0, 5, "$arg:") == 0)
            {
                const auto index = static_cast<std::size_t>(std::strtoull(origin.root.c_str() + 5, nullptr, 10));
                if (index < arguments.size()) concrete = place(arguments[index]);
            }
            if (concrete.root.empty()) continue;
            concrete.path.insert(concrete.path.end(), origin.path.begin(), origin.path.end());
            result.insert(std::move(concrete));
        }
        return result;
    }
    /**
     * @brief 添加一项操作。
     * @param kind 操作。
     * @param target 目标。
     * @param source 来源。
     * @param next 后继。
     * @param id 源码。
     * @return 新节点。
     */
    std::size_t add(MBorrowFlowNode::Kind kind, MBorrowPlace target, MBorrowPlace source, std::size_t next, int id)
    {
        const auto index = graph.nodes.size();
        graph.nodes.push_back({ kind, std::move(target), std::move(source), {}, id >= 0 ? N(id).sourceLine : 0u, id >= 0 && !N(id).s.empty() ? N(id).s : Memory::String("表达式") });
        if (next != static_cast<std::size_t>(-1)) graph.nodes.back().next.push_back(next);
        return index;
    }
    /**
     * @brief 引用节点是否指向存储。
     * @param id 源码节点。
     * @return 引用时为 true。
     */
    bool reference(int id) const
    {
        const auto found = capture.references.find(id);
        return found != capture.references.end() && found->second;
    }
    /**
     * @brief 获取带解引用分段的对象路径。
     * @param id 源码节点。
     * @param slot 是否保留最终引用槽。
     * @return 存储路径。
     */
    MBorrowPlace place(int id, bool slot = false) const
    {
        if (id < 0) return {};
        const auto node = N(id);
        MBorrowPlace result;
        const auto variable = capture.variables.find(id);
        if (variable != capture.variables.end())
            result = variable->second;
        else if (node.kind == MNode::Call && reference(id) && !node.kids.empty())
        {
            const auto callee = N(node.kids[0]);
            if (!methodOrigins(id).empty()) result = { "$call:" + Memory::toString(id), {} };
            if (callee.kind != MNode::Member && callee.kind != MNode::Arrow && node.kids.size() > 1)
                for (const int argument : N(node.kids[1]).kids)
                    if (reference(argument) && !place(argument).root.empty())
                    {
                        result = { "$call:" + Memory::toString(id), {} };
                        break;
                    }
        }
        else if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index) && !node.kids.empty())
        {
            result = place(node.kids[0]);
            if (node.kind == MNode::Index)
            {
                const auto index = N(node.kids[1]);
                result.path.push_back(index.kind == MNode::Int ? "[" + Memory::toString(index.i) + "]" : Memory::String("[*]"));
            }
            else
                result.path.push_back("." + node.s);
        }
        else if ((node.kind == MNode::Un || node.kind == MNode::Cast) && !node.kids.empty())
            return place(node.kids[0], slot);
        if (!result.root.empty() && reference(id) && !slot) result.path.push_back("*");
        return result;
    }
    /**
     * @brief 只计算目标地址中的下标和接收者副作用。
     * @param id 目标。
     * @param next 后继。
     * @return 入口。
     */
    std::size_t address(int id, std::size_t next)
    {
        if (id < 0) return next;
        const auto node = N(id);
        if (node.kind == MNode::Index) return address(node.kids[0], expression(node.kids[1], next));
        if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Un || node.kind == MNode::Cast) && !node.kids.empty())
            return address(node.kids[0], next);
        if (node.kind == MNode::Call) return expression(id, next);
        return next;
    }
    /**
     * @brief 识别引用赋值，与通过引用写入普通值区分。
     * @param target 目标。
     * @param value 来源。
     * @return 重绑时为 true。
     */
    bool binding(int target, int value) const
    {
        if (!reference(target) || value < 0) return false;
        const auto node = N(value);
        return (node.kind == MNode::Un && node.s == "&") || node.kind == MNode::NewPtr;
    }
    /**
     * @brief 复制聚合中直接引用字段及枚举活动载荷的来源。
     * @param target 目标聚合路径。
     * @param value 来源节点。
     * @param next 后继。
     * @return 入口。
     */
    std::size_t aggregateBindings(const MBorrowPlace &target, int value, std::size_t next)
    {
        const auto found = capture.types.find(value);
        if (found == capture.types.end() || !found->second || found->second->kind != MType::Struct) return next;
        const auto type = found->second;
        const auto node = N(value);
        const auto layout = g_structs.find(type->name);
        const auto sourcePlace = place(value);
        if (layout != g_structs.end() && (node.kind == MNode::EnumCtor || sourcePlace.root.empty()))
            for (const auto &field : layout->second.fields)
                if (field.type->kind == MType::Ptr && (node.kind != MNode::EnumCtor || !FindEnumVariant(type->name, node.s2) || std::none_of(FindEnumVariant(type->name, node.s2)->payloads.begin(), FindEnumVariant(type->name, node.s2)->payloads.end(), [&](const MEnumPayload &payload) { return payload.storageName == field.name; })))
                {
                    auto slot = target;
                    slot.path.push_back("." + field.name);
                    next = add(MBorrowFlowNode::Bind, slot, {}, next, value);
                }
        if (node.kind == MNode::EnumCtor)
        {
            const auto variant = FindEnumVariant(type->name, node.s2);
            if (variant)
                for (std::size_t i = 0; i < variant->payloads.size() && i < node.kids.size(); ++i)
                {
                    if (variant->payloads[i].type->kind != MType::Ptr) continue;
                    auto slot = target;
                    slot.path.push_back("." + variant->payloads[i].storageName);
                    next = add(MBorrowFlowNode::Bind, slot, place(node.kids[i]), next, value);
                }
        }
        else
        {
            const auto definition = g_structs.find(type->name);
            const auto source = place(value);
            if (definition != g_structs.end() && !source.root.empty())
                for (const auto &field : definition->second.fields)
                {
                    if (field.type->kind != MType::Ptr) continue;
                    auto slot = target;
                    slot.path.push_back("." + field.name);
                    auto origin = source;
                    origin.path.push_back("." + field.name);
                    origin.path.push_back("*");
                    next = add(MBorrowFlowNode::Bind, slot, origin, next, value);
                }
        }
        return next;
    }
    /**
     * @brief 构造表达式顺序，短路分支保留条件边。
     * @param id 表达式。
     * @param next 后继。
     * @return 入口。
     */
    std::size_t expression(int id, std::size_t next)
    {
        if (id < 0) return next;
        const auto node = N(id);
        const auto reads = capture.lambdaReads.find(id);
        if (reads != capture.lambdaReads.end())
        {
            const auto moves = capture.lambdaMoves.find(id);
            if (moves != capture.lambdaMoves.end())
                for (auto it = moves->second.rbegin(); it != moves->second.rend(); ++it)
                    next = add(MBorrowFlowNode::Move, *it, {}, next, id);
            for (auto it = reads->second.rbegin(); it != reads->second.rend(); ++it)
                next = add(MBorrowFlowNode::Read, *it, {}, next, id);
            return next;
        }
        const auto moves = capture.lambdaMoves.find(id);
        if (moves != capture.lambdaMoves.end())
        {
            for (auto it = moves->second.rbegin(); it != moves->second.rend(); ++it)
                next = add(MBorrowFlowNode::Read, *it, {}, add(MBorrowFlowNode::Move, *it, {}, next, id), id);
            return next;
        }
        if (node.kind == MNode::Block) return statement(id, next, next, next);
        if (node.kind == MNode::Assign)
        {
            const int target = node.kids[0], value = node.kids[1];
            if (reference(target) && N(value).kind == MNode::NewPtr)
            {
                const auto slot = place(target, true);
                const auto allocation = add(MBorrowFlowNode::Allocate, slot,
                                            { "$allocation:" + Memory::toString(id), {} }, next, id);
                return address(target, expression(value, allocation));
            }
            auto entry = add(binding(target, value) ? MBorrowFlowNode::Bind : MBorrowFlowNode::Write,
                             place(target, binding(target, value)), place(value), next, id);
            if (!binding(target, value)) entry = aggregateBindings(place(target), value, entry);
            entry = address(target, entry);
            return binding(target, value) ? address(value, entry) : expression(value, entry);
        }
        if (node.kind == MNode::Un && node.s == "&") return address(node.kids[0], next);
        if (node.kind == MNode::Call && !node.kids.empty())
        {
            const auto callee = N(node.kids[0]);
            const auto arguments = node.kids.size() > 1 ? N(node.kids[1]).kids : Memory::Vector<int>{};
            if (reference(id))
            {
                const auto origins = methodOrigins(id);
                Memory::Vector<std::size_t> choices;
                for (const auto &origin : origins)
                    choices.push_back(add(origin.root == "$fresh" ? MBorrowFlowNode::Allocate : MBorrowFlowNode::Bind,
                                          place(id, true), origin.root == "$fresh" ? MBorrowPlace{ "$returnedAllocation:" + Memory::toString(id), {} } : origin, next, id));
                if (!choices.empty())
                {
                    const auto branch = add(MBorrowFlowNode::Nop, {}, {}, static_cast<std::size_t>(-1), id);
                    graph.nodes[branch].next = std::move(choices);
                    next = branch;
                }
            }
            // 普通函数的引用返回值保守关联到所有引用实参，不猜测不透明方法的内部字段。
            if (reference(id) && callee.kind != MNode::Member && callee.kind != MNode::Arrow)
            {
                Memory::Vector<std::size_t> choices;
                for (const int argument : arguments)
                    if (reference(argument) && !place(argument).root.empty())
                        choices.push_back(add(MBorrowFlowNode::Bind, place(id, true), place(argument), next, id));
                if (!choices.empty())
                {
                    const auto branch = add(MBorrowFlowNode::Nop, {}, {}, static_cast<std::size_t>(-1), id);
                    graph.nodes[branch].next = std::move(choices);
                    next = branch;
                }
            }
            if (StandardCallName(callee) == "move" && arguments.size() == 1u)
                next = add(capture.managed.count(arguments[0]) && capture.managed.at(arguments[0]) ? MBorrowFlowNode::Move : MBorrowFlowNode::MoveScalar, place(arguments[0]), {}, next, id);
            else if ((callee.kind == MNode::Member || callee.kind == MNode::Arrow) && callee.s == "drop" && !callee.kids.empty())
                next = add(MBorrowFlowNode::Drop, place(callee.kids[0]), {}, next, id);
            for (auto it = arguments.rbegin(); it != arguments.rend(); ++it) next = expression(*it, next);
            if ((callee.kind == MNode::Member || callee.kind == MNode::Arrow) && !callee.kids.empty()) next = callee.s == "drop" ? address(callee.kids[0], next) : expression(callee.kids[0], next);
            return next;
        }
        if (node.kind == MNode::Loop)
        {
            valueStops.push_back(next);
            const auto entry = node.s.empty() ? statement(id, next, next, next) : statement(node.kids[0], next, next, next);
            valueStops.pop_back();
            return entry;
        }
        if (node.kind == MNode::Bin && (node.s == "&&" || node.s == "||"))
        {
            const auto right = expression(node.kids[1], next);
            const auto branch = add(MBorrowFlowNode::Nop, {}, {}, next, id);
            graph.nodes[branch].next.push_back(right);
            const auto left = N(node.kids[0]);
            if (left.kind == MNode::Bool) graph.nodes[branch].next = { (left.i != 0) == (node.s == "&&") ? right : next };
            return expression(node.kids[0], branch);
        }
        const auto target = place(id);
        if (!target.root.empty() && (node.kind == MNode::Var || node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index || (node.kind == MNode::Un && node.s == "*")))
        {
            auto read = add(MBorrowFlowNode::Read, target, {}, next, id);
            if (reference(id))
                read = add(MBorrowFlowNode::Read, place(id, true), {}, read, id);
            return address(id, read);
        }
        for (auto it = node.kids.rbegin(); it != node.kids.rend(); ++it) next = expression(*it, next);
        return next;
    }
    /**
     * @brief 构造结构化语句控制流及循环回边。
     * @param id 语句。
     * @param next 后继。
     * @param stop break 目标。
     * @param repeat continue 目标。
     * @return 入口。
     */
    std::size_t statement(int id, std::size_t next, std::size_t stop, std::size_t repeat)
    {
        if (id < 0) return next;
        const auto node = N(id);
        if (node.kind == MNode::ExprStmt && !node.kids.empty() && N(node.kids[0]).kind == MNode::Loop && N(node.kids[0]).s == "block")
            return statement(N(node.kids[0]).kids[0], next, stop, repeat);
        if (node.kind == MNode::ExprStmt && !node.kids.empty() && N(node.kids[0]).kind == MNode::Loop && N(node.kids[0]).s.empty())
            return statement(node.kids[0], next, stop, repeat);
        if (node.kind == MNode::Destructure && !node.i)
        {
            std::function<void(int)> initialize = [&](int pattern) {
                const auto found = capture.declarations.find(pattern);
                if (found != capture.declarations.end())
                    next = add(MBorrowFlowNode::Initialize, found->second, {}, next, pattern);
                else
                    for (const int child : N(pattern).kids) initialize(child);
            };
            initialize(node.kids[0]);
            return expression(node.kids[1], next);
        }
        if (node.kind == MNode::Block || node.kind == MNode::List)
        {
            const auto children = node.kind == MNode::Block ? N(node.i).kids : node.kids;
            for (auto it = children.rbegin(); it != children.rend(); ++it) next = statement(*it, next, stop, repeat);
            return next;
        }
        if (node.kind == MNode::Declare && node.s == "local")
        {
            const auto declarations = N(node.kids[0]).kids;
            for (auto it = declarations.rbegin(); it != declarations.rend(); ++it)
            {
                const auto found = capture.declarations.find(*it);
                if (found == capture.declarations.end()) continue;
                const int value = N(*it).kids.empty() ? -1 : N(*it).kids[0];
                const auto ref = capture.declarationReferences.find(*it);
                if (ref != capture.declarationReferences.end() && ref->second && value >= 0)
                {
                    auto source = place(value);
                    const bool allocation = N(value).kind == MNode::NewPtr;
                    if (allocation) source = { found->second.root + "$heap", {} };
                    next = add(allocation ? MBorrowFlowNode::Allocate : MBorrowFlowNode::Bind, found->second, source, next, *it);
                    next = allocation ? expression(value, next) : address(value, next);
                }
                else
                {
                    const auto write = add(MBorrowFlowNode::Initialize, found->second, {}, next, *it);
                    next = expression(value, aggregateBindings(found->second, value, write));
                }
                // 每次执行声明均创建新的绑定，包括循环内重复到达的声明。
                const auto refBinding = capture.declarationReferences.find(*it);
                if (refBinding != capture.declarationReferences.end() && refBinding->second)
                    next = add(MBorrowFlowNode::Initialize, found->second, {}, next, *it);
                const auto previous = capture.redeclarations.find(*it);
                if (previous != capture.redeclarations.end())
                    next = add(MBorrowFlowNode::Redeclare, previous->second, {}, next, *it);
            }
            return next;
        }
        if (node.kind == MNode::If)
        {
            auto alternate = statement(node.kids[3], next, stop, repeat);
            const auto elifs = N(node.kids[2]).kids;
            for (auto it = elifs.rbegin(); it != elifs.rend(); ++it)
            {
                const auto branch = N(*it);
                const auto test = add(MBorrowFlowNode::Nop, {}, {}, alternate, *it);
                const auto armEntry = statement(branch.kids[1], next, stop, repeat);
                graph.nodes[test].next.push_back(armEntry);
                const auto condition = N(branch.kids[0]);
                if (condition.kind == MNode::Bool) graph.nodes[test].next = { condition.i ? armEntry : alternate };
                alternate = expression(branch.kids[0], test);
            }
            const auto test = add(MBorrowFlowNode::Nop, {}, {}, alternate, id);
            const auto thenEntry = statement(node.kids[1], next, stop, repeat);
            graph.nodes[test].next.push_back(thenEntry);
            const auto condition = N(node.kids[0]);
            if (condition.kind == MNode::Bool) graph.nodes[test].next = { graph.nodes[test].next[condition.i ? 1u : 0u] };
            return expression(node.kids[0], test);
        }
        if (node.kind == MNode::While || node.kind == MNode::For || node.kind == MNode::DoWhile || node.kind == MNode::Foreach || node.kind == MNode::Loop)
        {
            const auto test = add(MBorrowFlowNode::Nop, {}, {}, next, id);
            const int condition = node.kind == MNode::While ? node.kids[0] : node.kind == MNode::For ? node.kids[1] :
                                                                         node.kind == MNode::DoWhile ? node.kids[1] :
                                                                                                       -1;
            const auto testEntry = expression(condition, test);
            const auto update = node.kind == MNode::For ? expression(node.kids[2], testEntry) : testEntry;
            const int body = node.kind == MNode::For ? node.kids[3] : node.kind == MNode::Loop ? node.kids[0] :
                                                                  node.kind == MNode::DoWhile  ? node.kids[0] :
                                                                                                 node.kids[1];
            const bool ownValue = capture.valueLoops.count(id) != 0u;
            if (ownValue) valueStops.push_back(next);
            loopStops.push_back(next);
            auto bodyEntry = statement(body, update, next, update);
            loopStops.pop_back();
            if (ownValue) valueStops.pop_back();
            const auto iteration = capture.foreachBindings.find(id);
            if (iteration != capture.foreachBindings.end())
                bodyEntry = add(MBorrowFlowNode::Initialize, iteration->second, {}, bodyEntry, id);
            graph.nodes[test].next.push_back(bodyEntry);
            if (node.kind == MNode::Loop || (node.kind == MNode::For && condition < 0))
                graph.nodes[test].next = { bodyEntry };
            else if (condition >= 0 && N(condition).kind == MNode::Bool)
                graph.nodes[test].next = { N(condition).i ? bodyEntry : next };
            if (node.kind == MNode::For) return statement(node.kids[0], testEntry, stop, repeat);
            if (node.kind == MNode::DoWhile) return bodyEntry;
            if (node.kind == MNode::Foreach) return expression(node.kids[0], testEntry);
            return testEntry;
        }
        if (node.kind == MNode::Switch)
        {
            const auto branch = add(MBorrowFlowNode::Nop, {}, {}, next, id);
            for (const int item : N(node.kids[1]).kids)
            {
                const auto arm = N(item);
                if (!arm.kids.empty())
                {
                    const auto armEntry = statement(arm.kids[0], next, stop, repeat);
                    const auto guardEntry = expression(arm.guard, armEntry);
                    graph.nodes[branch].next.push_back(guardEntry);
                }
            }
            return expression(node.kids[0], branch);
        }
        if (node.kind == MNode::Break)
        {
            if (node.i < 0 && !loopStops.empty()) return loopStops.front();
            if (node.i > 0 && static_cast<unsigned long long>(node.i) <= loopStops.size()) return loopStops[loopStops.size() - node.i];
            return node.kids.empty() ? stop : expression(node.kids[0], valueStops.empty() ? stop : valueStops.back());
        }
        if (node.kind == MNode::Continue) return repeat;
        if (node.kind == MNode::Return) return node.kids.empty() ? static_cast<std::size_t>(-1) : expression(node.kids[0], static_cast<std::size_t>(-1));
        if (node.kind == MNode::Delete && !node.kids.empty()) return address(node.kids[0], add(reference(node.kids[0]) ? MBorrowFlowNode::Destroy : MBorrowFlowNode::Drop, place(node.kids[0]), {}, next, id));
        return expression(id, next);
    }
};
/**
 * @brief 完成函数的借用控制流事实，不在语义分析中执行检查。
 * @param body 函数体。
 * @param capture 已绑定事实。
 * @return 无。
 */
void FinishBorrowFlow(int body, const MBorrowFlowCapture &capture)
{
    MBorrowFlowBuilder builder{ capture, {} };
    builder.graph.nodes.push_back({});
    const auto end = builder.add(MBorrowFlowNode::Nop, {}, {}, static_cast<std::size_t>(-1), body);
    const auto entry = builder.statement(body, end, end, end);
    builder.graph.nodes[0].next.push_back(entry);
    g_borrowFlows.push_back(std::move(builder.graph));
}
