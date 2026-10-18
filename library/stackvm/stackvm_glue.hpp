#include "../../source/allocator/magel_allocator.hpp"
#ifndef PARSE_ENGINE_STACKVM_GLUE_HPP
#define PARSE_ENGINE_STACKVM_GLUE_HPP

#include <memory>
#include <cstdint>
#include <string>

namespace stackvm
{

/**
 * 宿主文件句柄接口。VM 只依赖这组操作，不依赖 std::fstream、POSIX 或 RTOS。
 */
class FileHandle
{
public:
    virtual ~FileHandle() = default;
    [[nodiscard]] virtual bool isOpen() const = 0;
    virtual bool readLine(Memory::String &line, Memory::String &error) = 0;
    virtual bool write(const Memory::String &text, Memory::String &error) = 0;
    /**
     * @brief 读取至多 count 个原始字节；EOF 返回短数据，旧后端默认明确报告不支持。
     * @param count 字节上限。
     * @param bytes 输出数据。
     * @param error 失败说明。
     * @return 是否成功。
     */
    virtual bool readBytes(std::size_t count, Memory::String &bytes, Memory::String &error)
    {
        (void)count;
        bytes.clear();
        error = "这个文件后端不支持字节读取";
        return false;
    }
    virtual bool flush(Memory::String &error)
    {
        error = "这个文件后端不支持 flush";
        return false;
    }
    virtual bool seek(std::int64_t, int, std::int64_t &, Memory::String &error)
    {
        error = "这个文件后端不支持 seek";
        return false;
    }
    [[nodiscard]] virtual bool eof() const = 0;
    virtual void close() = 0;
};

/**
 * 可注入的宿主文件 API。嵌入式端可在单独翻译单元中对接 Flash、SD 卡或 RTOS VFS。
 */
class FileApi
{
public:
    virtual ~FileApi() = default;
    virtual std::shared_ptr<FileHandle> open(const Memory::String &path,
                                             const Memory::String &mode,
                                             Memory::String &error) = 0;
};

/**
 * 桌面端默认胶水。嵌入式程序应通过 Machine::setFileApi() 注入自己的实现。
 */
FileApi &defaultFileApi();

}  // namespace stackvm

#endif  // PARSE_ENGINE_STACKVM_GLUE_HPP
