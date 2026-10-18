#include <algorithm>
/**
 * @file mcu_file_api.cpp
 * @brief 板端文件系统：桩 / C stdio / FatFS 三个后端 + 板端注入点。
 *
 * 桌面构建链的是 `library/stackvm/stackvm_glue.cpp`（`std::fstream` 版）；板端不编那份，
 * 由本文件提供 `stackvm::defaultFileApi()`：符号在、语义可控、**可以整个关掉**。
 *
 * 语义照抄桌面参考实现（`library/stackvm/stackvm_glue.cpp`）：
 *   * `readLine`：读到 `\n` 为止；**EOF 拿不到字节时返回空串且成功**（EOF 不是错误），
 *     真读失败才报错；
 *   * `seek`：0=开头 / 1=当前 / 2=末尾，返回新位置；`write` 写完顺带同步。
 * 两个 MCU 约束按定容做：打开文件数 `MCU_VM_MAX_OPEN_FILES`（静态槽），
 * 按行读逐字节（不设行长上限、不额外要缓冲）。
 */

#include "mcu_filesystem.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#ifndef MCU_VM_FILE_BACKEND
#define MCU_VM_FILE_BACKEND 0
#endif
#ifndef MCU_VM_MAX_OPEN_FILES
#define MCU_VM_MAX_OPEN_FILES 4
#endif

#if MCU_VM_FILE_BACKEND == 2
/* FatFS：板端把 SDK 的 `ff.h` 放进 include 路径即可（不必放进仓库）。 */
extern "C" {
#include <ff.h>
}
#endif

namespace stackvm
{
namespace
{
const char kNoFilesystem[] = "这个构建没有文件系统（MCU_VM_FILE_BACKEND=0）";

/**
 * @brief 打开模式（与桌面 `DesktopFileHandle` 的 'r'/'w'/'a'/'+' 解析一致）。
 */
struct FileMode
{
    bool readable = false;
    bool writable = false;
    bool truncate = false;
    bool append = false;

    /**
     * @brief 解析文件打开模式并保存访问标志。
     * @param mode 文件访问或工具运行模式。
     * @retval 保存解析文件打开模式并保存访问标志结果的对象。
     */
    static FileMode Parse(const Memory::String &mode)
    {
        FileMode parsed;
        parsed.readable = mode.find('r') != Memory::String::npos || mode.find('+') != Memory::String::npos;
        parsed.writable = mode.find('w') != Memory::String::npos || mode.find('a') != Memory::String::npos || mode.find('+') != Memory::String::npos;
        parsed.truncate = mode.find('w') != Memory::String::npos;
        parsed.append = mode.find('a') != Memory::String::npos;
        return parsed;
    }
};

/* ---------------- 后端 0：关闭（缺省） ---------------- */

class UnavailableFileHandle final : public FileHandle
{
public:
    /**
     * @brief 判断文件句柄是否处于打开状态。
     * @retval 文件句柄处于打开状态时为 true，否则为 false。
     */
    [[nodiscard]] bool isOpen() const override { return false; }
    /**
     * @brief 从文件读取一行并报告到达末尾或读取错误。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool readLine(Memory::String &, Memory::String &error) override
    {
        error = kNoFilesystem;
        return false;
    }
    /**
     * @brief 将给定文本写入文件。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool write(const Memory::String &, Memory::String &error) override
    {
        error = kNoFilesystem;
        return false;
    }
    /**
     * @brief 刷新尚未提交的输出数据。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool flush(Memory::String &error) override
    {
        error = kNoFilesystem;
        return false;
    }
    /**
     * @brief 按指定基准调整文件读写位置。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool seek(std::int64_t, int, std::int64_t &, Memory::String &error) override
    {
        error = kNoFilesystem;
        return false;
    }
    /**
     * @brief 判断文件读取是否到达末尾。
     * @retval 文件读取到达末尾时为 true，否则为 false。
     */
    [[nodiscard]] bool eof() const override { return true; }
    /**
     * @brief 关闭文件并释放宿主句柄。
     */
    void close() override {}
};

class UnavailableFileApi final : public FileApi
{
public:
    /**
     * @brief 根据路径和模式打开宿主文件。
     * @param path 目标文件或目录的路径。
     * @param error 接收失败原因的错误信息。
     * @retval 保存根据路径和模式打开宿主文件结果的对象。
     */
    std::shared_ptr<FileHandle> open(const Memory::String &path, const Memory::String &,
                                     Memory::String &error) override
    {
        error = kNoFilesystem + (": " + path);
        return nullptr;
    }
};

/* ---------------- 后端 1：C stdio（桌面回归） ---------------- */

#if MCU_VM_FILE_BACKEND == 1
/**
 * @brief 将文件访问模式转换为标准 C 文件模式。
 * @param mode 文件访问或工具运行模式。
 * @retval 对应对象的指针。
 */
const char *StdioModeText(const FileMode &mode)
{
    if (mode.append) return "ab";
    if (mode.truncate) return mode.readable ? "w+b" : "wb";
    if (mode.readable && mode.writable) return "r+b";
    return "rb";
}

/**
 * @brief stdio 打开槽（定容静态数组；与 FatFS 后端同一套写法）。
 */
std::FILE *g_stdioSlots[MCU_VM_MAX_OPEN_FILES] = {};

class StdioFileHandle final : public FileHandle
{
public:
    /**
     * @brief 关闭标准 C 文件后端仍持有的句柄。
     */
    ~StdioFileHandle() override { close(); }

    /**
     * @brief 根据路径和模式打开宿主文件。
     * @param path 目标文件或目录的路径。
     * @param mode 文件访问或工具运行模式。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool open(const Memory::String &path, const FileMode &mode, Memory::String &error)
    {
        for (int index = 0; index < MCU_VM_MAX_OPEN_FILES; ++index)
        {
            if (g_stdioSlots[index] != nullptr) continue;
            std::FILE *stream = std::fopen(path.c_str(), StdioModeText(mode));
            if (stream == nullptr)
            {
                error = "打不开文件：" + path;
                return false;
            }
            g_stdioSlots[index] = stream;
            slot_ = index;
            return true;
        }
        error = "打开文件数已达上限（MCU_VM_MAX_OPEN_FILES="
                + Memory::toString(MCU_VM_MAX_OPEN_FILES) + "）";
        return false;
    }

    /**
     * @brief 判断文件句柄是否处于打开状态。
     * @retval 文件句柄处于打开状态时为 true，否则为 false。
     */
    [[nodiscard]] bool isOpen() const override { return File() != nullptr; }

    /**
     * @brief 从文件读取一行并报告到达末尾或读取错误。
     * @param line 源码行号。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool readLine(Memory::String &line, Memory::String &error) override
    {
        line.clear();
        std::FILE *stream = File();
        if (stream == nullptr)
        {
            error = "文件已经关闭";
            return false;
        }
        for (;;)
        {
            const int ch = std::fgetc(stream);
            if (ch == EOF)
            {
                if (std::ferror(stream) != 0)
                {
                    error = "读取文件失败";
                    return false;
                }
                return true; /* EOF：与桌面 std::getline 一样，返回空串且成功 */
            }
            if (ch == '\n') return true;
            line.push_back(static_cast<char>(ch));
        }
    }

    /**
     * @brief 按字节读取 stdio 文件，不将零字节或换行当结束。
     * @param count 上限。
     * @param bytes 输出。
     * @param error 错误。
     * @return 是否成功。
     */
    bool readBytes(std::size_t count, Memory::String &bytes, Memory::String &error) override
    {
        bytes.clear();
        auto *stream = File();
        if (!stream)
        {
            error = "文件已经关闭";
            return false;
        }
        char block[512];
        while (bytes.size() < count)
        {
            const auto size = std::min(count - bytes.size(), sizeof(block));
            const auto read = std::fread(block, 1, size, stream);
            bytes.append(block, read);
            if (read < size)
            {
                if (std::ferror(stream))
                {
                    error = "读取文件失败";
                    return false;
                }
                break;
            }
        }
        return true;
    }

    /**
     * @brief 将给定文本写入文件。
     * @param text 待处理的文本。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool write(const Memory::String &text, Memory::String &error) override
    {
        std::FILE *stream = File();
        if (stream == nullptr)
        {
            error = "文件已经关闭";
            return false;
        }
        if (!text.empty() && std::fwrite(text.data(), 1u, text.size(), stream) != text.size())
        {
            error = "写入文件失败";
            return false;
        }
        if (std::fflush(stream) != 0)
        {
            error = "刷新文件失败";
            return false;
        }
        return true;
    }

    /**
     * @brief 刷新尚未提交的输出数据。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool flush(Memory::String &error) override
    {
        std::FILE *stream = File();
        if (stream == nullptr)
        {
            error = "文件已经关闭";
            return false;
        }
        if (std::fflush(stream) != 0)
        {
            error = "刷新文件失败";
            return false;
        }
        return true;
    }

    /**
     * @brief 按指定基准调整文件读写位置。
     * @param offset 相对于起点的字节偏移。
     * @param origin 文件定位基准或语义诊断的来源节点。
     * @param position 目标位置或源码位置。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool seek(std::int64_t offset, int origin, std::int64_t &position, Memory::String &error) override
    {
        std::FILE *stream = File();
        if (stream == nullptr)
        {
            error = "文件已经关闭";
            return false;
        }
        int whence = SEEK_SET;
        if (origin == 1)
            whence = SEEK_CUR;
        else if (origin == 2)
            whence = SEEK_END;
        else if (origin != 0)
        {
            error = "未知的 seek 起点";
            return false;
        }
        if (std::fseek(stream, static_cast<long>(offset), whence) != 0)
        {
            error = "文件定位失败";
            return false;
        }
        const long at = std::ftell(stream);
        if (at < 0)
        {
            error = "读取文件位置失败";
            return false;
        }
        position = at;
        return true;
    }

    /**
     * @brief 判断文件读取是否到达末尾。
     * @retval 文件读取到达末尾时为 true，否则为 false。
     */
    [[nodiscard]] bool eof() const override
    {
        const std::FILE *stream = File();
        return stream == nullptr || std::feof(const_cast<std::FILE *>(stream)) != 0;
    }

    /**
     * @brief 关闭文件并释放宿主句柄。
     */
    void close() override
    {
        if (slot_ < 0) return;
        std::fclose(g_stdioSlots[slot_]);
        g_stdioSlots[slot_] = nullptr;
        slot_ = -1;
    }

private:
    /**
     * @brief 构造关联文件接口与后端句柄的文件对象。
     * @retval 对应对象的指针。
     */
    std::FILE *File() const { return slot_ >= 0 ? g_stdioSlots[slot_] : nullptr; }
    int slot_ = -1;
};

class StdioFileApi final : public FileApi
{
public:
    /**
     * @brief 根据路径和模式打开宿主文件。
     * @param path 目标文件或目录的路径。
     * @param mode 文件访问或工具运行模式。
     * @param error 接收失败原因的错误信息。
     * @retval 保存根据路径和模式打开宿主文件结果的对象。
     */
    std::shared_ptr<FileHandle> open(const Memory::String &path, const Memory::String &mode,
                                     Memory::String &error) override
    {
        auto handle = std::make_shared<StdioFileHandle>();
        if (!handle->open(path, FileMode::Parse(mode), error)) return nullptr;
        return handle;
    }
};
#endif  // MCU_VM_FILE_BACKEND == 1

/* ---------------- 后端 2：FatFS（板端） ---------------- */

#if MCU_VM_FILE_BACKEND == 2
/**
 * @brief FatFS 打开槽（定容静态数组，不吃堆）。
 */
struct FatFsSlot
{
    FIL file = {};
    bool used = false;
};
FatFsSlot g_fatFsSlots[MCU_VM_MAX_OPEN_FILES];

/**
 * @brief 将文件访问模式转换为 FatFs 标志。
 * @param mode 文件访问或工具运行模式。
 * @return FatFS 的 BYTE 访问与创建标志。
 */
BYTE FatFsModeFlags(const FileMode &mode)
{
    BYTE flags = (mode.readable ? FA_READ : 0) | (mode.writable ? FA_WRITE : 0);
    if (mode.truncate)
        flags |= FA_CREATE_ALWAYS;
    else if (mode.append)
        flags |= FA_OPEN_APPEND;
    return flags;
}

class FatFsFileHandle final : public FileHandle
{
public:
    /**
     * @brief 关闭 FatFs 后端仍持有的文件句柄。
     */
    ~FatFsFileHandle() override { close(); }

    /**
     * @brief 根据路径和模式打开宿主文件。
     * @param path 目标文件或目录的路径。
     * @param mode 文件访问或工具运行模式。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool open(const Memory::String &path, const FileMode &mode, Memory::String &error)
    {
        for (int index = 0; index < MCU_VM_MAX_OPEN_FILES; ++index)
        {
            if (g_fatFsSlots[index].used) continue;
            const FRESULT result = f_open(&g_fatFsSlots[index].file, path.c_str(),
                                          FatFsModeFlags(mode));
            if (result != FR_OK)
            {
                error = "FatFS 打不开文件：" + path + "（FRESULT=" + Memory::toString(static_cast<int>(result)) + "）";
                return false;
            }
            g_fatFsSlots[index].used = true;
            slot_ = index;
            append_ = mode.append;
            return true;
        }
        error = "FatFS 打开文件数已达上限（MCU_VM_MAX_OPEN_FILES="
                + Memory::toString(MCU_VM_MAX_OPEN_FILES) + "）";
        return false;
    }

    /**
     * @brief 判断文件句柄是否处于打开状态。
     * @retval 文件句柄处于打开状态时为 true，否则为 false。
     */
    [[nodiscard]] bool isOpen() const override { return slot_ >= 0; }

    /**
     * @brief 从文件读取一行并报告到达末尾或读取错误。
     * @param line 源码行号。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool readLine(Memory::String &line, Memory::String &error) override
    {
        FIL *handle = File();
        if (handle == nullptr)
        {
            error = "文件已经关闭";
            return false;
        }
        line.clear();
        for (;;)
        {
            char byte = 0;
            UINT read = 0u;
            if (f_read(handle, &byte, 1u, &read) != FR_OK)
            {
                error = "FatFS 读取失败";
                return false;
            }
            if (read == 0u) return true; /* EOF：返回空串且成功 */
            if (byte == '\n') return true;
            line.push_back(byte);
        }
    }

    /**
     * @brief 使用 FatFS 分块读取二进制，避免 UINT 计数截断。
     * @param count 上限。
     * @param bytes 输出。
     * @param error 错误。
     * @return 是否成功。
     */
    bool readBytes(std::size_t count, Memory::String &bytes, Memory::String &error) override
    {
        bytes.clear();
        auto *handle = File();
        if (!handle)
        {
            error = "文件已经关闭";
            return false;
        }
        char block[512];
        while (bytes.size() < count)
        {
            const auto size = std::min(count - bytes.size(), sizeof(block));
            UINT read = 0;
            if (f_read(handle, block, static_cast<UINT>(size), &read) != FR_OK)
            {
                error = "FatFS 读取失败";
                return false;
            }
            bytes.append(block, read);
            if (read < size) break;
        }
        return true;
    }

    /**
     * @brief 将给定文本写入文件。
     * @param text 待处理的文本。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool write(const Memory::String &text, Memory::String &error) override
    {
        FIL *handle = File();
        if (handle == nullptr)
        {
            error = "文件已经关闭";
            return false;
        }
        if (!text.empty())
        {
            if (append_ && f_lseek(handle, f_size(handle)) != FR_OK)
            {
                error = "FatFS 追加定位失败";
                return false;
            }
            UINT written = 0u;
            if (f_write(handle, text.data(), static_cast<UINT>(text.size()), &written) != FR_OK || written != text.size())
            {
                error = "FatFS 写入失败";
                return false;
            }
        }
        if (f_sync(handle) != FR_OK)
        {
            error = "FatFS 同步失败";
            return false;
        }
        return true;
    }

    /**
     * @brief 刷新尚未提交的输出数据。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool flush(Memory::String &error) override
    {
        FIL *handle = File();
        if (handle == nullptr)
        {
            error = "文件已经关闭";
            return false;
        }
        if (f_sync(handle) != FR_OK)
        {
            error = "FatFS 同步失败";
            return false;
        }
        return true;
    }

    /**
     * @brief 按指定基准调整文件读写位置。
     * @param offset 相对于起点的字节偏移。
     * @param origin 文件定位基准或语义诊断的来源节点。
     * @param position 目标位置或源码位置。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool seek(std::int64_t offset, int origin, std::int64_t &position, Memory::String &error) override
    {
        FIL *handle = File();
        if (handle == nullptr)
        {
            error = "文件已经关闭";
            return false;
        }
        FSIZE_t target = 0u;
        if (origin == 0)
            target = static_cast<FSIZE_t>(offset);
        else if (origin == 1)
            target = f_tell(handle) + static_cast<FSIZE_t>(offset);
        else if (origin == 2)
            target = f_size(handle) + static_cast<FSIZE_t>(offset);
        else
        {
            error = "未知的 seek 起点";
            return false;
        }
        if (f_lseek(handle, target) != FR_OK)
        {
            error = "FatFS 定位失败";
            return false;
        }
        position = static_cast<std::int64_t>(f_tell(handle));
        return true;
    }

    /**
     * @brief 判断文件读取是否到达末尾。
     * @retval 文件读取到达末尾时为 true，否则为 false。
     */
    [[nodiscard]] bool eof() const override
    {
        const FIL *handle = File();
        return handle == nullptr || f_eof(handle) != 0;
    }

    /**
     * @brief 关闭文件并释放宿主句柄。
     */
    void close() override
    {
        if (slot_ < 0) return;
        f_close(&g_fatFsSlots[slot_].file);
        g_fatFsSlots[slot_].used = false;
        slot_ = -1;
    }

private:
    /**
     * @brief 构造关联文件接口与后端句柄的文件对象。
     * @retval 对应对象的指针。
     */
    FIL *File() { return slot_ >= 0 ? &g_fatFsSlots[slot_].file : nullptr; }
    /**
     * @brief 构造关联文件接口与后端句柄的文件对象。
     * @retval 对应对象的指针。
     */
    const FIL *File() const { return slot_ >= 0 ? &g_fatFsSlots[slot_].file : nullptr; }
    int slot_ = -1;
    bool append_ = false;
};

class FatFsFileApi final : public FileApi
{
public:
    /**
     * @brief 根据路径和模式打开宿主文件。
     * @param path 目标文件或目录的路径。
     * @param mode 文件访问或工具运行模式。
     * @param error 接收失败原因的错误信息。
     * @retval 保存根据路径和模式打开宿主文件结果的对象。
     */
    std::shared_ptr<FileHandle> open(const Memory::String &path, const Memory::String &mode,
                                     Memory::String &error) override
    {
        auto handle = std::make_shared<FatFsFileHandle>();
        if (!handle->open(path, FileMode::Parse(mode), error)) return nullptr;
        return handle;
    }
};
#endif  // MCU_VM_FILE_BACKEND == 2

/**
 * @brief 取得当前编译配置选择的文件后端接口。
 * @retval 对应对象的引用。
 */
FileApi &BackendApi()
{
#if MCU_VM_FILE_BACKEND == 1
    static StdioFileApi api;
#elif MCU_VM_FILE_BACKEND == 2
    static FatFsFileApi api;
#else
    static UnavailableFileApi api;
#endif
    return api;
}
}  // namespace

/**
 * @brief 板端注入的实现（`mcuFilesystem::SetApi`）；为空时用编译期后端。
 */
static FileApi *g_installedApi = nullptr;

/**
 * @brief 取得当前平台默认的文件后端接口。
 * @retval 对应对象的引用。
 */
FileApi &defaultFileApi()
{
    return g_installedApi != nullptr ? *g_installedApi : BackendApi();
}

}  // namespace stackvm

namespace mcuFilesystem
{
/**
 * @brief 本次构建配置的后端。
 * @retval 保存本次构建配置的后端结果的对象。
 */
Backend ConfiguredBackend() { return static_cast<Backend>(MCU_VM_FILE_BACKEND); }

/**
 * @brief 后端名字（诊断 / footprint 报告用）。
 * @retval 对应对象的指针。
 */
const char *BackendName()
{
#if MCU_VM_FILE_BACKEND == 1
    return "stdio";
#elif MCU_VM_FILE_BACKEND == 2
    return "FatFS";
#else
    return "disabled";
#endif
}

/**
 * @brief 这个构建到底能不能打开文件（关闭时 `open` 必失败）。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool Available() { return ConfiguredBackend() != Backend::Disabled; }

/**
 * @brief 板端注入自己的文件系统实现（优先于编译期后端）。
 *
 * 必须在 `Machine::run()` 之前调用；`api` 的生命周期要覆盖整个运行期。
 * @param api 实现；传 `nullptr` 退回编译期后端。
 */
void SetApi(stackvm::FileApi *api) { stackvm::g_installedApi = api; }

/**
 * @brief 当前生效的实现（注入的优先，否则编译期后端）。
 * @retval 对应对象的引用。
 */
stackvm::FileApi &CurrentApi() { return stackvm::defaultFileApi(); }
}  // namespace mcuFilesystem
