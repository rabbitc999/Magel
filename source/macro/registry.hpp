#ifndef MAGEL_MACRO_REGISTRY_HPP
#define MAGEL_MACRO_REGISTRY_HPP
#include "template_binding.hpp"

namespace magel_macro
{
/**
 * @brief 在单次编译中注册宏定义，按调用签名实例化已绑定 AST。
 * @note 返回完整模板节点，语句拼接、分支选择及结果类型推导由语义层处理。
 */
class Registry
{
public:
    /**
     * @brief 关联语法池及定义环境解析器。
     * @param nodes 编译器节点池。
     * @param resolver 当前编译的卫生解析器。
     */
    Registry(Memory::Deque<MNode> &nodes, HygieneResolver &resolver);
    /**
     * @brief 在定义模块内登记宏，绑定模板形参并拒绝重名。
     * @param definition 宏定义节点。
     * @param scope 定义的词法作用域。
     * @param error 接收诊断文本。
     * @return 成功返回真。
     */
    bool add(int definition, ScopeId scope, std::string &error);
    /**
     * @brief 按调用模块查找宏，校验参数组并以实参实例化模板。
     * @param call 宏调用节点。
     * @param error 接收诊断文本。
     * @return 新模板根节点；失败返回负一且不保留部分实例化节点。
     * @note 尚未解析导入可见性或推导宏泛型，泛型调用明确报错。
     */
    int instantiate(int call, std::string &error);

private:
    /**
     * @brief 在辅助名称降级接入前拒绝可能捕获调用环境的引用。
     * @param root 待检查的已绑定模板节点。
     * @param depth 当前检查深度。
     * @param error 接收未接入能力或结构错误。
     * @return 当前支持的引用均安全时返回真。
     */
    bool checkReferences(int root, std::size_t depth, std::string &error) const;
    /**
     * @brief 保存定义作用域、原始形参顺序和绑定后的不可变模板。
     */
    struct Definition
    {
        BoundTemplate bound;
        Memory::Vector<BindingId> parameters;
        ScopeId scope = 0;
        ContextId context = 0;
        bool grouped = false;
        bool generic = false;
    };
    Memory::Deque<MNode> &nodes_;
    HygieneResolver &resolver_;
    std::size_t remainingNodes_ = 1000000;
    std::map<std::pair<std::string, std::string>, Definition> definitions_;
};
}  // namespace magel_macro
#endif
