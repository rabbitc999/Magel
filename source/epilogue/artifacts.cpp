/* ---------- 产物 ---------- */

/**
 * @brief 将诊断消息列表用中文分号连接为一段文本。
 * @param messages 待连接的诊断消息列表。
 * @retval 生成或读取的结果文本。
 */
Memory::String JoinMessages(const Memory::Vector<Memory::String> &messages)
{
    Memory::String text;
    for (std::size_t index = 0; index < messages.size(); ++index)
    {
        if (index != 0u) text += "；";
        text += messages[index];
    }
    return text;
}

/**
 * @brief 将生成内容写入指定产物文件。
 * @param path 目标文件或目录的路径。
 * @param text 待处理的文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool WriteFile(const Memory::String &path, const Memory::String &text)
{
    /* 父目录可能还没建（默认产物在 generate/ 下），先补上再写；
     * 目录建不起来时下面打开流自然会失败，错误信息照旧。 */
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
    {
        std::error_code created;
        std::filesystem::create_directories(parent, created);
    }
    std::ofstream stream(path, std::ios::binary);
    if (!stream) return Fail("写不了文件：" + path);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) return Fail("写文件失败：" + path);
    return true;
}

/**
 * @brief 将文本编码为合法 C++ 字符串字面量。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成或读取的结果文本。
 */
Memory::String CxxStringLiteral(const Memory::String &value)
{
    Memory::String out = "\"";
    for (unsigned char ch : value)
    {
        if (ch == '\\' || ch == '"')
        {
            out += '\\';
            out += static_cast<char>(ch);
        }
        else if (ch == '\n')
            out += "\\n";
        else if (ch == '\r')
            out += "\\r";
        else if (ch == '\t')
            out += "\\t";
        else
            out += static_cast<char>(ch);
    }
    return out + '"';
}

/**
 * @brief 生成 C ABI 胶水绑定所需的类型表达式。
 * @param type 类型描述。
 * @retval 生成或读取的结果文本。
 */
Memory::String CAbiBindingType(const std::shared_ptr<MType> &type)
{
    if (const auto reference = CAbiOptionalReference(type))
        return Memory::String("magel_glue::detail::CAbiOptionalReference<") + (reference->readOnly ? "const " : "") + CAbiType(reference->elem) + ">";
    if (type && type->kind == MType::Func)
    {
        Memory::String result = "magel_glue::detail::CAbiCallback<" + CAbiType(type->elem) + "(";
        for (std::size_t i = 0; i < type->paramTypes.size(); ++i)
        {
            if (i) result += ",";
            result += CAbiType(type->paramTypes[i]);
        }
        return result + ")>";
    }
    const auto underlying = CAbiUnderlying(type);
    if (underlying && underlying->kind == MType::Array)
        return "magel_glue::detail::CAbiArray<" + CAbiType(underlying->elem) + ">";
    return CAbiType(type);
}

/**
 * @brief 生成外部 ABI 声明对应的宿主胶水源码。
 * @retval 生成或读取的结果文本。
 */
Memory::String AbiGlueSource()
{
    Memory::Map<Memory::String, Memory::Vector<const MFunctionDecl *>> libraries;
    for (const MFunctionDecl &function : g_functionDecls)
        if (!function.abiLibrary.empty() && !function.superseded)
            libraries[function.abiLibrary].push_back(&function);
    if (libraries.empty()) return {};
    Memory::String definitions;
    Memory::Set<Memory::String> emitted;
    Memory::Function<void(const std::shared_ptr<MType> &)> emitType;
    emitType = [&](const std::shared_ptr<MType> &type) {
        if (!type) return;
        if (const auto reference = CAbiOptionalReference(type))
        {
            emitType(reference->elem);
            return;
        }
        if (type->kind == MType::Ptr || type->kind == MType::Host || type->kind == MType::Array)
        {
            emitType(type->elem);
            return;
        }
        if (type->kind != MType::Struct || !emitted.insert(type->name).second) return;
        const auto &definition = g_structs.at(type->name);
        for (const auto &field : definition.fields) emitType(field.type);
        const auto name = CAbiStructName(type->name);
        if (definition.abiPacked) definitions += "#pragma pack(push, 1)\n";
        definitions += "struct " + name + " {\n";
        for (std::size_t i = 0; i < definition.fields.size(); ++i)
            definitions += "    " + CAbiType(definition.fields[i].type) + " f" + Memory::toString(i) + ";\n";
        definitions += "};\n";
        if (definition.abiPacked) definitions += "#pragma pack(pop)\n";
        definitions += "namespace magel_glue { namespace detail {\n";
        definitions += "template <> struct CAbiStorage<" + name + "> {\nstatic constexpr std::size_t slots = " + Memory::toString(definition.slotCount) + ";\n";
        definitions += "static " + name + " read(Context &c, std::int64_t a) { " + name + " v{};\n";
        for (std::size_t i = 0; i < definition.fields.size(); ++i)
        {
            const auto &field = definition.fields[i];
            definitions += "v.f" + Memory::toString(i) + " = CAbiStorage<" + CAbiType(field.type) + ">::read(c, a + " + Memory::toString(field.slot * 8) + ");\n";
        }
        definitions += "return v; }\nstatic void write(Context &c, std::int64_t a, const " + name + " &v) {\n";
        for (std::size_t i = 0; i < definition.fields.size(); ++i)
        {
            const auto &field = definition.fields[i];
            definitions += "CAbiStorage<" + CAbiType(field.type) + ">::write(c, a + " + Memory::toString(field.slot * 8) + ", v.f" + Memory::toString(i) + ");\n";
        }
        definitions += "} };\n}\ntemplate <> struct Codec<" + name + "> {\n";
        definitions += "static " + name + " decode(Context &c, const Value &v) { return detail::CAbiStorage<" + name + ">::read(c, v.asInt()); }\n";
        definitions += "static Value encode(Context &c, const " + name + " &v) { auto a = c.makeTemporary(" + Memory::toString(definition.slotCount * 8) + "); if (a) detail::CAbiStorage<" + name + ">::write(c, a, v); return Value::fromInt(a); }\n};\n}\n";
    };
    for (const auto &library : libraries)
        for (const auto *function : library.second)
        {
            emitType(function->signature.result);
            for (const auto &param : function->signature.params) emitType(param.type);
        }
    Memory::String source = "bool bindMagelAbi(magel_glue::Registry &registry, Memory::String *error) {\n";
    std::size_t index = 0;
    for (const auto &library : libraries)
    {
        const Memory::String local = "abi_library_" + Memory::toString(index++);
        source += "    magel_glue::DynamicLibrary " + local + ";\n";
        source += "    if (!" + local + ".open(" + CxxStringLiteral(library.first) + ", error)) return false;\n";
        for (const MFunctionDecl *function : library.second)
        {
            Memory::String signature = CAbiBindingType(function->signature.result) + "(";
            for (std::size_t arg = 0; arg < function->signature.params.size(); ++arg)
            {
                if (arg != 0u) signature += ", ";
                signature += CAbiBindingType(function->signature.params[arg].type);
            }
            signature += ")";
            source += "    if (!" + local + ".bind<" + signature + ">(registry, " + CxxStringLiteral(function->label) + ", " + CxxStringLiteral(function->abiSymbol) + ", error, " + (function->abiResultCountIndex < 0 ? "static_cast<std::size_t>(-1)" : Memory::toString(function->abiResultCountIndex)) + ", " + CxxStringLiteral(function->abiRelease) + ")) return false;\n";
        }
    }
    source += "    return true;\n}\n";
    return definitions + source;
}

// Keep the existing small scalar interpreter path usable without a C++ compiler.
/**
 * @brief 为简单标量 ABI 绑定无需 C++ 编译器的解释执行实现。
 * @param library 外部 ABI 动态库的路径或句柄。
 * @param registry 接收函数绑定的宿主注册表。
 * @param function 函数定义或函数指针。
 * @param index 待访问的元素索引。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
template <typename Result, typename... Args>
bool BindRuntimeAbiScalar(const magel_glue::DynamicLibrary &library, magel_glue::Registry &registry,
                          const MFunctionDecl &function, std::size_t index, Memory::String *error)
{
    if (index == function.signature.params.size())
        return library.bind<Result(Args...)>(registry, function.label, function.abiSymbol, error);
    if constexpr (sizeof...(Args) < 4)
    {
        const auto type = CAbiType(function.signature.params[index].type);
        if (type == "std::int32_t") return BindRuntimeAbiScalar<Result, Args..., std::int32_t>(library, registry, function, index + 1, error);
        if (type == "double") return BindRuntimeAbiScalar<Result, Args..., double>(library, registry, function, index + 1, error);
        if (type == "const char *") return BindRuntimeAbiScalar<Result, Args..., const char *>(library, registry, function, index + 1, error);
    }
    return false;
}

struct RuntimeAbiType
{
    std::shared_ptr<MType> type;
    bool optionalReference = false;
    std::size_t id = 0, bytes = 0, slots = 1;
    std::shared_ptr<RuntimeAbiType> element;
    std::shared_ptr<RuntimeAbiType> callbackResult;
    Memory::Vector<std::shared_ptr<RuntimeAbiType>> callbackArgs;
    Memory::Vector<std::shared_ptr<RuntimeAbiType>> fields;
    Memory::Vector<std::size_t> offsets, vmOffsets;
};
struct RuntimeAbiBlock
{
    Memory::Vector<std::max_align_t> storage;
    /**
     * @brief 保存运行时 ABI 调用所用的内存块。
     * @param bytes 内存区域或数据的字节数。
     */
    explicit RuntimeAbiBlock(std::size_t bytes = 0) : storage((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t)) {}
    /**
     * @brief 添加数据区条目及其类型信息。
     * @retval 对应对象的指针。
     */
    char *data() { return reinterpret_cast<char *>(storage.data()); }
    /**
     * @brief 添加数据区条目及其类型信息。
     * @retval 对应对象的指针。
     */
    const char *data() const { return reinterpret_cast<const char *>(storage.data()); }
};

/**
 * @brief 创建运行时 ABI 标量的类型描述。
 * @param type 类型描述。
 * @param value 待转换、保存或参与计算的值。
 * @param out 接收处理结果的输出对象。
 */
void RuntimeAbiScalar(const MType &type, const magel_glue::Value &value, char *out)
{
    if (type.kind == MType::Double)
    {
        if (type.bits == 32)
        {
            const float v = static_cast<float>(value.asFloat());
            std::memcpy(out, &v, 4);
        }
        else
        {
            const double v = value.asFloat();
            std::memcpy(out, &v, 8);
        }
    }
    else
    {
        const auto raw = static_cast<std::uint64_t>(type.kind == MType::Bool ? value.asInt() != 0 : value.asInt());
        const auto bits = type.kind == MType::Bool || type.kind == MType::Char ? 8 : type.bits;
        if (bits == 8)
        {
            const auto v = static_cast<std::uint8_t>(raw);
            std::memcpy(out, &v, 1);
        }
        else if (bits == 16)
        {
            const auto v = static_cast<std::uint16_t>(raw);
            std::memcpy(out, &v, 2);
        }
        else if (bits == 32)
        {
            const auto v = static_cast<std::uint32_t>(raw);
            std::memcpy(out, &v, 4);
        }
        else
            std::memcpy(out, &raw, 8);
    }
}
/**
 * @brief 将运行时值转换为 ABI 标量。
 * @param type 类型描述。
 * @param input 程序输入或待解码的字节流。
 * @retval 对应对象的指针。
 */
magel_glue::Value RuntimeAbiScalarValue(const MType &type, const char *input)
{
    using Value = magel_glue::Value;
    if (type.kind == MType::Double)
    {
        if (type.bits == 32)
        {
            float v;
            std::memcpy(&v, input, 4);
            return Value::fromFloat(v);
        }
        double v;
        std::memcpy(&v, input, 8);
        return Value::fromFloat(v);
    }
    const int bits = type.kind == MType::Bool || type.kind == MType::Char ? 8 : type.bits;
    std::uint64_t v = 0;
    if (bits == 8)
    {
        std::uint8_t x;
        std::memcpy(&x, input, 1);
        v = x;
    }
    else if (bits == 16)
    {
        std::uint16_t x;
        std::memcpy(&x, input, 2);
        v = x;
    }
    else if (bits == 32)
    {
        std::uint32_t x;
        std::memcpy(&x, input, 4);
        v = x;
    }
    else
        std::memcpy(&v, input, 8);
    if (type.kind == MType::Int && !type.isUnsigned && bits < 64 && (v & (std::uint64_t(1) << (bits - 1))))
        v |= ~((std::uint64_t(1) << bits) - 1);
    return Value::fromInt(static_cast<std::int64_t>(v));
}
/**
 * @brief 保存一次同步 C 回调的 VM 函数值和签名。
 */
struct RuntimeAbiCallback
{
    magel_glue::Context *context;
    std::int64_t function;
    std::shared_ptr<RuntimeAbiType> type;
    std::exception_ptr error;
};
/**
 * @brief 从 C 跳板进入当前 VM，并将失败延迟到原生函数返回后处理。
 * @param user 同步调用持有的状态。
 * @param result C 返回值存储。
 * @param arguments C 标量参数地址列表。
 */
void DispatchRuntimeAbiCallback(void *user, void *result, void **arguments) noexcept
{
    auto &state = *static_cast<RuntimeAbiCallback *>(user);
    if (state.error) return;
    try
    {
        Memory::Vector<magel_glue::Value> values;
        for (std::size_t i = 0; i < state.type->callbackArgs.size(); ++i)
            values.push_back(RuntimeAbiScalarValue(*state.type->callbackArgs[i]->type, static_cast<const char *>(arguments[i])));
        magel_glue::Value value;
        if (!state.context->invoke(state.function, values, value)) throw std::runtime_error("ABI callback invocation failed");
        if (state.type->callbackResult->type->kind != MType::Void)
            RuntimeAbiScalar(*state.type->callbackResult->type, value, static_cast<char *>(result));
    } catch (...) { state.error = std::current_exception(); }
}

/**
 * @brief 按 ABI 类型读取指定内存位置。
 * @param type 类型描述。
 * @param context 当前处理或执行上下文。
 * @param address 运行时内存地址。
 * @param out 接收处理结果的输出对象。
 */
void RuntimeAbiRead(const RuntimeAbiType &type, magel_glue::Context &context, std::int64_t address, char *out)
{
    if (type.type->kind == MType::Struct)
    {
        for (std::size_t i = 0; i < type.fields.size(); ++i)
            RuntimeAbiRead(*type.fields[i], context, address + type.vmOffsets[i], out + type.offsets[i]);
    }
    else
    {
        std::int64_t raw;
        if (!context.readBytes(address, reinterpret_cast<char *>(&raw), 8))
            throw std::runtime_error("ABI read outside VM storage");
        magel_glue::Value value = magel_glue::Value::fromInt(raw);
        if (type.type->kind == MType::Double)
        {
            double v;
            std::memcpy(&v, &raw, 8);
            value = magel_glue::Value::fromFloat(v);
        }
        RuntimeAbiScalar(*type.type, value, out);
    }
}
/**
 * @brief 按 ABI 类型写入指定内存位置。
 * @param type 类型描述。
 * @param context 当前处理或执行上下文。
 * @param address 运行时内存地址。
 * @param input 程序输入或待解码的字节流。
 */
void RuntimeAbiWrite(const RuntimeAbiType &type, magel_glue::Context &context, std::int64_t address, const char *input)
{
    if (type.type->kind == MType::Struct)
    {
        for (std::size_t i = 0; i < type.fields.size(); ++i)
            RuntimeAbiWrite(*type.fields[i], context, address + type.vmOffsets[i], input + type.offsets[i]);
    }
    else
    {
        const auto value = RuntimeAbiScalarValue(*type.type, input);
        std::int64_t raw = value.asInt();
        if (type.type->kind == MType::Double)
        {
            const double v = value.asFloat();
            std::memcpy(&raw, &v, 8);
        }
        if (!context.writeBytes(address, reinterpret_cast<const char *>(&raw), 8))
            throw std::runtime_error("ABI write outside VM storage");
    }
}

struct RuntimeAbiBridge
{
    std::filesystem::path directory;
    magel_glue::DynamicLibrary library;
    /**
     * @brief 释放运行时 ABI 桥接持有的动态资源。
     */
    ~RuntimeAbiBridge()
    {
        library.close();
        std::error_code ec;
        if (!directory.empty()) std::filesystem::remove_all(directory, ec);
    }
};

/**
 * @brief 编译运行时 ABI 所需的宿主接口。
 * @param arguments 按调用顺序保存的实参列表。
 * @param log 接收诊断日志的输出流。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CompileRuntimeAbi(const Memory::Vector<Memory::String> &arguments, const std::filesystem::path &log)
{
#if defined(_WIN32)
    auto quote = [](const Memory::String &value) {
        Memory::String out = "\"";
        std::size_t slashes = 0;
        for (char ch : value)
        {
            if (ch == '\\')
            {
                ++slashes;
                continue;
            }
            out.append(slashes * (ch == '"' ? 2 : 1), '\\');
            slashes = 0;
            if (ch == '"') out += '\\';
            out += ch;
        }
        out.append(slashes * 2, '\\');
        return out + '"';
    };
    Memory::String command;
    for (const auto &argument : arguments)
    {
        if (!command.empty()) command += ' ';
        command += quote(argument);
    }
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, command.c_str(), -1, nullptr, 0);
    if (!count) return false;
    Memory::Vector<wchar_t> wide(count);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, command.c_str(), -1, wide.data(), count);
    SECURITY_ATTRIBUTES security{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) return false;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = startup.hStdError = output;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const bool started = CreateProcessW(nullptr, wide.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != 0;
    CloseHandle(output);
    if (!started) return false;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code == 0;
#else
    const pid_t child = fork();
    if (child < 0) return false;
    if (!child)
    {
        FILE *output = std::fopen(log.c_str(), "wb");
        if (!output) _exit(127);
        dup2(fileno(output), STDOUT_FILENO);
        dup2(fileno(output), STDERR_FILENO);
        Memory::Vector<char *> argv;
        for (const auto &argument : arguments) argv.push_back(const_cast<char *>(argument.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0)
    {
        if (errno != EINTR) return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

/**
 * @brief 计算调用桥缓存文件名；加载时仍逐字节比对完整签名，不把散列作为兼容依据。
 * @param identity 包含桥源码、目标与编译器信息的完整身份。
 * @return 用于文件名的十六进制散列。
 */
Memory::String AbiCacheKey(const Memory::String &identity)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char byte : identity)
    {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    char buffer[17];
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hash));
    return buffer;
}

/**
 * @brief 将运行时 ABI 实现绑定到原生注册表。
 * @param registry 接收函数绑定的宿主注册表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BindRuntimeAbi(magel_glue::Registry *registry)
{
    using Type = std::shared_ptr<RuntimeAbiType>;
    Memory::Vector<const MFunctionDecl *> functions;
    for (const auto &function : g_functionDecls)
        if (!function.abiLibrary.empty() && !function.superseded) functions.push_back(&function);
    if (functions.empty()) return true;
    const bool scalarOnly = !g_abiPrepareOnly && g_abiWriteOption.empty() && g_abiBridgeOption.empty() && !std::getenv("MAGEL_ABI_WRITE_BRIDGE") && !std::getenv("MAGEL_ABI_BRIDGE") && std::all_of(functions.begin(), functions.end(), [](const MFunctionDecl *f) {
        if (!f->abiRelease.empty()) return false;
        const auto result = CAbiType(f->signature.result);
        if (result != "void" && result != "std::int32_t" && result != "double" && result != "const char *") return false;
        if (f->signature.params.size() > 4) return false;
        return std::all_of(f->signature.params.begin(), f->signature.params.end(), [](const MParam &p) {
            const auto kind = CAbiUnderlying(p.type)->kind;
            const auto type = CAbiType(p.type);
            return kind != MType::Ptr && kind != MType::Array && (type == "std::int32_t" || type == "double" || type == "const char *");
        });
    });
    if (scalarOnly)
    {
        for (const auto *function : functions)
        {
            magel_glue::DynamicLibrary native;
            Memory::String error;
            if (!native.open(function->abiLibrary, &error)) return Fail(error);
            const auto result = CAbiType(function->signature.result);
            bool bound = false;
            if (result == "void")
                bound = BindRuntimeAbiScalar<void>(native, *registry, *function, 0, &error);
            else if (result == "std::int32_t")
                bound = BindRuntimeAbiScalar<std::int32_t>(native, *registry, *function, 0, &error);
            else if (result == "double")
                bound = BindRuntimeAbiScalar<double>(native, *registry, *function, 0, &error);
            else
                bound = BindRuntimeAbiScalar<const char *>(native, *registry, *function, 0, &error);
            if (!bound) return Fail(error);
        }
        return true;
    }
    Memory::Vector<Type> types;
    Memory::Map<const MType *, Type> lookup;
    Memory::String declarations;
    Memory::Set<Memory::String> declared;
    Memory::Function<Type(std::shared_ptr<MType>)> build = [&](std::shared_ptr<MType> input) -> Type {
        input = CAbiUnderlying(input);
        const auto key = input.get();
        if (lookup.count(key)) return lookup.at(key);
        const auto optional = CAbiOptionalReference(input);
        if (optional) input = optional;
        auto type = Memory::makeShared<RuntimeAbiType>();
        type->type = input;
        type->id = types.size();
        type->optionalReference = optional != nullptr;
        types.push_back(type);
        lookup[key] = type;
        if (input->kind == MType::Ptr || input->kind == MType::Array) type->element = build(input->elem);
        if (input->kind == MType::Func)
        {
            type->callbackResult = build(input->elem);
            Memory::String alias = "using " + CAbiType(input) + " = " + CAbiType(input->elem) + " (*)(";
            for (std::size_t i = 0; i < input->paramTypes.size(); ++i)
            {
                if (i) alias += ",";
                alias += CAbiType(input->paramTypes[i]);
                type->callbackArgs.push_back(build(input->paramTypes[i]));
            }
            declarations += alias + ");\n";
        }
        if (input->kind == MType::Struct)
        {
            const auto &definition = g_structs.at(input->name);
            type->slots = definition.slotCount;
            for (const auto &field : definition.fields)
            {
                type->fields.push_back(build(field.type));
                type->vmOffsets.push_back(field.slot * 8);
            }
            if (declared.insert(input->name).second)
            {
                if (definition.abiPacked) declarations += "#pragma pack(push, 1)\n";
                declarations += "struct " + CAbiStructName(input->name) + " {\n";
                for (std::size_t i = 0; i < definition.fields.size(); ++i)
                    declarations += CAbiType(definition.fields[i].type) + " f" + Memory::toString(i) + ";\n";
                declarations += "};\n";
                if (definition.abiPacked) declarations += "#pragma pack(pop)\n";
            }
        }
        return type;
    };
    Memory::Vector<Type> results;
    Memory::Vector<Memory::Vector<Type>> parameters;
    for (const auto *function : functions)
    {
        results.push_back(build(function->signature.result));
        parameters.emplace_back();
        for (const auto &parameter : function->signature.params) parameters.back().push_back(build(parameter.type));
    }
    Memory::String source = "#include <cstddef>\n#include <cstdint>\n#include <cstring>\n#if defined(_WIN32)\n#define API __declspec(dllexport)\n#else\n#define API\n#endif\n" + declarations;
    Memory::Vector<Memory::Vector<std::size_t>> callbackSlots;
    std::size_t callbackCount = 0;
    for (const auto &functionParameters : parameters)
    {
        callbackSlots.emplace_back(functionParameters.size(), static_cast<std::size_t>(-1));
        for (std::size_t i = 0; i < functionParameters.size(); ++i)
            if (functionParameters[i]->type->kind == MType::Func) callbackSlots.back()[i] = callbackCount++;
    }
    source += "struct McAbiCallbackSlot { void (*dispatch)(void*,void*,void**); void *user; };\nstatic thread_local McAbiCallbackSlot callbacks[" + Memory::toString(std::max<std::size_t>(1, callbackCount)) + "]{};\n";
    for (std::size_t f = 0; f < parameters.size(); ++f)
        for (std::size_t i = 0; i < parameters[f].size(); ++i)
        {
            const auto &type = parameters[f][i];
            if (type->type->kind != MType::Func) continue;
            const auto slot = callbackSlots[f][i];
            const auto resultType = CAbiType(type->callbackResult->type);
            source += "static " + resultType + " callback_" + Memory::toString(slot) + "(";
            for (std::size_t a = 0; a < type->callbackArgs.size(); ++a)
            {
                if (a) source += ",";
                source += CAbiType(type->callbackArgs[a]->type) + " a" + Memory::toString(a);
            }
            source += ") {\n";
            if (resultType != "void") source += resultType + " result{};\n";
            source += "void *args[" + Memory::toString(std::max<std::size_t>(1, type->callbackArgs.size())) + "]={";
            for (std::size_t a = 0; a < type->callbackArgs.size(); ++a)
            {
                if (a) source += ",";
                source += "&a" + Memory::toString(a);
            }
            source += "}; const auto slot = callbacks[" + Memory::toString(slot) + "]; if (slot.dispatch) slot.dispatch(slot.user," + Memory::String(resultType == "void" ? "nullptr" : "&result") + ",args);\n";
            if (resultType != "void") source += "return result;\n";
            source += "}\n";
        }
    source += "extern \"C\" API void *magel_abi_callback_set(std::size_t id, void (*dispatch)(void*,void*,void**), void *user) { if(id >= " + Memory::toString(callbackCount) + ") return nullptr; auto old=callbacks[id].user; callbacks[id]={user ? dispatch : nullptr,user}; return old; }\n";
    source += "extern \"C\" API void *magel_abi_callback_address(std::size_t id) { switch(id) {\n";
    for (std::size_t i = 0; i < callbackCount; ++i) source += "case " + Memory::toString(i) + ": return reinterpret_cast<void*>(&callback_" + Memory::toString(i) + ");\n";
    source += "default: return nullptr; } }\n";
    source += "extern \"C\" API std::size_t magel_abi_layout(std::size_t type, std::size_t field) { switch(type) {\n";
    for (const auto &type : types)
    {
        source += "case " + Memory::toString(type->id) + ":\n";
        if (type->type->kind == MType::Void)
        {
            source += "return 0;\n";
            continue;
        }
        source += "if (!field) return sizeof(" + CAbiType(type->type) + ");\n";
        source += "if (field == static_cast<std::size_t>(-1)) return alignof(" + CAbiType(type->type) + ");\n";
        for (std::size_t i = 0; i < type->fields.size(); ++i)
            source += "if (field == " + Memory::toString(i + 1) + ") return offsetof(" + CAbiType(type->type) + ", f" + Memory::toString(i) + ");\n";
        source += "return 0;\n";
    }
    source += "default: return sizeof(void*); } }\nextern \"C\" API void magel_abi_call(std::size_t id, void *function, void *result, void **args) { switch(id) {\n";
    for (std::size_t i = 0; i < functions.size(); ++i)
    {
        source += "case " + Memory::toString(i) + ": { using Fn = " + CAbiType(results[i]->type) + " (*)(";
        for (std::size_t j = 0; j < parameters[i].size(); ++j)
        {
            if (j) source += ",";
            source += CAbiType(parameters[i][j]->type);
        }
        source += ");\n";
        for (std::size_t j = 0; j < parameters[i].size(); ++j)
        {
            const auto name = "a" + Memory::toString(j);
            source += CAbiType(parameters[i][j]->type) + " " + name + "{}; std::memcpy(&" + name + ", args[" + Memory::toString(j) + "], sizeof(" + name + "));\n";
        }
        if (results[i]->type->kind != MType::Void) source += "auto returned = ";
        source += "reinterpret_cast<Fn>(function)(";
        for (std::size_t j = 0; j < parameters[i].size(); ++j)
        {
            if (j) source += ",";
            source += "a" + Memory::toString(j);
        }
        source += ");\n";
        if (results[i]->type->kind != MType::Void) source += "std::memcpy(result, &returned, sizeof(returned));\n";
        source += "break; }\n";
    }
    source += "} }\n";
    const Memory::String identity = Memory::String("magel-abi-v1.0.0|") + Memory::toString(sizeof(void *)) + "|" + source;
    source += "extern \"C\" API const char *magel_abi_identity() { return " + CxxStringLiteral(identity) + "; }\n";
    auto bridge = Memory::makeShared<RuntimeAbiBridge>();
    Memory::String error;
    std::filesystem::path cached;
    const char *explicitBridge = g_abiBridgeOption.empty() ? std::getenv("MAGEL_ABI_BRIDGE") : g_abiBridgeOption.c_str();
    const char *cacheRoot = g_abiCacheOption.empty() ? std::getenv("MAGEL_ABI_CACHE_DIR") : g_abiCacheOption.c_str();
    const char *noCache = std::getenv("MAGEL_ABI_NO_CACHE");
    if (explicitBridge && *explicitBridge)
        cached = std::filesystem::u8path(explicitBridge);
    else if (!g_abiNoCache && !(noCache && Memory::String(noCache) == "1"))
    {
        const char *base = std::getenv("LOCALAPPDATA");
        if (!base || !*base) base = std::getenv("XDG_CACHE_HOME");
        if (!base || !*base) base = std::getenv("HOME");
        if (cacheRoot && *cacheRoot)
            cached = std::filesystem::u8path(cacheRoot);
        else if (base && *base)
            cached = std::filesystem::u8path(base) / ".magel" / "abi-cache-v1";
        if (!cached.empty()) cached /= AbiCacheKey(identity) + ".dll";
    }
    bool loaded = false;
    if (!cached.empty() && std::filesystem::is_regular_file(cached))
    {
        if (bridge->library.open(cached.u8string(), &error))
        {
            const auto describe = reinterpret_cast<const char *(*)()>(bridge->library.resolve("magel_abi_identity", &error));
            loaded = describe && describe() && identity == describe();
            if (!loaded) bridge->library.close();
        }
    }
    if (!loaded && !cached.empty() && !(explicitBridge && *explicitBridge))
    {
        std::error_code stale;
        std::filesystem::remove(cached, stale);
    }
    if (explicitBridge && *explicitBridge && !loaded) return Fail("预编译 ABI 调用桥不可用或完整签名不匹配：" + Memory::String(explicitBridge));
    if (!loaded)
    {
        std::error_code ec;
        const auto root = std::filesystem::temp_directory_path(ec);
        if (ec) return Fail("abi 调用桥无法取得临时目录");
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const auto directory = root / ("magel-abi-" + Memory::toString(stamp) + "-" + Memory::toString(attempt));
            if (std::filesystem::create_directory(directory, ec))
            {
                bridge->directory = directory;
                break;
            }
        }
        if (bridge->directory.empty()) return Fail("abi 调用桥无法创建临时目录");
        const auto cpp = bridge->directory / "bridge.cpp";
        const auto output = bridge->directory / "bridge.dll";
        const auto log = bridge->directory / "compiler.log";
        {
            std::ofstream file(cpp, std::ios::binary);
            file << source;
            if (!file) return Fail("abi 调用桥源码写入失败");
        }
        const char *configured = std::getenv("MAGEL_ABI_CXX");
#if defined(_MSC_VER)
        const Memory::String compiler = configured && *configured ? configured : "cl.exe";
#else
        const Memory::String compiler = configured && *configured ? configured : "g++";
#endif
        const auto compilerName = std::filesystem::path(compiler).filename().string();
        Memory::Vector<Memory::String> arguments;
        if (compilerName == "cl" || compilerName == "cl.exe")
            arguments = { compiler, "/nologo", "/std:c++17", "/O2", "/LD", cpp.u8string(), "/Fo" + (bridge->directory / "bridge.obj").u8string(), "/link", "/OUT:" + output.u8string() };
        else
            arguments = { compiler, "-std=c++17", "-O2", "-shared", "-fPIC", cpp.u8string(), "-o", output.u8string() };
        if (!CompileRuntimeAbi(arguments, log))
        {
            std::ifstream file(log, std::ios::binary);
            Memory::String diagnostics((std::istreambuf_iterator<char>(file)), {});
            return Fail("abi 调用桥编译失败；需要同位数 C++ 编译器，可用 MAGEL_ABI_CXX 指定：" + diagnostics);
        }
        if (!bridge->library.open(output.u8string(), &error)) return Fail(error);
        if (!cached.empty())
        {
            std::error_code cacheError;
            std::filesystem::create_directories(cached.parent_path(), cacheError);
            // 独立临时产物复制到最终槽位；并发已有缓存时不覆盖正在使用的库。
            const auto staging = cached.parent_path() / (cached.filename().string() + "." + bridge->directory.filename().string() + ".building");
            std::filesystem::copy_file(output, staging, std::filesystem::copy_options::overwrite_existing, cacheError);
            if (!cacheError) std::filesystem::rename(staging, cached, cacheError);
            std::error_code cleanup;
            std::filesystem::remove(staging, cleanup);
        }
    }
    const char *writeBridge = g_abiWriteOption.empty() ? std::getenv("MAGEL_ABI_WRITE_BRIDGE") : g_abiWriteOption.c_str();
    if (writeBridge && *writeBridge)
    {
        const auto origin = bridge->directory.empty() ? cached : bridge->directory / "bridge.dll";
        std::error_code copyError;
        const auto destination = std::filesystem::u8path(writeBridge);
        std::filesystem::copy_file(origin, destination, std::filesystem::copy_options::overwrite_existing, copyError);
        if (copyError) return Fail("无法保存预编译 ABI 调用桥：" + Memory::String(copyError.message()));
    }
    using Layout = std::size_t (*)(std::size_t, std::size_t);
    using Call = void (*)(std::size_t, void *, void *, void **);
    const auto layout = reinterpret_cast<Layout>(bridge->library.resolve("magel_abi_layout", &error));
    const auto call = reinterpret_cast<Call>(bridge->library.resolve("magel_abi_call", &error));
    using SetCallback = void *(*)(std::size_t, void (*)(void *, void *, void **), void *);
    using CallbackAddress = void *(*)(std::size_t);
    const auto setCallback = reinterpret_cast<SetCallback>(bridge->library.resolve("magel_abi_callback_set", &error));
    const auto callbackAddress = reinterpret_cast<CallbackAddress>(bridge->library.resolve("magel_abi_callback_address", &error));
    if (!layout || !call || !setCallback || !callbackAddress) return Fail(error);
    if (layout(static_cast<std::size_t>(-1), 0) != sizeof(void *)) return Fail("abi 调用桥与 VM 位数不一致");
    for (const auto &type : types)
    {
        type->bytes = layout(type->id, 0);
        for (std::size_t i = 0; i < type->fields.size(); ++i) type->offsets.push_back(layout(type->id, i + 1));
    }
    for (const auto &type : types)
    {
        if (type->type->kind == MType::Void) continue;
        if (!type->bytes || type->bytes > magel_glue::detail::CAbiMaxBufferBytes)
            return Fail("ABI 类型布局大小无效");
        const auto alignment = layout(type->id, static_cast<std::size_t>(-1));
        if (!alignment || (alignment & (alignment - 1)) || type->bytes % alignment)
            return Fail("ABI 类型布局对齐无效");
        for (std::size_t i = 0; i < type->fields.size(); ++i)
            if (type->offsets[i] > type->bytes || type->fields[i]->bytes > type->bytes - type->offsets[i])
                return Fail("ABI 结构体字段超出布局边界");
    }
    if (g_abiPrepareOnly) return true;
    for (std::size_t index = 0; index < functions.size(); ++index)
    {
        const auto &function = *functions[index];
        magel_glue::DynamicLibrary native;
        if (!native.open(function.abiLibrary, &error)) return Fail(error);
        void *symbol = native.resolve(function.abiSymbol, &error);
        if (!symbol) return Fail(error);
        void *release = function.abiRelease.empty() ? nullptr : native.resolve(function.abiRelease, &error);
        if (!function.abiRelease.empty() && !release) return Fail(error);
        if (!registry->bind(function.label, parameters[index].size(), [bridge, native, symbol, release, call, setCallback, callbackAddress, slots = callbackSlots[index], index, args = parameters[index], result = results[index], countIndex = function.abiResultCountIndex](magel_glue::Context &context, const Memory::Vector<magel_glue::Value> &values) {
                using Value = magel_glue::Value;
                const auto copiedCount = countIndex < 0 ? 1u : magel_glue::detail::cAbiCount(values.at(countIndex).asInt(), result->element->slots * 8);
                struct Payload { Type type; std::int64_t address; std::size_t count; bool writable; RuntimeAbiBlock bytes; };
                Memory::Map<std::tuple<Memory::String, std::int64_t, std::size_t>, std::shared_ptr<Payload>> payloads;
                Memory::Vector<std::unique_ptr<RuntimeAbiCallback>> callbacks;
                struct CallbackGuard
                {
                    SetCallback setter;
                    std::size_t slot;
                    void *previous;
                    /**
                     * @brief 恢复外层同步调用，保证重入和异常路径不留下悬空状态。
                     */
                    ~CallbackGuard() { setter(slot, DispatchRuntimeAbiCallback, previous); }
                };
                Memory::Vector<std::unique_ptr<CallbackGuard>> callbackGuards;
                Memory::Vector<RuntimeAbiBlock> blocks; blocks.reserve(args.size());
                Memory::Vector<Memory::String> strings(args.size());
                Memory::Vector<void *> pointers;
                for (std::size_t i = 0; i < args.size(); ++i) {
                    const auto &type = args[i]; blocks.emplace_back(type->bytes); char *data = blocks.back().data();
                    if (type->type->kind == MType::Func) {
                        if (!values[i].asInt()) throw std::runtime_error("ABI callback is null");
                        auto state = std::make_unique<RuntimeAbiCallback>(RuntimeAbiCallback{ &context, values[i].asInt(), type, {} });
                        const auto previous = setCallback(slots[i], DispatchRuntimeAbiCallback, state.get());
                        callbackGuards.emplace_back(new CallbackGuard{ setCallback, slots[i], previous });
                        callbacks.push_back(std::move(state));
                        const auto address = callbackAddress(slots[i]); std::memcpy(data, &address, sizeof(address));
                    } else if (type->type->kind == MType::String) {
                        strings[i] = magel_glue::Codec<Memory::String>::decode(context, values[i]);
                        if (strings[i].find(static_cast<char>(0)) != Memory::String::npos) throw std::runtime_error("ABI C string contains NUL; use a byte buffer");
                        const char *text = strings[i].c_str(); std::memcpy(data, &text, sizeof(text));
                    } else if (type->element) {
                        auto address = type->optionalReference
                            ? magel_glue::detail::decodeOptionalReference(context, values[i]).value_or(0)
                            : values[i].asInt();
                        std::size_t count = 1;
                        if (!address && !type->optionalReference && type->type->kind == MType::Ptr) throw std::runtime_error("ABI reference argument is null");
                        if (type->type->kind == MType::Array) {
                            if (!address) throw std::runtime_error("ABI array argument is null");
                            count = magel_glue::detail::cAbiCount(magel_glue::detail::CAbiStorage<std::int64_t>::read(context, address - 8), type->element->slots * 8);
                        }
                        void *pointer = nullptr;
                        if (address && count) {
                            auto &payload = payloads[{CAbiType(type->element->type), address, count}];
                            if (!payload) {
                                payload = Memory::makeShared<Payload>(Payload{type->element, address, count, false, RuntimeAbiBlock(count * type->element->bytes)});
                                for (std::size_t j = 0; j < count; ++j)
                                    RuntimeAbiRead(*type->element, context, address + j * type->element->slots * 8, payload->bytes.data() + j * type->element->bytes);
                            }
                            payload->writable = payload->writable || type->type->kind == MType::Array || !type->type->readOnly;
                            pointer = payload->bytes.data();
                        }
                        std::memcpy(data, &pointer, sizeof(pointer));
                    } else if (type->type->kind == MType::Struct) RuntimeAbiRead(*type, context, values[i].asInt(), data);
                    else RuntimeAbiScalar(*type->type, values[i], data);
                    pointers.push_back(data);
                }
                RuntimeAbiBlock returned(result->bytes);
                call(index, symbol, returned.data(), pointers.data());
                struct OwnedReturn
                {
                    void *pointer = nullptr;
                    void *release = nullptr;
                    /**
                     * @brief 在返回值复制完成或异常退出时执行声明的宿主释放函数。
                     */
                    ~OwnedReturn() { if (pointer && release) reinterpret_cast<void (*)(void *)>(release)(pointer); }
                } owned;
                if (release) { std::memcpy(&owned.pointer, returned.data(), sizeof(void *)); owned.release = release; }
                for (const auto &callback : callbacks)
                    if (callback->error) std::rethrow_exception(callback->error);
                Value value = Value::fromInt(0);
                if (result->element) {
                    const auto count = copiedCount;
                    const char *pointer; std::memcpy(&pointer, returned.data(), sizeof(pointer));
                    const bool array = result->type->kind == MType::Array;
                    if (!array && !pointer && !result->optionalReference) throw std::runtime_error("ABI returned null for nonnull reference");
                    if (pointer || array) {
                        if (!pointer && count) throw std::runtime_error("ABI returned null for nonempty buffer");
                        const auto address = context.makeTemporary(count * result->element->slots * 8 + (array ? 8 : 0));
                        if (address) {
                            if (array) magel_glue::detail::CAbiStorage<std::int64_t>::write(context, address, count);
                            for (std::size_t j = 0; j < count; ++j)
                                RuntimeAbiWrite(*result->element, context, address + (array ? 8 : 0) + j * result->element->slots * 8, pointer + j * result->element->bytes);
                        }
                        value = Value::fromInt(address ? address + (array ? 8 : 0) : 0);
                    }
                    if (result->optionalReference)
                        value = magel_glue::detail::encodeOptionalReference(context,
                            pointer ? std::optional<std::int64_t>(value.asInt()) : std::nullopt);
                } else if (result->type->kind == MType::Struct) {
                    const auto address = context.makeTemporary(result->slots * 8);
                    if (address) RuntimeAbiWrite(*result, context, address, returned.data()); value = Value::fromInt(address);
                } else if (result->type->kind == MType::String) {
                    const char *text; std::memcpy(&text, returned.data(), sizeof(text)); value = Value::fromInt(context.makeString(magel_glue::detail::copyCAbiString(text)));
                } else if (result->type->kind != MType::Void) value = RuntimeAbiScalarValue(*result->type, returned.data());
                for (const auto &entry : payloads) {
                    const auto &payload = *entry.second;
                    if (payload.writable) for (std::size_t j = 0; j < payload.count; ++j)
                        RuntimeAbiWrite(*payload.type, context, payload.address + j * payload.type->slots * 8, payload.bytes.data() + j * payload.type->bytes);
                }
                return value; }, &error)) return Fail(error);
    }
    return true;
}

/**
 * @brief 写出编译产生的四元式、数据与二进制产物。
 * @param prefix 生成产物文件的路径前缀。
 * @param result 接收处理结果的对象。
 * @param binaryPath 二进制产物的文件路径。
 * @param layoutPath 数据布局描述文件的路径。
 * @param parsed 已完成组装的程序产物。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool WriteArtifacts(const Memory::String &prefix, const simple_ast::CodegenResult &result,
                    Memory::String *binaryPath, Memory::String *layoutPath, stackvm::QuadFile *parsed)
{
    *binaryPath = prefix + ".data.bin";
    *layoutPath = prefix + ".layout";
    Memory::String layout;
    for (const Memory::String &line : result.exportDataLayout()) layout += line + "\n";
    /* ABI 绑定只在真有 `@abi` 库时才出文件：没有绑定时 `AbiGlueSource()` 给空串，
     * 再落一个 0 字节的 .abi.cpp 只会让人以为产物不完整。不生成是安全的——
     * magel-emit 读不到就按"没有绑定"处理；同时清掉上次的残留，
     * 免得旧绑定被当成本次的用上。 */
    const Memory::String abiGlue = AbiGlueSource();
    if (abiGlue.empty())
    {
        std::error_code removed;
        std::filesystem::remove(std::filesystem::path(prefix + ".abi.cpp"), removed);
    }
    else if (!WriteFile(prefix + ".abi.cpp", abiGlue))
        return false;
    Memory::String exportHeader;
    const auto exportSource = CExportSource(&exportHeader);
    for (const auto &part : Memory::Vector<std::pair<Memory::String, Memory::String>>{ { ".exports.cpp", exportSource }, { ".exports.h", exportHeader } })
    {
        if (!part.second.empty())
        {
            if (!WriteFile(prefix + part.first, part.second)) return false;
        }
        else
        {
            std::error_code removed;
            std::filesystem::remove(Memory::u8path(prefix + part.first), removed);
        }
    }
    *parsed = stackvm::parseQuads(result.quads, prefix + ".quad");
    const auto quadBytes = stackvm::encodeQuadBinary(*parsed);
    Memory::Vector<stackvm::LayoutRow> rows;
    if (quadBytes.empty() || !stackvm::decodeDataLayout(layout, rows)) return false;
    // 文本转储与 C 数组都是重复数据；清理旧版本留下的同前缀产物。
    for (const char *suffix : { ".data", ".data.h" })
    {
        std::error_code removed;
        std::filesystem::remove(std::filesystem::path(prefix + suffix), removed);
    }
    return WriteFile(prefix + ".quad", quadBytes) && WriteFile(*binaryPath, result.exportDataBinary()) && WriteFile(*layoutPath, stackvm::encodeDataLayout(rows));
}

/**
 * @brief 每次编译前清空降级期的状态（同一个进程里可以编译多份源文件）。
 */
void ResetState()
{
    g_diagnostics.clear();
    g_warnings.clear();
    g_borrowFacts.clear();
    g_borrowFlows.clear();
    g_borrowFlowCapture = nullptr;
    g_effectConflicts.clear();
    g_effectAliases.clear();
    g_inferenceConcreteTypes.clear();
    g_inferenceConcreteCount = 0;
    g_error.clear();
    g_errorLine = 0;
    g_errorColumn = 0;
    g_constantBindings.clear();
    g_addressTakenNames.clear();
    g_referenceStorage.clear();
    g_parameterReferenceOwners.clear();
    g_pendingParameterStorage.clear();
    g_parameterValueStorage.clear();
    for (const auto &node : g_nodes)
        if (node.kind == MNode::Un && node.s == "&" && !node.kids.empty())
        {
            const auto &operand = N(node.kids[0]);
            if (operand.kind == MNode::Var) g_addressTakenNames.insert(operand.s);
            if (operand.kind == MNode::EnumCtor) g_addressTakenNames.insert(operand.s2);
        }
    /* 捕获在函数体降级时才解析；提前为 lambda 中的变量准备稳定引用槽。 */
    std::function<void(int)> prepareCaptureStorage = [&](int id) {
        if (id < 0) return;
        const auto node = N(id);
        if (node.kind == MNode::Var) g_addressTakenNames.insert(node.s);
        if (node.kind == MNode::Block) prepareCaptureStorage(static_cast<int>(node.i));
        for (const int child : node.kids) prepareCaptureStorage(child);
    };
    for (const auto &node : g_nodes)
        if (node.kind == MNode::FuncDef && node.s.rfind("__magel_lambda_", 0) == 0)
            for (const int child : node.kids) prepareCaptureStorage(child);
    // 带值退出在作用域绑定后才选择内部变量移动，提前准备其可能使用的稳定引用槽。
    std::function<void(int)> prepareBreakStorage = [&](int id) {
        if (id < 0) return;
        const auto &node = N(id);
        if (node.kind == MNode::Var) g_addressTakenNames.insert(node.s);
        if (node.kind == MNode::Tuple)
            for (const int child : node.kids) prepareBreakStorage(child);
    };
    for (const auto &node : g_nodes)
        if (node.kind == MNode::Break && node.type2 != 1 && !node.kids.empty()) prepareBreakStorage(node.kids[0]);
    for (const auto &specification : magel_lambda::captureSpecifications)
        for (const auto &capture : specification.second) g_addressTakenNames.insert(capture.first);
    /* 迭代器入口隐式借用或转移容器，必须在局部声明降级前准备引用槽。 */
    for (const auto &node : g_nodes)
    {
        if (node.kind != MNode::Call || node.kids.empty()) continue;
        const auto &callee = N(node.kids[0]);
        if (callee.kind != MNode::Member || callee.kids.empty() || (callee.s != "iter" && callee.s != "iterMut" && callee.s != "intoIter")) continue;
        const auto &receiver = N(callee.kids[0]);
        if (receiver.kind == MNode::Var) g_addressTakenNames.insert(receiver.s);
        if (receiver.kind == MNode::EnumCtor) g_addressTakenNames.insert(receiver.s2);
    }
    g_structs.clear();
    g_tupleElements.clear();
    g_staticFields.clear();
    g_enums.clear();
    g_funcs.clear();
    g_operators.clear();
    g_conversions.clear();
    g_indexes.clear();
    g_iterators.clear();
    g_hashes.clear();
    g_mapKeyPolicies.clear();
    g_collectionConstructors.clear();
    g_structType.clear();
    g_loweringProgram = nullptr;
    g_scope.clear();
    g_globalInit.clear();
    g_usedNames.clear();
    g_owner.clear();
    g_currentModule.clear();
    g_localSerial = 0;
    /* 接口 / 类型号 / 装箱的上下文（同一个进程里编译第二份源文件时不能继承） */
    g_interfaces.clear();
    g_interfaceBases.clear();
    g_interfaceOrder.clear();
    g_usedInterfaceMethods.clear();
    g_typeIdIndex.clear();
    g_nextTypeId = 1;
    g_expectedObjGet = nullptr;
    RegisterBuiltinTypeIds(); /* 内建占 1..9、10..32 预留：`__typeTag(int)` 永远是 1 */
    g_functionIndex.clear();
    g_moduleImports.clear();
    g_inferenceFunctions.clear();
    g_functionDecls.clear();
    g_lambdaCaptures.clear();
    g_lambdaAutoReturns.clear();
    g_lambdaPrepared.clear();
    g_hostTypes.clear();
    g_hostInterfaceImpls.clear();
    g_globalNames.clear();
    /* "用到才生成"的运行时库开关也要归位（同一个进程里编译第二份源文件时不能继承） */
    g_usesNewArray = false;
    g_usesStructCopy = false;
    g_usesLanguageMove = true;  // drop / delete 的幂等析构同样需要生命周期标记。
    for (const auto &node : g_nodes)
    {
        if (node.kind != MNode::Call || node.kids.empty()) continue;
        const auto &callee = N(node.kids[0]);
        if ((callee.kind == MNode::EnumCtor && callee.s == "std::lang" && callee.s2 == "move") || (callee.kind == MNode::Var && callee.s == "move")) g_usesLanguageMove = true;
    }
    g_needsAggregateRelease.clear();
    g_needsCaptureDestructor.clear();
    g_needsAggregateCopy.clear();
    g_expectedFuncType = nullptr;
    g_expectedNewType = nullptr;
    /* 循环的跳转目标栈按降级过程自然清空；这里再兜一次底，
     * 免得上一次编译在中途跳出（报错提前返回）留下的残状态影响下一次。 */
    g_blockStack.clear();
    g_blockSerial = 0;
    g_ownedScopes.clear();
    g_ownedHome.clear();
    g_scope.push_back(Memory::Map<Memory::String, MVar>()); /* 0 号 = 全局作用域 */
    /* 运行时辅助函数的内部名字先占住，免得用户程序撞名 */
    for (const char *name : { "__magel_p", "__magel_ip", "__magel_fr", "__magel_k", "__magel_d", "__magel_code",
                              "__magel_num", "__magel_flt", "__magel_addr", "__magel_tgt", "__magel_src", "__magel_n",
                              "__magel_val", "__magel_l", "__magel_r" })
        g_usedNames.insert(name);
}

/**
 * @brief 把 `--input` 的文本按空白切成一项一项（`input(变量)` 按顺序取）。
 * @param text 输入文本；空引用 / 空串表示没有输入。
 * @return 输入项列表。
 */
Memory::Vector<Memory::String> SplitInput(const char *text)
{
    Memory::Vector<Memory::String> items;
    if (text == nullptr) return items;
    const Memory::String source(text);
    std::size_t index = 0u;
    while (index < source.size())
    {
        while ((index < source.size()) && ((source[index] == ' ') || (source[index] == '\t') || (source[index] == '\r') || (source[index] == '\n')))
        {
            ++index;
        }
        const std::size_t start = index;
        while ((index < source.size()) && (source[index] != ' ') && (source[index] != '\t') && (source[index] != '\r') && (source[index] != '\n'))
        {
            ++index;
        }
        if (index > start) items.push_back(source.substr(start, index - start));
    }
    return items;
}
