#ifndef PARSE_ENGINE_MAGEL_PLATFORM_HPP
#define PARSE_ENGINE_MAGEL_PLATFORM_HPP

/*
 * Magel 宿主侧的平台小 API：把 Windows / Linux / macOS 的写法差异收在一处。
 *
 * 只覆盖"两边写法不一样、规模又很小"的那几件事：平台标识、路径拼接与绝对化、
 * 文件与目录的增删查改、当前目录 / 临时目录、时间与休眠、跑一条命令行。
 *
 * **纯头文件、纯 C++、直接调用**：不依赖 StackVM，也不经过胶水层的 Registry ——
 * 宿主程序 `#include` 之后直接调：
 *
 *   #include "magel_platform.hpp"
 *   magel_platform::writeText(magel_platform::join(folder, "a.txt"), "hi");
 *
 * 实现全部落在标准库上（`std::filesystem` 管路径与文件，`std::chrono` /
 * `std::this_thread` 管时间与休眠），所以平台分支只剩三处：
 *   1. 平台标识：`_WIN32` / `__APPLE__`；
 *   2. UTF-8 路径：`std::filesystem::u8path` 在 Windows 上按 UTF-8 解码成宽字符，
 *      在 Linux / macOS 上就是原始字节 —— 这正是不必自己写 `MultiByteToWideChar`
 *      的原因；
 *   3. `execute()` 的退出码：POSIX 侧要用 `WEXITSTATUS` 把 wait 状态拆开。
 *
 * 约定：失败一律返回 false / -1（`fileSize()` 失败给 -1），`lastError()` 给出
 * 人话说明；它是 thread_local，只描述**本线程**最近一次失败，成功调用不会清它。
 * 路径参数与返回值都是 UTF-8。
 *
 * GCC 8 需要额外链接 `stdc++fs`；GCC 9+ 与 MSVC 不必。
 */

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace magel_platform
{

/**
 * 运行平台。
 */
enum class Kind
{
    Windows,
    Linux,
    MacOS,
    Other,
};

namespace detail
{

/**
 * 本线程最近一次失败的说明；成功调用不动它。
 */
inline Memory::String &errorText()
{
    static thread_local Memory::String text;
    return text;
}

inline bool fail(const Memory::String &message)
{
    errorText() = message;
    return false;
}

inline bool fail(const Memory::String &what, const Memory::String &path, const std::error_code &code)
{
    Memory::String message = what;
    if (!path.empty()) message += "：" + path;
    if (code) message += "（" + code.message() + "）";
    return fail(message);
}

/**
 * UTF-8 文本 → 平台路径；Windows 上按 UTF-8 解码，其它平台就是原字节。
 */
inline std::filesystem::path fromUtf8(const Memory::String &text)
{
#if defined(__cpp_char8_t)
    /* C++20 起 u8 字符串是 char8_t，`u8path` 也弃用了，直接构造 path。 */
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
#else
    return Memory::u8path(text);
#endif
}

/**
 * 路径 → UTF-8 文本。
 */
inline Memory::String toUtf8(const std::filesystem::path &path)
{
#if defined(__cpp_char8_t)
    const std::u8string text = path.u8string();
    return Memory::String(text.begin(), text.end());
#else
    return path.u8string();
#endif
}

}  // namespace detail

/**
 * 当前平台。
 */
inline Kind kind()
{
#if defined(_WIN32)
    return Kind::Windows;
#elif defined(__APPLE__)
    return Kind::MacOS;
#elif defined(__linux__)
    return Kind::Linux;
#else
    return Kind::Other;
#endif
}

/**
 * 平台名：`windows` / `linux` / `macos` / `unknown`。
 */
inline const char *name()
{
    switch (kind())
    {
    case Kind::Windows: return "windows";
    case Kind::Linux: return "linux";
    case Kind::MacOS: return "macos";
    default: return "unknown";
    }
}

/**
 * 本线程最近一次失败的说明；没失败过就是空串。
 */
inline Memory::String lastError()
{
    return detail::errorText();
}

/**
 * 用平台分隔符拼两段路径；左段为空时直接给右段。
 */
inline Memory::String join(const Memory::String &left, const Memory::String &right)
{
    if (left.empty()) return right;
    if (right.empty()) return left;
    return detail::toUtf8(detail::fromUtf8(left) / detail::fromUtf8(right));
}

/**
 * 绝对化并规范化（`..`、冗余分隔符）；**不要求路径存在**。
 */
inline Memory::String absolute(const Memory::String &path)
{
    std::error_code code;
    std::filesystem::path result = std::filesystem::absolute(detail::fromUtf8(path), code);
    if (code)
    {
        detail::fail("取绝对路径失败", path, code);
        return {};
    }
    return detail::toUtf8(result.lexically_normal());
}

/**
 * 当前工作目录。
 */
inline Memory::String currentDirectory()
{
    std::error_code code;
    const std::filesystem::path result = std::filesystem::current_path(code);
    if (code)
    {
        detail::fail("取当前目录失败", "", code);
        return {};
    }
    return detail::toUtf8(result);
}

/**
 * 系统临时目录。
 */
inline Memory::String tempDirectory()
{
    std::error_code code;
    const std::filesystem::path result = std::filesystem::temp_directory_path(code);
    if (code)
    {
        detail::fail("取临时目录失败", "", code);
        return {};
    }
    return detail::toUtf8(result);
}

/**
 * 路径存在（文件或目录都算）。
 */
inline bool exists(const Memory::String &path)
{
    std::error_code code;
    const bool found = std::filesystem::exists(detail::fromUtf8(path), code);
    if (code) return detail::fail("查询路径失败", path, code);
    return found;
}

/**
 * 路径是目录。
 */
inline bool isDirectory(const Memory::String &path)
{
    std::error_code code;
    const bool found = std::filesystem::is_directory(detail::fromUtf8(path), code);
    if (code) return detail::fail("查询目录失败", path, code);
    return found;
}

/**
 * 文件字节数；不是普通文件或读不到就给 -1。
 */
inline std::int64_t fileSize(const Memory::String &path)
{
    std::error_code code;
    const std::filesystem::path target = detail::fromUtf8(path);
    if (!std::filesystem::is_regular_file(target, code) || code)
    {
        detail::fail("不是普通文件", path, code);
        return -1;
    }
    const std::uintmax_t size = std::filesystem::file_size(target, code);
    if (code)
    {
        detail::fail("读文件大小失败", path, code);
        return -1;
    }
    return static_cast<std::int64_t>(size);
}

/**
 * 递归创建目录；目录已存在也算成功。
 */
inline bool makeDirectory(const Memory::String &path)
{
    std::error_code code;
    const std::filesystem::path target = detail::fromUtf8(path);
    std::filesystem::create_directories(target, code);
    std::error_code check;
    if (code && !std::filesystem::is_directory(target, check))
        return detail::fail("建目录失败", path, code);
    return true;
}

/**
 * 删除文件或**空**目录；不存在给 false。
 */
inline bool remove(const Memory::String &path)
{
    std::error_code code;
    const bool removed = std::filesystem::remove(detail::fromUtf8(path), code);
    if (code) return detail::fail("删除失败", path, code);
    if (!removed) return detail::fail("要删的路径不存在", path, code);
    return true;
}

/**
 * 递归删除目录（连同里面的东西）；不存在也算成功。
 */
inline bool removeTree(const Memory::String &path)
{
    std::error_code code;
    std::filesystem::remove_all(detail::fromUtf8(path), code);
    if (code) return detail::fail("递归删除失败", path, code);
    return true;
}

/**
 * 按二进制读整个文件；读不到给 false 并保持 `out` 原样。
 */
inline bool readText(const Memory::String &path, Memory::String &out)
{
    const std::filesystem::path target = detail::fromUtf8(path);
    std::ifstream stream(target, std::ios::binary);
    if (!stream)
        return detail::fail("打不开文件", path, std::error_code());
    Memory::OutputStringStream buffer;
    buffer << stream.rdbuf();
    if (stream.bad())
        return detail::fail("读文件失败", path, std::error_code());
    out = buffer.str();
    return true;
}

/**
 * 按二进制写整个文件（覆盖）；所在目录必须已经存在。
 */
inline bool writeText(const Memory::String &path, const Memory::String &text)
{
    const std::filesystem::path target = detail::fromUtf8(path);
    std::ofstream stream(target, std::ios::binary | std::ios::trunc);
    if (!stream)
        return detail::fail("写不了文件", path, std::error_code());
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
        return detail::fail("写文件失败", path, std::error_code());
    return true;
}

/**
 * 追加写（不存在就新建）；所在目录必须已经存在。
 */
inline bool appendText(const Memory::String &path, const Memory::String &text)
{
    const std::filesystem::path target = detail::fromUtf8(path);
    std::ofstream stream(target, std::ios::binary | std::ios::app);
    if (!stream)
        return detail::fail("追加不了文件", path, std::error_code());
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
        return detail::fail("追加写失败", path, std::error_code());
    return true;
}

/**
 * UTC 毫秒时间戳（1970 起算）。
 */
inline std::int64_t timeMillis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/**
 * 单调毫秒计数，只适合量时间间隔。
 */
inline std::int64_t monotonicMillis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/**
 * 睡够指定毫秒；负数当 0。
 */
inline void sleepMillis(std::int64_t milliseconds)
{
    if (milliseconds <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

/**
 * 跑一条命令行（走 shell），返回子进程退出码；起不来给 -1。
 */
inline std::int64_t execute(const Memory::String &command)
{
    if (command.empty()) return detail::fail("命令为空"), -1;
    const int status = std::system(command.c_str());
    if (status == -1) return detail::fail("起不了子进程：" + command), -1;
#if defined(_WIN32)
    return static_cast<std::int64_t>(status);
#else
    /* POSIX 的返回值是 wait 状态：正常退出才拆得开，被信号打断就原样给出去。 */
    if (WIFEXITED(status)) return static_cast<std::int64_t>(WEXITSTATUS(status));
    return static_cast<std::int64_t>(status);
#endif
}

/**
 * 当前用户的主目录（`HOME` / `USERPROFILE`）；取不到给空串。
 */
inline Memory::String homeDirectory()
{
    const auto read = [](const char *key) {
        const char *value = std::getenv(key);
        return (value == nullptr) ? Memory::String() : Memory::String(value);
    };
#if defined(_WIN32)
    const Memory::String profile = read("USERPROFILE");
    if (!profile.empty()) return profile;
    const Memory::String drive = read("HOMEDRIVE");
    const Memory::String tail = read("HOMEPATH");
    if (!drive.empty() && !tail.empty()) return drive + tail;
#endif
    return read("HOME");
}

}  // namespace magel_platform

#endif  // PARSE_ENGINE_MAGEL_PLATFORM_HPP
