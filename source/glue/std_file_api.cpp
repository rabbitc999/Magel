#include "std_file_api.hpp"

namespace chali_file
{
/**
 * @brief 读取原始字节，未实现的适配器明确报告不支持。
 * @param count 最多读取的字节数。
 * @param bytes 接收数据，EOF 允许短数据。
 * @param error 失败说明。
 * @return 默认返回 false。
 */
bool FileHandle::readBytes(std::size_t count, Memory::String &bytes, Memory::String &error)
{
    (void)count;
    bytes.clear();
    error = "这个文件后端不支持字节读取";
    return false;
}

/**
 * @brief 同步待写数据，未实现的适配器明确报告不支持。
 * @param error 失败说明。
 * @return 默认返回 false。
 */
bool FileHandle::flush(Memory::String &error)
{
    error = "这个文件后端不支持 flush";
    return false;
}

/**
 * @brief 移动文件游标，未实现的适配器明确报告不支持。
 * @param offset 相对指定基准的字节偏移。
 * @param origin 基准：0 开头、1 当前、2 末尾。
 * @param position 成功时接收新游标。
 * @param error 失败说明。
 * @return 默认返回 false。
 */
bool FileHandle::seek(std::int64_t offset, int origin, std::int64_t &position, Memory::String &error)
{
    (void)offset;
    (void)origin;
    (void)position;
    error = "这个文件后端不支持 seek";
    return false;
}
}  // namespace chali_file
