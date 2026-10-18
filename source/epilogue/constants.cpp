/**
 * @file constants.cpp
 * @brief 有界、无副作用的编译期标量常量求值。
 */
/**
 * @brief 常量值；整数运算沿用 64 位槽的模二进制语义。
 */
struct MConstant
{
    enum Kind
    {
        Integer,
        Floating,
        Boolean
    } kind = Integer;
    long long integer = 0;
    double floating = 0;
    bool isUnsigned = false;
    unsigned bits = 64;
};
/**
 * @brief 按二进制位转换整数，避免有符号溢出的宿主未定义行为。
 */
long long ConstantBits(std::uint64_t bits)
{
    long long value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
/**
 * @brief 求值上下文，检测循环引用并限制递归与工作量。
 */
Memory::Map<int, MVar> g_constantBindings;
/**
 * @brief 固定局部常量初始化式中的名称绑定，避免后续同名遮蔽改变值。
 */
void BindConstantReferences(int id)
{
    if (id < 0) return;
    const auto &n = N(id);
    if (n.kind == MNode::Var)
    {
        if (auto *v = FindIn(n.module, n.s)) g_constantBindings[id] = *v;
    }
    for (int child : n.kids) BindConstantReferences(child);
}
/**
 * @brief 常量定义中的全局名称查找，不受使用处局部遮蔽影响。
 */
MVar *FindConstantGlobal(const Memory::String &module, const Memory::String &name)
{
    if (g_scope.empty()) return nullptr;
    for (const auto &key : { ModuleSymbol(module, name), name })
    {
        auto it = g_scope.front().find(key);
        if (it != g_scope.front().end()) return &it->second;
    }
    Memory::Set<Memory::String> owners{ "std::lang" };
    auto imports = g_moduleImports.find(module);
    if (imports != g_moduleImports.end()) owners.insert(imports->second.begin(), imports->second.end());
    MVar *result = nullptr;
    for (const auto &owner : owners)
    {
        auto it = g_scope.front().find(ModuleSymbol(owner, name));
        if (it != g_scope.front().end())
        {
            if (result) return nullptr;
            result = &it->second;
        }
    }
    return result;
}
/**
 * @brief 按声明位宽归一整数常量，显式保留符号。
 */
void NormalizeConstant(MConstant *v)
{
    if (v->kind != MConstant::Integer || v->bits >= 64) return;
    auto mask = (std::uint64_t(1) << v->bits) - 1;
    auto bits = std::uint64_t(v->integer) & mask;
    if (!v->isUnsigned && (bits & (std::uint64_t(1) << (v->bits - 1)))) bits |= ~mask;
    v->integer = ConstantBits(bits);
}
struct MConstantContext
{
    bool globalOnly = false;
    Memory::Set<int> active;
    std::size_t budget = 10000;
    unsigned depth = 0;
};
/**
 * @brief 校验所有分支的纯度；短路只省略计算，不放过调用或运行时变量。
 */
bool ValidateConstantTree(int id, MConstantContext &context, MType::Kind *result = nullptr)
{
    if (id < 0) return Fail("[MGL1300] 常量表达式缺少初值");
    if (!context.budget-- || context.depth >= 128) return Fail("[MGL1300] 常量表达式超过计算限制");
    if (!context.active.insert(id).second) return Fail("[MGL1300] 常量定义存在循环引用");
    struct Scope
    {
        MConstantContext &c;
        int id;
        bool global;
        /**
         * @brief 退出常量求值作用域并恢复递归深度与求值状态。
         */
        ~Scope()
        {
            c.active.erase(id);
            --c.depth;
            c.globalOnly = global;
        }
    } scope{ context, id, context.globalOnly };
    ++context.depth;
    const auto &node = N(id);
    g_reductionLine = node.sourceLine;
    if (node.kind == MNode::Int || node.kind == MNode::Float || node.kind == MNode::Bool)
    {
        if (result) *result = node.kind == MNode::Int ? MType::Int : node.kind == MNode::Float ? MType::Double :
                                                                                                 MType::Bool;
        return true;
    }
    if (node.kind == MNode::Var || node.kind == MNode::EnumCtor)
    {
        auto bound = g_constantBindings.find(id);
        auto *variable = bound != g_constantBindings.end() ? &bound->second : node.kind == MNode::Var ? (context.globalOnly ? FindConstantGlobal(node.module, node.s) : FindIn(node.module, node.s)) :
                                                                                                        FindQualifiedGlobal(node.s, node.s2);
        if (!variable || !variable->isConst || variable->constantExpression < 0) return Fail("[MGL1300] 常量表达式不能引用运行时变量：" + node.s);
        if (!variable->type || (variable->type->kind != MType::Int && variable->type->kind != MType::Double && variable->type->kind != MType::Bool)) return Fail("[MGL1300] 常量表达式仅支持整数、浮点和 bool");
        if (result) *result = variable->type->kind;
        context.globalOnly = true;
        return ValidateConstantTree(variable->constantExpression, context);
    }
    if (node.kind != MNode::Un && node.kind != MNode::Bin && node.kind != MNode::Cast) return Fail("[MGL1300] 常量表达式禁止调用、赋值、自增减或运行时操作");
    Memory::Vector<MType::Kind> kinds;
    for (int child : node.kids)
    {
        MType::Kind type = MType::Void;
        if (!ValidateConstantTree(child, context, &type)) return false;
        kinds.push_back(type);
    }
    auto numeric = [](MType::Kind type) { return type == MType::Int || type == MType::Double; };
    MType::Kind type = kinds.front();
    if (node.kind == MNode::Cast)
    {
        type = T(node.type)->kind;
        if (type != MType::Int && type != MType::Double && type != MType::Bool) return Fail("[MGL1300] 常量转换仅支持标量数值类型");
    }
    else if (node.kind == MNode::Un)
    {
        if (node.s != "!" && node.s != "~" && node.s != "-") return Fail("[MGL1300] 常量表达式禁止引用或地址操作");
        if (node.s == "!" && type != MType::Bool) return Fail("[MGL1300] 常量逻辑运算需要 bool");
        if (node.s == "~" && type != MType::Int) return Fail("[MGL1300] 常量位运算需要整数");
        if (node.s == "-" && !numeric(type)) return Fail("[MGL1300] 常量一元运算与类型不兼容");
    }
    else
    {
        const auto right = kinds[1];
        const auto &op = node.s;
        if (type == MType::Bool && right == MType::Bool && op != "&&" && op != "||" && op != "==" && op != "!=") return Fail("[MGL1300] bool 只支持逻辑运算和相等比较");
        if (op == "&&" || op == "||")
        {
            if (type != MType::Bool || right != MType::Bool) return Fail("[MGL1300] 常量逻辑运算需要 bool");
            type = MType::Bool;
        }
        else if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=")
        {
            if (!((numeric(type) && numeric(right)) || (type == MType::Bool && right == MType::Bool))) return Fail("[MGL1300] 常量比较类型不兼容");
            type = MType::Bool;
        }
        else if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>" || op == "%")
        {
            if (type != MType::Int || right != MType::Int) return Fail("[MGL1300] 常量位运算或余数运算需要整数");
            type = MType::Int;
        }
        else
        {
            if (!numeric(type) || !numeric(right)) return Fail("[MGL1300] bool 常量不能用于算术");
            type = type == MType::Double || right == MType::Double ? MType::Double : MType::Int;
        }
    }
    if (result) *result = type;
    return true;
}
/**
 * @brief 递归求值常量；不调用 MAGEL 或宿主函数。
 */
bool EvaluateConstantImpl(int id, MConstant *out, MConstantContext &context)
{
    if (id < 0) return Fail("[MGL1300] 常量表达式缺少初值");
    if (!context.budget-- || context.depth >= 128) return Fail("[MGL1300] 常量表达式超过计算限制");
    if (!context.active.insert(id).second) return Fail("[MGL1300] 常量定义存在循环引用");
    struct Scope
    {
        MConstantContext &c;
        int id;
        /**
         * @brief 退出常量求值作用域并恢复递归深度与求值状态。
         */
        ~Scope()
        {
            c.active.erase(id);
            --c.depth;
        }
    } scope{ context, id };
    ++context.depth;
    const auto &node = N(id);
    g_reductionLine = node.sourceLine;
    if (node.kind == MNode::Int || node.kind == MNode::Bool || node.kind == MNode::Float)
    {
        out->kind = node.kind == MNode::Float ? MConstant::Floating : node.kind == MNode::Bool ? MConstant::Boolean :
                                                                                                 MConstant::Integer;
        out->integer = node.i;
        out->floating = node.d;
        out->isUnsigned = node.unsignedLiteral;
        if (node.type >= 0)
        {
            const auto type = T(node.type);
            out->bits = type->bits;
            out->isUnsigned = type->isUnsigned;
            NormalizeConstant(out);
        }
        return out->kind != MConstant::Floating || std::isfinite(out->floating) || Fail("[MGL1300] 常量浮点值必须有限");
    }
    if (node.kind == MNode::Var || node.kind == MNode::EnumCtor)
    {
        auto bound = g_constantBindings.find(id);
        auto *variable = bound != g_constantBindings.end() ? &bound->second : node.kind == MNode::Var ? (context.globalOnly ? FindConstantGlobal(node.module, node.s) : FindIn(node.module, node.s)) :
                                                                                                        FindQualifiedGlobal(node.s, node.s2);
        if (!variable || !variable->isConst || variable->constantExpression < 0)
            return Fail("[MGL1300] 常量表达式不能引用运行时变量：" + node.s);
        const int initializer = variable->constantExpression;
        const auto type = variable->type;
        const bool saved = context.globalOnly;
        context.globalOnly = true;
        const bool evaluated = EvaluateConstantImpl(initializer, out, context);
        context.globalOnly = saved;
        if (!evaluated) return false;
        if (!type) return false;
        if (type->kind == MType::Int && out->kind == MConstant::Integer)
        {
            out->bits = type->bits;
            out->isUnsigned = type->isUnsigned;
            NormalizeConstant(out);
        }
        else if (type->kind == MType::Double)
        {
            if (out->kind == MConstant::Integer) out->floating = out->isUnsigned ? double(std::uint64_t(out->integer)) : double(out->integer);
            out->kind = MConstant::Floating;
            out->bits = type->bits;
            if (type->bits == 32) out->floating = static_cast<float>(out->floating);
        }
        else if (type->kind != MType::Bool)
            return Fail("[MGL1300] 常量表达式仅支持整数、浮点和 bool");
        return true;
    }
    if (node.kind == MNode::Cast)
    {
        if (!EvaluateConstantImpl(node.kids[0], out, context)) return false;
        auto type = T(node.type);
        if (!type) return false;
        if (type->kind == MType::Int)
        {
            if (out->kind == MConstant::Floating)
            {
                double value = std::trunc(out->floating);
                if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0) return Fail("[MGL1300] 浮点常量转换越界");
                out->integer = static_cast<long long>(value);
            }
            out->kind = MConstant::Integer;
            out->bits = type->bits;
            out->isUnsigned = type->isUnsigned;
            NormalizeConstant(out);
            return true;
        }
        if (type->kind == MType::Bool)
        {
            out->integer = out->kind == MConstant::Floating ? out->floating != 0 : out->integer != 0;
            out->kind = MConstant::Boolean;
            return true;
        }
        if (type->kind == MType::Double)
        {
            if (out->kind != MConstant::Floating) out->floating = out->isUnsigned ? double(std::uint64_t(out->integer)) : double(out->integer);
            out->kind = MConstant::Floating;
            out->bits = type->bits;
            if (type->bits == 32) out->floating = static_cast<float>(out->floating);
            return std::isfinite(out->floating) || Fail("[MGL1300] 常量浮点转换溢出");
        }
        return Fail("[MGL1300] 常量转换仅支持标量数值类型");
    }
    if (node.kind != MNode::Un && node.kind != MNode::Bin)
        return Fail("[MGL1300] 常量表达式禁止调用、赋值、自增减或运行时操作");
    MConstant left, right;
    if (!EvaluateConstantImpl(node.kids[0], &left, context)) return false;
    *out = left;
    if (node.kind == MNode::Un)
    {
        if (node.s == "!" && left.kind == MConstant::Boolean)
        {
            out->integer = !left.integer;
            return true;
        }
        if (node.s == "~" && left.kind == MConstant::Integer)
        {
            out->integer = ~left.integer;
            NormalizeConstant(out);
            return true;
        }
        if (node.s == "-" && left.kind != MConstant::Boolean)
        {
            if (left.kind == MConstant::Integer)
                out->integer = ConstantBits(0 - std::uint64_t(left.integer));
            else
                out->floating = -left.floating;
            NormalizeConstant(out);
            return true;
        }
        return Fail("[MGL1300] 常量一元运算与类型不兼容");
    }
    if ((node.s == "&&" || node.s == "||") && left.kind == MConstant::Boolean)
    {
        if ((node.s == "&&" && !left.integer) || (node.s == "||" && left.integer)) return true;
        if (!EvaluateConstantImpl(node.kids[1], &right, context)) return false;
        if (right.kind != MConstant::Boolean) return Fail("[MGL1300] 常量逻辑运算需要 bool");
        *out = right;
        return true;
    }
    if (!EvaluateConstantImpl(node.kids[1], &right, context)) return false;
    const unsigned floatingBits = std::max(left.kind == MConstant::Floating ? left.bits : 0u, right.kind == MConstant::Floating ? right.bits : 0u);
    if (left.kind != right.kind && left.kind != MConstant::Boolean && right.kind != MConstant::Boolean)
    {
        if (left.kind == MConstant::Integer)
        {
            left.floating = left.isUnsigned ? double(std::uint64_t(left.integer)) : double(left.integer);
            left.kind = MConstant::Floating;
        }
        if (right.kind == MConstant::Integer)
        {
            right.floating = right.isUnsigned ? double(std::uint64_t(right.integer)) : double(right.integer);
            right.kind = MConstant::Floating;
        }
        out->kind = MConstant::Floating;
    }
    if (floatingBits == 32 && left.kind == MConstant::Floating && right.kind == MConstant::Floating)
    {
        left.floating = static_cast<float>(left.floating);
        right.floating = static_cast<float>(right.floating);
    }
    if (left.kind != right.kind) return Fail("[MGL1300] 常量二元运算需要相同类型");
    if (left.kind == MConstant::Integer)
    {
        unsigned width = std::max(left.bits, right.bits);
        bool u = left.isUnsigned || right.isUnsigned;
        left.bits = right.bits = width;
        left.isUnsigned = right.isUnsigned = u;
        NormalizeConstant(&left);
        NormalizeConstant(&right);
    }
    const auto &op = node.s;
    const bool comparison = op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
    if (comparison)
    {
        out->kind = MConstant::Boolean;
        const auto cmp = [&](auto a, auto b) { return op == "==" ? a == b : op == "!=" ? a != b :
                                                                        op == "<"      ? a < b :
                                                                        op == "<="     ? a <= b :
                                                                        op == ">"      ? a > b :
                                                                                         a >= b; };
        out->integer = left.kind == MConstant::Floating ? cmp(left.floating, right.floating) : (left.isUnsigned || right.isUnsigned ? cmp(std::uint64_t(left.integer), std::uint64_t(right.integer)) : cmp(left.integer, right.integer));
        return true;
    }
    if (left.kind == MConstant::Boolean) return Fail("[MGL1300] bool 常量不能用于算术");
    if (left.kind == MConstant::Floating)
    {
        out->bits = floatingBits;
        const double a = left.floating, b = right.floating;
        if ((op == "/" || op == "%") && b == 0) return Fail("[MGL1300] 常量表达式除零");
        if (op == "+")
            out->floating = a + b;
        else if (op == "-")
            out->floating = a - b;
        else if (op == "*")
            out->floating = a * b;
        else if (op == "/")
            out->floating = a / b;
        else
            return Fail("[MGL1300] 不支持的浮点常量运算：" + op);
        if (out->bits == 32) out->floating = static_cast<float>(out->floating);
        return std::isfinite(out->floating) || Fail("[MGL1300] 常量浮点运算溢出");
    }
    out->bits = std::max(left.bits, right.bits);
    out->isUnsigned = left.isUnsigned || right.isUnsigned;
    const auto a = std::uint64_t(left.integer), b = std::uint64_t(right.integer);
    if (op == "+")
        out->integer = ConstantBits(a + b);
    else if (op == "-")
        out->integer = ConstantBits(a - b);
    else if (op == "*")
        out->integer = ConstantBits(a * b);
    else if (op == "&")
        out->integer = ConstantBits(a & b);
    else if (op == "|")
        out->integer = ConstantBits(a | b);
    else if (op == "^")
        out->integer = ConstantBits(a ^ b);
    else if (op == "<<" || op == ">>")
    {
        if (right.integer < 0 || right.integer >= 64) return Fail("[MGL1300] 常量移位数必须在 0 到 63 之间");
        out->integer = op == "<<" ? ConstantBits(a << b) : (out->isUnsigned ? ConstantBits(a >> b) : left.integer >> b);
    }
    else if (op == "/" || op == "%")
    {
        if (!b) return Fail("[MGL1300] 常量表达式除零");
        if (out->isUnsigned)
            out->integer = ConstantBits(op == "/" ? a / b : a % b);
        else if (left.integer == LLONG_MIN && right.integer == -1)
            out->integer = op == "/" ? LLONG_MIN : 0;
        else
            out->integer = op == "/" ? left.integer / right.integer : left.integer % right.integer;
    }
    else
        return Fail("[MGL1300] 不支持的整数常量运算：" + op);
    NormalizeConstant(out);
    return true;
}
/**
 * @brief 按需求值常量初始化式或固定长度，不改变普通 const 的运行时语义。
 */
bool EvaluateConstant(int id, MConstant *out)
{
    MConstantContext validation;
    if (!ValidateConstantTree(id, validation)) return false;
    ExprType(id);
    if (!g_error.empty()) return false;
    MConstantContext context;
    return EvaluateConstantImpl(id, out, context);
}
