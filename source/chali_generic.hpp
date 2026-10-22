#include "allocator/chali_allocator.hpp"
#ifndef CHALI_GENERIC_HPP
#define CHALI_GENERIC_HPP

#include "preprocess.hpp"
#include "chali_generic_name.hpp"

namespace chali_generic
{

/**
 * @brief 泛型形参：类型约束或编译期整数，以及可选默认值。
 */
struct Parameter
{
    Memory::String name;
    Memory::String constraint;
    Memory::String fallback;
    bool hasDefault = false;
    bool constant = false;        ///< 是否为编译期整数参数。
    Memory::String constantType;  ///< 整数参数的声明类型。
};

/**
 * @brief 保留完整声明和来源位置的单态化模板。
 */
struct Generic
{
    Memory::String name;
    Memory::String module;
    Memory::String sourceName;  ///< 模块内原始名称，用于隔离不同模块的同名模板。
    Memory::Vector<Parameter> params;
    Memory::String declaration;
    std::size_t nameOffset = 0;
    preprocess::LineOrigin origin;
    bool intrinsic = false;
    Memory::String requiredQualifier;
    Memory::String derivation;  ///< 保留到具体实例的 #derive 指令，不交给文本宏做语义检查。
    Memory::String operatorSwitches;  ///< 实例声明前的编译器指令 `#No<接口>Replace` / `#NoRight<接口>`，必须排在 `#derive` 之前。
};
/**
 * @brief 可用于整数泛型实参的模块常量原文；求值时仍拒绝运行时初始化式。
 */
extern Memory::Map<Memory::String, Memory::String> constantArguments;

/**
 * @brief 判断字符能否作为类型或模板标识符的首字符。
 * @param value 待检查的字符。
 * @return 字母或下划线返回 true。
 */
bool identStart(char value);

/**
 * @brief 判断字符能否继续组成类型或模板标识符。
 * @param value 待检查的字符。
 * @return 字母、数字或下划线返回 true。
 */
bool identPart(char value);

/**
 * @brief 移除文本两端的空白并保留内部文本。
 * @param value 原始文本。
 * @return 去除首尾空白的文本；全为空白时返回空串。
 */
Memory::String trim(const Memory::String &value);

/**
 * @brief 推进扫描位置至下一个非空白字符。
 * @param text 待扫描的源码。
 * @param at 输入起点并接收跳过空白后的位置。
 */
void skipSpace(const Memory::String &text, std::size_t &at);
/**
 * @brief 收集模块层整数常量，保留限定名与无歧义的裸名。
 * @param source 预处理后的完整源码。
 */
void collectConstantArguments(const Memory::String &source);

/**
 * @brief 检查当前位置是否为一个完整的源码单词。
 * @param text 待扫描的源码。
 * @param at 候选单词起点。
 * @param word 需要匹配的完整单词。
 * @return 拼写匹配且两端不是标识符字符时返回 true。
 */
bool wordAt(const Memory::String &text, std::size_t at, const Memory::String &word);

/**
 * @brief 跳过注释和字面量，查找成对分隔符的结束位置。
 * @param text 完整源码。
 * @param open 起始分隔符的位置。
 * @param left 左分隔符。
 * @param right 右分隔符。
 * @param close 成功时接收匹配的右分隔符位置。
 * @return 嵌套分隔符完整闭合时返回 true。
 */
bool balanced(const Memory::String &text, std::size_t open, char left, char right,
              std::size_t &close);

/**
 * @brief 仅在最外层逗号处分割泛型形参或实参。
 * @param text 可能包含嵌套类型和括号的参数文本。
 * @return 保留嵌套结构的参数列表；空输入返回空列表。
 */
Memory::Vector<Memory::String> splitTop(const Memory::String &text);

/**
 * @brief 解析泛型形参的约束、常量类型和默认实参。
 * @param text 尖括号内部的形参文本。
 * @param result 追加解析成功的形参。
 * @param error 失败时接收重复、格式或默认值顺序诊断。
 * @return 所有形参符合泛型声明规则时返回 true。
 */
bool parseParameters(const Memory::String &text, Memory::Vector<Parameter> &result,
                     Memory::String &error);

/**
 * @brief 将源码偏移换算为从零开始的行下标。
 * @param text 完整源码。
 * @param offset 需要定位的字节偏移。
 * @return 该偏移之前的换行数量。
 */
std::size_t lineOf(const Memory::String &text, std::size_t offset);

/**
 * @brief 暂存泛型类型前的派生与运算符开关指令，在产生具体实例时恢复原文。
 * @param source 待单态化源码；移走的指令以空白占位保持行号。
 * @param directives 输出模板名到派生指令文本的映射。
 * @param switches 输出模板名到 `#No<接口>Replace` / `#NoRight<接口>` 指令文本的映射。
 * @param error 失败时接收非法派生请求的诊断。
 * @return 指令扫描成功返回 true，不执行字段或接口语义检查。
 */
bool collectGenericDerives(Memory::String &source, Memory::Map<Memory::String, Memory::String> &directives,
                           Memory::Map<Memory::String, Memory::String> &switches, Memory::String &error);

/**
 * @brief 归一化顶层声明中名称后置的泛型参数，正文仍由生成解析器处理。
 * @param source 原地规范化的源码，保留行数以映射诊断。
 */
void normalizeGenericSyntax(Memory::String &source);

/**
 * @brief 标记标准容器类型实参的标点，保留普通标识符与比较运算符的词法角色。
 * @param source 已完成单态化的源码；仅插入内部标点，不改变换行。
 * @param typeFragment 是否已确定为类型片段，此时不依赖类型名是否已声明或首字母大写。
 * @return 无。
 */
void markContainerTypeArguments(Memory::String &source, bool typeFragment = false);

/**
 * @brief 提取泛型声明并保留原始行映射。
 * @param source 读取并移除模板声明的完整源码。
 * @param lines 原始源码的逐行来源。
 * @param generics 接收按限定名索引的模板。
 * @param error 失败时接收模板声明诊断。
 * @return 所有模板成功提取时返回 true。
 */
bool collect(Memory::String &source, const Memory::Vector<preprocess::LineOrigin> &lines,
             Memory::Map<Memory::String, Generic> &generics, Memory::String &error);

/**
 * @brief 生成具体泛型接口名称，供派生请求与普通接口约束使用相同身份。
 * @param name 模板名称。
 * @param args 实际类型参数。
 * @return 带稳定类型指纹的实例名。
 */
Memory::String mangle(const Memory::String &name, const Memory::Vector<Memory::String> &args);

/**
 * @brief 收集显式及请求派生的接口身份，排除字符串和注释中的伪声明。
 * @param source 已规范化的声明与普通源码。
 * @param implemented 输出具体类型到接口身份集合的映射。
 */
void collectInterfaces(const Memory::String &source,
                       Memory::Map<Memory::String, Memory::Set<Memory::String>> &implemented);

/**
 * @brief 生成长度受控的实例名，完整类型拼写参与散列以区分标点不同的类型。
 * @param name 模板名称。
 * @param args 已归一化的实际类型参数。
 * @return 保留可读前缀和稳定类型指纹的实例名称。
 */
Memory::String mangle(const Memory::String &name, const Memory::Vector<Memory::String> &args);

/**
 * @brief 用户声明覆盖的核心公开别名，供泛型实参归一使用。
 */
extern Memory::Set<Memory::String> coreAliasShadows;
/**
 * @brief 源码具名类型的限定身份，供唯一导入名称与限定名称归一使用。
 */
extern Memory::Map<Memory::String, Memory::Set<Memory::String>> nominalTypeNames;
/**
 * @brief 泛型具名类型的模块，用于归一生成实例的限定名称。
 */
extern Memory::Map<Memory::String, Memory::String> genericTypeModules;

/**
 * @brief 收集用户具名类型，用于保留与核心别名同名的泛型实参。
 * @param source 完整预处理源码。
 */
void collectCoreAliasShadows(const Memory::String &source);

/**
 * @brief 归一化类型拼写并保留被用户声明遮蔽的内建别名。
 * @param value 待比较或生成实例的类型拼写。
 * @return 统一空白、标准库限定名与泛型符号后的类型文本。
 */
Memory::String canonicalType(Memory::String value);

/**
 * @brief 单态化实例的实际参数、生成名称与已展开接口约束。
 */
struct Expansion
{
    const Generic *generic = nullptr;
    Memory::Vector<Memory::String> args;
    Memory::String name;
    Memory::Vector<Memory::String> constraints;
};

/**
 * @brief 一次编译保留的模板与特化记录，供语义分析按实参补充实例。
 */
struct Session
{
    Memory::Map<Memory::String, Generic> generics;
    Memory::Map<Memory::String, Memory::Set<Memory::String>> implemented;
    Memory::Map<Memory::String, Expansion> made;
    Memory::Map<Memory::String, Memory::Vector<Memory::String>> intrinsicArguments;
    Memory::Vector<Memory::String> pending;
    std::size_t emitted = 0;
};
extern Session session;

/**
 * @brief 按标识符替换类型参数，保留字符串、字符和注释的原始内容。
 * @param source 待替换的声明或类型表达式。
 * @param values 类型参数与实际类型的映射。
 * @param nameOffset 可选的模板声明名称位置。
 * @param name 模板声明的实例名称。
 * @return 完成替换的文本。
 */
Memory::String substituteIdentifiers(const Memory::String &source,
                                     const Memory::Map<Memory::String, Memory::String> &values,
                                     std::size_t nameOffset = Memory::String::npos, const Memory::String &name = {});

/**
 * @brief 以实际类型参数生成声明，并保留标准可选值的引用访问器语义。
 * @param expansion 模板及其完整类型实参。
 * @return 已替换参数和声明名称的源码。
 */
Memory::String substitute(const Expansion &expansion);

/**
 * @brief 检查具体类型表达式的嵌套深度，避免归一化默认类型时耗尽调用栈。
 * @param text 类型表达式。
 * @param error 超限时接收诊断。
 * @return 嵌套深度未达到上限时返回 true。
 */
bool checkTypeNesting(const Memory::String &text, Memory::String &error);

/**
 * @brief 按已知模板改写泛型使用点并排队等待展开。
 * @param text 需要单态化的声明或表达式文本。
 * @param generics 当前可见的泛型声明。
 * @param implemented 具体类型已实现的接口集合。
 * @param made 复用并补充已创建的模板实例。
 * @param pending 按发现顺序追加待展开的实例名。
 * @param error 失败时接收实参、约束或递归深度诊断。
 * @param depth 当前默认类型展开的递归层数。
 * @return 所有泛型使用点可完成实例化时返回 true。
 */
bool rewrite(Memory::String &text, const Memory::Map<Memory::String, Generic> &generics,
             const Memory::Map<Memory::String, Memory::Set<Memory::String>> &implemented,
             Memory::Map<Memory::String, Expansion> &made, Memory::Vector<Memory::String> &pending,
             Memory::String &error, std::size_t depth = 0);

/**
 * @brief 在全部嵌套实例展开后检查约束，包含泛型实例实现的接口及传递继承。
 * @param made 本次编译的实例记录。
 * @param implemented 普通声明与生成声明实现的接口。
 * @param error 失败时接收具体实例的诊断。
 * @return 所有具体类型实参满足约束时返回 true。
 */
bool validateConstraints(const Memory::Map<Memory::String, Expansion> &made,
                         const Memory::Map<Memory::String, Memory::Set<Memory::String>> &implemented,
                         Memory::String &error);

/**
 * @brief 收集泛型模板、展开已知实例并标记剩余类型实参。
 * @param result 读取并替换源码及对应来源映射。
 * @param error 失败时接收泛型展开诊断。
 * @return 模板收集、约束检查和单态化成功时返回 true。
 */
bool run(preprocess::Result &result, Memory::String &error);

/**
 * @brief 将语义推导出的具体类型展开成普通声明；错误由调用方报告。
 * @param name 模板名称。
 * @param args 推导出的完整类型实参。
 * @param generated 接收本次新增声明。
 * @param error 失败时接收诊断。
 * @return 类型实参和约束有效且展开成功时返回 true。
 */
bool instantiate(const Memory::String &name, const Memory::Vector<Memory::String> &args,
                 Memory::String &generated, Memory::String &error);

}  // namespace chali_generic

#endif
