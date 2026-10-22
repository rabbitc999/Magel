#ifndef CHALI_MACRO_SIGNATURE_HPP
#define CHALI_MACRO_SIGNATURE_HPP
#include "../syntax/source_text.hpp"

namespace chali_macro
{
/**
 * @brief 编辑器显示的单个形参组，不执行或展开宏体。
 */
struct SignatureGroup
{
    char opening = '(';
    Memory::Vector<Memory::String> params;
    Memory::String rest;
};
/**
 * @brief 不完整文档可使用的宏头模型，最终语法合法性由生成解析器判断。
 */
struct Signature
{
    Memory::String name;
    Memory::Vector<Memory::String> generics;
    Memory::Vector<SignatureGroup> groups;
    bool replacement = false;
};
/**
 * @brief 为编辑器读取宏名称、类型参数和显式形参组，不接受已删除的括号保护标注。
 * @param source 已屏蔽注释的声明行。
 * @param signature 接收声明头。
 * @param error 接收宏头错误。
 * @return 宏头结构有效时返回真；不替代实际编译诊断。
 */
bool readSignature(const Memory::String &source, Signature &signature, Memory::String &error);
}  // namespace chali_macro
#endif
