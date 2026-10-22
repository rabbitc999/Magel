#ifndef CHALI_GLUE_FILE_API_HPP
#define CHALI_GLUE_FILE_API_HPP
#include "../../source/allocator/chali_allocator.hpp"
#include <cstdint>
#include <memory>

namespace chali_file
{
/**
 * @brief 文件胶水私有的宿主句柄协议，VM 不包含或调用此接口。
 */
class FileHandle
{
public:
    /**
     * @brief 销毁宿主文件句柄的接口部分。
     */
    virtual ~FileHandle() = default;
    /**
     * @brief 查询宿主文件是否仍然打开。
     * @return 打开时为 true。
     */
    [[nodiscard]] virtual bool isOpen() const = 0;
    /**
     * @brief 从当前游标读取一行文本，EOF 返回空串且成功。
     * @param line 接收不含行尾分隔符的文本。
     * @param error 失败说明。
     * @return 读取成功或到达 EOF 时为 true。
     */
    virtual bool readLine(Memory::String &line, Memory::String &error) = 0;
    /**
     * @brief 将字节文本写入文件当前游标。
     * @param text 完整待写数据。
     * @param error 失败说明。
     * @return 全部写入成功时为 true。
     */
    virtual bool write(const Memory::String &text, Memory::String &error) = 0;
    /**
     * @brief 读取原始字节，未实现的适配器明确报告不支持。
     * @param count 最多读取的字节数。
     * @param bytes 接收数据，EOF 允许短数据。
     * @param error 失败说明。
     * @return 默认返回 false。
     */
    virtual bool readBytes(std::size_t count, Memory::String &bytes, Memory::String &error);
    /**
     * @brief 同步待写数据，未实现的适配器明确报告不支持。
     * @param error 失败说明。
     * @return 默认返回 false。
     */
    virtual bool flush(Memory::String &error);
    /**
     * @brief 移动文件游标，未实现的适配器明确报告不支持。
     * @param offset 相对指定基准的字节偏移。
     * @param origin 基准：0 开头、1 当前、2 末尾。
     * @param position 成功时接收新游标。
     * @param error 失败说明。
     * @return 默认返回 false。
     */
    virtual bool seek(std::int64_t offset, int origin, std::int64_t &position, Memory::String &error);
    /**
     * @brief 查询最近读取是否到达文件末尾。
     * @return 已到达末尾时为 true。
     */
    [[nodiscard]] virtual bool eof() const = 0;
    /**
     * @brief 关闭宿主文件，重复关闭应保持安全。
     */
    virtual void close() = 0;
};
/**
 * @brief 文件胶水自己的适配器，操作系统宿主提供具体实现。
 */
class FileApi
{
public:
    /**
     * @brief 销毁文件系统适配器接口。
     */
    virtual ~FileApi() = default;
    /**
     * @brief 按宿主路径和模式打开文件。
     * @param path 目标路径。
     * @param mode fopen 风格模式。
     * @param error 打开失败的说明。
     * @return 新句柄；失败返回空指针。
     */
    virtual std::shared_ptr<FileHandle> open(const Memory::String &path, const Memory::String &mode, Memory::String &error) = 0;
};
/**
 * @brief 取得当前平台提供的文件胶水默认适配器。
 * @return 比当前运行时活得更久的适配器引用。
 */
FileApi &defaultFileApi();
inline constexpr const char *serviceName = "chali.file.api";
}  // namespace chali_file
#endif
