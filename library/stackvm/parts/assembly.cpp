// ---------------------------------------------------------------------------
//  组装
// ---------------------------------------------------------------------------

namespace
{

/**
 * @brief 组装期的临时状态。
 */
struct Assembler
{
    const QuadFile *file = nullptr;
    Program *program = nullptr;
    Memory::String sourceFile;
    std::int32_t sourceLine = 0;
    std::size_t quadLine = 0u;  ///< 当前正在组装的那条四元式在 `.quad` 里的行号。
    Memory::String function;    ///< 当前所在的函数 / 标签名。

    /**
     * @brief 追加一条指令，并顺手登记它的源码位置。
     *
     * 位置来自"当前正在组装的那条四元式"：行号是 `.quad` 的文本行、函数名是最近扫过的
     * `(label …)`。相同位置的连续指令由 `DebugInfo::add` 去重，表不会随指令数线性膨胀。
     *
     * @param instruction 指令。
     */
    void emit(Instruction instruction)
    {
        instruction.quadIndex = program->code.size();
        SourceLocation location;
        location.function = function;
        location.quadLine = quadLine;
        if (file != nullptr) location.file = file->path;
        if (sourceLine > 0)
        {
            location.file = sourceFile;
            location.line = sourceLine;
        }
        program->debug.add(program->code.size(), std::move(location));
        program->code.push_back(std::move(instruction));
    }

    /**
     * @brief 只带操作码的指令（避免聚合初始化漏字段的警告）。
     * @param op 操作码。
     */
    void emitOp(Op op)
    {
        Instruction instruction;
        instruction.op = op;
        emit(std::move(instruction));
    }

    /**
     * @brief 把操作数压栈（立即数直接压，变量名走 `LOAD`）。
     * @param token 操作数原文。
     * @param index 来自第几条四元式。
     * @return 成功返回 true。
     */
    bool operand(const Memory::String &token, std::size_t index)
    {
        if (token.empty())
        {
            program->diagnostics.push_back("第 " + Memory::toString(index + 1u) + " 条四元式：缺少操作数");
            return false;
        }

        // 1. 字符串字面量。
        if (token.front() == '"' || token.front() == '\'')
        {
            Instruction instruction;
            instruction.op = Op::PushText;
            instruction.textValue = unquote(token);
            emit(std::move(instruction));
            return true;
        }

        // 2. 数字。
        bool isFloat = false;
        std::int64_t intValue = 0;
        double doubleValue = 0.0;
        if (parseNumber(token, isFloat, intValue, doubleValue))
        {
            Instruction instruction;
            instruction.op = isFloat ? Op::PushDouble : Op::PushInt;
            instruction.intValue = intValue;
            instruction.doubleValue = doubleValue;
            emit(std::move(instruction));
            return true;
        }

        // 3. 变量。
        Instruction instruction;
        instruction.op = Op::Load;
        instruction.name = token;
        emit(std::move(instruction));
        return true;
    }

    /**
     * @brief 弹栈写回变量。
     * @param token 变量名。
     * @param index 来自第几条四元式。
     * @return 成功返回 true。
     */
    bool store(const Memory::String &token, std::size_t index)
    {
        if (token.empty())
        {
            program->diagnostics.push_back("第 " + Memory::toString(index + 1u) + " 条四元式：缺少结果变量");
            return false;
        }
        Instruction instruction;
        instruction.op = Op::Store;
        instruction.name = token;
        emit(std::move(instruction));
        return true;
    }

    /**
     * @brief 打印格式：优先用后缀类型，其次看变量声明。
     * @param typeName 后缀类型名（可空）。
     * @param variable 变量名（可空）。
     * @return 格式。
     */
    PrintFormat formatFor(const Memory::String &typeName, const Memory::String &variable) const
    {
        TypeInfo type;
        if (!typeName.empty())
        {
            type = typeOf(typeName);
        }
        else if (!variable.empty())
        {
            const auto found = program->types.find(variable);
            type = (found == program->types.end()) ? typeOf("") : found->second;
        }
        else
        {
            type = typeOf("");
        }
        if (type.isFloat) return PrintFormat::Float;
        return type.isUnsigned ? PrintFormat::Unsigned : PrintFormat::Signed;
    }

    /**
     * @brief 比较指令：按类型选有符号 / 无符号版本。
     * @param base 比较符（`<` 等）。
     * @param type 类型。
     * @return 操作码。
     */
    static Op compareOp(const Memory::String &base, const TypeInfo &type)
    {
        if (base == "==") return Op::Eq;
        if (base == "!=") return Op::Ne;
        if (!type.isUnsigned)
        {
            if (base == "<") return Op::Lt;
            if (base == "<=") return Op::Le;
            if (base == ">") return Op::Gt;
            return Op::Ge;
        }
        if (base == "<") return Op::LtU;
        if (base == "<=") return Op::LeU;
        if (base == ">") return Op::GtU;
        return Op::GeU;
    }
};

/**
 * @brief 把 `op:type` 拆成基础操作符与类型。
 * @param op 原文。
 * @param typeName 输出类型名。
 * @return 基础操作符。
 */
Memory::String splitType(const Memory::String &op, Memory::String &typeName)
{
    // `:=` 自己就带冒号，不能按类型后缀切。
    if (op == ":=")
    {
        typeName.clear();
        return op;
    }
    const auto colon = op.find(':');
    if ((colon == Memory::String::npos) || (colon + 1u >= op.size()))
    {
        typeName.clear();
        return op;
    }
    typeName = op.substr(colon + 1u);
    return op.substr(0u, colon);
}

}  // namespace

/**
 * @brief 把四元式组装成栈指令。
 * @param file 四元式文件。
 * @param out 组装结果。
 * @return 成功返回 true（`out.diagnostics` 里给出不支持的原因）。
 */
bool assemble(const QuadFile &file, Program &out)
{
    out = Program{};
    out.name = file.name;
    if (!file.ok())
    {
        out.diagnostics = file.diagnostics;
        return false;
    }
    out.requiredFeatures = file.requiredFeatures;
    out.reflection = file.reflection;
    if (!validateProgramMetadata(out, &out.diagnostics)) return false;

    Assembler assembler;
    assembler.file = &file;
    assembler.program = &out;

    // 0. 先扫一遍"标签 → 形参名"：前端把形参声明排在函数标签**前面**
    //    （`(decl, T, local, a)` … `(label, _, _, f)`），所以紧挨着标签之前那一段
    //    连续的 decl 就是这个函数的形参、顺序就是实参顺序。函数指针的间接调用
    //    按位置绑定实参，靠的就是这张表。
    {
        Memory::Vector<Memory::String> pending;
        for (const Quad &quad : file.quads)
        {
            Memory::String ignored;
            const Memory::String base = splitType(quad.op, ignored);
            if (base == "decl")
            {
                if (quad.arg(1) == "local") pending.push_back(quad.arg(2));
                continue;
            }
            if (base == "label")
            {
                if (!pending.empty()) out.parameters[quad.arg(2)] = pending;
                pending.clear();
                continue;
            }
            pending.clear();
        }
    }

    for (std::size_t index = 0; index < file.quads.size(); ++index)
    {
        const Quad &quad = file.quads[index];
        Memory::String typeName;
        const Memory::String base = splitType(quad.op, typeName);
        assembler.quadLine = quad.line;
        assembler.sourceFile = quad.sourceFile;
        assembler.sourceLine = quad.sourceLine;

        // 1. 声明：登记类型，并各发一条 `DECLARE`。
        //    顶层执行时调用帧是空的，`DECLARE` 就落在全局槽上 —— **全局槽必须在程序开头
        //    就建好**，否则"先读后写"的代码（典型是线程入口函数读全局）会报未定义变量。
        //    线程从指令 0 开始跑，所以它也顺带把本线程的全局槽建出来（各自隔离的副本）。
        if (base == "decl")
        {
            out.types[quad.arg(2)] = typeOf(quad.arg(0));
            Instruction instruction;
            instruction.op = Op::Declare;
            instruction.name = quad.arg(2);
            assembler.emit(std::move(instruction));
            continue;
        }
        // 2. 标签：记下当前位置（也当作后续指令的"函数名"，调试信息要用）。
        if (base == "label")
        {
            out.labels[quad.arg(2)] = out.code.size();
            assembler.function = quad.arg(2);
            if (!quad.arg(2).empty())
            {
                SourceLocation location;
                location.function = quad.arg(2);
                location.quadLine = quad.line;
                location.file = quad.sourceLine > 0 ? quad.sourceFile : file.path;
                location.line = quad.sourceLine;
                out.debug.add(out.code.size(), std::move(location));
            }
            continue;
        }
        // 3. 停机。
        if (base == "halt")
        {
            assembler.emitOp(Op::Halt);
            continue;
        }

        // 4. 赋值与一元运算。
        if (base == ":=")
        {
            assembler.operand(quad.arg(0), index);
            assembler.store(quad.arg(2), index);
            continue;
        }
        if (base == "neg" || base == "~")
        {
            const TypeInfo type = typeName.empty() ? typeOf("") : typeOf(typeName);
            assembler.operand(quad.arg(0), index);
            Instruction instruction;
            instruction.op = (base == "neg") ? Op::Neg : Op::Not;
            instruction.intValue = type.bits;
            instruction.unsignedOp = type.isUnsigned;
            instruction.isFloat = type.isFloat;
            assembler.emit(std::move(instruction));
            assembler.store(quad.arg(2), index);
            continue;
        }

        // 5. 类型转换。
        if (base == "cvt")
        {
            const TypeInfo type = typeOf(typeName);
            assembler.operand(quad.arg(0), index);
            Instruction instruction;
            instruction.op = type.isFloat ? Op::ToDouble : Op::ToInt;
            instruction.intValue = type.bits;
            instruction.unsignedOp = type.isFloat ? typeOf(quad.arg(1)).isUnsigned : type.isUnsigned;
            instruction.isFloat = type.isFloat;
            assembler.emit(std::move(instruction));
            assembler.store(quad.arg(2), index);
            continue;
        }

        // 6. 算术 / 位运算。
        static const Memory::Map<Memory::String, Op> kBinary = {
            { "+", Op::Add },
            { "-", Op::Sub },
            { "*", Op::Mul },
            { "/", Op::Div },
            { "%", Op::Mod },
            { "&", Op::BitAnd },
            { "|", Op::BitOr },
            { "^", Op::BitXor },
        };
        const auto binary = kBinary.find(base);
        if (binary != kBinary.end())
        {
            const TypeInfo type = typeName.empty() ? typeOf("") : typeOf(typeName);
            assembler.operand(quad.arg(0), index);
            assembler.operand(quad.arg(1), index);
            Instruction instruction;
            instruction.op = binary->second;
            instruction.intValue = type.bits;
            instruction.unsignedOp = type.isUnsigned;
            instruction.isFloat = type.isFloat;
            assembler.emit(std::move(instruction));
            assembler.store(quad.arg(2), index);
            continue;
        }
        if (base == "<<" || base == ">>")
        {
            const TypeInfo type = typeOf(typeName);
            assembler.operand(quad.arg(0), index);
            assembler.operand(quad.arg(1), index);
            Op op = Op::Shl;
            if (base == ">>") op = type.isUnsigned ? Op::ShrU : Op::Shr;
            Instruction instruction;
            instruction.op = op;
            instruction.intValue = type.bits;
            instruction.unsignedOp = type.isUnsigned;
            assembler.emit(std::move(instruction));
            assembler.store(quad.arg(2), index);
            continue;
        }

        // 7. 比较。
        if (base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=")
        {
            assembler.operand(quad.arg(0), index);
            assembler.operand(quad.arg(1), index);
            assembler.emitOp(Assembler::compareOp(base, typeOf(typeName)));
            assembler.store(quad.arg(2), index);
            continue;
        }

        // 8. 双结果乘除：`(mul:u32, a, b, lo, hi)` / `(div:u32, a, b, q, r)`。
        if (base == "mul" || base == "div")
        {
            // 双字乘除按类型位宽切高低半 / 商余，所以把位宽与符号带进指令。
            TypeInfo wideType = typeName.empty() ? typeOf("") : typeOf(typeName);
            if (typeName.empty())
            {
                const auto declared = out.types.find(quad.arg(0));
                if (declared != out.types.end()) wideType = declared->second;
            }
            assembler.operand(quad.arg(0), index);
            assembler.operand(quad.arg(1), index);
            Instruction wide;
            wide.op = (base == "mul") ? Op::MulWide : Op::DivWide;
            wide.intValue = wideType.bits;
            wide.unsignedOp = wideType.isUnsigned;
            assembler.emit(std::move(wide));
            assembler.store(quad.arg(3), index);  // 先弹 hi / r
            assembler.store(quad.arg(2), index);  // 再弹 lo / q
            continue;
        }

        // 9. 跳转。
        if (base == "j" || base == "jz" || base == "jnz")
        {
            Instruction instruction;
            if (base != "j") assembler.operand(quad.arg(0), index);
            instruction.op = (base == "j") ? Op::Jmp : ((base == "jz") ? Op::Jz : Op::Jnz);
            instruction.name = quad.arg(2);
            assembler.emit(std::move(instruction));
            continue;
        }

        // 10. 输入输出。
        if (base == "write")
        {
            assembler.operand(quad.arg(0), index);
            Instruction instruction;
            instruction.op = Op::Print;
            instruction.format = assembler.formatFor(typeName, quad.arg(0));
            assembler.emit(std::move(instruction));
            continue;
        }
        if (base == "print")
        {
            Instruction instruction;
            instruction.op = Op::PrintText;
            instruction.textValue = unquote(quad.arg(0));
            assembler.emit(std::move(instruction));
            continue;
        }
        if (base == "putc")
        {
            assembler.operand(quad.arg(0), index);
            assembler.emitOp(Op::Putc);
            continue;
        }
        if (base == "read" || base == "getc")
        {
            Instruction instruction;
            instruction.op = (base == "read") ? Op::Read : Op::Getc;
            instruction.name = quad.arg(2);
            assembler.emit(std::move(instruction));
            continue;
        }

        // 11. 调用与返回。
        if (base == "param")
        {
            assembler.operand(quad.arg(0), index);
            Instruction instruction;
            instruction.op = Op::Param;
            instruction.name = quad.arg(2);
            assembler.emit(std::move(instruction));
            continue;
        }
        if (base == "call")
        {
            Instruction instruction;
            instruction.op = Op::Call;
            instruction.name = quad.arg(0);
            instruction.name2 = quad.arg(2);
            assembler.emit(std::move(instruction));
            continue;
        }
        if (base == "ret")
        {
            assembler.operand(quad.arg(0), index);
            assembler.emitOp(Op::Ret);
            continue;
        }

        // 11.5 数据区三件套：`(addr, 符号, _, t)` / `(load, 指针, _, 目标)` / `(store, 指针, 值, _)`。
        if (base == "addr")
        {
            Instruction instruction;
            instruction.op = Op::AddrOf;
            instruction.name = quad.arg(0);
            assembler.emit(std::move(instruction));
            assembler.store(quad.arg(2), index);
            continue;
        }
        if (base == "alloc")
        {
            // `(alloc, 字节数, _, 目标)`：弹栈取字节数，把拿到的地址压栈再由 `store` 写目标。
            assembler.operand(quad.arg(0), index);
            assembler.emitOp(Op::Alloc);
            assembler.store(quad.arg(2), index);
            continue;
        }
        if (base == "free")
        {
            // `(free, 地址, _, _)`：把地址压栈，`Op::Free` 弹出来还回堆里。
            assembler.operand(quad.arg(0), index);
            assembler.emitOp(Op::Free);
            continue;
        }
        if (base == "drop")
        {
            // `(drop, 地址, _, _)`：软释放（作用域出口用；地址不是活跃堆块就跳过）。
            assembler.operand(quad.arg(0), index);
            assembler.emitOp(Op::Drop);
            continue;
        }
        if (base == "check")
        {
            // `(check, 下标, 上限, 标签)`：下标越界就终止运行（上限可以是常量也可以是变量）。
            assembler.operand(quad.arg(0), index);
            assembler.operand(quad.arg(1), index);
            Instruction instruction;
            instruction.op = Op::Check;
            instruction.name = quad.arg(2);
            assembler.emit(std::move(instruction));
            continue;
        }
        if (base == "checkptr")
        {
            // `(checkptr, 下标, 基址, 步长[, 标签])`：按"基址所在那一块"的容量查下标。
            assembler.operand(quad.arg(0), index);
            assembler.operand(quad.arg(1), index);
            std::int64_t stride = 8;
            if (!quad.arg(2).empty()) stride = std::strtoll(quad.arg(2).c_str(), nullptr, 10);
            Instruction instruction;
            instruction.op = Op::CheckPointer;
            instruction.intValue = stride;
            instruction.name = quad.arg(3);
            assembler.emit(std::move(instruction));
            continue;
        }
        if (base == "callptr")
        {
            // `(callptr, 函数指针, _, 结果)`：目标是个**值**（不是标签名），所以它的
            // 地址先压栈，`Op::CallIndirect` 弹出来去数据区读标签。
            assembler.operand(quad.arg(0), index);
            Instruction instruction;
            instruction.op = Op::CallIndirect;
            instruction.name2 = quad.arg(2);
            assembler.emit(std::move(instruction));
            continue;
        }
        if (base == "load")
        {
            assembler.operand(quad.arg(0), index);
            Instruction instruction;
            instruction.op = Op::LoadAt;
            // `(load:f64, 指针, _, 目标)`：浮点按 IEEE-754 双精度解释这 8 个字节
            // （数据区每个槽位就是 8 字节，整型与浮点共用同一片内存）。
            instruction.isFloat = typeOf(typeName).isFloat;
            assembler.emit(std::move(instruction));
            assembler.store(quad.arg(2), index);
            continue;
        }
        if (base == "store")
        {
            assembler.operand(quad.arg(0), index);  // 指针
            assembler.operand(quad.arg(1), index);  // 值
            Instruction instruction;
            instruction.op = Op::StoreAt;
            instruction.isFloat = typeOf(typeName).isFloat;
            assembler.emit(std::move(instruction));
            continue;
        }

        // 12. 其它：明确报出来，而不是悄悄跳过。
        out.diagnostics.push_back("第 " + Memory::toString(index + 1u) + " 条四元式：本虚拟机不支持操作 '" + quad.op + "'");
    }

    // 13. 收尾：没有 Halt 就补一条，避免"掉出程序尾"。
    if (out.code.empty() || out.code.back().op != Op::Halt)
    {
        Instruction halt;
        halt.op = Op::Halt;
        out.code.push_back(std::move(halt));
    }
    /* 裸四元式没有可见性语法，保持历史行为：它的标签默认都是公开入口。
     * 具备模块语义的前端可在 assemble 后收窄这张表。 */
    for (const auto &[label, ignored] : out.labels)
    {
        (void)ignored;
        out.exports.insert(label);
    }
    return out.ok();
}
