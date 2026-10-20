#ifndef MAGEL_MACRO_AST_SUBSTITUTION_HPP
#define MAGEL_MACRO_AST_SUBSTITUTION_HPP
#include "allocator/magel_allocator.hpp"
#include "hygiene.hpp"
#include <map>
#include <string>

struct MNode;

namespace magel_macro
{
/**
 * @brief 根据已绑定的参数占位克隆 AST，保留表达式结构及调用者来源。
 * @note 不插入括号、闭包、额外作用域或实参缓存，不修改原模板及实参节点。
 */
class AstSubstitution
{
public:
    /**
     * @brief 关联本次编译的现有 AST 节点池。
     * @param nodes 生成解析器与语义分析共用的稳定节点池。
     * @param nodeBudget 一次实例化允许新建的最大节点数。
     */
    explicit AstSubstitution(Memory::Deque<MNode> &nodes, std::size_t nodeBudget = 1000000);
    /**
     * @brief 用实参 AST 实例化模板，参数使用绑定身份而不是文字名字匹配。
     * @param root 模板根节点编号。
     * @param arguments 形参绑定身份与调用实参根节点的映射。
     * @param context 本次宏展开的卫生上下文。
     * @param error 接收错误，成功时清空。
     * @return 实例化根编号；失败返回负一并回滚新建节点。
     */
    int instantiate(int root, const std::map<BindingId, int> &arguments, ContextId context, std::string &error);

private:
    /**
     * @brief 克隆子树，模板占位可被替换，实参内部不再次执行形参替换。
     * @param id 待克隆节点编号。
     * @param substitute 是否允许替换模板占位。
     * @param depth 当前递归深度。
     * @return 新节点编号，失败抛出 runtime_error 由实例化入口转换诊断。
     */
    int clone(int id, bool substitute, std::size_t depth);
    Memory::Deque<MNode> &nodes_;
    std::size_t budget_;
    std::size_t start_ = 0;
    ContextId context_ = 0;
    const std::map<BindingId, int> *arguments_ = nullptr;
};
}  // namespace magel_macro
#endif
