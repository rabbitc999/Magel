// ---------------------------------------------------------------------------
//  四元式
// ---------------------------------------------------------------------------

/**
 * @brief 取下第 index 个参数；`_` 与不存在都算空。
 * @param index 下标。
 * @return 参数文本。
 */
Memory::String Quad::arg(std::size_t index) const
{
    if (index >= args.size()) return Memory::String();
    const Memory::String &value = args[index];
    return (value == "_") ? Memory::String() : value;
}

/**
 * @brief 解析 `.quad` 文本。
 * @param text 文本（UTF-8）。
 * @param label 诊断里显示的文件名。
 * @return 解析结果。
 */
QuadFile parseQuads(std::string_view text, Memory::String label)
{
    if (text.size() >= 4 && text.substr(0, 4) == "MGQI")
        return decodeQuadBinary(text, std::move(label));
    QuadFile file;
    file.path = std::move(label);

    Memory::InputStringStream stream{ Memory::String(text) };
    Memory::String line;
    std::size_t lineNumber = 0;
    Memory::String sourceFile;
    std::int32_t sourceLine = 0;
    bool seenReflection = false;

    while (std::getline(stream, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const Memory::String trimmed = trim(line);
        if (trimmed.empty()) continue;

        // 1. 注释：可能是元信息（名称 / 期望 / 输入）。
        if (trimmed[0] == '#')
        {
            const Memory::String body = trim(trimmed.substr(1u));
            const auto colon = body.find(':');
            if (colon == Memory::String::npos) continue;
            const Memory::String key = trim(body.substr(0u, colon));
            const Memory::String value = trim(body.substr(colon + 1u));
            if (key == "magel-source")
            {
                Memory::InputStringStream marker(value);
                std::int32_t number = 0;
                Memory::String path;
                if (marker >> number >> std::quoted(path) && number > 0)
                {
                    sourceLine = number;
                    sourceFile = std::move(path);
                }
                else
                {
                    sourceLine = 0;
                    sourceFile.clear();
                }
            }
            else if (key == "magel-features")
            {
                if (value == "reflection")
                    file.requiredFeatures |= kFeatureReflection;
                else
                    file.diagnostics.push_back("第 " + Memory::toString(lineNumber) + " 行：不支持的 magel-features 标志：" + value);
            }
            else if (key == "magel-reflection")
            {
                if (seenReflection || !reflection::fromHex(value, file.reflection))
                    file.diagnostics.push_back("第 " + Memory::toString(lineNumber) + " 行：反射元数据无效或重复");
                seenReflection = true;
            }
            else if (key == "名称" || key == "module" || key == "模块" || key == "name")
                file.name = value;
            else if (key == "期望")
                file.expected = value;
            else if (key == "期望文本")
                file.expectedText = value;
            else if (key == "输入")
                file.input = value;
            continue;
        }

        // 2. 四元式：`N: (op, a, b, r)`。
        const auto open = trimmed.find('(');
        const auto close = trimmed.rfind(')');
        if (open == Memory::String::npos || close == Memory::String::npos || close < open)
        {
            file.diagnostics.push_back(
                "第 " + Memory::toString(lineNumber) + " 行：不是四元式（缺少括号）");
            continue;
        }

        Memory::Vector<Memory::String> parts = splitArguments(trimmed.substr(open + 1u, close - open - 1u));
        if (parts.empty())
        {
            file.diagnostics.push_back(
                "第 " + Memory::toString(lineNumber) + " 行：四元式是空的");
            continue;
        }

        Quad quad;
        quad.line = lineNumber;
        quad.sourceFile = sourceFile;
        quad.sourceLine = sourceLine;
        quad.op = parts[0];
        for (std::size_t index = 1u; index < parts.size(); ++index) quad.args.push_back(parts[index]);
        file.quads.push_back(std::move(quad));
    }
    if (seenReflection && !(file.requiredFeatures & kFeatureReflection))
        file.diagnostics.push_back("反射元数据缺少 reflection 特征位");
    return file;
}

/**
 * @brief 读一个 `.quad` 文件。
 * @param path 路径。
 * @return 解析结果（打不开时诊断里给消息）。
 */
QuadFile loadQuadFile(const Memory::String &path)
{
#if STACKVM_WITH_HOST_IO

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        QuadFile file;
        file.path = path;
        file.diagnostics.push_back("打不开文件：" + path);
        return file;
    }
    Memory::OutputStringStream buffer;
    buffer << stream.rdbuf();
    return parseQuads(buffer.str(), path);

#else
    QuadFile file;
    file.diagnostics.push_back("这个构建禁用了宿主文件 I/O");
    return file;
#endif
}

/**
 * @brief 名字 → 类型；没写 `decl` 时按 `i16`（与 transfer 的旧语义一致）。
 * @param name 类型名；空串表示"没声明"。
 * @return 类型信息。
 */
TypeInfo typeOf(std::string_view name)
{
    TypeInfo type;
    type.name = Memory::String(name);
    if (name == "i8")
    {
        type.bits = 8;
        type.size = 1;
    }
    else if (name == "u8")
    {
        type.bits = 8;
        type.size = 1;
        type.isUnsigned = true;
    }
    else if (name == "i16" || name.empty())
    {
        type.bits = 16;
        type.size = 2;
    }
    else if (name == "u16")
    {
        type.bits = 16;
        type.size = 2;
        type.isUnsigned = true;
    }
    else if (name == "i32")
    {
        type.bits = 32;
        type.size = 4;
    }
    else if (name == "u32")
    {
        type.bits = 32;
        type.size = 4;
        type.isUnsigned = true;
    }
    else if (name == "i64")
    {
        type.bits = 64;
        type.size = 8;
    }
    else if (name == "u64")
    {
        type.bits = 64;
        type.size = 8;
        type.isUnsigned = true;
    }
    /* MAGEL 语言只写 float32 / float64；`f32` / `f64` 是**四元式层面**的旧名字，
     * 仓库里手写的 .quad 还在用，所以 VM 两个都认（VM 不是语言表面）。 */
    else if (name == "float32" || name == "f32")
    {
        type.bits = 32;
        type.size = 4;
        type.isFloat = true;
    }
    else if (name == "float64" || name == "f64")
    {
        type.bits = 64;
        type.size = 8;
        type.isFloat = true;
    }
    if (type.name.empty()) type.name = "i16";
    return type;
}

/**
 * @brief 将虚拟机指令编码为可读文本。
 * @retval 生成或读取的结果文本。
 */
Memory::String Instruction::toString() const
{
    switch (op)
    {
    case Op::PushInt: return "PUSH " + Memory::toString(intValue);
    case Op::PushDouble: return "PUSH " + formatDouble(doubleValue);
    case Op::PushText: return "PUSH \"" + textValue + "\"";
    case Op::Load: return "LOAD " + name;
    case Op::Store: return "STORE " + name;
    case Op::Declare: return "DECLARE " + name;
    case Op::Alloc: return "ALLOC";
    case Op::Free: return "FREE";
    case Op::Drop: return "DROP";
    case Op::Check: return "CHECK " + name;
    case Op::CheckPointer: return "CHECKPTR " + name + " stride " + Memory::toString(intValue);
    case Op::CallIndirect:
        return name2.empty() ? Memory::String("CALLPTR") : ("CALLPTR -> " + name2);
    case Op::Neg: return "NEG";
    case Op::Not: return "NOT";
    case Op::Add: return "ADD";
    case Op::Sub: return "SUB";
    case Op::Mul: return "MUL";
    case Op::Div: return "DIV";
    case Op::Mod: return "MOD";
    case Op::Shl: return "SHL";
    case Op::Shr: return "SAR";
    case Op::ShrU: return "SHR";
    case Op::BitAnd: return "AND";
    case Op::BitOr: return "OR";
    case Op::BitXor: return "XOR";
    case Op::Eq: return "EQ";
    case Op::Ne: return "NE";
    case Op::Lt: return "LT";
    case Op::Le: return "LE";
    case Op::Gt: return "GT";
    case Op::Ge: return "GE";
    case Op::LtU: return "LTU";
    case Op::LeU: return "LEU";
    case Op::GtU: return "GTU";
    case Op::GeU: return "GEU";
    case Op::ToDouble: return "I2F";
    case Op::ToInt: return "F2I";
    case Op::MulWide: return "MULWIDE";
    case Op::DivWide: return "DIVWIDE";
    case Op::Jmp: return "JMP " + name;
    case Op::Jz: return "JZ " + name;
    case Op::Jnz: return "JNZ " + name;
    case Op::Print:
        switch (format)
        {
        case PrintFormat::Unsigned: return "PRINT u";
        case PrintFormat::Float: return "PRINT f";
        case PrintFormat::Text: return "PRINT text";
        default: return "PRINT";
        }
    case Op::PrintText: return "PRINT \"" + textValue + "\"";
    case Op::Read: return "READ " + name;
    case Op::Getc: return "GETC " + name;
    case Op::Putc: return "PUTC";
    case Op::Param: return "PARAM " + name;
    case Op::Call: return name2.empty() ? ("CALL " + name) : ("CALL " + name + " -> " + name2);
    case Op::Ret: return "RET";
    case Op::Halt: return "HALT";
    case Op::AddrOf: return "ADDR " + name;
    case Op::LoadAt: return "LOADAT";
    case Op::StoreAt: return "STOREAT";
    }
    return "?";
}

/**
 * @brief 将源码位置编码为可读文本。
 * @retval 生成或读取的结果文本。
 */
Memory::String SourceLocation::toString() const
{
    if (line > 0)
    {
        return file.empty() ? ("第 " + Memory::toString(line) + " 行") : (file + ":" + Memory::toString(line));
    }
    if (quadLine > 0u)
    {
        return (file.empty() ? Memory::String("四元式") : file) + ":" + Memory::toString(quadLine);
    }
    return function.empty() ? Memory::String("?") : function;
}

/**
 * @brief 追加一条记录；与上一条位置完全相同则跳过（省地方）。
 * @param instruction 指令下标；必须不小于已有的最后一条。
 * @param location 位置。
 */
void DebugInfo::add(std::size_t instruction, SourceLocation location)
{
    if (!location.known()) return;
    if (!entries.empty())
    {
        const auto &last = entries.back();
        // 同一指令下标重复登记（函数标签本身不产生指令，"标签"与"紧跟它的第一条语句"
        // 会落在同一条指令上）：后来者更精确，覆盖掉。这样**每个下标最多一条**。
        if (last.first == instruction)
        {
            entries.back().second = std::move(location);
            return;
        }
        // 位置没变（一条语句的几十条指令）就不用再记，表因此保持很小。
        if (last.second.function == location.function && last.second.file == location.file && last.second.line == location.line && last.second.quadLine == location.quadLine)
            return;
    }
    entries.emplace_back(instruction, std::move(location));
}

/**
 * @brief 取不超过 instruction 的最近一条记录。
 * @param instruction 指令下标。
 * @return 命中返回指针；表里没有更早的记录返回 nullptr。
 */
const SourceLocation *DebugInfo::lookup(std::size_t instruction) const
{
    if (entries.empty() || instruction < entries.front().first) return nullptr;
    std::size_t low = 0u;
    std::size_t high = entries.size();
    while (low + 1u < high)
    {
        const std::size_t middle = (low + high) / 2u;
        if (entries[middle].first <= instruction)
            low = middle;
        else
            high = middle;
    }
    return &entries[low].second;
}
