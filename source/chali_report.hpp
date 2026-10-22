#pragma once
/* chali_report.hpp —— 报错呈现（驱动 / 语言服务共用的一层），中英双语。
 *
 * 三件事：
 *   ① **Token 显示名**：解析器给的语法错误里写的是内部名（`SEMI` / `IMPL` /
 *      `VARIABLE`…），用户看不懂。这里给每个 Token 配一份「名字(写法)」
 *      （`分号(;)` / `semicolon(;)` / `セミコロン(;)`），打印前换掉。
 *      表与 `chali_token.gram` 一一对应；文法加了终结符要同步改这里。
 *      替换按**整词**做：只认全大写的标识符片段，且必须整段命中表，
 *      所以源码路径、类型名（`std::lang::List`）里的大小写都不会被误伤。
 *      这份是**名字表**不是**消息表**，所以就地双语、不进 `i18n/messages.tsv`
 *      （那边按整条中文消息做键，装不了逐词替换）。
 *   ② **语法错误骨架的双语重建**：`chali_parser.cpp` 是生成文件，文案写死中文，
 *      改不动。这里按中文骨架把「期望哪些 / 实际是什么」拆出来，再按当前语言
 *      重新拼一遍——英文是 `expected one of A / B, found C` 的语序，直接
 *      逐词替换中文串是翻不出来的。拆不动（碰上没见过的文案）就退回①。
 *   ③ **源码片段 + 波浪号**：定位到第几行第几列之后，回显那一行的原文，
 *      并按出错位置的**长度**画波浪号。画的时候按显示宽度算（中文占 2 列、
 *      制表符按 4 列制表位展开），否则列会歪。
 */
#include "allocator/chali_allocator.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace chali_report
{

/**
 * @brief 一个 Token 名在两种语言下的写法。
 */
struct TokenName
{
    const char *zh;
    const char *en;
};

/**
 * @brief 按当前显示语言挑一份写法。
 * @param name 双语名字。
 * @return 当前语言的文本。
 */
const char *pick(const TokenName &name);

/**
 * @brief 内部 Token 名 → 双语显示名。
 * @return 只读表。
 */
const std::map<std::string, TokenName> &tokenDisplayNames();

/**
 * @brief 查一个内部 Token 名在当前语言下的写法。
 * @param internal 内部名（如 `SEMI`）。
 * @return 显示名；表里没有就返回原样。
 */
std::string tokenDisplayName(const std::string &internal);

/**
 * @brief 把消息里的内部 Token 名换成当前语言下的「名字(写法)」。
 * @param message 解析器 / 语义检查给出的原始消息。
 * @return 换过名字的消息；没有命中就原样返回。
 */
Memory::String humanizeMessage(const Memory::String &message);

/**
 * @brief 剥掉解析器消息自带的「语法错误：」前缀。
 *
 * 驱动打印时已经写了「语法错误：<文件> 第 X 行第 Y 列：」，消息里再来一次就成了
 * 「语法错误：…：语法错误：期望…」。
 * @param message 原始消息。
 * @return 剥掉前缀后的消息（没有该前缀就原样返回）。
 */
Memory::String stripSyntaxPrefix(const Memory::String &message);

/**
 * @brief 按当前语言把「期望 A / B 之一，实际是 C」重拼一遍。
 *
 * 语序是跟着语言走的（英文的 `one of` 在前面、中文的「之一」在后面），所以整句重建，
 * 不做逐词替换。
 * @param expected 期望的 Token 显示名（可能只列了前几个）。
 * @param total 解析器统计到的期望总数（不超过 `expected` 大小时按后者算）。
 * @param actual 实际看到的 Token 显示名。
 * @return 一条完整说明。
 */
std::string composeExpectation(const std::vector<std::string> &expected,
                               std::size_t total, const std::string &actual);

/**
 * @brief 把解析器的语法错误消息翻成当前语言。
 *
 * 走两步：先剥「语法错误：」，再按中文骨架拆出「期望 / 实际」重建；拆不出来
 * （遇到没见过的文案）就只换 Token 名，至少保证不显示内部名。
 * @param message 解析器给的原始消息。
 * @return 当前语言的说明。
 */
Memory::String localizeSyntaxMessage(const Memory::String &message);

/**
 * @brief 一个字符的显示宽度（中文等宽字符算 2）。
 * @param text UTF-8 文本。
 * @return 列数。
 */
std::size_t displayWidth(const std::string &text);

/**
 * @brief 把制表符按 4 列制表位展开成空格（画波浪号前先铺平，否则列会歪）。
 */
std::string expandTabs(const std::string &text);

/**
 * @brief `renderSnippet` 的 length 取这个值：**整行都算重点**。
 *
 * 语义诊断经常只知道"第几行"（拿不到精确列），这时只标一格反而像指着别处，
 * 所以整行铺满波浪号，告诉用户"这一行整句都在报错范围内"。
 */
inline constexpr std::size_t kSnippetWholeLine = static_cast<std::size_t>(-1);

/**
 * @brief 判断字节是否可用于诊断中标识符的连续范围。
 * @param c 待检查的 UTF-8 字节。
 * @return ASCII 标识符字符或非 ASCII 字节返回 true。
 */
bool isWordByte(unsigned char c);

/**
 * @brief 自动定位时，把落点间接到「这一行第一个词」上。
 *
 * 语义诊断给的是**节点被创建时**的列，而节点是归约完才造的 —— 语句级的节点
 * 落点就是它在行的最后那个 token（`;` / `}`）。指着分号说"这里错了"等于没说，
 * 所以落到实处是标点 / 空白时，改指本行第一个词（语句开头），比指成分号好认。
 * @param raw 未经 Tab 展开的整行原文。
 * @param start 原先的 0 基落点。
 * @return 调整后的 0 基落点。
 */
std::size_t anchorColumn(const std::string &raw, std::size_t start);

/**
 * @brief 从 raw 的 start 位置起，取出那个「词」的长度。
 *
 * 报错有时只给得出起始列（比如降级期诊断），把整条标识符画出来比只画一格好认；
 * 指到空白 / 运算符上时退成一个字符。
 * @param raw 未经 Tab 展开的整行原文。
 * @param start 0 基起始下标。
 * @return 词的长度，至少 1。
 */
std::size_t wordExtentAt(const std::string &raw, std::size_t start);

/**
 * @brief 读一个源文件的全部行并缓存（同一轮里报好几处错时只读一次）。
 * @param file 源文件路径。
 * @return 按行存好的文本；读不到就是空表。
 */
const std::vector<std::string> &cachedFileLines(const Memory::String &file);

/**
 * @brief 这一行能不能从磁盘上读回来 —— 决定**要不要补"第 N 行"**。
 * 合成文件（`<内建 lang>` 之类）与前处理行映射失效时读不到，这时给行号等于指着空气。
 * @param file 源文件路径。
 * @param line 行号（1 基）。
 * @return 读得到返回 true。
 */
bool hasSourceLine(const Memory::String &file, std::size_t line);

/**
 * @brief 自动定位用的实际落点列号（1 基）—— 调整过的，供**打印 "第 N 列"** 用。
 *
 * 调整必须在这里做而不能只在画图时做：否则文案写"第 15 列"、波浪号却画在"第 6 列"
 * 上，人看到的两处说的不是一回事（编辑器里的划波浪线也按列号走）。
 * @param file 源文件路径。
 * @param line 行号（1 基）。
 * @param column 原始落点列号（1 基）。
 * @return 调整后的列号；拿不到源文时原样返回。
 */
std::size_t anchoredColumn(const Memory::String &file, std::size_t line, std::size_t column);

/**
 * @brief 回显出错那一行并在下面画波浪号。
 *
 * 形如（行号栏右对齐，与行号位数对齐）：
 * ```
 *    |
 *  2 | extern impl Value : Some = binding;
 *    |        ~~~~
 * ```
 * @param file 源文件路径（读不到就返回空串，调用方跳过）。
 * @param line 行号（1 基）。
 * @param column 列号（1 基）；0 表示不知道列，从头画起。
 * @param length 出错位置的字符数；0 = 按列的那个「词」自动定；`kSnippetWholeLine` = 整行。
 * @return 三行文本；拿不到源文时返回空串。
 */
Memory::String renderSnippet(const Memory::String &file, std::size_t line,
                             std::size_t column, std::size_t length = 0u);

}  // namespace chali_report
