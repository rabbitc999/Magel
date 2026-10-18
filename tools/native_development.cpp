/**
 * @file native_development.cpp
 * @brief 原生 LSP、格式化及开发进程服务，复用编译器诊断和宏展开。
 */
#include "native_development.hpp"
#include "../library/console/utf8.hpp"
#include "../library/json/json.hpp"
#include "../source/magel_macro.hpp"
#include "../source/magel_module.hpp"
#include "../source/magel_diagnostics.hpp"
#include "../library/miniz/miniz_tinfl.h"
#include "../library/toml/toml.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <future>
#include "native_lsp_scheduler.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <winhttp.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

const char *magel_development_tool(const char *name);
namespace magel_development
{
namespace
{
namespace fs = std::filesystem;
using J = json::Value;
using A = json::Array;
J object(std::initializer_list<std::pair<std::string, J>> values = {})
{
    json::Object o;
    for (auto &p : values)
        o[p.first] = p.second;
    return J(std::move(o));
}
/**
 * @brief 按名称从 JSON 对象读取成员。
 * @param v 待读取或转换的 JSON 值。
 * @param key 查找或写入使用的键。
 * @retval 对应对象的指针。
 */
const J &get(const J &v, const char *key)
{
    static const J empty;
    auto p = v.find(key);
    return p ? *p : empty;
}
/**
 * @brief 取得 JSON 字符串内容或使用默认值。
 * @param v 待读取或转换的 JSON 值。
 * @retval 生成或读取的结果文本。
 */
std::string str(const J &v)
{
    auto s = v.asString();
    return s ? std::string(*s) : std::string();
}
/**
 * @brief 取得 JSON 字符串内容或使用默认值。
 * @param v 待读取或转换的 JSON 值。
 * @param key 查找或写入使用的键。
 * @retval 生成或读取的结果文本。
 */
std::string str(const J &v, const char *key) { return str(get(v, key)); }
/**
 * @brief 取得 JSON 数组内容或构造空数组。
 * @param v 待读取或转换的 JSON 值。
 * @retval 对应对象的引用。
 */
const A &array(const J &v)
{
    static const A empty;
    auto p = v.asArray();
    return p ? *p : empty;
}
/**
 * @brief 将 JSON 数值读取为整数。
 * @param v 待读取或转换的 JSON 值。
 * @param fallback 无法取得目标值时使用的默认值。
 * @retval 将 JSON 数值读取为整数。
 */
int integer(const J &v, int fallback = 0) { return int(v.intOr(fallback)); }
/**
 * @brief 去除文本首尾空白。
 * @param s 待处理的文本或节点文本值。
 * @retval 生成或读取的结果文本。
 */
std::string trim(const std::string &s)
{
    auto a = s.find_first_not_of(" \t\r\n");
    return a == s.npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
/**
 * @brief 判断文本是否具有指定前缀。
 * @param s 待处理的文本或节点文本值。
 * @param p 输入字节位置或关联的测试计数器。
 * @retval 文本具有指定前缀时为 true，否则为 false。
 */
bool starts(const std::string &s, const std::string &p)
{
    return s.compare(0, p.size(), p) == 0;
}
/**
 * @brief 判断文本是否具有指定后缀。
 * @param s 待处理的文本或节点文本值。
 * @param p 输入字节位置或关联的测试计数器。
 * @retval 文本具有指定后缀时为 true，否则为 false。
 */
bool ends(const std::string &s, const std::string &p)
{
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
/**
 * @brief 读取指定文件的完整内容。
 * @param p 输入字节位置或关联的测试计数器。
 * @retval 生成或读取的结果文本。
 */
std::string read(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    if (!in)
        throw std::runtime_error("无法读取：" + p.u8string());
    std::string s((std::istreambuf_iterator<char>(in)), {});
    if (starts(s, "\xef\xbb\xbf"))
        s.erase(0, 3);
    return s;
}
/**
 * @brief 将给定文本写入文件。
 * @param p 输入字节位置或关联的测试计数器。
 * @param s 待处理的文本或节点文本值。
 */
void write(const fs::path &p, const std::string &s)
{
    std::ofstream out(p, std::ios::binary);
    out << s;
    out.close();
    if (!out)
        throw std::runtime_error("无法写入：" + p.u8string());
}
/**
 * @brief 解析输入文本并返回结构化结果。
 * @param s 待处理的文本或节点文本值。
 * @retval 保存解析输入文本并返回结构化结果结果的对象。
 */
J parse(const std::string &s)
{
    auto p = json::parse(s);
    if (!p.ok())
        throw std::runtime_error("无效 JSON");
    return *p.value;
}
/**
 * @brief 读取工程配置清单。
 * @param p 输入字节位置或关联的测试计数器。
 * @retval 保存读取工程配置清单结果的对象。
 */
toml::Value manifest(const fs::path &p)
{
    auto data = toml::parse(read(p));
    if (!data.ok())
        throw std::runtime_error("无效 TOML：" + p.u8string());
    return *data.value;
}
/**
 * @brief 从配置对象中取得指定字段。
 * @param v 待读取或转换的 JSON 值。
 * @param key 查找或写入使用的键。
 * @retval 对应对象的指针。
 */
const toml::Value &field(const toml::Value &v, const char *key)
{
    static const toml::Value empty;
    auto p = v.find(key);
    return p ? *p : empty;
}
/**
 * @brief 取得文本字段的字符串内容。
 * @param v 待读取或转换的 JSON 值。
 * @retval 生成或读取的结果文本。
 */
std::string text(const toml::Value &v)
{
    auto p = v.asString();
    return p ? std::string(*p) : "";
}
/**
 * @brief 将文件路径转换为规范化的绝对路径。
 * @param p 输入字节位置或关联的测试计数器。
 * @retval 保存将文件路径转换为规范化的绝对路径结果的对象。
 */
fs::path normalizedPath(const fs::path &p)
{
    std::error_code ec;
    auto value = fs::weakly_canonical(p, ec);
    return ec ? fs::absolute(p).lexically_normal() : value;
}
struct Temporary
{
    fs::path path;
    /**
     * @brief 创建用于工具命令的临时工作目录。
     */
    Temporary()
    {
        for (int i = 0; i < 100; ++i)
        {
            path = fs::temp_directory_path() / ("magel-native-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(i));
            if (fs::create_directory(path))
                return;
        }
        throw std::runtime_error("无法创建临时目录");
    }
    /**
     * @brief 清理工具命令创建的临时工作目录。
     */
    ~Temporary()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};
struct Execution
{
    int code = 1;
    std::string output, error;
    bool cancelled = false;
};
// 文件管道避免子进程填满 stdout/stderr 管道；超时终止整个进程树。
/**
 * @brief 文件管道避免子进程填满 stdout/stderr 管道；超时终止整个进程树。
 * @param args 按接口约定顺序传入的实参列表。
 * @param cwd 子进程的工作目录。
 * @param input 程序输入或待解码的字节流。
 * @param timeout 子进程执行的超时限制。
 * @param cancelled 请求取消标志。
 * @retval 保存文件管道避免子进程填满 stdout/stderr 管道；超时终止整个进程树结果的对象。
 */
Execution execute(const std::vector<std::string> &args, const fs::path &cwd,
                  const std::string &input = "", double timeout = 30,
                  const std::function<bool()> &cancelled = {})
{
    Temporary temp;
    write(temp.path / "in", input);
    Execution result;
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hin =
        CreateFileW((temp.path / "in").c_str(), GENERIC_READ, FILE_SHARE_READ,
                    &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE hout =
        CreateFileW((temp.path / "out").c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                    &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE herr =
        CreateFileW((temp.path / "err").c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                    &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hin == INVALID_HANDLE_VALUE || hout == INVALID_HANDLE_VALUE || herr == INVALID_HANDLE_VALUE)
    {
        if (hin != INVALID_HANDLE_VALUE)
            CloseHandle(hin);
        if (hout != INVALID_HANDLE_VALUE)
            CloseHandle(hout);
        if (herr != INVALID_HANDLE_VALUE)
            CloseHandle(herr);
        throw std::runtime_error("无法创建进程输入输出");
    }
    std::wstring command;
    for (auto &arg : args)
    {
        if (!command.empty())
            command += L' ';
        command += tool_console::quoteArgument(fs::u8path(arg).wstring());
    }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hin;
    si.hStdOutput = hout;
    si.hStdError = herr;
    PROCESS_INFORMATION pi{};
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job)
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                                sizeof(limits));
    BOOL ok = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                             cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(hin);
    CloseHandle(hout);
    CloseHandle(herr);
    if (!ok)
    {
        if (job)
            CloseHandle(job);
        throw std::runtime_error("无法启动：" + args.front());
    }
    if (job && !AssignProcessToJobObject(job, pi.hProcess))
    {
        CloseHandle(job);
        job = nullptr;
    }
    ResumeThread(pi.hThread);
    const auto began = std::chrono::steady_clock::now();
    DWORD status = WAIT_TIMEOUT;
    while ((status = WaitForSingleObject(pi.hProcess, 20)) == WAIT_TIMEOUT)
    {
        if (cancelled && cancelled())
        {
            result.cancelled = true;
            break;
        }
        if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >= timeout) break;
    }
    if (status == WAIT_TIMEOUT)
    {
        if (job)
            TerminateJobObject(job, 1);
        else
            TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        if (!result.cancelled) result.error = "timed out";
    }
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    result.code = int(code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (job)
        CloseHandle(job);
#else
    // fork 后只调用进程操作，避免继承其他 LSP 线程持有的 C++ 分配器锁。
    std::vector<char *> values;
    for (const auto &a : args) values.push_back(const_cast<char *>(a.c_str()));
    values.push_back(nullptr);
    const auto inputPath = (temp.path / "in").string(), outputPath = (temp.path / "out").string(),
               errorPath = (temp.path / "err").string(), workingDirectory = cwd.string();
    pid_t child = fork();
    if (child < 0)
        throw std::runtime_error("无法启动子进程");
    if (!child)
    {
        setpgid(0, 0);
        if (!workingDirectory.empty() && chdir(workingDirectory.c_str()))
            _exit(126);
        int in = open(inputPath.c_str(), O_RDONLY),
            out = open(outputPath.c_str(), O_CREAT | O_WRONLY, 0600),
            err = open(errorPath.c_str(), O_CREAT | O_WRONLY, 0600);
        dup2(in, 0);
        dup2(out, 1);
        dup2(err, 2);
        close(in);
        close(out);
        close(err);
        execvp(values[0], values.data());
        _exit(127);
    }
    setpgid(child, child);
    int status = 0;
    auto begin = std::chrono::steady_clock::now();
    while (waitpid(child, &status, WNOHANG) == 0)
    {
        result.cancelled = cancelled && cancelled();
        if (result.cancelled || std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count() > timeout)
        {
            kill(-child, SIGKILL);
            waitpid(child, &status, 0);
            if (!result.cancelled) result.error = "timed out";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    result.code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
    result.output = read(temp.path / "out");
    result.error += read(temp.path / "err");
    return result;
}
/**
 * @brief 将文件路径编码为语言服务使用的 URI。
 * @param path 目标文件或目录的路径。
 * @retval 生成或读取的结果文本。
 */
std::string uri(const fs::path &path)
{
    std::string p = normalizedPath(path).generic_u8string(), out = "file://";
    if (p.empty() || p[0] != '/')
        out += '/';
    const char *hex = "0123456789ABCDEF";
    for (unsigned char c : p)
    {
        if (std::isalnum(c) || c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~')
            out += char(c);
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}
/**
 * @brief 将文件 URI 解码为本地路径。
 * @param value 待转换、保存或参与计算的值。
 * @retval 保存将文件 URI 解码为本地路径结果的对象。
 */
fs::path pathOf(const std::string &value)
{
    if (!starts(value, "file://"))
        return {};
    std::string p = value.substr(7), out;
    for (size_t i = 0; i < p.size(); ++i)
    {
        if (p[i] == '%' && i + 2 < p.size() && std::isxdigit(p[i + 1]) && std::isxdigit(p[i + 2]))
        {
            out += char(std::stoi(p.substr(i + 1, 2), nullptr, 16));
            i += 2;
        }
        else
            out += p[i];
    }
#ifdef _WIN32
    if (out.size() > 2 && out[0] == '/' && out[2] == ':')
        out.erase(0, 1);
    else if (!out.empty() && out[0] != '/')
        out = "//" + out;
#endif
    return fs::u8path(out);
}
/**
 * @brief 计算当前位置 UTF-8 字符占用的字节数。
 * @param s 待处理的文本或节点文本值。
 * @param p 输入字节位置或关联的测试计数器。
 * @retval 当前位置 UTF-8 字符占用的字节数。
 */
size_t charWidth(const std::string &s, size_t p)
{
    unsigned char c = s[p];
    return c < 0x80 ? 1 : c < 0xe0 ? 2 :
                      c < 0xf0     ? 3 :
                                     4;
}
/**
 * @brief 当前请求的行位置索引，仅保存不可变源码；每个工作线程独立。
 */
struct PositionIndex
{
    const std::string *text = nullptr;
    std::vector<size_t> lines;
    /**
     * @brief 为文本建立字节偏移与协议行列位置索引。
     * @param source 提供输入数据的来源。
     */
    explicit PositionIndex(const std::string &source) : text(&source), lines{ 0 }
    {
        for (size_t at = 0; at < source.size(); ++at)
            if (source[at] == '\n') lines.push_back(at + 1);
    }
};
thread_local const PositionIndex *activePositions = nullptr;
/**
 * @brief 将文本字节偏移转换为协议行列位置。
 * @param s 待处理的文本或节点文本值。
 * @param offset 相对于起点的字节偏移。
 * @retval 包含 line 与 character 字段的协议位置对象。
 */
J position(const std::string &s, size_t offset)
{
    int line = 0, col = 0;
    size_t begin = 0;
    if (activePositions && activePositions->text == &s)
    {
        const auto it = std::upper_bound(activePositions->lines.begin(), activePositions->lines.end(), std::min(offset, s.size()));
        line = int(it - activePositions->lines.begin()) - 1;
        begin = activePositions->lines[size_t(line)];
    }
    for (size_t p = begin; p < std::min(offset, s.size());)
    {
        if (s[p] == '\n')
        {
            ++line;
            col = 0;
            ++p;
        }
        else
        {
            auto w = charWidth(s, p);
            if (p + w > offset)
                break;
            col += w == 4 ? 2 : 1;
            p += w;
        }
    }
    return object({ { "line", line }, { "character", col } });
}
/**
 * @brief 将协议行列位置转换为文本字节偏移。
 * @param s 待处理的文本或节点文本值。
 * @param pos 协议规定的行列位置。
 * @retval 对应于协议行列位置的文本字节偏移。
 */
size_t offsetAt(const std::string &s, const J &pos)
{
    int line = std::max(0, integer(get(pos, "line"))),
        col = std::max(0, integer(get(pos, "character")));
    size_t p = 0;
    while (line && p < s.size())
        if (s[p++] == '\n')
            --line;
    while (col && p < s.size() && s[p] != '\n')
    {
        auto w = charWidth(s, p);
        int units = w == 4 ? 2 : 1;
        if (units > col)
            break;
        col -= units;
        p += w;
    }
    return p;
}
/**
 * @brief 将源码位置转换为语言服务器范围对象。
 * @param s 待处理的文本或节点文本值。
 * @param a 第一个操作数或节点编号。
 * @param b 第二个操作数或节点编号。
 * @retval 保存将源码位置转换为语言服务器范围对象结果的对象。
 */
J range(const std::string &s, size_t a, size_t b)
{
    return object({ { "start", position(s, a) }, { "end", position(s, b) } });
}
const std::set<std::string> keywords = {
    "defer", "scope", "heap",
    "_", "abi", "auto", "bool", "break", "cast", "char",
    "const", "continue", "copy", "delete", "do", "drop", "elif", "else",
    "enum", "export", "false", "float32", "float64", "fn", "for",
    "if", "impl", "import", "in", "inline", "int16", "int32",
    "int64", "int8", "interface", "loop", "match", "module",
    "move", "mut", "new", "return", "static", "struct",
    "true", "type", "uint16", "uint32", "uint64", "uint8", "void",
    "while", "yield"
};
const std::set<std::string> primitives = {
    "bool", "char", "float32", "float64", "int8", "int16", "int32",
    "int64", "uint8", "uint16", "uint32", "uint64", "void"
};
/**
 * @brief 判断文本是否为有效标识符。
 * @param s 待处理的文本或节点文本值。
 * @retval 文本为有效标识符时为 true，否则为 false。
 */
bool identifier(const std::string &s)
{
    return !s.empty() && (std::isalpha((unsigned char)s[0]) || s[0] == '_') && std::all_of(s.begin() + 1, s.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}
struct Token
{
    std::string word;
    size_t a, b;
    int kind = 0;
};
// 原生编辑期词法扫描器；保留不完整 token，不参与权威编译文法解析。
/**
 * @brief 扫描源文本并产生词法项。
 * @param s 待处理的文本或节点文本值。
 * @param trivia 是否保留空白及注释词法项。
 * @param multilineStrings 是否跨行扫描字符串，格式化时用于逐字保护字符串内容。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<Token> lex(const std::string &s, bool trivia = false, bool multilineStrings = false)
{
    std::vector<Token> out;
    size_t i = 0;
    while (i < s.size())
    {
        if (std::isspace((unsigned char)s[i]))
        {
            ++i;
            continue;
        }
        size_t a = i;
        int kind = 0;
        char c = s[i++];
        if (c == '/' && i < s.size() && (s[i] == '/' || s[i] == '*'))
        {
            kind = 8;
            if (s[i++] == '/')
            {
                while (i < s.size() && s[i] != '\n')
                    ++i;
            }
            else
            {
                auto end = s.find("*/", i);
                i = end == s.npos ? s.size() : end + 2;
            }
        }
        else if (c == '"' || c == '\'')
        {
            kind = 9;
            while (i < s.size() && (multilineStrings || s[i] != '\n'))
            {
                char v = s[i++];
                if (v == '\\' && i < s.size())
                    ++i;
                else if (v == c)
                    break;
            }
        }
        else if (std::isdigit((unsigned char)c))
        {
            kind = 10;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == '.' || ((s[i] == '+' || s[i] == '-') && (s[i - 1] == 'e' || s[i - 1] == 'E'))))
                ++i;
        }
        else if (std::isalpha((unsigned char)c) || c == '_' || c == '$')
        {
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == '$'))
                ++i;
        }
        else
        {
            for (auto op : { "::", "->", "=>", "...", "==", "!=", "<=", ">=", "++", "--",
                             "+=", "-=", "*=", "/=", "&&", "||" })
                if (s.compare(a, std::char_traits<char>::length(op), op) == 0)
                {
                    i = a + std::char_traits<char>::length(op);
                    break;
                }
        }
        if (trivia || !kind || kind == 10)
            out.push_back({ s.substr(a, i - a), a, i, kind });
    }
    return out;
}
/**
 * @brief 查找括号组对应的结束位置。
 * @param t 待扫描的词法项数组。
 * @param begin 范围起点或首个可变实参的索引。
 * @param open 左括号或起始分组的位置。
 * @param close 需要匹配的右括号字符。
 * @retval 对应右括号的位置。
 */
size_t groupEnd(const std::vector<Token> &t, size_t begin,
                const std::string &open, const std::string &close)
{
    int depth = 0;
    for (size_t i = begin; i < t.size(); ++i)
    {
        if (t[i].word == open)
            ++depth;
        else if (t[i].word == close && !--depth)
            return i + 1;
    }
    return t.size();
}
/**
 * @brief 整理符号声明为可显示的函数签名。
 * @param s 待处理的文本或节点文本值。
 * @retval 生成或读取的结果文本。
 */
std::string cleanSignature(const std::string &s)
{
    std::string out;
    size_t last = 0;
    for (auto &t : lex(s, true))
    {
        if (t.kind == 8)
            continue;
        if (!out.empty() && t.a > last)
            out += ' ';
        out += t.word;
        last = t.b;
    }
    return trim(out);
}
/**
 * @brief 提取文档注释中的可显示说明。
 * @param s 待处理的文本或节点文本值。
 * @retval 生成或读取的结果文本。
 */
std::string doc(const std::string &s)
{
    std::string value = s;
    if (starts(value, "/*"))
    {
        value.erase(0, 2);
        if (ends(value, "*/"))
            value.resize(value.size() - 2);
    }
    value = std::regex_replace(
        value, std::regex(R"((^|\n)[ \t]*(\*|///|//!)[ \t]?)"), "$1");
    value = std::regex_replace(value, std::regex("@brief[ \\t]*"), "");
    value = std::regex_replace(value, std::regex(R"(@param\s+(\w+)\s*)"),
                               "\n\n**参数 `$1`**：");
    value = std::regex_replace(value, std::regex(R"(@returns?\s*)"),
                               "\n\n**返回值**：");
    return trim(value);
}
/**
 * @brief 查找符号声明之前的文档注释。
 * @param s 待处理的文本或节点文本值。
 * @param at 读取或扫描的当前位置。
 * @param tokens 已扫描的词法项数组。
 * @retval 生成或读取的结果文本。
 */
std::string precedingDoc(const std::string &s, size_t at, const std::vector<Token> *tokens = nullptr)
{
    auto owned = tokens ? std::vector<Token>() : lex(s.substr(0, at), true);
    const auto &raw = tokens ? *tokens : owned;
    std::string out;
    size_t end = at;
    auto stop = std::lower_bound(raw.begin(), raw.end(), at, [](const Token &token, size_t value) { return token.a < value; });
    for (auto it = std::make_reverse_iterator(stop); it != raw.rend(); ++it)
    {
        if (it->kind != 8 || !trim(s.substr(it->b, end - it->b)).empty())
            break;
        if (!starts(it->word, "/*") && !starts(it->word, "///") && !starts(it->word, "//!"))
            break;
        out = it->word + "\n" + out;
        end = it->a;
    }
    return doc(out);
}
/**
 * @brief 提取源文件中的预处理指令。
 * @param s 待处理的文本或节点文本值。
 * @param key 查找或写入使用的键。
 * @retval 生成或读取的结果文本。
 */
std::vector<std::string> directives(const std::string &s,
                                    const std::string &key)
{
    auto t = lex(s);
    std::vector<std::string> out;
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i].word == key)
        {
            std::string name;
            for (++i; i < t.size() && (identifier(t[i].word) || t[i].word == "::");
                 ++i)
                name += t[i].word;
            if (!name.empty())
                out.push_back(name);
        }
    return out;
}
struct Record
{
    std::string name, module, owner, qualified, signature, documentation, source,
        type, kind = "function";
    size_t offset = 0;
    A parameters;
};
/**
 * @brief 拼接模块与符号的限定名称。
 * @param r 符号、类型或请求记录。
 * @retval 生成或读取的结果文本。
 */
std::string qualified(const Record &r)
{
    return (r.module.empty() ? "" : r.module + "::") + (r.owner.empty() ? "" : r.owner + "::") + r.name;
}
/**
 * @brief 从函数声明中提取参数说明。
 * @param signature 待提取参数的函数签名。
 * @retval 保存从函数声明中提取参数说明结果的对象。
 */
A parameters(const std::string &signature)
{
    auto t = lex(signature);
    size_t opening = t.size();
    int depth = 0;
    for (size_t i = 0; i < t.size(); ++i)
    {
        if (t[i].word == "(" && !depth && i && (identifier(t[i - 1].word) || t[i - 1].word == ">"))
        {
            opening = i;
            break;
        }
        if (t[i].word == "(")
            ++depth;
        else if (t[i].word == ")")
            --depth;
    }
    A out;
    if (opening == t.size())
        return out;
    auto end = groupEnd(t, opening, "(", ")");
    size_t start = t[opening].b;
    depth = 0;
    for (size_t i = opening + 1; i < end; ++i)
    {
        auto w = t[i].word;
        if ((w == "," && !depth) || i == end - 1)
        {
            auto label = trim(signature.substr(start, t[i].a - start));
            if (!label.empty())
                out.push_back(object({ { "label", label } }));
            start = t[i].b;
        }
        else if (w == "(" || w == "<" || w == "[")
            ++depth;
        else if (w == ")" || w == ">" || w == "]")
            --depth;
    }
    return out;
}
/**
 * @brief 扫描源文件并收集语言服务所需的符号信息。
 * @param source 提供输入数据的来源。
 * @param s 待处理的文本或节点文本值。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<Record> scan(const std::string &source, const std::string &s)
{
    auto t = lex(s);
    const auto raw = lex(s, true);
    std::vector<Record> records;
    auto modules = directives(s, "module");
    std::string module = modules.empty() ? "" : modules[0];
    // 宏体不是普通函数声明；单独索引定义和撤销，支持同名重载。
    std::set<size_t> macroTokens;
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i].word == "$macro" && i + 1 < t.size())
        {
            size_t end = i;
            while (end < t.size() && t[end].word != "$end_macro")
                ++end;
            for (size_t j = i; j <= end && j < t.size(); ++j)
                macroTokens.insert(j);
            Record r;
            const size_t name = t[i + 1].word == "expr" && i + 2 < t.size() ? i + 2 : i + 1;
            r.name = t[name].word;
            r.module = module;
            r.kind = "macro";
            r.source = source;
            r.offset = t[name].a;
            auto nl = s.find('\n', t[i].a);
            r.signature = s.substr(t[i].a, (nl == s.npos ? s.size() : nl) - t[i].a);
            r.documentation = precedingDoc(s, t[i].a, &raw);
            r.parameters = parameters(r.signature);
            r.qualified = qualified(r);
            records.push_back(r);
            i = end < t.size() ? end : t.size() - 1;
        }
    for (size_t i = 0; i + 1 < t.size(); ++i)
    {
        if (t[i].word == "$replace")
        {
            Record r;
            const size_t name = t[i + 1].word == "expr" && i + 2 < t.size() ? i + 2 : i + 1;
            r.name = t[name].word;
            r.module = module;
            r.kind = "macro";
            r.source = source;
            r.offset = t[name].a;
            const auto end = s.find('\n', t[i].a);
            r.signature = s.substr(t[i].a, (end == s.npos ? s.size() : end) - t[i].a);
            r.documentation = precedingDoc(s, t[i].a, &raw);
            r.parameters = parameters(r.signature);
            r.qualified = qualified(r);
            records.push_back(r);
        }
        if (t[i].word == "$delete_macro")
        {
            const auto name = t[i + 1].word;
            records.erase(std::remove_if(records.begin(), records.end(),
                                         [&](const Record &r) {
                                             return r.kind == "macro" && r.name == name && r.offset < t[i].a;
                                         }),
                          records.end());
        }
        if (t[i].word == "%" && t[i + 1].word == "undefine" && i + 2 < t.size())
        {
            records.erase(std::remove_if(records.begin(), records.end(),
                                         [&](const Record &r) {
                                             return r.kind == "macro" && r.name == t[i + 2].word && starts(r.signature, "%define") && r.offset < t[i].a;
                                         }),
                          records.end());
        }
        if (t[i].word == "%" && t[i + 1].word == "define" && i + 2 < t.size())
        {
            Record r;
            r.name = t[i + 2].word;
            r.module = module;
            r.kind = "macro";
            r.source = source;
            r.offset = t[i + 2].a;
            const auto end = s.find('\n', t[i].a);
            r.signature = s.substr(t[i].a, (end == s.npos ? s.size() : end) - t[i].a);
            r.documentation = precedingDoc(s, t[i].a, &raw);
            r.qualified = qualified(r);
            records.push_back(r);
        }
        if (starts(t[i].word, "$") || t[i].word == "%")
        {
            const auto end = s.find('\n', t[i].a);
            for (size_t j = i; j < t.size() && (end == s.npos || t[j].a < end); ++j)
                macroTokens.insert(j);
        }
    }
    std::function<void(size_t, size_t, std::string)> visit =
        [&](size_t begin, size_t limit, std::string owner) {
            size_t i = begin;
            while (i < limit)
            {
                if (macroTokens.count(i) || t[i].word == ";" || starts(t[i].word, "$"))
                {
                    ++i;
                    continue;
                }
                size_t start = i;
                int parens = 0, brackets = 0;
                while (i < limit)
                {
                    auto w = t[i].word;
                    if (w == "(")
                        ++parens;
                    if (w == ")")
                        --parens;
                    if (w == "[")
                        ++brackets;
                    if (w == "]")
                        --brackets;
                    if (!parens && !brackets && (w == ";" || w == "{"))
                        break;
                    ++i;
                }
                if (i == limit)
                    break;
                auto end = t[i].word == "{" ? groupEnd(t, i, "{", "}") : i + 1;
                Record r;
                r.module = module;
                r.owner = owner;
                r.source = source;
                r.signature =
                    cleanSignature(s.substr(t[start].a, t[i].a - t[start].a));
                r.documentation = precedingDoc(s, t[start].a, &raw);
                r.offset = t[start].a;
                if ((t[start].word == "struct" || t[start].word == "enum" || t[start].word == "interface") && start + 1 < i)
                {
                    r.kind = t[start].word;
                    r.name = t[start + 1].word;
                    r.offset = t[start + 1].a;
                    r.qualified = qualified(r);
                    records.push_back(r);
                    if (t[i].word == "{")
                        visit(i + 1, end - 1,
                              owner.empty() ? r.name : owner + "::" + r.name);
                }
                else if (t[start].word != "module" && t[start].word != "import" && t[start].word != "type" && t[start].word[0] != '%')
                {
                    size_t name = i;
                    int depth = 0;
                    for (size_t n = start; n < i; ++n)
                    {
                        if (t[n].word == "(" && !depth && n > start)
                        {
                            size_t k = n - 1;
                            if (t[k].word == ">")
                            {
                                int angle = 1;
                                while (k > start && angle)
                                {
                                    --k;
                                    if (t[k].word == ">")
                                        ++angle;
                                    else if (t[k].word == "<")
                                        --angle;
                                }
                                if (k > start)
                                    --k;
                            }
                            if (identifier(t[k].word) && t[k].word != "fn")
                            {
                                name = k;
                                break;
                            }
                        }
                        if (t[n].word == "(")
                            ++depth;
                        else if (t[n].word == ")")
                            --depth;
                    }
                    if (name < i && s.substr(t[start].a, t[name].a - t[start].a).find('=') == s.npos)
                    {
                        r.name = t[name].word;
                        r.offset = t[name].a;
                        r.parameters = parameters(r.signature);
                        r.qualified = qualified(r);
                        records.push_back(r);
                    }
                    else if (!owner.empty())
                    {
                        size_t stop = start;
                        while (stop < i && t[stop].word != "=")
                            ++stop;
                        size_t n = stop;
                        while (n > start)
                        {
                            --n;
                            if (identifier(t[n].word))
                            {
                                r.kind = "member";
                                r.name = t[n].word;
                                r.offset = t[n].a;
                                r.type =
                                    cleanSignature(s.substr(t[start].a, t[n].a - t[start].a));
                                r.qualified = qualified(r);
                                records.push_back(r);
                                break;
                            }
                        }
                    }
                }
                i = std::max(i + 1, end);
            }
        };
    visit(0, t.size(), "");
    return records;
}
struct Binding
{
    std::string name, type, kind;
    size_t offset = 0, end = 0;
    std::vector<int> scope;
    std::string iterable = {};
    size_t context = 0, parameter = 0, limit = std::string::npos, headerEnd = 0;
};
#include "native_lsp_type_shapes.hpp"
/**
 * @brief 为扫描到的符号建立名称绑定。
 * @param s 待处理的文本或节点文本值。
 * @param types 当前可见的类型定义集合。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<Binding> scanBindings(const std::string &s,
                                  std::set<std::string> types)
{
    auto t = lex(s);
    types.insert(primitives.begin(), primitives.end());
    for (auto n :
         { "String", "List", "Set", "Map", "Option", "Result", "Any", "auto" })
        types.insert(n);
    std::vector<std::vector<int>> scopes;
    std::vector<int> scope{ 0 };
    int serial = 0;
    for (auto &token : t)
    {
        if (token.word == "{")
            scope.push_back(++serial);
        scopes.push_back(scope);
        if (token.word == "}" && scope.size() > 1)
            scope.pop_back();
    }
    std::vector<Binding> out;
    std::vector<std::pair<size_t, size_t>> regions;
    for (size_t i = 0; i + 2 < t.size(); ++i)
        if ((t[i].word == "struct" || t[i].word == "enum" || t[i].word == "interface") && identifier(t[i + 1].word))
            types.insert(t[i + 1].word);
    for (size_t i = 1; i < t.size(); ++i)
        if (t[i].word == "<" && identifier(t[i - 1].word))
        {
            const auto end = groupEnd(t, i, "<", ">");
            if (end >= t.size() || (t[end].word != "(" && t[end].word != "{" && t[end].word != ":")) continue;
            for (size_t j = i + 1; j + 1 < end; ++j)
                if (identifier(t[j].word) && (t[j - 1].word == "<" || t[j - 1].word == ",")) types.insert(t[j].word);
        }
    for (size_t i = 0; i < t.size(); ++i)
    {
        if (!types.count(t[i].word) && !(i + 1 < t.size() && t[i + 1].word == "::"))
            continue;
        if (i && (t[i - 1].word == "struct" || t[i - 1].word == "enum" || t[i - 1].word == "interface" || t[i - 1].word == "."))
            continue;
        size_t j = i + 1;
        while (j + 1 < t.size() && t[j].word == "::")
            j += 2;
        if (j < t.size() && t[j].word == "<")
            j = groupEnd(t, j, "<", ">");
        while (j < t.size() && (t[j].word == "const" || t[j].word == "mut" || t[j].word == "&"))
            ++j;
        while (j + 1 < t.size() && t[j].word == "fn" && t[j + 1].word == "(")
            j = groupEnd(t, j + 1, "(", ")");
        if (j >= t.size() || !identifier(t[j].word) || keywords.count(t[j].word))
            continue;
        size_t next = j + 1;
        if (next < t.size() && t[next].word == "<")
            next = groupEnd(t, next, "<", ">");
        auto following = next < t.size() ? t[next].word : "";
        bool function = following == "(";
        if (!function && following != "=" && following != ";" && following != "," && following != "[" && following != ")" && following != "in")
            continue;
        std::string type = cleanSignature(s.substr(t[i].a, t[j].a - t[i].a));
        out.push_back({ t[j].word, type, function ? "function" : "variable", t[j].a,
                        t[j].b, scopes[j] });
        if (function)
            regions.push_back({ next, groupEnd(t, next, "(", ")") });
        else
        {
            int depth = 0;
            for (size_t k = j + 1;
                 k < t.size() && !(depth == 0 && (t[k].word == ";" || t[k].word == ")"));
                 ++k)
            {
                auto w = t[k].word;
                if (w == "(" || w == "[" || w == "{")
                    ++depth;
                else if (w == ")" || w == "]" || w == "}")
                    --depth;
                else if (w == "," && !depth && k + 1 < t.size() && identifier(t[k + 1].word) && !types.count(t[k + 1].word))
                    out.push_back({ t[k + 1].word, type, "variable", t[k + 1].a,
                                    t[k + 1].b, scopes[k + 1] });
            }
        }
    }
    // 具名函数与匿名函数共享参数作用域；函数类型中的形参没有函数体，不建立作用域。
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i].word == "fn")
        {
            size_t open = i + 1;
            if (open < t.size() && (t[open].word == "copy" || t[open].word == "move" || t[open].word == "mut" || t[open].word == "const")) ++open;
            if (open >= t.size() || t[open].word != "(") continue;
            const auto end = groupEnd(t, open, "(", ")");
            if (end >= t.size() || t[end].word != "{") continue;
            regions.push_back({ open, end });
            size_t ordinal = 0, begin = open + 1;
            for (size_t j = begin; j < end; ++j)
            {
                if (t[j].word == "(" || t[j].word == "[")
                {
                    j = groupEnd(t, j, t[j].word, t[j].word == "(" ? ")" : "]") - 1;
                    continue;
                }
                if (t[j].word != "," && j + 1 != end) continue;
                if (j == begin + 1 && identifier(t[begin].word) && !keywords.count(t[begin].word))
                {
                    Binding b{ t[begin].word, "auto", "parameter", t[begin].a, t[begin].b, scopes[end] };
                    b.context = t[i].a;
                    b.parameter = ordinal;
                    b.limit = lspBlockLimit(t, end);
                    out.push_back(std::move(b));
                }
                begin = j + 1;
                ++ordinal;
            }
        }
    for (auto &b : out)
        for (auto &region : regions)
            if (b.offset > t[region.first].a && region.second && b.offset < t[region.second - 1].b)
            {
                b.kind = "parameter";
                size_t body = region.second;
                while (body < t.size() && (t[body].word == "copy" || t[body].word == "move" || t[body].word == "mut" || t[body].word == "const")) ++body;
                if (body < t.size() && t[body].word == "{")
                {
                    b.scope = scopes[body];
                    b.limit = lspBlockLimit(t, body);
                }
                else if (region.second)
                    b.limit = t[region.second - 1].b;
            }
    for (size_t i = 0; i + 1 < t.size(); ++i)
        if (t[i].word == "for" && t[i + 1].word == "(")
        {
            const auto end = groupEnd(t, i + 1, "(", ")");
            if (end >= t.size()) continue;
            for (auto &binding : out)
            {
                if (binding.offset <= t[i + 1].a || binding.offset >= t[end - 1].a || binding.scope != scopes[i]) continue;
                binding.headerEnd = t[end - 1].b;
                if (t[end].word == "{")
                {
                    binding.scope = scopes[end];
                    binding.limit = lspBlockLimit(t, end);
                }
                else
                {
                    size_t stop = end;
                    while (stop < t.size() && t[stop].word != ";") ++stop;
                    binding.limit = stop < t.size() ? t[stop].b : std::string::npos;
                }
            }
        }
    for (size_t i = 0; i + 3 < t.size(); ++i)
        if (t[i].word == "for" && t[i + 1].word == "(")
        {
            const auto end = groupEnd(t, i + 1, "(", ")");
            size_t in = i + 2;
            while (in < end && t[in].word != "in") ++in;
            if (in >= end || in == i + 2 || end > t.size()) continue;
            const auto &name = t[in - 1];
            auto found = std::find_if(out.begin(), out.end(), [&](const Binding &b) { return b.offset == name.a; });
            if (found == out.end())
            {
                out.push_back({ name.word, "auto", "variable", name.a, name.b, scopes[i] });
                found = out.end() - 1;
            }
            found->iterable = s.substr(t[in].b, t[end - 1].a - t[in].b);
            if (end < t.size() && t[end].word == "{")
            {
                found->scope = scopes[end];
                found->limit = lspBlockLimit(t, end);
            }
            else
            {
                auto stop = end;
                while (stop < t.size() && t[stop].word != ";") ++stop;
                found->limit = stop < t.size() ? t[stop].b : std::string::npos;
            }
        }
    std::sort(out.begin(), out.end(), [](const Binding &a, const Binding &b) { return a.offset < b.offset; });
    out.erase(std::unique(out.begin(), out.end(), [](const Binding &a, const Binding &b) { return a.offset == b.offset; }), out.end());
    return out;
}
#include "native_lsp_cache.hpp"
#include "native_lsp_bindings.hpp"
/**
 * @brief 查询指定源码位置所属的词法作用域。
 * @param s 待处理的文本或节点文本值。
 * @param cursor 当前编辑或查询的源码位置。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<int> scopeAt(const std::string &s, size_t cursor)
{
    if (activeUnit && activeUnit->text.get() == &s)
    {
        const auto at = std::upper_bound(activeUnit->braces.begin(), activeUnit->braces.end(), cursor);
        return at == activeUnit->braces.begin() ? std::vector<int>{ 0 } : activeUnit->scopes[size_t(at - activeUnit->braces.begin() - 1)];
    }
    std::vector<int> scope{ 0 };
    int serial = 0;
    for (auto &t : lex(s))
    {
        if (t.a > cursor)
            break;
        if (t.word == "{")
            scope.push_back(++serial);
        else if (t.word == "}" && scope.size() > 1)
            scope.pop_back();
    }
    return scope;
}
/**
 * @brief 判断符号在指定位置是否可见。
 * @param b 第二个操作数或节点编号。
 * @param scope 待查询或清理的词法作用域。
 * @param cursor 当前编辑或查询的源码位置。
 * @retval 符号在指定位置可见时为 true，否则为 false。
 */
bool visible(const Binding &b, const std::vector<int> &scope, size_t cursor)
{
    const bool loopHeader = b.headerEnd && b.offset <= cursor && cursor < b.headerEnd && b.scope.size() <= scope.size() + 1 && std::equal(b.scope.begin(), b.scope.end() - 1, scope.begin());
    return cursor < b.limit && (b.offset <= cursor || b.scope.size() == 1) && (loopHeader || (b.scope.size() <= scope.size() && std::equal(b.scope.begin(), b.scope.end(), scope.begin())));
}
/**
 * @brief 读取光标前的限定名或后缀表达式。
 * @param s 编辑源码。
 * @param cursor 光标偏移。
 * @return 保留调用与索引接收者的表达式文本。
 */
std::string expression(const std::string &s, size_t cursor)
{
    std::vector<Token> fallback;
    const std::vector<Token> *indexed = nullptr;
    size_t end = 0;
    if (activeUnit && activeUnit->text.get() == &s)
    {
        const auto &tokens = activeUnit->tokens;
        const auto stop = std::lower_bound(tokens.begin(), tokens.end(), cursor, [](const Token &token, size_t value) { return token.a < value; });
        end = size_t(stop - tokens.begin());
        if (!end || tokens[end - 1].b <= cursor) indexed = &tokens;
    }
    if (!indexed)
    {
        fallback = lex(s.substr(0, cursor));
        indexed = &fallback;
        end = fallback.size();
    }
    const auto &t = *indexed;
    if (!end) return "";
    if (t[end - 1].word == ">")
    {
        int level = 1;
        size_t at = end - 1;
        while (at && level)
        {
            --at;
            if (t[at].word == ">") ++level;
            if (t[at].word == "<") --level;
        }
        if (!level && at) end = at;
    }
    std::function<size_t(size_t)> consume = [&](size_t stop) -> size_t {
        if (!stop) return stop;
        size_t start = stop - 1;
        auto word = t[start].word;
        if (word == "." || word == "::" || word == "?") return consume(start);
        if (word == ")" || word == "]" || word == ">")
        {
            auto open = word == ")" ? "(" : word == "]" ? "[" :
                                                          "<";
            int level = 1;
            while (start && level)
            {
                --start;
                if (t[start].word == word) ++level;
                if (t[start].word == open) --level;
            }
            if (level) return stop;
            if (start && (identifier(t[start - 1].word) || t[start - 1].word == ")" || t[start - 1].word == "]" || t[start - 1].word == ">"))
                start = consume(start);
        }
        else if (!identifier(word) && !starts(word, "$"))
            return stop;
        while (start && (t[start - 1].word == "." || t[start - 1].word == "::"))
        {
            auto previous = consume(start - 1);
            if (previous == start - 1) break;
            start = previous;
        }
        return start;
    };
    auto start = consume(end);
    if (start == end) return "";
    std::string out;
    for (size_t i = start; i < end; ++i) out += t[i].word;
    return out;
}
/**
 * @brief 提取限定名称的最后一段。
 * @param s 待处理的文本或节点文本值。
 * @retval 生成或读取的结果文本。
 */
std::string tail(const std::string &s)
{
    auto p = s.rfind("::");
    return p == s.npos ? s : s.substr(p + 2);
}

/**
 * @brief 从编辑期声明和初始化式解析接收者类型，供悬停与成员补全共用。
 */
class ReceiverTypes
{
    const std::vector<Record> &records;
    const std::string &source;
    std::vector<Binding> vars;

    /**
     * @brief 从表达式中取得用于类型推导的词法项。
     * @param value 待转换、保存或参与计算的值。
     * @retval 按处理顺序保存的结果元素集合。
     */
    std::vector<Token> valueTokens(const std::string &value) const
    {
        auto tokens = lex(value, true);
        tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](const Token &t) {
                         return t.kind == 8;
                     }),
                     tokens.end());
        return tokens;
    }

    /**
     * @brief 取得底层内存区域的起始地址。
     * @param type 类型描述。
     * @retval 生成或读取的结果文本。
     */
    std::string base(std::string type) const
    {
        auto p = type.find_first_of("< &");
        return p == type.npos ? type : type.substr(0, p);
    }
    /**
     * @brief 判断符号是否表示类型。
     * @param r 符号、类型或请求记录。
     * @retval 符号表示类型时为 true，否则为 false。
     */
    bool isType(const Record &r) const
    {
        return r.kind == "struct" || r.kind == "enum" || r.kind == "interface";
    }
    /**
     * @brief 将名称解析为当前作用域中的限定名称。
     * @param type 类型描述。
     * @param module 当前模块的名称。
     * @retval 生成或读取的结果文本。
     */
    std::string qualify(std::string type, const std::string &module = "") const
    {
        type = trim(type);
        for (auto modifier : { "static ", "inline " })
            if (starts(type, modifier)) type = trim(type.substr(std::string(modifier).size()));
        auto name = base(type);
        std::set<std::string> found, preferred;
        for (auto &r : records)
            if (isType(r) && (r.qualified == name || r.name == name || ends(r.qualified, "::" + name)))
            {
                found.insert(r.qualified);
                if (r.module == module) preferred.insert(r.qualified);
            }
        if (preferred.size() == 1) found = preferred;
        if (found.size() == 1) type.replace(0, name.size(), *found.begin());
        return type;
    }
    /**
     * @brief 推导表达式或调用结果的类型。
     * @param r 符号、类型或请求记录。
     * @param receiver 方法调用的接收者。
     * @retval 生成或读取的结果文本。
     */
    std::string resultType(const Record &r, const std::string &receiver) const
    {
        auto type = r.type;
        if (type.empty())
        {
            auto tokens = lex(r.signature);
            for (auto &t : tokens)
                if (t.word == r.name)
                {
                    type = trim(r.signature.substr(0, t.a));
                    break;
                }
        }
        // 泛型字段与方法返回值中的类型形参按实例接收者替换。
        auto args = receiver.find('<');
        if (args != receiver.npos)
            for (auto &rType : records)
                if (isType(rType) && rType.qualified == base(receiver))
                {
                    auto parameters = lex(rType.signature), actual = lex(receiver.substr(args));
                    std::vector<std::string> names, values;
                    bool generic = false;
                    for (auto &t : parameters)
                    {
                        if (t.word == ">") break;
                        if (generic && identifier(t.word)) names.push_back(t.word);
                        if (t.word == "<") generic = true;
                    }
                    size_t begin = 1;
                    int depth = 0;
                    for (size_t i = 1; i < actual.size(); ++i)
                    {
                        auto w = actual[i].word;
                        if (!depth && (w == "," || w == ">"))
                        {
                            std::string value;
                            for (size_t j = begin; j < i; ++j) value += actual[j].word;
                            values.push_back(value);
                            begin = i + 1;
                            if (w == ">") break;
                        }
                        else if (w == "<")
                            ++depth;
                        else if (w == ">")
                            --depth;
                    }
                    auto tokens = lex(type);
                    std::string substituted;
                    size_t end = 0;
                    for (auto &t : tokens)
                    {
                        substituted += type.substr(end, t.a - end);
                        auto n = std::find(names.begin(), names.end(), t.word);
                        auto index = size_t(n - names.begin());
                        substituted += n != names.end() && index < values.size() ? values[index] : t.word;
                        end = t.b;
                    }
                    type = substituted + type.substr(end);
                    break;
                }
        return qualify(type, r.module);
    }

public:
    /**
     * @brief 建立声明视图。
     * @param entries 可见模块声明。
     * @param text 编辑器源码。
     * @param known 已登记的类型名。
     */
    ReceiverTypes(const std::vector<Record> &entries, const std::string &text,
                  const std::set<std::string> &known) : records(entries), source(text), vars(bindings(text, known)) {}

    /**
     * @brief 取得绑定的声明类型或根据初始化、遍历源及调用上下文推断类型。
     * @param binding 当前声明身份。
     * @param cursor 查询位置，保留给共享推断入口。
     * @param depth 推断递归深度。
     * @return 唯一可确定的类型，未知时返回空串。
     */
    std::string bindingType(const Binding &binding, size_t cursor, int depth = 0) const
    {
        (void)cursor;
        if (depth > 24) return "";
        if (binding.kind == "function")
        {
            auto tokens = valueTokens(source.substr(binding.end));
            size_t open = 0;
            if (!tokens.empty() && tokens[0].word == "<") open = groupEnd(tokens, 0, "<", ">");
            if (open >= tokens.size() || tokens[open].word != "(") return "";
            const auto end = groupEnd(tokens, open, "(", ")");
            if (!end || end > tokens.size()) return "";
            return lspFunctionText(lspFunctionShape(binding.type + " fn" + source.substr(binding.end + tokens[open].a, tokens[end - 1].b - tokens[open].a)));
        }
        if (base(binding.type) != "auto") return qualify(binding.type);
        if (!binding.iterable.empty())
        {
            auto type = infer(binding.iterable, binding.offset, depth + 1);
            auto open = type.find('<');
            const auto head = tail(type.substr(0, open));
            if (head == "String") return "char";
            if (open != type.npos && (head == "List" || head == "Set" || head == "Iterator" || head == "Range"))
            {
                auto tokens = valueTokens(type);
                size_t begin = 0;
                while (begin < tokens.size() && tokens[begin].word != "<") ++begin;
                int nesting = 0;
                for (size_t i = begin + 1; i < tokens.size(); ++i)
                {
                    if (!nesting && (tokens[i].word == "," || tokens[i].word == ">")) return qualify(trim(type.substr(tokens[begin].b, tokens[i].a - tokens[begin].b)));
                    if (tokens[i].word == "<") ++nesting;
                    if (tokens[i].word == ">") --nesting;
                }
            }
            if (binding.iterable.find("..") != std::string::npos) return "int64";
            return "";
        }
        if (binding.context)
        {
            auto expected = contextualType(binding.context, depth + 1);
            auto shape = lspFunctionShape(expected);
            return shape.valid && binding.parameter < shape.arguments.size() ? shape.arguments[binding.parameter] : "";
        }
        auto initializer = valueTokens(source.substr(binding.end));
        if (initializer.empty() || initializer[0].word != "=") return "";
        size_t end = 1;
        int nesting = 0;
        for (; end < initializer.size(); ++end)
        {
            const auto &word = initializer[end].word;
            if (!nesting && (word == ";" || word == "," || word == "}")) break;
            if (word == "(" || word == "[" || word == "{") ++nesting;
            if (word == ")" || word == "]" || word == "}") --nesting;
        }
        if (end <= 1) return "";
        const auto begin = initializer[1].a;
        return infer(source.substr(binding.end + begin, initializer[end - 1].b - begin), binding.offset, depth + 1);
    }

    /**
     * @brief 为省略类型的匿名函数查询初始化目标或外层调用的参数类型。
     * @param at fn 关键字的源码偏移。
     * @param depth 递归预算。
     * @return 唯一的预期函数类型，歧义时返回空串。
     */
    std::string contextualType(size_t at, int depth = 0) const
    {
        if (depth > 24) return "";
        for (const auto &binding : vars)
            if (binding.end < at && base(binding.type) != "auto" && trim(source.substr(binding.end, at - binding.end)) == "=") return binding.type;
        auto tokens = valueTokens(source.substr(0, at));
        struct Frame
        {
            size_t open;
            size_t argument;
        };
        std::vector<Frame> stack;
        for (size_t i = 0; i < tokens.size(); ++i)
        {
            const auto &w = tokens[i].word;
            if (w == "(" || w == "[" || w == "{")
                stack.push_back({ i, 0 });
            else if (w == ")" || w == "]" || w == "}")
            {
                if (!stack.empty()) stack.pop_back();
            }
            else if (w == "," && !stack.empty())
                ++stack.back().argument;
        }
        if (stack.empty() || tokens[stack.back().open].word != "(") return "";
        const auto &frame = stack.back();
        auto name = expression(source, tokens[frame.open].a);
        auto callable = lspFunctionShape(infer(name, at, depth + 1));
        if (callable.valid && frame.argument < callable.arguments.size()) return callable.arguments[frame.argument];
        std::set<std::string> expected;
        for (const auto &record : records)
            if (record.kind == "function" && (record.qualified == name || (record.owner.empty() && record.name == name)))
            {
                std::string args;
                for (const auto &parameter : record.parameters) args += (args.empty() ? "" : ", ") + str(parameter, "label");
                auto shape = lspFunctionShape("void fn(" + args + ")");
                if (shape.valid && frame.argument < shape.arguments.size()) expected.insert(shape.arguments[frame.argument]);
            }
        return expected.size() == 1 ? *expected.begin() : "";
    }

    /**
     * @brief 使用实参数量和唯一的精确类型匹配筛选重载，未知或需要转换时保留候选。
     * @param entries 同名调用候选。
     * @param arguments 原始实参文本。
     * @param cursor 调用位置。
     * @param depth 类型推断递归预算。
     * @return 精确匹配集合，不能确定时返回原候选。
     */
    std::vector<Record> selectedCalls(const std::vector<Record> &entries, const Memory::Vector<Memory::String> &arguments, size_t cursor, int depth = 0) const
    {
        std::vector<Record> selected;
        for (const auto &record : entries)
        {
            std::string params;
            for (const auto &parameter : record.parameters) params += (params.empty() ? "" : ", ") + str(parameter, "label");
            auto shape = lspFunctionShape("void fn(" + params + ")");
            if (shape.arguments.size() != arguments.size()) continue;
            bool exact = true;
            for (size_t i = 0; i < arguments.size(); ++i)
            {
                const auto actual = infer(arguments[i].c_str(), cursor, depth + 1);
                const auto expected = qualify(shape.arguments[i]);
                if (actual.empty() || actual != expected)
                {
                    exact = false;
                    break;
                }
            }
            if (exact) selected.push_back(record);
        }
        return selected.empty() ? entries : selected;
    }

    /**
     * @brief 将显式泛型实参或直接类型形参的推断结果代入调用返回类型。
     * @param record 函数声明。
     * @param result 尚未实例化的返回类型。
     * @param callee 含可选尖括号实参的被调名称。
     * @param arguments 调用实参。
     * @param cursor 调用位置。
     * @param depth 推断递归预算。
     * @return 已替换的返回类型；未确定的形参保留原文。
     */
    std::string instantiateResult(const Record &record, std::string result, const std::string &callee, const Memory::Vector<Memory::String> &arguments, size_t cursor, int depth) const
    {
        auto tokens = valueTokens(record.signature);
        std::vector<std::string> names;
        for (size_t i = 0; i + 1 < tokens.size(); ++i)
            if (tokens[i].word == record.name && tokens[i + 1].word == "<")
            {
                const auto end = groupEnd(tokens, i + 1, "<", ">");
                for (size_t j = i + 2; j + 1 < end; ++j)
                    if (identifier(tokens[j].word) && (tokens[j - 1].word == "<" || tokens[j - 1].word == ",")) names.push_back(tokens[j].word);
                break;
            }
        if (names.empty()) return qualify(result, record.module);
        std::map<std::string, std::string> values;
        const auto open = callee.find('<');
        if (open != callee.npos)
        {
            Memory::Vector<Memory::String> actual;
            size_t end = open;
            if (magel_macro::groupArguments(callee.c_str(), open, actual, end) && actual.size() == names.size())
                for (size_t i = 0; i < names.size(); ++i) values[names[i]] = trim(actual[i].c_str());
        }
        else
        {
            std::string params;
            for (const auto &parameter : record.parameters) params += (params.empty() ? "" : ", ") + str(parameter, "label");
            const auto shape = lspFunctionShape("void fn(" + params + ")");
            for (size_t i = 0; i < shape.arguments.size() && i < arguments.size(); ++i)
                if (std::find(names.begin(), names.end(), shape.arguments[i]) != names.end())
                {
                    auto actual = infer(arguments[i].c_str(), cursor, depth + 1);
                    if (actual.empty()) continue;
                    auto old = values.find(shape.arguments[i]);
                    if (old != values.end() && old->second != actual) return "";
                    values[shape.arguments[i]] = actual;
                }
        }
        std::string substituted;
        size_t previous = 0;
        for (const auto &token : valueTokens(result))
        {
            substituted += result.substr(previous, token.a - previous);
            auto found = values.find(token.word);
            if (found == values.end() && std::find(names.begin(), names.end(), token.word) != names.end()) return "";
            substituted += found == values.end() ? token.word : found->second;
            previous = token.b;
        }
        return qualify(substituted + result.substr(previous), record.module);
    }

    /**
     * @brief 解析表达式类型。
     * @param value 接收者或初始化式。
     * @param cursor 可见位置。
     * @param depth 递归深度，限制循环初始化。
     * @return 唯一可确定的类型；未知返回空串。
     */
    std::string infer(std::string value, size_t cursor, int depth = 0) const
    {
        if (depth > 24) return "";
        value = trim(value);
        const auto originalValue = value;
        if (starts(value, "f\"")) return qualify("std::lang::String");
        auto t = valueTokens(value);
        if (t.empty()) return "";
        const auto shape = lspFunctionShape(value);
        if (shape.valid)
        {
            if (!shape.result.empty()) return lspFunctionText(shape);
            auto at = source.find(value, cursor);
            return at == source.npos ? "" : contextualType(at, depth + 1);
        }
        if (t.back().word == "?")
        {
            auto sourceType = infer(value.substr(0, t.back().a), cursor, depth + 1);
            return propagationTypes(sourceType).first;
        }
        if (t.front().word == "if" || t.front().word == "match")
        {
            std::set<std::string> results;
            auto origin = source.find(value, cursor);
            if (origin == source.npos) origin = cursor;
            for (size_t i = 0; i < t.size(); ++i)
                if (t[i].word == "{")
                {
                    if (t.front().word == "match" && (!i || t[i - 1].word != "=>")) continue;
                    auto end = groupEnd(t, i, "{", "}");
                    if (end <= i + 1) break;
                    bool complex = false;
                    for (size_t j = i + 1; j + 1 < end; ++j)
                        if (t[j].word == ";") complex = true;
                    if (!complex && end > i + 2)
                    {
                        auto result = infer(value.substr(t[i + 1].a, t[end - 2].b - t[i + 1].a), origin + t[i + 1].a, depth + 1);
                        if (!result.empty()) results.insert(result);
                    }
                    else
                        for (size_t j = i + 1; j + 1 < end; ++j)
                            if (t[j].word == "break")
                            {
                                size_t stop = j + 1;
                                while (stop + 1 < end && t[stop].word != ";") ++stop;
                                if (stop > j + 1)
                                {
                                    auto result = infer(value.substr(t[j + 1].a, t[stop - 1].b - t[j + 1].a), origin + t[j].a, depth + 1);
                                    if (!result.empty()) results.insert(result);
                                }
                            }
                    i = end - 1;
                }
            return results.size() == 1 ? *results.begin() : "";
        }
        if (t.front().word == "(" && groupEnd(t, 0, "(", ")") == t.size())
            return infer(value.substr(t.front().b, t.back().a - t.front().b), cursor, depth + 1);
        bool call = t.back().word == ")";
        Memory::Vector<Memory::String> arguments;
        if (call)
        {
            int level = 0;
            size_t open = t.size() - 1;
            for (size_t i = t.size(); i--;)
            {
                if (t[i].word == ")") ++level;
                if (t[i].word == "(" && !--level)
                {
                    open = i;
                    break;
                }
            }
            value = trim(value.substr(0, t[open].a));
            size_t ignored = 0;
            magel_macro::groupArguments(originalValue.c_str(), t[open].a, arguments, ignored);
            if (value.empty()) return "";
        }
        int level = 0;
        size_t dot = value.npos;
        auto words = valueTokens(value);
        for (auto &word : words)
        {
            if (word.word == "(" || word.word == "[" || word.word == "<")
                ++level;
            else if (word.word == ")" || word.word == "]" || word.word == ">")
                --level;
            else if (!level && word.word == ".")
                dot = word.a;
        }
        if (dot != value.npos)
        {
            auto receiver = infer(value.substr(0, dot), cursor, depth + 1);
            auto memberName = trim(value.substr(dot + 1));
            if (call && memberName == "toString" && arguments.empty() && !receiver.empty()) return "std::lang::String";
            auto entries = members(receiver, base(memberName));
            if (call) entries = selectedCalls(entries, arguments, cursor, depth + 1);
            std::set<std::string> results;
            for (auto &r : entries)
                if (call || r.kind == "member")
                {
                    auto result = resultType(r, receiver);
                    auto explicitAt = memberName.find('<');
                    if (explicitAt != memberName.npos && !memberName.empty() && memberName.back() == '>')
                    {
                        auto argument = memberName.substr(explicitAt + 1, memberName.size() - explicitAt - 2);
                        auto parameter = r.name == "mapErr" ? "F" : "E";
                        auto tokens = valueTokens(result);
                        std::string replaced;
                        size_t previous = 0;
                        for (auto &token : tokens)
                        {
                            replaced += result.substr(previous, token.a - previous);
                            replaced += token.word == parameter ? argument : token.word;
                            previous = token.b;
                        }
                        result = replaced + result.substr(previous);
                    }
                    if (!result.empty()) results.insert(result);
                }
            return results.size() == 1 ? *results.begin() : "";
        }
        auto variantAt = value.rfind("::");
        if (variantAt != value.npos)
        {
            auto owner = value.substr(0, variantAt), variant = value.substr(variantAt + 2);
            auto ownerName = base(qualify(owner));
            if ((ownerName == "std::lang::Result" && (variant == "Ok" || variant == "Err")) || (ownerName == "std::lang::Option" && (variant == "Value" || variant == "None"))) return qualify(owner);
        }
        if (!call)
        {
            const Binding *best = nullptr;
            auto scope = scopeAt(source, cursor);
            for (auto &b : vars)
                if (b.name == value && visible(b, scope, cursor) && (!best || b.scope.size() > best->scope.size() || (b.scope.size() == best->scope.size() && b.offset > best->offset))) best = &b;
            if (best)
            {
                if (best->kind == "function" && best->scope.size() == 1)
                {
                    size_t count = 0;
                    for (const auto &record : records)
                        if (record.kind == "function" && record.owner.empty() && (record.name == value || record.qualified == value)) ++count;
                    if (count > 1) return "";
                }
                return bindingType(*best, cursor, depth + 1);
            }
            if (words.size() == 1)
            {
                if (words[0].kind == 9) return words[0].word[0] == '"' ? "std::lang::String" : "char";
                if (value == "true" || value == "false") return "bool";
                const bool binaryUnsigned = value.size() > 2 && (value[0] == 'b' || value[0] == 'B') && value.back() == 'u' && std::all_of(value.begin() + 1, value.end() - 1, [](char ch) { return ch == '0' || ch == '1' || ch == '_'; });
                if (words[0].kind == 10 || binaryUnsigned)
                {
                    if (!value.empty() && value.back() == 'u') return "uint64";
                    const bool decimal = !starts(value, "0x") && !starts(value, "0X");
                    if (decimal && !value.empty() && value.back() == 'f') return "float32";
                    return decimal && value.find_first_of(".eE") != value.npos ? "float64" : "int64";
                }
            }
            return "";
        }
        auto name = base(value);
        if ((name == "move" || name == "copy" || name == "std::lang::move" || name == "std::lang::copy") && t.back().word == ")")
        {
            size_t open = 0;
            while (open < t.size() && t[open].word != "(") ++open;
            if (open < t.size())
            {
                auto operand = trim(originalValue.substr(t[open].b, t.back().a - t[open].b));
                if (!operand.empty() && operand[0] == '&') operand = trim(operand.substr(1));
                return infer(operand, cursor, depth + 1);
            }
        }
        for (auto &r : records)
            if (isType(r) && (r.qualified == name || r.name == name || ends(r.qualified, "::" + name))) return qualify(value);
        std::vector<Record> functions;
        for (const auto &record : records)
            if (record.kind == "function" && (record.qualified == name || (record.owner.empty() && record.name == name) || ends(record.qualified, "::" + name))) functions.push_back(record);
        if (functions.empty())
        {
            const auto callable = lspFunctionShape(infer(value, cursor, depth + 1));
            if (callable.valid) return qualify(callable.result);
        }
        std::set<std::string> results;
        for (auto &r : selectedCalls(functions, arguments, cursor, depth + 1))
            if (r.kind == "function" && (r.qualified == name || (r.owner.empty() && r.name == name) || ends(r.qualified, "::" + name)))
            {
                auto type = instantiateResult(r, resultType(r, ""), value, arguments, cursor, depth + 1);
                if (!type.empty() && base(type) != "auto") results.insert(type);
            }
        return results.size() == 1 ? *results.begin() : "";
    }

    /**
     * @brief 查询显式传播协议的成功及失败类型。
     * @param type 来源类型。
     * @return 成功类型与传播载荷。
     */
    std::pair<std::string, std::string> propagationTypes(const std::string &type) const
    {
        auto head = base(qualify(type));
        std::string signature = type;
        if (head == "std::lang::Result" || head == "std::lang::Option")
        {
            auto open = signature.find('<');
            if (open == signature.npos) return {};
            auto tokens = valueTokens(signature.substr(open));
            int level = 0;
            std::vector<std::string> arguments;
            std::string current;
            for (size_t i = 1; i < tokens.size(); ++i)
            {
                auto word = tokens[i].word;
                if (!level && (word == "," || word == ">"))
                {
                    arguments.push_back(current);
                    current.clear();
                    if (word == ">") break;
                }
                else
                {
                    current += word;
                    if (word == "<")
                        ++level;
                    else if (word == ">")
                        --level;
                }
            }
            if (arguments.empty()) return {};
            return { qualify(arguments[0]), arguments.size() > 1 ? "ErrorResidual<" + arguments[1] + ">" : "NoneResidual" };
        }
        for (auto &r : records)
            if (isType(r) && r.qualified == head)
            {
                auto tokens = valueTokens(r.signature);
                for (size_t i = 0; i + 2 < tokens.size(); ++i)
                    if ((tokens[i].word == "Try" || tokens[i].word == "TryMove") && tokens[i + 1].word == "<")
                    {
                        std::string output, residual;
                        int level = 0;
                        bool second = false;
                        for (size_t j = i + 2; j < tokens.size(); ++j)
                        {
                            auto word = tokens[j].word;
                            if (!level && word == ">")
                            {
                                Record result;
                                result.module = r.module;
                                result.type = output;
                                auto actualOutput = resultType(result, type);
                                result.type = residual;
                                return { actualOutput, resultType(result, type) };
                            }
                            if (!level && word == ",")
                            {
                                second = true;
                                continue;
                            }
                            (second ? residual : output) += word;
                            if (word == "<")
                                ++level;
                            else if (word == ">")
                                --level;
                        }
                    }
            }
        return {};
    }

    /**
     * @brief 获取指定类型的直接成员。
     * @param type 实例类型。
     * @param name 成员过滤名。
     * @return 所属类型唯一匹配的成员声明。
     */
    std::vector<Record> members(const std::string &type, const std::string &name = "", bool includeStatic = false) const
    {
        std::vector<Record> out;
        if (type.empty()) return out;
        auto owner = base(qualify(type));
        for (auto &r : records)
            if (!r.owner.empty() && r.name != tail(r.owner) && (includeStatic || !starts(r.signature, "static ")) && (r.module.empty() ? r.owner : r.module + "::" + r.owner) == owner && (name.empty() || r.name == name)) out.push_back(r);
        if (!includeStatic && (name.empty() || name == "toString") && std::none_of(out.begin(), out.end(), [](const Record &record) { return record.name == "toString" && record.parameters.empty(); }))
        {
            Record method;
            method.name = "toString";
            method.owner = owner;
            method.qualified = owner + "::toString";
            method.signature = "String toString()";
            method.type = "std::lang::String";
            method.documentation = "返回拥有型字符串。默认结构体使用 {值, ...}，List 使用 [...]，Set/Map 使用 {...}，元组使用 (...)；嵌套字符串加引号。自定义无参 toString 或 ImplicitConvert<String> 优先。";
            method.source = "source/epilogue/value_text.cpp";
            out.push_back(std::move(method));
        }
        return out;
    }
};
/**
 * @brief 查找与当前位置成对的括号。
 * @param records 当前文件的符号记录集合。
 * @param name 名称文本。
 * @param source 提供输入数据的来源。
 * @param cursor 当前编辑或查询的源码位置。
 * @param types 当前可见的类型定义集合。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<Record> matching(const std::vector<Record> &records,
                             const std::string &name, const std::string &source,
                             size_t cursor,
                             const std::set<std::string> &types)
{
    if (starts(name, "$"))
        return matching(records, name.substr(1), source, cursor, types);
    std::vector<Record> out;
    auto dot = name.rfind('.');
    if (dot != name.npos)
    {
        auto receiver = name.substr(0, dot), member = name.substr(dot + 1);
        ReceiverTypes resolver(records, source, types);
        return resolver.members(resolver.infer(receiver, cursor), member);
    }
    for (auto &r : records)
        if (name.find("::") != name.npos ? (r.qualified == name || ends(r.qualified, "::" + name)) : (r.name == name && r.owner.empty()))
            out.push_back(r);
    if (name.find("::") == name.npos && std::any_of(out.begin(), out.end(), [](const Record &entry) {
            return entry.module != "std::lang" && entry.kind == "function";
        }))
        out.erase(std::remove_if(out.begin(), out.end(), [](const Record &entry) {
                      return entry.module == "std::lang" && entry.kind == "function";
                  }),
                  out.end());
    return out;
}
/**
 * @brief 按 Magel 源码格式规则重新排版文本。
 * @param s 待处理的文本或节点文本值。
 * @retval 生成或读取的结果文本。
 */
std::string format(const std::string &s)
{
    std::string mask = s;
    std::set<int> protectedLines;
    for (auto &t : lex(s, true, true))
        if (t.kind == 8 || t.kind == 9)
        {
            int line = integer(get(position(s, t.a), "line"));
            if (s.substr(t.a, t.b - t.a).find('\n') != s.npos)
            {
                for (size_t p = t.a; p < t.b; ++p)
                {
                    protectedLines.insert(line);
                    if (s[p] == '\n')
                        ++line;
                }
                protectedLines.insert(line);
            }
            for (size_t p = t.a; p < t.b; ++p)
                if (mask[p] != '\n')
                    mask[p] = ' ';
        }
    std::istringstream input(s), code(mask);
    std::string line, m, out;
    int depth = 0, n = 0;
    while (std::getline(input, line) && std::getline(code, m))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto v = trim(m);
        int closing = 0;
        while (size_t(closing) < v.size() && v[closing] == '}')
            ++closing;
        auto content = trim(line);
        if (protectedLines.count(n) || starts(content, "$") || starts(content, "%"))
            out += line;
        else if (!content.empty())
            out +=
                std::string(size_t(std::max(0, depth - closing)) * 4, ' ') + content;
        out += '\n';
        depth = std::max(0, depth + int(std::count(m.begin(), m.end(), '{')) - int(std::count(m.begin(), m.end(), '}')));
        ++n;
    }
    return out;
}

// HTTP 包的完整 SHA-256，与源码内容指纹分别使用，不能互相替代。
/**
 * @brief HTTP 包的完整 SHA-256，与源码内容指纹分别使用，不能互相替代。
 * @param input 程序输入或待解码的字节流。
 * @retval 生成或读取的结果文本。
 */
std::string sha256(const std::string &input)
{
    static const uint32_t constants[] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    std::array<uint32_t, 8> hash{ 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                  0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    auto r = [](uint32_t v, int n) { return (v >> n) | (v << (32 - n)); };
    std::string data = input;
    uint64_t bits = uint64_t(data.size()) * 8;
    data += char(0x80);
    while (data.size() % 64 != 56)
        data += '\0';
    for (int i = 7; i >= 0; --i)
        data += char(bits >> (i * 8));
    for (size_t at = 0; at < data.size(); at += 64)
    {
        uint32_t w[64]{};
        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < 4; ++j)
                w[i] = (w[i] << 8) | uint8_t(data[at + i * 4 + j]);
        for (int i = 16; i < 64; ++i)
        {
            auto x = w[i - 15], y = w[i - 2];
            w[i] = w[i - 16] + (r(x, 7) ^ r(x, 18) ^ (x >> 3)) + w[i - 7] + (r(y, 17) ^ r(y, 19) ^ (y >> 10));
        }
        auto v = hash;
        for (int i = 0; i < 64; ++i)
        {
            auto a = v[0], b = v[1], c = v[2], d = v[3], e = v[4], f = v[5], g = v[6],
                 h = v[7];
            uint32_t first = h + (r(e, 6) ^ r(e, 11) ^ r(e, 25)) + ((e & f) ^ (~e & g)) + constants[i] + w[i],
                     second = (r(a, 2) ^ r(a, 13) ^ r(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            v = { first + second, a, b, c, d + first, e, f, g };
        }
        for (int i = 0; i < 8; ++i)
            hash[i] += v[i];
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto v : hash)
        out << std::setw(8) << v;
    return out.str();
}
/**
 * @brief 计算字节数据的 CRC32 校验值。
 * @param s 待处理的文本或节点文本值。
 * @retval 输入字节数据的 CRC32 校验值。
 */
uint32_t crc32(const std::string &s)
{
    uint32_t crc = ~0u;
    for (unsigned char c : s)
    {
        crc ^= c;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & uint32_t(-int(crc & 1)));
    }
    return ~crc;
}
/**
 * @brief 从归档字节流读取小端整数。
 * @param s 待处理的文本或节点文本值。
 * @param at 读取或扫描的当前位置。
 * @param count 元素数量或操作次数。
 * @retval 按小端字节序解码的整数值。
 */
uint32_t little(const std::string &s, size_t at, int count = 4)
{
    if (at > s.size() || size_t(count) > s.size() - at)
        throw std::runtime_error("压缩包截断");
    uint32_t value = 0;
    for (int i = count - 1; i >= 0; --i)
        value = (value << 8) | uint8_t(s[at + size_t(i)]);
    return value;
}
constexpr size_t downloadLimit = 64 * 1024 * 1024,
                 unpackLimit = 256 * 1024 * 1024;
/**
 * @brief 解压归档中的 DEFLATE 数据块。
 * @param data 输入或待写入的字节数据。
 * @param size 输入数据或目标区域的大小。
 * @retval 生成或读取的结果文本。
 */
std::string inflate(const std::string &data, size_t size)
{
    if (size > unpackLimit)
        throw std::runtime_error("在线依赖解压超出限制");
    std::string output(std::max<size_t>(1, size), '\0');
    size_t written = tinfl_decompress_mem_to_mem(output.data(), output.size(),
                                                 data.data(), data.size(), 0);
    if (written != size)
        throw std::runtime_error("无效 Deflate 压缩数据");
    output.resize(size);
    return output;
}
/**
 * @brief 从指定地址下载依赖资源。
 * @param url 依赖资源的下载地址。
 * @retval 生成或读取的结果文本。
 */
std::string download(const std::string &url)
{
#ifdef _WIN32
    struct Http
    {
        HINTERNET h = nullptr;
        /**
         * @brief 关闭下载过程持有的网络句柄。
         */
        ~Http()
        {
            if (h)
                WinHttpCloseHandle(h);
        }
    };
    auto wide = fs::u8path(url).wstring();
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
        DWORD(-1);
    if (!WinHttpCrackUrl(wide.c_str(), DWORD(wide.size()), 0, &parts) || (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS))
        throw std::runtime_error("仅支持 HTTP/HTTPS 来源");
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength),
        path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (path.empty())
        path = L"/";
    Http session{ WinHttpOpen(L"Magel/1.0.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
    if (!session.h)
        throw std::runtime_error("无法初始化 HTTP");
    WinHttpSetTimeouts(session.h, 30000, 30000, 30000, 30000);
    Http connection{ WinHttpConnect(session.h, host.c_str(), parts.nPort, 0) };
    if (!connection.h)
        throw std::runtime_error("无法连接依赖服务器");
    Http request{ WinHttpOpenRequest(
        connection.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) };
    if (!request.h || !WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.h, nullptr))
        throw std::runtime_error("在线依赖请求失败");
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(
            request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &status, &size, nullptr)
        || status < 200 || status >= 300)
        throw std::runtime_error("在线依赖 HTTP 状态：" + std::to_string(status));
    std::string out;
    char buffer[65536];
    DWORD used = 0;
    while (true)
    {
        if (!WinHttpReadData(request.h, buffer, sizeof(buffer), &used))
            throw std::runtime_error("在线依赖读取失败");
        if (!used)
            break;
        if (used > downloadLimit - out.size())
            throw std::runtime_error("在线依赖下载超过 64 MiB");
        out.append(buffer, used);
    }
    return out;
#else
    Temporary tmp;
    auto result = execute(
        { "curl", "--fail", "--location", "--proto", "=http,https",
          "--proto-redir", "=http,https", "--max-time", "30", "--max-filesize",
          "67108864", "--output", (tmp.path / "download").u8string(), "--", url },
        fs::current_path(), "", 35);
    if (result.code)
        throw std::runtime_error("在线依赖下载失败：" + result.error);
    if (fs::file_size(tmp.path / "download") > downloadLimit)
        throw std::runtime_error("在线依赖下载超过 64 MiB");
    return read(tmp.path / "download");
#endif
}
/**
 * @brief 解包依赖归档并校验目标路径。
 * @param data 输入或待写入的字节数据。
 * @param destination 解压文件的目标目录。
 * @retval 保存解包依赖归档并校验目标路径结果的对象。
 */
fs::path extract(std::string data, const fs::path &destination)
{
    size_t count = 0, total = 0;
    std::set<std::string> written;
    auto store = [&](std::string name, const std::string &content,
                     bool directory) {
        std::replace(name.begin(), name.end(), '\\', '/');
        auto p = fs::u8path(name);
        if (name.empty() || name[0] == '/' || name.find(':') != name.npos || name.find('\0') != name.npos)
            throw std::runtime_error("在线依赖包含不安全路径：" + name);
        for (auto &part : p)
            if (part == "..")
                throw std::runtime_error("在线依赖包含不安全路径：" + name);
        if (++count > 10000 || content.size() > unpackLimit - total)
            throw std::runtime_error("在线依赖解压大小或文件数量超出限制");
        total += content.size();
        auto target = destination / p;
        if (directory)
            fs::create_directories(target);
        else
        {
            if (!written.insert(p.lexically_normal().generic_u8string()).second)
                throw std::runtime_error("压缩包包含重复文件");
            fs::create_directories(target.parent_path());
            write(target, content);
        }
    };
    if (starts(data, "PK\003\004") || starts(data, "PK\005\006"))
    {
        size_t eocd = data.rfind("PK\005\006");
        if (eocd == data.npos || data.size() - eocd < 22)
            throw std::runtime_error("无效 ZIP 目录");
        if (little(data, eocd + 4, 2) || little(data, eocd + 6, 2))
            throw std::runtime_error("不支持分卷 ZIP");
        size_t entries = little(data, eocd + 10, 2), at = little(data, eocd + 16);
        if (entries > 10000)
            throw std::runtime_error("ZIP 文件数量超出限制");
        for (size_t n = 0; n < entries; ++n)
        {
            if (little(data, at) != 0x02014b50)
                throw std::runtime_error("无效 ZIP 文件项");
            auto flags = little(data, at + 8, 2), method = little(data, at + 10, 2),
                 crc = little(data, at + 16), compressed = little(data, at + 20),
                 size = little(data, at + 24), names = little(data, at + 28, 2),
                 extra = little(data, at + 30, 2), comment = little(data, at + 32, 2),
                 attributes = little(data, at + 38), local = little(data, at + 42);
            if (flags & 1 || size == 0xffffffffu || compressed == 0xffffffffu)
                throw std::runtime_error("不支持加密或 ZIP64 压缩包");
            uint32_t mode = (attributes >> 16) & 0170000;
            if (mode && mode != 0100000 && mode != 0040000)
                throw std::runtime_error("在线依赖不支持链接或特殊文件");
            if (at + 46 > data.size() || names > data.size() - at - 46)
                throw std::runtime_error("ZIP 文件名截断");
            auto name = data.substr(at + 46, names);
            if (little(data, local) != 0x04034b50)
                throw std::runtime_error("无效 ZIP 本地文件项");
            size_t body = size_t(local) + 30 + little(data, local + 26, 2) + little(data, local + 28, 2);
            if (body > data.size() || compressed > data.size() - body || size > unpackLimit - total)
                throw std::runtime_error("ZIP 数据截断或过大");
            auto bytes = data.substr(body, compressed);
            if (method == 8)
                bytes = inflate(bytes, size);
            else if (method != 0 || bytes.size() != size)
                throw std::runtime_error("不支持的 ZIP 压缩方法");
            if (crc32(bytes) != crc)
                throw std::runtime_error("ZIP CRC 不匹配");
            store(name, bytes, ends(name, "/"));
            at += 46 + names + extra + comment;
        }
    }
    else
    {
        if (data.size() > 2 && uint8_t(data[0]) == 0x1f && uint8_t(data[1]) == 0x8b)
        {
            if (data.size() < 18 || uint8_t(data[2]) != 8 || (uint8_t(data[3]) & 0xe0))
                throw std::runtime_error("无效 Gzip 头");
            size_t at = 10;
            unsigned flags = uint8_t(data[3]);
            if (flags & 4)
            {
                auto length = little(data, at, 2);
                at += 2 + length;
            }
            for (auto bit : { 8, 16 })
                if (flags & bit)
                {
                    auto end = data.find('\0', at);
                    if (end == data.npos)
                        throw std::runtime_error("Gzip 头截断");
                    at = end + 1;
                }
            if (flags & 2)
                at += 2;
            if (at > data.size() - 8)
                throw std::runtime_error("Gzip 头截断");
            auto expected = little(data, data.size() - 8),
                 size = little(data, data.size() - 4);
            data = inflate(data.substr(at, data.size() - 8 - at), size);
            if (crc32(data) != expected)
                throw std::runtime_error("Gzip CRC 不匹配");
        }
        if (data.size() > unpackLimit)
            throw std::runtime_error("TAR 包过大");
        auto octal = [&](size_t at, size_t width) {
            size_t value = 0;
            auto end = at + width;
            if (end > data.size())
                throw std::runtime_error("TAR 头截断");
            while (at < end && (data[at] == ' ' || data[at] == '\0'))
                ++at;
            for (; at < end && data[at] != '\0' && data[at] != ' '; ++at)
            {
                if (data[at] < '0' || data[at] > '7' || value > unpackLimit)
                    throw std::runtime_error("无效 TAR 数值");
                value = value * 8 + size_t(data[at] - '0');
            }
            return value;
        };
        auto tarText = [&](size_t at, size_t width) {
            auto value = data.substr(at, width);
            auto end = value.find('\0');
            if (end != value.npos)
                value.resize(end);
            return value;
        };
        std::string longName;
        for (size_t at = 0; at + 512 <= data.size();)
        {
            bool zero = true;
            for (size_t i = at; i < at + 512; ++i)
                if (data[i])
                {
                    zero = false;
                    break;
                }
            if (zero)
                break;
            size_t checksum = 0;
            for (size_t i = 0; i < 512; ++i)
                checksum += i >= 148 && i < 156 ? 32 : uint8_t(data[at + i]);
            if (checksum != octal(at + 148, 8))
                throw std::runtime_error("TAR 头校验失败");
            auto size = octal(at + 124, 12);
            auto name = longName.empty() ? tarText(at, 100) : longName;
            longName.clear();
            auto prefix = tarText(at + 345, 155);
            if (!prefix.empty())
                name = prefix + "/" + name;
            if (size > data.size() - at - 512 || size > unpackLimit - total)
                throw std::runtime_error("TAR 内容截断或过大");
            auto content = data.substr(at + 512, size);
            auto type = data[at + 156];
            if (type == 'L')
            {
                longName = content;
                if (!longName.empty() && longName.back() == '\0')
                    longName.pop_back();
            }
            else if (type == '0' || type == '\0' || type == '5')
                store(name, content, type == '5');
            else
                throw std::runtime_error("在线依赖不支持链接、特殊文件或扩展 TAR 项");
            at += 512 + ((size + 511) / 512) * 512;
        }
    }
    if (fs::is_regular_file(destination / "magel.toml"))
        return destination;
    std::vector<fs::path> children;
    for (auto &child : fs::directory_iterator(destination))
        children.push_back(child.path());
    if (children.size() == 1 && fs::is_directory(children[0]) && fs::is_regular_file(children[0] / "magel.toml"))
        return children[0];
    throw std::runtime_error("在线依赖压缩包根目录或唯一顶层目录需要 magel.toml");
}
const std::string toolVersion = "1.0.0";
/**
 * @brief 校验下载依赖的版本信息。
 * @param data 输入或待写入的字节数据。
 */
void checkVersion(const toml::Value &data)
{
    auto requirement = text(field(field(data, "package"), "compiler"));
    if (requirement.empty())
        return;
    if (!std::regex_match(requirement, std::regex(R"(>=\d+\.\d+\.\d+)")))
        throw std::runtime_error("package.compiler 必须为 >=X.Y.Z");
    auto parts = [](const std::string &s) {
        std::array<int, 3> values{};
        std::istringstream input(s);
        std::string word;
        for (auto &v : values)
        {
            std::getline(input, word, '.');
            v = std::stoi(word);
        }
        return values;
    };
    if (parts(toolVersion) < parts(requirement.substr(2)))
        throw std::runtime_error("编译器版本不满足 " + requirement);
}
/**
 * @brief 将下载结果替换到依赖目标文件。
 * @param from 替换操作的源文件路径。
 * @param to 替换操作的目标文件路径。
 */
void replaceFile(const fs::path &from, const fs::path &to)
{
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("无法更新锁文件：" + to.u8string());
#else
    fs::rename(from, to);
#endif
}
/**
 * @brief 计算工程依赖的本地存放目录。
 * @param base 起始地址、基值或目标表达式。
 * @param name 名称文本。
 * @param source 提供输入数据的来源。
 * @param offline 是否禁止联网并仅使用本地依赖。
 * @param refresh 是否刷新已有依赖缓存。
 * @param libraries 依赖库的存放目录。
 * @retval 保存计算工程依赖的本地存放目录结果的对象。
 */
fs::path dependencyRoot(const fs::path &base, const std::string &name,
                        const std::string &source, bool offline, bool refresh,
                        const fs::path &libraries)
{
    if (!std::regex_match(name, std::regex("[A-Za-z0-9_-]+")) || source.empty())
        throw std::runtime_error("非法依赖名称或来源：" + name);
    if (starts(source, "http://") || starts(source, "https://"))
    {
        std::smatch match;
        if (!std::regex_match(
                source, match,
                std::regex(R"((https?://[^#]+)#sha256=([0-9a-fA-F]{64}))")))
            throw std::runtime_error("HTTP 依赖必须使用 URL#sha256=完整64位摘要");
        auto digest = match[2].str();
        std::transform(digest.begin(), digest.end(), digest.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        auto root = libraries / (name + "-" + digest);
        if (fs::exists(root))
            return normalizedPath(root);
        if (offline || !refresh)
            throw std::runtime_error("离线缓存缺失：" + name + "；请先运行 magel deps lock");
        fs::create_directories(libraries);
        Temporary tmp;
        auto bytes = download(match[1]);
        if (sha256(bytes) != digest)
            throw std::runtime_error("在线依赖 SHA-256 不匹配：" + name);
        auto staging = libraries / (".download-" + tmp.path.filename().u8string());
        fs::create_directory(staging);
        struct Cleanup
        {
            fs::path p;
            /**
             * @brief 清理当前操作尚未释放的临时资源。
             */
            ~Cleanup()
            {
                std::error_code ec;
                fs::remove_all(p, ec);
            }
        } cleanup{ staging };
        auto package = extract(std::move(bytes), staging);
        manifest(package / "magel.toml");
        std::error_code ec;
        fs::rename(package, root, ec);
        if (ec && !fs::is_directory(root))
            throw std::runtime_error("无法发布依赖缓存：" + ec.message());
        return normalizedPath(root);
    }
    if (!starts(source, "git+"))
        return normalizedPath(
            base / fs::u8path(starts(source, "path:") ? source.substr(5) : source));
    std::smatch match;
    if (!std::regex_match(source, match,
                          std::regex(R"(git\+(.+)#([0-9a-fA-F]{40}))"))
        || starts(match[1], "-"))
        throw std::runtime_error("Git 依赖必须使用 git+URL#完整40位提交号");
    auto url = match[1].str(), commit = match[2].str();
    auto root = libraries / (name + "-" + commit);
    auto git = [&](std::vector<std::string> args) {
        auto result = execute(args, base, "", 120);
        if (result.code)
            throw std::runtime_error("Git 依赖操作失败：" + result.error);
        return trim(result.output);
    };
    if (!fs::exists(root))
    {
        auto legacy = base / ".magel" / "deps" / (name + "-" + commit);
        if (fs::exists(legacy))
            root = legacy;
        else
        {
            if (offline || !refresh)
                throw std::runtime_error("离线缓存缺失：" + name + "；请先运行 magel deps lock");
            fs::create_directories(libraries);
            Temporary tmp;
            auto staging =
                libraries / (".git-download-" + tmp.path.filename().u8string());
            struct Cleanup
            {
                fs::path p;
                /**
                 * @brief 清理当前操作尚未释放的临时资源。
                 */
                ~Cleanup()
                {
                    std::error_code ec;
                    fs::remove_all(p, ec);
                }
            } cleanup{ staging };
            git({ "git", "clone", "--no-checkout", "--", url, staging.u8string() });
            git({ "git", "-C", staging.u8string(), "checkout", "--detach", commit });
            fs::rename(staging, root);
        }
    }
    auto head = git({ "git", "-C", root.u8string(), "rev-parse", "HEAD" });
    std::transform(commit.begin(), commit.end(), commit.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (head != commit)
        throw std::runtime_error("依赖 " + name + " 缓存提交号不符");
    if (git({ "git", "-C", root.u8string(), "remote", "get-url", "origin" }) != url)
        throw std::runtime_error("依赖 " + name + " 缓存来源不符");
    return normalizedPath(root);
}
/**
 * @brief 为依赖更新取得工程锁。
 * @param requested 用户请求的依赖版本或集合。
 * @param offline 是否禁止联网并仅使用本地依赖。
 * @param refresh 是否刷新已有依赖缓存。
 * @param libraries 依赖库的存放目录。
 * @param active 当前正在处理的节点集合。
 * @retval 保存为依赖更新取得工程锁结果的对象。
 */
fs::path lockProject(const fs::path &requested, bool offline, bool refresh,
                     const fs::path &libraries, std::set<fs::path> &active)
{
    auto base = normalizedPath(requested);
    if (active.size() >= 128 || !active.insert(base).second)
        throw std::runtime_error("循环依赖或嵌套过深：" + base.u8string());
    struct Remove
    {
        std::set<fs::path> &active;
        fs::path p;
        /**
         * @brief 移除作用域结束时的临时锁文件。
         */
        ~Remove() { active.erase(p); }
    } cleanup{ active, base };
    auto data = manifest(base / "magel.toml");
    checkVersion(data);
    std::map<std::string, A> records;
    auto dependencies = field(data, "dependencies");
    if (auto table = dependencies.asTable())
        for (auto &entry : table->members())
        {
            auto source = text(entry.value);
            auto root =
                dependencyRoot(base, entry.key, source, offline, refresh, libraries);
            auto child = manifest(root / "magel.toml");
            if (auto nested = field(child, "dependencies").asTable())
            {
                if (!nested->members().empty())
                    lockProject(root, offline, refresh, libraries, active);
                else if (fs::exists(root / "magel.lock"))
                    lockProject(root, offline, refresh, libraries, active);
            }
            else if (fs::exists(root / "magel.lock"))
                lockProject(root, offline, refresh, libraries, active);
            checkVersion(child);
            auto relative = root.lexically_relative(base);
            if (relative.empty())
                relative = root;
            records[entry.key] =
                A{ source, relative.generic_u8string(),
                   text(field(field(child, "package"), "version")),
                   std::string(magel_package::dependencyFingerprint(root).c_str()) };
        }
    else if (data.find("dependencies"))
        throw std::runtime_error("dependencies 必须是表");
    auto path = base / "magel.lock";
    if (refresh)
    {
        std::string output = "[lock]\nversion = \"1\"\ncompiler = \"" + toolVersion + "\"\n\n[dependencies]\n";
        for (auto &entry : records)
            output += entry.first + " = " + J(entry.second).dump() + "\n";
        auto temp =
            base / (".magel-lock-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        try
        {
            write(temp, output);
            replaceFile(temp, path);
        } catch (...)
        {
            std::error_code ec;
            fs::remove(temp, ec);
            throw;
        }
    }
    else
    {
        auto lock = manifest(path);
        bool equal = text(field(field(lock, "lock"), "version")) == "1";
        auto table = field(lock, "dependencies").asTable();
        if (!table || table->members().size() != records.size())
            equal = false;
        else
            for (auto &entry : table->members())
            {
                auto it = records.find(entry.key);
                auto values = entry.value.asArray();
                if (it == records.end() || !values || values->size() != it->second.size())
                {
                    equal = false;
                    break;
                }
                for (size_t i = 0; i < values->size(); ++i)
                    if (!(*values)[i].asString() || text((*values)[i]) != str(it->second[i]))
                        equal = false;
            }
        if (!equal)
            throw std::runtime_error("锁文件与来源或内容不符：" + path.u8string() + "；运行 magel deps lock");
    }
    return path;
}
/**
 * @brief 执行依赖安装、校验或查询命令。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @param compiler 执行工程命令使用的编译器路径。
 * @retval 执行依赖安装、校验或查询命令。
 */
int deps(int argc, char **argv, const fs::path &compiler)
{
    bool offline = false;
    fs::path base = fs::current_path();
    if (argc < 3)
        throw std::runtime_error(
            "用法：magel deps lock|check [--project 目录] [--offline]");
    std::string action = argv[2];
    if (action == "--help" || action == "-h")
    {
        std::cout << MAGEL_TEXT(
            "用法：magel deps lock|check [--project 目录] [--offline]\n");
        return 0;
    }
    if (action != "lock" && action != "check")
        throw std::runtime_error("依赖操作必须是 lock 或 check");
    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--offline")
            offline = true;
        else if (arg == "--project" && i + 1 < argc)
            base = fs::u8path(argv[++i]);
        else
            throw std::runtime_error("未知依赖参数：" + arg);
    }
    base = normalizedPath(base);
    if (fs::is_regular_file(base))
        base = base.parent_path();
    while (!fs::exists(base / "magel.toml"))
    {
        auto next = base.parent_path();
        if (base == next || base.empty())
            throw std::runtime_error("找不到 magel.toml");
        base = next;
    }
    std::set<fs::path> active;
    auto lock = lockProject(base, offline, action == "lock",
                            compiler.parent_path() / "libraries", active);
    std::cout << (action == "lock" ? "依赖已锁定：" : "依赖校验通过：")
              << lock.u8string() << "\n";
    return 0;
}
/**
 * @brief 执行工程测试命令并汇总结果。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @param compiler 执行工程命令使用的编译器路径。
 * @retval 执行工程测试命令并汇总结果。
 */
int tests(int argc, char **argv, const fs::path &compiler)
{
    bool json = false, list = false;
    double timeout = 120;
    std::string filter, backend = "interpret";
    std::vector<fs::path> paths;
    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--json")
            json = true;
        else if (arg == "--list")
            list = true;
        else if (arg == "--filter" && i + 1 < argc)
            filter = argv[++i];
        else if (arg == "--backend" && i + 1 < argc)
            backend = argv[++i];
        else if (arg == "--timeout" && i + 1 < argc)
            timeout = std::stod(argv[++i]);
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << MAGEL_TEXT(
                "用法：magel test [路径...] [--filter 文本] [--backend "
                "interpret|aot|all] [--timeout 秒] [--json|--list]\n");
            return 0;
        }
        else if (starts(arg, "--"))
            throw std::runtime_error("未知测试参数：" + arg);
        else
            paths.push_back(fs::u8path(arg));
    }
    if (backend != "interpret" && backend != "aot" && backend != "all")
        throw std::runtime_error("不支持的测试后端");
    if (!std::isfinite(timeout) || timeout <= 0)
        throw std::runtime_error("测试超时必须为正数");
    if (paths.empty())
        paths.push_back("tests");
    std::set<fs::path> files;
    for (auto &path : paths)
    {
        if (fs::is_directory(path))
        {
            for (auto &entry : fs::recursive_directory_iterator(path))
                if (entry.is_regular_file() && entry.path().extension() == ".mgl" && (starts(entry.path().filename().u8string(), "test_") || ends(entry.path().filename().u8string(), "_test.mgl")))
                    files.insert(normalizedPath(entry.path()));
        }
        else if (path.extension() == ".mgl")
            files.insert(normalizedPath(path));
    }
    A results;
    int passed = 0, failed = 0, skipped = 0;
    for (auto &file : files)
    {
        if (!filter.empty() && file.u8string().find(filter) == std::string::npos)
            continue;
        if (list)
        {
            std::cout << file.u8string() << "\n";
            continue;
        }
        auto begin = std::chrono::steady_clock::now();
        std::string status = "passed", message;
        try
        {
            auto sidecar = file;
            sidecar.replace_extension(".test.toml");
            toml::Value config;
            if (fs::exists(sidecar))
                config = manifest(sidecar);
            auto skip = field(config, "skip");
            bool skipTest = skip.asBool().value_or(false) || !text(skip).empty();
            if (skipTest)
            {
                status = "skipped";
                message = text(skip);
            }
            else
            {
                Temporary temp;
                std::vector<std::string> arguments;
                if (auto a = field(config, "args").asArray())
                    for (auto &v : *a)
                    {
                        if (!v.asString())
                            throw std::runtime_error("测试 args 必须为字符串数组");
                        arguments.push_back(text(v));
                    }
                double limit = timeout;
                if (auto p = config.find("timeout"))
                    limit = p->asDouble().value_or(
                        double(p->asInt().value_or(int64_t(timeout))));
                if (!std::isfinite(limit) || limit <= 0)
                    throw std::runtime_error("测试超时必须为正数");
                int expected = int(field(config, "expected_exit").asInt().value_or(0));
                std::string first;
                bool hasFirst = false;
                for (auto &mode : backend == "all" ? std::vector<std::string>{ "interpret", "aot" } : std::vector<std::string>{ backend })
                {
                    auto output = temp.path / ("test-" + mode
#ifdef _WIN32
                                               + ".exe"
#endif
                                  );
                    std::vector<std::string> command;
                    if (mode == "interpret")
                        command = { compiler.u8string(), "interpret",
                                    file.u8string(), "--out",
                                    output.u8string(), "--" };
                    else
                    {
                        auto compilation =
                            execute({ compiler.u8string(), "aot", file.u8string(), "--out",
                                      output.u8string(), "--opt-level=0" },
                                    fs::current_path(), "", limit);
                        if (compilation.code)
                            throw std::runtime_error(compilation.error + compilation.output);
                        command = { output.u8string() };
                    }
                    command.insert(command.end(), arguments.begin(), arguments.end());
                    auto result = execute(command, fs::current_path(), "", limit);
                    result.output =
                        std::regex_replace(result.output, std::regex("\r\n"), "\n");
                    if (result.code != expected || result.error.find("timed out") != std::string::npos)
                        throw std::runtime_error(mode + " 退出码 " + std::to_string(result.code) + "：" + result.error);
                    auto expectedFile = file;
                    expectedFile.replace_extension(".expected");
                    if (fs::exists(expectedFile) && result.output != std::regex_replace(read(expectedFile), std::regex("\r\n"), "\n"))
                        throw std::runtime_error(mode + " 输出不符合 " + expectedFile.u8string() + "：" + result.output);
                    if (!result.error.empty())
                        throw std::runtime_error(mode + " 标准错误非空：" + result.error);
                    if (hasFirst && result.output != first)
                        throw std::runtime_error("各执行方式输出不同");
                    first = result.output;
                    hasFirst = true;
                }
            }
        } catch (const std::exception &e)
        {
            status = "failed";
            message = e.what();
        }
        int duration = int(std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - begin)
                               .count());
        results.push_back(object({ { "file", file.u8string() },
                                   { "status", status },
                                   { "message", message },
                                   { "durationMs", duration } }));
        if (status == "passed")
            ++passed;
        else if (status == "failed")
            ++failed;
        else
            ++skipped;
        if (!json)
            std::cout << "[" << status << "] " << file.u8string()
                      << (message.empty() ? "" : "：" + message) << "\n";
    }
    if (list)
        return 0;
    if (results.empty())
        throw std::runtime_error("没有找到测试文件（test_*.mgl 或 *_test.mgl）");
    if (json)
        std::cout << object({ { "schemaVersion", 1 },
                              { "tests", results },
                              { "summary", object({ { "passed", passed },
                                                    { "failed", failed },
                                                    { "skipped", skipped } }) } })
                         .dump()
                  << "\n";
    else
        std::cout << "通过 " << passed << "，失败 " << failed << "，跳过 "
                  << skipped << "\n";
    return failed ? 1 : 0;
}

/**
 * @brief 多个提示线程共用的只读标准库声明。
 */
struct LspLibrary
{
    std::vector<Record> standard;
    std::map<std::string, std::vector<std::string>> imports;
    std::set<std::string> types;
};
#include "native_lsp_macro_help.hpp"
#include "native_lsp_macro_tokens.hpp"
class Server
{
    fs::path compiler;
    using Document = std::pair<std::string, J>;
    using Documents = std::map<std::string, std::shared_ptr<const Document>>;
    Documents documents;
    std::vector<fs::path> roots;
    std::shared_ptr<LspLibrary> library;
    const std::vector<Record> &standard;
    const std::map<std::string, std::vector<std::string>> &standardImports;
    const std::set<std::string> &baseTypes;
    std::set<std::string> types;
    std::shared_ptr<LspCache> cache;
    std::shared_ptr<std::mutex> outputMutex;
    std::mutex stateMutex;
    std::map<std::string, std::shared_ptr<std::atomic<bool>>> requests, diagnostics;
    std::array<std::unique_ptr<LspQueue>, 6> queues;
    uint64_t revision = 0;
    std::string analysisGraph;
    std::set<std::string> usedDocuments;
    bool shutdown = false;
    std::shared_ptr<std::atomic<bool>> requestCancelled;
    /**
     * @brief 初始化语言服务器的缓存与请求调度状态。
     * @param parent 语言服务器父进程的信息。
     * @param snapshot 用于当前请求的一致性文档快照。
     * @param folders 工作区文件夹列表。
     */
    Server(const Server &parent, Documents snapshot, std::vector<fs::path> folders) : compiler(parent.compiler), documents(std::move(snapshot)), roots(std::move(folders)),
                                                                                      library(parent.library), standard(library->standard), standardImports(library->imports),
                                                                                      baseTypes(library->types), types(baseTypes), cache(parent.cache), outputMutex(parent.outputMutex) {}
    /**
     * @brief 取得指定 URI 对应的当前源码内容。
     * @param u 待处理文档的 URI。
     * @retval 生成或读取的结果文本。
     */
    std::string source(const std::string &u)
    {
        auto it = documents.find(u);
        return it == documents.end() ? cache->source(pathOf(u)) : it->second->first;
    }
    /**
     * @brief 取得语言服务器可见的源码路径。
     * @param base 起始地址、基值或目标表达式。
     * @param out 接收处理结果的输出对象。
     * @param visited 已遍历的文件路径集合。
     */
    void paths(const fs::path &base, std::vector<fs::path> &out,
               std::set<fs::path> &visited)
    {
        auto p = normalizedPath(base);
        if (!visited.insert(p).second)
            return;
        out.push_back(p);
        out.push_back(p / "modules");
        try
        {
            auto data = manifest(p / "magel.toml");
            for (auto key : { "search_paths", "import_paths", "include_paths" })
                if (auto a = field(field(data, "magel"), key).asArray())
                    for (auto &v : *a)
                        out.push_back(p / fs::u8path(text(v)));
            if (fs::is_regular_file(p / "magel.lock"))
            {
                auto lock = manifest(p / "magel.lock");
                if (auto table = field(lock, "dependencies").asTable())
                    for (auto &entry : table->members())
                        if (auto a = entry.value.asArray())
                            if (a->size() > 1)
                                paths(p / fs::u8path(text((*a)[1])), out, visited);
            }
        } catch (const std::exception &)
        {
        }
    }
    /**
     * @brief 收集当前工程内参与语言服务的文件路径。
     * @param file 目标文件或文件描述。
     * @retval 按处理顺序保存的结果元素集合。
     */
    std::vector<fs::path> projectPaths(const fs::path &file)
    {
        std::vector<fs::path> out;
        std::set<fs::path> visited;
        for (auto p = file.parent_path(); !p.empty();)
        {
            if (fs::exists(p / "magel.toml"))
            {
                paths(p, out, visited);
                break;
            }
            auto next = p.parent_path();
            if (next == p)
                break;
            p = next;
        }
        return out;
    }
    /**
     * @brief 收集指定文件的符号和依赖信息。
     * @param u 待处理文档的 URI。
     * @param s 待处理的文本或节点文本值。
     * @retval 按处理顺序保存的结果元素集合。
     */
    std::vector<Record> collect(const std::string &u, const std::string &s)
    {
        types = baseTypes;
        auto search = projectPaths(pathOf(u));
        std::set<std::string> visited, modules{ "std::lang", "std" };
        std::vector<Record> out;
        std::string graph;
        std::function<void(const std::string &, const std::string &)> visit =
            [&](const std::string &current, const std::string &body) {
                if (!visited.insert(current).second)
                    return;
                graph += current + ":" + lspHash(body) + "\n";
                const auto unit = cache->unit(current, body);
                const auto &entries = unit->records;
                for (auto &r : entries)
                {
                    if (r.kind == "struct" || r.kind == "enum" || r.kind == "interface")
                        types.insert(r.name);
                    out.push_back(r);
                }
                auto local = pathOf(current).parent_path();
                std::vector<fs::path> lookup{ local, local / "modules" };
                lookup.insert(lookup.end(), search.begin(), search.end());
                for (auto &module : unit->imports)
                {
                    modules.insert(module);
                    if (standardImports.count(module))
                        continue;
                    auto relative =
                        std::regex_replace(module, std::regex("::"), "/") + ".mgl";
                    for (auto &root : lookup)
                    {
                        auto target = uri(root / fs::u8path(relative));
                        try
                        {
                            auto child = source(target);
                            auto declared = directives(child, "module");
                            if (!declared.empty() && declared[0] == module)
                                visit(target, child);
                            break;
                        } catch (const std::exception &)
                        {
                        }
                    }
                }
                // %include 的宏也必须进入索引；只加载真实词法指令，不扫描字符串正文。
                for (const auto &name : unit->includes)
                {
                    for (auto &root : lookup)
                    {
                        auto target = uri(root / fs::u8path(name));
                        try
                        {
                            visit(target, source(target));
                            break;
                        } catch (const std::exception &)
                        {
                        }
                    }
                }
            };
        visit(u, s);
        cache->dependencies(u, visited);
        usedDocuments = visited;
        analysisGraph = lspHash(graph);
        std::vector<std::string> pending(modules.begin(), modules.end());
        while (!pending.empty())
        {
            auto name = pending.back();
            pending.pop_back();
            auto found = standardImports.find(name);
            if (found == standardImports.end())
                continue;
            for (auto &child : found->second)
                if (modules.insert(child).second)
                    pending.push_back(child);
        }
        for (auto &r : standard)
            if (modules.count(r.module))
                out.push_back(r);
        return out;
    }
    /**
     * @brief 刷新工作区文件及其符号信息。
     * @retval 生成或读取的结果文本。
     */
    std::map<std::string, std::string> workspace()
    {
        std::map<std::string, std::string> sources;
        for (auto &root : roots)
        {
            std::error_code ec;
            fs::recursive_directory_iterator it(
                root, fs::directory_options::skip_permission_denied, ec),
                end;
            for (; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }
                if (requestCancelled && requestCancelled->load()) throw std::runtime_error("请求已取消");
                auto name = it->path().filename().u8string();
                if (it->is_directory(ec) && (name == ".git" || name == "build" || name == "node_modules" || name == ".magel" || name == "tmp" || name == "libraries" || name == "bin"))
                {
                    it.disable_recursion_pending();
                    continue;
                }
                if (it->is_regular_file(ec) && it->path().extension() == ".mgl")
                    try
                    {
                        sources[uri(it->path())] = cache->source(it->path());
                    } catch (const std::exception &)
                    {
                    }
            }
        }
        for (auto &d : documents)
            sources[d.first] = d.second->first;
        return sources;
    }
    /**
     * @brief 查询指定文档或工作区的符号信息。
     * @param query 工作区符号名称的查询文本。
     * @retval 保存查询指定文档或工作区的符号信息结果的对象。
     */
    J symbols(const std::string &query)
    {
        A out;
        auto lower = [](std::string s) {
            for (auto &c : s)
                c = char(std::tolower((unsigned char)c));
            return s;
        };
        for (auto &file : workspace())
            for (auto &r : cache->unit(file.first, file.second)->records)
                if (lower(r.qualified).find(lower(query)) != std::string::npos)
                    out.push_back(
                        object({ { "name", r.qualified },
                                 { "kind", symbolKind(r) },
                                 { "containerName", r.module },
                                 { "location",
                                   object({ { "uri", r.source },
                                            { "range", range(file.second, r.offset,
                                                             r.offset + r.name.size()) } }) } }));
        return out;
    }
    /**
     * @brief 将内部符号类别转换为 LSP 文档符号类别。
     * @param r 符号、类型或请求记录。
     * @retval 协议规定的符号类别编号。
     */
    static int symbolKind(const Record &r)
    {
        if (r.kind == "struct")
            return 23;
        if (r.kind == "enum")
            return 10;
        if (r.kind == "interface")
            return 11;
        if (r.kind == "member")
            return 8;
        return r.owner.empty() ? 12 : 6;
    }
    /**
     * @brief 将内部符号类别转换为 LSP 补全类别。
     * @param r 符号、类型或请求记录。
     * @retval 协议规定的符号类别编号。
     */
    static int completionKind(const Record &r)
    {
        if (r.kind == "struct")
            return 22;
        if (r.kind == "enum")
            return 13;
        if (r.kind == "interface")
            return 8;
        if (r.kind == "member")
            return 5;
        return 3;
    }
    /**
     * @brief 将文本说明包装为 LSP 标记内容。
     * @param s 待处理的文本或节点文本值。
     * @retval 保存将文本说明包装为 LSP 标记内容结果的对象。
     */
    J markup(const std::string &s)
    {
        return object({ { "kind", "markdown" }, { "value", s } });
    }
    /**
     * @brief 将符号位置转换为 LSP 位置数组。
     * @param entries 待编码的位置或符号记录列表。
     * @retval 保存将符号位置转换为 LSP 位置数组结果的对象。
     */
    J locations(const std::vector<Record> &entries)
    {
        A out;
        for (auto &r : entries)
            if (!r.source.empty())
                try
                {
                    auto s = source(r.source);
                    out.push_back(object(
                        { { "uri", r.source },
                          { "range", range(s, r.offset, r.offset + r.name.size()) } }));
                } catch (const std::exception &)
                {
                }
        return out;
    }
    /**
     * @brief 将 JSON-RPC 消息封装并写入协议输出。
     * @param message 待报告或输出的信息文本。
     */
    void send(const J &message)
    {
        auto data = message.dump();
        std::lock_guard<std::mutex> lock(*outputMutex);
        std::cout << "Content-Length: " << data.size() << "\r\n\r\n"
                  << data;
        std::cout.flush();
    }
    /**
     * @brief 向语言客户端发送通知消息。
     * @param method 压缩方法编号或目标方法名称。
     * @param params 形参或类型参数列表。
     */
    void notify(const std::string &method, J params)
    {
        send(object({ { "jsonrpc", "2.0" },
                      { "method", method },
                      { "params", std::move(params) } }));
    }
    /**
     * @brief 计算文档内容与分析配置的缓存指纹。
     * @param file 目标文件或文件描述。
     * @retval 生成或读取的结果文本。
     */
    std::string fingerprint(const std::string &file)
    {
        const auto target = uri(fs::u8path(file));
        try
        {
            return lspHash(source(target));
        } catch (...) { return "missing"; }
    }
    /**
     * @brief 生成前端检查报告并复用有效缓存。
     * @param u 待处理文档的 URI。
     * @param cancelled 请求取消标志。
     * @retval 保存生成前端检查报告并复用有效缓存结果的对象。
     */
    J checkReport(const std::string &u, const std::function<bool()> &cancelled)
    {
        const auto &body = documents.at(u)->first;
        collect(u, body);
        const auto file = pathOf(u), saved = cache->path("diagnostics", u);
        try
        {
            const auto entry = parse(read(saved));
            bool valid = integer(get(entry, "schema")) == 1 && integer(get(entry, "language"), -1) == int(magel_i18n::language()) && str(entry, "input") == lspHash(body) && str(entry, "graph") == analysisGraph && get(entry, "dependencies").asObject();
            if (valid)
                for (const auto &dependency : get(entry, "dependencies").asObject()->members())
                    if (fingerprint(std::string(dependency.key)) != str(dependency.value))
                    {
                        valid = false;
                        break;
                    }
            if (valid && get(entry, "report").find("diagnostics"))
            {
                ++cache->diagnosticHits;
                cache->touch(cache->path("diagnostics", u));
                return get(entry, "report");
            }
        } catch (...) {}
        if (cancelled()) return J();
        Temporary temporary;
        json::Object overlays;
        for (const auto &document : documents)
        {
            const auto path = pathOf(document.first);
            if (!path.empty()) overlays[normalizedPath(path).u8string()] = document.second->first;
        }
        write(temporary.path / "overlays.json", J(std::move(overlays)).dump());
        ++cache->checks;
        const auto result = execute({ compiler.u8string(), "--check", "--diagnostics=json", "--incremental", "--source-stdin",
                                      "--cache-dir", (cache->directory / "preprocess").u8string(), "--source-overlays", (temporary.path / "overlays.json").u8string(),
                                      "--analysis-deps", (temporary.path / "dependencies.json").u8string(), file.u8string() },
                                    file.parent_path(), body, 30, cancelled);
        if (result.cancelled || cancelled()) return J();
        const auto report = parse(result.output);
        try
        {
            const auto dependencies = parse(read(temporary.path / "dependencies.json"));
            if (get(dependencies, "complete").asBool() && *get(dependencies, "complete").asBool())
            {
                json::Object hashes;
                std::set<std::string> dependencyUris;
                for (const auto &dependency : array(get(dependencies, "dependencies")))
                {
                    const auto name = str(dependency);
                    if (!name.empty() && name.front() != '<')
                    {
                        hashes[normalizedPath(fs::u8path(name)).u8string()] = fingerprint(name);
                        dependencyUris.insert(uri(fs::u8path(name)));
                    }
                }
                cache->dependencies(u, dependencyUris);
                // 配置和锁文件的创建、删除也必须使已缓存的诊断失效。
                for (auto parent = file.parent_path(); !parent.empty();)
                {
                    for (const auto name : { "magel.toml", "magel.lock" })
                    {
                        const auto config = (parent / name).u8string();
                        hashes[config] = fingerprint(config);
                    }
                    const auto next = parent.parent_path();
                    if (next == parent) break;
                    parent = next;
                }
                cache->save(saved, object({ { "schema", 1 }, { "language", int(magel_i18n::language()) }, { "input", lspHash(body) }, { "graph", analysisGraph }, { "dependencies", J(std::move(hashes)) }, { "report", report } }));
            }
        } catch (...) {}
        return report;
    }
    /**
     * @brief 对文档执行检查并发布诊断。
     * @param u 待处理文档的 URI。
     * @param cancelled 请求取消标志。
     * @retval 保存对文档执行检查并发布诊断结果的对象。
     */
    J diagnose(const std::string &u, const std::function<bool()> &cancelled)
    {
        const auto &document = *documents.at(u);
        const auto unit = cache->unit(u, document.first);
        LspIndexScope index(*unit);
        A diagnostics;
        try
        {
            auto report = checkReport(u, cancelled);
            if (cancelled()) return J();
            for (auto &entry : array(get(report, "diagnostics")))
            {
                auto &span = get(entry, "span");
                int line = std::max(0, integer(get(span, "line")) - 1),
                    column = std::max(0, integer(get(span, "column")) - 1),
                    end = std::max(column + 1, integer(get(span, "endColumn")) - 1);
                size_t start = offsetAt(document.first,
                                        object({ { "line", line }, { "character", 0 } }));
                auto a = position(document.first, std::min(document.first.size(),
                                                           start + size_t(column))),
                     b = position(document.first,
                                  std::min(document.first.size(), start + size_t(end)));
                auto filename = str(span, "file"), message = str(entry, "message");
                if (str(entry, "code") == "MGL1205")
                {
                    static const std::regex nameAtEnd("[:：][ \\t]*([A-Za-z_][A-Za-z0-9_]*)[ \\t]*$");
                    std::smatch name;
                    if (std::regex_search(message, name, nameAtEnd))
                    {
                        auto stop = document.first.find('\n', start);
                        if (stop == document.first.npos) stop = document.first.size();
                        for (const auto &token : unit->tokens)
                            if (token.a >= start && token.a < stop && token.word == name[1].str())
                            {
                                a = position(document.first, token.a);
                                b = position(document.first, token.b);
                                break;
                            }
                    }
                }
                if (!filename.empty() && filename != "<stdin>" && filename != "<标准输入>" && normalizedPath(fs::u8path(filename)) != normalizedPath(pathOf(u)))
                {
                    a = object({ { "line", 0 }, { "character", 0 } });
                    b = object({ { "line", 0 }, { "character", 1 } });
                    message += "\n" + filename + ":" + std::to_string(line + 1);
                }
                diagnostics.push_back(
                    object({ { "range", object({ { "start", a }, { "end", b } }) },
                             { "severity", str(entry, "severity") == "warning" ? 2 : 1 },
                             { "source", "magel" },
                             { "code", get(entry, "code") },
                             { "message", message } }));
            }
        } catch (const std::exception &e)
        {
            diagnostics.push_back(
                object({ { "range", range(document.first, 0,
                                          std::min<size_t>(1, document.first.size())) },
                         { "severity", 1 },
                         { "source", "magel" },
                         { "message", std::string("编译器检查失败：") + e.what() } }));
        }
        // 命名风格只产生编辑器警告，和编译器错误独立，不改变程序是否可编译。
        for (auto &diagnostic : diagnostics) diagnostic = ownershipInformation(document.first, u, std::move(diagnostic));
        static const std::regex methodName("[a-z][A-Za-z0-9]*");
        static const std::regex typeName("[A-Z][a-z0-9]*([A-Z][a-z0-9]+)*");
        for (const auto &record : unit->records)
        {
            const bool type = record.kind == "struct" || record.kind == "enum" || record.kind == "interface";
            const bool function = record.kind == "function";
            if ((!type && !function) || starts(record.name, "__") || (function && (record.name == tail(record.owner) || starts(record.signature, record.name + "("))))
                continue;
            if (std::regex_match(record.name, type ? typeName : methodName)) continue;
            diagnostics.push_back(object({ { "range", range(document.first, record.offset, record.offset + record.name.size()) },
                                           { "severity", 2 },
                                           { "source", "magel" },
                                           { "code", type ? "MGL2101" : "MGL2102" },
                                           { "message", (type ? "类型名应使用大驼峰（PascalCase）：" :
                                                                "函数或方法名应使用小驼峰（camelCase）：")
                                                            + record.name } }));
        }
        return object({ { "uri", u },
                        { "version", document.second },
                        { "diagnostics", diagnostics } });
    }
    /**
     * @brief 为文档生成语义着色词法项。
     * @param s 待处理的文本或节点文本值。
     * @param records 当前文件的符号记录集合。
     * @retval 保存为文档生成语义着色词法项结果的对象。
     */
    J semantic(const std::string &s, const std::vector<Record> &records)
    {
        const auto unit = activeUnit ? std::shared_ptr<const LspUnit>() : cache->unit("semantic", s);
        const auto &raw = activeUnit ? activeUnit->raw : unit->raw;
        const auto &t = activeUnit ? activeUnit->tokens : unit->tokens;
        std::map<size_t, std::pair<int, int>> classes;
        std::set<std::string> generic;
        std::set<std::string> known = types;
        known.insert(primitives.begin(), primitives.end());
        for (size_t i = 0; i + 1 < t.size(); ++i)
        {
            if (t[i].word == "struct" || t[i].word == "enum" || t[i].word == "interface" || t[i].word == "type")
            {
                known.insert(t[i + 1].word);
                classes[t[i + 1].a] = { 1, 1 };
                if (i + 2 < t.size() && t[i + 2].word == "<")
                {
                    auto end = groupEnd(t, i + 2, "<", ">");
                    for (size_t g = i + 3; g + 1 < end; ++g)
                        if (identifier(t[g].word) && (t[g - 1].word == "<" || t[g - 1].word == ","))
                        {
                            generic.insert(t[g].word);
                            classes[t[g].a] = { 2, 1 };
                        }
                }
            }
            if (t[i + 1].word == "<" && identifier(t[i].word))
            {
                auto end = groupEnd(t, i + 1, "<", ">");
                if (end < t.size() && t[end].word == "(" && i && (known.count(t[i - 1].word) || t[i - 1].word == ")" || t[i - 1].word == ">"))
                {
                    for (size_t g = i + 2; g + 1 < end; ++g)
                        if (identifier(t[g].word) && (t[g - 1].word == "<" || t[g - 1].word == ","))
                        {
                            generic.insert(t[g].word);
                            classes[t[g].a] = { 2, 1 };
                        }
                }
            }
        }
        known.insert(generic.begin(), generic.end());
        auto vars = bindings(s, known);
        std::map<std::string, std::vector<const Binding *>> names;
        for (const auto &binding : vars) names[binding.name].push_back(&binding);
        for (auto &b : vars)
            classes[b.offset] = { b.kind == "function" ? 6 : b.kind == "parameter" ? 3 :
                                                                                     4,
                                  1 };
        std::set<std::string> functions;
        for (auto &r : records)
            if (r.kind == "function" || r.kind == "macro")
                functions.insert(r.name);
        A data;
        int lastLine = 0, lastCol = 0;
        const auto macroTokens = lspMacroTokens(s, t);
        size_t coveredUntil = 0;
        for (auto &token : raw)
        {
            if (token.a < coveredUntil) continue;
            int kind = 11, mod = 0;
            size_t tokenEnd = token.b;
            auto word = token.word;
            auto at =
                std::lower_bound(t.begin(), t.end(), token.a,
                                 [](const Token &v, size_t p) { return v.a < p; });
            size_t index = size_t(at - t.begin());
            auto prev = index ? t[index - 1].word : "",
                 next = index + 1 < t.size() ? t[index + 1].word : "";
            const auto macro = macroTokens.find(token.a);
            if (macro != macroTokens.end() && token.kind != 8 && token.kind != 9)
            {
                kind = macro->second.kind;
                mod = macro->second.modifiers;
                tokenEnd = macro->second.end;
                coveredUntil = tokenEnd;
            }
            else if (token.kind)
                kind = token.kind;
            else if (classes.count(token.a))
            {
                kind = classes[token.a].first;
                mod = classes[token.a].second;
            }
            else if (primitives.count(word))
                kind = 1;
            else if (keywords.count(word))
                kind = 7;
            else if (identifier(word))
            {
                const Binding *best = nullptr;
                auto scope = scopeAt(s, token.a);
                for (const auto *item : names[word])
                {
                    const auto &b = *item;
                    if (b.name == word && visible(b, scope, token.a) && (!best || b.scope.size() > best->scope.size() || (b.scope.size() == best->scope.size() && b.offset > best->offset)))
                        best = &b;
                }
                if (prev == ".")
                    kind = next == "(" ? 6 : 5;
                else if (best)
                    kind = best->kind == "function" ? 6 : best->kind == "parameter" ? 3 :
                                                                                      4;
                else if (generic.count(word))
                    kind = 2;
                else if (known.count(word))
                    kind = 1;
                else if (next == "::" || prev == "module" || prev == "import")
                    kind = 0;
                else if (next == "(" || functions.count(word))
                    kind = 6;
                else
                    kind = 4;
            }
            size_t start = token.a;
            while (start < tokenEnd)
            {
                auto end = s.find('\n', start);
                if (end == s.npos || end > tokenEnd)
                    end = tokenEnd;
                auto stop = end;
                if (stop > start && s[stop - 1] == '\r')
                    --stop;
                if (stop > start)
                {
                    auto p = position(s, start), e = position(s, stop);
                    int line = integer(get(p, "line")),
                        col = integer(get(p, "character")),
                        length = integer(get(e, "character")) - col;
                    data.emplace_back(line - lastLine);
                    data.emplace_back(line == lastLine ? col - lastCol : col);
                    data.emplace_back(length);
                    data.emplace_back(kind);
                    data.emplace_back(mod);
                    lastLine = line;
                    lastCol = col;
                }
                start = end + 1;
            }
        }
        return object({ { "data", data } });
    }
    /**
     * @brief 为指定宏或源码位置生成展开预览。
     * @param u 待处理文档的 URI。
     * @param s 待处理的文本或节点文本值。
     * @retval 保存为指定宏或源码位置生成展开预览结果的对象。
     */
    J preview(const std::string &u, const std::string &s)
    {
        auto file = pathOf(u);
        if (file.empty())
            throw std::runtime_error("展开预览需要文件 URI");
        auto dirs = projectPaths(file);
        preprocess::Options options = preprocess::Preprocessor::forMetro();
        Memory::Vector<Memory::String> paths;
        for (auto &p : dirs)
        {
            paths.push_back(p.u8string().c_str());
            options.includeDirs.push_back(p.u8string().c_str());
        }
        options.includeLoader = [&](const preprocess::IncludeRequest &request) {
            auto target = uri(fs::u8path(std::string(request.resolvedPath.c_str())));
            auto it = documents.find(target);
            return it == documents.end() ? preprocess::IncludeResult::useDefault() : preprocess::IncludeResult::inlineText(it->second->first.c_str(), request.resolvedPath);
        };
        magel_module::Loader loader(options, paths);
        preprocess::Result processed;
        Memory::String error;
        bool ok = loader.rootText(s.c_str(), file.u8string().c_str(),
                                  file.parent_path().u8string().c_str(), processed,
                                  error);
        if (ok)
            ok = magel_macro::run(processed, error, &loader.moduleByFile());
        A diagnostics;
        for (auto &d : processed.diagnostics)
            diagnostics.push_back(d.toString().c_str());
        if (!ok && !error.empty())
            diagnostics.push_back(error.c_str());
        A mappings;
        if (ok)
            for (size_t line = 0; line < processed.lineMap.size(); ++line)
            {
                const auto &origin = processed.lineMap[line];
                if (!origin.line) continue;
                const auto file = std::string(origin.file.c_str());
                const auto target = file.empty() || file == "<stdin>" ? u : uri(fs::u8path(file));
                mappings.push_back(object({ { "expandedLine", int(line) }, { "uri", target }, { "sourceLine", int(origin.line - 1) } }));
            }
        A temporaries;
        if (ok)
        {
            auto records = collect(u, s);
            std::set<std::string> seen;
            for (const auto &token : lex(processed.text.c_str()))
            {
                if (!starts(token.word, "__magel_macro_") || !seen.insert(token.word).second) continue;
                for (const auto &record : records)
                {
                    if (record.kind != "macro" || record.source.empty() || !starts(record.signature, "$macro")) continue;
                    const auto prefix = "__magel_macro_" + std::string(magel_macro::sanitizeMacroName(record.name.c_str()).c_str()) + "_";
                    if (!starts(token.word, prefix)) continue;
                    const auto split = token.word.find('_', prefix.size());
                    if (split == token.word.npos) continue;
                    const auto expansionId = token.word.substr(prefix.size(), split - prefix.size());
                    if (expansionId.empty() || expansionId.find_first_not_of("0123456789") != expansionId.npos) continue;
                    auto slot = token.word.substr(split + 1);
                    if (starts(slot, "negative_")) slot = "-" + slot.substr(9);
                    const auto digits = starts(slot, "-") ? slot.substr(1) : slot;
                    if (digits.empty() || digits.find_first_not_of("0123456789") != digits.npos) continue;
                    auto definition = source(record.source);
                    J location = object({ { "uri", record.source }, { "range", range(definition, record.offset, record.offset + record.name.size()) } });
                    const auto end = definition.find("$end_macro", record.offset);
                    const Memory::String raw(definition.c_str());
                    for (const auto &part : lex(definition, true))
                    {
                        if (part.a <= record.offset || part.a >= end || part.kind == 8 || part.kind == 9 || !starts(part.word, "$")) continue;
                        const auto stop = magel_macro::placeholderEnd(raw, part.a);
                        if (stop <= part.a + 1) continue;
                        if (magel_macro::gensymName(record.name.c_str(), 0, raw.substr(part.a + 1, stop - part.a - 1)) != magel_macro::gensymName(record.name.c_str(), 0, slot.c_str())) continue;
                        location = object({ { "uri", record.source }, { "range", range(definition, part.a, stop) } });
                        break;
                    }
                    temporaries.push_back(object({ { "name", token.word }, { "slot", "$" + slot }, { "macro", record.name }, { "expansionId", token.word.substr(prefix.size(), split - prefix.size()) }, { "definition", location } }));
                    break;
                }
            }
        }
        return object({ { "text", ok ? std::string(processed.text.c_str()) : "" },
                        { "diagnostics", diagnostics },
                        { "mappings", mappings },
                        { "temporaries", temporaries } });
    }
#include "native_lsp_features.hpp"
    /**
     * @brief 处理一个语言服务器请求并构造结果。
     * @param method 压缩方法编号或目标方法名称。
     * @param params 形参或类型参数列表。
     * @retval 保存处理一个语言服务器请求并构造结果结果的对象。
     */
    J request(const std::string &method, const J &params)
    {
        if (method == "initialize")
        {
            roots.clear();
            for (auto &folder : array(get(params, "workspaceFolders")))
                roots.push_back(pathOf(str(folder, "uri")));
            if (roots.empty() && !str(params, "rootUri").empty())
                roots.push_back(pathOf(str(params, "rootUri")));
            A legend;
            for (auto n : { "namespace", "type", "typeParameter", "parameter",
                            "variable", "property", "function", "keyword", "comment",
                            "string", "number", "operator" })
                legend.emplace_back(n);
            return object(
                { { "capabilities",
                    object(
                        { { "positionEncoding", "utf-16" },
                          { "textDocumentSync", 2 },
                          { "hoverProvider", true },
                          { "completionProvider",
                            object({ { "triggerCharacters", A{ ":", ".", "$", "%" } } }) },
                          { "definitionProvider", true },
                          { "referencesProvider", true },
                          { "renameProvider", object({ { "prepareProvider", true } }) },
                          { "inlayHintProvider", true },
                          { "documentSymbolProvider", true },
                          { "workspaceSymbolProvider", true },
                          { "documentFormattingProvider", true },
                          { "signatureHelpProvider",
                            object({ { "triggerCharacters", A{ "(", ",", "[", "<", "{" } },
                                     { "retriggerCharacters", A{ "," } } }) },
                          { "semanticTokensProvider",
                            object({ { "legend", object({ { "tokenTypes", legend },
                                                          { "tokenModifiers",
                                                            A{ "declaration", "readonly" } } }) },
                                     { "full", true } }) },
                          { "experimental",
                            object({ { "macroExpansionProvider", true } }) } }) },
                  { "serverInfo", object({ { "name", "Magel" }, { "version", "1.0.0" } }) } });
        }
        if (method == "shutdown")
        {
            shutdown = true;
            return J();
        }
        if (method == "workspace/symbol")
            return symbols(str(params, "query"));
        if (method == "magel/lspStats") return cache->stats();
        auto u = str(get(params, "textDocument"), "uri");
        auto d = documents.find(u);
        if (d == documents.end())
            return J();
        const auto unit = cache->unit(u, d->second->first);
        const auto &s = *unit->text;
        LspIndexScope index(*unit);
        size_t cursor = offsetAt(s, get(params, "position"));
        auto records = collect(u, s);
        auto vars = bindings(s, types);
        if (method == "textDocument/references" || method == "textDocument/prepareRename" || method == "textDocument/rename")
            return refactor(method, params, u, s, records, cursor);
        if (method == "textDocument/inlayHint") return inlayHints(s, records, get(params, "range"));
        if (method == "magel/expandMacro")
            return preview(u, s);
        if (method == "textDocument/semanticTokens/full")
            return semantic(s, records);
        if (method == "textDocument/formatting")
        {
            auto output = format(s);
            return output == s ? J(A{}) : J(A{ object({ { "range", range(s, 0, s.size()) }, { "newText", output } }) });
        }
        if (method == "textDocument/documentSymbol")
        {
            A out;
            std::set<size_t> seen;
            for (auto &r : unit->records)
            {
                seen.insert(r.offset);
                out.push_back(
                    object({ { "name", r.name },
                             { "kind", symbolKind(r) },
                             { "range", range(s, r.offset, r.offset + r.name.size()) },
                             { "selectionRange",
                               range(s, r.offset, r.offset + r.name.size()) } }));
            }
            for (auto &b : vars)
                if (!seen.count(b.offset))
                    out.push_back(
                        object({ { "name", b.name },
                                 { "kind", b.kind == "function" ? 12 : 13 },
                                 { "range", range(s, b.offset, b.end) },
                                 { "selectionRange", range(s, b.offset, b.end) } }));
            return out;
        }
        if (method == "textDocument/signatureHelp")
        {
            auto macro = lspMacroSignature(s, cursor, records);
            if (!macro.isNull()) return macro;
            auto t = lex(s.substr(0, cursor));
            struct Call
            {
                std::string word;
                size_t at;
                int active;
            };
            std::vector<Call> stack;
            for (size_t i = 0; i < t.size(); ++i)
            {
                auto w = t[i].word;
                bool angle = false;
                if (w == "<" && i && identifier(t[i - 1].word))
                {
                    auto end = groupEnd(t, i, "<", ">");
                    angle = end < t.size() && (t[end].word == "(" || t[end].word == "::" || t[end].word == ">");
                }
                if (w == "(" || w == "[" || w == "{" || angle)
                    stack.push_back({ w, t[i].a, 0 });
                else if (w == ")" || w == "]" || w == "}" || (w == ">" && !stack.empty() && stack.back().word == "<"))
                {
                    if (!stack.empty())
                        stack.pop_back();
                }
                else if (w == "," && !stack.empty())
                    ++stack.back().active;
            }
            for (auto it = stack.rbegin(); it != stack.rend(); ++it)
                if (it->word == "(")
                {
                    for (size_t at = 2; at < t.size(); ++at)
                        if (t[at].a == it->at && t[at - 2].word == "fn" && (t[at - 1].word == "move" || t[at - 1].word == "copy" || t[at - 1].word == "const" || t[at - 1].word == "mut")) return J();
                    auto name = expression(s, it->at);
                    if (name.empty())
                        continue;
                    auto entries = matching(records, name, s, cursor, types);
                    A signatures;
                    const auto callable = lspFunctionShape(ReceiverTypes(records, s, types).infer(name, cursor));
                    if (callable.valid)
                    {
                        A params;
                        for (const auto &argument : callable.arguments) params.push_back(object({ { "label", argument } }));
                        signatures.push_back(object({ { "label", lspFunctionText(callable) }, { "parameters", params } }));
                    }
                    int active = 0;
                    bool chosen = false;
                    for (auto &r : entries)
                        if (r.kind == "function" || r.kind == "macro")
                        {
                            if (!chosen && int(r.parameters.size()) > it->active)
                            {
                                active = int(signatures.size());
                                chosen = true;
                            }
                            signatures.push_back(
                                object({ { "label", r.signature },
                                         { "documentation", markup(r.documentation) },
                                         { "parameters", r.parameters } }));
                        }
                    if (signatures.empty())
                        return J();
                    return object({ { "signatures", signatures },
                                    { "activeSignature", active },
                                    { "activeParameter", it->active } });
                }
            return J();
        }
        if (method == "textDocument/completion")
        {
            for (auto &token : lex(s, true))
                if (token.a < cursor && cursor <= token.b && (token.kind == 8 || token.kind == 9) && (cursor < token.b || starts(token.word, "//") || (starts(token.word, "/*") && !ends(token.word, "*/")) || (token.kind == 9 && token.word.back() != token.word.front())))
                    return A{};
            auto name = expression(s, cursor);
            auto separator = name.rfind("::"), dot = name.rfind('.');
            bool member = dot != name.npos, qualifiedName = separator != name.npos;
            std::string receiver;
            if (member)
                receiver = name.substr(0, dot);
            else if (qualifiedName)
                receiver = name.substr(0, separator);
            std::set<std::string> modules;
            for (auto &m : standardImports)
                modules.insert(m.first);
            auto workspaceSymbols = symbols("");
            for (auto &symbol : array(workspaceSymbols))
                if (!str(symbol, "containerName").empty())
                    modules.insert(str(symbol, "containerName"));
            if (qualifiedName && !member)
            {
                std::string selected;
                for (const auto &m : modules)
                    if ((receiver == m || starts(receiver, m + "::")) && m.size() > selected.size())
                        selected = m;
                if (!selected.empty())
                    records = collect(u, "import " + selected + ";\n" + s);
            }
            A out;
            auto addImport = [&](J &item, const std::string &module) {
                auto imported = directives(s, "import");
                if (module.empty() || module == "std::lang" || module == "std" || std::find(imported.begin(), imported.end(), module) != imported.end())
                    return;
                auto t = lex(s);
                size_t insertion = 0;
                if (!t.empty() && t[0].word == "module")
                    for (auto &token : t)
                        if (token.word == ";")
                        {
                            insertion = token.b;
                            break;
                        }
                (*item.asObject())["additionalTextEdits"] = A{
                    object({ { "range", range(s, insertion, insertion) },
                             { "newText", (insertion ? "\n" : "") + std::string("import ") + module + ";\n" } })
                };
            };
            if (!member && !qualifiedName)
            {
                auto scope = scopeAt(s, cursor);
                std::map<std::string, Binding> names;
                for (auto &b : vars)
                    if (visible(b, scope, cursor))
                        names[b.name] = b;
                ReceiverTypes resolver(records, s, types);
                for (auto &entry : names)
                    if (entry.second.kind != "function")
                    {
                        auto type = resolver.infer(entry.first, cursor);
                        if (type.empty()) type = entry.second.type;
                        out.push_back(object({ { "label", entry.first }, { "kind", 6 }, { "detail", type + " " + entry.first } }));
                    }
                for (auto &word : keywords)
                    out.push_back(object({ { "label", word }, { "kind", 14 } }));
            }
            if (member)
                records = matching(records, receiver + ".", s, cursor, types);
            // 同一模块的重载共享短名称；跨模块同名或可见局部遮蔽时保留限定名。
            std::map<std::string, std::set<std::string>> completionNames;
            const auto completionScope = scopeAt(s, cursor);
            for (const auto &record : records)
                if (record.owner.empty())
                    completionNames[record.name].insert(record.qualified);
            auto shortCompletion = [&](const Record &record) {
                auto owners = completionNames[record.name];
                if (record.module != "std::lang") owners.erase("std::lang::" + record.name);
                if (owners.size() != 1)
                    return false;
                for (const auto &binding : vars)
                    if (binding.name == record.name && visible(binding, completionScope, cursor) && !(record.source == u && (binding.offset == record.offset || (record.kind == "function" && binding.kind == "function" && binding.scope.size() == 1))))
                        return false;
                return true;
            };
            std::set<std::string> ownerTypes;
            for (auto &r : records)
            {
                if (!member && qualifiedName)
                {
                    auto prefix =
                        r.qualified.substr(0, r.qualified.size() - r.name.size() - 2);
                    if (prefix != receiver && !(receiver.find("::") == receiver.npos && ends(prefix, "::" + receiver)))
                        continue;
                }
                else if (!member && !r.owner.empty())
                    continue;
                auto label =
                    member || qualifiedName || r.source == u || r.kind == "macro" ? r.name : r.qualified;
                const auto insertion =
                    r.kind == "macro" && (starts(r.signature, "$macro") || starts(r.signature, "$replace")) ? (starts(name, "$") ? "" : "$") + r.name : (member || qualifiedName || shortCompletion(r) ? r.name : r.qualified);
                J item = object({ { "label", label },
                                  { "insertText", insertion },
                                  { "kind", completionKind(r) },
                                  { "detail", r.signature },
                                  { "documentation", markup(r.documentation) } });
                if (qualifiedName)
                    addImport(item, r.module);
                out.push_back(item);
            }
            if (qualifiedName)
                for (auto &r : standard)
                    if (r.module == receiver && !r.owner.empty())
                        ownerTypes.insert(r.owner.substr(0, r.owner.find("::")));
            for (auto &name : ownerTypes)
            {
                if (std::any_of(records.begin(), records.end(), [&](const Record &entry) {
                        return entry.name == name && entry.owner.empty() && (entry.kind == "struct" || entry.kind == "enum" || entry.kind == "interface");
                    })) continue;
                auto item = object({ { "label", name },
                                     { "insertText", name },
                                     { "kind", 7 },
                                     { "detail", "类型 " + receiver + "::" + name } });
                addImport(item, receiver);
                out.push_back(item);
            }
            auto t = lex(s.substr(0, cursor));
            bool importContext = false;
            std::string prefix;
            for (size_t i = t.size(); i > 0; --i)
            {
                if (t[i - 1].word == ";" || t[i - 1].word == "{" || t[i - 1].word == "}")
                    break;
                if (t[i - 1].word == "import")
                {
                    importContext = true;
                    for (size_t j = i; j < t.size(); ++j)
                        prefix += t[j].word;
                    break;
                }
            }
            if (importContext)
                out.clear();
            else if (qualifiedName)
                prefix = receiver + "::";
            if (!member)
            {
                auto split = prefix.rfind("::");
                auto base = split == prefix.npos ? "" : prefix.substr(0, split + 2);
                std::set<std::string> choices;
                for (auto &m : modules)
                    if (starts(m, prefix) && m.size() > base.size())
                        choices.insert(m.substr(
                            base.size(), m.find("::", base.size()) == m.npos ? m.size() - base.size() : m.find("::", base.size()) - base.size()));
                for (auto &choice : choices)
                {
                    if (importContext && base == "std::" && choice == "lang") continue;
                    out.push_back(object({ { "label", choice },
                                           { "insertText", choice },
                                           { "kind", 9 },
                                           { "detail", "模块 " + base + choice } }));
                }
            }
            return out;
        }
        auto tokens = lex(s);
        if (method == "textDocument/hover")
            for (size_t at = 1; at < tokens.size(); ++at)
            {
                const auto &token = tokens[at];
                if (token.a <= cursor && cursor <= token.b && tokens[at - 1].word == "fn" && (token.word == "move" || token.word == "copy" || token.word == "const" || token.word == "mut"))
                {
                    const auto message = token.word == "move"  ? MAGEL_TEXT("按值捕获并转移外部变量所有权；创建闭包后，被移出的源变量不能继续读取。") :
                                         token.word == "copy"  ? MAGEL_TEXT("按值捕获外部变量的副本；原变量仍可继续使用。") :
                                         token.word == "const" ? MAGEL_TEXT("按只读借用捕获外部变量；借用来源必须在闭包使用期间保持有效。") :
                                                                 MAGEL_TEXT("按可写借用捕获外部变量；闭包读写当前值，借用来源必须保持有效。");
                    return object({ { "contents", markup("```magel\n" + tokens[at - 1].word + " " + token.word + "(...) { ... }\n```\n\n" + message) },
                                    { "range", range(s, token.a, token.b) } });
                }
            }
        if (method == "textDocument/hover" || method == "textDocument/definition")
            for (const auto &part : tokens)
                if (part.word == "?" && part.a <= cursor && cursor <= part.b)
                {
                    ReceiverTypes resolver(records, s, types);
                    auto value = expression(s, part.a);
                    auto sourceType = resolver.infer(value, part.a);
                    auto propagation = resolver.propagationTypes(sourceType);
                    if (propagation.first.empty()) return J();
                    std::string description = "成功类型：`" + propagation.first + "`\n\n来源类型：`" + sourceType + "`\n\n传播载荷：`" + propagation.second + "`";
                    const Binding *function = nullptr;
                    for (auto &b : vars)
                        if (b.kind == "function" && b.offset < cursor && (!function || b.offset > function->offset)) function = &b;
                    bool hasMove = sourceType.find("std::lang::Result<") == 0 || sourceType.find("std::lang::Option<") == 0;
                    for (auto &r : records)
                        if (r.qualified == sourceType.substr(0, sourceType.find('<')) && r.signature.find("TryMove") != std::string::npos) hasMove = true;
                    bool moving = hasMove && !value.empty() && value.back() == ')';
                    description += "\n\n拆分入口：`" + sourceType + "::" + (moving ? "intoBranch()" : "branch()") + "`；传播模式：" + (moving ? "移动" : "读取");
                    if (method == "textDocument/definition")
                    {
                        auto definitions = resolver.members(sourceType, moving ? "intoBranch" : "branch");
                        if (function)
                        {
                            auto receivers = resolver.members(function->type, "fromResidual", true);
                            definitions.insert(definitions.end(), receivers.begin(), receivers.end());
                        }
                        return locations(definitions);
                    }
                    if (function) description += "\n\n接收入口：`" + function->type + "::fromResidual(" + propagation.second + ")`";
                    if (function) description += "\n\n退出范围：当前函数 `" + function->name + "`；目标返回类型：`" + function->type + "`";
                    return object({ { "contents", markup(description) }, { "range", range(s, part.a, part.b) } });
                }
        auto token =
            std::find_if(tokens.begin(), tokens.end(), [&](const Token &t) {
                return t.a <= cursor && cursor <= t.b && (identifier(t.word) || (starts(t.word, "$") && identifier(t.word.substr(1))));
            });
        if (token == tokens.end())
            return J();
        auto name = expression(s, token->b);
        auto entries = matching(records, name, s, cursor, types);
        auto scope = scopeAt(s, cursor);
        const Binding *binding = nullptr;
        for (auto &b : vars)
            if (b.name == token->word && (b.offset == token->a || visible(b, scope, cursor)) && (!binding || b.scope.size() > binding->scope.size() || (b.scope.size() == binding->scope.size() && b.offset > binding->offset)))
                binding = &b;
        const auto identity = resolveSymbol(u, s, records, std::min(cursor, token->b - 1));
        if (identity.valid())
            entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const Record &entry) { return entry.source != identity.source || entry.offset != identity.offset; }), entries.end());
        if (name.find("::") == name.npos && name.find('.') == name.npos && binding && binding->kind != "function")
            entries.clear();
        if (method == "textDocument/definition")
        {
            auto locationsFound = locations(entries);
            if (!array(locationsFound).empty())
                return locationsFound;
            if (binding)
                return object(
                    { { "uri", u }, { "range", range(s, binding->offset, binding->end) } });
            return J();
        }
        if (method == "textDocument/hover")
        {
            if (token->word == "auto")
                for (auto &b : vars)
                    if (b.offset >= token->b && std::all_of(tokens.begin(), tokens.end(), [&](const Token &part) { return part.a < token->b || part.a >= b.offset || part.word == "const" || part.word == "mut" || part.word == "&"; }))
                    {
                        binding = &b;
                        break;
                    }
            std::string content;
            for (auto &r : entries)
            {
                if (!content.empty())
                    content += "\n\n---\n\n";
                content += "```magel\n" + r.signature + "\n```\n\n" + r.documentation;
            }
            if (content.empty() && binding)
            {
                auto type = binding->type;
                if (starts(type, "auto") || binding->kind == "function")
                {
                    auto inferred = ReceiverTypes(records, s, types).bindingType(*binding, std::max(cursor, binding->end));
                    if (!inferred.empty()) type = inferred;
                }
                content = "```magel\n" + type + " " + binding->name + "\n```";
                if (starts(binding->type, "auto"))
                {
                    content += "\n\n" + lspTypeDescription(type);
                    content += binding->context ? "\n\n推断来源：外层调用的参数类型。" : !binding->iterable.empty() ? "\n\n推断来源：遍历对象的元素类型。" :
                                                                                                                      "\n\n推断来源：变量初始化表达式。";
                }
            }
            if (content.empty())
                return J();
            return object({ { "contents", markup(content) },
                            { "range", range(s, token->a, token->b) } });
        }
        throw std::runtime_error("不支持的 LSP 请求：" + method);
    }

    /**
     * @brief 为文档安排延迟诊断任务。
     * @param u 待处理文档的 URI。
     * @param delay 延迟启动诊断的等待时长。
     */
    void scheduleDiagnostics(const std::string &u, int delay = 250)
    {
        Documents snapshot;
        std::vector<fs::path> folders;
        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        std::shared_ptr<const Document> document;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            const auto found = documents.find(u);
            if (found == documents.end()) return;
            if (diagnostics.count(u)) diagnostics[u]->store(true);
            diagnostics[u] = cancelled;
            snapshot = documents;
            folders = roots;
            document = found->second;
        }
        queues[5]->post(u, [this, u, cancelled, document, snapshot = std::move(snapshot), folders = std::move(folders)] {
      if (cancelled->load()) return;
      try {
        Server view(*this, snapshot, folders);
        const auto result = view.diagnose(u, [cancelled] { return cancelled->load(); });
        std::lock_guard<std::mutex> lock(stateMutex);
        const auto found = documents.find(u);
        if (!cancelled->load() && found != documents.end() && found->second == document && !result.isNull())
          notify("textDocument/publishDiagnostics", result);
      } catch (const std::exception &e) {
        if (!cancelled->load()) notify("window/logMessage",object({{"type",1},{"message",e.what()}}));
      } }, cancelled, delay);
    }
    /**
     * @brief 将协议消息分派到相应的请求处理函数。
     * @param message 待报告或输出的信息文本。
     */
    void dispatch(const J &message)
    {
        const auto method = str(message, "method"), u = str(get(get(message, "params"), "textDocument"), "uri");
        const auto id = get(message, "id");
        const auto key = id.dump();
        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        Documents snapshot;
        std::vector<fs::path> folders;
        uint64_t version;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            requests[key] = cancelled;
            snapshot = documents;
            folders = roots;
            version = revision;
        }
        const size_t lane = method == "workspace/symbol" ? 3 : method == "magel/expandMacro"                                               ? 4 :
                                                           (method == "textDocument/completion" || method == "textDocument/signatureHelp") ? 0 :
                                                           (method == "textDocument/hover" || method == "textDocument/definition")         ? 1 :
                                                                                                                                             2;
        queues[lane]->post("", [this, message, id, key, u, cancelled, version, snapshot = std::move(snapshot), folders = std::move(folders)] {
      J response;
      try {
        if (cancelled->load()) throw std::runtime_error("请求已取消");
        Server view(*this,snapshot,folders);
        view.requestCancelled=cancelled;
        const auto result = view.request(str(message,"method"),get(message,"params"));
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          if (u.empty() || str(message,"method")=="textDocument/rename" || str(message,"method")=="textDocument/references") {if(revision != version)cancelled->store(true);}
          else {
            const auto current=documents.find(u);const auto previous=snapshot.find(u);
            if(current==documents.end() || previous==snapshot.end() || current->second!=previous->second)cancelled->store(true);
            for(const auto &dependency:view.usedDocuments) {
              const auto before=snapshot.find(dependency);const auto after=documents.find(dependency);
              if(before!=snapshot.end()) {
                if(after==documents.end() || before->second->first!=after->second->first)cancelled->store(true);
              } else if(after!=documents.end()) {
                try {if(cache->source(pathOf(dependency))!=after->second->first)cancelled->store(true);} catch(...) {cancelled->store(true);}
              }
            }
          }
        }
        response = object({{"jsonrpc","2.0"},{"id",id},{"result",result}});
      } catch (const std::exception &e) {
        response = object({{"jsonrpc","2.0"},{"id",id},{"error",object({{"code",-32602},{"message",e.what()}})}});
      }
      if (cancelled->load()) {
        ++cache->cancelled;
        response = object({{"jsonrpc","2.0"},{"id",id},{"error",object({{"code",-32800},{"message","请求已取消或文档已更新"}})}});
      }
      send(response);
      std::lock_guard<std::mutex> lock(stateMutex); requests.erase(key); }, cancelled);
    }
    /**
     * @brief 结束会话并收尾异步工作。
     */
    void finish()
    {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            for (auto &entry : diagnostics) entry.second->store(true);
        }
        if (queues[5]) queues[5]->stop();
        for (size_t i = 0; i < 5; ++i)
            if (queues[i]) queues[i]->drain();
    }

public:
    /**
     * @brief 创建原生服务并加载内嵌标准库声明。
     * @param path 编译器路径。
     */
    explicit Server(fs::path path) : compiler(std::move(path)), library(std::make_shared<LspLibrary>()),
                                     standard(library->standard), standardImports(library->imports), baseTypes(library->types),
                                     cache(std::make_shared<LspCache>(compiler)), outputMutex(std::make_shared<std::mutex>())
    {
        auto data = parse(magel_development_tool("tools/magel_stdlib_api.json"));
        // `__` 前缀是编译器/宿主协议：标准库里带这个前缀的函数与类型不进补全。
        // 工程内同名符号属于用户自己的命名空间，不受此限。
        for (auto &v : array(get(data, "types")))
            if (!starts(str(v), "__")) library->types.insert(str(v));
        A entries = array(get(data, "functions"));
        for (const auto &symbol : array(get(data, "symbols"))) entries.push_back(symbol);
        for (auto &r : entries)
        {
            if (starts(str(r, "name"), "__") || starts(str(r, "owner"), "__")) continue;
            Record entry;
            entry.module = str(r, "module");
            entry.name = tail(str(r, "name"));
            entry.owner = str(r, "owner");
            if (!str(r, "kind").empty()) entry.kind = str(r, "kind");
            entry.type = str(r, "type");
            entry.qualified = entry.module + "::" + str(r, "name");
            entry.signature = str(r, "signature");
            entry.documentation =
                str(r, "documentation") + "\n\n" + doc(str(r, "comment"));
            entry.parameters = array(get(r, "parameters"));
            library->standard.push_back(entry);
        }
        if (auto modules = get(data, "modules").asObject())
            for (auto &m : modules->members())
            {
                auto &out = library->imports[m.key];
                for (auto &v : array(m.value))
                    out.push_back(str(v));
            }
        types = baseTypes;
    }
    /**
     * @brief 处理 stdio JSON-RPC 请求。
     * @return 正常关闭为零，协议错误为一。
     */
    int run()
    {
        for (auto &queue : queues) queue = std::make_unique<LspQueue>();
        queues[3]->post("cleanup", [this] { cache->cleanup(); }, std::make_shared<std::atomic<bool>>(false));
        auto nextCleanup = std::chrono::steady_clock::now() + std::chrono::minutes(15);
        std::string line;
        while (true)
        {
            size_t length = 0;
            bool found = false;
            while (std::getline(std::cin, line))
            {
                if (line == "\r" || line.empty())
                    break;
                auto colon = line.find(':');
                if (colon == line.npos)
                    continue;
                auto key = line.substr(0, colon);
                std::transform(key.begin(), key.end(), key.begin(),
                               [](unsigned char c) { return char(std::tolower(c)); });
                if (key == "content-length")
                {
                    auto value = trim(line.substr(colon + 1));
                    if (value.empty() || value.find_first_not_of("0123456789") != value.npos)
                        return 1;
                    length = std::stoull(value);
                    found = true;
                }
            }
            if (!found)
            {
                finish();
                return shutdown ? 0 : 1;
            }
            if (length > 16 * 1024 * 1024)
                return 1;
            std::string payload(length, '\0');
            std::cin.read(payload.data(), std::streamsize(length));
            if (size_t(std::cin.gcount()) != length)
                return 1;
            J message;
            try
            {
                message = parse(payload);
            } catch (const std::exception &)
            {
                send(object({ { "jsonrpc", "2.0" },
                              { "id", J() },
                              { "error", object({ { "code", -32700 },
                                                  { "message", "Parse error" } }) } }));
                continue;
            }
            auto method = str(message, "method");
            auto &params = get(message, "params");
            if (method == "exit")
            {
                finish();
                return shutdown ? 0 : 1;
            }
            if (std::chrono::steady_clock::now() >= nextCleanup)
            {
                queues[3]->post("cleanup", [this] { cache->cleanup(); }, std::make_shared<std::atomic<bool>>(false));
                nextCleanup = std::chrono::steady_clock::now() + std::chrono::minutes(15);
            }
            if (message.find("id"))
            {
                if (method != "initialize" && method != "shutdown" && method != "magel/lspStats")
                {
                    dispatch(message);
                    continue;
                }
                if (method == "shutdown") finish();
                try
                {
                    send(object({ { "jsonrpc", "2.0" },
                                  { "id", get(message, "id") },
                                  { "result", request(method, params) } }));
                } catch (const std::exception &e)
                {
                    send(object(
                        { { "jsonrpc", "2.0" },
                          { "id", get(message, "id") },
                          { "error", object({ { "code", -32602 }, { "message", e.what() } }) } }));
                }
                continue;
            }
            try
            {
                if (method == "$/cancelRequest")
                {
                    std::shared_ptr<std::atomic<bool>> flag;
                    {
                        std::lock_guard<std::mutex> lock(stateMutex);
                        const auto found = requests.find(get(params, "id").dump());
                        if (found != requests.end())
                        {
                            flag = found->second;
                            flag->store(true);
                        }
                    }
                    if (flag)
                        for (auto &queue : queues) queue->cancel(flag);
                    continue;
                }
                auto &item = get(params, "textDocument");
                auto u = str(item, "uri");
                if (method == "textDocument/didClose")
                {
                    {
                        std::lock_guard<std::mutex> lock(stateMutex);
                        documents.erase(u);
                        ++revision;
                        if (diagnostics.count(u))
                        {
                            diagnostics[u]->store(true);
                            diagnostics.erase(u);
                        }
                    }
                    notify("textDocument/publishDiagnostics",
                           object({ { "uri", u }, { "diagnostics", A{} } }));
                    cache->forget(u);
                    for (const auto &dependent : cache->dependents(u))
                        if (documents.count(dependent)) scheduleDiagnostics(dependent);
                }
                else if (method == "textDocument/didOpen" || method == "textDocument/didChange" || method == "textDocument/didSave")
                {
                    {
                        std::lock_guard<std::mutex> lock(stateMutex);
                        if (method == "textDocument/didOpen")
                            documents[u] = std::make_shared<const Document>(str(item, "text"), get(item, "version"));
                        else if (method == "textDocument/didChange")
                        {
                            auto changes = array(get(params, "contentChanges"));
                            if (!changes.empty())
                            {
                                auto body = documents.count(u) ? documents[u]->first : std::string();
                                for (const auto &change : changes)
                                {
                                    if (change.find("range"))
                                    {
                                        const auto &span = get(change, "range");
                                        const auto begin = offsetAt(body, get(span, "start")), end = offsetAt(body, get(span, "end"));
                                        if (end < begin) throw std::runtime_error("无效增量范围");
                                        body.replace(begin, end - begin, str(change, "text"));
                                    }
                                    else
                                        body = str(change, "text");
                                }
                                documents[u] = std::make_shared<const Document>(std::move(body), get(item, "version"));
                            }
                        }
                        else if (params.find("text"))
                            documents[u] = std::make_shared<const Document>(str(params, "text"), documents.count(u) ? documents[u]->second : J());
                        ++revision;
                    }
                    if (documents.count(u) && !pathOf(u).empty())
                        scheduleDiagnostics(u, method == "textDocument/didSave" ? 0 : 250);
                    for (const auto &dependent : cache->dependents(u))
                        if (documents.count(dependent)) scheduleDiagnostics(dependent);
                }
                else if (method == "workspace/didChangeWatchedFiles")
                {
                    {
                        std::lock_guard<std::mutex> lock(stateMutex);
                        ++revision;
                    }
                    for (const auto &change : array(get(params, "changes")))
                    {
                        const auto changed = str(change, "uri");
                        cache->invalidate(pathOf(changed));
                        for (const auto &dependent : cache->dependents(changed))
                            if (documents.count(dependent)) scheduleDiagnostics(dependent);
                        if (documents.count(changed)) scheduleDiagnostics(changed);
                    }
                }
                else if (method == "workspace/didChangeWorkspaceFolders")
                {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    ++revision;
                    auto &event = get(params, "event");
                    for (auto &folder : array(get(event, "removed")))
                    {
                        auto path = pathOf(str(folder, "uri"));
                        roots.erase(std::remove(roots.begin(), roots.end(), path),
                                    roots.end());
                    }
                    for (auto &folder : array(get(event, "added")))
                        roots.push_back(pathOf(str(folder, "uri")));
                }
            } catch (const std::exception &e)
            {
                notify("window/logMessage",
                       object({ { "type", 1 }, { "message", e.what() } }));
            }
        }
    }
};
#include "native_man.hpp"
/**
 * @brief 执行命令行源码格式化任务。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @retval 执行命令行源码格式化任务。
 */
int fmt(int argc, char **argv)
{
    bool check = false, save = false;
    std::set<fs::path> files;
    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--check")
            check = true;
        else if (arg == "--write")
            save = true;
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << MAGEL_TEXT("用法：magel fmt 路径... [--check|--write]\n");
            return 0;
        }
        else if (starts(arg, "--"))
            throw std::runtime_error("未知格式化参数：" + arg);
        else
        {
            auto path = fs::u8path(arg);
            if (fs::is_directory(path))
            {
                for (auto &entry : fs::recursive_directory_iterator(path))
                    if (entry.is_regular_file() && entry.path().extension() == ".mgl")
                        files.insert(entry.path());
            }
            else
                files.insert(path);
        }
    }
    if (files.empty() || (check && save) || (!check && !save && files.size() > 1))
        throw std::runtime_error("请指定文件；多个文件需使用 --check 或 --write");
    bool changed = false;
    for (auto &file : files)
    {
        auto original = read(file),
             normalized = std::regex_replace(original, std::regex("\r\n"), "\n"),
             output = format(original);
        if (normalized != output)
        {
            changed = true;
            if (save)
                write(file, output);
            if (check)
                std::cerr << "需要格式化：" << file.u8string() << "\n";
        }
        if (!check && !save)
            std::cout << output;
    }
    return check && changed ? 1 : 0;
}
}  // namespace
/**
 * @brief 分派原生开发命令。
 * @param argc 参数数量。
 * @param argv 原始参数。
 * @param compiler 当前工具链绝对路径。
 * @return 命令退出码。
 */
int run(int argc, char **argv, const fs::path &compiler)
{
    try
    {
        if (argc > 1 && std::string(argv[1]) == "man") return man(argc, argv, compiler);
        if (argc > 1 && std::string(argv[1]) == "fmt")
            return fmt(argc, argv);
        if (argc > 1 && std::string(argv[1]) == "deps")
            return deps(argc, argv, compiler);
        if (argc > 1 && std::string(argv[1]) == "test")
            return tests(argc, argv, compiler);
        if (argc > 2 && (std::string(argv[2]) == "--help" || std::string(argv[2]) == "-h"))
        {
            std::cout << MAGEL_TEXT("用法：magel lsp [--macros]\n");
            return 0;
        }
        if (argc > 2)
            throw std::runtime_error("用法：magel lsp [--macros]");
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        Server server(compiler);
        return server.run();
    } catch (const std::exception &e)
    {
        std::cerr << "magel：" << e.what() << "\n";
        return 1;
    }
}
}  // namespace magel_development
