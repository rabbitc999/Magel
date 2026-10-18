#include "server.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>

namespace lsp
{
namespace
{

const char *kServerName = "magel-lsp";
const char *kServerVersion = "1.0.0";

/**
 * 语义 token 的 legend：与 `mc.cpp` 里写进 token 的类型名一一对应。
 */
const std::vector<std::string> kTokenTypes = { "keyword", "type", "function", "string",
                                               "number", "comment", "operator", "variable",
                                               "parameter", "macro" };

/**
 * @brief 将内部符号类别转换为 LSP 补全类别。
 * @param kind 待处理对象的类别。
 * @retval 协议规定的符号类别编号。
 */
int completionKind(const std::string &kind)
{
    if (kind == "keyword") return 14;
    if (kind == "function" || kind == "macro") return 3;
    if (kind == "enum") return 13;
    return 7;  // type / class
}

/**
 * @brief 将内部符号类别转换为 LSP 文档符号类别。
 * @param kind 待处理对象的类别。
 * @retval 协议规定的符号类别编号。
 */
int symbolKind(const std::string &kind)
{
    if (kind == "keyword") return 20;  // Key
    if (kind == "macro") return 12;    // Function
    return 22;                         // EnumMember（宏参数等）
}

/**
 * @brief 建一个空对象（`json::Value` 是变体，得先装上 Object）。
 * @retval 保存空对象的 JSON 值。
 */
json::Value Object() { return json::Value(json::Object{}); }
/**
 * @brief 建一个空数组。
 * @retval 保存空数组的 JSON 值。
 */
json::Value Array() { return json::Value(json::Array{}); }
/**
 * @brief 往对象里放一个成员（`Object` 按插入顺序保存，dump 也按这个顺序）。
 * @param object 待访问的对象。
 * @param key 查找或写入使用的键。
 * @param value 待转换、保存或参与计算的值。
 */
void Set(json::Value &object, const char *key, json::Value value)
{
    object.asObject()->operator[](key) = std::move(value);
}
/**
 * @brief 从对象里取成员的整数值；成员不存在或不是整数时用 `fallback`。
 * @param object 待访问的对象。
 * @param key 查找或写入使用的键。
 * @param fallback 无法取得目标值时使用的默认值。
 * @retval 目标成员的整数值；缺失或类型不匹配时为 fallback。
 */
std::int64_t IntOr(const json::Value &object, const char *key, std::int64_t fallback)
{
    const json::Value *found = object.find(key);
    return found == nullptr ? fallback : found->intOr(fallback);
}
/**
 * @brief 从对象里取成员的字符串；成员不存在或不是字符串时返回空串。
 * @param object 对象。
 * @param key 成员名。
 * @return 字符串。
 */
std::string StringOf(const json::Value &object, const char *key)
{
    const json::Value *found = object.find(key);
    if (found == nullptr) return {};
    const std::optional<std::string_view> text = found->asString();
    return text.has_value() ? std::string(*text) : std::string();
}

}  // namespace

/**
 * @brief 为协议消息添加 Content-Length 头部。
 * @param payload 枚举载荷或协议消息正文。
 * @retval 生成或读取的结果文本。
 */
std::string frameMessage(const std::string &payload)
{
    return "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + payload;
}

/**
 * @brief 从输入流读取带 Content-Length 的协议消息。
 * @param in 输入流或待写入的数据缓冲区。
 * @param payload 枚举载荷或协议消息正文。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool readMessage(std::istream &in, std::string &payload, std::string &error)
{
    payload.clear();
    error.clear();
    std::string line;
    std::size_t length = 0;
    bool hasLength = false;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const auto first = value.find_first_not_of(" \t");
        value = first == std::string::npos ? std::string() : value.substr(first);
        if (key == "content-length")
        {
            try
            {
                length = static_cast<std::size_t>(std::stoull(value));
                hasLength = true;
            } catch (const std::exception &)
            {
                error = "invalid Content-Length";
                return false;
            }
        }
    }
    if (!hasLength) return false;
    payload.resize(length);
    if (length > 0) in.read(&payload[0], static_cast<std::streamsize>(length));
    if (static_cast<std::size_t>(in.gcount()) != length)
    {
        error = "truncated message";
        return false;
    }
    return true;
}

/**
 * @brief 构造 JSON-RPC 成功响应。
 * @param id 待查找或操作的对象编号。
 * @param result 接收处理结果的对象。
 * @retval 封装处理结果的 JSON 值。
 */
json::Value Server::response(const json::Value &id, json::Value result) const
{
    json::Value message = Object();
    Set(message, "jsonrpc", json::Value(std::string("2.0")));
    Set(message, "id", id);
    Set(message, "result", std::move(result));
    return message;
}

/**
 * @brief 构造 JSON-RPC 错误响应。
 * @param id 待查找或操作的对象编号。
 * @param code 错误码、字符值或待分析的源码文本。
 * @param text 待处理的文本。
 * @retval 最近一次错误的说明文本。
 */
json::Value Server::error(const json::Value &id, int code, const std::string &text) const
{
    json::Value message = Object();
    Set(message, "jsonrpc", json::Value(std::string("2.0")));
    Set(message, "id", id);
    json::Value errorObject = Object();
    Set(errorObject, "code", json::Value(static_cast<std::int64_t>(code)));
    Set(errorObject, "message", json::Value(text));
    Set(message, "error", std::move(errorObject));
    return message;
}

/**
 * @brief 将源码范围编码为 LSP JSON 对象。
 * @param range 待编码的源码范围。
 * @retval 封装处理结果的 JSON 值。
 */
json::Value Server::rangeJson(const Range &range) const
{
    json::Value start = Object();
    Set(start, "line", json::Value(static_cast<std::int64_t>(range.start.line)));
    Set(start, "character", json::Value(static_cast<std::int64_t>(range.start.column)));
    json::Value end = Object();
    Set(end, "line", json::Value(static_cast<std::int64_t>(range.end.line)));
    Set(end, "character", json::Value(static_cast<std::int64_t>(range.end.column)));
    json::Value result = Object();
    Set(result, "start", std::move(start));
    Set(result, "end", std::move(end));
    return result;
}

/**
 * @brief 向客户端发布文档诊断信息。
 * @param uri 文档的文件 URI。
 * @param diagnostics 接收处理诊断的列表。
 * @retval 封装处理结果的 JSON 值。
 */
json::Value Server::publishDiagnostics(const std::string &uri,
                                       const std::vector<Diagnostic> &diagnostics) const
{
    json::Value items = Array();
    for (const auto &diagnostic : diagnostics)
    {
        json::Value item = Object();
        Set(item, "range", rangeJson(diagnostic.range));
        Set(item, "severity", json::Value(static_cast<std::int64_t>(diagnostic.severity)));
        Set(item, "code", json::Value(diagnostic.code));
        Set(item, "source", json::Value(std::string("magel")));
        Set(item, "message", json::Value(diagnostic.message));
        items.asArray()->push_back(std::move(item));
    }
    json::Value params = Object();
    Set(params, "uri", json::Value(uri));
    Set(params, "diagnostics", std::move(items));
    json::Value notification = Object();
    Set(notification, "jsonrpc", json::Value(std::string("2.0")));
    Set(notification, "method", json::Value(std::string("textDocument/publishDiagnostics")));
    Set(notification, "params", std::move(params));
    return notification;
}

/**
 * @brief 按 URI 查找或建立文档状态。
 * @param params 形参或类型参数列表。
 * @retval 对应对象的指针。
 */
Server::Document *Server::document(const json::Value &params)
{
    const json::Value *textDocument = params.find("textDocument");
    if (textDocument == nullptr) return nullptr;
    const std::string uri = StringOf(*textDocument, "uri");
    if (uri.empty()) return nullptr;
    const auto found = documents_.find(uri);
    return found == documents_.end() ? nullptr : &found->second;
}

/**
 * @brief 处理客户端打开文档的通知。
 * @param uri 文档的文件 URI。
 * @param text 待处理的文本。
 */
void Server::openDocument(const std::string &uri, const std::string &text)
{
    Document &doc = documents_[uri];
    doc.text = text;
    doc.model.setText(text);
}

/**
 * @brief 将客户端文本变更应用到文档状态。
 * @param uri 文档的文件 URI。
 * @param text 待处理的文本。
 */
void Server::changeDocument(const std::string &uri, const std::string &text)
{
    Document &doc = documents_[uri];
    doc.text = text;
    doc.model.setText(text);
}

/**
 * @brief 检查文档文本并生成诊断。
 * @param text 待处理的文本。
 * @param path 目标文件或目录的路径。
 * @retval 生成或读取的结果文本。
 */
std::string Server::checkText(const std::string &text, const std::string &path)
{
    McDocument model;
    model.setText(text);
    return model.checkReport(path);
}

/**
 * @brief 解析并派发语言服务器协议消息。
 * @param payload 枚举载荷或协议消息正文。
 * @retval 生成或读取的结果文本。
 */
std::string Server::handleMessage(const std::string &payload)
{
    const json::ParseResult parsed = json::parse(payload);
    if (!parsed.ok())
    {
        const std::string why =
            parsed.diagnostics.empty() ? std::string() : parsed.diagnostics.front().toString();
        return frameMessage(error(json::Value(), -32700, "JSON 解析失败：" + why).dump());
    }
    const json::Value &message = *parsed.value;
    const json::Value *id = message.find("id");
    const json::Value null;
    const json::Value &requestId = id == nullptr ? null : *id;
    const std::string method = StringOf(message, "method");
    const json::Value *paramsPointer = message.find("params");
    const json::Value &params = paramsPointer == nullptr ? null : *paramsPointer;
    const bool isRequest = id != nullptr && !id->isNull();
    if (method.empty()) return {};

    if (method == "initialize")
    {
        json::Value legend = Object();
        json::Value types = Array();
        for (const std::string &type : kTokenTypes)
            types.asArray()->push_back(json::Value(type));
        Set(legend, "tokenTypes", std::move(types));
        Set(legend, "tokenModifiers", Array());
        json::Value semantic = Object();
        Set(semantic, "legend", std::move(legend));
        Set(semantic, "full", json::Value(true));
        json::Value sync = Object();
        Set(sync, "openClose", json::Value(true));
        Set(sync, "change", json::Value(static_cast<std::int64_t>(1)));  // 全量同步
        json::Value triggers = Array();
        for (const char *trigger : { "$", " ", "(" })
            triggers.asArray()->push_back(json::Value(std::string(trigger)));
        json::Value completion = Object();
        Set(completion, "triggerCharacters", std::move(triggers));
        json::Value capabilities = Object();
        Set(capabilities, "textDocumentSync", std::move(sync));
        Set(capabilities, "hoverProvider", json::Value(true));
        Set(capabilities, "completionProvider", std::move(completion));
        Set(capabilities, "definitionProvider", json::Value(true));
        Set(capabilities, "documentSymbolProvider", json::Value(true));
        Set(capabilities, "semanticTokensProvider", std::move(semantic));
        Set(capabilities, "documentFormattingProvider", json::Value(true));
        json::Value info = Object();
        Set(info, "name", json::Value(std::string(kServerName)));
        Set(info, "version", json::Value(std::string(kServerVersion)));
        json::Value result = Object();
        Set(result, "capabilities", std::move(capabilities));
        Set(result, "serverInfo", std::move(info));
        return frameMessage(response(requestId, std::move(result)).dump());
    }
    if (method == "initialized") return {};
    if (method == "shutdown")
    {
        shutdown_ = true;
        return frameMessage(response(requestId, json::Value()).dump());
    }
    if (method == "exit")
    {
        exitRequested_ = true;
        exitCode_ = shutdown_ ? 0 : 1;
        return {};
    }
    if (method == "$/cancelRequest") return {};
    if (method == "textDocument/didOpen")
    {
        const json::Value *textDocument = params.find("textDocument");
        if (textDocument == nullptr) return {};
        const std::string uri = StringOf(*textDocument, "uri");
        openDocument(uri, StringOf(*textDocument, "text"));
        const auto found = documents_.find(uri);
        return found == documents_.end() ? std::string() : frameMessage(publishDiagnostics(uri, found->second.model.diagnostics()).dump());
    }
    if (method == "textDocument/didChange")
    {
        const json::Value *textDocument = params.find("textDocument");
        if (textDocument == nullptr) return {};
        const std::string uri = StringOf(*textDocument, "uri");
        const json::Value *changes = params.find("contentChanges");
        if (changes != nullptr && changes->isArray() && !changes->asArray()->empty())
        {
            changeDocument(uri, StringOf(changes->asArray()->back(), "text"));
        }
        const auto found = documents_.find(uri);
        return found == documents_.end() ? std::string() : frameMessage(publishDiagnostics(uri, found->second.model.diagnostics()).dump());
    }
    if (method == "textDocument/didClose")
    {
        const json::Value *textDocument = params.find("textDocument");
        if (textDocument == nullptr) return {};
        const std::string uri = StringOf(*textDocument, "uri");
        documents_.erase(uri);
        return frameMessage(publishDiagnostics(uri, {}).dump());
    }

    Document *doc = document(params);
    if (doc == nullptr)
    {
        return isRequest ? frameMessage(error(requestId, -32602, "未打开的文本文档").dump()) : std::string();
    }
    const json::Value *positionJson = params.find("position");
    const Position position{
        static_cast<std::size_t>(positionJson == nullptr ? 0 : IntOr(*positionJson, "line", 0)),
        static_cast<std::size_t>(
            positionJson == nullptr ? 0 : IntOr(*positionJson, "character", 0))
    };

    if (method == "textDocument/hover")
    {
        const HoverResult hover = doc->model.hover(position);
        if (!hover.found) return frameMessage(response(requestId, json::Value()).dump());
        json::Value contents = Object();
        Set(contents, "kind", json::Value(std::string("markdown")));
        Set(contents, "value", json::Value(hover.markdown));
        json::Value result = Object();
        Set(result, "contents", std::move(contents));
        Set(result, "range", rangeJson(hover.range));
        return frameMessage(response(requestId, std::move(result)).dump());
    }
    if (method == "textDocument/completion")
    {
        json::Value items = Array();
        for (const auto &item : doc->model.completion(position))
        {
            json::Value json = Object();
            Set(json, "label", json::Value(item.label));
            Set(json, "kind", json::Value(static_cast<std::int64_t>(completionKind(item.kind))));
            Set(json, "detail", json::Value(item.detail));
            Set(json, "insertText", json::Value(item.insertText));
            items.asArray()->push_back(std::move(json));
        }
        json::Value result = Object();
        Set(result, "isIncomplete", json::Value(false));
        Set(result, "items", std::move(items));
        return frameMessage(response(requestId, std::move(result)).dump());
    }
    if (method == "textDocument/definition")
    {
        const std::string uri = StringOf(params.find("textDocument") ? *params.find("textDocument") : null,
                                         "uri");
        json::Value locations = Array();
        for (const auto &range : doc->model.definition(position))
        {
            json::Value location = Object();
            Set(location, "uri", json::Value(uri));
            Set(location, "range", rangeJson(range));
            locations.asArray()->push_back(std::move(location));
        }
        return frameMessage(response(requestId, std::move(locations)).dump());
    }
    if (method == "textDocument/documentSymbol")
    {
        json::Value symbols = Array();
        for (const auto &symbol : doc->model.symbols())
        {
            json::Value json = Object();
            Set(json, "name", json::Value(symbol.name));
            Set(json, "kind", json::Value(static_cast<std::int64_t>(symbolKind(symbol.kind))));
            Set(json, "detail", json::Value(symbol.detail));
            Set(json, "range", rangeJson(symbol.range));
            Set(json, "selectionRange", rangeJson(symbol.range));
            symbols.asArray()->push_back(std::move(json));
        }
        return frameMessage(response(requestId, std::move(symbols)).dump());
    }
    if (method == "textDocument/semanticTokens/full")
    {
        // 编码：deltaLine、deltaStart、length、tokenType、tokenModifiers。
        json::Value data = Array();
        std::size_t lastLine = 0;
        std::size_t lastColumn = 0;
        for (const auto &token : doc->model.semanticTokens())
        {
            const auto index = std::find(kTokenTypes.begin(), kTokenTypes.end(), token.type);
            const std::size_t typeIndex = index == kTokenTypes.end() ? 0 : static_cast<std::size_t>(index - kTokenTypes.begin());
            const std::size_t deltaLine = token.range.start.line - lastLine;
            const std::size_t deltaStart = deltaLine == 0 ? token.range.start.column - lastColumn : token.range.start.column;
            const std::size_t length = token.range.start.line == token.range.end.line ? token.range.end.column - token.range.start.column : 0;
            if (length == 0) continue;
            data.asArray()->push_back(json::Value(static_cast<std::int64_t>(deltaLine)));
            data.asArray()->push_back(json::Value(static_cast<std::int64_t>(deltaStart)));
            data.asArray()->push_back(json::Value(static_cast<std::int64_t>(length)));
            data.asArray()->push_back(json::Value(static_cast<std::int64_t>(typeIndex)));
            data.asArray()->push_back(json::Value(static_cast<std::int64_t>(0)));
            lastLine = token.range.start.line;
            lastColumn = token.range.start.column;
        }
        json::Value result = Object();
        Set(result, "data", std::move(data));
        return frameMessage(response(requestId, std::move(result)).dump());
    }
    if (method == "textDocument/formatting")
    {
        const std::string formatted = doc->model.format();
        json::Value edits = Array();
        if (formatted != doc->text)
        {
            json::Value edit = Object();
            Range whole;
            whole.start = Position{ 0, 0 };
            whole.end = Position{
                static_cast<std::size_t>(std::count(doc->text.begin(), doc->text.end(), '\n') + 1),
                0
            };
            Set(edit, "range", rangeJson(whole));
            Set(edit, "newText", json::Value(formatted));
            edits.asArray()->push_back(std::move(edit));
        }
        return frameMessage(response(requestId, std::move(edits)).dump());
    }
    if (isRequest)
    {
        return frameMessage(error(requestId, -32601, "未实现的方法：" + method).dump());
    }
    return {};
}

/**
 * @brief 在标准输入输出上运行语言服务器。
 * @param server 当前语言服务器实例。
 * @param in 输入流或待写入的数据缓冲区。
 * @param out 接收处理结果的输出对象。
 * @param log 接收诊断日志的输出流。
 * @retval 在标准输入输出上运行语言服务器。
 */
int runStdioServer(Server &server, std::istream &in, std::ostream &out, std::ostream &log)
{
    while (!server.exitRequested())
    {
        std::string payload;
        std::string error;
        if (!readMessage(in, payload, error))
        {
            if (!error.empty()) log << "读取 LSP 消息失败：" << error << "\n";
            break;
        }
        const std::string response = server.handleMessage(payload);
        if (!response.empty())
        {
            out << response;
            out.flush();
        }
    }
    return server.exitCode();
}

}  // namespace lsp
