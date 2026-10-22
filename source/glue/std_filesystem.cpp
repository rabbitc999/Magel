#include "std_filesystem.hpp"
#include "../chali_glue.hpp"
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>

namespace std_filesystem_glue
{
/**
 * @brief 文件系统操作结果的短期结构体视图，槽布局与 std::io::IoReply 一致。
 */
struct ReplyTag
{
};
using Reply = chali_glue::Record<ReplyTag>;
using Bytes = Memory::Vector<std::uint8_t>;
/**
 * @brief 写入可恢复错误。
 * @param reply 当前回调的结果视图。
 * @param message 错误文本。
 */
inline void fail(Reply reply, const std::string &message)
{
    reply.set<Memory::String>(0, Memory::String(message.data(), message.size()));
}
/**
 * @brief 注册桌面文件系统和 UTC 时钟。
 * @param registry 注册表。
 * @param error 绑定失败说明。
 * @return 是否全部成功。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    namespace fs = std::filesystem;
    bool ok = registry.bindTypedWithContext<void, Memory::String, std::int64_t, Reply>("std::io::__readAllBytes", [](chali_glue::Context &ctx, Memory::String path, std::int64_t limit, Reply reply) {
            if (limit<0) { fail(reply,"negative byte limit"); return; }
            std::ifstream input(Memory::u8path(path),std::ios::binary);
            if (!input) { fail(reply,"cannot open file for reading"); return; }
            Bytes bytes; char block[4096];
            while (input) {
                input.read(block,sizeof(block)); const auto count=input.gcount();
                if (count<0 || static_cast<std::uint64_t>(count)>static_cast<std::uint64_t>(limit)-bytes.size()) {
                    fail(reply,"file exceeds byte limit"); return;
                }
                for (std::streamsize i=0;i<count;++i) bytes.push_back(static_cast<std::uint8_t>(block[i]));
            }
            if (input.bad()) { fail(reply,"file read failed"); return; }
            const auto handle=reply.field<std::int64_t>(1);
            for (auto byte:bytes) collection_glue::listPush(ctx, handle,chali_glue::Value::fromInt(byte)); }, error);
    ok = registry.bindTyped<void, Memory::String, Bytes, Reply>("std::io::__writeAllBytes", [](Memory::String path, Bytes bytes, Reply reply) {
            std::ofstream output(Memory::u8path(path),std::ios::binary|std::ios::trunc);
            if (!output) { fail(reply,"cannot open file for writing"); return; }
            if (!bytes.empty()) output.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
            output.close();
            if (!output) { fail(reply,"file write or close failed"); return; }
            reply.set<std::int64_t>(3,static_cast<std::int64_t>(bytes.size())); }, error) && ok;
    ok = registry.bindTypedWithContext<void, Memory::String, Reply>("std::io::__listDirectory", [](chali_glue::Context &ctx, Memory::String path, Reply reply) {
            std::error_code ec; fs::directory_iterator it(Memory::u8path(path),ec),end;
            if (ec) { fail(reply,ec.message()); return; }
            Memory::Vector<Memory::String> paths;
            for (;it!=end;it.increment(ec)) {
                if (ec) { fail(reply,ec.message()); return; }
                const auto name=it->path().generic_u8string(); paths.emplace_back(name.data(),name.size());
            }
            if (ec) { fail(reply,ec.message()); return; }
            std::sort(paths.begin(),paths.end()); const auto handle=reply.field<std::int64_t>(2);
            for (const auto &path:paths) collection_glue::listPush(ctx, handle,chali_glue::Value::fromText(path)); }, error) && ok;
    ok = registry.bindTyped<void, Memory::String, Reply>("std::io::__fileInfo", [](Memory::String path, Reply reply) {
            std::error_code ec; const auto p=Memory::u8path(path); const auto status=fs::status(p,ec);
            if (ec || !fs::exists(status)) { fail(reply,ec?ec.message():"path does not exist"); return; }
            const bool regular=fs::is_regular_file(status);
            const auto size=regular?fs::file_size(p,ec):0;
            if (ec || size>static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max())) { fail(reply,"cannot represent file size"); return; }
            const auto modified=fs::last_write_time(p,ec);
            if (ec) { fail(reply,ec.message()); return; }
            const auto system=std::chrono::time_point_cast<std::chrono::system_clock::duration>(modified-fs::file_time_type::clock::now()+std::chrono::system_clock::now());
            reply.set<std::int64_t>(4,static_cast<std::int64_t>(size));
            reply.set<bool>(5,regular); reply.set<bool>(6,fs::is_directory(status));
            reply.set<std::int64_t>(7,std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count()); }, error) && ok;
    ok = registry.bindTyped<void, Memory::String, Reply>("std::io::__createDirectories", [](Memory::String path, Reply reply) { std::error_code ec; fs::create_directories(Memory::u8path(path),ec); if (ec) fail(reply,ec.message()); }, error) && ok;
    ok = registry.bindTyped<void, Memory::String, Memory::String, Reply>("std::io::__renamePath", [](Memory::String from, Memory::String to, Reply reply) { std::error_code ec; fs::rename(Memory::u8path(from),Memory::u8path(to),ec); if (ec) fail(reply,ec.message()); }, error) && ok;
    ok = registry.bindTyped<std::int64_t>("std::datetime::__utcNow", []() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }, error) && ok;
    return ok;
}
}  // namespace std_filesystem_glue
