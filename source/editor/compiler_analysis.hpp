#ifndef CHALI_EDITOR_COMPILER_ANALYSIS_HPP
#define CHALI_EDITOR_COMPILER_ANALYSIS_HPP
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace chali_editor
{
/**
 * @brief 对原始源码声明给出的已完成语义推导结果。
 */
struct TypeHint
{
    std::string file, name, type;
    std::size_t line = 0, column = 0;
};
/**
 * @brief 一个成功实例化的宏及其结构化代码预览。
 */
struct MacroExpansion
{
    std::string file, name, tree;
    std::size_t line = 0, column = 0;
};
/**
 * @brief 编码一个 JSON 字符串，保持非 ASCII 源码字节。
 * @param text 需要编码的 UTF-8 文本。
 * @return 带双引号的合法 JSON 字符串。
 */
std::string jsonString(std::string_view text);
/**
 * @brief 汇集编辑器请求的真实编译结果，独立于 VM 的执行内存和对象。
 */
class CompilerAnalysis
{
public:
    /**
     * @brief 开始新的编辑器分析，普通编译可完全禁用收集。
     * @param enabled 是否需要生成分析数据。
     */
    void begin(bool enabled);
    /**
     * @brief 清除上轮语义重放中的类型结果，保留成功宏展开的来源记录。
     */
    void resetPass();
    /**
     * @brief 查询是否需要收集编译数据。
     * @return 编辑器已请求数据时返回真。
     */
    bool enabled() const;
    /**
     * @brief 保存一个原始声明的完整静态类型。
     * @param hint 带源码位置和用户名称的类型结果。
     */
    void addType(TypeHint hint);
    /**
     * @brief 保存去重后的卫生展开预览，限制编辑期输出规模。
     * @param expansion 已序列化的宏模板 AST 及调用位置。
     */
    void addMacro(MacroExpansion expansion);
    /**
     * @brief 将本次编译数据写入请求方指定的文件。
     * @param path UTF-8 文件路径。
     * @param complete 编译是否已通过全部语义检查。
     * @param error 接收写入错误。
     * @return 文件完整写出时返回真。
     */
    bool write(const std::string &path, bool complete, std::string &error) const;

private:
    bool enabled_ = false;
    std::size_t previewBytes_ = 0;
    std::vector<TypeHint> types_;
    std::vector<MacroExpansion> macros_;
};
/**
 * @brief 当前编译进程的编辑器分析收集器，不保存运行时地址或闭包环境。
 */
extern CompilerAnalysis compilerAnalysis;
}  // namespace chali_editor
#endif
