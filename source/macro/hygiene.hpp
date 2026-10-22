#ifndef CHALI_MACRO_HYGIENE_HPP
#define CHALI_MACRO_HYGIENE_HPP

#include <cstdint>
#include <memory>
#include <string_view>

namespace chali_macro
{
/**
 * @brief 卫生名字的稳定绑定身份，零表示尚未解析到声明。
 */
using BindingId = std::uint32_t;
/**
 * @brief 不引入额外运行时块的词法作用域身份。
 */
using ScopeId = std::uint32_t;
/**
 * @brief 保留宏定义或调用来源的语法上下文身份。
 */
using ContextId = std::uint32_t;

/**
 * @brief 按语法上下文和词法作用域解析声明身份，不改写用户名字。
 * @note 调用者片段保留原上下文；宏体使用捕获定义环境的新上下文。
 */
class HygieneResolver
{
public:
    /**
     * @brief 建立编号为零的根作用域与根语法上下文。
     */
    HygieneResolver();
    /**
     * @brief 释放本次编译中的作用域、展开上下文与名字绑定。
     */
    ~HygieneResolver();
    /**
     * @brief 禁止复制解析会话，避免绑定身份和上下文被混用。
     * @param other 不允许复制的另一个卫生解析器。
     * @note 此操作已删除。
     */
    HygieneResolver(const HygieneResolver &other) = delete;
    /**
     * @brief 禁止以其他解析会话覆盖当前绑定表。
     * @param other 不允许赋值的另一个卫生解析器。
     * @note 此操作已删除，不会执行赋值。
     */
    HygieneResolver &operator=(const HygieneResolver &other) = delete;
    /**
     * @brief 创建词法子作用域，保留与运行时块结构相同的父子关系。
     * @param parent 已存在的词法父作用域。
     * @return 新作用域编号；无效父编号抛出 out_of_range。
     */
    ScopeId createScope(ScopeId parent);
    /**
     * @brief 为一次展开建立新上下文，捕获宏定义处环境而不是调用环境。
     * @param definitionScope 宏定义所在的词法作用域。
     * @param definitionContext 宏定义片段的卫生上下文。
     * @return 新展开上下文；非法参数抛出 out_of_range。
     */
    ContextId createExpansion(ScopeId definitionScope, ContextId definitionContext);
    /**
     * @brief 在当前作用域登记一个名字及其来源上下文。
     * @param name 用户声明的原始名字。
     * @param scope 声明所在作用域。
     * @param context 声明片段携带的上下文。
     * @return 唯一绑定身份；同作用域同上下文重复声明返回零。
     */
    BindingId declare(std::string_view name, ScopeId scope, ContextId context);
    /**
     * @brief 优先查找同上下文词法声明，未找到时转到宏定义环境。
     * @param name 引用的原始名字。
     * @param scope 引用所在的词法作用域。
     * @param context 引用片段的卫生上下文。
     * @return 对应声明身份，未声明返回零；非法编号抛出 out_of_range。
     */
    BindingId resolve(std::string_view name, ScopeId scope, ContextId context) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace chali_macro
#endif
