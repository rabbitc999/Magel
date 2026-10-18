/**
 * @file module_build.hpp
 * @brief 按 MAGEL 模块缓存本地 AOT 目标文件，统一链接运行时与模块。
 * 本文件由 main.cpp 在工具内部命名空间中包含，复用命令捕获及受管清单。
 */

/** @brief 计算缓存内容指纹；用于失效检查，不作为安全签名。 */
std::uint64_t moduleHash(const Memory::String &text)
{
    std::uint64_t value = 14695981039346656037ull;
    for (unsigned char byte : text) value = (value ^ byte) * 1099511628211ull;
    return value;
}

/** @brief 把内容指纹编码为文件名。 */
Memory::String moduleHex(std::uint64_t value)
{
    char text[17]; std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

/** @brief 读取文件指纹，失败返回 false。 */
bool moduleFileHash(const std::filesystem::path &path, std::uint64_t *value)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    *value = 14695981039346656037ull;
    char buffer[16384];
    while (input.read(buffer, sizeof(buffer)) || input.gcount())
        for (std::streamsize index = 0; index < input.gcount(); ++index)
            *value = (*value ^ static_cast<unsigned char>(buffer[index])) * 1099511628211ull;
    return !input.bad();
}

/** @brief 写出缓存文件；内容不变时不更新文件时间。 */
bool moduleWrite(const std::filesystem::path &path, const Memory::String &text)
{
    Memory::String old;
    if (readWholeFile(path.string(), &old) && old == text) return true;
    std::error_code statusError;
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path, statusError))) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(output);
}

/** @brief 编码稳定 JSON 字符串。 */
Memory::String moduleJson(const Memory::String &text)
{
    Memory::String result = "\"";
    for (unsigned char ch : text)
    {
        if (ch == '\\' || ch == '"') { result += '\\'; result += static_cast<char>(ch); }
        else if (ch < 32) { char escape[7]; std::snprintf(escape, sizeof(escape), "\\u%04x", ch); result += escape; }
        else result += static_cast<char>(ch);
    }
    return result + '"';
}

/** @brief 校验上次编译读取的所有头文件内容，包括工具链系统头。 */
bool moduleDependenciesCurrent(const std::filesystem::path &path)
{
    std::ifstream input(path);
    std::uint64_t expected = 0, actual = 0;
    std::string file;
    std::size_t count = 0;
    while (input >> std::ws && input.peek() != std::char_traits<char>::eof()) {
        if (!(input >> expected >> std::quoted(file)) || !moduleFileHash(file, &actual) || expected != actual) return false;
        ++count;
    }
    return !input.bad() && count != 0;
}

/** @brief 将 GCC 依赖文件转换为逐文件内容指纹，保留转义路径。 */
bool moduleSaveDependencies(const std::filesystem::path &dependencyFile, const std::filesystem::path &stamp)
{
    Memory::String text;
    if (!readWholeFile(dependencyFile.string(), &text)) return false;
    const auto colon = text.find(':');
    if (colon == Memory::String::npos) return false;
    Memory::Vector<Memory::String> files;
    Memory::String word;
    for (std::size_t index = colon + 1; index <= text.size(); ++index) {
        const char ch = index < text.size() ? text[index] : ' ';
        if (ch == '\\' && index + 1 < text.size()) {
            const char next = text[index + 1];
            if (next == '\n') { ++index; continue; }
            if (next == '\r') { ++index; if (index + 1 < text.size() && text[index + 1] == '\n') ++index; continue; }
            if (next == ' ' || next == '\t' || next == '#' || next == '$') { ++index; word += next; continue; }
            // MinGW 依赖文件可能原样输出 Windows 路径分隔符，不能当作任意转义删除。
            word += ch; continue;
        }
        if (ch == '$' && index + 1 < text.size() && text[index + 1] == '$') ++index;
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!word.empty()) { files.push_back(word); word.clear(); }
        } else word += ch;
    }
    Memory::OutputStringStream output;
    for (const auto &file : files) {
        std::uint64_t value = 0;
        if (!moduleFileHash(std::filesystem::absolute(file), &value)) return false;
        output << value << ' ' << std::quoted(std::filesystem::absolute(file).string()) << '\n';
    }
    return !files.empty() && moduleWrite(stamp, output.str());
}

/** @brief 缓存目录的单写者锁；异常退出后可显式删除空锁目录再重试。 */
struct ModuleBuildLock
{
    std::filesystem::path path; ///< 本次持有的空目录锁。
    /** @brief 离开构建时释放空目录锁。 */
    ~ModuleBuildLock() { std::error_code error; if (!path.empty()) std::filesystem::remove(path, error); }
};

/**
 * @brief 编译变化的模块并链接；缓存对象和源码通过 AOT 清单显式管理。
 * @param modules 共享 ABI 与模块函数。
 * @param runtime 唯一运行时、数据区、入口和分派表。
 * @param executable 最终目标路径。
 * @param cacheDirectory 用户指定缓存目录，空时使用目标名.magel-modules。
 * @param compiler 本地 C++ 工具链。
 * @param compileFlags 编译选项。
 * @param linkFlags 链接选项。
 * @param error 失败说明。
 * @return 成功返回 true。
 */
bool buildAotModules(const magel_emit::AotModules &modules, const Memory::String &runtime,
                     const std::filesystem::path &executable, const Memory::String &cacheDirectory,
                     const Memory::String &compiler, const Memory::String &compileFlags,
                     const Memory::String &linkFlags, Memory::String *error)
{
    namespace fs = std::filesystem;
    const fs::path cache = fs::absolute(cacheDirectory.empty() ? Memory::String(executable.string()) + ".magel-modules" : cacheDirectory).lexically_normal();
    std::error_code filesystemError;
    fs::create_directories(cache, filesystemError);
    if (filesystemError) { *error = "无法创建模块缓存：" + filesystemError.message(); return false; }
    ModuleBuildLock lock;
    const auto lockPath = cache / ".build-lock";
    if (!fs::create_directory(lockPath, filesystemError)) {
        *error = "模块缓存正在使用，或上次构建异常退出；确认无构建进程后删除空目录 " + lockPath.string();
        return false;
    }
    lock.path = lockPath;
    Memory::Set<Memory::String> managed;
    bool exists = false;
    if (!readAotManifest(cache, &managed, &exists, error)) return false;
    Memory::String compilerVersion;
    if (!runShellCommand(executableCommand(compiler) + " --version", &compilerVersion)) {
        *error = "无法识别模块编译器：" + compiler + "\n" + compilerVersion; return false;
    }
    Memory::String environment;
    for (const char *name : {"PATH", "CPATH", "CPLUS_INCLUDE_PATH", "COMPILER_PATH", "LIBRARY_PATH", "SDKROOT"}) {
        const char *value = std::getenv(name);
        environment += Memory::String(name) + "=" + (value ? value : "") + "\n";
    }
    Memory::String compilerIdentity;
    Memory::Vector<std::filesystem::path> candidates{std::filesystem::path(compiler)};
    const char *pathEnvironment = std::getenv("PATH");
    Memory::String paths = pathEnvironment ? pathEnvironment : "";
#ifdef _WIN32
    const char separator = ';';
#else
    const char separator = ':';
#endif
    for (std::size_t at = 0; at <= paths.size();) {
        const auto end = paths.find(separator, at);
        const auto directory = paths.substr(at, end == Memory::String::npos ? paths.size() - at : end - at);
        candidates.push_back(std::filesystem::path(directory) / compiler);
#ifdef _WIN32
        candidates.push_back(std::filesystem::path(directory) / (compiler + ".exe"));
#endif
        if (end == Memory::String::npos) break;
        at = end + 1;
    }
    for (const auto &candidate : candidates) {
        std::uint64_t value = 0;
        if (moduleFileHash(candidate, &value)) { compilerIdentity = candidate.string() + "=" + moduleHex(value); break; }
    }
    const Memory::String configuration = "MAGEL-AOT-MODULE-1\n" + compiler + "\n" + compilerVersion + "\n" + compilerIdentity + "\n" + compileFlags + "\n" + environment;
    const Memory::String headerName = "magel_modules_" + moduleHex(moduleHash(modules.header)) + ".hpp";
    managed.insert(headerName);
    if (!moduleWrite(cache / headerName, modules.header)) { *error = "写不了模块公共头"; return false; }
    auto units = modules.units;
    units.emplace_back("@runtime", runtime);
    Memory::Vector<Memory::String> objects;
    Memory::String table = "{\"schemaVersion\":1,\"backend\":\"native-aot\",\"modules\":[";
    std::size_t reused = 0, compiled = 0;
    for (const auto &unit : units)
    {
        const bool isRuntime = unit.first == "@runtime";
        const Memory::String source = isRuntime ? unit.second : "#include \"" + headerName + "\"\n" + unit.second;
        const Memory::String key = moduleHex(moduleHash(unit.first)) + "-" + moduleHex(moduleHash(configuration + "\n" + source));
        const Memory::String sourceName = key + ".cpp";
#ifdef _WIN32
        const Memory::String objectName = key + ".obj";
#else
        const Memory::String objectName = key + ".o";
#endif
        const Memory::String stampName = key + ".digest";
        const Memory::String logName = key + ".log";
        const Memory::String dependenciesName = key + ".dependencies";
        const Memory::String dependencyFileName = key + ".d";
        for (const auto &name : {sourceName, objectName, stampName, logName, dependenciesName, dependencyFileName, objectName + ".building"}) managed.insert(name);
        // 先登记，编译失败时 clear 仍可安全清理本次产生的文件。
        if (!writeAotManifest(cache, managed, error)) return false;
        if (!moduleWrite(cache / sourceName, source)) { *error = "写不了模块源码"; return false; }
        std::uint64_t expected = 0, actual = 0;
        std::ifstream stamp(cache / stampName);
        const bool hit = static_cast<bool>(stamp >> expected) && moduleFileHash(cache / objectName, &actual) && expected == actual && moduleDependenciesCurrent(cache / dependenciesName);
        if (!hit)
        {
            const auto temporary = cache / (objectName + ".building");
            const Memory::String command = executableCommand(compiler) + " " + compileFlags + " -MD -MF \"" + (cache / dependencyFileName).generic_string() + "\" -MT module -c \"" + (cache / sourceName).generic_string() + "\" -o \"" + temporary.string() + "\"";
            Memory::String output;
            const bool success = runShellCommand(command, &output);
            moduleWrite(cache / logName, command + "\n" + output);
            if (!success) {
                fs::remove(temporary, filesystemError);
                *error = "模块 " + unit.first + " 编译失败：\n" + output; return false;
            }
            fs::remove(cache / objectName, filesystemError); filesystemError.clear();
            fs::rename(temporary, cache / objectName, filesystemError);
            if (filesystemError || !moduleFileHash(cache / objectName, &actual) ||
                !moduleWrite(cache / stampName, Memory::toString(actual)) ||
                !moduleSaveDependencies(cache / dependencyFileName, cache / dependenciesName)) {
                *error = "保存模块目标文件失败：" + unit.first; return false;
            }
            ++compiled;
        }
        else ++reused;
        objects.push_back((cache / objectName).string());
        if (objects.size() > 1) table += ',';
        table += "{\"name\":" + moduleJson(unit.first) + ",\"object\":" + moduleJson(objects.back()) +
                 ",\"reused\":" + (hit ? "true" : "false") + "}";
    }
    table += "]}\n";
    managed.insert("modules.json"); managed.insert("link.log"); managed.insert("objects.rsp");
    if (!writeAotManifest(cache, managed, error)) return false;
    // 响应文件避免 Windows 命令长度限制；只包含本次实际参与链接的目标文件。
    Memory::String response;
    for (const auto &object : objects) {
        auto path = fs::path(object).generic_string(); response += "\"" + path + "\"\n";
    }
    if (!moduleWrite(cache / "objects.rsp", response)) { *error = "写不了模块链接响应文件"; return false; }
    // 最终文件名已由调用方按目标工具链和产物类型补齐。
    const auto linkOutput = executable;
    const Memory::String command = executableCommand(compiler) + " " + linkFlags + " @\"" + (cache / "objects.rsp").string() + "\" -o \"" + linkOutput.string() + "\"";
    Memory::String output;
    const bool linked = runShellCommand(command, &output);
    moduleWrite(cache / "link.log", command + "\n" + output);
    if (!linked) { *error = "模块链接失败：\n" + output; return false; }
    if (!moduleWrite(cache / "modules.json", table)) { *error = "写不了模块构建报告"; return false; }
    std::cout << "模块独立编译：" << compiled << " 个编译，" << reused << " 个复用；报告 " << (cache / "modules.json").string() << "\n";
    return true;
}
