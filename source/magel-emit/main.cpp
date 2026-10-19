/** @file main.cpp
 * @brief 将 MAGEL 四元式与数据区编译为自包含 AOT 可执行文件。
 * `.svmb` 由 MAGEL 前端生成；本工具只提供原生 AOT 导出。
 */

#include "../runtime_allocator.hpp"
#include "../magel_stdlib_embedded.cpp"
#include "stackvm.hpp"
#include "aot.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <set>
#include <string>
#include <vector>

namespace magel_emit
{

/** @brief AOT 数据布局中一个符号的名称、偏移及可选容量信息。 */
struct DataSymbolRow
{
    Memory::String name;
    std::int64_t offset = 0;
    std::int64_t size = 0;
    std::int64_t stride = 0;
    bool hasSize = false;
    bool hasStride = false;
};

} // namespace magel_emit

namespace
{


/** 取一个路径的目录部分（没有分隔符就返回 "."）。 */
Memory::String dirNameOf(const Memory::String &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == Memory::String::npos) return ".";
    if (slash == 0u) return path.substr(0u, 1u);
    return path.substr(0u, slash);
}

/** 取一个路径的文件名部分。 */
Memory::String baseNameOf(const Memory::String &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const Memory::String name = (slash == Memory::String::npos) ? path : path.substr(slash + 1u);
    const std::size_t dot = name.find_last_of('.');
    return (dot == Memory::String::npos) ? name : name.substr(0u, dot);
}

/** makefile 里的文件路径：不加引号（make 不做 shell 解析），只转义 make 元字符
 *  （空格 / `#` / `:` / `\` / `$`），这样带空格的输出路径也成立。配方行仍交给
 *  shell，用普通引号即可。 */
Memory::String makeFilePath(const Memory::String &path)
{
    Memory::String out;
    auto makePath = path;
#ifdef _WIN32
    std::replace(makePath.begin(), makePath.end(), '\\', '/');
#endif
    for (char c : makePath)
    {
        if (c == '$') { out += "$$"; continue; }
        if (c == ' ' || c == '#' || c == ':' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

/**
 * @brief 读取用户文件或显式命名的内嵌运行时资产。
 * @param path 文件路径；embedded:// 前缀仅查内嵌资产，不访问相邻仓库。
 * @param out 成功时接收完整内容。
 * @return 读取成功时返回 true，找不到内容时返回 false。
 */
bool readWholeFile(const Memory::String &path, Memory::String *out)
{
    const Memory::String embeddedPrefix = "embedded://";
    if (path.compare(0, embeddedPrefix.size(), embeddedPrefix) == 0)
        return magel_module::embedded::findGeneratorAssetByPath(path.substr(embeddedPrefix.size()), out);
    std::ifstream stream(path, std::ios::binary);
    if (stream)
    {
        Memory::OutputStringStream buffer;
        buffer << stream.rdbuf();
        *out = buffer.str();
        return true;
    }
    /* 磁盘上没有：回退到内嵌的运行时资产（单文件生成器）。 */
    return magel_module::embedded::findGeneratorAssetByPath(path, out);
}

bool managedRelativePath(const Memory::String &text, std::filesystem::path *path,
                         Memory::String *error)
{
    *path = std::filesystem::path(text).lexically_normal();
    if (path->empty() || *path == "." || path->is_absolute() || path->has_root_path())
    {
        *error = "产物清单包含不安全路径：" + text;
        return false;
    }
    for (const std::filesystem::path &part : *path)
    {
        if (part == "..")
        {
            *error = "产物清单不能越出输出目录：" + text;
            return false;
        }
    }
    if (path->generic_string() == ".magel-aot-manifest")
    {
        *error = "产物不能占用内部清单名";
        return false;
    }
    return true;
}

bool rejectSymlinkParents(const std::filesystem::path &root,
                          const std::filesystem::path &relative,
                          Memory::String *error)
{
    std::filesystem::path current = root;
    for (const std::filesystem::path &part : relative.parent_path())
    {
        current /= part;
        std::error_code statusError;
        const std::filesystem::file_status status =
            std::filesystem::symlink_status(current, statusError);
        if (statusError && statusError != std::errc::no_such_file_or_directory)
        {
            *error = "无法检查产物目录：" + current.string() + "（" +
                     statusError.message() + "）";
            return false;
        }
        if (!statusError && std::filesystem::is_symlink(status))
        {
            *error = "产物目录不能经过符号链接：" + current.string();
            return false;
        }
    }
    return true;
}



bool removeManaged(const std::filesystem::path &root,
                   const Memory::Set<Memory::String> &files,
                   const Memory::Set<Memory::String> &kept,
                   std::size_t *removed, Memory::String *error)
{
    Memory::Vector<std::filesystem::path> parents;
    for (const Memory::String &name : files)
    {
        if (kept.count(name) != 0u) continue;
        std::filesystem::path relative;
        if (!managedRelativePath(name, &relative, error)) return false;
        if (!rejectSymlinkParents(root, relative, error)) return false;
        const std::filesystem::path target = root / relative;
        std::error_code statusError;
        const bool exists = std::filesystem::exists(target, statusError) ||
                            std::filesystem::is_symlink(target, statusError);
        if (statusError == std::errc::no_such_file_or_directory) continue;
        if (statusError)
        {
            *error = "无法检查旧产物：" + target.string() + "（" +
                     statusError.message() + "）";
            return false;
        }
        if (!exists) continue;
        std::error_code removeError;
        if (!std::filesystem::remove(target, removeError) || removeError)
        {
            *error = "无法删除旧产物：" + target.string() + "（" +
                     removeError.message() + "）";
            return false;
        }
        ++*removed;
        for (std::filesystem::path parent = target.parent_path();
             parent != root && !parent.empty(); parent = parent.parent_path())
            parents.push_back(parent);
    }
    std::sort(parents.begin(), parents.end(),
              [](const std::filesystem::path &left,
                 const std::filesystem::path &right) {
                  if (left.native().size() != right.native().size())
                      return left.native().size() > right.native().size();
                  return left.native() < right.native();
              });
    parents.erase(std::unique(parents.begin(), parents.end()), parents.end());
    for (const std::filesystem::path &parent : parents)
    {
        std::error_code emptyError;
        const bool empty = std::filesystem::is_empty(parent, emptyError);
        if (emptyError == std::errc::no_such_file_or_directory) continue;
        if (emptyError)
        {
            *error = "无法检查产物目录：" + parent.string() + "（" +
                     emptyError.message() + "）";
            return false;
        }
        if (!empty) continue;
        std::error_code removeError;
        if (!std::filesystem::remove(parent, removeError) || removeError)
        {
            *error = "无法清理空产物目录：" + parent.string() + "（" +
                     removeError.message() + "）";
            return false;
        }
    }
    return true;
}



// ---------------------------------------------------------------------------
//  AOT（--target cpp-aot）：中间文件清单、编译器调用与清理
// ---------------------------------------------------------------------------

const char *const kAotManifestName = ".magel-aot-manifest";
const char *const kAotManifestHeader = "# magel-aot generated files v1";

/** 读 AOT 中间文件清单；不存在返回 true 且 `*exists = false`。 */
bool readAotManifest(const std::filesystem::path &root, Memory::Set<Memory::String> *files,
                     bool *exists, Memory::String *error)
{
    files->clear();
    const std::filesystem::path manifest = root / kAotManifestName;
    std::error_code statusError;
    *exists = std::filesystem::exists(manifest, statusError);
    if (statusError)
    {
        *error = "无法检查 AOT 清单：" + statusError.message();
        return false;
    }
    if (!*exists) return true;
    std::ifstream input(manifest, std::ios::binary);
    Memory::String line;
    if (!input || !std::getline(input, line))
    {
        *error = "AOT 清单为空或无法读取：" + manifest.string();
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != kAotManifestHeader)
    {
        *error = "AOT 清单版本无法识别：" + manifest.string();
        return false;
    }
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::filesystem::path relative;
        if (!managedRelativePath(line, &relative, error)) return false;
        if (relative.generic_string() == kAotManifestName)
        {
            *error = "AOT 中间文件不能占用清单名：" + line;
            return false;
        }
        files->insert(relative.generic_string());
    }
    return true;
}

/** 写 AOT 中间文件清单。 */
bool writeAotManifest(const std::filesystem::path &root,
                      const Memory::Set<Memory::String> &files, Memory::String *error)
{
    std::error_code directoryError;
    std::filesystem::create_directories(root, directoryError);
    if (directoryError)
    {
        *error = MAGEL_TEXT("无法创建输出目录：") + directoryError.message();
        return false;
    }
    std::ofstream output(root / kAotManifestName, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        *error = "无法写入 AOT 清单";
        return false;
    }
    output << kAotManifestHeader << '\n';
    for (const Memory::String &file : files) output << file << '\n';
    return static_cast<bool>(output);
}

/** 删除 AOT 清单登记的全部中间文件（并删除清单）。 */
bool clearAotExport(const Memory::String &dir, std::size_t *removed, Memory::String *error)
{
    const std::filesystem::path root = std::filesystem::absolute(dir).lexically_normal();
    Memory::Set<Memory::String> files;
    bool exists = false;
    if (!readAotManifest(root, &files, &exists, error)) return false;
    if (!exists) return true;
    if (!removeManaged(root, files, {}, removed, error)) return false;
    std::error_code removeError;
    std::filesystem::remove(root / kAotManifestName, removeError);
    return true;
}

/** `--opt-level` / `-O*` → 传给 C++ 编译器的旗标。 */
bool aotOptFlag(const Memory::String &text, Memory::String *flag, Memory::String *error)
{
    if (text.rfind("raw:", 0u) == 0u)
    {
        const Memory::String raw = text.substr(4u);
        if (raw.size() < 2u || raw[0] != '-' || raw[1] != 'O')
        {
            *error = "无法识别的优化级别：" + text;
            return false;
        }
        *flag = raw;
        return true;
    }
    if (text == "0") *flag = "-O0";
    else if (text == "1") *flag = "-O1";
    else if (text == "2") *flag = "-O2";
    else if (text == "3") *flag = "-O3";
    else if (text == "s") *flag = "-Os";
    else if (text == "fast") *flag = "-Ofast";
    else
    {
        *error = "优化级别只支持 0/1/2/3/s/fast（或直接传 -O2 这类编译器旗标）：" + text;
        return false;
    }
    return true;
}

/**
 * @brief 将单个工具路径引用为 shell 可执行文件参数。
 * @param path 完整路径或命令名，不包含额外命令行参数。
 * @return 已引用的可执行文件。
 */
Memory::String executableCommand(const Memory::String &path)
{
#ifdef _WIN32
    if (path.find_first_of("\"%!?\r\n") != Memory::String::npos) throw std::runtime_error("工具路径包含不支持的命令字符");
    return "\"" + path + "\"";
#else
    Memory::String out = "'";
    for (char value : path) out += value == '\'' ? Memory::String("'\"'\"'") : Memory::String(1, value);
    return out + "'";
#endif
}

/** 跑一条 shell 命令并收集输出；返回是否成功。 */
bool runShellCommand(const Memory::String &command, Memory::String *output)
{
    output->clear();
    Memory::String shellCommand = command + " 2>&1";
#ifdef _WIN32
    /* cmd /c 保留首个带引号的工具路径及重定向需要外层引号。 */
    if (!command.empty() && command.front() == '"') shellCommand = "\"" + shellCommand + "\"";
#endif
    FILE *pipe = ::popen(shellCommand.c_str(), "r");
    if (pipe == nullptr) return false;
    char buffer[512];
    std::size_t count = 0u;
    while ((count = std::fread(buffer, 1u, sizeof(buffer), pipe)) > 0u)
    {
        output->append(buffer, count);
    }
    const int status = ::pclose(pipe);
    return status == 0;
}

#include "module_build.hpp"

/**
 * @brief 把 `#include "xxx"` 那一行注释掉。
 *
 * 内嵌的时候头文件内容已经在同一份文件里了，所以指向**仓库头**的 include 必须去掉
 * （生成物不依赖仓库路径）；系统头（<...>）保留。行尾可能是 CRLF（core.autocrlf
 * 检出的工作区就是这样），所以按"从 # 到行尾"整段替换，不假设行尾是 \n。
 * @param text 源码。
 * @param header 要去掉的头文件名（如 `stackvm.hpp`）。
 */
void stripLocalInclude(Memory::String *text, const Memory::String &header)
{
    const Memory::String needle = "#include \"" + header + "\"";
    std::size_t at = 0u;
    while ((at = text->find(needle, at)) != Memory::String::npos)
    {
        const std::size_t lineEnd = text->find('\n', at);
        const std::size_t end = (lineEnd == Memory::String::npos) ? text->size() : lineEnd;
        text->replace(at, end - at, "/* 内嵌：这个头已经在同一份文件里了 */");
        at = text->find(needle, at + 1u);
    }
}

/**
 * @brief 把 `#include "<prefix>xxx"` 就地展开成被包含文件的内容。
 *
 * `stackvm.cpp` 按职责拆成了 `parts/` 下的 `.cpp`，用 `#include` 拼回同一个
 * 翻译单元；但导出程序必须是**单文件**产物（编译命令只有 `g++ <文件>`），所以
 * 内嵌时要把这些分片也展开进来。只处理带指定前缀的相对 include，别的不动。
 * @param text 源码（原地修改）。
 * @param baseDir 分片所在目录。
 * @param prefix 分片相对路径前缀（如 `parts/`）。
 */
void inlineLocalIncludes(Memory::String *text, const Memory::String &baseDir,
                         const Memory::String &prefix)
{
    const Memory::String needle = "#include \"" + prefix;
    std::size_t at = 0u;
    while ((at = text->find(needle, at)) != Memory::String::npos)
    {
        const std::size_t quote = at + Memory::String("#include \"").size();
        const std::size_t lineEnd = text->find('\n', at);
        const std::size_t end = (lineEnd == Memory::String::npos) ? text->size() : lineEnd;
        const std::size_t closeQuote = text->find('"', quote);
        if ((closeQuote == Memory::String::npos) || (closeQuote > end))
        {
            at = (end == text->size()) ? Memory::String::npos : end + 1u;
            continue;
        }
        const Memory::String relative = text->substr(quote, closeQuote - quote);
        Memory::String body;
        if (readWholeFile(baseDir + "/" + relative, &body))
        {
            *text = text->substr(0u, at) + body + text->substr(end);
            at += body.size();
        }
        else
        {
            at = (end == text->size()) ? Memory::String::npos : end + 1u;
        }
    }
}

/** 把一段文本按行切开（去掉行尾的 \r 与空行）。 */
Memory::Vector<Memory::String> splitLines(const Memory::String &text)
{
    Memory::Vector<Memory::String> out;
    Memory::InputStringStream stream(text);
    Memory::String line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

Memory::String trimAscii(const Memory::String &text)
{
    const std::size_t begin = text.find_first_not_of(" \t");
    if (begin == Memory::String::npos) return Memory::String();
    const std::size_t end = text.find_last_not_of(" \t");
    return text.substr(begin, end - begin + 1u);
}

/** 兼容 magel 的 `# module:` 与 stackvm 的 `# 名称:` 元数据。 */
Memory::String programNameMetadata(const Memory::String &text)
{
    for (const Memory::String &rawLine : splitLines(text))
    {
        const Memory::String line = trimAscii(rawLine);
        if (line.empty() || line.front() != '#') continue;
        const Memory::String body = trimAscii(line.substr(1u));
        const std::size_t colon = body.find(':');
        if (colon == Memory::String::npos) continue;
        const Memory::String key = trimAscii(body.substr(0u, colon));
        if (key == "模块" || key == "module" || key == "名称" || key == "name")
            return trimAscii(body.substr(colon + 1u));
    }
    return Memory::String();
}

Memory::String cppString(const Memory::String &text);

bool decimalField(const Memory::String &line, const Memory::String &key, std::int64_t *value)
{
    const std::size_t at = line.find(key + "=");
    if (at == Memory::String::npos) return false;
    const std::size_t begin = at + key.size() + 1u;
    const std::size_t end = line.find(' ', begin);
    const Memory::String text = line.substr(begin, end - begin);
    char *tail = nullptr;
    const unsigned long long parsed = std::strtoull(text.c_str(), &tail, 10);
    if (tail == nullptr || *tail != '\0' || parsed > static_cast<unsigned long long>(
                                                    std::numeric_limits<std::int64_t>::max()))
        return false;
    *value = static_cast<std::int64_t>(parsed);
    return true;
}

bool parseDataSymbol(const Memory::String &line, magel_emit::DataSymbolRow *row)
{
    const std::size_t space = line.find(' ');
    if (space == Memory::String::npos) return false;
    row->name = cppString(line.substr(0u, space));
    if (!decimalField(line, "offset", &row->offset)) return false;
    row->hasSize = decimalField(line, "size", &row->size);
    row->hasStride = decimalField(line, "stride", &row->stride);
    return true;
}

/** 把任意字节串变成不依赖源码编码的 C++ 字符串字面量。 */
Memory::String cppString(const Memory::String &text)
{
    Memory::OutputStringStream out;
    out << '"';
    for (const unsigned char byte : text)
    {
        switch (byte)
        {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (byte >= 0x20u && byte <= 0x7eu)
            {
                out << static_cast<char>(byte);
            }
            else
            {
                out << '\\' << std::oct << std::setw(3) << std::setfill('0')
                    << static_cast<unsigned int>(byte) << std::dec << std::setfill(' ');
            }
        }
    }
    out << '"';
    return out.str();
}

/** 精确保留 double 位值所需的十六进制浮点字面量。 */




void printUsage(const Memory::String &program)
{
    std::cout << "用法：" << program << " --quad FILE [--data FILE] [--layout FILE]\n"
              << "              [--out FILE | --out-dir DIR]"
                 " [--target cpp-aot] [--glue FILE] [--stack-slots N] [--threads]\n"
              << "       " << program << " --target cpp-aot --quad FILE ... [--keep-intermediate]\n"
              << "              [--opt-level 0|1|2|3|s|fast | -O2] [--debug | --release]\n"
              << "              [--checked | --unchecked] AOT 默认 checked；unchecked 关闭可选护栏与 panic 跟踪\n"
              << "              [--cpp-output] [--cpp-output-dir DIR] [--execute-output FILE]\n"
              << "              [--shared | --object] 库模式允许省略 main，要求 export 函数\n"
              << "       " << program << " clear --out-dir DIR\n"
              << "       " << program << " --target cpp-aot --clean [--out FILE | --out-dir DIR]\n\n"
              << "把四元式和数据区编译为 AOT 程序或 C 接口库。默认目标 cpp-aot。\n"
              << "  --out 的 .dll / .so 自动选择共享库，.o 选择可重定位目标文件；不支持 .lib / .a。\n"
              << "  --cpp-output 导出程序时保留生成的 C++ 源码（缺省不写/不留代码）；\n"
              << "  --cpp-output-dir DIR 指定源码写到哪里（隐含 --cpp-output）；\n"
              << "  --separate-modules 按 MAGEL 模块编译本地目标文件并缓存；仅 cpp-aot。\n"
              << "  --module-cache DIR 指定模块目标文件缓存目录。\n"
              << "  --execute-output FILE 指定可执行文件 / 二进制的位置（等价扩展 --out）。\n"
              << "  --gcc-toolchain DIR|COMMAND 指定 GCC 根目录、bin 目录或编译器路径/命令。\n"
              << "  --cxx PATH 编译 / 链接用的 C++ 编译器（缺省环境变量 CXX，再 g++）；\n"
              << "  --g++ PATH / --gxx PATH 是 --cxx 的别名，--gcc PATH 是 --cc 的别名。\n"
              << "  --cc PATH  C 编译器（写进构建脚本的 CC；AOT 产物是 C++，用 --cxx 编）。\n"
              << "  --make PATH 用指定 make 驱动编译（先生成 <exe>.aot.mk 构建脚本再跑 make；\n"
              << "    缺省不传就直接调编译器）。构建脚本是中间文件，成功后按清单删除。\n"
              << "--glue 用于 C++ 导出：传一个原样内嵌的 .cpp（里面定义 bindMagelGlue）。\n"
              << "--runtime-arena BYTES 指定运行内存池总容量（默认 512 MiB）。\n"
              << "  --no-panic-details 隐藏 panic 路径及调用记录（上游符号压缩时自动启用）。\n"
              << "默认导出程序不带调试信息表（位置没了，程序行为不变）；--debug 才带上。\n"
              << "--threads 给 C++ 产物启用隔离的宿主线程与文本消息。\n"
              << "默认输出为四元式同名可执行文件；\n"
              << "--vm-dir <生成器>/../../magel/library/stackvm。\n";
}

} // namespace

namespace magel_emit
{

/**
 * @brief 解析 AOT 导出参数，生成运行时源码并调用宿主工具链构建产物。
 * @param argc 包含工具名称的命令行参数数量。
 * @param argv 按调用顺序排列的参数文本。
 * @return 导出成功时返回零；参数、输入、资产或宿主构建失败时返回非零状态。
 * @note 默认使用内嵌运行时资产；显式目录参数仍可选择用户提供的运行时源码。
 */
int runCli(int argc, char **argv)
{
    const Memory::String self = (argc > 0) ? argv[0] : "magel-emit";

    Memory::String quadPath;
    Memory::String dataPath;
    Memory::String layoutPath;
    Memory::String outPath;
    Memory::String outDir;
    Memory::String targetKind = "cpp-aot";
    Memory::String gluePath;
    Memory::String vmDir = "embedded://stackvm";
    Memory::String bigintDir = "embedded://bignum";
    Memory::String mcDir = "embedded://magel";
    Memory::String nameOverride;
    /** `--runtime-arena` 的原始值：-1 = 没传（按档给默认）。 */
    std::int64_t runtimeArenaBytes = -1;
    std::int64_t stackSlots = 0;
    bool stackSlotsSet = false;
    bool threadsEnabled = false;
    bool optimizeQuads = true;
    /** 默认**不带**调试信息（旧默认带上）：`--debug` 才写调试表并保留 `-g`。 */
    bool stripDebug = true;
    bool clearCommand = false;
    bool aotKeepIntermediate = false;
    bool aotCleanCommand = false;
    bool aotRelease = false;
    bool sharedLibrary = false;
    bool objectLibrary = false;
    bool aotDebug = false;
    Memory::String optLevelText = "3";
    /** `--cpp-output`：导出可执行文件时是否保留生成的 C++ 源码（缺省关闭）。 */
    bool cppOutput = false;
    /** `--cpp-output-dir DIR`：生成的 C++ 源码写到哪里（缺省与可执行文件同目录）。 */
    Memory::String cppOutputDir;
    /** `--execute-output FILE`：可执行文件 / 二进制的位置（等价扩展 `--out`）。 */
    bool hidePanicDetails = false; ///< 隐藏符号的上游配置禁止 panic 泄露调用记录。
    bool unchecked = false; ///< AOT 默认开启可选分配区域护栏及 panic 调用记录。
    Memory::String executeOutput;
    bool separateModules = false;
    Memory::String moduleCache;
    /** `--cxx PATH`：编译 / 链接 AOT 产物用的 C++ 编译器（缺省环境变量 `CXX`、再 `g++`）。 */
    Memory::String cxxProgram;
    Memory::String gccToolchain;
    /** `--cc PATH`：C 编译器（写进生成的构建脚本的 `CC`；AOT 产物是 C++）。 */
    Memory::String ccProgram;
    /** `--make PATH`：用指定 make 驱动 AOT 编译（缺省 = 直接调编译器）。 */
    Memory::String makeProgram;

    for (int index = 1; index < argc; ++index)
    {
        const Memory::String argument = argv[index];
        const auto value = [&](const char *option) -> Memory::String {
            if (index + 1 >= argc)
            {
                std::cerr << option << " 缺少取值\n";
                std::exit(2);
            }
            return argv[++index];
        };
        if (argument == "clear" && index == 1) { clearCommand = true; continue; }
        if (argument == "-h" || argument == "--help") { printUsage(self); return 0; }
        if (argument == "--quad") { quadPath = value("--quad"); continue; }
        if (argument == "--data") { dataPath = value("--data"); continue; }
        if (argument == "--layout") { layoutPath = value("--layout"); continue; }
        if (argument == "--out") { outPath = value("--out"); continue; }
        if (argument == "--out-dir") { outDir = value("--out-dir"); continue; }
        if (argument == "--target") { targetKind = value("--target"); continue; }
        if (argument == "--glue") { gluePath = value("--glue"); continue; }
        if (argument == "--vm-dir") { vmDir = value("--vm-dir"); continue; }
        if (argument == "--bigint-dir") { bigintDir = value("--bigint-dir"); continue; }
        if (argument == "--magel-dir") { mcDir = value("--magel-dir"); continue; }
        if (argument == "--runtime-arena")
        {
            const auto bytes = value("--runtime-arena");
            if (bytes.empty() || bytes.find_first_not_of("0123456789") != Memory::String::npos) return 2;
            try { runtimeArenaBytes = std::stoll(bytes); } catch (...) { return 2; }
            if (runtimeArenaBytes < 64) return 2;
            continue;
        }
        if (argument == "--stack-slots")
        {
            stackSlots = std::strtoll(value("--stack-slots").c_str(), nullptr, 10);
            stackSlotsSet = true;
            continue;
        }
        if (argument == "--name") { nameOverride = value("--name"); continue; }
        if (argument == "--threads") { threadsEnabled = true; continue; }
        if (argument == "--no-opt") { optimizeQuads = false; continue; }
        /* `--no-debug` 是兼容别名：显式不带调试信息（现在也是缺省）。 */
        if (argument == "--no-panic-details") { hidePanicDetails = true; continue; }
        if (argument == "--unchecked") { unchecked = true; continue; }
        if (argument == "--checked") { unchecked = false; continue; }
        if (argument == "--no-debug") { stripDebug = true; aotDebug = false; continue; }
        if (argument == "--keep-intermediate") { aotKeepIntermediate = true; continue; }
        if (argument == "--clean") { aotCleanCommand = true; continue; }
        /* `--debug` 才带调试信息：导出程序写调试表，AOT 编译加 `-g` 与 `#line`。 */
        if (argument == "--debug") { aotDebug = true; stripDebug = false; continue; }
        if (argument == "--shared") { sharedLibrary = true; continue; }
        if (argument == "--object") { objectLibrary = true; continue; }
        if (argument == "--release") { aotRelease = true; aotDebug = false; stripDebug = true; continue; }
        if (argument == "--opt-level") { optLevelText = value("--opt-level"); continue; }
        /* `--cpp-output` / `--no-cpp-output` / `--cpp-output-dir` / `--execute-output`：
         * 控制导出程序时是否顺带保留生成的 C++ 源码，以及产物路径。 */
        if (argument == "--cpp-output" || argument == "--cpp-output=true" || argument == "--cpp-output=1")
        { cppOutput = true; continue; }
        if (argument == "--no-cpp-output" || argument == "--cpp-output=false" || argument == "--cpp-output=0")
        { cppOutput = false; continue; }
        if (argument == "--cpp-output-dir") { cppOutputDir = value("--cpp-output-dir"); cppOutput = true; continue; }
        if (argument.rfind("--cpp-output-dir=", 0u) == 0u)
        { cppOutputDir = argument.substr(Memory::String("--cpp-output-dir=").size()); cppOutput = true; continue; }
        if (argument == "--separate-modules") { separateModules = true; continue; }
        if (argument == "--module-cache") { moduleCache = value("--module-cache"); continue; }
        if (argument == "--execute-output") { executeOutput = value("--execute-output"); continue; }
        if (argument.rfind("--execute-output=", 0u) == 0u)
        { executeOutput = argument.substr(Memory::String("--execute-output=").size()); continue; }
        if (argument == "--gcc-toolchain") { gccToolchain = value("--gcc-toolchain"); continue; }
        if (argument.rfind("--gcc-toolchain=", 0u) == 0u) { gccToolchain = argument.substr(16); continue; }
        if (argument == "--cxx" || argument == "--g++" || argument == "--gxx")
        { cxxProgram = value(argument.c_str()); continue; }
        if (argument.rfind("--cxx=", 0u) == 0u)
        { cxxProgram = argument.substr(Memory::String("--cxx=").size()); continue; }
        if (argument.rfind("--g++=", 0u) == 0u)
        { cxxProgram = argument.substr(Memory::String("--g++=").size()); continue; }
        if (argument.rfind("--gxx=", 0u) == 0u)
        { cxxProgram = argument.substr(Memory::String("--gxx=").size()); continue; }
        if (argument == "--cc" || argument == "--gcc")
        { ccProgram = value(argument.c_str()); continue; }
        if (argument.rfind("--cc=", 0u) == 0u)
        { ccProgram = argument.substr(Memory::String("--cc=").size()); continue; }
        if (argument.rfind("--gcc=", 0u) == 0u)
        { ccProgram = argument.substr(Memory::String("--gcc=").size()); continue; }
        if (argument == "--make")
        { makeProgram = value("--make"); continue; }
        if (argument.rfind("--make=", 0u) == 0u)
        { makeProgram = argument.substr(Memory::String("--make=").size()); continue; }
        if (argument.rfind("--opt-level=", 0u) == 0u)
        {
            optLevelText = argument.substr(Memory::String("--opt-level=").size());
            continue;
        }
        if (argument.size() >= 2u && argument[0] == '-' && argument[1] == 'O')
        {
            optLevelText = "raw:" + argument;
            continue;
        }
        std::cerr << "不认识的选项：" << argument << "\n";
        printUsage(self);
        return 2;
    }
    if (clearCommand) aotCleanCommand = true;
    if (targetKind != "cpp-aot")
    {
        std::cerr << "--target 只支持 cpp-aot\n";
        return 2;
    }
    if (targetKind == "cpp-aot" && !aotCleanCommand && quadPath.empty())
    {
        std::cerr << "cpp-aot 至少要给 --quad FILE\n";
        return 2;
    }
    if (!outPath.empty() && !outDir.empty())
    {
        std::cerr << "--out 与 --out-dir 只能使用一个\n";
        return 2;
    }
    if (stackSlotsSet && stackSlots <= 0)
    {
        std::cerr << "--stack-slots 必须是正整数\n";
        return 2;
    }
    if (targetKind == "cpp-aot" && aotCleanCommand)
    {
        if (outPath.empty() && outDir.empty())
        {
            std::cerr << "--clean 需要 --out FILE 或 --out-dir DIR\n";
            return 2;
        }
        const Memory::String dir = !outDir.empty() ? outDir : dirNameOf(outPath);
        std::size_t removed = 0u;
        Memory::String cleanupError;
        if (!clearAotExport(dir, &removed, &cleanupError))
        {
            std::cerr << "清理失败：" << cleanupError << "\n";
            return 1;
        }
        std::cout << "清理完成，共删除 " << removed << " 个 AOT 中间文件\n";
        return 0;
    }
    if (quadPath.empty())
    {
        std::cerr << "至少要给 --quad FILE\n";
        printUsage(self);
        return 2;
    }
    if (outPath.empty() && outDir.empty())
    {
        if (targetKind == "cpp-aot")
        {
            std::filesystem::path exe(quadPath);
            exe.replace_extension();
            outPath = exe.string();
        }

    }

    /* 2. 虚拟机 / 大整数的源码（整段内嵌）。 */
    Memory::String vmHpp;
    Memory::String vmCpp;
    Memory::String vmGlueHpp;
    Memory::String vmGlueCpp;
    Memory::String bigintHpp;
    Memory::String bigintCpp;
    Memory::String bigfloatHpp;
    Memory::String bigfloatCpp;
    Memory::String magicGlueHpp;
    Memory::String regexGlueHpp;
    Memory::String standardGlue;
    Memory::String runtimeAllocatorHpp;   ///< C 分配器与 STL 适配器源码。
    Memory::String userGlue;
    const Memory::String vmHppPath = vmDir + "/stackvm.hpp";
    const Memory::String vmCppPath = vmDir + "/stackvm.cpp";
    const Memory::String vmGlueHppPath = vmDir + "/stackvm_glue.hpp";
    const Memory::String vmGlueCppPath = vmDir + "/stackvm_glue.cpp";
    const Memory::String bigintHppPath = bigintDir + "/bigint.hpp";
    const Memory::String bigintCppPath = bigintDir + "/bigint.cpp";
    const Memory::String bigfloatHppPath = bigintDir + "/bigfloat.hpp";
    const Memory::String bigfloatCppPath = bigintDir + "/bigfloat.cpp";
    if (!readWholeFile(vmHppPath, &vmHpp) || !readWholeFile(vmCppPath, &vmCpp) ||
        !readWholeFile(vmGlueHppPath, &vmGlueHpp) ||
        !readWholeFile(vmGlueCppPath, &vmGlueCpp))
    {
        std::cerr << "读不到虚拟机源码（" << vmHppPath << " / " << vmCppPath
                  << "）；用 --vm-dir 指一下\n";
        return 2;
    }
    /* stackvm.cpp 按职责拆成了 parts/ 下的 .cpp：内嵌时展开成单文件。 */
    inlineLocalIncludes(&vmCpp, vmDir, "parts/");
    inlineLocalIncludes(&vmCpp, vmDir, "value_text.hpp");
    inlineLocalIncludes(&vmCpp, vmDir + "/parts", "machine_value_text.cpp");
    inlineLocalIncludes(&vmCpp, vmDir, "execution_plan.hpp");
    inlineLocalIncludes(&vmCpp, vmDir, "register_core.hpp");
    inlineLocalIncludes(&vmCpp, vmDir, "allocation_index.hpp");
    inlineLocalIncludes(&vmCpp, vmDir, "data_capacity_index.hpp");
    inlineLocalIncludes(&vmCpp, vmDir + "/parts", "optimization_flow.inc");
    inlineLocalIncludes(&vmCpp, vmDir + "/parts", "execution_fusion.inc");
    inlineLocalIncludes(&vmCpp, vmDir + "/parts", "register_execution.inc");
    stripLocalInclude(&vmHpp, "stackvm_glue.hpp");
    Memory::String reflectionHpp;
    if (!readWholeFile(vmDir + "/reflection.hpp", &reflectionHpp)) {
        std::cerr << "读不到反射元数据头；用 --vm-dir 指一下\n"; return 2;
    }
    stripLocalInclude(&reflectionHpp, "../../source/allocator/magel_allocator.hpp");
    stripLocalInclude(&vmHpp, "reflection.hpp");
    vmHpp = reflectionHpp + "\n" + vmHpp;
    stripLocalInclude(&vmGlueCpp, "stackvm_glue.hpp");
    const bool withBigInt = readWholeFile(bigintHppPath, &bigintHpp) &&
                            readWholeFile(bigintCppPath, &bigintCpp);
    if (withBigInt)
    {
        stripLocalInclude(&bigintCpp, "bigint.hpp");
        stripLocalInclude(&vmCpp, "bigint.hpp");
    }
    else
    {
        std::cerr << "提示：读不到 " << bigintHppPath << "，生成物里不会有 `bint` 的后端"
                     "（程序用到 bint 的话会在编译期缺 bigint.hpp）\n";
    }
    const bool withBigFloat = withBigInt && readWholeFile(bigfloatHppPath, &bigfloatHpp) &&
                              readWholeFile(bigfloatCppPath, &bigfloatCpp);
    if (withBigFloat)
    {
        stripLocalInclude(&bigfloatHpp, "bigint.hpp");
        stripLocalInclude(&bigfloatCpp, "bigfloat.hpp");
        stripLocalInclude(&vmCpp, "bigfloat.hpp");
    }
    else
    {
        std::cerr << "提示：读不到大浮点源码，生成物里不会有 `bfloat` 后端\n";
    }
    stripLocalInclude(&vmCpp, "stackvm.hpp");
    if (!readWholeFile(mcDir + "/source/magel_glue.hpp", &magicGlueHpp))
    {
        std::cerr << "读不到 Magel 胶水源码；用 --magel-dir 指一下\n";
        return 2;
    }
    {
        stripLocalInclude(&magicGlueHpp, "stackvm.hpp");
        Memory::String genericNameHpp;
        if (!readWholeFile(mcDir + "/source/magel_generic_name.hpp", &genericNameHpp)) {
            std::cerr << "读不到泛型实例符号源码；用 --magel-dir 指一下\n";
            return 2;
        }
        stripLocalInclude(&magicGlueHpp, "magel_generic_name.hpp");
        magicGlueHpp = genericNameHpp + "\n" + magicGlueHpp;
        /* 标准胶水是手写 C++（`glue/std_glue.cpp`）：原样内嵌，不再经过 DSL。 */
        if (!readWholeFile(mcDir + "/source/glue/std_glue.cpp", &standardGlue))
        {
            std::cerr << "读不到标准胶水 magel/source/glue/std_glue.cpp；用 --magel-dir 指一下\n";
            return 2;
        }
        if (!readWholeFile(mcDir + "/source/glue/std_regex.hpp", &regexGlueHpp))
        {
            std::cerr << "读不到 Magel 正则胶水源码；用 --magel-dir 指一下\n";
            return 2;
        }
        stripLocalInclude(&regexGlueHpp, "magel_glue.hpp");
        stripLocalInclude(&regexGlueHpp, "../magel_glue.hpp");
        Memory::String mathGlueHpp, randomGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/std_math.hpp", &mathGlueHpp) ||
            !readWholeFile(mcDir + "/source/glue/std_random.hpp", &randomGlueHpp)) return 2;
        stripLocalInclude(&mathGlueHpp, "magel_glue.hpp");
        stripLocalInclude(&mathGlueHpp, "../magel_glue.hpp");
        stripLocalInclude(&randomGlueHpp, "magel_glue.hpp");
        stripLocalInclude(&randomGlueHpp, "../magel_glue.hpp");
        Memory::String signalGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/std_signal.hpp", &signalGlueHpp)) return 2;
        stripLocalInclude(&signalGlueHpp, "../magel_glue.hpp");
        stripLocalInclude(&signalGlueHpp, "magel_glue.hpp");
        Memory::String numericGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/lang_numeric.hpp", &numericGlueHpp)) return 2;
        stripLocalInclude(&numericGlueHpp, "../magel_glue.hpp");
        Memory::String structuredGlueHpp, jsonGlueHpp, tomlGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/structured_data.hpp", &structuredGlueHpp)) return 2;
        stripLocalInclude(&structuredGlueHpp, "../magel_glue.hpp");
        const auto loadDataGlue = [&](const char *format, Memory::String *output) {
            const Memory::String name = format;
            Memory::String header, implementation, glue;
            if (!readWholeFile(mcDir + "/library/" + name + "/" + name + ".hpp", &header) ||
                !readWholeFile(mcDir + "/library/" + name + "/" + name + ".cpp", &implementation) ||
                !readWholeFile(mcDir + "/source/glue/std_" + name + ".hpp", &glue)) return false;
            stripLocalInclude(&implementation, (name + ".hpp").c_str());
            stripLocalInclude(&glue, ("../../library/" + name + "/" + name + ".hpp").c_str());
            stripLocalInclude(&glue, ("../../library/" + name + "/" + name + ".cpp").c_str());
            stripLocalInclude(&glue, "structured_data.hpp");
            *output = header + "\n" + implementation + "\n" + glue;
            return true;
        };
        if (!loadDataGlue("json", &jsonGlueHpp) || !loadDataGlue("toml", &tomlGlueHpp)) return 2;
        Memory::String binaryGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/std_binary.hpp", &binaryGlueHpp)) return 2;
        stripLocalInclude(&binaryGlueHpp, "../magel_glue.hpp");
        /* std::compress 依赖 lztext 库，所以连库源码一起内嵌：先头文件和实现，再胶水，
         * 三者同置于 GLUE_NO_STD_COMPRESS 内，未引用该模块时整段不进产物。 */
        Memory::String lztextHeader, lztextImpl, compressGlueHpp;
        if (!readWholeFile(mcDir + "/library/lztext/lztext.hpp", &lztextHeader) ||
            !readWholeFile(mcDir + "/library/lztext/lztext.cpp", &lztextImpl) ||
            !readWholeFile(mcDir + "/source/glue/std_compress.hpp", &compressGlueHpp)) return 2;
        stripLocalInclude(&lztextImpl, "lztext.hpp");
        stripLocalInclude(&compressGlueHpp, "../../library/lztext/lztext.hpp");
        stripLocalInclude(&compressGlueHpp, "../magel_glue.hpp");
        compressGlueHpp = lztextHeader + "\n" + lztextImpl + "\n" + compressGlueHpp;
        Memory::String linalgGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/std_linalg.hpp", &linalgGlueHpp)) return 2;
        stripLocalInclude(&linalgGlueHpp, "../magel_glue.hpp");
        stripLocalInclude(&linalgGlueHpp, "magel_glue.hpp");
        Memory::String filesystemGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/std_filesystem.hpp", &filesystemGlueHpp)) return 2;
        stripLocalInclude(&filesystemGlueHpp, "../magel_glue.hpp");
        Memory::String parseGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/std_parse.hpp", &parseGlueHpp)) return 2;
        stripLocalInclude(&parseGlueHpp, "../magel_glue.hpp");
        stripLocalInclude(&parseGlueHpp, "magel_glue.hpp");
        Memory::String processGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/std_process.hpp", &processGlueHpp)) return 2;
        stripLocalInclude(&processGlueHpp, "../magel_glue.hpp");
        stripLocalInclude(&processGlueHpp, "magel_glue.hpp");
        Memory::String sqlGlueHpp, netGlueHpp, imageGlueHpp, treeGlueHpp, graphGlueHpp,
            linkedListGlueHpp, kmpGlueHpp;
        if (!readWholeFile(mcDir + "/source/glue/std_sql.hpp", &sqlGlueHpp) ||
            !readWholeFile(mcDir + "/source/glue/std_net.hpp", &netGlueHpp) ||
            !readWholeFile(mcDir + "/source/glue/std_image.hpp", &imageGlueHpp) ||
            !readWholeFile(mcDir + "/source/glue/std_tree.hpp", &treeGlueHpp) ||
            !readWholeFile(mcDir + "/source/glue/std_graph.hpp", &graphGlueHpp) ||
            !readWholeFile(mcDir + "/source/glue/std_linkedlist.hpp", &linkedListGlueHpp) ||
            !readWholeFile(mcDir + "/source/glue/std_kmp.hpp", &kmpGlueHpp)) return 2;
        stripLocalInclude(&imageGlueHpp, "../magel_glue.hpp");
        stripLocalInclude(&imageGlueHpp, "magel_glue.hpp");
        for (Memory::String *text : {&treeGlueHpp, &graphGlueHpp, &linkedListGlueHpp, &kmpGlueHpp}) {
            stripLocalInclude(text, "../magel_glue.hpp");
            stripLocalInclude(text, "magel_glue.hpp");
        }
        Memory::String sqliteHeader;
        if (!readWholeFile(mcDir + "/library/sqlite/sqlite3.h", &sqliteHeader)) return 2;
        stripLocalInclude(&sqlGlueHpp, "../../library/sqlite/sqlite3.h");
        sqlGlueHpp = sqliteHeader + "\n" + sqlGlueHpp;
        for (Memory::String *text : {&sqlGlueHpp, &netGlueHpp}) {
            stripLocalInclude(text, "../magel_glue.hpp");
            stripLocalInclude(text, "magel_glue.hpp");
        }
        // 展开的头文件也必须置于模块宏内，避免只移除注册却保留模块实现。
        const auto guardGlue = [](Memory::String &text, const char *macro) {
            text = Memory::String("#ifndef ") + macro + "\n" + text + "\n#endif\n";
        };
        guardGlue(mathGlueHpp, "GLUE_NO_STD_MATH");
        guardGlue(randomGlueHpp, "GLUE_NO_STD_RANDOM");
        guardGlue(regexGlueHpp, "GLUE_NO_STD_REGEX");
        guardGlue(signalGlueHpp, "GLUE_NO_STD_SIGNAL");
        guardGlue(jsonGlueHpp, "GLUE_NO_STD_JSON");
        guardGlue(tomlGlueHpp, "GLUE_NO_STD_TOML");
        structuredGlueHpp = "#if !defined(GLUE_NO_STD_JSON) || !defined(GLUE_NO_STD_TOML)\n" + structuredGlueHpp + "\n#endif\n";
        guardGlue(binaryGlueHpp, "GLUE_NO_STD_BINARY");
        guardGlue(compressGlueHpp, "GLUE_NO_STD_COMPRESS");
        guardGlue(linalgGlueHpp, "GLUE_NO_STD_LINALG");
        guardGlue(filesystemGlueHpp, "GLUE_NO_STD_FILESYSTEM");
        guardGlue(parseGlueHpp, "GLUE_NO_STD_PARSE");
        guardGlue(processGlueHpp, "GLUE_NO_STD_PROCESS");
        guardGlue(sqlGlueHpp, "GLUE_NO_STD_SQL");
        guardGlue(netGlueHpp, "GLUE_NO_STD_NET");
        guardGlue(imageGlueHpp, "GLUE_NO_STD_IMAGE");
        guardGlue(treeGlueHpp, "GLUE_NO_STD_TREE");
        guardGlue(graphGlueHpp, "GLUE_NO_STD_GRAPH");
        guardGlue(linkedListGlueHpp, "GLUE_NO_STD_LINKEDLIST");
        guardGlue(kmpGlueHpp, "GLUE_NO_STD_KMP");
        standardGlue = structuredGlueHpp + "\n" + jsonGlueHpp + "\n" + tomlGlueHpp + "\n" + numericGlueHpp + "\n" + mathGlueHpp + "\n" + randomGlueHpp + "\n" + regexGlueHpp + "\n" +
                       signalGlueHpp + "\n" +
                       binaryGlueHpp + "\n" + compressGlueHpp + "\n" + linalgGlueHpp + "\n" +
                       parseGlueHpp + "\n" + processGlueHpp + "\n" + sqlGlueHpp + "\n" +
                       netGlueHpp + "\n" + imageGlueHpp + "\n" + treeGlueHpp + "\n" +
                       graphGlueHpp + "\n" + linkedListGlueHpp + "\n" + kmpGlueHpp + "\n" +
                       filesystemGlueHpp + "\n" + standardGlue;
        /* C 内存池与显式 STL allocator，一起内嵌进产物。 */
        if (!readWholeFile(mcDir + "/source/runtime_allocator.hpp", &runtimeAllocatorHpp))
        {
            std::cerr << "读不到整机分配器 magel/runtime_allocator.hpp；用 --magel-dir 指一下\n";
            return 2;
        }
    }
    {
        Memory::String poolHeader, poolSource, adapter, i18nHeader, consoleHeader;
        if (!readWholeFile(mcDir + "/source/magel_i18n.hpp", &i18nHeader)) return 2;
        if (!readWholeFile(mcDir + "/library/console/utf8.hpp", &consoleHeader)) return 2;
        stripLocalInclude(&i18nHeader, "console/utf8.hpp");
        i18nHeader = consoleHeader + "\n" + i18nHeader;
        const Memory::String allocatorDir = mcDir + "/source/allocator/";
        if (!readWholeFile(allocatorDir + "magel_pool.h", &poolHeader) ||
            !readWholeFile(allocatorDir + "magel_pool.c", &poolSource) ||
            !readWholeFile(allocatorDir + "magel_allocator.hpp", &adapter)) {
            std::cerr << "读不到 C 分配器与 STL allocator 适配器\n";
            return 2;
        }
        stripLocalInclude(&poolSource, "magel_pool.h");
        stripLocalInclude(&adapter, "magel_pool.h");
        stripLocalInclude(&adapter, "magel_pool.c");
        stripLocalInclude(&runtimeAllocatorHpp, "allocator/magel_allocator.hpp");
        stripLocalInclude(&runtimeAllocatorHpp, "magel_i18n.hpp");
        stripLocalInclude(&vmHpp, "../../source/magel_i18n.hpp");
        runtimeAllocatorHpp = i18nHeader + "\n#define MAGEL_CUSTOM_ALLOCATOR 1\n" + poolHeader + "\n" + poolSource + "\n" + adapter + "\n" + runtimeAllocatorHpp;
        for (Memory::String *text : {&vmCpp, &vmHpp, &vmGlueHpp, &magicGlueHpp, &regexGlueHpp, &standardGlue, &bigintHpp, &bigfloatHpp}) {
            stripLocalInclude(text, "../allocator/magel_allocator.hpp");
            stripLocalInclude(text, "allocator/magel_allocator.hpp");
            stripLocalInclude(text, "../../source/allocator/magel_allocator.hpp");
        }
    }
    if (!gluePath.empty() && !readWholeFile(gluePath, &userGlue))
    {
        std::cerr << "读不到应用胶水：" << gluePath << "\n";
        return 2;
    }
    {
        for (const char *header : {"magel_glue.hpp", "stackvm.hpp",
                                   "stackvm_glue.hpp"})
            stripLocalInclude(&userGlue, header);
    }

    /* 3. 四元式在生成期解析、化简、组装；产物里只留下 VM 指令与程序元数据。 */
    Memory::String quadText;
    if (!readWholeFile(quadPath, &quadText))
    {
        std::cerr << "打不开四元式文件：" << quadPath << "\n";
        return 2;
    }
    /* 按 import 集合裁剪标准胶水：未用到的模块定义对应 GLUE_NO_XXX，
     * 让 std_registry / std_glue.cpp 里那段的注册整段不编进来。 */
    Memory::String glueDefines;
    glueDefines += "#undef STACKVM_WITH_ALLOCATION_GUARDS\n#define STACKVM_WITH_ALLOCATION_GUARDS ";
    glueDefines += unchecked ? "0\n" : "1\n";
    if (unchecked || hidePanicDetails) glueDefines += "#define NO_MAGEL_PANIC_TRACE 1\n";
    if (hidePanicDetails) { stripDebug = true; aotDebug = false; }
    const auto noStd = [&](const char *macro, bool used) {
        if (!used) { glueDefines += "#define "; glueDefines += macro; glueDefines += " 1\n"; }
    };
    noStd("GLUE_NO_STD_MATH", quadText.find("std::math::") != Memory::String::npos || quadText.find("std::clock::") != Memory::String::npos);
    noStd("GLUE_NO_STD_RANDOM", quadText.find("std::random::") != Memory::String::npos);
    noStd("GLUE_NO_STD_REGEX", quadText.find("std::regex::") != Memory::String::npos);
    noStd("GLUE_NO_STD_SIGNAL", quadText.find("std::signal::") != Memory::String::npos);
    noStd("GLUE_NO_STD_JSON", quadText.find("std::json::") != Memory::String::npos);
    noStd("GLUE_NO_STD_TOML", quadText.find("std::toml::") != Memory::String::npos);
    noStd("GLUE_NO_STD_BINARY", quadText.find("std::binary::") != Memory::String::npos);
    noStd("GLUE_NO_STD_COMPRESS", quadText.find("std::compress::") != Memory::String::npos);
    noStd("GLUE_NO_STD_LINALG", quadText.find("std::linalg::") != Memory::String::npos);
    noStd("GLUE_NO_STD_PARSE", quadText.find("std::parse::") != Memory::String::npos);
    noStd("GLUE_NO_STD_PROCESS", quadText.find("std::process::") != Memory::String::npos);
    noStd("GLUE_NO_STD_SQL", quadText.find("std::sql::") != Memory::String::npos);
    noStd("GLUE_NO_STD_FILESYSTEM", quadText.find("std::io::__") != Memory::String::npos || quadText.find("std::datetime::__utcNow") != Memory::String::npos);
    noStd("GLUE_NO_STD_NET", quadText.find("std::net::") != Memory::String::npos);
    noStd("GLUE_NO_STD_IMAGE", quadText.find("std::image::") != Memory::String::npos);
    noStd("GLUE_NO_STD_TREE", quadText.find("std::tree::") != Memory::String::npos);
    noStd("GLUE_NO_STD_GRAPH", quadText.find("std::graph::") != Memory::String::npos);
    noStd("GLUE_NO_STD_LINKEDLIST", quadText.find("std::linkedlist::") != Memory::String::npos);
    noStd("GLUE_NO_STD_KMP", quadText.find("std::text::") != Memory::String::npos);

    Memory::String abiGlue;
    std::filesystem::path abiPath(quadPath);
    abiPath.replace_extension(".abi.cpp");
    if (std::filesystem::exists(abiPath) &&
        !readWholeFile(abiPath.string(), &abiGlue))
    {
        std::cerr << "读不到 MAGEL ABI 绑定：" << abiPath.string() << "\n";
        return 2;
    }
    stackvm::QuadFile quadFile = stackvm::parseQuads(quadText, quadPath);
    for (const Memory::String &diagnostic : quadFile.diagnostics)
    {
        std::cerr << "四元式：" << diagnostic << "\n";
    }
    if (!quadFile.ok()) return 1;
    const Memory::String metadataName = quadText.compare(0,4,"MGQI") == 0 ? Memory::String() : programNameMetadata(quadText);
    const Memory::String programName = !nameOverride.empty()
                                        ? nameOverride
                                        : (!quadFile.name.empty()
                                               ? quadFile.name
                                               : (metadataName.empty() ? baseNameOf(quadPath)
                                                                       : metadataName));
    quadFile.name = programName;
    if (!gluePath.empty() && std::filesystem::path(gluePath).extension() == ".mcpp")
    {
        std::cerr << "不支持的胶水文件格式：" << gluePath << "\n";
        return 2;
    }
    if (!outDir.empty())
        outPath = (std::filesystem::path(outDir) /
                   baseNameOf(quadPath)).string();

    stackvm::OptimizationReport optimization;
    if (optimizeQuads) optimization = stackvm::optimize(quadFile);

    stackvm::Program program;
    if (!stackvm::assemble(quadFile, program))
    {
        for (const Memory::String &diagnostic : program.diagnostics)
        {
            std::cerr << "组装：" << diagnostic << "\n";
        }
        return 1;
    }

    // 将产物所需能力固化到内嵌运行时，并拒绝外部宏覆盖为不兼容配置。
    const bool requiresReflection = (program.requiredFeatures & stackvm::kFeatureReflection) != 0;
    glueDefines += "\n#ifndef STACKVM_WITH_REFLECTION\n#define STACKVM_WITH_REFLECTION " +
        Memory::String(requiresReflection ? "1" : "0") + "\n#endif\n";
    if (requiresReflection)
        glueDefines += "#if !STACKVM_WITH_REFLECTION\n#error AOT reflection feature configuration mismatch\n#endif\n";

    Memory::String dataBytes;
    if (!dataPath.empty() && !readWholeFile(dataPath, &dataBytes))
    {
        std::cerr << "打不开数据区文件：" << dataPath << "（没有就省掉 --data）\n";
        return 2;
    }
    Memory::Vector<magel_emit::DataSymbolRow> dataSymbols;
    Memory::String layoutText;
    if (!layoutPath.empty())
    {
        if (!readWholeFile(layoutPath, &layoutText))
        {
            std::cerr << "打不开布局文件：" << layoutPath << "（没有就省掉 --layout）\n";
            return 2;
        }
        Memory::Vector<stackvm::LayoutRow> rows;
        if (!stackvm::decodeDataLayout(layoutText, rows)) {
            std::cerr << "布局文件损坏或版本不受支持\n"; return 2;
        }
        for (const auto &item : rows) {
            magel_emit::DataSymbolRow row;
            row.name = cppString(item.name); row.offset = item.offset;
            row.size = item.size; row.stride = item.stride;
            row.hasSize = item.hasSize; row.hasStride = item.hasStride;
            dataSymbols.push_back(std::move(row));
        }
    }


    if ((separateModules || !moduleCache.empty()) && targetKind != "cpp-aot") {
        std::cerr << "模块独立编译只支持 --target cpp-aot\n"; return 2;
    }
    if (!moduleCache.empty() && !separateModules) {
        std::cerr << "--module-cache 需要 --separate-modules\n"; return 2;
    }
    /* ---- `--target cpp-aot`：翻译成基本块 + goto 的 C++，编译并（默认）清理中间文件 ---- */
    if (targetKind == "cpp-aot")
    {
        Memory::String optFlag;
        Memory::String optError;
        if (!aotOptFlag(optLevelText, &optFlag, &optError))
        {
            std::cerr << optError << "\n";
            return 2;
        }

        if (separateModules && !makeProgram.empty()) {
            std::cerr << "--separate-modules 使用内置模块构建驱动，不能与 --make 混用\n";
            return 2;
        }
        magel_emit::AotRequest request;
        request.declarationSources = glueDefines + "\n" + runtimeAllocatorHpp + "\n" + vmGlueHpp + "\n" + vmHpp + "\n";

        request.embeddedSources = glueDefines + "\n" + runtimeAllocatorHpp + "\n" + vmGlueHpp + "\n" +
                                  vmHpp + "\n" + magicGlueHpp + "\n" + standardGlue + "\n" +
                                  bigintHpp + "\n" + bigintCpp + "\n" +
                                  bigfloatHpp + "\n" + bigfloatCpp + "\n" + vmGlueCpp + "\n" + vmCpp +
                                  "\n" + abiGlue + "\n" + userGlue + "\n";
        request.programName = programName;
        request.dataBytes = dataBytes;
        request.hasAbi = !abiGlue.empty();
        request.hasUser = !userGlue.empty();
        request.threads = threadsEnabled;
        request.hasStackLimit = stackSlotsSet;
        request.stackSlots = stackSlots;
        request.debugInfo = aotDebug;
        request.runtimeArena = (runtimeArenaBytes > 0) ? runtimeArenaBytes : 536870912;
        request.quads = &quadFile;
        request.program = &program;
        for (const magel_emit::DataSymbolRow &row : dataSymbols)
        {
            magel_emit::AotDataSymbol symbol;
            symbol.name = row.name;
            symbol.offset = row.offset;
            symbol.size = row.size;
            symbol.stride = row.stride;
            symbol.hasSize = row.hasSize;
            symbol.hasStride = row.hasStride;
            request.symbols.push_back(std::move(symbol));
        }

        Memory::String exportsSource, exportsHeader;
        auto exportsPath = std::filesystem::path(quadPath); exportsPath.replace_extension(".exports.cpp");
        if (std::filesystem::exists(exportsPath))
        {
            if (!readWholeFile(exportsPath.string(), &exportsSource)) return 1;
            std::istringstream entries(exportsSource.c_str());
            std::string entry;
            while (std::getline(entries, entry))
                if (entry.rfind("// magel-c-entry ", 0) == 0)
                    request.cExportEntries.push_back(Memory::String(entry.substr(17).c_str()));
        }
        Memory::Vector<Memory::String> aotDiagnostics;
        magel_emit::AotModules modules;
        Memory::String source = magel_emit::generateAotFile(request, &aotDiagnostics, separateModules ? &modules : nullptr);
        if (source.empty())
        {
            for (const Memory::String &diagnostic : aotDiagnostics)
                std::cerr << "AOT：" << diagnostic << "\n";
            return 1;
        }

        if (std::filesystem::exists(exportsPath))
        {
            if (!readWholeFile(exportsPath.string(), &exportsSource)) return 1;
            exportsPath.replace_extension(".h");
            if (!readWholeFile(exportsPath.string(), &exportsHeader)) return 1;
            source += "\n#define MAGEL_C_ARENA_DEFAULT " + Memory::toString(request.runtimeArena) + "\n";
            if (request.hasAbi) source += "#define MAGEL_C_HAS_ABI 1\n";
            if (request.hasUser) source += "#define MAGEL_C_HAS_USER 1\n";
            source += exportsSource;
        }
        Memory::String exePath = outPath;
        if (!executeOutput.empty())
        {
            exePath = executeOutput;
        }
        else if (!outDir.empty())
        {
            exePath = (std::filesystem::path(outDir) / baseNameOf(quadPath)).string();
        }
        const std::filesystem::path exeAbsolute =
            std::filesystem::absolute(exePath).lexically_normal();
        // 库模式既可以显式指定，也可以由最终产物扩展名选择。
        auto extension = exeAbsolute.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        if (extension == ".lib" || extension == ".a") {
            std::cerr << "AOT 不支持静态库输出；请使用 .dll、.so 或 .o\n"; return 2;
        }
        if ((sharedLibrary && (objectLibrary || extension == ".o")) ||
            (objectLibrary && (extension == ".dll" || extension == ".so"))) {
            std::cerr << "--shared、--object 与输出扩展名冲突\n"; return 2;
        }
        sharedLibrary = sharedLibrary || extension == ".dll" || extension == ".so";
        objectLibrary = objectLibrary || extension == ".o";
        const bool libraryOutput = sharedLibrary || objectLibrary;
        if (libraryOutput && request.cExportEntries.empty()) {
            std::cerr << "AOT 库输出需要至少一个 export 函数声明，可以省略 main\n"; return 1;
        }
        std::filesystem::path exeImage = exeAbsolute;
        const std::filesystem::path root = exeAbsolute.parent_path();
        const Memory::String exeName = exeAbsolute.filename().string();
        std::error_code directoryError;
        std::filesystem::create_directories(root, directoryError);
        if (directoryError)
        {
            std::cerr << "无法创建输出目录：" << directoryError.message() << "\n";
            return 1;
        }

        const Memory::String sourceName = exeName + ".aot.cpp";
#ifdef _WIN32
        const Memory::String objectName = exeName + ".aot.obj";
#else
        const Memory::String objectName = exeName + ".aot.o";
#endif
        const Memory::String logName = exeName + ".aot.log";
        /* `cpp_output` 指定的目录只影响生成的 C++ 源码；可执行文件、目标文件与
         * 编译日志仍在 exe 同目录。缺省不导出代码，源码只是编译用的临时文件。 */
        std::filesystem::path cppRoot = root;
        if (cppOutput && !cppOutputDir.empty())
        {
            cppRoot = std::filesystem::absolute(cppOutputDir).lexically_normal();
            std::error_code cppDirError;
            std::filesystem::create_directories(cppRoot, cppDirError);
            if (cppDirError)
            {
                std::cerr << "无法创建 C++ 输出目录：" << cppDirError.message() << "\n";
                return 1;
            }
        }
        const std::filesystem::path sourcePath = cppRoot / sourceName;
        const std::filesystem::path objectPath = root / objectName;
        const std::filesystem::path logPath = root / logName;
        {
            std::ofstream out(sourcePath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                std::cerr << "写不了 AOT 源文件：" << sourcePath.string() << "\n";
                return 1;
            }
            out << source;
        }

        /* 工具链：`--cxx` > `$CXX` > `g++`；`--cc` > `$CC` > `gcc`；`--make` 才走 make。 */
        if (!gccToolchain.empty())
        {
            std::filesystem::path selected = Memory::u8path(gccToolchain);
            std::error_code toolError;
            if (std::filesystem::is_directory(selected, toolError))
            {
                if (std::filesystem::is_directory(selected / "bin", toolError)) selected /= "bin";
#ifdef _WIN32
                const auto cxx = selected / "g++.exe";
                const auto cc = selected / "gcc.exe";
#else
                const auto cxx = selected / "g++";
                const auto cc = selected / "gcc";
#endif
                if (!std::filesystem::is_regular_file(cxx, toolError) || !std::filesystem::is_regular_file(cc, toolError))
                { std::cerr << "GCC 工具链目录缺少 g++ 或 gcc：" << gccToolchain << "\n"; return 1; }
                if (cxxProgram.empty()) cxxProgram = cxx.u8string();
                if (ccProgram.empty()) ccProgram = cc.u8string();
            }
            else
            {
                Memory::String command = gccToolchain;
                const auto name = selected.filename().string();
                if (name == "gcc" || name == "gcc.exe" || name.rfind("gcc-", 0) == 0 || name.find("-gcc") != std::string::npos)
                {
                    if (ccProgram.empty()) ccProgram = command;
                    const auto at = command.rfind("gcc"); command.replace(at, 3, "g++");
                }
                if (ccProgram.empty() && name.find("g++") != std::string::npos)
                {
                    auto cc = gccToolchain; cc.replace(cc.rfind("g++"), 3, "gcc"); ccProgram = cc;
                }
                if (cxxProgram.empty()) cxxProgram = command;
            }
        }
        const char *cxxEnvironment = std::getenv("CXX");
        const char *ccEnvironment = std::getenv("CC");
        const Memory::String compiler =
            !cxxProgram.empty()
                ? cxxProgram
                : ((cxxEnvironment != nullptr && *cxxEnvironment != '\0') ? cxxEnvironment
                                                                          : "g++");
        const Memory::String cCompiler =
            !ccProgram.empty()
                ? ccProgram
                : ((ccEnvironment != nullptr && *ccEnvironment != '\0') ? ccEnvironment : "gcc");
#ifdef _WIN32
        if (compiler.find_first_of("\"%!?\r\n") != Memory::String::npos || cCompiler.find_first_of("\"%!?\r\n") != Memory::String::npos)
        { std::cerr << "工具链值必须是单个可执行文件路径或命令名\n"; return 1; }
#endif
        Memory::String targetTriple;
        if (!runShellCommand(executableCommand(compiler) + " -dumpmachine", &targetTriple) || targetTriple.empty())
        { std::cerr << "无法执行指定 GCC C++ 工具链：" << compiler << "\n" << targetTriple; return 1; }
        if (targetTriple.find("none-eabi") != Memory::String::npos)
        { std::cerr << "当前 AOT 宿主运行时不支持裸机 GCC 目标；板端使用独立 MCU VM\n"; return 1; }
        const bool windowsTarget = targetTriple.find("mingw") != Memory::String::npos || targetTriple.find("w64") != Memory::String::npos;
        if ((extension == ".dll" && !windowsTarget) ||
            (extension == ".so" && targetTriple.find("linux") == Memory::String::npos)) {
            std::cerr << "库扩展名与 GCC 目标不匹配：" << targetTriple
                      << "；.dll 需要 Windows 工具链，.so 需要 Linux 工具链\n"; return 2;
        }
        if (!exeImage.has_extension()) {
            if (objectLibrary) exeImage += ".o";
            else if (sharedLibrary) exeImage += windowsTarget ? ".dll" : ".so";
            else if (windowsTarget) exeImage += ".exe";
        }
        if (!exportsHeader.empty()) {
            auto headerPath = exeImage; headerPath.replace_extension(".h");
            std::ofstream header(headerPath, std::ios::binary); header << exportsHeader;
            if (!header) { std::cerr << "无法写入 C 导出头文件\n"; return 1; }
        }
        const bool wantDebug = aotDebug;
        const Memory::String debugFlag = wantDebug ? "-g" : "-g0";
        Memory::String objectFlags;
        if (windowsTarget) objectFlags = " -Wa,-mbig-obj";
        if (libraryOutput) objectFlags += " -DMAGEL_NO_MAIN -fPIC";
        if (objectLibrary) objectFlags += " -DMAGEL_C_OBJECT";
        Memory::String linkExtra;
        if (sharedLibrary) linkExtra += " -shared";
        if (objectLibrary) linkExtra += " -r -nostdlib -no-pie";
        if (!objectLibrary && targetTriple.find("linux") != Memory::String::npos) linkExtra += " -ldl";
        // 可重定位文件保留供宿主链接的符号，--release 也不能 strip。
        if (aotRelease && !aotDebug && !objectLibrary) linkExtra += " -s";
        // SQLite 必须按 C 编译；按 SQL 引用生成独立 C 目标，最终静态链接。
        const bool sqliteUsed = quadText.find("std::sql::") != Memory::String::npos;
        const Memory::String sqliteSourceName = exeName + ".sqlite3.c";
        const Memory::String sqliteObjectName = exeName + ".sqlite3.o";
        const auto sqliteSourcePath = cppRoot / sqliteSourceName;
        const auto sqliteObjectPath = root / sqliteObjectName;
        if (sqliteUsed) {
            Memory::String sqliteSource;
            if (!readWholeFile(mcDir + "/library/sqlite/sqlite3.c", &sqliteSource)) {
                std::cerr << "读不到内置 SQLite 源码\n"; return 2;
            }
            {
                std::ofstream sqliteOut(sqliteSourcePath, std::ios::binary | std::ios::trunc);
                if (!sqliteOut || !(sqliteOut << sqliteSource)) {
                    std::cerr << "无法写入 SQLite 编译源码\n"; return 1;
                }
            }
            const Memory::String sqliteCommand = executableCommand(cCompiler) + " -std=c11 " + optFlag +
                (libraryOutput ? " -fPIC" : "") + " -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION=1 -c \"" + sqliteSourcePath.string() +
                "\" -o \"" + sqliteObjectPath.string() + "\"";
            Memory::String sqliteOutput;
            if (!runShellCommand(sqliteCommand, &sqliteOutput)) {
                std::cerr << "SQLite 静态编译失败：" << sqliteCommand << "\n" << sqliteOutput; return 1;
            }
            linkExtra += " \"" + sqliteObjectPath.string() + "\"";
        }

        if (separateModules) {
            Memory::String moduleError;
            const bool ok = buildAotModules(modules, source, exeImage, moduleCache, compiler,
                "-std=c++17 " + optFlag + " " + debugFlag + " -pthread" + objectFlags,
                optFlag + " " + debugFlag + " -pthread" + linkExtra, &moduleError);
            if (!aotKeepIntermediate && !cppOutput) { std::error_code cleanupError; std::filesystem::remove(sourcePath, cleanupError); }
            if (!ok) { std::cerr << moduleError << "\n"; return 1; }
            if (sqliteUsed && !aotKeepIntermediate) {
                std::error_code cleanupError;
                std::filesystem::remove(sqliteObjectPath, cleanupError);
                if (!cppOutput) std::filesystem::remove(sqliteSourcePath, cleanupError);
            }

            std::cout << "已生成 " << exeImage.string() << "（模块目标文件保留供增量构建）\n";
            return 0;
        }


        /* 构建脚本：`--make` 时生成 `<exe>.aot.mk` 并交给指定 make 驱动（脚本登记为
         * 中间文件）；缺省直接把编译 / 链接命令发给编译器，不落构建脚本。 */
        const Memory::String makefileName = exeName + ".aot.mk";
        const std::filesystem::path makefilePath = root / makefileName;
        Memory::String makefileText;
        if (!makeProgram.empty())
        {
            makefileText = "# magel-aot 构建脚本（由 magel-emit 生成；构建成功后按清单删除）\n";
            makefileText += "# make：" + makeProgram + "  CXX：" + compiler + "  CC：" + cCompiler +
                            "  优化级别：" + optFlag + "\n";
            makefileText += "CXX = " + compiler + "\n";
            makefileText += "CC = " + cCompiler + "\n";
            makefileText += "CXXFLAGS = -std=c++17 " + optFlag + " " + debugFlag + " -pthread" + objectFlags + "\n";
            makefileText += "LDFLAGS = " + optFlag + " " + debugFlag + " -pthread" + linkExtra + "\n";
            makefileText += "all: " + makeFilePath(exeImage.string()) + "\n\n";
            makefileText += makeFilePath(objectPath.string()) + ": " +
                            makeFilePath(sourcePath.string()) + "\n\t\"$(CXX)\" $(CXXFLAGS) -c \"" +
                            sourcePath.string() + "\" -o \"" + objectPath.string() + "\"\n\n";
            if (sqliteUsed) {
                makefileText += makeFilePath(sqliteObjectPath.string()) + ": " + makeFilePath(sqliteSourcePath.string()) +
                    "\n\t\"$(CC)\" -std=c11 " + optFlag +
                    (libraryOutput ? " -fPIC" : "") + " -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION=1 -c \"" + sqliteSourcePath.string() +
                    "\" -o \"" + sqliteObjectPath.string() + "\"\n\n";
            }
            makefileText += makeFilePath(exeImage.string()) + ": " +
                            makeFilePath(objectPath.string()) + (sqliteUsed ? " " + makeFilePath(sqliteObjectPath.string()) : Memory::String()) + "\n\t\"$(CXX)\" $(LDFLAGS) \"" +
                            objectPath.string() + "\" -o \"" + exeImage.string() + "\"\n";
        }

        Memory::Set<Memory::String> generated;
        /* 只有"没要求保留代码"时源码才算本次中间文件，才登记进清单；
         * `--cpp-output` 要的是交付物，永远不删。 */
        if (!cppOutput) generated.insert(sourceName);
        generated.insert(objectName);
        if (sqliteUsed) {
            if (!cppOutput) generated.insert(sqliteSourceName);
            generated.insert(sqliteObjectName);
        }
        generated.insert(logName);
        if (!makeProgram.empty()) generated.insert(makefileName);
        Memory::String manifestError;
        if (!writeAotManifest(root, generated, &manifestError))
        {
            std::cerr << manifestError << "\n";
            return 1;
        }
        if (!makeProgram.empty())
        {
            std::ofstream makefileOut(makefilePath, std::ios::binary | std::ios::trunc);
            if (!makefileOut)
            {
                std::cerr << "写不了构建脚本：" << makefilePath.string() << "\n";
                return 1;
            }
            makefileOut << makefileText;
        }

        Memory::String compileCommand = executableCommand(compiler) + " -std=c++17 " + optFlag + " " + debugFlag +
                                           " -pthread" + objectFlags + " -c \"" + sourcePath.string() + "\" -o \"" +
                                           objectPath.string() + "\"";
        Memory::String linkCommand = executableCommand(compiler) + " " + optFlag + " " + debugFlag + " -pthread \"" +
                                        objectPath.string() + "\" -o \"" + exeImage.string() + "\"" +
                                        linkExtra;
        Memory::String compileLog;
        Memory::String commandOutput;
        bool compiled = false;
        bool linked = false;
        if (makeProgram.empty())
        {
            compileLog = "编译命令：" + compileCommand + "\n链接命令：" + linkCommand +
                         "\n  CXX：" + compiler + "  CC：" + cCompiler + "\n";
            compiled = runShellCommand(compileCommand, &commandOutput);
            compileLog += "\n[编译输出]\n" + commandOutput;
            if (compiled)
            {
                linked = runShellCommand(linkCommand, &commandOutput);
                compileLog += "\n[链接输出]\n" + commandOutput;
            }
        }
        else
        {
            const Memory::String makeCommand = "\"" + makeProgram + "\" -f \"" +
                                                  makefilePath.string() + "\" CXX=\"" + compiler +
                                                  "\" CC=\"" + cCompiler + "\"";
            compileLog = "构建驱动：make（" + makeProgram + "）\n编译器：CXX=" + compiler +
                         "  CC=" + cCompiler + "\n优化级别：" + optFlag + "\n构建脚本：" +
                         makefilePath.string() + "\nmake 命令：" + makeCommand +
                         "\n\n[构建脚本内容]\n" + makefileText + "\n";
            compiled = runShellCommand(makeCommand, &commandOutput);
            compileLog += "\n[make 输出]\n" + commandOutput;
            linked = compiled;
        }
        {
            std::ofstream log(logPath, std::ios::binary | std::ios::trunc);
            log << compileLog;
        }

        if (!compiled || !linked)
        {
            std::cerr << "AOT 编译失败（中间文件已保留，便于定位）：\n";
            std::cerr << compileLog << "\n";
            std::cerr << MAGEL_TEXT("  生成源码：") << sourcePath.string() << "\n";
            std::cerr << MAGEL_TEXT("  目标文件：") << objectPath.string() << "\n";
            if (!makeProgram.empty()) std::cerr << MAGEL_TEXT("  构建脚本：") << makefilePath.string() << "\n";
            std::cerr << MAGEL_TEXT("  编译日志：") << logPath.string() << "\n";
            return 1;
        }

#ifdef _WIN32
        /* 链接已经直接落到带后缀的最终路径，不再需要改名；仅校验产物确实存在。 */
        if (exeImage != exeAbsolute && !std::filesystem::exists(exeImage))
        {
            std::cerr << "无法保存指定可执行文件：" << exeImage.string() << "\n";
            return 1;
        }
#endif

        if (aotKeepIntermediate)
        {
            std::cout << MAGEL_TEXT("已生成 ") << exeImage.string() << MAGEL_TEXT("（保留中间文件）\n");
            std::cout << MAGEL_TEXT("  生成源码：") << sourcePath.string() << "\n";
            std::cout << MAGEL_TEXT("  目标文件：") << objectPath.string() << "\n";
            std::cout << MAGEL_TEXT("  编译日志：") << logPath.string() << "\n";
            if (cppOutput) std::cout << MAGEL_TEXT("  生成源码：") << sourcePath.string() << "\n";
            if (!makeProgram.empty()) std::cout << MAGEL_TEXT("  构建脚本：") << makefilePath.string() << "\n";
            std::cout << MAGEL_TEXT("  优化级别：") << optFlag << "\n";
        }
        else
        {
            std::size_t removed = 0u;
            Memory::String cleanupError;
            if (!removeManaged(root, generated, {}, &removed, &cleanupError))
            {
                std::cerr << MAGEL_TEXT("警告：中间文件清理失败：") << cleanupError << "\n";
            }
            std::error_code removeError;
            std::filesystem::remove(root / kAotManifestName, removeError);
            std::cout << MAGEL_TEXT("已生成 ") << exeImage.string() << MAGEL_TEXT("（已清理 ") << removed
                      << MAGEL_TEXT(" 个中间文件；用 --keep-intermediate 保留）\n");
            if (cppOutput) std::cout << MAGEL_TEXT("  生成源码：") << sourcePath.string() << "\n";
            if (!makeProgram.empty()) std::cout << MAGEL_TEXT("  构建脚本已删（--keep-intermediate 可保留）\n");
            std::cout << MAGEL_TEXT("  优化级别：") << optFlag << "\n";
        }
        return 0;
    }

    return 0;
}

} // namespace magel_emit

/* 独立 `magel-emit` 二进制自带 main；统一入口 `magel_tool.cpp` 在进程内直接调用
 * `magel_emit::runCli`，构建时用 `-DMAGEL_EMIT_NO_MAIN` 关掉这里。 */
#ifndef MAGEL_EMIT_NO_MAIN
int main(int argc, char **argv) {
    auto arguments=magel_i18n::processArguments(argc,argv);
    std::string error;
    if(!magel_i18n::configure(arguments,error)){std::fprintf(stderr,"magel: %s\n",error.c_str());return 2;}
    std::vector<char*> values;
    for(auto &argument:arguments)values.push_back(argument.data());
    values.push_back(nullptr);
    return magel_emit::runCli(static_cast<int>(arguments.size()),values.data());
}
#endif
