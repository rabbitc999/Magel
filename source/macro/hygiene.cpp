#include "hygiene.hpp"
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace chali_macro
{
/**
 * @brief 存储词法作用域与语法上下文；上下文只引用已经建立的定义环境。
 */
struct HygieneResolver::Impl
{
    static constexpr auto noScope = std::numeric_limits<ScopeId>::max();
    struct Scope
    {
        ScopeId parent;
        std::map<std::pair<ContextId, std::string>, BindingId> bindings;
    };
    struct Context
    {
        ScopeId definitionScope;
        ContextId definitionContext;
    };
    std::vector<Scope> scopes{ Scope{ noScope, {} } };
    std::vector<Context> contexts{ Context{ 0, 0 } };
    BindingId nextBinding = 1;

    /**
     * @brief 防止无效编号或编号耗尽产生错误绑定。
     * @param scope 已存在的词法作用域编号。
     * @param context 已存在的语法上下文编号。
     */
    void validate(ScopeId scope, ContextId context) const
    {
        if (scope >= scopes.size() || context >= contexts.size())
            throw std::out_of_range("非法宏卫生作用域或上下文");
    }
};

/**
 * @brief 初始化根作用域和根语法上下文。
 */
HygieneResolver::HygieneResolver() : impl_(std::make_unique<Impl>()) {}

/**
 * @brief 销毁本次编译的卫生绑定状态。
 */
HygieneResolver::~HygieneResolver() = default;

/**
 * @brief 新建词法子作用域，不增加任何运行时作用域。
 * @param parent 已存在的父作用域。
 * @return 新词法作用域编号。
 */
ScopeId HygieneResolver::createScope(ScopeId parent)
{
    impl_->validate(parent, 0);
    if (impl_->scopes.size() >= Impl::noScope) throw std::length_error("宏词法作用域数量超过上限");
    const auto id = static_cast<ScopeId>(impl_->scopes.size());
    impl_->scopes.push_back({ parent, {} });
    return id;
}

/**
 * @brief 创建捕获定义处环境的展开上下文。
 * @param definitionScope 宏定义所在的作用域。
 * @param definitionContext 宏定义片段的来源上下文。
 * @return 新展开上下文编号。
 */
ContextId HygieneResolver::createExpansion(ScopeId definitionScope, ContextId definitionContext)
{
    impl_->validate(definitionScope, definitionContext);
    if (impl_->contexts.size() >= std::numeric_limits<ContextId>::max()) throw std::length_error("宏展开上下文数量超过上限");
    const auto id = static_cast<ContextId>(impl_->contexts.size());
    impl_->contexts.push_back({ definitionScope, definitionContext });
    return id;
}

/**
 * @brief 按原始名字和上下文登记声明，拒绝同作用域的重复绑定。
 * @param name 原始源码名字。
 * @param scope 词法作用域。
 * @param context 名字的语法上下文。
 * @return 绑定身份，重复声明返回零。
 */
BindingId HygieneResolver::declare(std::string_view name, ScopeId scope, ContextId context)
{
    impl_->validate(scope, context);
    auto &bindings = impl_->scopes[scope].bindings;
    const auto key = std::make_pair(context, std::string(name));
    if (bindings.find(key) != bindings.end()) return 0;
    if (impl_->nextBinding == 0) throw std::length_error("宏名字绑定数量超过上限");
    const auto id = impl_->nextBinding++;
    bindings.emplace(key, id);
    return id;
}

/**
 * @brief 查找同来源的词法绑定，宏自由名字只回退到定义环境。
 * @param name 待解析名字。
 * @param scope 实际使用位置的词法作用域。
 * @param context 该片段保留的语法上下文。
 * @return 精确绑定身份，未找到返回零。
 */
BindingId HygieneResolver::resolve(std::string_view name, ScopeId scope, ContextId context) const
{
    impl_->validate(scope, context);
    const std::string text(name);
    for (;;)
    {
        const auto key = std::make_pair(context, text);
        for (auto lexical = scope; lexical != Impl::noScope; lexical = impl_->scopes[lexical].parent)
        {
            const auto &bindings = impl_->scopes[lexical].bindings;
            const auto found = bindings.find(key);
            if (found != bindings.end()) return found->second;
        }
        if (context == 0) return 0;
        const auto &origin = impl_->contexts[context];
        scope = origin.definitionScope;
        context = origin.definitionContext;
    }
}
}  // namespace chali_macro
