#include "std_process.hpp"

#include "../chali_glue.hpp"
#include "std_native_record.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace std_process_glue
{

/**
 * @brief 关闭宿主进程管道并取得退出状态。
 * @param pipe 有效的 stdout 管道。
 * @return 宿主进程结束状态。
 */
inline int closeProcessPipe(std::FILE *pipe)
{
#ifdef _WIN32
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

/**
 * @brief 由运行时壳独占的宿主进程管道。
 */
struct ProcessRecord
{
    std::FILE *pipe = nullptr;
};
/**
 * @brief 回收记录并关闭尚未关闭的管道。
 * @param record 待销毁的宿主记录。
 */
inline void destroyProcess(void *record)
{
    auto *process = static_cast<ProcessRecord *>(record);
    if (process->pipe) closeProcessPipe(process->pipe);
    delete process;
}
inline const stackvm::NativeRecordType processType{ "Process", nullptr, destroyProcess };

/**
 * @brief 注册使用通用不透明记录的进程操作。
 * @param registry 胶水注册表。
 * @param error 绑定失败的诊断。
 * @return 全部注册成功时为 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    using chali_glue::Context;
    using Memory::String;
    return native_record_glue::bind(registry.native(), "std::process::__processDrop", 1, native_record_glue::drop, error)
           && registry.bindTypedWithContext<std::int64_t, String>(
               "std::process::__processRun",
               [](Context &context, String command) -> std::int64_t {
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
                   auto *record = new ProcessRecord{ pipe };
                   const auto handle = context.makeNativeRecord(processType, record);
                   if (!handle) destroyProcess(record);
                   return handle;
               },
               error)
           && registry.bindTypedWithContext<String, std::int64_t>(
               "std::process::__processRead",
               [](Context &context, std::int64_t handle) -> String {
                   auto *record = static_cast<ProcessRecord *>(context.nativeRecord(handle, processType));
                   std::FILE *pipe = record ? record->pipe : nullptr;
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
               [](Context &context, std::int64_t handle) -> std::int64_t {
                   auto *record = static_cast<ProcessRecord *>(context.nativeRecord(handle, processType));
                   if (!record || !record->pipe)
                   {
                       context.fail("pclose 用到的进程句柄无效或已经关闭");
                       return 0;
                   }
                   const auto code = closeProcessPipe(record->pipe);
                   record->pipe = nullptr;
                   return code;
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
