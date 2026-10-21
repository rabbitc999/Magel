/**
 * @file native_format.hpp
 * @brief VM 内部 native format 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 取一个实参的整数值（整数槽直接取，浮点槽取整）。
 * @param slot 待转换或传递的虚拟机值槽。
 * @retval 取一个实参的整数值（整数槽直接取，浮点槽取整）。
 */
std::int64_t nativeInt(const Slot &slot);

/**
 * @brief 按格式串把实参拼成文本。
 *
 * 这是 `printf` 与 `format` **共用的一份实现**（语言侧不写格式化逻辑）。占位符：
 *
 * | 写法 | 说明 |
 * | --- | --- |
 * | `%d` / `%i` | 有符号整数 |
 * | `%u` | 无符号整数 | `%x` / `%X` | 十六进制（小写 / 大写） |
 * | `%f` | 浮点（**默认 3 位小数**，`.N` 给定就按 N） |
 * | `%s` | 字符串（`.N` = 最多取 N 个字符） |
 * | `%c` | 一个字符（整数按低 8 位当字符码） |
 * | `%%` | 一个百分号 |
 *
 * 每个占位符前面可以写旗标 / 宽度 / 精度：`%-8d`（左对齐）、`%08.2f`（补零）、
 * `%.3s`（截断）。数字部分直接交给 `snprintf`，所以语义就是 C 的那一套。
 *
 * @param ctx 环境。
 * @param format 格式串（已经过语言的转义处理）。
 * @param args 实参（下标 0 起）。
 * @param argCount 实参个数。
 * @param out 拼出来的文本。
 * @return 成功返回 true；`ctx.error` 里是出错原因。
 */
bool nativeFormatInto(NativeContext &ctx, const Memory::String &format, const Memory::Vector<Slot> &args,
                      std::size_t argCount, Memory::String *out);
}  // namespace stackvm::runtimeDetail
