// 单元测试：LSP 帧、Magel 宏模型（诊断 / 悬停 / 补全 / 跳转 / 大纲 / 语义高亮）
// 与一次完整的服务器会话。
#include "server.hpp"

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
    {
        lsp::McDocument grouped;
        grouped.setText("$macro CODE<..Types>[..Attrs](..Args){..Body}\nBody\n$end_macro\n"
                        "$CODE<List<List<int64>>>[readonly](x){\n    work();\n}\n");
        CHECK(grouped.diagnostics().empty());
        CHECK(grouped.symbols().size() == 1);
        CHECK(grouped.hover({ 0, 8 }).markdown.find("<..Types>[..Attrs](..Args){..Body}") != std::string::npos);
        bool completed = false;
        for (const auto &item : grouped.completion({ 3, 1 }))
            if (item.label == "CODE") completed = item.insertText == "$CODE<..Types>[..Attrs](..Args){..Body}";
        CHECK(completed);
    }
    lsp::McDocument model;
    model.setText("$macro ADD(x, y)\n((x) + (y))\n$end_macro\n"
                  "int main() { return ADD(1, 2); }\n");
    CHECK(model.diagnostics().empty());
    CHECK(model.symbols().size() == 1);
    CHECK(model.symbols()[0].name == "ADD");
    CHECK(model.hover({ 3, 22 }).found);
    CHECK(model.definition({ 3, 22 }).size() == 1);
    CHECK(model.definition({ 3, 22 })[0].start.line == 0);
    bool hasMacro = false;
    for (const auto &item : model.completion({ 3, 20 }))
        if (item.label == "ADD" && item.insertText == "ADD(x, y)") hasMacro = true;
    CHECK(hasMacro);

    /* 宏名前的 `expr`（整段展开加括号）与形参括号标注（每次替换加括号）都不是名字的一部分：
     * 大纲 / 定义 / 补全仍按裸名字走。 */
    model.setText("$macro expr PLUS((a), (b))\n((a) + (b))\n$end_macro\n"
                  "int main() { return PLUS(1, 2); }\n");
    CHECK(model.diagnostics().empty());
    CHECK(model.symbols().size() == 1);
    CHECK(model.symbols()[0].name == "PLUS");
    CHECK(model.hover({ 3, 22 }).found);
    CHECK(model.definition({ 3, 22 }).size() == 1);
    bool hasPlus = false;
    for (const auto &item : model.completion({ 3, 20 }))
        if (item.label == "PLUS" && item.insertText == "PLUS(a, b)") hasPlus = true;
    CHECK(hasPlus);

    /* `$replace` 是单行宏：头这一行剩下的内容就是宏体，没有 `$end_macro`。
     * 名字 / 定义 / 补全与 `$macro` 同口径。 */
    model.setText("$replace expr PLUS((a), (b)) a+b\n"
                  "int main() { return PLUS(1, 2); }\n");
    CHECK(model.diagnostics().empty());
    CHECK(model.symbols().size() == 1);
    CHECK(model.symbols()[0].name == "PLUS");
    CHECK(model.definition({ 1, 22 }).size() == 1);
    bool hasReplacePlus = false;
    for (const auto &item : model.completion({ 1, 20 }))
        if (item.label == "PLUS" && item.insertText == "PLUS(a, b)") hasReplacePlus = true;
    CHECK(hasReplacePlus);
    /* 只有头和形参表、同一行没写宏体要报错。 */
    model.setText("$replace EMPTY(a)\nint main() { return 0; }\n");
    CHECK(!model.diagnostics().empty());
    CHECK(model.diagnostics()[0].code == "magel-replace-header");
    /* 单行宏也可以省括号：`$replace NAME 宏体` 直接就是对象宏，裸写名字取用。 */
    model.setText("$replace LIMIT 100\nint main() { return LIMIT; }\n");
    CHECK(model.diagnostics().empty());
    CHECK(model.symbols().size() == 1);
    CHECK(model.definition({ 1, 21 }).size() == 1);
    /* 宏体和 `)` 之间可以没有空白（编译器也照收），别误报格式错。 */
    model.setText("$replace TWICE(x)x*2\nint main() { return TWICE(3); }\n");
    CHECK(model.diagnostics().empty());
    /* 旧反引号标注已废除。 */
    model.setText("$macro BAD(`...rest)\nrest\n$end_macro\nint main() { return 0; }\n");
    CHECK(!model.diagnostics().empty());
    CHECK(model.diagnostics()[0].code == "magel-macro-params");

    model.setText("$macro ADD(x, y)\n((x) + (y))\n$end_macro\n"
                  "int main() { return ADD(1); }\n");
    CHECK(!model.diagnostics().empty());
    CHECK(model.diagnostics()[0].code == "magel-macro-arity");
    model.setText("$macro ADD(x, y)\n((x) + (y))\n");
    CHECK(!model.diagnostics().empty());
    CHECK(model.diagnostics()[0].code == "magel-macro-unclosed");

    /* `$for` 块用 `$end_for` 收尾，不算宏结束；旧拼写 `$end` 给迁移诊断。 */
    model.setText("$macro SHOW(label, ...rest)\n$for v in rest\nlang::println(label, v);\n$end_for\n$end_macro\n"
                  "int main() { SHOW(\"v={}\", 1, 2); }\n");
    CHECK(model.diagnostics().empty());
    /* 对象宏：声明带 `()`，调用裸写；写成 `NAME(...)` 反而是错的。 */
    model.setText("$macro LIMIT()\n100\n$end_macro\nint main() { return LIMIT; }\n");
    CHECK(model.diagnostics().empty());
    model.setText("$macro LIMIT()\n100\n$end_macro\nint main() { return LIMIT(); }\n");
    CHECK(!model.diagnostics().empty());
    CHECK(model.diagnostics()[0].code == "magel-macro-object-call");
    model.setText("$macro ADD(x, y)\n((x) + (y))\n$end\n");
    CHECK(!model.diagnostics().empty());
    CHECK(model.diagnostics()[0].code == "magel-macro-end-legacy");
    model.setText("$end_for\n");
    CHECK(!model.diagnostics().empty());
    CHECK(model.diagnostics()[0].code == "magel-end-for");
    bool hasLine = false;
    for (const auto &item : model.completion({ 0, 0 }))
        if (item.label == "$line" || item.label == "$end_for") hasLine = true;
    CHECK(hasLine);

    /* `$delete_macro NAME` 撤掉整组之后可以重新定义：不算重复定义，也不该把新声明
     * 当成对象宏调用（真实样例 macros.mgl 就是"删掉 REVISION 再定义"）。 */
    model.setText("$macro REVISION()\n1\n$end_macro\n$delete_macro REVISION\n"
                  "$macro REVISION()\n2\n$end_macro\nint main() { return REVISION; }\n");
    CHECK(model.diagnostics().empty());
    CHECK(model.symbols().size() == 1); /* 撤掉的那个不进大纲 */
    /* 同名按实参个数重载：不算重复定义，调用点按个数挑形态（编译器同口径）。 */
    model.setText("$macro REPORT(a)\nlang::println(\"one {}\", a);\n$end_macro\n"
                  "$macro REPORT(a, b)\nlang::println(\"two {} {}\", a, b);\n$end_macro\n"
                  "int main() {\n    REPORT(7)\n    REPORT(1, 2)\n    return 0;\n}\n");
    CHECK(model.diagnostics().empty());
    /* 没有形态能对上时，列出候选（和编译器的报错口径一致）。 */
    model.setText("$macro REPORT(a)\nlang::println(\"one {}\", a);\n$end_macro\n"
                  "int main() {\n    REPORT(1, 2, 3)\n    return 0;\n}\n");
    CHECK(!model.diagnostics().empty());
    CHECK(model.diagnostics()[0].code == "magel-macro-arity");
    CHECK(model.diagnostics()[0].message.find("没有匹配的形态") != std::string::npos);

    /* `$match` 那一套已从语言删除：补全列表里不该再有它（防它被悄悄加回来）。
     * 宏体里再写 `$match` 只是普通文本，编辑期不报错，由编译器在编译期报。 */
    bool hasMatch = false;
    for (const auto &item : model.completion({ 0, 0 }))
        if (item.label == "$match" || item.label == "$end_when") hasMatch = true;
    CHECK(!hasMatch);
    model.setText("$macro SHAPE(kind)\nlang::println(\"圆\");\n$end_macro\n");
    CHECK(model.diagnostics().empty());

    lsp::Server server;
    const std::string opened = server.handleMessage(
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///demo.mgl\",\"languageId\":\"magel\","
        "\"text\":\"$macro ADD(x, y)\\n((x) + (y))\\n\"}}}");
    CHECK(opened.find("magel-macro-unclosed") != std::string::npos);
    CHECK(opened.find("\"source\":\"magel\"") != std::string::npos);
    const std::string untitled = server.handleMessage(
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"untitled:Untitled-1\",\"languageId\":\"magel\","
        "\"text\":\"$macro EMPTY()\\n\"}}}");
    CHECK(untitled.find("magel-macro-unclosed") != std::string::npos);
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
    CHECK(initResponse.find("magel-lsp") != std::string::npos);
    CHECK(server.handleMessage("{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}").empty());

    // 1. didOpen：宏定义与调用都配对（参数个数一致）→ 没有诊断。
    const std::string open =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":"
        "{\"uri\":\"file:///demo.mgl\",\"languageId\":\"magel\",\"version\":1,"
        "\"text\":\"$macro A(x, y)\\n((x) + (y))\\n$end_macro\\nint main() { return A(1, 2); }\\n\"}}}";
    const std::string opened = server.handleMessage(open);
    CHECK(opened.find("publishDiagnostics") != std::string::npos);
    CHECK(opened.find("\"diagnostics\":[]") != std::string::npos);

    // 2. 悬停：调用处应给出宏声明。
    const std::string hover =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/hover\",\"params\":"
        "{\"textDocument\":{\"uri\":\"file:///demo.mgl\"},\"position\":{\"line\":3,\"character\":20}}}";
    CHECK(server.handleMessage(hover).find("contents") != std::string::npos);

    // 3. didChange（未保存内容）：实参个数不符 → 报 magel-macro-arity。
    const std::string change =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":"
        "{\"uri\":\"file:///demo.mgl\",\"version\":2},\"contentChanges\":[{\"text\":"
        "\"$macro A(x, y)\\n((x) + (y))\\n$end_macro\\nint main() { return A(1); }\\n\"}]}}";
    const std::string changed = server.handleMessage(change);
    CHECK(changed.find("magel-macro-arity") != std::string::npos);
    CHECK(changed.find("\"source\":\"magel\"") != std::string::npos);
    CHECK(changed.find("\"source\":\"magel\"") != std::string::npos);

    const std::string unknownMethod =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/unknown\",\"params\":"
        "{\"textDocument\":{\"uri\":\"file:///demo.mgl\"}}}";
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
        std::cout << "magel-lsp tests：全部通过\n";
        return 0;
    }
    std::cerr << "失败 " << failures << " 项\n";
    return 1;
}
