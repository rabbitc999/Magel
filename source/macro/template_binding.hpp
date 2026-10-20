#ifndef MAGEL_MACRO_TEMPLATE_BINDING_HPP
#define MAGEL_MACRO_TEMPLATE_BINDING_HPP
#include "ast_substitution.hpp"
#include <set>

namespace magel_macro
{
/**
 * @brief 保存已绑定模板及形参身份，供调用展开按身份替换。
 */
struct BoundTemplate
{
    int root = -1;
    std::map<std::string, BindingId> parameters;
};

/**
 * @brief 按词法作用域绑定宏形参和局部声明，保留原模板。
 */
class TemplateBinding
{
public:
    /**
     * @brief 关联语法节点池及本次编译的卫生解析器。
     * @param nodes 语法节点池。
     * @param resolver 维护唯一绑定身份的解析器。
     */
    TemplateBinding(Memory::Deque<MNode> &nodes, HygieneResolver &resolver);
    /**
     * @brief 克隆并绑定一个宏定义，将未被局部声明遮蔽的形参引用转为占位。
     * @param definition 宏定义节点。
     * @param definitionScope 定义所在的词法作用域。
     * @param result 成功时接收模板根及形参身份，失败时清空。
     * @param error 失败原因，成功时清空。
     * @return 绑定成功返回真；失败回滚本次新建节点。
     */
    bool bind(int definition, ScopeId definitionScope, BoundTemplate &result, std::string &error);

private:
    /**
     * @brief 按节点结构与声明顺序绑定引用，不改写成员名称。
     * @param id 当前节点，负值表示省略。
     * @param scope 当前词法作用域。
     * @param depth 当前遍历深度。
     */
    void visit(int id, ScopeId scope, std::size_t depth);
    /**
     * @brief 为具名声明分配绑定身份并诊断重复声明。
     * @param id 声明节点。
     * @param scope 声明所在作用域。
     */
    void declare(int id, ScopeId scope);
    Memory::Deque<MNode> &nodes_;
    HygieneResolver &resolver_;
    std::set<BindingId> parameters_;
};
}  // namespace magel_macro
#endif
