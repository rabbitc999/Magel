#include "std_compress.hpp"

#include "../../source/lztext/lztext.hpp"
#include "../chali_glue.hpp"

/**
 * @brief CHALI 文本压缩（`std::compress`）：把字符串压成字节序列、再解回来。
 *
 * 底层是仓库自带的 `lztext/`（LZ77 + 规范哈夫曼，纯 C++17、无第三方依赖），
 * 与内嵌标准库/运行时资产用的是同一份实现。
 *
 * 压缩结果是**二进制**（含 NUL 字节），所以 CHALI 侧用 `std::lang::List<uint8>` 表示，
 * 不要当字符串打印或拼接。压不划算时（随机数据、极短文本）lztext 会自动退回
 * 原样存储，结果仍能正常解压，只是体积没变小。
 */
namespace std_compress_glue
{

/**
 * @brief 字节序列类型，与 CHALI 侧 `std::lang::List<uint8>` 对应。
 */
using Bytes = Memory::Vector<std::uint8_t>;
using Context = chali_glue::Context;

/**
 * @brief 注册 `std::compress` 的宿主绑定。
 * @param registry 宿主函数注册表。
 * @param error 注册失败时写入中文错误信息。
 * @return 全部绑定成功返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTyped<Bytes, Memory::String>(
               "std::compress::compress",
               [](const Memory::String &text) {
                   const std::vector<unsigned char> packed = lztext::compress(
                       reinterpret_cast<const unsigned char *>(text.data()), text.size());
                   Bytes out;
                   out.assign(packed.begin(), packed.end());
                   return out;
               },
               error)
           && registry.bindTypedWithContext<Memory::String, Bytes>(
               "std::compress::decompress",
               [](Context &ctx, const Bytes &bytes) {
                   std::string plain;
                   if (!lztext::decompress(bytes.data(), bytes.size(), plain))
                   {
                       ctx.fail("std::compress::decompress：数据不是有效的 lztext 流或已损坏");
                       return Memory::String();
                   }
                   return Memory::String(plain.data(), plain.size());
               },
               error);
}

}  // namespace std_compress_glue
