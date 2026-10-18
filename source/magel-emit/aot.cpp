/**
 * @file aot.cpp
 * @brief `--target cpp-aot`：把 `.quad` 翻译成**基本块 + 局部变量 + `goto`** 的 C++。
 *
 * 不重写运行时：字符串 / 容器 / 大整数 / BigDecimal / 对象表 / 边界检查 / 指针失效检查 /
 * 宿主绑定都通过内嵌的 `stackvm::RuntimeBridge` 复用解释器那套既有实现；AOT 只做计算
 * 与控制流。
 */
#include "aot.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <cstdlib>
#include <map>
#include <set>
#include <vector>

namespace magel_emit
{
namespace
{

using Memory::String;

/** @brief 为宿主调试器去除诊断用宏标签。 @param file 来源标签。 @return 实际源文件路径。 */
String sourcePath(const String& file) {
    const auto macro=file.rfind(" [宏 ");
    return macro!=String::npos && !file.empty() && file.back()==']' ? file.substr(0,macro) : file;
}

/** 拆 `+ : i64` 这种带类型后缀的操作符。 */
String baseOp(const String &op, String *suffix)
{
    if (op.rfind(":=", 0u) == 0u)
    {
        *suffix = op.substr(2u);
        return ":=";
    }
    const std::size_t pos = op.find(':');
    if (pos == String::npos)
    {
        *suffix = String();
        return op;
    }
    *suffix = op.substr(pos + 1u);
    return op.substr(0u, pos);
}

/** 把任意名字净化成合法 C++ 标识符片段。 */
String mangle(const String &text)
{
    String out;
    out.reserve(text.size() + 1u);
    for (const char ch : text)
    {
        const unsigned char c = static_cast<unsigned char>(ch);
        out.push_back((std::isalnum(c) != 0 || ch == '_') ? ch : '_');
    }
    if (out.empty()) out = "empty";
    if (std::isdigit(static_cast<unsigned char>(out.front())) != 0) out.insert(out.begin(), 'x');
    return out;
}

/** 整数 / 浮点字面量的 C++ 文本。 */
String numberLiteral(const String &token) { return token; }

bool isQuoted(const String &token)
{
    return !token.empty() && (token.front() == '"' || token.front() == '\'');
}

bool isLiteralToken(const String &token)
{
    if (token.empty() || token == "_") return false;
    const char front = token.front();
    return std::isdigit(static_cast<unsigned char>(front)) != 0 || front == '-' || front == '+' ||
           front == '.' || front == '"' || front == '\'';
}

bool literalIsFloat(const String &token)
{
    if (isQuoted(token)) return false;
    if (token.find("0x") != String::npos || token.find("0X") != String::npos) return false;
    return token.find('.') != String::npos || token.find('e') != String::npos ||
           token.find('E') != String::npos;
}

struct VarType
{
    bool isFloat = false;
    int bits = 64;
    bool isUnsigned = false;
    bool known = false;
    bool declared = false; ///< 来自 `decl` / 形参：不允许被用法推断降级。
    /**
     * 编译期推不出整数 / 浮点的值：常见于宿主内建（`std::math::*`）与
     * `__vm_vec_at` 这类按元素类型返回的重载。生成代码用一个 `RuntimeValue`
     * 槽同时保存整数、浮点与运行期标签，使用点再按上下文取值。
     */
    bool dynamic = false;
};

struct Function
{
    String label;                        ///< 空 = 顶层。
    std::vector<std::size_t> quads;      ///< 属于本函数的四元式下标。
    std::vector<std::pair<String, VarType>> params;
    std::map<String, VarType> vars;      ///< 形参 + 局部。
    bool retFloat = false;
};

struct Expr
{
    String text;
    VarType type;
    bool dynamic = false; ///< `text` 是 `RuntimeValue` 变量名。
};

String intText(long long value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%lld", value);
    return String(buffer);
}

/** C++ 字符串字面量（转义）。 */
String quoteCpp(const String &text)
{
    String out = "\"";
    for (const char ch : text)
    {
        switch (ch)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    out += "\"";
    return out;
}

} // namespace

Memory::String generateAotFile(const AotRequest &request,
                                  Memory::Vector<Memory::String> *diagnostics, AotModules *modules)
{
    const auto fail = [&](const String &message) -> String {
        if (diagnostics != nullptr) diagnostics->push_back(message);
        return String();
    };

    if (request.quads == nullptr || request.program == nullptr)
        return fail("AOT：缺少四元式或程序");

    const stackvm::QuadFile &quads = *request.quads;
    const stackvm::Program &program = *request.program;
    if (!stackvm::validateProgramMetadata(program, diagnostics)) return String();

    /* ------------------------------------------------ 1. 找出函数入口 + 切函数 */
    std::set<String> allLabels;
    std::vector<String> labelOrder;
    for (const stackvm::Quad &quad : quads.quads)
    {
        String suffix;
        if (baseOp(quad.op, &suffix) != "label") continue;
        const String name = quad.arg(2);
        if (name.empty()) continue;
        if (allLabels.insert(name).second) labelOrder.push_back(name);
    }
    /* `.layout` 里字符串符号的内容：`fn_twice` 这类函数指针数据指向标签名。
     * 注意 `program.parameters` **不能**用来判定函数入口——组装器会给每个
     * 前面有 `(decl …)` 的标签（包括 `__magel_loop_1` / `else_1` 这些块标签）都记一份。 */
    std::map<String, String> dataStrings;
    for (const AotDataSymbol &symbol : request.symbols)
    {
        String key = symbol.name;
        if (key.size() >= 2u && key.front() == '"' && key.back() == '"')
            key = key.substr(1u, key.size() - 2u);
        if (symbol.offset < 0) continue;
        const std::size_t start = static_cast<std::size_t>(symbol.offset);
        if (start >= request.dataBytes.size()) continue;
        String value;
        for (std::size_t i = start; i < request.dataBytes.size(); ++i)
        {
            const char byte = request.dataBytes[i];
            if (byte == '\0') break;
            value.push_back(byte);
        }
        dataStrings[key] = value;
    }

    std::set<String> functionEntries;
    for (const auto &entry : request.cExportEntries)
        if (allLabels.count(entry)) functionEntries.insert(entry);
    for (const stackvm::Quad &quad : quads.quads)
    {
        String suffix;
        const String base = baseOp(quad.op, &suffix);
        if (base != "call" && base != "addr") continue;
        const String target = quad.arg(0);
        if (allLabels.count(target) != 0u)
        {
            functionEntries.insert(target);
            continue;
        }
        /* `addr fn_twice`：数据符号内容是标签名，说明该标签可被函数指针调用。 */
        const auto content = dataStrings.find(target);
        if (content != dataStrings.end() && allLabels.count(content->second) != 0u)
            functionEntries.insert(content->second);
    }
    if (allLabels.count("main") != 0u) functionEntries.insert("main");
    if (!labelOrder.empty()) functionEntries.insert(labelOrder.front());

    std::vector<Function> functions;
    {
        Function top;
        functions.push_back(top); // functions[0] = 顶层
        std::size_t current = 0u;
        std::vector<std::pair<String, VarType>> pendingParams;
        for (std::size_t index = 0; index < quads.quads.size(); ++index)
        {
            const stackvm::Quad &quad = quads.quads[index];
            String suffix;
            const String base = baseOp(quad.op, &suffix);
            if (base == "decl")
            {
                if (quad.arg(1) == "local")
                {
                    VarType type;
                    const stackvm::TypeInfo info = stackvm::typeOf(quad.arg(0));
                    type.isFloat = info.isFloat;
                    type.bits = info.bits;
                    type.isUnsigned = info.isUnsigned;
                    type.known = true;
                    type.declared = true;
                    pendingParams.emplace_back(quad.arg(2), type);
                }
                functions[current].quads.push_back(index);
                continue;
            }
            if (base == "label")
            {
                if (functionEntries.count(quad.arg(2)) != 0u)
                {
                    Function fn;
                    fn.label = quad.arg(2);
                    fn.params = pendingParams;
                    for (auto &param : fn.params) fn.vars[param.first] = param.second;
                    functions.push_back(std::move(fn));
                    current = functions.size() - 1u;
                }
                pendingParams.clear();
                functions[current].quads.push_back(index);
                continue;
            }
            pendingParams.clear();
            functions[current].quads.push_back(index);
        }
    }

    /* ------------------------------------------------------------ 2. 全局变量 */
    std::map<String, VarType> globals;
    {
        for (const std::size_t index : functions[0].quads)
        {
            const stackvm::Quad &quad = quads.quads[index];
            String suffix;
            if (baseOp(quad.op, &suffix) != "decl") continue;
            VarType type;
            const stackvm::TypeInfo info = stackvm::typeOf(quad.arg(0));
            type.isFloat = info.isFloat;
            type.bits = info.bits;
            type.isUnsigned = info.isUnsigned;
            type.known = true;
            globals[quad.arg(2)] = type;
        }
    }

    /* 每个变量的类型推断：先登记声明，再按用法补全。 */
    const auto inferType = [&](const String &suffix, VarType fallback) {
        if (suffix.empty()) return fallback;
        VarType type;
        const stackvm::TypeInfo info = stackvm::typeOf(suffix);
        type.isFloat = info.isFloat;
        type.bits = info.bits;
        type.isUnsigned = info.isUnsigned;
        type.known = true;
        return type;
    };

    for (std::size_t round = 0; round < 3u; ++round)
    {
        for (Function &fn : functions)
        {
            for (const std::size_t index : fn.quads)
            {
                const stackvm::Quad &quad = quads.quads[index];
                String suffix;
                const String base = baseOp(quad.op, &suffix);
                const VarType opType = inferType(suffix, VarType{});
                const auto note = [&](const String &token, const VarType &type) {
                    if (token.empty() || token == "_" || isLiteralToken(token)) return;
                    if (globals.count(token) != 0u) return;
                    const auto existing = fn.vars.find(token);
                    if (existing != fn.vars.end() && existing->second.declared && !type.declared)
                        return;
                    if (type.known) fn.vars[token] = type;
                    else if (existing == fn.vars.end())
                    {
                        VarType unknown;
                        fn.vars[token] = unknown; /* 先记为未知，收尾时再决定是否动态。 */
                    }
                };
                if (base == "decl")
                {
                    VarType declared;
                    const stackvm::TypeInfo info = stackvm::typeOf(quad.arg(0));
                    declared.isFloat = info.isFloat;
                    declared.bits = info.bits;
                    declared.isUnsigned = info.isUnsigned;
                    declared.known = true;
                    declared.declared = true;
                    fn.vars[quad.arg(2)] = declared;
                }
                else if (base == ":=")
                {
                    VarType source = opType;
                    if (isLiteralToken(quad.arg(0)))
                    {
                        source = VarType{};
                        source.known = true;
                        source.isFloat = literalIsFloat(quad.arg(0));
                    }
                    else if (fn.vars.count(quad.arg(0)) != 0u) source = fn.vars.at(quad.arg(0));
                    else if (globals.count(quad.arg(0)) != 0u) source = globals.at(quad.arg(0));
                    note(quad.arg(0), source);
                    note(quad.arg(2), source);
                }
                else if (base == "neg" || base == "~" || base == "cvt")
                {
                    note(quad.arg(0), base == "cvt" && quad.arg(1) != "_" ? inferType(quad.arg(1), VarType{}) : opType);
                    note(quad.arg(2), opType);
                }
                else if (base == "+" || base == "-" || base == "*" || base == "/" || base == "%" ||
                         base == "&" || base == "|" || base == "^" || base == "<<" || base == ">>")
                {
                    note(quad.arg(0), opType);
                    note(quad.arg(1), opType);
                    note(quad.arg(2), opType);
                }
                else if (base == "==" || base == "!=" || base == "<" || base == "<=" ||
                         base == ">" || base == ">=")
                {
                    /* 比较后缀描述操作数；比较结果始终为布尔整数槽。 */
                    note(quad.arg(0), VarType{});
                    note(quad.arg(1), VarType{});
                    VarType boolean;
                    boolean.known = true;
                    boolean.bits = 32;
                    note(quad.arg(2), boolean);
                }
                else if (base == "load")
                {
                    note(quad.arg(2), opType);
                }
                else if (base == "addr" || base == "alloc" || base == "read" || base == "getc")
                {
                    VarType whole;
                    whole.known = true;
                    note(quad.arg(2), whole);
                }
                else if (base == "call" || base == "callptr")
                {
                    note(quad.arg(2), VarType{}); /* 返回类型未知：可能是浮点。 */
                }
                else if (base == "param")
                {
                    note(quad.arg(0), VarType{});
                }
                else if (base == "jz" || base == "jnz" || base == "ret")
                {
                    note(quad.arg(0), VarType{});
                }
                else if (base == "check" || base == "checkptr" || base == "store")
                {
                    note(quad.arg(0), VarType{});
                    note(quad.arg(1), VarType{});
                }
            }
        }
    }

    /* 函数返回类型：看 `ret` 的操作数。 */
    std::map<String, bool> retFloat;
    for (const Function &fn : functions)
    {
        if (fn.label.empty()) continue;
        bool isFloat = false;
        for (const std::size_t index : fn.quads)
        {
            const stackvm::Quad &quad = quads.quads[index];
            String suffix;
            if (baseOp(quad.op, &suffix) != "ret") continue;
            const String token = quad.arg(0);
            if (isLiteralToken(token)) { if (literalIsFloat(token)) isFloat = true; }
            else if (fn.vars.count(token) != 0u && fn.vars.at(token).isFloat) isFloat = true;
            else if (globals.count(token) != 0u && globals.at(token).isFloat) isFloat = true;
        }
        retFloat[fn.label] = isFloat;
    }

    /* 用函数返回类型补 `call` 的目的地类型。 */
    for (Function &fn : functions)
    {
        for (const std::size_t index : fn.quads)
        {
            const stackvm::Quad &quad = quads.quads[index];
            String suffix;
            if (baseOp(quad.op, &suffix) != "call") continue;
            const String target = quad.arg(0);
            const String dest = quad.arg(2);
            if (dest.empty() || dest == "_" || isLiteralToken(dest)) continue;
            if (globals.count(dest) != 0u) continue;
            const auto found = retFloat.find(target);
            if (found == retFloat.end()) continue;
            VarType type;
            type.known = true;
            type.isFloat = found->second;
            fn.vars[dest] = type;
        }
    }

    /* 仍然推不出类型的值：按运行期标签处理（宿主内建 / 重载容器读出的浮点）。 */
    for (Function &fn : functions)
    {
        for (auto &item : fn.vars)
        {
            if (item.second.known) continue;
            item.second.known = true;
            item.second.dynamic = true;
            item.second.isFloat = false;
        }
    }

    /* 唯一 label ID：不同 MAGEL 标签可能净化成同一 C++ 名，统一加序号。 */
    std::map<String, String> labelId;
    for (const String &name : labelOrder)
        labelId[name] = mangle(name) + "_" + intText(static_cast<long long>(labelId.size()));

    const auto varName = [&](const Function &fn, const String &token) -> String {
        if (globals.count(token) != 0u) return "magel_globals(rt).v_" + mangle(token);
        (void)fn;
        return "v_" + mangle(token);
    };
    const auto typeOfVar = [&](const Function &fn, const String &token) -> VarType {
        const auto global = globals.find(token);
        if (global != globals.end()) return global->second;
        const auto local = fn.vars.find(token);
        if (local != fn.vars.end()) return local->second;
        VarType fallback;
        fallback.known = true;
        return fallback;
    };

    /* 用到软件定时器 / 中断才在每个基本块入口发 `boundary()`（交付中断）。 */
    bool usesEventTimers = false;
    for (const stackvm::Quad &quad : quads.quads)
    {
        String suffix;
        const String base = baseOp(quad.op, &suffix);
        if ((base == "call" || base == "callptr") &&
            quad.arg(0).rfind("std::event::", 0u) == 0u)
        {
            usesEventTimers = true;
            break;
        }
    }

    /* -------------------------------------------------------------- 3. 生成体 */
    String code;
    std::vector<String> body;         ///< 每个函数一段。
    std::vector<String> forward;      ///< 前置声明（函数可以相互递归 / 前向引用）。
    forward.push_back(
        String(modules ? "" : "static ") + "bool magel_aot_callptr(stackvm::RuntimeBridge &rt, std::int64_t fptr, "
        "const std::int64_t *magel_args, std::size_t magel_count, std::int64_t &magel_out, "
        "bool &magel_isFloat);\n");

    for (const Function &fn : functions)
    {
        const bool top = fn.label.empty();
        const String retType = top ? String("void")
                                   : (retFloat.count(fn.label) != 0u && retFloat.at(fn.label)
                                          ? String("double")
                                          : String("int64_t"));
        const String zero = (retType == "double") ? String("0.0") : String("0");

        String parameters;
        for (const auto &param : fn.params)
        {
            const String type = param.second.isFloat ? String("double") : String("int64_t");
            parameters += ", " + type + " v_" + mangle(param.first);
        }
        if (!top)
        {
            forward.push_back(String(modules ? "" : "static ") + retType + " magel_fn_" + mangle(fn.label) +
                              "(stackvm::RuntimeBridge &rt" + parameters + ");\n");
        }

        String out;
        out += (modules && !top) ? "" : "static ";
        out += top ? String("void ") : (retType + " ");
        out += top ? String("magel_aot_entry(stackvm::RuntimeBridge &rt)")
                   : ("magel_fn_" + mangle(fn.label) + "(stackvm::RuntimeBridge &rt");
        if (!top)
        {
            for (const auto &param : fn.params)
            {
                const String type = param.second.isFloat ? String("double") : String("int64_t");
                out += ", " + type + " v_" + mangle(param.first);
            }
            out += ")";
        }
        out += "\n{\n";
        out += "#if STACKVM_WITH_PANIC_TRACE\n    stackvm::RuntimeCallFrame magel_trace_frame(rt, " + quoteCpp(top ? String("(入口)") : fn.label) + ");\n#endif\n";

        /* 局部声明放函数最前，避免 `goto` 跳过初始化。 */
        for (const auto &item : fn.vars)
        {
            bool isParam = false;
            for (const auto &param : fn.params)
                if (param.first == item.first) { isParam = true; break; }
            if (isParam) continue;
            out += "    ";
            out += item.second.dynamic ? "stackvm::RuntimeValue "
                                       : (item.second.isFloat ? "double " : "int64_t ");
            out += "v_" + mangle(item.first) + " = ";
            out += item.second.dynamic ? "stackvm::RuntimeValue{};\n"
                                       : (item.second.isFloat ? "0.0;\n" : "0;\n");
        }

        bool hasNativeCalls = false;
        for (const std::size_t quadIndex : fn.quads)
        {
            String nativeSuffix;
            const auto &candidate = quads.quads[quadIndex];
            if (baseOp(candidate.op, &nativeSuffix) == "call" &&
                (program.labels.count(candidate.arg(0)) == 0u || labelId.count(candidate.arg(0)) == 0u)) hasNativeCalls = true;
        }
        // 缓冲区归当前调用帧所有；同步回调重入时使用独立调用帧，不覆盖外层参数。
        if (hasNativeCalls) out += "    Memory::Vector<stackvm::RuntimeValue> magel_native_args;\n";

        /* `param` 攒下来的实参：保留形参名，调用点按名字绑定（VM 也是按名字），
         * 不能按位置——`decl` 顺序未必等于源码形参顺序。 */
        std::vector<std::pair<String, Expr>> pending;

        for (const std::size_t index : fn.quads)
        {
            const stackvm::Quad &quad = quads.quads[index];
            String suffix;
            const String base = baseOp(quad.op, &suffix);
            const VarType opType = inferType(suffix, VarType{});
            const auto exprOf = [&](const String &token) -> Expr {
                if (isLiteralToken(token))
                {
                    Expr expr;
                    expr.text = numberLiteral(token);
                    expr.type.known = true;
                    expr.type.isFloat = literalIsFloat(token);
                    return expr;
                }
                Expr expr;
                expr.text = varName(fn, token);
                expr.type = typeOfVar(fn, token);
                expr.dynamic = expr.type.dynamic;
                return expr;
            };
            const auto castTo = [](const Expr &expr, bool wantFloat) -> String {
                if (expr.dynamic || expr.type.dynamic)
                {
                    if (wantFloat && expr.type.isUnsigned)
                        return "(double)(std::uint64_t)(magel_as_i(" + expr.text + "))";
                    return wantFloat ? ("magel_as_f(" + expr.text + ")")
                                     : ("magel_as_i(" + expr.text + ")");
                }
                if (wantFloat)
                    return expr.type.isFloat ? expr.text : (expr.type.isUnsigned ? ("(double)(std::uint64_t)(" + expr.text + ")") : ("(double)(" + expr.text + ")"));
                return expr.type.isFloat ? ("(std::int64_t)(" + expr.text + ")") : expr.text;
            };
            /* 静态赋值：来源的整数 / 浮点性是编译期已知的。 */
            const auto assign = [&](const String &dest, const String &rhs, bool rhsFloat) {
                const VarType dt = typeOfVar(fn, dest);
                if (dt.dynamic)
                {
                    out += "    " + varName(fn, dest) + " = ";
                    out += rhsFloat
                               ? ("stackvm::RuntimeValue::ofFloat(" + rhs + ");\n")
                               : ("stackvm::RuntimeValue::ofInt((std::int64_t)(" + rhs + "));\n");
                    return;
                }
                String value = rhs;
                if (dt.isFloat)
                {
                    if (!rhsFloat) value = "(double)(" + rhs + ")";
                    else if (dt.bits == 32) value = "magel_norm_f(" + rhs + ", 32)";
                }
                else
                {
                    if (rhsFloat) value = "(std::int64_t)(" + rhs + ")";
                    else if (dt.bits < 64)
                        value = "magel_norm_i(" + rhs + ", " + intText(dt.bits) + ", " +
                                (dt.isUnsigned ? "true" : "false") + ")";
                }
                out += "    " + varName(fn, dest) + " = " + value + ";\n";
            };
            /* 表达式赋值：来源可能是运行期带标签的动态值。 */
            const auto assignExpr = [&](const String &dest, const Expr &value) {
                const VarType dt = typeOfVar(fn, dest);
                if (value.dynamic || value.type.dynamic)
                {
                    if (dt.dynamic)
                    {
                        out += "    " + varName(fn, dest) + " = " + value.text + ";\n";
                        return;
                    }
                    assign(dest, castTo(value, dt.isFloat), dt.isFloat);
                    return;
                }
                assign(dest, value.text, value.type.isFloat);
            };
            const auto guard = [&]() {
                out += "    if (rt.failed()) return " + zero + ";\n";
            };

            /* 调试信息优先使用 MAGEL 原始位置，无来源时回退四元式。 */
            if (request.debugInfo && quad.line > 0u)
                out += "#line " + intText(static_cast<long long>(quad.sourceLine > 0 ? quad.sourceLine : quad.line)) + " " +
                       quoteCpp(quad.sourceLine > 0 ? sourcePath(quad.sourceFile) : quads.path) + "\n";

            // panic 的源码位置独立于 C++ 调试符号；隐私裁剪仍由 PANIC_TRACE 宏统一控制。
            if (quad.sourceLine > 0 && base != "label")
                out += "#if STACKVM_WITH_PANIC_TRACE\n    rt.locateTrace(" + quoteCpp(quad.sourceFile) + ", " + intText(quad.sourceLine) + ");\n#endif\n";
            if (base == "decl") continue;
            if (base == "label")
            {
                out += "L_" + labelId[quad.arg(2)] + ": ;\n";
                /* 用到软件定时器 / 中断的程序：每个基本块入口交付一轮（解释器是每条指令
                 * 边界交付；AOT 只在控制流汇合点交付，粒度更粗但语义一致）。 */
                if (usesEventTimers || request.threads)
                    out += top ? String("    if (!rt.boundary()) return;\n")
                               : ("    if (!rt.boundary()) return " + zero + ";\n");
                continue;
            }
            if (base == "halt")
            {
                out += top ? String("    return;\n") : ("    return " + zero + ";\n");
                continue;
            }
            if (base == ":=")
            {
                const Expr value = exprOf(quad.arg(0));
                if (quad.arg(2).empty() || quad.arg(2) == "_")
                {
                    /* 丢弃值。 */
                    out += "    (void)(" + value.text + ");\n";
                    continue;
                }
                assignExpr(quad.arg(2), value);
                continue;
            }
            if (base == "neg" || base == "~")
            {
                const Expr value = exprOf(quad.arg(0));
                if (base == "~")
                {
                    assign(quad.arg(2), "~" + castTo(value, false), false);
                }
                else if (opType.isFloat)
                {
                    assign(quad.arg(2), "magel_norm_f(-" + castTo(value, true) + ", " +
                                            intText(opType.bits) + ")",
                           true);
                }
                else
                {
                    assign(quad.arg(2), "magel_negi(" + castTo(value, false) + ", " +
                                            intText(opType.bits) + ", " +
                                            (opType.isUnsigned ? "true" : "false") + ")",
                           false);
                }
                continue;
            }
            if (base == "cvt")
            {
                Expr value = exprOf(quad.arg(0));
                if (quad.arg(1) != "_") value.type.isUnsigned = stackvm::typeOf(quad.arg(1)).isUnsigned;
                if (opType.isFloat)
                    assign(quad.arg(2),
                           "magel_norm_f(" + castTo(value, true) + ", " + intText(opType.bits) + ")",
                           true);
                else
                    assign(quad.arg(2),
                           "magel_norm_i(" + castTo(value, false) + ", " + intText(opType.bits) + ", " +
                               (opType.isUnsigned ? "true" : "false") + ")",
                           false);
                continue;
            }
            if (base == "+" || base == "-" || base == "*" || base == "/" || base == "%" ||
                base == "&" || base == "|" || base == "^" || base == "<<" || base == ">>")
            {
                const Expr left = exprOf(quad.arg(0));
                const Expr right = exprOf(quad.arg(1));
                const String dest = quad.arg(2);
                const bool isFloat = opType.isFloat;
                String lhs = castTo(left, isFloat);
                String rhs = castTo(right, isFloat);
                const String bits = intText(opType.bits);
                const String uns = opType.isUnsigned ? "true" : "false";
                if (base == "+")
                    assign(dest, isFloat ? ("magel_norm_f(" + lhs + " + " + rhs + ", " + bits + ")")
                                         : ("magel_addi(" + lhs + ", " + rhs + ", " + bits + ", " + uns + ")"),
                           isFloat);
                else if (base == "-")
                    assign(dest, isFloat ? ("magel_norm_f(" + lhs + " - " + rhs + ", " + bits + ")")
                                         : ("magel_subi(" + lhs + ", " + rhs + ", " + bits + ", " + uns + ")"),
                           isFloat);
                else if (base == "*")
                    assign(dest, isFloat ? ("magel_norm_f(" + lhs + " * " + rhs + ", " + bits + ")")
                                         : ("magel_muli(" + lhs + ", " + rhs + ", " + bits + ", " + uns + ")"),
                           isFloat);
                else if (base == "/" || base == "%")
                {
                    const String op = (base == "/") ? "div" : "mod";
                    if (isFloat)
                    {
                        out += "    if (" + rhs + " == 0.0) { rt.fail(\"浮点除零\"); return " + zero +
                               "; }\n";
                        const String result =
                            (base == "/") ? ("magel_norm_f(" + lhs + " / " + rhs + ", " + bits + ")")
                                          : ("magel_norm_f(std::fmod(" + lhs + ", " + rhs + "), " + bits + ")");
                        assign(dest, result, true);
                    }
                    else
                    {
                        out += "    if (" + rhs + " == 0) { rt.fail(\"整数除零\"); return " + zero +
                               "; }\n";
                        assign(dest,
                               "magel_" + op + "i(" + lhs + ", " + rhs + ", " + bits + ", " + uns + ")",
                               false);
                    }
                }
                else if (base == "&")
                    assign(dest, "(" + lhs + " & " + rhs + ")", false);
                else if (base == "|")
                    assign(dest, "(" + lhs + " | " + rhs + ")", false);
                else if (base == "^")
                    assign(dest, "(" + lhs + " ^ " + rhs + ")", false);
                else if (base == "<<")
                    assign(dest, "magel_shl(" + castTo(left, false) + ", " + castTo(right, false) + ")", false);
                else if (base == ">>")
                    assign(dest,
                           opType.isUnsigned
                               ? ("magel_shru(" + castTo(left, false) + ", " + castTo(right, false) + ")")
                               : ("magel_shr(" + castTo(left, false) + ", " + castTo(right, false) + ")"),
                           false);
                continue;
            }
            if (base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" ||
                base == ">=")
            {
                const Expr left = exprOf(quad.arg(0));
                const Expr right = exprOf(quad.arg(1));
                const bool useFloat = left.type.isFloat || right.type.isFloat || left.dynamic ||
                                      right.dynamic || left.type.dynamic || right.type.dynamic;
                String compare;
                if (base == "==" || base == "!=")
                {
                    const String a = castTo(left, useFloat);
                    const String b = castTo(right, useFloat);
                    compare = "(" + a + (base == "==" ? " == " : " != ") + b + ")";
                }
                else if (opType.isUnsigned)
                {
                    compare = "((std::uint64_t)(" + castTo(left, false) + ") " + base +
                              " (std::uint64_t)(" + castTo(right, false) + "))";
                }
                else if (!useFloat)
                {
                    compare = "(" + castTo(left, false) + " " + base + " " + castTo(right, false) + ")";
                }
                else
                {
                    compare = "((double)(" + castTo(left, true) + ") " + base + " (double)(" +
                              castTo(right, true) + "))";
                }
                assign(quad.arg(2), compare, false);
                continue;
            }
            if (base == "j" || base == "jz" || base == "jnz")
            {
                String condition;
                if (base != "j")
                {
                    const Expr value = exprOf(quad.arg(0));
                    if (value.dynamic || value.type.dynamic)
                        condition = "(magel_truthy(" + value.text + "))";
                    else
                        condition = value.type.isFloat ? ("(" + value.text + " != 0.0)")
                                                       : ("(" + value.text + " != 0)");
                }
                const String target = "L_" + labelId[quad.arg(2)];
                if (base == "j") out += "    goto " + target + ";\n";
                else if (base == "jz") out += "    if (!" + condition + ") goto " + target + ";\n";
                else out += "    if (" + condition + ") goto " + target + ";\n";
                continue;
            }
            if (base == "param")
            {
                pending.emplace_back(quad.arg(2), exprOf(quad.arg(0)));
                continue;
            }
            if (base == "call")
            {
                const String target = quad.arg(0);
                const String dest = quad.arg(2);
                const auto known = program.labels.find(target);
                if (known != program.labels.end() && labelId.count(target) != 0u)
                {
                    const Function *callee = nullptr;
                    for (const Function &candidate : functions)
                        if (candidate.label == target) { callee = &candidate; break; }
                    const auto findArg = [&](const String &name) -> const Expr * {
                        for (const auto &item : pending)
                            if (item.first == name) return &item.second;
                        return nullptr;
                    };
                    String call = "magel_fn_" + mangle(target) + "(rt";
                    if (callee != nullptr)
                    {
                        /* 按被调函数的形参顺序（`decl` 顺序）逐个取同名实参。 */
                        for (const auto &param : callee->params)
                        {
                            const Expr *argument = findArg(param.first);
                            const bool wantFloat = param.second.isFloat;
                            if (argument == nullptr)
                            {
                                call += wantFloat ? ", 0.0" : ", 0";
                                continue;
                            }
                            call += ", " + castTo(*argument, wantFloat);
                        }
                    }
                    else
                    {
                        for (const auto &item : pending) call += ", " + item.second.text;
                    }
                    call += ")";
                    const bool returnsFloat = retFloat.count(target) != 0u && retFloat.at(target);
                    if (dest.empty() || dest == "_")
                        out += "    " + call + ";\n";
                    else if (typeOfVar(fn, dest).dynamic)
                        out += "    " + varName(fn, dest) + " = " +
                               (returnsFloat ? ("stackvm::RuntimeValue::ofFloat(" + call + ")")
                                             : ("stackvm::RuntimeValue::ofInt((std::int64_t)(" + call + "))")) +
                               ";\n";
                    else
                        assign(dest, call, returnsFloat);
                }
                else
                {
                    String call = "    { stackvm::RuntimeValue magel_rv;\n";
                    call += "      magel_native_args.resize(" + intText(static_cast<long long>(pending.size())) + ");\n";
                    for (std::size_t i = 0; i < pending.size(); ++i)
                    {
                        call += "      magel_native_args[" + intText(static_cast<long long>(i)) + "] = ";
                        const Expr &argument = pending[i].second;
                        if (argument.dynamic || argument.type.dynamic)
                            call += argument.text;
                        else if (argument.type.isFloat)
                            call += "stackvm::RuntimeValue::ofFloat(" + argument.text + ")";
                        else
                            call += "stackvm::RuntimeValue::ofInt((std::int64_t)(" + argument.text + "))";
                        call += ";\n";
                    }
                    call += "      rt.callNative(\"" + target + "\", magel_native_args, magel_rv);\n";
                    call += "      if (rt.failed()) return " + zero + ";\n";
                    if (!dest.empty() && dest != "_")
                    {
                        const VarType dt = typeOfVar(fn, dest);
                        if (dt.dynamic)
                            call += "      v_" + mangle(dest) + " = magel_rv;\n";
                        else if (dt.isFloat)
                            call += "      v_" + mangle(dest) + " = magel_rv.isFloat ? magel_rv.real : (double)magel_rv.integer;\n";
                        else
                            call += "      v_" + mangle(dest) + " = magel_rv.integer;\n";
                    }
                    call += "    }\n";
                    out += call;
                }
                pending.clear();
                continue;
            }
            if (base == "ret")
            {
                if (top)
                {
                    out += "    return;\n";
                    continue;
                }
                const Expr value = exprOf(quad.arg(0));
                const bool wantFloat = retType == "double";
                out += "    return " + castTo(value, wantFloat) + ";\n";
                continue;
            }
            if (base == "addr")
            {
                out += "    if (!rt.dataAddress(\"" + quad.arg(0) + "\", " +
                       varName(fn, quad.arg(2)) + ")) return " + zero + ";\n";
                continue;
            }
            if (base == "alloc")
            {
                const Expr bytes = exprOf(quad.arg(0));
                out += "    if (!rt.allocate((std::int64_t)(" + bytes.text + "), " +
                       varName(fn, quad.arg(2)) + ")) return " + zero + ";\n";
                continue;
            }
            if (base == "free" || base == "drop")
            {
                const Expr address = exprOf(quad.arg(0));
                out += "    rt.release((std::int64_t)(" + address.text + "), " +
                       (base == "drop" ? String("true") : String("false")) + ");\n";
                guard();
                continue;
            }
            if (base == "check")
            {
                const Expr index = exprOf(quad.arg(0));
                const Expr limit = exprOf(quad.arg(1));
                out += "    if ((std::int64_t)(" + castTo(index, false) + ") < 0 || (std::int64_t)(" + castTo(index, false) + ") >= (std::int64_t)(" + castTo(limit, false) + ")) { rt.checkIndex((std::int64_t)(" + castTo(index, false) +
                       "), (std::int64_t)(" + castTo(limit, false) + "), \"" + quad.arg(2) +
                       "\"); return " + zero + "; }\n";
                continue;
            }
            if (base == "checkptr")
            {
                const Expr index = exprOf(quad.arg(0));
                const Expr baseAddress = exprOf(quad.arg(1));
                String stride = quad.arg(2);
                if (stride.empty() || stride == "_") stride = "8";
                out += "    if (!rt.checkPointer((std::int64_t)(" + castTo(index, false) +
                       "), (std::int64_t)(" + castTo(baseAddress, false) + "), (std::int64_t)(" +
                       stride + "), \"" + quad.arg(3) + "\")) return " + zero + ";\n";
                continue;
            }
            if (base == "load")
            {
                const Expr address = exprOf(quad.arg(0));
                const String dest = quad.arg(2);
                const bool isFloat = opType.isFloat;
                const VarType staticDest = typeOfVar(fn, dest);
                if (!staticDest.dynamic && staticDest.isFloat == isFloat)
                {
                    out += "    if (!rt." + String(isFloat ? "loadFloat" : "loadInteger") + "((std::int64_t)(" + castTo(address, false) + "), " + varName(fn, dest) + ")) return " + zero + ";\n";
                    continue;
                }
                out += "    { stackvm::RuntimeValue magel_rv; if (!rt.loadMemory((std::int64_t)(" +
                       castTo(address, false) + "), " + (isFloat ? "true" : "false") +
                       ", magel_rv)) return " + zero + ";\n";
                const VarType loadDest = typeOfVar(fn, dest);
                if (loadDest.dynamic)
                    out += "      " + varName(fn, dest) + " = magel_rv; }\n";
                else if (isFloat)
                    out += "      " + varName(fn, dest) +
                           " = magel_rv.isFloat ? magel_rv.real : (double)magel_rv.integer; }\n";
                else
                    out += "      " + varName(fn, dest) + " = magel_rv.integer; }\n";
                continue;
            }
            if (base == "store")
            {
                const Expr address = exprOf(quad.arg(0));
                const Expr value = exprOf(quad.arg(1));
                const bool isFloat = opType.isFloat;
                if (!value.dynamic && !value.type.dynamic)
                {
                    out += "    if (!rt." + String(isFloat ? "storeFloat" : "storeInteger") + "((std::int64_t)(" + castTo(address, false) + "), " + castTo(value, isFloat) + ")) return " + zero + ";\n";
                    continue;
                }
                out += "    { stackvm::RuntimeValue magel_rv = ";
                if (value.dynamic || value.type.dynamic)
                    out += value.text + ";\n";
                else if (isFloat)
                    out += "stackvm::RuntimeValue::ofFloat(" + castTo(value, true) + ");\n";
                else
                    out += "stackvm::RuntimeValue::ofInt((std::int64_t)(" +
                           castTo(value, false) + "));\n";
                out += "      if (!rt.storeMemory((std::int64_t)(" + castTo(address, false) + "), " +
                       (isFloat ? "true" : "false") + ", magel_rv)) return " + zero + ";\n";
                out += "    }\n";
                continue;
            }
            if (base == "print")
            {
                out += "    rt.appendOutput(" + quoteCpp(quad.arg(0)) + ");\n";
                continue;
            }
            if (base == "write")
            {
                const Expr value = exprOf(quad.arg(0));
                out += "    rt.appendValue(";
                if (value.dynamic || value.type.dynamic)
                    out += value.text + ");\n";
                else
                    out += value.type.isFloat
                               ? ("stackvm::RuntimeValue::ofFloat(" + value.text + "));\n")
                               : ("stackvm::RuntimeValue::ofInt((std::int64_t)(" + value.text + ")));\n");
                continue;
            }
            if (base == "putc")
            {
                const Expr value = exprOf(quad.arg(0));
                out += "    rt.appendChar((int)(std::int64_t)(" + castTo(value, false) + "));\n";
                continue;
            }
            if (base == "read" || base == "getc")
            {
                const String dest = quad.arg(2);
                const VarType dt = typeOfVar(fn, dest);
                out += "    { stackvm::RuntimeValue magel_rv; if (!rt.readInput(" +
                       String(base == "getc" ? "true" : "false") + ", " +
                       ((dt.isFloat || dt.dynamic) ? "true" : "false") + ", magel_rv)) return " + zero + ";\n";
                if (dt.dynamic)
                    out += "      " + varName(fn, dest) + " = magel_rv; }\n";
                else
                    out += "      " + varName(fn, dest) +
                           (dt.isFloat ? " = magel_rv.real; }\n" : " = magel_rv.integer; }\n");
                continue;
            }
            if (base == "callptr")
            {
                /* `(callptr, 函数指针, _, 结果)`：函数指针是数据区里的标签名地址，
                 * 运行期读出标签走 `magel_aot_callptr` 分派到对应的 MAGEL 函数。参数统一
                 * 打成 `int64_t` 位模式（浮点按 IEEE-754 位转换），由被调函数的
                 * 分发包装按自己的形参类型还原。 */
                const Expr pointer = exprOf(quad.arg(0));
                const String dest = quad.arg(2);
                out += "    {\n";
                if (!pending.empty())
                {
                    out += "        const std::int64_t magel_args[] = {";
                    for (std::size_t i = 0; i < pending.size(); ++i)
                    {
                        if (i != 0u) out += ", ";
                        const Expr &argument = pending[i].second;
                        if (argument.dynamic || argument.type.dynamic)
                            out += "magel_dyn_bits(" + argument.text + ")";
                        else
                            out += argument.type.isFloat
                                       ? ("magel_aot::magel_d2b(" + castTo(argument, true) + ")")
                                       : castTo(argument, false);
                    }
                    out += "};\n";
                }
                out += "        std::int64_t magel_res = 0; bool magel_res_f = false;\n";
                out += "        if (!magel_aot_callptr(rt, (std::int64_t)(" + castTo(pointer, false) +
                       "), " + (pending.empty() ? String("nullptr") : String("magel_args")) + ", " +
                       intText(static_cast<long long>(pending.size())) +
                       "u, magel_res, magel_res_f)) return " + zero + ";\n";
                if (!dest.empty() && dest != "_")
                {
                    const VarType dt = typeOfVar(fn, dest);
                    if (dt.dynamic)
                        out += "        " + varName(fn, dest) +
                               " = magel_res_f ? stackvm::RuntimeValue::ofFloat(magel_aot::magel_b2d(magel_res)) "
                               ": stackvm::RuntimeValue::ofInt(magel_res);\n";
                    else if (dt.isFloat)
                        out += "        " + varName(fn, dest) +
                               " = magel_res_f ? magel_aot::magel_b2d(magel_res) : (double)magel_res;\n";
                    else
                        out += "        " + varName(fn, dest) +
                               " = magel_res_f ? (std::int64_t)magel_aot::magel_b2d(magel_res) : magel_res;\n";
                }
                out += "    }\n";
                pending.clear();
                continue;
            }

            return fail("AOT：暂不支持的四元式操作 \"" + quad.op + "\"（第 " +
                        intText(static_cast<long long>(index + 1u)) + " 条）");
        }

        /* 收尾：函数末尾补 return，避免掉出。 */
        if (!fn.quads.empty())
        {
            const stackvm::Quad &last = quads.quads[fn.quads.back()];
            String suffix;
            const String base = baseOp(last.op, &suffix);
            if (base != "ret" && base != "halt")
                out += top ? String("    return;\n") : ("    return " + zero + ";\n");
        }
        out += "}\n\n";
        body.push_back(out);
    }

    /* --------------------------------------------------------- 4. 拼装整文件 */
    code += "// 由 magel-emit --target cpp-aot 生成：基本块 + 局部变量 + goto。\n";
    const String featureContract = "\nstatic_assert((" +
        intText(request.program->requiredFeatures) +
        "u & ~(STACKVM_WITH_REFLECTION ? 2u : 0u)) == 0, \"AOT required features unsupported\");\n";
    code += request.embeddedSources + featureContract;
    if (request.program->requiredFeatures & stackvm::kFeatureReflection) {
        const auto metadata = stackvm::reflection::cppDefinition(request.program->reflection);
        if (!metadata) { if (diagnostics) diagnostics->push_back("AOT 反射元数据无效"); return String(); }
        code += *metadata;
    }
    const std::size_t commonBegin = code.size();
    code += "\n// ---------------------------------------------------------------------------\n";
    code += "//  AOT 运行时辅助\n";
    code += "// ---------------------------------------------------------------------------\n";
    code += R"AOTPRE(
#include <cstring>
namespace magel_aot {
static std::int64_t magel_d2b(double value)
{
    std::int64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double 必须是 8 字节");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
static double magel_b2d(std::int64_t bits)
{
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
static std::int64_t magel_as_i(const stackvm::RuntimeValue &value)
{
    return value.isFloat ? static_cast<std::int64_t>(value.real) : value.integer;
}
static double magel_as_f(const stackvm::RuntimeValue &value)
{
    return value.isFloat ? value.real : static_cast<double>(value.integer);
}
static bool magel_truthy(const stackvm::RuntimeValue &value) { return magel_as_f(value) != 0.0; }
static std::int64_t magel_dyn_bits(const stackvm::RuntimeValue &value)
{
    return value.isFloat ? magel_d2b(value.real) : value.integer;
}
static std::int64_t magel_norm_i(std::int64_t value, int bits, bool uns)
{
    if (bits <= 0 || bits >= 64) return value;
    const std::int64_t mask = (static_cast<std::int64_t>(1) << bits) - 1;
    value &= mask;
    if (!uns && (value & (static_cast<std::int64_t>(1) << (bits - 1))))
        value -= (static_cast<std::int64_t>(1) << bits);
    return value;
}
static double magel_norm_f(double value, int bits)
{
    return bits == 32 ? static_cast<double>(static_cast<float>(value)) : value;
}
static std::int64_t magel_addi(std::int64_t a, std::int64_t b, int bits, bool uns)
{ return magel_norm_i(static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b)), bits, uns); }
static std::int64_t magel_subi(std::int64_t a, std::int64_t b, int bits, bool uns)
{ return magel_norm_i(static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)), bits, uns); }
static std::int64_t magel_muli(std::int64_t a, std::int64_t b, int bits, bool uns)
{ return magel_norm_i(static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b)), bits, uns); }
static std::int64_t magel_divi(std::int64_t a, std::int64_t b, int bits, bool uns)
{
    if (uns) return magel_norm_i(static_cast<std::int64_t>(static_cast<std::uint64_t>(a) / static_cast<std::uint64_t>(b)), bits, uns);
    if (a == INT64_MIN && b == -1) return a;
    return magel_norm_i(a / b, bits, uns);
}
static std::int64_t magel_modi(std::int64_t a, std::int64_t b, int bits, bool uns)
{
    if (uns) return magel_norm_i(static_cast<std::int64_t>(static_cast<std::uint64_t>(a) % static_cast<std::uint64_t>(b)), bits, uns);
    if (a == INT64_MIN && b == -1) return 0;
    return magel_norm_i(a % b, bits, uns);
}
static std::int64_t magel_negi(std::int64_t a, int bits, bool uns)
{ return magel_norm_i(static_cast<std::int64_t>(0u - static_cast<std::uint64_t>(a)), bits, uns); }
static std::int64_t magel_shl(std::int64_t a, std::int64_t b) { return a << (b & 63); }
static std::int64_t magel_shr(std::int64_t a, std::int64_t b) { return a >> (b & 63); }
static std::int64_t magel_shru(std::int64_t a, std::int64_t b)
{ return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) >> (b & 63)); }
} // namespace magel_aot
using magel_aot::magel_norm_i; using magel_aot::magel_norm_f; using magel_aot::magel_addi; using magel_aot::magel_subi;
using magel_aot::magel_muli; using magel_aot::magel_divi; using magel_aot::magel_modi; using magel_aot::magel_negi;
using magel_aot::magel_shl; using magel_aot::magel_shr; using magel_aot::magel_shru;
using magel_aot::magel_d2b; using magel_aot::magel_b2d; using magel_aot::magel_as_i; using magel_aot::magel_as_f;
using magel_aot::magel_truthy; using magel_aot::magel_dyn_bits;
static const char *const kProgramName = )AOTPRE";
    const String sharedHelpers = code.substr(commonBegin, code.size() - commonBegin - String("static const char *const kProgramName = ").size());
    code += quoteCpp(request.programName) + ";\n";

    /* 数据区。 */
    {
        code += "\nstatic const unsigned char kDataBytes[] = {\n";
        for (std::size_t i = 0; i < request.dataBytes.size(); ++i)
        {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "0x%02x,", static_cast<unsigned char>(request.dataBytes[i]));
            code += buffer;
            if ((i % 16u) == 15u) code += "\n";
        }
        code += "\n};\nstatic const std::size_t kDataSize = " +
                intText(static_cast<long long>(request.dataBytes.size())) + "u;\n";
        code += "static void loadData(stackvm::DataArea *data)\n{\n";
        code += "    data->binary.assign(kDataBytes, kDataBytes + kDataSize);\n";
        for (const AotDataSymbol &symbol : request.symbols)
        {
            code += "    data->symbols.emplace(" + symbol.name + ", " +
                    intText(symbol.offset) + "u);\n";
            if (symbol.hasSize)
                code += "    data->sizes.emplace(" + symbol.name + ", " +
                        intText(symbol.size) + "u);\n";
            if (symbol.hasStride)
                code += "    data->strides.emplace(" + symbol.name + ", " +
                        intText(symbol.stride) + "u);\n";
        }
        code += "}\n";
    }

    /* 全局变量。 */
    const std::size_t globalsBegin = code.size();
    code += "\nstruct McGlobals {\n";
    for (const auto &item : globals)
    {
        code += String("    ") + (item.second.isFloat ? "double " : "std::int64_t ") + "v_" +
                mangle(item.first) + (item.second.isFloat ? " = 0.0;\n" : " = 0;\n");
    }
    code += "};\n";
    /* 全局变量块挂在桥上：隔离线程各一份（零值副本），协程共享入口那一份。 */
    code += "static McGlobals &magel_globals(stackvm::RuntimeBridge &rt)\n{\n";
    code += "    return *static_cast<McGlobals *>(rt.globalsBlock());\n}\n\n";

    const String sharedGlobals = code.substr(globalsBegin);
    /* 前置声明确保相互调用 / 前向引用可编译。 */
    for (const String &text : forward) code += text;
    code += "\n";
    /* 函数体。 */
    if (modules)
    {
        modules->header = "#pragma once\n" + request.declarationSources + featureContract + sharedHelpers + sharedGlobals;
        for (const auto &text : forward) modules->header += text;
        Memory::Map<String, String> grouped;
        for (std::size_t index = 0; index < functions.size(); ++index)
        {
            const auto &label = functions[index].label;
            if (label.empty()) { code += body[index]; continue; }
            const auto separator = label.rfind("::");
            const String module = separator == String::npos ? String("__root") : label.substr(0, separator);
            grouped[module] += body[index];
        }
        for (auto &unit : grouped) modules->units.push_back(unit);
    }
    else for (const String &text : body) code += text;

    /* 函数指针分派：每个 MAGEL 函数一个统一包装，按标签名查表分派。 */
    {
        code += "\nstruct McDispatchEntry { const char *name; "
                "std::int64_t (*fn)(stackvm::RuntimeBridge &, const std::int64_t *, bool &); std::size_t arity; };\n";
        for (const Function &fn : functions)
        {
            if (fn.label.empty()) continue;
            const bool isFloat = retFloat.count(fn.label) != 0u && retFloat.at(fn.label);
            code += "static std::int64_t magel_dispatch_" + mangle(fn.label) +
                    "(stackvm::RuntimeBridge &rt, const std::int64_t *magel_args, bool &magel_isFloat)\n{\n";
            code += String("    magel_isFloat = ") + (isFloat ? "true" : "false") + ";\n";
            String call = String("    ") + (isFloat ? "double" : "std::int64_t") + " magel_value = magel_fn_" +
                          mangle(fn.label) + "(rt";
            for (std::size_t i = 0; i < fn.params.size(); ++i)
            {
                call += ", ";
                call += fn.params[i].second.isFloat
                            ? ("magel_aot::magel_b2d(magel_args[" + intText(static_cast<long long>(i)) + "])")
                            : ("magel_args[" + intText(static_cast<long long>(i)) + "]");
            }
            call += ");\n";
            code += call;
            code += isFloat ? "    return magel_aot::magel_d2b(magel_value);\n}\n"
                            : "    return magel_value;\n}\n";
        }
        code += "static const McDispatchEntry kMcDispatchTable[] = {\n";
        for (const Function &fn : functions)
        {
            if (fn.label.empty()) continue;
            code += "    {" + quoteCpp(fn.label) + ", magel_dispatch_" + mangle(fn.label) + ", " + intText(static_cast<long long>(fn.params.size())) + "u},\n";
        }
        code += "};\n";
        code += R"AOTINV(
static bool magel_aot_callptr(stackvm::RuntimeBridge &rt, std::int64_t fptr,
                           const std::int64_t *magel_args, std::size_t magel_count,
                           std::int64_t &magel_out, bool &magel_isFloat)
{
    if (fptr == 0)
    {
        rt.fail("AOT：调用了空函数指针");
        return false;
    }
    std::int64_t environment = 0;
    const auto target = rt.resolveFunction(fptr, environment);
    if (!target) return false;
    Memory::Vector<std::int64_t> closureArguments;
    if (environment) {
        if (magel_count) closureArguments.assign(magel_args, magel_args + magel_count);
        closureArguments.push_back(environment);
        magel_args = closureArguments.data();
        ++magel_count;
    }
    const Memory::String label = rt.readCString(target);
    for (const McDispatchEntry &entry : kMcDispatchTable)
    {
        if (label == entry.name)
        {
            if (magel_count != entry.arity)
            {
                rt.fail("AOT：通过函数指针调用 " + label + " 需要 " + Memory::toString(entry.arity) + " 个参数，实到 " + Memory::toString(magel_count) + " 个");
                return false;
            }
            magel_out = entry.fn(rt, magel_args, magel_isFloat);
            return !rt.failed();
        }
    }
    rt.fail("AOT：函数指针没有指向已知函数：" + label);
    return false;
}

/**
 * @brief 在**指定的桥**上按函数指针跑一个 MAGEL 函数（线程入口 / 协程入口 / 中断处理器都用）。
 * @param rt 目标桥：线程传新建的子桥，协程与中断传当前桥。
 */
static bool magel_aot_run_on(stackvm::RuntimeBridge &rt, std::int64_t fptr,
                          const Memory::Vector<stackvm::RuntimeValue> &args,
                          stackvm::RuntimeValue &result)
{
    Memory::Vector<std::int64_t> storage;
    for (const stackvm::RuntimeValue &argument : args)
    {
        if (argument.isText)
        {
            rt.fail("AOT：宿主回调不支持未编码的文本参数");
            return false;
        }
        storage.push_back(argument.isFloat ? magel_aot::magel_d2b(argument.real) : argument.integer);
    }
    std::int64_t out = 0;
    bool isFloat = false;
    if (!magel_aot_callptr(rt, fptr, storage.empty() ? nullptr : storage.data(),
                        storage.size(), out, isFloat))
        return false;
    result = isFloat ? stackvm::RuntimeValue::ofFloat(magel_aot::magel_b2d(out))
                     : stackvm::RuntimeValue::ofInt(out);
    return true;
}

static stackvm::RuntimeBridge *g_magel_bridge = nullptr;

static bool magel_aot_invoke(std::int64_t fptr,
                          const Memory::Vector<stackvm::RuntimeValue> &args,
                          stackvm::RuntimeValue &result)
{
    if (g_magel_bridge == nullptr) return false;
    return magel_aot_run_on(*g_magel_bridge, fptr, args, result);
}
)AOTINV";
    }

    /* 入口。 */
    code += "\n#ifdef _WIN32\n#include <io.h>\n#else\n#include <unistd.h>\n#endif\n";
    code += "\n#include <string>\n#include <string_view>\n#include <vector>\n";
    code += R"AOTMAIN(
static int runMagel(int argumentCount, const char *const *arguments, void *buffer,
                     std::size_t memoryBytes);

#ifndef MAGEL_NO_MAIN
int main(int argc, char **argv)
{
    std::size_t memoryBytes = )AOTMAIN";
    code += intText(request.runtimeArena) + "u;\n";
    code += R"AOTMAIN(
    std::vector<std::string> args;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument = argv[index];
        if (argument == "--memory-size") {
            if (index + 1 >= argc) { std::fprintf(stderr, "--memory-size 缺少容量\n"); return 2; }
            const Memory::String value = argv[++index];
            if (value.empty() || value.find_first_not_of("0123456789") != Memory::String::npos) return 2;
            try { memoryBytes = static_cast<std::size_t>(std::stoull(value)); } catch (...) { return 2; }
            if (memoryBytes < 64) return 2;
            continue;
        }
        if (argument == "--stats" || argument == "-s") continue;
        args.emplace_back(argument);
    }
    std::vector<const char *> pointers;
    for (const std::string &item : args) pointers.push_back(item.c_str());
    return runMagel(static_cast<int>(pointers.size()),
                     pointers.empty() ? nullptr : pointers.data(), nullptr, memoryBytes);
}
#endif

static int runMagel(int argumentCount, const char *const *arguments, void *buffer,
                     std::size_t memoryBytes)
{
    auto execute = [&](Memory::BufferRuntimeArena &arena) -> int {
        Memory::RuntimeSession session(arena);
        Memory::Vector<Memory::String> input;
        for (int i = 0; i < argumentCount; ++i) input.emplace_back(arguments[i]);

        stackvm::DataArea data;
        loadData(&data);

        magel_glue::Registry glue;
        Memory::String glueError;
        if (!bindMagelStandardGlue(glue, &glueError))
        {
            std::fprintf(stderr, "%s：标准胶水注册失败：%s\n", kProgramName, glueError.c_str());
            return 1;
        }
)AOTMAIN";
    if (request.hasAbi)
        code += "        if (!bindMagelAbi(glue, &glueError))\n        {\n            std::fprintf(stderr, \"%s：C ABI 绑定失败：%s\\n\", kProgramName, glueError.c_str());\n            return 1;\n        }\n";
    if (request.hasUser)
        code += "        if (!bindMagelGlue(glue, &glueError))\n        {\n            std::fprintf(stderr, \"%s：应用胶水注册失败：%s\\n\", kProgramName, glueError.c_str());\n            return 1;\n        }\n";
    code += R"AOTMAIN(
        stackvm::RuntimeBridge rt;
        rt.setData(&data);
        rt.setNativeRegistry(&glue.native());
        rt.setProgramArguments(input);
#ifdef _WIN32
        if (_isatty(_fileno(stdin)))
#else
        if (isatty(STDIN_FILENO))
#endif
            rt.setOutputSink([](void *, const char *bytes, std::size_t count) {
                std::fwrite(bytes, 1u, count, stdout);
            }, nullptr, false);

        g_magel_bridge = &rt;
        rt.setGlobalsSize(sizeof(McGlobals));
        rt.setFunctionRunner(magel_aot_run_on);
        rt.setInvoker(magel_aot_invoke);
)AOTMAIN";
    if (request.threads) code += "        rt.setThreadsEnabled(true);\n";
    code += R"AOTMAIN(
        rt.begin(0u);
        magel_aot_entry(rt);
        rt.end(); // 必须在自动回收子线程后判断错误，不能漏掉未显式等待的 panic。
        std::fwrite(rt.output().data(), 1u, rt.output().size(), stdout);
        const bool ok = !rt.failed();
        const Memory::String error = rt.error();
        if (!ok)
        {
            std::fprintf(stderr, MAGEL_TEXT("%s：运行失败：%s\n"), kProgramName, error.c_str());
#if STACKVM_WITH_PANIC_TRACE
            const auto trace = rt.callStack();
            if (!trace.empty()) std::fprintf(stderr, MAGEL_TEXT("调用栈（外 → 内）：\n"));
            for (const auto &frame : trace) {
                if (frame.location.line > 0) std::fprintf(stderr, "  %s  %s [thread %zu]\n", frame.function.c_str(), frame.location.toString().c_str(), frame.threadId);
                else std::fprintf(stderr, "  %s [thread %zu]\n", frame.function.c_str(), frame.threadId);
            }
#endif
            return 1;
        }
        return 0;
    };
    try {
        if (buffer) {
            Memory::BufferRuntimeArena arena(buffer, memoryBytes);
            return execute(arena);
        }
        Memory::DynamicRuntimeArena arena(memoryBytes);
        return execute(arena);
    } catch (const std::bad_alloc &) {
        std::fprintf(stderr, MAGEL_TEXT("运行时分配器：内存池容量不足\n"));
        return 1;
    }
}
)AOTMAIN";

    if (modules)
    {
        const String marker = "static bool magel_aot_callptr(";
        const auto at = code.find(marker);
        if (at != String::npos) code.erase(at, 7u);
    }
    return code;
}

} // namespace magel_emit
