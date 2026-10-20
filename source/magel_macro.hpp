#include "allocator/magel_allocator.hpp"
#ifndef MAGEL_MACRO_HPP
#define MAGEL_MACRO_HPP

#include "preprocess.hpp"
#include "magel_macro_controls.hpp"

namespace magel_macro
{

/**
 * @brief 定义中一组独立的文本参数，保留括号种类与局部变长尾参。
 */
struct ParameterGroup
{
    char opening = '(';
    Memory::Vector<Memory::String> params;
    Memory::String rest;
};

/**
 * @brief 保存宏的原始文本、调用分组和既有表达式保护规则。
 */
struct Macro
{
    Memory::Vector<ParameterGroup> groups;
    Memory::Vector<Memory::String> params;
    /**
     * 使用 `(参数名)` 标注或处于 expr 宏中的形参：每次出现在宏体里都替换成 `(实参)`，给表达式宏当护栏。
     */
    Memory::Set<Memory::String> parenthesized;
    bool functionLike = false;
    bool variadic = false;
    Memory::String variadicParam;
    /**
     * 变长尾参使用 `...(rest)` 标注或处于 expr 宏中：替换时给**每个**尾参元素各包一层括号
     * （`rest` → `(a), (b), (c)`），`$for v in rest` 的循环变量也跟着变成 `(a)`。
     */
    bool parenthesizeVariadic = false;
    /**
     * `$macro expr NAME(…)`：整段展开及所有实参加括号。
     */
    bool parenthesizeResult = false;
    Memory::String body;
    Memory::String debugName;
};

/**
 * @brief 编码内部宏来源标记；解析前移除。
 * @param origin 原始位置。
 * @param column 原文中从零计数的列偏移。
 * @return 标记文本。
 */
Memory::String debugMarker(const preprocess::LineOrigin &origin, std::size_t column = 0);

/**
 * @brief 清除拼接边界上的内部标记，保持 token 相邻。
 * @param text 展开文本。
 * @param all 是否清除所有标记。
 * @return 语义不变的文本。
 */
Memory::String cleanPasteMarkers(const Memory::String &text, bool all = false);

/**
 * @brief 判断字符能否开始一个文本宏标识符。
 * @param ch 待检查的字符。
 * @return 字母或下划线返回 true。
 */
bool identStart(char ch);

/**
 * @brief 判断字符能否继续组成文本宏标识符。
 * @param ch 待检查的字符。
 * @return 字母、数字或下划线返回 true。
 */
bool identPart(char ch);

/**
 * @brief 跳过当前位置开始的宏文本空白。
 * @param s 待扫描的宏文本。
 * @param i 输入并更新扫描位置。
 */
void spaces(const Memory::String &s, std::size_t &i);

/**
 * @brief 移除文本两端的空白并保留内部文本。
 * @param text 原始文本。
 * @return 去除首尾空白的文本；全为空白时返回空串。
 */
Memory::String trim(const Memory::String &text);

/**
 * @brief 将原始文本编码为带引号且正确转义的字符串字面量。
 * @param text 需要字符串化的原始文本。
 * @return 保持原始字节内容的语言字符串字面量。
 */
Memory::String stringify(const Memory::String &text);

/**
 * @brief 判断文本是不是**单个**标识符（调用方先去掉首尾空白）。
 *
 * `$concat` 的每个实参都要满足它：`$` 后缀的拼接只拼标识符，不拼任意文本，
 * 所以实参类型在宏展开阶段就能查出来。
 *
 * @param text 待检查文本（应已去掉首尾空白）。
 * @return 是单个标识符返回 true。
 */
bool identifierText(const Memory::String &text);

/**
 * @brief 根据展开行映射构造宏诊断的源位置。
 * @param result 保存源码及逐行来源的预处理结果。
 * @param line 从零开始的展开后行下标。
 * @return 包含原文件和行号的诊断前缀。
 */
Memory::String origin(const preprocess::Result &result, std::size_t line);

/**
 * @brief 解析 `$macro` / `$replace` 头的三种写法，并填好形态。
 *
 * | 写法 | 形态 | 调用 |
 * | --- | --- | --- |
 * | `$macro NAME()` | 对象宏 | 裸写 `NAME`（**不带**括号） |
 * | `$macro NAME(a, b)` | 定参宏 | `NAME(x, y)` |
 * | `$macro NAME(x, ...rest)` | 变长宏 | `NAME(x, y, z)` |
 * | `$macro NAME(x, ...(rest))` | 变长宏（元素加括号） | `NAME(x, y, z)` → `x, (y), (z)` |
 *
 * `$replace NAME(x, y) 宏体` 是**单行宏**：宏体就是头这同一行剩下的内容，没有 `$end_macro`，
 * 无形参的对象宏可省括号（`$replace NAME 宏体`）；行内规则（`expr` / 参数括号 / `$stringify` /
 * `$concat` / `$line`…）都支持，但装不下 `$for` 这类块。
 *
 * 表达式括号规则：`$macro expr ADD(a, b)` 为结果和全部实参加括号；
 * expr 宏允许 `(a)` 冗余标注，与 `a` 完全等价。普通宏使用 `(a)` 显式标注，
 * `...(rest)` 为每个尾参元素分别加括号，`$for` 循环变量沿用该标注。
 * 定义端标注不改变调用形式：`$ADD(x + 1, y * 2)`。
 *
 * 拼接与字符串化推荐写结构化形式：`$concat(a, b)` 只拼标识符，结果是标识符，不重新参与
 * 宏展开；`$stringify(expr)` 捕获实参原文，结果是字符串字面量。两者都用**原始**实参、
 * 不加括号。
 *
 * `$macro` 的括号不能省：括号里空着表示对象宏，有形参表示函数式宏。
 * `$replace` 的对象宏可以省括号。
 *
 * @param line 整行源码。
 * @param name 输出：宏名。
 * @param macro 输出：形态（含形参、变长尾参）。
 * @param problem 输出：失败原因（成功时为空串）。
 * @param inlineBody 非空时走单行宏（`$replace`）：头这一行剩下的内容当宏体收下；
 *        为空指针时走多行宏（`$macro`）：头这一行后面不能有多余内容。
 * @return 成功返回 true。
 */

/**
 * @brief 完成形参表解析，或收取单行替换宏的宏体。
 * @param line 声明源码。
 * @param i 参数表后的位置。
 * @param problem 输出声明尾部的错误。
 * @param inlineBody 非空时接收剩余文本，否则禁止多余内容。
 * @retval 声明尾部有效返回 true。
 */
bool headerTail(const Memory::String &line, std::size_t i, Memory::String &problem,
                Memory::String *inlineBody);
/**
 * @brief 保持单圆括号宏的对象形态、表达式保护和三点尾参解析。
 * @param line 声明源码。
 * @param name 输出宏名。
 * @param macro 输出原有参数形态。
 * @param problem 输出声明错误。
 * @param inlineBody 非空时接收单行宏体。
 * @param keyword 当前声明关键字。
 * @retval 解析成功返回 true。
 */
bool legacyHeader(const Memory::String &line, Memory::String &name, Macro &macro,
                  Memory::String &problem, Memory::String *inlineBody = nullptr,
                  const Memory::String &keyword = Memory::String("$macro"));

/**
 * @brief 返回文本分组左括号对应的右括号。
 * @param ch 待检查字符。
 * @retval 右括号；非左括号返回零。
 */
char groupClose(char ch);

/**
 * @brief 按字符栈切分一组文本，不解释运算符或类型。
 * @param text 原始文本。
 * @param open 左括号位置。
 * @param parts 输出当前层的逗号分隔片段。
 * @param end 输出右括号后一位置。
 * @param allowIncomplete 是否允许编辑器读取尚未闭合的末尾分组；编译默认关闭。
 * @param closed 可选输出，指出分组是否已完整闭合。
 * @retval 成功返回 true；缺少或交叉闭合括号返回 false。
 * @note 字符串和注释中的括号不参与分组，连续右尖括号逐字符处理。
 */
bool groupArguments(const Memory::String &text, std::size_t open,
                    Memory::Vector<Memory::String> &parts, std::size_t &end,
                    bool allowIncomplete = false, bool *closed = nullptr);

/**
 * @brief 在原有宏声明解析上扩展有序括号组与每组独立的双点尾参。
 * @param line 声明行。
 * @param name 输出宏名。
 * @param macro 输出宏参数定义。
 * @param problem 输出分组或声明错误。
 * @param inlineBody 单行替换宏的输出宏体。
 * @param keyword 当前声明关键字。
 * @retval 声明有效返回 true。
 */
bool header(const Memory::String &line, Memory::String &name, Macro &macro,
            Memory::String &problem, Memory::String *inlineBody = nullptr,
            const Memory::String &keyword = Memory::String("$macro"));

/**
 * @brief 判断一行文本是否以指定宏指令开头。
 * @param line 完整的源代码行。
 * @param word 需要匹配的宏指令关键字。
 * @return 跳过空白后指令名称及边界均匹配时返回 true。
 */
bool directive(const Memory::String &line, const Memory::String &word);

/**
 * @brief 比较两份宏定义的文本调用形态，忽略形参名称。
 * @param left 已有宏。
 * @param right 新宏。
 * @retval 括号顺序、每组元数及尾参形态相同返回 true。
 */
bool sameShape(const Macro &left, const Macro &right);

/**
 * @brief 找出当前行开始的结构化宏调用所占源码范围。
 * @param text 完整源码。
 * @param start 当前行起点。
 * @param stop 当前行终点，不含换行。
 * @param macros 当前可见宏。
 * @retval 返回应一起送入文本展开的末尾位置。
 * @note 只扫描字符、注释和引号，不调用词法器或解析器。
 */
std::size_t groupedLineEnd(const Memory::String &text, std::size_t start, std::size_t stop,
                           const Memory::Map<Memory::String, Memory::Vector<Macro>> &macros);

/**
 * @brief 宏名不能占用的内部 DSL 关键字（`$macro` / `$replace` / `$for` / `$concat` …）。
 *
 * 这些词在 `$` 指令里有固定含义，拿来当宏名会让 `$for(…)` 这类调用点无法区分，所以
 * 定义期直接拒绝；`$line` / `$file` / `$module` 与 `$concat` / `$stringify` 也一并占住。
 */
bool macroDslKeyword(const Memory::String &name);

/**
 * 找单行宏体里出现的块指令（`$for` 及它的 `$end_for`，外加 `$end_macro`）。
 * `$replace` 的宏体只有一行，装不下这些块；找到就报错让用户改回 `$macro`。
 * 字符串和注释里的字面 `$for` 不算。
 */
bool findBlockDirective(const Memory::String &text, Memory::String &found);

/**
 * @brief 使用同一字符分组器读取原有圆括号调用与内建文本操作。
 * @param s 调用文本。
 * @param open 左括号位置。
 * @param args 输出当前层的原始参数。
 * @param end 输出右括号后的偏移。
 * @retval 分组成功返回 true。
 */
bool arguments(const Memory::String &s, std::size_t open,
               Memory::Vector<Memory::String> &args, std::size_t &end);

/**
 * @brief 展开 `$concat(…)` / `$stringify(…)`。
 *
 * 两者和 `$line` / `$file` / `$module` 一样带 `$` 前缀，普通代码与宏体内都能用；
 * 旧的 `##` 拼接与 `#形参` 字符串化已在 0.16.0 删除，只留这两个结构化形式。
 *
 * - `$concat(A, B)`：两个实参各解析成一段文本（宏体内是形参名就取调用点的实参文本，
 *   否则就是写下的字面标识符），二者都必须整体是一个标识符；结果是拼出来的标识符，
 *   **不重新参与宏展开**。
 * - `$stringify(X)`：X 在宏体内是形参名时取调用点的**原始**实参文本，否则取写下的原文；
 *   结果是一个字符串字面量（内部来源标记不会进字符串）。
 *
 * 检查放在展开阶段：写错的实参在宏被调用时立刻报错，不需要宏体再往下走。
 *
 * @param text 正在扫描的文本（普通代码行或宏体）。
 * @param at `$` 的位置。
 * @param keyword `"$concat"` 或 `"$stringify"`。
 * @param values 宏形参 → 展开后的实参文本；宏体外为空指针。
 * @param rawValues 宏形参 → 调用点的原始实参文本；宏体外为空指针。
 * @param output 输出：结果追加到这里。
 * @param end 输出：`)` 之后的位置。
 * @param problem 输出：失败原因（成功时为空串）。
 * @return 成功返回 true。
 */
Memory::String substituteMacroLocals(const Memory::String &text,
                                     const Memory::Map<Memory::String, Memory::String> &values);
/**
 * @brief 展开宏内建字符串化、拼接或类型构造指令。
 * @param text 待展开的宏文本。
 * @param at 指令在文本中的起始位置。
 * @param keyword 当前内建指令的名称。
 * @param values 已保护的形参绑定，可为空。
 * @param rawValues 保留原始文本的形参绑定，可为空。
 * @param output 追加生成的源码文本。
 * @param end 成功时接收指令结束位置。
 * @param problem 失败时接收指令诊断。
 * @return 当前内建指令解析和展开成功时返回 true。
 */
bool expandIntrinsic(const Memory::String &text, std::size_t at,
                     const Memory::String &keyword,
                     const Memory::Map<Memory::String, Memory::String> *values,
                     const Memory::Map<Memory::String, Memory::String> *rawValues,
                     Memory::String &output, std::size_t &end,
                     Memory::String &problem);

/**
 * @brief 把宏体里的 `$for NAME in <尾参名>` … `$end_for` 按尾参逐个重复放开。
 *
 * 只能遍历**变长尾参**（`...rest`）：一次一个实参，用**展开后**的实参文本（和 `rest` 一致），
 * 循环变量按标识符整体替换，每次迭代自成一行。体里还能再嵌 `$for` —— 从最内层开始逐层展开。
 *
 * @param body 宏体（原地替换）。
 * @param variadicParam 变长尾参名；宏不是变长宏时为空串（此时任何 `$for` 都报错）。
 * @param tail 展开后的尾部实参，按顺序。
 * @param parenthesizeElement 尾参使用括号标注或处于 expr 宏中时，循环变量每次替换成 `(实参)`。
 * @param error 出错信息。
 * @return 成功返回 true。
 */
bool expandFors(Memory::String &body, const Memory::String &variadicParam,
                const Memory::Vector<Memory::String> &tail,
                bool parenthesizeElement, Memory::String &error);

/**
 * @brief 一次展开发生的位置，供 `$line` / `$file` 取值。
 *
 * 宏体里的 `$line` 取的是**调用点**那一行（和 C 的 `__LINE__` 一个意思），不是宏体
 * 自己所在的行；所以这个上下文只描述"当前正在展开的输出行"，递归进宏体时原样传下去。
 */
struct Site
{
    /**
     * 输出行（0 基）。
     */
    std::size_t line = 0;

    /**
     * 行号映射的来源（可为空指针，此时按"行号与主文件一致"处理）。
     */
    const preprocess::Result *result = nullptr;

    /**
     * 源文件 → 模块名；为空指针时 `$module` 给空串。
     */
    const Memory::Map<Memory::String, Memory::String> *moduleByFile = nullptr;
};

/**
 * @brief `$line` 的取值：映射回源文件后的行号（1 基）。
 */
std::size_t siteLine(const Site &site);

/**
 * @brief `$file` 的取值：映射回源文件后的文件名；没有映射时为空串。
 */
Memory::String siteFile(const Site &site);

/**
 * @brief `$module` 的取值：当前源文件所属的模块名；查不到时为空串。
 */
Memory::String siteModule(const Site &site);

/**
 * @brief 把宏名净化成生成名里能安全当标识符用的一段：非 `[A-Za-z0-9_]` 一律换成 `_`。
 */
Memory::String sanitizeMacroName(const Memory::String &name);

/**
 * @brief 识别美元符号后的十进制整数编号，不把编号转换成宿主整数。
 * @param text 待扫描的原始文本。
 * @param start 美元符号的位置。
 * @return 合法编号后一位置；不是整数占位符时返回 start。
 */
std::size_t placeholderEnd(const Memory::String &text, std::size_t start);

/**
 * @brief 为任意长度的整数编号产生当前展开独有的合法标识符。
 * @param macroName 宏名称。
 * @param expandId 当前宏调用的唯一展开编号。
 * @param slot 十进制整数文本，允许符号和前导零。
 * @return 相同整数在一次展开内对应相同名字，不同展开互相隔离。
 * @note 不使用数值转换，因此编号不受 int64 或 uint64 上限限制。
 */
Memory::String gensymName(const Memory::String &macroName, std::size_t expandId, const Memory::String &slot);

/**
 * @brief 为原始调用片段记录文件、行及列，供展开后还原诊断位置。
 * @param source 当前调用所在文本。
 * @param begin 片段起点。
 * @param length 片段字节长度。
 * @param site 当前输入文本的来源。
 * @retval 带内部来源标记的原始片段。
 * @note 引号和块注释内部不插入标记，避免改变字面文本。
 */
Memory::String mappedPiece(const Memory::String &source, std::size_t begin, std::size_t length, const Site &site);

/**
 * @brief 按宏形态与当前来源递归展开一段源码。
 * @param source 需要展开的源码片段。
 * @param macros 按名称及分组形态索引的可见宏定义。
 * @param output 接收展开后的文本。
 * @param error 失败时接收包含来源的诊断。
 * @param stack 检测递归调用的宏名称栈。
 * @param blockComment 输入并更新跨片段块注释状态。
 * @param site 本次调用的位置与来源映射。
 * @param nextExpandId 输入并递增局部名称唯一编号。
 * @return 所有宏调用均成功展开时返回 true。
 */
bool expand(const Memory::String &source,
            const Memory::Map<Memory::String, Memory::Vector<Macro>> &macros,
            Memory::String &output, Memory::String &error,
            Memory::Vector<Memory::String> &stack, bool &blockComment,
            const Site &site, std::size_t &nextExpandId);

/**
 * @brief 提取文本宏定义并展开源码，同时维护逐行来源。
 * @param result 读取并替换源码及其逐行来源。
 * @param error 失败时接收原始宏诊断。
 * @param moduleByFile 可选的源文件到模块名映射，供模块内建指令使用。
 * @return 宏定义、调用和控制块均有效时返回 true。
 */
bool run(preprocess::Result &result, Memory::String &error,
         const Memory::Map<Memory::String, Memory::String> *moduleByFile = nullptr);

}  // namespace magel_macro

#endif
