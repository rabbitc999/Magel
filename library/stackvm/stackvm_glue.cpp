/**
 * @file stackvm_glue.cpp
 * @brief StackVM 的桌面宿主胶水；嵌入式移植可用自己的 FileApi 替换本实现。
 */

#include "stackvm_glue.hpp"

#include <fstream>
#include <algorithm>

namespace stackvm
{
namespace
{

class DesktopFileHandle final : public FileHandle
{
public:
    /**
     * @brief 保存桌面宿主文件流及其打开模式。
     * @param path 目标文件或目录的路径。
     * @param mode 文件访问或工具运行模式。
     */
    DesktopFileHandle(const Memory::String &path, const Memory::String &mode)
    {
        std::ios::openmode flags = std::ios::binary;
        readable_ = mode.find('r') != Memory::String::npos || mode.find('+') != Memory::String::npos;
        writable_ = mode.find('w') != Memory::String::npos || mode.find('a') != Memory::String::npos || mode.find('+') != Memory::String::npos;
        if (readable_) flags |= std::ios::in;
        if (mode.find('w') != Memory::String::npos) flags |= std::ios::out | std::ios::trunc;
        if (mode.find('a') != Memory::String::npos) flags |= std::ios::out | std::ios::app;
        if (mode.find('+') != Memory::String::npos) flags |= std::ios::in | std::ios::out;
        stream_.open(path, flags);
    }

    /**
     * @brief 判断文件句柄是否处于打开状态。
     * @retval 文件句柄处于打开状态时为 true，否则为 false。
     */
    [[nodiscard]] bool isOpen() const override { return stream_.is_open(); }

    /**
     * @brief 从文件读取一行并报告到达末尾或读取错误。
     * @param line 源码行号。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool readLine(Memory::String &line, Memory::String &error) override
    {
        if (std::getline(stream_, line)) return true;
        if (stream_.bad())
        {
            error = "读取文件失败";
            return false;
        }
        stream_.clear(stream_.rdstate() & ~std::ios::failbit);
        line.clear();
        return true;
    }

    /**
     * @brief 读取原始字节，EOF 是成功的短读取。
     * @param count 上限。
     * @param bytes 结果。
     * @param error 错误。
     * @return 是否成功。
     */
    bool readBytes(std::size_t count, Memory::String &bytes, Memory::String &error) override
    {
        bytes.clear();
        if (!readable_)
        {
            error = "文件未以读取模式打开";
            return false;
        }
        char block[4096];
        while (bytes.size() < count)
        {
            const auto size = std::min(count - bytes.size(), sizeof(block));
            stream_.read(block, static_cast<std::streamsize>(size));
            const auto read = stream_.gcount();
            if (read > 0) bytes.append(block, static_cast<std::size_t>(read));
            if (stream_.bad() || (stream_.fail() && !stream_.eof()))
            {
                error = "读取文件失败";
                return false;
            }
            if (stream_.eof())
            {
                stream_.clear(stream_.rdstate() & ~std::ios::failbit);
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
        stream_ << text;
        if (!stream_)
        {
            error = "写入文件失败";
            return false;
        }
        stream_.flush();
        return true;
    }

    /**
     * @brief 刷新尚未提交的输出数据。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool flush(Memory::String &error) override
    {
        stream_.flush();
        if (stream_) return true;
        error = "刷新文件失败";
        return false;
    }

    /**
     * @brief 按指定基准调整文件读写位置。
     * @param offset 相对于起点的字节偏移。
     * @param origin 文件定位基准或语义诊断的来源节点。
     * @param position 目标位置或源码位置。
     * @param error 接收失败原因的错误信息。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool seek(std::int64_t offset, int origin, std::int64_t &position,
              Memory::String &error) override
    {
        std::ios::seekdir direction = std::ios::beg;
        if (origin == 1)
            direction = std::ios::cur;
        else if (origin == 2)
            direction = std::ios::end;
        else if (origin != 0)
        {
            error = "未知的 seek 起点";
            return false;
        }
        stream_.clear();
        if (readable_) stream_.seekg(static_cast<std::streamoff>(offset), direction);
        if (!stream_)
        {
            error = "文件定位失败";
            return false;
        }
        if (writable_) stream_.seekp(static_cast<std::streamoff>(offset), direction);
        if (!stream_)
        {
            error = "文件定位失败";
            return false;
        }
        const std::streampos readPosition = readable_ ? stream_.tellg() : std::streampos(-1);
        const std::streampos writePosition = writable_ ? stream_.tellp() : std::streampos(-1);
        const std::streampos selected = readPosition != std::streampos(-1) ? readPosition : writePosition;
        if (selected == std::streampos(-1))
        {
            error = "读取文件位置失败";
            return false;
        }
        position = static_cast<std::int64_t>(selected);
        return true;
    }

    /**
     * @brief 判断文件读取是否到达末尾。
     * @retval 文件读取到达末尾时为 true，否则为 false。
     */
    [[nodiscard]] bool eof() const override { return stream_.eof(); }
    /**
     * @brief 关闭文件并释放宿主句柄。
     */
    void close() override { stream_.close(); }

private:
    std::fstream stream_;
    bool readable_ = false;
    bool writable_ = false;
};

class DesktopFileApi final : public FileApi
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
        auto result = Memory::makeShared<DesktopFileHandle>(path, mode);
        if (!result->isOpen())
        {
            error = "打不开文件：" + path + "（mode=" + mode + "）";
            return {};
        }
        return result;
    }
};

}  // namespace

/**
 * @brief 取得当前平台默认的文件后端接口。
 * @retval 对应对象的引用。
 */
FileApi &defaultFileApi()
{
    static DesktopFileApi api;
    return api;
}

}  // namespace stackvm
