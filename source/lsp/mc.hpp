#pragma once

// Magel 宏/块的编辑期文档模型（`$macro` / `$replace` / `$for` … 的配对、参数与引用）。
//
// 共用的值类型（Position / Range / Diagnostic / HoverResult … ）来自 ParseEngine 的
// 语言服务器共用类型 `types.hpp`：本目录自带的副本（原本来自 ParseEngine 的
// 不复制一份，免得两边的类型漂移。
#include "types.hpp"

#include <string>
#include <vector>

namespace lsp
{

class McDocument
{
public:
    void setText(std::string text);
    const std::vector<Diagnostic> &diagnostics() const { return diagnostics_; }
    std::vector<SymbolInfo> symbols() const;
    std::vector<SemanticToken> semanticTokens() const { return semanticTokens_; }
    HoverResult hover(Position position) const;
    std::vector<CompletionItem> completion(Position position) const;
    std::vector<Range> definition(Position position) const;
    std::vector<CodeAction> codeActions() const { return {}; }
    const std::string &format() const { return text_; }
    /**
     * `magel-lsp --check FILE` 的文本报告：每条诊断一行
     * `路径:行:列: 级别: 消息 [码]`（行列从 1 起，编辑器以外的地方用）。
     *
     * @param path 报告里显示的文件路径。
     * @return 报告文本；没有诊断时返回空串。
     */
    std::string checkReport(const std::string &path) const;

private:
    struct Macro
    {
        std::string name;
        std::vector<std::string> params;
        bool variadic = false;
        std::string variadicParam;
        std::string groupedSignature;
        std::string groupedShape;
        /**
         * 括号里空着（`$macro NAME()`）= 对象宏：裸写 `NAME` 取用，不带括号。
         */
        bool objectMacro = false;
        /**
         * `$delete_macro NAME` 撤销之后置位：不再参与补全 / 悬停 / 重载挑选，也不算重复定义。
         */
        bool deleted = false;
        Range range;
    };
    struct Occurrence
    {
        std::size_t macro;
        Range range;
    };
    std::string text_;
    std::vector<std::string> lines_;
    std::vector<Macro> macros_;
    std::vector<Occurrence> occurrences_;
    std::vector<Diagnostic> diagnostics_;
    std::vector<SemanticToken> semanticTokens_;
};

}  // namespace lsp
