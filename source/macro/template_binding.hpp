#ifndef CHALI_MACRO_TEMPLATE_BINDING_HPP
#define CHALI_MACRO_TEMPLATE_BINDING_HPP
#include "ast_substitution.hpp"
#include "regex_matcher.hpp"
#include <set>

namespace chali_macro
{
/**
 * @brief 保存已绑定模板及形参身份，供调用展开按身份替换。
 */
struct BoundTemplate
{
    int root = -1;
    std::map<std::string, BindingId> parameters;
    std::map<BindingId, int> iterations;
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
     * @param types 当前编译的类型池，供类型片段的卫生绑定使用。
     */
    TemplateBinding(Memory::Deque<MNode> &nodes, HygieneResolver &resolver, Memory::Vector<std::shared_ptr<MType>> *types = nullptr);
    /**
     * @brief 克隆并绑定一个宏定义，将未被局部声明遮蔽的形参引用转为占位。
     * @param definition 宏定义节点。
     * @param definitionScope 定义所在的词法作用域。
     * @param result 成功时接收模板根及形参身份，失败时清空。
     * @param error 失败原因，成功时清空。
     * @param inputs 按形参名提供的结构化实参；空时只做普通模板绑定。
     * @param matcher 本次编译的正则模式缓存。
     * @param bindBody 是否绑定宏体；登记签名时为假，避免检查未选中分支。
     * @return 绑定成功返回真；失败回滚本次新建节点。
     */
    bool bind(int definition, ScopeId definitionScope, BoundTemplate &result, std::string &error,
              const std::map<std::string, int> *inputs = nullptr, RegexMatcher *matcher = nullptr, bool bindBody = true);

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
    /**
     * @brief 为元组及枚举载荷模式建立局部声明身份。
     * @param id 模式或绑定节点。
     * @param scope 模式所在词法作用域。
     * @param depth 当前模式深度。
     */
    void bindPattern(int id, ScopeId scope, std::size_t depth);
    /**
     * @brief 按当前位置的词法绑定克隆类型树，标记类型片段形参。
     * @param type 原始类型树。
     * @param scope 当前词法作用域。
     * @param context 当前语法上下文。
     * @param depth 类型深度。
     * @return 带形参绑定的独立类型树。
     */
    std::shared_ptr<MType> bindType(const std::shared_ptr<MType> &type, ScopeId scope, ContextId context, std::size_t depth);
    Memory::Deque<MNode> &nodes_;
    HygieneResolver &resolver_;
    std::set<BindingId> parameters_;
    ScopeId parameterScope_ = 0;
    const std::map<std::string, int> *inputs_ = nullptr;
    RegexMatcher *matcher_ = nullptr;
    BoundTemplate *result_ = nullptr;
    std::map<std::string, int> iterationInputs_;
    Memory::Vector<std::shared_ptr<MType>> *types_ = nullptr;
};
}  // namespace chali_macro
#endif
