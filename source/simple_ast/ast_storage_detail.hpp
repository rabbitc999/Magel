/**
 * @file ast_storage_detail.hpp
 * @brief 四元式生成与数据导出共用的轻量存储布局查询。
 */
#pragma once
#include "simple_ast.hpp"
namespace simple_ast
{
/**
 * @brief 查询标量或句柄的物理存储宽度，u8 按一个字节保存。
 * @param type 元素或独立数据项的低层类型。
 * @return u8 返回 1，其他标量和句柄继续返回 8。
 */
inline std::size_t scalarStorageBytes(const TypePtr &type)
{
    return type && type->kind == Type::Kind::Uint && type->bits == 8 ? 1u : 8u;
}

}  // namespace simple_ast
