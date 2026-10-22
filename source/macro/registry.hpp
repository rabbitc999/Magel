#ifndef CHALI_MACRO_REGISTRY_HPP
#define CHALI_MACRO_REGISTRY_HPP
#include "template_binding.hpp"
#include "../literals/string_escape.hpp"
#include <vector>
#include <set>
#include <functional>

namespace chali_macro
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
     * @param types 当前编译的类型池，无类型的独立测试可省略。
     */
    Registry(Memory::Deque<MNode> &nodes, HygieneResolver &resolver, Memory::Vector<std::shared_ptr<MType>> *types = nullptr);
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
    int instantiate(int call, std::string &error,
                    const std::function<int(int, int, int)> &resolveType = {},
                    const std::function<bool(int, const std::map<std::string, int> &, std::string &)> &applyTypes = {});
    /**
     * @brief 登记调用模块可见的直接导入模块。
     * @param from 导入声明所属模块。
     * @param target 被导入模块。
     */
    void addImport(const std::string &from, const std::string &target);

private:
    /**
     * @brief 将选中的编译期分支拼入模板，保留表达式尾项及原有作用域。
     * @param root 模板根节点。
     * @param depth 当前深度。
     * @param error 接收结构错误。
     * @return 归一化成功返回真。
     */
    bool normalizeTemplate(int root, std::size_t depth, std::string &error);
    /**
     * @brief 保存定义作用域、原始形参顺序和绑定后的不可变模板。
     */
    struct Definition
    {
        struct Group
        {
            int opening = '(';
            std::size_t begin = 0;
            std::size_t count = 0;
            bool variadic = false;
        };
        int declaration = -1;
        BoundTemplate bound;
        Memory::Vector<BindingId> parameters;
        Memory::Vector<std::string> names;
        std::vector<Group> groups;
        ScopeId scope = 0;
        ContextId context = 0;
        bool grouped = false;
        bool generic = false;
    };
    Memory::Deque<MNode> &nodes_;
    HygieneResolver &resolver_;
    RegexMatcher matcher_;
    std::map<std::string, std::set<std::string>> imports_;
    Memory::Vector<std::shared_ptr<MType>> *types_ = nullptr;
    std::size_t remainingNodes_ = 1000000;
    std::map<std::pair<std::string, std::string>, std::vector<Definition>> definitions_;
};
}  // namespace chali_macro
#endif
