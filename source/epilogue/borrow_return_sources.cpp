/* 方法返回来源摘要：读取已解析方法体，不执行合法性判定。 */
/**
 * @brief 方法体中的符号来源集合；根为 self、形参或本次新分配。
 */
using MBorrowOrigins = Memory::Set<MBorrowPlace>;
/**
 * @brief 为引用返回方法提取字段、形参和新分配来源，分支保留来源并集。
 */
struct MBorrowReturnSummary
{
    const Memory::String owner;
    const MMethod &method;
    Memory::Map<Memory::String, MBorrowOrigins> locals;
    MBorrowOrigins returned;
    /**
     * @brief 构造符号形参来源。
     * @param type 所属类型。
     * @param declaration 方法声明。
     */
    MBorrowReturnSummary(Memory::String type, const MMethod &declaration) : owner(std::move(type)), method(declaration)
    {
        locals["self"] = { { "$self", {} } };
        for (std::size_t i = 0; i < method.params.size(); ++i)
            if (method.params[i].type->kind == MType::Ptr)
                locals[method.params[i].source] = { { "$arg:" + Memory::toString(i), {} } };
    }
    /**
     * @brief 从符号路径寻找字段类型。
     * @param path 符号路径。
     * @return 可确定时返回具体类型。
     */
    std::shared_ptr<MType> typeOf(const MBorrowPlace &path) const
    {
        std::shared_ptr<MType> type;
        if (path.root == "$self")
            type = NamedType(owner);
        else if (path.root.compare(0, 5, "$arg:") == 0)
        {
            const auto index = static_cast<std::size_t>(std::strtoull(path.root.c_str() + 5, nullptr, 10));
            if (index < method.params.size()) type = method.params[index].type;
        }
        for (const auto &part : path.path)
        {
            if (!type) break;
            if (part == "*")
            {
                if (type->kind == MType::Ptr) type = type->elem;
                continue;
            }
            if (type->kind == MType::Ptr) type = type->elem;
            if (part.size() > 1 && part[0] == '.' && type && type->kind == MType::Struct)
            {
                const auto field = FindField(type->name, part.substr(1));
                type = field ? field->type : nullptr;
            }
            else
                type = nullptr;
        }
        return type;
    }
    /**
     * @brief 提取表达式的符号来源。
     * @param id 表达式。
     * @return 可能的引用来源。
     */
    MBorrowOrigins expression(int id)
    {
        if (id < 0) return {};
        const auto node = N(id);
        if (node.kind == MNode::NewPtr) return { { "$fresh", {} } };
        if (node.kind == MNode::Var)
        {
            const auto local = locals.find(node.s);
            if (local != locals.end()) return local->second;
            const auto field = FindField(owner, node.s);
            if (field)
            {
                MBorrowPlace place{ "$self", { "." + node.s } };
                if (field->type->kind == MType::Ptr) place.path.push_back("*");
                return { place };
            }
            return {};
        }
        if ((node.kind == MNode::Un || node.kind == MNode::Cast) && !node.kids.empty()) return expression(node.kids[0]);
        if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index) && !node.kids.empty())
        {
            MBorrowOrigins result;
            for (auto place : expression(node.kids[0]))
            {
                place.path.push_back(node.kind == MNode::Index ? "[*]" : "." + node.s);
                const auto type = typeOf(place);
                if (type && type->kind == MType::Ptr) place.path.push_back("*");
                result.insert(std::move(place));
            }
            return result;
        }
        return {};
    }
    /**
     * @brief 合并路径环境。
     * @param into 合流环境。
     * @param from 分支环境。
     * @return 无。
     */
    static void merge(Memory::Map<Memory::String, MBorrowOrigins> &into, const Memory::Map<Memory::String, MBorrowOrigins> &from)
    {
        for (const auto &item : from) into[item.first].insert(item.second.begin(), item.second.end());
    }
    /**
     * @brief 遍历方法体，记录局部别名与返回来源。
     * @param id 语句。
     * @return 是否仍存在可继续执行的路径。
     */
    bool statement(int id)
    {
        if (id < 0) return true;
        const auto node = N(id);
        if (node.kind == MNode::Block || node.kind == MNode::List)
        {
            const auto original = locals;
            Memory::Set<Memory::String> declared;
            bool reachable = true;
            for (int child : node.kind == MNode::Block ? N(node.i).kids : node.kids)
            {
                const auto declaration = N(child);
                if (node.kind == MNode::Block && declaration.kind == MNode::Declare && declaration.s == "local")
                    for (int item : N(declaration.kids[0]).kids) declared.insert(N(item).s);
                if (!statement(child))
                {
                    reachable = false;
                    break;
                }
            }
            // 块内同名声明遮蔽外层绑定；离开块时只恢复声明，不撤销对外层引用的赋值。
            for (const auto &name : declared)
            {
                const auto previous = original.find(name);
                if (previous == original.end())
                    locals.erase(name);
                else
                    locals[name] = previous->second;
            }
            return reachable;
        }
        else if (node.kind == MNode::Declare && node.s == "local")
        {
            for (int child : N(node.kids[0]).kids) locals[N(child).s] = N(child).kids.empty() ? MBorrowOrigins{} : expression(N(child).kids[0]);
        }
        else if (node.kind == MNode::ExprStmt && !node.kids.empty())
            statement(node.kids[0]);
        else if (node.kind == MNode::Assign && N(node.kids[0]).kind == MNode::Var)
            locals[N(node.kids[0]).s] = expression(node.kids[1]);
        else if (node.kind == MNode::Return && !node.kids.empty())
        {
            const auto values = expression(node.kids[0]);
            returned.insert(values.begin(), values.end());
            return false;
        }
        else if (node.kind == MNode::If)
        {
            const auto original = locals;
            Memory::Map<Memory::String, MBorrowOrigins> joined;
            bool reachable = false, remainder = true;
            const auto branch = [&](int condition, int body) {
                if (!remainder) return;
                const auto test = N(condition);
                if (test.kind == MNode::Bool && !test.i) return;
                locals = original;
                if (statement(body))
                {
                    merge(joined, locals);
                    reachable = true;
                }
                if (test.kind == MNode::Bool && test.i) remainder = false;
            };
            branch(node.kids[0], node.kids[1]);
            for (int arm : N(node.kids[2]).kids) branch(N(arm).kids[0], N(arm).kids[1]);
            if (remainder)
            {
                locals = original;
                if (statement(node.kids[3]))
                {
                    merge(joined, locals);
                    reachable = true;
                }
            }
            locals = std::move(joined);
            return reachable;
        }
        else if (node.kind == MNode::Switch)
        {
            const auto original = locals;
            auto joined = locals;
            const auto subject = expression(node.kids[0]);
            for (int item : N(node.kids[1]).kids)
            {
                locals = original;
                const auto arm = N(item);
                if (arm.i >= 0)
                {
                    const auto pattern = N(arm.i);
                    const auto variant = FindEnumVariant(owner, pattern.s2);
                    if (variant)
                        for (std::size_t i = 0; i < pattern.kids.size() && i < variant->payloads.size(); ++i)
                        {
                            MBorrowOrigins values;
                            for (auto place : subject)
                            {
                                place.path.push_back("." + variant->payloads[i].storageName);
                                if (variant->payloads[i].type->kind == MType::Ptr) place.path.push_back("*");
                                values.insert(std::move(place));
                            }
                            locals[N(pattern.kids[i]).s] = std::move(values);
                        }
                }
                if (!arm.kids.empty()) statement(arm.kids[0]);
                merge(joined, locals);
            }
            locals = std::move(joined);
        }
        else
            for (int child : node.kids) statement(child);
        return true;
    }
};
