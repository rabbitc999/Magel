// 单元测试：LSP 帧、ChaLi 宏模型（诊断 / 悬停 / 补全 / 跳转 / 大纲 / 语义高亮）
// 与一次完整的服务器会话。
#include "language_server.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace
{

int failures = 0;
#define CHECK(condition)                                                    \
    do                                                                      \
    {                                                                       \
        if (!(condition))                                                   \
        {                                                                   \
            std::cerr << "FAIL 第 " << __LINE__ << " 行: " #condition "\n"; \
            ++failures;                                                     \
        }                                                                   \
    } while (0)

/**
 * @brief 验证语言服务器消息封装与读取。
 */
void testFraming()
{
    const std::string payload = "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\"}";
    const std::string framed = lsp::frameMessage(payload);
    CHECK(framed.rfind("Content-Length: ", 0) == 0);
    std::istringstream in(framed);
    std::string read;
    std::string error;
    CHECK(lsp::readMessage(in, read, error));
    CHECK(read == payload);
    std::istringstream empty("");
    CHECK(!lsp::readMessage(empty, read, error));
}

/**
 * @brief 验证语言服务对宏及其符号的识别。
 */
void testMcMacros()
{
    lsp::McDocument model;
    model.setText("$macro ADD<T:x>(x, y)\nx + y\n$end_macro\n"
                  "int main() { return $ADD<int64>(1, 2); }\n");
    CHECK(model.diagnostics().empty());
    CHECK(model.symbols().size() == 1);
    CHECK(model.symbols()[0].name == "ADD");
    CHECK(model.hover({ 3, 22 }).found);
    CHECK(model.definition({ 3, 22 }).size() == 1);
    bool completed = false;
    for (const auto &item : model.completion({ 3, 22 }))
        if (item.label == "$ADD") completed = item.insertText == "$ADD(x, y)";
    CHECK(completed);
    model.setText("$macro CODE<T:x>[attrs](x, ...rest){body}\nbody;\n$end_macro\n"
                  "$CODE<int64>[readonly](1, 2){work();}\n");
    CHECK(model.diagnostics().empty());
    CHECK(model.hover({ 0, 9 }).markdown.find("<T:x>[attrs](x, ...rest){body}") != std::string::npos);
    model.setText("$macro VALUE\n42\n$end_macro\n$VALUE\n");
    CHECK(model.diagnostics().empty());
    model.setText("$macro VALUE()\n42\n$end_macro\n$VALUE\n");
    CHECK(!model.diagnostics().empty());
    model.setText("$macro VALUE\n42\n$end_macro\n$VALUE()\n");
    CHECK(!model.diagnostics().empty());
    model.setText("$macro VALUE()\n42\n$end_macro\n$VALUE()\n");
    CHECK(model.diagnostics().empty());
    model.setText("$replace ADD(a, b) a + b\n$ADD(1, 2)\n");
    CHECK(model.diagnostics().empty());
    model.setText("$replace VALUE 42\n$VALUE\n");
    CHECK(model.diagnostics().empty());
    model.setText("$replace EMPTY(a)\n");
    CHECK(!model.diagnostics().empty());
    model.setText("$macro expr OLD((a))\na\n$end_macro\n");
    CHECK(!model.diagnostics().empty());
    model.setText("$macro A(x)\nx\n");
    CHECK(!model.diagnostics().empty());
    model.setText("$macro A(x)\nx\n$end_macro\n$macro A(y)\ny\n$end_macro\n");
    CHECK(!model.diagnostics().empty());
    model.setText("$macro A(x)\nx\n$end_macro\n$A(\n");
    CHECK(model.diagnostics().empty());
}

/**
 * @brief 验证语言服务器会话请求与响应。
 */
void testSession()
{
    lsp::Server server;
    const std::string initResponse =
        server.handleMessage("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    CHECK(initResponse.find("semanticTokensProvider") != std::string::npos);
    CHECK(initResponse.find("chali-lsp") != std::string::npos);
    CHECK(server.handleMessage("{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}").empty());

    // 1. didOpen：宏定义与调用都配对（参数个数一致）→ 没有诊断。
    const std::string open =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":"
        "{\"uri\":\"file:///demo.chl\",\"languageId\":\"chali\",\"version\":1,"
        "\"text\":\"$macro A(x, y)\\n((x) + (y))\\n$end_macro\\nint main() { return $A(1, 2); }\\n\"}}}";
    const std::string opened = server.handleMessage(open);
    CHECK(opened.find("publishDiagnostics") != std::string::npos);
    CHECK(opened.find("\"diagnostics\":[]") != std::string::npos);

    // 2. 悬停：调用处应给出宏声明。
    const std::string hover =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/hover\",\"params\":"
        "{\"textDocument\":{\"uri\":\"file:///demo.chl\"},\"position\":{\"line\":3,\"character\":20}}}";
    CHECK(server.handleMessage(hover).find("contents") != std::string::npos);

    // 3. didChange（未保存内容）：实参个数不符 → 报 chali-macro-arity。
    const std::string change =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":"
        "{\"uri\":\"file:///demo.chl\",\"version\":2},\"contentChanges\":[{\"text\":"
        "\"$macro A(x, y)\\n((x) + (y))\\n$end_macro\\nint main() { return $A(1); }\\n\"}]}}";
    const std::string changed = server.handleMessage(change);
    CHECK(changed.find("chali-macro-arity") != std::string::npos);
    CHECK(changed.find("\"source\":\"chali\"") != std::string::npos);
    CHECK(changed.find("\"source\":\"chali\"") != std::string::npos);

    const std::string unknownMethod =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/unknown\",\"params\":"
        "{\"textDocument\":{\"uri\":\"file:///demo.chl\"}}}";
    CHECK(server.handleMessage(unknownMethod).find("-32601") != std::string::npos);
    CHECK(server.handleMessage("{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"shutdown\"}").find("\"result\"") != std::string::npos);
    CHECK(server.handleMessage("{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}").empty());
    CHECK(server.exitRequested() && server.exitCode() == 0);
    CHECK(!server.handleMessage("not json").empty());
}

}  // namespace

/**
 * @brief 运行本文件的回归测试并报告执行结果。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main()
{
    testFraming();
    testMcMacros();
    testSession();
    if (failures == 0)
    {
        std::cout << "chali-lsp tests：全部通过\n";
        return 0;
    }
    std::cerr << "失败 " << failures << " 项\n";
    return 1;
}
