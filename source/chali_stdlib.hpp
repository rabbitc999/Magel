#pragma once
#include "allocator/chali_allocator.hpp"

namespace chali_module
{
namespace embedded
{
/**
 * @brief 指向进程内解压缓冲区的标准库源码视图。
 */
struct Entry
{
    const char *name;
    const char *text;
};
/**
 * @brief 指向进程内解压缓冲区的工程生成器资产视图。
 */
struct GeneratorAsset
{
    const char *path;
    const char *text;
};
/**
 * @brief 首次使用时解压标准库，并取得以空项结尾的模块表。
 * @return 生命周期覆盖整个进程的模块表。
 */
const Entry *table();
/**
 * @brief 按限定模块名称复制内嵌标准库源码。
 * @param name 完整模块名。
 * @param out 找到模块时接收其源码，必须非空。
 * @return 找到模块时返回 true，未找到时保持输出不变。
 */
bool find(const Memory::String &name, Memory::String *out);
/**
 * @brief 首次使用时解压生成器资产，并取得以空项结尾的资产表。
 * @return 生命周期覆盖整个进程的资产表。
 */
const GeneratorAsset *generatorAssetTable();
/**
 * @brief 按路径后缀或唯一文件名查找内嵌工程资产。
 * @param path 调用方需要读取的资产路径。
 * @param out 成功时接收资产内容，必须非空。
 * @return 唯一命中资产时返回 true，未命中或文件名歧义时保持输出不变。
 */
bool findGeneratorAssetByPath(const Memory::String &path, Memory::String *out);
}  // namespace embedded
}  // namespace chali_module
