#include "std_file.hpp"
#include "std_native_record.hpp"
#include "std_collection.hpp"
#include "std_file_api.hpp"
#include "std_desktop_file.hpp"
#include <limits>

/**
 * @brief File 的宿主记录与操作；文件资源不可复制。
 */
namespace file_native_glue
{
using namespace native_record_glue;
/**
 * @brief 由胶水独占的文件适配器、路径及最近操作结果。
 */
struct FileRecord
{
    std::shared_ptr<chali_file::FileHandle> file;
    Memory::String filePath;
    Memory::String fileError;
    bool fileSucceeded = true;
};
/**
 * @brief 关闭并销毁文件宿主记录。
 * @param record 已从运行时壳移除的文件记录。
 */
inline void destroyFile(void *record)
{
    auto *file = static_cast<FileRecord *>(record);
    if (file->file && file->file->isOpen()) file->file->close();
    delete file;
}
inline const stackvm::NativeRecordType fileType{ "file", nullptr, destroyFile };
/**
 * @brief 取得原生调用参数对应的文件句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @param who 诊断信息中使用的操作名称。
 * @retval 匹配项的指针，未找到时为空指针。
 */
inline FileRecord *nativeFile(State &ctx, const Value &handle, const char *who)
{
    FileRecord *object = record<FileRecord>(ctx, handle, fileType, who);
    if (object == nullptr) return nullptr;
    if (!object->file || !object->file->isOpen())
    {
        ctx.error = Memory::String(who) + " 用到的文件已经关闭";
        return nullptr;
    }
    return object;
}

/**
 * @brief `__std_file_open(路径, 模式) -> file`。模式同 C 的 fopen：`r` / `w` / `a` / `r+`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileOpen(State &ctx, const Memory::Vector<Value> &args)
{
    Memory::String path;
    Memory::String mode;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &path))
    {
        ctx.error = "fopen 读不到路径字符串";
        return Value::fromInt(0);
    }
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &mode))
    {
        ctx.error = "fopen 读不到模式字符串";
        return Value::fromInt(0);
    }
    auto *api = static_cast<chali_file::FileApi *>(ctx.host.hostService(chali_file::serviceName));
    if (!api) api = &chali_file::defaultFileApi();
    if (!api)
    {
        ctx.error = "没有文件系统适配器";
        return Value::fromInt(0);
    }
    Memory::String openError;
    std::shared_ptr<chali_file::FileHandle> stream = api->open(path, mode, openError);
    if (!stream || !stream->isOpen())
    {
        ctx.error = openError.empty() ? "打不开文件：" + path : openError;
        return Value::fromInt(0);
    }
    auto *object = new FileRecord{ std::move(stream), path, {}, true };
    const auto handle = ctx.host.makeNativeRecord(fileType, object);
    if (!handle) destroyFile(object);
    return Value::fromInt(handle);
}

/**
 * @brief Result 包装层使用：打开失败只返回空句柄，不终止 VM。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileTryOpen(State &ctx, const Memory::Vector<Value> &args)
{
    const Value result = fileOpen(ctx, args);
    ctx.error.clear();
    return result;
}

/**
 * @brief 判断文件对象的宿主句柄是否有效。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileValid(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = static_cast<FileRecord *>(ctx.host.nativeRecord(nativeInt(args[0]), fileType));
    return Value::fromInt(object != nullptr && object->file && object->file->isOpen() ? 1 : 0);
}

/**
 * @brief `__std_file_read(file) -> string`：读下一行；EOF 返回空串。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileRead(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = nativeFile(ctx, args[0], "fread");
    if (object == nullptr) return Value::fromInt(0);
    Memory::String line;
    Memory::String error;
    if (!object->file->readLine(line, error))
    {
        ctx.error = error + "：" + object->filePath;
        return Value::fromInt(0);
    }
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, line)));
}

/**
 * @brief 将文本写入文件并返回实际写入字节数。
 *
 * `__std_file_write(file, text) -> bytes`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileWrite(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = nativeFile(ctx, args[0], "fwrite");
    if (object == nullptr) return Value::fromInt(0);
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &text))
    {
        ctx.error = "fwrite 读不到要写入的字符串";
        return Value::fromInt(0);
    }
    Memory::String error;
    if (!object->file->write(text, error))
    {
        ctx.error = error + "：" + object->filePath;
        return Value::fromInt(0);
    }
    return Value::fromInt(static_cast<std::int64_t>(text.size()));
}

/**
 * @brief 读取文件对象并为失败结果设置错误信息。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @retval 对应对象的指针。
 */
inline FileRecord *fileForResult(State &ctx, const Value &handle)
{
    FileRecord *object = static_cast<FileRecord *>(ctx.host.nativeRecord(nativeInt(handle), fileType));
    if (object == nullptr)
        return nullptr;
    object->fileError.clear();
    object->fileSucceeded = false;
    if (!object->file || !object->file->isOpen())
    {
        object->fileError = "文件已经关闭";
        return object;
    }
    return object;
}

/**
 * @brief 尝试从宿主文件读取一行文本。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileTryRead(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = fileForResult(ctx, args[0]);
    if (object == nullptr || !object->file || !object->file->isOpen()) return Value::fromInt(0);
    Memory::String line;
    Memory::String error;
    if (!object->file->readLine(line, error))
    {
        object->fileError = error.empty() ? "读取文件失败" : error;
        return Value::fromInt(0);
    }
    object->fileSucceeded = true;
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, line)));
}

/**
 * @brief 新文本接口的行读取，拒绝内嵌零字节并去掉 CRLF 的 CR。
 * @param ctx 调用上下文。
 * @param args 文件句柄。
 * @return 新行字符串槽；错误由 Result 层读取。
 */
inline Value fileTryReadLine(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = fileForResult(ctx, args[0]);
    if (!object || !object->file || !object->file->isOpen()) return Value::fromInt(0);
    Memory::String line, error;
    if (!object->file->readLine(line, error))
    {
        object->fileError = error.empty() ? Memory::String("读取文件失败") : error;
        return Value::fromInt(0);
    }
    if (line.find('\0') != Memory::String::npos)
    {
        object->fileError = "text contains a zero byte";
        return Value::fromInt(0);
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    object->fileSucceeded = true;
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, line)));
}

/**
 * @brief 尝试向宿主文件写入文本。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileTryWrite(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = fileForResult(ctx, args[0]);
    if (object == nullptr || !object->file || !object->file->isOpen()) return Value::fromInt(-1);
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &text))
    {
        object->fileError = "读不到待写入字符串";
        ctx.error.clear();
        return Value::fromInt(-1);
    }
    Memory::String error;
    if (!object->file->write(text, error))
    {
        object->fileError = error.empty() ? "写入文件失败" : error;
        return Value::fromInt(-1);
    }
    object->fileSucceeded = true;
    return Value::fromInt(static_cast<std::int64_t>(text.size()));
}

/**
 * @brief 通过 FileApi 读取原始字节列表。
 * @param ctx 上下文。
 * @param args 文件句柄及数量。
 * @return 拥有型列表或零句柄；错误由文件 Result 层读取。
 */
inline Value fileTryReadBytes(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *file = fileForResult(ctx, args[0]);
    if (!file || !file->file || !file->file->isOpen()) return Value::fromInt(0);
    const auto count = nativeInt(args[1]);
    if (count < 0 || static_cast<std::uint64_t>(count) > std::numeric_limits<std::size_t>::max())
    {
        file->fileError = "非法字节读取数量";
        return Value::fromInt(0);
    }
    Memory::String bytes, error;
    if (!file->file->readBytes(static_cast<std::size_t>(count), bytes, error))
    {
        file->fileError = error.empty() ? Memory::String("字节读取失败") : error;
        return Value::fromInt(0);
    }
    const auto result = collection_glue::makeList(ctx.host);
    if (!result) return Value::fromInt(0);
    for (unsigned char byte : bytes)
        if (!collection_glue::listPush(ctx.host, result, Value::fromInt(byte)))
        {
            ctx.host.releaseNativeRecord(result);
            return Value::fromInt(0);
        }
    file->fileSucceeded = true;
    return Value::fromInt(result);
}
/**
 * @brief 写入字节列表，保留零字节。
 * @param ctx 上下文。
 * @param args 文件及列表句柄。
 * @return 写入数量或 -1。
 */
inline Value fileTryWriteBytes(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *file = fileForResult(ctx, args[0]);
    if (!file || !file->file || !file->file->isOpen()) return Value::fromInt(-1);
    Memory::Vector<Value> values;
    if (!collection_glue::readList(ctx.host, nativeInt(args[1]), values)) return Value::fromInt(-1);
    Memory::String bytes;
    bytes.reserve(values.size());
    for (const auto &value : values) bytes.push_back(static_cast<char>(value.asInt()));
    Memory::String error;
    if (!file->file->write(bytes, error))
    {
        file->fileError = error.empty() ? Memory::String("字节写入失败") : error;
        return Value::fromInt(-1);
    }
    file->fileSucceeded = true;
    return Value::fromInt(static_cast<std::int64_t>(bytes.size()));
}

/**
 * @brief 尝试刷新宿主文件输出缓冲区。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileTryFlush(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = fileForResult(ctx, args[0]);
    if (object == nullptr || !object->file || !object->file->isOpen()) return Value::fromInt(-1);
    Memory::String error;
    if (!object->file->flush(error))
    {
        object->fileError = error.empty() ? "刷新文件失败" : error;
        return Value::fromInt(-1);
    }
    object->fileSucceeded = true;
    return Value::fromInt(0);
}

/**
 * @brief 尝试调整宿主文件的读写位置。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileTrySeek(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = fileForResult(ctx, args[0]);
    if (object == nullptr || !object->file || !object->file->isOpen()) return Value::fromInt(-1);
    Memory::String error;
    std::int64_t position = -1;
    if (!object->file->seek(nativeInt(args[1]), static_cast<int>(nativeInt(args[2])),
                            position, error))
    {
        object->fileError = error.empty() ? "文件定位失败" : error;
        return Value::fromInt(-1);
    }
    object->fileSucceeded = true;
    return Value::fromInt(position);
}

/**
 * @brief 查询最近一次文件操作是否成功。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileOperationSucceeded(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = static_cast<FileRecord *>(ctx.host.nativeRecord(nativeInt(args[0]), fileType));
    return Value::fromInt(object != nullptr && object->fileSucceeded ? 1 : 0);
}

/**
 * @brief 读取最近一次文件操作的错误信息。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileLastError(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = static_cast<FileRecord *>(ctx.host.nativeRecord(nativeInt(args[0]), fileType));
    const Memory::String error = object != nullptr ? object->fileError : "无效的 file 句柄";
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, error)));
}

/**
 * @brief 尝试关闭宿主文件句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileTryClose(State &ctx, const Memory::Vector<Value> &args)
{
    const std::int64_t handle = nativeInt(args[0]);
    FileRecord *object = static_cast<FileRecord *>(ctx.host.nativeRecord(handle, fileType));
    if (object != nullptr && object->file && object->file->isOpen())
        object->file->close();
    return Value::fromInt(0);
}

/**
 * @brief 判断宿主文件是否已到末尾。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileEof(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = nativeFile(ctx, args[0], "feof");
    if (object == nullptr) return Value::fromInt(0);
    return Value::fromInt(object->file->eof() ? 1 : 0);
}

/**
 * @brief 关闭宿主文件句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value fileClose(State &ctx, const Memory::Vector<Value> &args)
{
    FileRecord *object = nativeFile(ctx, args[0], "fclose");
    if (object == nullptr) return Value::fromInt(0);
    object->file->close();
    return Value::fromInt(0);
}

/**
 * @brief 将文件操作注册为普通宿主函数。
 * @param registry 宿主注册表。
 * @param error 注册失败诊断。
 * @return 全部注册成功时为 true。
 */
bool bind(stackvm::NativeRegistry &registry, Memory::String *error)
{
    bool ok = true;
    ok = native_record_glue::bind(registry, "__std_file_open", 2, fileOpen, error) && ok;
    ok = native_record_glue::bind(registry, "__std_file_try_open", 2, fileTryOpen, error) && ok;
    ok = native_record_glue::bind(registry, "__std_file_valid", 1, fileValid, error) && ok;
    ok = native_record_glue::bind(registry, "__std_file_read", 1, fileRead, error) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileTryReadLine", 1, fileTryReadLine, error, true) && ok;
    ok = native_record_glue::bind(registry, "__fileTryReadLine", 1, fileTryReadLine, error, true) && ok;
    ok = native_record_glue::bind(registry, "__std_file_write", 2, fileWrite, error) && ok;
    ok = native_record_glue::bind(registry, "__std_file_eof", 1, fileEof, error) && ok;
    ok = native_record_glue::bind(registry, "__std_file_close", 1, fileClose, error) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileTryRead", 1, fileTryRead, error, true) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileTryReadBytes", 2, fileTryReadBytes, error, true) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileTryWriteBytes", 2, fileTryWriteBytes, error, true) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileTryWrite", 2, fileTryWrite, error, true) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileTryFlush", 1, fileTryFlush, error, true) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileTrySeek", 3, fileTrySeek, error, true) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileSucceeded", 1, fileOperationSucceeded, error, true) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileLastError", 1, fileLastError, error, true) && ok;
    ok = native_record_glue::bind(registry, "std::io::__fileTryClose", 1, fileTryClose, error, true) && ok;
    ok = native_record_glue::bind(registry, "__std_file_drop", 1, native_record_glue::drop, error) && ok;
    return ok;
}
}  // namespace file_native_glue
