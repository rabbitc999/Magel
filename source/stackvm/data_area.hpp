/**
 * @file data_area.hpp
 * @brief 静态数据区和布局加载接口。
 */
#pragma once
#include "quad_model.hpp"

namespace stackvm
{
/**
 * @brief 数据区：加载 `.data.bin` 与布局表（`simple_ast` 的 `exportDataLayout()` 输出）。
 *
 * 有了它，四元式里的 `(addr, 符号, _, t)` / `(load, 指针, _, 目标)` / `(store, 指针, 值, _)`
 * 就能真正读写数组 / 结构体所在的那片内存。
 */
struct DataArea
{
    Memory::Vector<std::uint8_t> binary;               ///< 数据区原始字节（不是文本）。
    Memory::Map<Memory::String, std::size_t> symbols;  ///< 符号 → 基址（字节偏移）。
    Memory::Map<Memory::String, std::size_t> strides;  ///< 符号 → 槽位步长（布局行里的 `stride=`）。
    Memory::Map<Memory::String, std::size_t> sizes;    ///< 符号 → 占多少字节（布局行里的 `size=`）。

    /**
     * @brief 加载二进制与布局表。
     * @param binaryPath `.data.bin` 路径。
     * @param layoutPath 布局文件：一行 `符号 offset=0 size=8 type=…`。
     * @param diagnostics 出错信息。
     * @return 成功返回 true。
     */
    bool load(const Memory::String &binaryPath, const Memory::String &layoutPath,
              Memory::Vector<Memory::String> *diagnostics = nullptr);

    /**
     * @brief 从**内存映像 + 布局文本**加载数据区（目标宿主：映像与布局都在 flash 里）。
     *
     * 行格式与文件版一致：`符号 offset=0 size=8 type=… stride=8`；`layoutText` 可为空。
     * @param bytes 数据区字节。
     * @param size 字节数。
     * @param layoutText 布局文本。
     * @param diagnostics 诊断。
     * @return 成功返回 true。
     */
    bool loadFromBytes(const void *bytes, std::size_t size, const Memory::String &layoutText,
                       Memory::Vector<Memory::String> *diagnostics = nullptr);

    /**
     * @brief 数据区大小。
     */
    [[nodiscard]] std::size_t size() const { return binary.size(); }

    /**
     * @brief 读一个以 `\0` 结尾的字符串。
     *
     * 函数指针就是"数据区里存着函数标签的那串字节的地址"，间接调用靠它拿标签。
     * @param address 地址（数据区内的偏移）。
     * @param limit 最多读多少字节（防越界）。
     * @return 字符串；地址越界返回空串。
     */
    [[nodiscard]] Memory::String readCString(std::size_t address, std::size_t limit = 256u) const;

    /**
     * @brief 读 8 字节小端。
     * @param address 地址（数据区内的偏移）。
     * @param out 结果。
     * @return 越界返回 false。
     */
    [[nodiscard]] bool readInt(std::size_t address, std::int64_t &out) const;

    /**
     * @brief 写 8 字节小端。
     * @param address 地址。
     * @param value 值。
     * @return 越界返回 false。
     */
    bool writeInt(std::size_t address, std::int64_t value);

    /**
     * @brief 读 8 字节小端，按 IEEE-754 双精度解释（`(load:f64 …)` 用它）。
     * @param address 地址。
     * @param out 结果。
     * @return 越界返回 false。
     */
    [[nodiscard]] bool readDouble(std::size_t address, double &out) const;

    /**
     * @brief 把双精度写成 8 字节小端（`(store:f64 …)` 用它）。
     * @param address 地址。
     * @param value 值。
     * @return 越界返回 false。
     */
    bool writeDouble(std::size_t address, double value);

    /**
     * @brief 按字节读一段（字符串是"以 `\0` 结尾的字节串"，跨 8 字节槽）。
     * @param address 地址（数据区内的偏移）。
     * @param out 输出缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 越界返回 false。
     */
    [[nodiscard]] bool readBytes(std::size_t address, char *out, std::size_t count) const;
    /**
     * @brief 按字节写一段。
     * @param address 地址。
     * @param in 输入缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 越界返回 false。
     */
    bool writeBytes(std::size_t address, const char *in, std::size_t count);
};

}  // namespace stackvm
