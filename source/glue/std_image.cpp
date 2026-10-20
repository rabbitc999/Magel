#include "std_image.hpp"

#include "../magel_glue.hpp"
#include "std_native_record.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

/**
 * @brief MAGEL 图片处理胶水：BMP/PPM/PGM 编解码、卷积、池化、格式转换与大津法。
 *
 * 槽布局与 std::image::ImageReply 一致（字段顺序即槽序）：
 *   0 error(String)  1 width  2 height  3 pixels(List<uint8>)
 *   4 bytes(List<uint8>)  5 gray(List<uint8>)  6 argb(List<uint32>)
 *   7 rgb565(List<uint16>)  8 threshold  9 r  10 g  11 b  12 a
 *
 * 像素缓冲统一为 RGBA8888，每像素 4 字节，按行优先排列。
 * 可恢复错误写入槽 0 并返回；不可恢复（胶水层调用错）由 ctx.fail 触发 panic。
 */
namespace std_image_glue
{
using Context = magel_glue::Context;
using Bytes = Memory::Vector<std::uint8_t>;

/**
 * @brief 图片 opcode 拥有的连续 RGBA 像素及尺寸。
 */
struct ImageData
{
    std::int64_t width = 0, height = 0;
    Bytes pixels;
};

/**
 * @brief 深复制图片尺寸与像素缓冲。
 * @param pointer 源图片记录。
 * @param error 复制失败时的诊断输出。
 * @return 新图片记录。
 */
inline void *copyImage(const void *pointer, Memory::String &error)
{
    (void)error;
    return new ImageData(*static_cast<const ImageData *>(pointer));
}

/**
 * @brief 释放宿主图片及其像素缓冲。
 * @param pointer 待释放图片记录。
 */
inline void destroyImage(void *pointer) { delete static_cast<ImageData *>(pointer); }

inline const stackvm::NativeRecordType imageType{ "std::image::Image", copyImage, destroyImage };

/**
 * @brief 校验并读取图片 opcode 的宿主记录。
 * @param ctx 当前调用上下文。
 * @param handle 图片记录句柄。
 * @return 有效图片；类型不符或已释放时报告错误并返回空。
 */
inline ImageData *imageData(Context &ctx, std::int64_t handle)
{
    auto *image = static_cast<ImageData *>(ctx.nativeRecord(handle, imageType));
    if (!image) ctx.fail("Image opcode 已释放或类型不匹配");
    return image;
}

/**
 * @brief 将合法尺寸与 RGBA 像素收进当前运行时。
 * @param ctx 当前调用上下文。
 * @param width 图片宽度。
 * @param height 图片高度。
 * @param pixels 行优先像素，空缓冲表示分配透明黑图。
 * @return 新图片句柄；尺寸或长度无效时报告错误并返回零。
 */
inline std::int64_t makeImage(Context &ctx, std::int64_t width, std::int64_t height, Bytes pixels = {})
{
    if (width <= 0 || height <= 0 || width > 536870911 / height)
    {
        ctx.fail("图片尺寸无效或像素缓冲超过预算");
        return 0;
    }
    const auto size = static_cast<std::size_t>(width * height * 4);
    if (pixels.empty()) pixels.resize(size, 0);
    if (pixels.size() != size)
    {
        ctx.fail("图片像素缓冲长度与尺寸不符");
        return 0;
    }
    return ctx.makeNativeRecord(imageType, new ImageData{ width, height, std::move(pixels) });
}

struct ReplyTag
{
};
using Reply = magel_glue::Record<ReplyTag>;

/**
 * @brief 写入可恢复错误并清空结果字段。
 * @param reply 当前回执视图。
 * @param message 错误文本。
 */
inline void fail(Reply reply, const std::string &message)
{
    reply.set<Memory::String>(0, Memory::String(message.data(), message.size()));
    reply.set<std::int64_t>(1, 0);
    reply.set<std::int64_t>(2, 0);
    reply.set<std::int64_t>(8, 0);
}

/**
 * @brief 读取回执里已预分配的 List 槽句柄。
 * @param reply 回执视图。
 * @param slot 槽下标。
 * @return 列表句柄。
 */
inline std::int64_t listSlot(Reply reply, std::size_t slot)
{
    return reply.field<std::int64_t>(slot);
}

/**
 * @brief 读取传入的 RGBA 像素缓冲。
 * @param ctx 调用上下文。
 * @param handle 列表句柄。
 * @param expected 期望字节数（width*height*4）。
 * @param out 输出缓冲。
 * @return 长度匹配返回 true。
 */
inline bool readPixels(Context &ctx, std::int64_t handle, std::size_t expected, Bytes &out)
{
    Memory::Vector<magel_glue::Value> raw;
    if (!collection_glue::readList(ctx, handle, raw))
    {
        ctx.fail("图片处理：像素缓冲不是有效的 List");
        return false;
    }
    if (raw.size() != expected)
    {
        ctx.fail("图片处理：像素缓冲长度与 width*height*4 不符");
        return false;
    }
    out.resize(expected);
    for (std::size_t i = 0; i < expected; ++i) out[i] = static_cast<std::uint8_t>(raw[i].asInt() & 0xFF);
    return true;
}

/**
 * @brief 把 RGBA 缓冲写入回执的 pixels 槽。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param pixels 源缓冲。
 */
inline void writePixels(Context &ctx, Reply reply, const Bytes &pixels)
{
    const std::int64_t handle = listSlot(reply, 3);
    for (std::uint8_t byte : pixels) collection_glue::listPush(ctx, handle, magel_glue::Value::fromInt(byte));
}

/**
 * @brief 把字节序列写入回执的 bytes 槽。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param bytes 源字节。
 */
inline void writeBytes(Context &ctx, Reply reply, const Bytes &bytes)
{
    const std::int64_t handle = listSlot(reply, 4);
    for (std::uint8_t byte : bytes) collection_glue::listPush(ctx, handle, magel_glue::Value::fromInt(byte));
}

/**
 * @brief 把灰度缓冲写入回执的 gray 槽。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param gray 源缓冲。
 */
inline void writeGray(Context &ctx, Reply reply, const Bytes &gray)
{
    const std::int64_t handle = listSlot(reply, 5);
    for (std::uint8_t byte : gray) collection_glue::listPush(ctx, handle, magel_glue::Value::fromInt(byte));
}

/* ---- 编解码辅助 ---- */

inline std::uint16_t readU16LE(const std::uint8_t *p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
/**
 * @brief 从已验证的连续四字节读取小端无符号整数。
 * @param p 至少可读取四字节的输入地址。
 * @return 对应的 32 位无符号值。
 */
inline std::uint32_t readU32LE(const std::uint8_t *p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
/**
 * @brief 向编码缓冲追加一个小端 16 位整数。
 * @param out 追加编码字节的输出缓冲。
 * @param v 需要保存的数值。
 */
inline void writeU16LE(Bytes &out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
/**
 * @brief 向编码缓冲追加一个小端 32 位整数。
 * @param out 追加编码字节的输出缓冲。
 * @param v 需要保存的数值。
 */
inline void writeU32LE(Bytes &out, std::uint32_t v)
{
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

/**
 * @brief ITU-R BT.601 亮度：灰度 = (299R + 587G + 114B) / 1000。
 */
inline std::uint8_t luma(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    return static_cast<std::uint8_t>((299 * r + 587 * g + 114 * b) / 1000);
}

/**
 * @brief 解码 BMP（BI_RGB 无压缩，24/32 位，行序不限）。
 * @param data 文件字节。
 * @param width 输出宽。
 * @param height 输出高。
 * @param pixels 输出 RGBA。
 * @return 成功返回空串，失败返回错误说明。
 */
inline std::string decodeBmp(const Bytes &data, std::int64_t &width, std::int64_t &height, Bytes &pixels)
{
    if (data.size() < 54 || data[0] != 'B' || data[1] != 'M') return "BMP：文件头不合法";
    const std::uint32_t offset = readU32LE(data.data() + 10);
    const std::uint32_t headerSize = readU32LE(data.data() + 14);
    if (headerSize < 40) return "BMP：仅支持 BITMAPINFOHEADER 及以上";
    if (data[30] != 0) return "BMP：仅支持无压缩 BI_RGB";
    const std::int32_t w = static_cast<std::int32_t>(readU32LE(data.data() + 18));
    const std::int32_t rawH = static_cast<std::int32_t>(readU32LE(data.data() + 22));
    if (w <= 0 || rawH == 0) return "BMP：宽高不合法";
    const bool topDown = rawH < 0;
    const std::int64_t h = rawH < 0 ? -static_cast<std::int64_t>(rawH) : rawH;
    const std::uint16_t planes = readU16LE(data.data() + 26);
    const std::uint16_t bpp = readU16LE(data.data() + 28);
    if (planes != 1 || (bpp != 24 && bpp != 32)) return "BMP：仅支持 24/32 位";
    const std::size_t stride = (static_cast<std::size_t>(w) * (bpp / 8) + 3) & ~std::size_t(3);
    if (static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(h) > data.size())
        return "BMP：像素数据越界";
    pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4, 0);
    for (std::int64_t row = 0; row < h; ++row)
    {
        const std::uint8_t *src = data.data() + offset + static_cast<std::size_t>(topDown ? row : (h - 1 - row)) * stride;
        std::uint8_t *dst = pixels.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(w) * 4;
        for (std::int64_t x = 0; x < w; ++x)
        {
            dst[x * 4 + 0] = src[x * (bpp / 8) + 2];
            dst[x * 4 + 1] = src[x * (bpp / 8) + 1];
            dst[x * 4 + 2] = src[x * (bpp / 8) + 0];
            dst[x * 4 + 3] = bpp == 32 ? src[x * 4 + 3] : 255;
        }
    }
    width = w;
    height = h;
    return "";
}

/**
 * @brief 编码 24 位 BMP（BI_RGB，自底向上，行按 4 字节对齐）。
 */
inline Bytes encodeBmp(std::int64_t width, std::int64_t height, const Bytes &pixels)
{
    const std::size_t stride = (static_cast<std::size_t>(width) * 3 + 3) & ~std::size_t(3);
    const std::uint32_t dataSize = static_cast<std::uint32_t>(stride * static_cast<std::size_t>(height));
    Bytes out;
    out.reserve(54 + dataSize);
    out.push_back('B');
    out.push_back('M');
    writeU32LE(out, 54 + dataSize);
    writeU32LE(out, 0);
    writeU32LE(out, 54);
    writeU32LE(out, 40);
    writeU32LE(out, static_cast<std::uint32_t>(width));
    writeU32LE(out, static_cast<std::uint32_t>(height));
    writeU16LE(out, 1);
    writeU16LE(out, 24);
    writeU32LE(out, 0);
    writeU32LE(out, dataSize);
    writeU32LE(out, 2835);
    writeU32LE(out, 2835);
    writeU32LE(out, 0);
    writeU32LE(out, 0);
    for (std::int64_t row = height - 1; row >= 0; --row)
    {
        const std::uint8_t *src = pixels.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 4;
        for (std::int64_t x = 0; x < width; ++x)
        {
            out.push_back(src[x * 4 + 2]);
            out.push_back(src[x * 4 + 1]);
            out.push_back(src[x * 4 + 0]);
        }
        for (std::size_t fill = static_cast<std::size_t>(width) * 3; fill < stride; ++fill) out.push_back(0);
    }
    return out;
}

/**
 * @brief 解码 Netpbm 二进制格式（P5 灰度 / P6 彩色，maxval ≤ 255）。
 */
inline std::string decodeNetpbm(const Bytes &data, std::int64_t &width, std::int64_t &height, Bytes &pixels)
{
    if (data.size() < 2 || data[0] != 'P') return "Netpbm：魔数不合法";
    const char kind = static_cast<char>(data[1]);
    if (kind != '5' && kind != '6') return "Netpbm：仅支持二进制 P5/P6";
    std::size_t pos = 2;
    std::int64_t fields[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; ++i)
    {
        auto skip = [&] {
            while (pos < data.size())
            {
                const std::uint8_t c = data[pos];
                if (c == '#')
                {
                    while (pos < data.size() && data[pos] != '\n') ++pos;
                }
                else if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                    ++pos;
                else
                    break;
            }
        };
        skip();
        if (pos >= data.size() || data[pos] < '0' || data[pos] > '9') return "Netpbm：头部不完整";
        std::int64_t value = 0;
        while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9')
            value = value * 10 + (data[pos++] - '0');
        fields[i] = value;
    }
    ++pos; /* 单个空白分隔符 */
    width = fields[0];
    height = fields[1];
    const std::int64_t maxval = fields[2];
    if (width <= 0 || height <= 0 || maxval <= 0 || maxval > 255) return "Netpbm：宽高或 maxval 不合法";
    const std::size_t channels = kind == '6' ? 3 : 1;
    if (data.size() - pos < static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * channels)
        return "Netpbm：像素数据不足";
    pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0);
    for (std::size_t p = 0; p < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++p)
    {
        std::uint8_t rgb[3];
        for (std::size_t c = 0; c < channels; ++c) rgb[c] = data[pos++];
        pixels[p * 4 + 0] = rgb[0];
        pixels[p * 4 + 1] = channels == 3 ? rgb[1] : rgb[0];
        pixels[p * 4 + 2] = channels == 3 ? rgb[2] : rgb[0];
        pixels[p * 4 + 3] = 255;
    }
    return "";
}

/**
 * @brief 将连续 RGBA 像素编码为指定 Netpbm 格式。
 * @param magic Netpbm 文件的格式标识。
 * @param width 输入图像的像素宽度。
 * @param height 输入图像的像素高度。
 * @param pixels 连续保存的 RGBA 像素。
 * @return 包含文件头及像素数据的完整字节序列。
 */
inline Bytes encodeNetpbm(const char *magic, std::int64_t width, std::int64_t height, const Bytes &pixels)
{
    Bytes out;
    std::string header = std::string(magic) + "\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";
    out.insert(out.end(), header.begin(), header.end());
    for (std::size_t p = 0; p < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++p)
    {
        if (magic[1] == '6')
        {
            out.push_back(pixels[p * 4 + 0]);
            out.push_back(pixels[p * 4 + 1]);
            out.push_back(pixels[p * 4 + 2]);
        }
        else
        {
            out.push_back(luma(pixels[p * 4 + 0], pixels[p * 4 + 1], pixels[p * 4 + 2]));
        }
    }
    return out;
}

/**
 * @brief 注册图片处理胶水。
 * @param registry 注册表。
 * @param error 绑定失败说明。
 * @return 是否全部成功。
 */
bool bind(magel_glue::Registry &registry, Memory::String *error)
{
    bool records = registry.bindTypedWithContext<std::int64_t>("std::image::__std_imageNew", [](Context &ctx) { return ctx.makeNativeRecord(imageType, new ImageData); }, error);
    records = registry.bindTypedWithContext<void, std::int64_t>("std::image::__std_imageDrop", [](Context &ctx, std::int64_t handle) { ctx.releaseNativeRecord(handle); }, error) && records;
    records = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::image::__std_imageCopy", [](Context &ctx, std::int64_t handle) { auto *image = imageData(ctx, handle); return image ? ctx.makeNativeRecord(imageType, new ImageData(*image)) : 0; }, error) && records;
    records = registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int64_t>("std::image::__std_imageMake", [](Context &ctx, std::int64_t width, std::int64_t height) { return makeImage(ctx, width, height); }, error) && records;
    records = registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int64_t, Bytes>("std::image::__std_imageFromPixels", [](Context &ctx, std::int64_t width, std::int64_t height, Bytes pixels) { return makeImage(ctx, width, height, std::move(pixels)); }, error) && records;
    records = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::image::__std_imageWidth", [](Context &ctx, std::int64_t handle) { auto *image = imageData(ctx, handle); return image ? image->width : 0; }, error) && records;
    records = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::image::__std_imageHeight", [](Context &ctx, std::int64_t handle) { auto *image = imageData(ctx, handle); return image ? image->height : 0; }, error) && records;
    records = registry.bindTypedWithContext<Bytes, std::int64_t>("std::image::__std_imagePixels", [](Context &ctx, std::int64_t handle) { auto *image = imageData(ctx, handle); return image ? image->pixels : Bytes{}; }, error) && records;
    records = registry.bindTypedWithContext<std::uint8_t, std::int64_t, std::int64_t>("std::image::__std_imageByte", [](Context &ctx, std::int64_t handle, std::int64_t index) { auto *image = imageData(ctx, handle); if (!image) return std::uint8_t{}; if (index < 0 || static_cast<std::uint64_t>(index) >= image->pixels.size()) { ctx.fail("图片像素下标越界"); return std::uint8_t{}; } return image->pixels[static_cast<std::size_t>(index)]; }, error) && records;
    records = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, std::uint8_t>("std::image::__std_imageSetByte", [](Context &ctx, std::int64_t reference, std::int64_t index, std::uint8_t value) { auto *image = imageData(ctx, native_record_glue::referenceHandle(ctx, reference, imageType)); if (!image) return; if (index < 0 || static_cast<std::uint64_t>(index) >= image->pixels.size()) { ctx.fail("图片像素下标越界"); return; } image->pixels[static_cast<std::size_t>(index)] = value; }, error) && records;

    bool ok = registry.bindTypedWithContext<void, Bytes, Reply>("std::image::__decode", [](Context &ctx, Bytes data, Reply reply) {
        std::int64_t width = 0, height = 0;
        Bytes pixels;
        std::string message;
        if (data.size() >= 2 && data[0] == 'B' && data[1] == 'M') message = decodeBmp(data, width, height, pixels);
        else if (data.size() >= 2 && data[0] == 'P') message = decodeNetpbm(data, width, height, pixels);
        else message = "图片解码：无法识别的格式（支持 BMP/P5/P6）";
        if (!message.empty()) { fail(reply, message); return; }
        reply.set<std::int64_t>(1, width);
        reply.set<std::int64_t>(2, height);
        writePixels(ctx, reply, pixels); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, Reply>("std::image::__encodeBmp", [](Context &ctx, std::int64_t imageHandle, Reply reply) {
        auto *image = imageData(ctx, imageHandle); if (!image) return;
        const auto width = image->width, height = image->height;
        const auto &pixels = image->pixels;
        if (width <= 0 || height <= 0) { fail(reply, "BMP 编码：宽高必须为正"); return; }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) { fail(reply, "BMP 编码：像素缓冲长度不符"); return; }
        writeBytes(ctx, reply, encodeBmp(width, height, pixels)); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, Reply>("std::image::__encodePpm", [](Context &ctx, std::int64_t imageHandle, Reply reply) {
        auto *image = imageData(ctx, imageHandle); if (!image) return;
        const auto width = image->width, height = image->height;
        const auto &pixels = image->pixels;
        if (width <= 0 || height <= 0) { fail(reply, "PPM 编码：宽高必须为正"); return; }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) { fail(reply, "PPM 编码：像素缓冲长度不符"); return; }
        writeBytes(ctx, reply, encodeNetpbm("P6", width, height, pixels)); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, Reply>("std::image::__encodePgm", [](Context &ctx, std::int64_t imageHandle, Reply reply) {
        auto *image = imageData(ctx, imageHandle); if (!image) return;
        const auto width = image->width, height = image->height;
        const auto &pixels = image->pixels;
        if (width <= 0 || height <= 0) { fail(reply, "PGM 编码：宽高必须为正"); return; }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) { fail(reply, "PGM 编码：像素缓冲长度不符"); return; }
        writeBytes(ctx, reply, encodeNetpbm("P5", width, height, pixels)); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, Memory::Vector<double>, std::int64_t, std::int64_t, Reply>("std::image::__convolve", [](Context &ctx, std::int64_t handle, Memory::Vector<double> kernel, std::int64_t kw, std::int64_t kh, Reply reply) {
        auto *image = imageData(ctx, handle); if (!image) return;
        const auto width = image->width, height = image->height;
        const auto &pixels = image->pixels;
        if (width <= 0 || height <= 0) { fail(reply, "卷积：宽高必须为正"); return; }
        if (kw <= 0 || kh <= 0 || kw % 2 == 0 || kh % 2 == 0 || kw > 15 || kh > 15) { fail(reply, "卷积：核边长必须是不超过 15 的奇数"); return; }
        if (kernel.size() != static_cast<std::size_t>(kw) * static_cast<std::size_t>(kh)) { fail(reply, "卷积：核元素数与 kw*kh 不符"); return; }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) { fail(reply, "卷积：像素缓冲长度不符"); return; }
        const std::int64_t cx = kw / 2, cy = kh / 2;
        Bytes out(pixels.size(), 0);
        for (std::int64_t y = 0; y < height; ++y)
        {
            for (std::int64_t x = 0; x < width; ++x)
            {
                double acc[3] = {0.0, 0.0, 0.0};
                for (std::int64_t ky = 0; ky < kh; ++ky)
                {
                    const std::int64_t sy = std::min(std::max(y + ky - cy, static_cast<std::int64_t>(0)), height - 1);
                    for (std::int64_t kx = 0; kx < kw; ++kx)
                    {
                        const std::int64_t sx = std::min(std::max(x + kx - cx, static_cast<std::int64_t>(0)), width - 1);
                        const double weight = kernel[static_cast<std::size_t>(ky) * static_cast<std::size_t>(kw) + static_cast<std::size_t>(kx)];
                        const std::uint8_t *p = pixels.data() + (static_cast<std::size_t>(sy) * static_cast<std::size_t>(width) + static_cast<std::size_t>(sx)) * 4;
                        acc[0] += weight * p[0];
                        acc[1] += weight * p[1];
                        acc[2] += weight * p[2];
                    }
                }
                std::uint8_t *dst = out.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
                for (int c = 0; c < 3; ++c)
                    dst[c] = static_cast<std::uint8_t>(std::min(std::max(acc[c] + 0.5, 0.0), 255.0));
                dst[3] = pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4 + 3];
            }
        }
        reply.set<std::int64_t>(1, width);
        reply.set<std::int64_t>(2, height);
        writePixels(ctx, reply, out); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, std::int64_t, std::int64_t, Reply>("std::image::__pool", [](Context &ctx, std::int64_t handle, std::int64_t mode, std::int64_t kw, std::int64_t kh, Reply reply) {
        auto *image = imageData(ctx, handle); if (!image) return;
        const auto width = image->width, height = image->height;
        const auto &pixels = image->pixels;
        if (width <= 0 || height <= 0) { fail(reply, "池化：宽高必须为正"); return; }
        if (kw <= 0 || kh <= 0) { fail(reply, "池化：窗口边长必须为正"); return; }
        if (mode != 0 && mode != 1) { fail(reply, "池化：mode 只能是 0(max) 或 1(avg)"); return; }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) { fail(reply, "池化：像素缓冲长度不符"); return; }
        const std::int64_t ow = width / kw, oh = height / kh;
        if (ow <= 0 || oh <= 0) { fail(reply, "池化：窗口大于图片"); return; }
        Bytes out(static_cast<std::size_t>(ow) * static_cast<std::size_t>(oh) * 4, 0);
        for (std::int64_t oy = 0; oy < oh; ++oy)
        {
            for (std::int64_t ox = 0; ox < ow; ++ox)
            {
                std::int64_t best[4] = {0, 0, 0, 0};
                std::int64_t sum[4] = {0, 0, 0, 0};
                std::int64_t count = 0;
                for (std::int64_t ky = 0; ky < kh; ++ky)
                {
                    for (std::int64_t kx = 0; kx < kw; ++kx)
                    {
                        const std::uint8_t *p = pixels.data() + (static_cast<std::size_t>(oy * kh + ky) * static_cast<std::size_t>(width) + static_cast<std::size_t>(ox * kw + kx)) * 4;
                        for (int c = 0; c < 4; ++c)
                        {
                            if ((ky == 0 && kx == 0) || p[c] > best[c]) best[c] = p[c];
                            sum[c] += p[c];
                        }
                        ++count;
                    }
                }
                std::uint8_t *dst = out.data() + (static_cast<std::size_t>(oy) * static_cast<std::size_t>(ow) + static_cast<std::size_t>(ox)) * 4;
                for (int c = 0; c < 4; ++c) dst[c] = static_cast<std::uint8_t>(mode == 0 ? best[c] : (sum[c] + count / 2) / count);
            }
        }
        reply.set<std::int64_t>(1, ow);
        reply.set<std::int64_t>(2, oh);
        writePixels(ctx, reply, out); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, Reply>("std::image::__toGray", [](Context &ctx, std::int64_t imageHandle, Reply reply) {
        auto *image = imageData(ctx, imageHandle); if (!image) return;
        const auto width = image->width, height = image->height;
        const auto &pixels = image->pixels;
        if (width <= 0 || height <= 0) { fail(reply, "灰度转换：宽高必须为正"); return; }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) { fail(reply, "灰度转换：像素缓冲长度不符"); return; }
        Bytes gray(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
        for (std::size_t p = 0; p < gray.size(); ++p)
            gray[p] = luma(pixels[p * 4 + 0], pixels[p * 4 + 1], pixels[p * 4 + 2]);
        writeGray(ctx, reply, gray); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, Bytes, Reply>("std::image::__fromGray", [](Context &ctx, std::int64_t width, std::int64_t height, Bytes gray, Reply reply) {
        if (width <= 0 || height <= 0) { fail(reply, "灰度还原：宽高必须为正"); return; }
        if (gray.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) { fail(reply, "灰度还原：灰度缓冲长度不符"); return; }
        Bytes out(gray.size() * 4, 0);
        for (std::size_t p = 0; p < gray.size(); ++p)
        {
            out[p * 4 + 0] = gray[p];
            out[p * 4 + 1] = gray[p];
            out[p * 4 + 2] = gray[p];
            out[p * 4 + 3] = 255;
        }
        reply.set<std::int64_t>(1, width);
        reply.set<std::int64_t>(2, height);
        writePixels(ctx, reply, out); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, Reply>("std::image::__toARGB", [](Context &ctx, std::int64_t imageHandle, Reply reply) {
        auto *image = imageData(ctx, imageHandle); if (!image) return;
        const auto width = image->width, height = image->height;
        const auto &pixels = image->pixels;
        if (width <= 0 || height <= 0) { fail(reply, "ARGB 转换：宽高必须为正"); return; }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) { fail(reply, "ARGB 转换：像素缓冲长度不符"); return; }
        const std::int64_t handle = listSlot(reply, 6);
        for (std::size_t p = 0; p < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++p)
        {
            const std::uint32_t value = (static_cast<std::uint32_t>(pixels[p * 4 + 3]) << 24) | (static_cast<std::uint32_t>(pixels[p * 4 + 0]) << 16) |
                                        (static_cast<std::uint32_t>(pixels[p * 4 + 1]) << 8) | static_cast<std::uint32_t>(pixels[p * 4 + 2]);
            collection_glue::listPush(ctx, handle, magel_glue::Value::fromInt(static_cast<std::int64_t>(value)));
        } }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, Memory::Vector<std::uint32_t>, Reply>("std::image::__fromARGB", [](Context &ctx, std::int64_t width, std::int64_t height, Memory::Vector<std::uint32_t> argb, Reply reply) {
        if (width <= 0 || height <= 0) { fail(reply, "ARGB 还原：宽高必须为正"); return; }
        if (argb.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) { fail(reply, "ARGB 还原：缓冲长度不符"); return; }
        Bytes out(argb.size() * 4, 0);
        for (std::size_t p = 0; p < argb.size(); ++p)
        {
            const std::uint32_t value = argb[p];
            out[p * 4 + 0] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
            out[p * 4 + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
            out[p * 4 + 2] = static_cast<std::uint8_t>(value & 0xFF);
            out[p * 4 + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
        }
        reply.set<std::int64_t>(1, width);
        reply.set<std::int64_t>(2, height);
        writePixels(ctx, reply, out); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, Reply>("std::image::__toRGB565", [](Context &ctx, std::int64_t imageHandle, Reply reply) {
        auto *image = imageData(ctx, imageHandle); if (!image) return;
        const auto width = image->width, height = image->height;
        const auto &pixels = image->pixels;
        if (width <= 0 || height <= 0) { fail(reply, "RGB565 转换：宽高必须为正"); return; }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) { fail(reply, "RGB565 转换：像素缓冲长度不符"); return; }
        const std::int64_t handle = listSlot(reply, 7);
        for (std::size_t p = 0; p < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++p)
        {
            const std::uint16_t value = static_cast<std::uint16_t>(((pixels[p * 4 + 0] & 0xF8) << 8) | ((pixels[p * 4 + 1] & 0xFC) << 3) | (pixels[p * 4 + 2] >> 3));
            collection_glue::listPush(ctx, handle, magel_glue::Value::fromInt(value));
        } }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, Memory::Vector<std::uint16_t>, Reply>("std::image::__fromRGB565", [](Context &ctx, std::int64_t width, std::int64_t height, Memory::Vector<std::uint16_t> rgb565, Reply reply) {
        if (width <= 0 || height <= 0) { fail(reply, "RGB565 还原：宽高必须为正"); return; }
        if (rgb565.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) { fail(reply, "RGB565 还原：缓冲长度不符"); return; }
        Bytes out(rgb565.size() * 4, 0);
        for (std::size_t p = 0; p < rgb565.size(); ++p)
        {
            const std::uint16_t value = rgb565[p];
            const int r5 = (value >> 11) & 0x1F, g6 = (value >> 5) & 0x3F, b5 = value & 0x1F;
            out[p * 4 + 0] = static_cast<std::uint8_t>((r5 << 3) | (r5 >> 2));
            out[p * 4 + 1] = static_cast<std::uint8_t>((g6 << 2) | (g6 >> 4));
            out[p * 4 + 2] = static_cast<std::uint8_t>((b5 << 3) | (b5 >> 2));
            out[p * 4 + 3] = 255;
        }
        reply.set<std::int64_t>(1, width);
        reply.set<std::int64_t>(2, height);
        writePixels(ctx, reply, out); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, Bytes, Reply>("std::image::__otsu", [](Context &, std::int64_t width, std::int64_t height, Bytes gray, Reply reply) {
        if (width <= 0 || height <= 0) { fail(reply, "大津法：宽高必须为正"); return; }
        if (gray.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) { fail(reply, "大津法：灰度缓冲长度不符"); return; }
        std::int64_t histogram[256] = {0};
        for (std::uint8_t value : gray) ++histogram[value];
        const std::int64_t total = static_cast<std::int64_t>(gray.size());
        double sumAll = 0.0;
        for (int v = 0; v < 256; ++v) sumAll += static_cast<double>(v) * histogram[v];
        double sumBackground = 0.0;
        std::int64_t countBackground = 0;
        double bestVariance = -1.0;
        int bestThreshold = 0;
        for (int v = 0; v < 256; ++v)
        {
            countBackground += histogram[v];
            if (countBackground == 0) continue;
            const std::int64_t countForeground = total - countBackground;
            if (countForeground == 0) break;
            sumBackground += static_cast<double>(v) * histogram[v];
            const double wB = static_cast<double>(countBackground) / total;
            const double wF = static_cast<double>(countForeground) / total;
            const double meanB = sumBackground / countBackground;
            const double meanF = (sumAll - sumBackground) / countForeground;
            const double variance = wB * wF * (meanB - meanF) * (meanB - meanF);
            if (variance > bestVariance) { bestVariance = variance; bestThreshold = v; }
        }
        reply.set<std::int64_t>(8, bestThreshold); }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, Bytes, std::int64_t, Reply>("std::image::__binarize", [](Context &ctx, std::int64_t width, std::int64_t height, Bytes gray, std::int64_t threshold, Reply reply) {
        if (width <= 0 || height <= 0) { fail(reply, "二值化：宽高必须为正"); return; }
        if (threshold < 0 || threshold > 255) { fail(reply, "二值化：阈值必须在 0..255"); return; }
        if (gray.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) { fail(reply, "二值化：灰度缓冲长度不符"); return; }
        Bytes out(gray.size(), 0);
        for (std::size_t p = 0; p < gray.size(); ++p) out[p] = gray[p] > threshold ? 255 : 0;
        writeGray(ctx, reply, out); }, error) && ok;
    return ok && records;
}
}  // namespace std_image_glue
