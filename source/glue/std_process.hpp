#ifndef PARSE_ENGINE_STD_PROCESS_HPP
#define PARSE_ENGINE_STD_PROCESS_HPP

#include "../magel_glue.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace std_process_glue
{

/* `std::process` 的宿主实现从 VM 内部原生表（`kSlotNativeEntries` 的 `__vm_proc_*`）
 * 搬到 Registry 胶水：宿主管道改用 `HandlePool` 的整数 ID，VM 侧只看到 `proc`(int64)。
 * `processRun` 打开命令的 stdout 管道，`processRead` 读一行并剥掉 CR/LF，
 * `processClose` 用 `pclose` 取退出码，`system` 是一次性执行。句柄池随注册表存活，
 * 析构时兜底关闭残留管道；`pclose` 过的句柄立即从池里删除，重复关闭会报错。 */
inline int closeProcessPipe(std::FILE *pipe)
{
#ifdef _WIN32
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

struct ProcessTag;

struct ProcessPool
{
    magel_glue::HandlePool<ProcessTag, std::FILE *> pipes;

    std::FILE *find(std::int64_t handle)
    {
        std::FILE **slot = pipes.get(static_cast<std::uint64_t>(handle));
        return slot == nullptr ? nullptr : *slot;
    }

    /**
     * 关闭句柄并返回退出码；句柄无效 / 已关闭时返回 false（`code` 不动）。
     */
    bool close(std::int64_t handle, int *code)
    {
        const std::uint64_t id = static_cast<std::uint64_t>(handle);
        const auto found = pipes.objects.find(id);
        if (found == pipes.objects.end() || found->second == nullptr) return false;
        if (code != nullptr) *code = closeProcessPipe(found->second);
        pipes.objects.erase(found);
        return true;
    }

    ~ProcessPool()
    {
        for (auto &entry : pipes.objects)
            if (entry.second != nullptr)
            {
                closeProcessPipe(entry.second);
                entry.second = nullptr;
            }
    }
};

inline bool bind(magel_glue::Registry &registry, Memory::String *error = nullptr)
{
    using magel_glue::Context;
    using Memory::String;
    const auto pool = Memory::makeShared<ProcessPool>();
    return registry.bindTypedWithContext<std::int64_t, String>(
               "std::process::__processRun",
               [pool](Context &context, String command) -> std::int64_t {
#ifdef _WIN32
                   std::FILE *pipe = _popen(command.c_str(), "r");
#else
                   std::FILE *pipe = popen(command.c_str(), "r");
#endif
                   if (pipe == nullptr)
                   {
                       context.fail("启动命令失败：" + command);
                       return 0;
                   }
                   return static_cast<std::int64_t>(pool->pipes.add(pipe));
               },
               error)
           && registry.bindTypedWithContext<String, std::int64_t>(
               "std::process::__processRead",
               [pool](Context &context, std::int64_t handle) -> String {
                   std::FILE *pipe = pool->find(handle);
                   if (pipe == nullptr)
                   {
                       context.fail("pread 用到的进程句柄无效或已经关闭");
                       return String();
                   }
                   String line;
                   char buffer[512];
                   if (std::fgets(buffer, sizeof(buffer), pipe) != nullptr)
                   {
                       line = buffer;
                       while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                           line.pop_back();
                   }
                   return line;
               },
               error)
           && registry.bindTypedWithContext<std::int64_t, std::int64_t>(
               "std::process::__processClose",
               [pool](Context &context, std::int64_t handle) -> std::int64_t {
                   int code = 0;
                   if (!pool->close(handle, &code))
                   {
                       context.fail("pclose 用到的进程句柄无效或已经关闭");
                       return 0;
                   }
                   return static_cast<std::int64_t>(code);
               },
               error)
           && registry.bindTypedWithContext<std::int64_t, String>(
               "std::process::__system",
               [](Context &, String command) -> std::int64_t {
                   return static_cast<std::int64_t>(std::system(command.c_str()));
               },
               error);
}

}  // namespace std_process_glue

#endif
