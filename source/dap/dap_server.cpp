#include "dap_server.hpp"

#include "../../library/json/json.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace dap
{
namespace
{

constexpr std::int64_t kThreadId = 1;

/**
 * 单步意图：空表示"没有待执行的单步，停在断点上等命令"。
 */
enum class StepKind
{
    None,  ///< 未请求单步。
    Into,  ///< 步进：下一条指令就停。
    Over,  ///< 步过：同层内跑到源码行变化为止。
    Out    ///< 步出：回到更浅的栈为止。
};

stackvm::Machine *g_machine = nullptr;
const stackvm::DebugInfo *g_debugInfo = nullptr;
bool g_finished = false;

/**
 * 用户下的断点：`源码路径 → 行号集合`（行号 1 基，与 DAP 一致）。
 */
std::map<std::string, std::set<std::int64_t>> g_sourceBreakpoints;
/**
 * 由上面反查出来的指令下标；每次断点变化都重算并写进 VM。
 */
std::vector<std::size_t> g_instructionBreaks;

/**
 * 调用栈里一帧的展示信息。
 */
struct StoppedFrame
{
    std::string function;   ///< 函数名；空表示入口。
    std::string file;       ///< 源文件（`.mgl`）；空表示没有位置。
    std::int32_t line = 0;  ///< 行号（1 基）；0 表示未知。
};

/**
 * 最近一次停下的现场（外 → 内）。
 *
 * 刻意只存 `std::` 容器：**不要**留 `stackvm::DebugEvent` / `Memory::String` 这类
 * `Memory::` 容器——它们从运行时 arena 分配，留到进程末尾会让 arena 收尾时判定
 * "仍有活动块"并 `abort()`。
 */
std::vector<StoppedFrame> g_stoppedFrames;
std::string g_stoppedFile;           ///< 当前位置的文件。
std::int32_t g_stoppedLine = 0;      ///< 当前位置的行号。
std::size_t g_stoppedDepth = 0;      ///< 停下时的栈深。
std::size_t g_stoppedSteps = 0;      ///< 已执行指令数（判断是不是入口那一停）。
bool g_stoppedByBreakpoint = false;  ///< 是撞断点停的还是单步停的。
bool g_hasStopped = false;

/**
 * 本轮待执行的单步意图与目标栈深（步过 / 步出用）。
 */
StepKind g_stepKind = StepKind::None;
std::size_t g_stepDepth = 0;
std::int32_t g_stepLine = 0;
std::string g_stepFile;

/**
 * @brief Windows 下 stdout 默认是文本模式，会把 `\n` 翻成 `\r\n`，
 * 于是我们写的 `\r\n` 变成 `\r\r\n`，协议就废了：先把 stdio 切成二进制。
 */
void ensureBinaryStdio()
{
#ifdef _WIN32
    static bool switched = false;
    if (switched) return;
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
    switched = true;
#endif
}

/**
 * @brief 将 JSON-RPC 消息封装并写入协议输出。
 *
 * 写一条 DAP 消息（Content-Length 帧 + JSON）。
 * @param message 待报告或输出的信息文本。
 */
void send(const json::Value &message)
{
    const std::string payload = message.dump();
    std::cout << "Content-Length: " << payload.size() << "\r\n\r\n"
              << payload;
    std::cout.flush();
}

/**
 * @brief 发事件（`event` 字段 + 可选 body）。
 * @param name 名称文本。
 * @param body 待处理的函数体或循环体。
 */
void sendEvent(const std::string &name, json::Value body = json::Value())
{
    json::Value message = json::Value::object({ { "type", json::Value("event") },
                                                { "event", json::Value(name) } });
    if (!body.isNull()) message["body"] = std::move(body);
    send(message);
}

/**
 * @brief 成功响应。
 * @param id 请求的 `seq`（回包的 `request_seq` 必须是它，否则前端会一直等）。
 * @param command 请求的命令名（前端按它匹配回包）。
 * @param body 响应体。
 */
void sendResponse(const json::Value &id, const std::string &command,
                  json::Value body = json::Value())
{
    json::Value message = json::Value::object(
        { { "type", json::Value("response") },
          { "request_seq", id },
          { "success", json::Value(true) },
          { "command", json::Value(command) } });
    if (!body.isNull()) message["body"] = std::move(body);
    send(message);
}

/**
 * @brief 读一帧；`false` 表示流已结束。
 * @param payload 枚举载荷或协议消息正文。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool readFrame(std::string &payload)
{
    std::string headerLine;
    std::size_t length = 0;
    bool hasLength = false;
    while (true)
    {
        if (!std::getline(std::cin, headerLine)) return false;
        if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
        if (headerLine.empty()) break;
        const std::string prefix = "Content-Length:";
        if (headerLine.compare(0u, prefix.size(), prefix) == 0u)
        {
            length = static_cast<std::size_t>(std::stoll(headerLine.substr(prefix.size())));
            hasLength = true;
        }
    }
    if (!hasLength) return false;
    payload.resize(length);
    if (length == 0u) return true;
    std::cin.read(payload.data(), static_cast<std::streamsize>(length));
    if (std::cin.gcount() != static_cast<std::streamsize>(length)) return false;
    return true;
}

/**
 * @brief 从请求里取字符串字段；没有给默认值。
 * @param message 待报告或输出的信息文本。
 * @param key 查找或写入使用的键。
 * @param fallback 无法取得目标值时使用的默认值。
 * @retval 生成或读取的结果文本。
 */
std::string stringField(const json::Value &message, const std::string &key,
                        const std::string &fallback = std::string())
{
    const json::Value *value = message.find(key);
    if (value == nullptr || !value->isString()) return fallback;
    const std::optional<std::string_view> text = value->asString();
    return text ? std::string(*text) : fallback;
}

/**
 * @brief 从请求里取整数字段；没有给默认值。
 * @param message 待报告或输出的信息文本。
 * @param key 查找或写入使用的键。
 * @param fallback 无法取得目标值时使用的默认值。
 * @retval 从请求里取整数字段；没有给默认值。
 */
std::int64_t intField(const json::Value &message, const std::string &key, std::int64_t fallback = 0)
{
    const json::Value *value = message.find(key);
    if (value == nullptr) return fallback;
    const std::optional<std::int64_t> number = value->asInt();
    return number ? *number : fallback;
}

/**
 * @brief 取请求 id（数字形式；字符串 id 这里按 0 处理）。
 * @param message 待报告或输出的信息文本。
 * @retval 封装处理结果的 JSON 值。
 */
json::Value requestId(const json::Value &message)
{
    const json::Value *id = message.find("seq");
    return id != nullptr ? *id : json::Value(static_cast<std::int64_t>(0));
}

/**
 * @brief `Memory::String` 转 `std::string`（两者是不同的 basic_string 特化，不能隐式互转）。
 * @param text 待处理的文本。
 * @retval 生成或读取的结果文本。
 */
std::string toStd(const Memory::String &text) { return std::string(text.data(), text.size()); }

/**
 * @brief 路径归一化：统一分隔符，Windows 上再统一大小写。
 *
 * 前端给的是绝对路径（`D:\...\a.mgl`），调试表里可能是相对路径（`example/a.mgl`），
 * 分隔符还可能是 `/`：不做归一化的话后缀比对永远失败，断点就一个也下不上。
 */
std::string normalizePath(const std::string &path)
{
    std::string text;
    text.reserve(path.size());
    for (const char character : path)
    {
        if (character == '\\')
        {
            text.push_back('/');
            continue;
        }
#ifdef _WIN32
        text.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
#else
        text.push_back(character);
#endif
    }
    return text;
}

/**
 * @brief 判断两条路径是不是同一个文件（容忍绝对 / 相对、分隔符与大小写差异）。
 * @param entryFile 入口源码文件的路径。
 * @param wanted 待比较的目标文件路径。
 * @retval 两条路径是不是同一个文件（容忍绝对 / 相对、分隔符与大小写差异）时为 true，否则为 false。
 */
bool sameFile(const std::string &entryFile, const std::string &wanted)
{
    if (entryFile.empty() || wanted.empty()) return false;
    if (entryFile == wanted) return true;
    const std::string left = normalizePath(entryFile);
    const std::string right = normalizePath(wanted);
    if (left == right) return true;
    /* 一端是另一端的路径后缀，就当同一个文件（绝对 vs 相对）。 */
    if (left.size() >= right.size() && left.compare(left.size() - right.size(), right.size(), right) == 0)
        return true;
    if (right.size() >= left.size() && right.compare(right.size() - left.size(), left.size(), left) == 0)
        return true;
    return false;
}

/**
 * @brief 调试表里的一条位置是否指向编译器内部的 `.quad`（不展示给用户）。
 * @param file 目标文件或文件描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool isInternalQuad(const std::string &file)
{
    return file.size() >= 5u && file.compare(file.size() - 5u, 5u, ".quad") == 0;
}

/**
 * @brief 把「文件 + 行号」反查成指令下标：取该行及其后最近的一条有位置的指令。
 * @param file 目标文件或文件描述。
 * @param line 源码行号。
 * @retval 保存把「文件 + 行号」反查成指令下标：取该行及其后最近的一条有位置的指令结果的对象。
 */
std::optional<std::size_t> instructionForLine(const std::string &file, std::int64_t line)
{
    if (g_debugInfo == nullptr || line <= 0) return std::nullopt;
    std::optional<std::size_t> exact;
    std::optional<std::size_t> after;
    /* `entries` 是「指令下标 → 位置」的递增表，元素是 `pair<下标, 位置>`。 */
    for (const auto &entry : g_debugInfo->entries)
    {
        const std::size_t instruction = entry.first;
        const stackvm::SourceLocation &location = entry.second;
        /* `Memory::String` 同时能转 `std::string` 和 `std::string_view`，直接传会二义，
         * 这里统一先转成 `std::string`。 */
        const std::string entryFile = toStd(location.file);
        if (isInternalQuad(entryFile)) continue;
        const std::int64_t entryLine = location.line;
        if (entryLine <= 0) continue;
        if (!sameFile(entryFile, file)) continue;
        if (entryLine == line)
        {
            if (!exact || instruction < *exact) exact = instruction;
        }
        else if (entryLine > line)
        {
            if (!after || instruction < *after) after = instruction;
        }
    }
    if (exact) return exact;
    return after;
}

/**
 * @brief 重算指令级断点并写进 VM。
 */
void refreshBreakpoints()
{
    g_instructionBreaks.clear();
    for (const auto &entry : g_sourceBreakpoints)
        for (const std::int64_t line : entry.second)
            if (const std::optional<std::size_t> instruction = instructionForLine(entry.first, line))
                g_instructionBreaks.push_back(*instruction);
    std::sort(g_instructionBreaks.begin(), g_instructionBreaks.end());
    g_instructionBreaks.erase(std::unique(g_instructionBreaks.begin(), g_instructionBreaks.end()),
                              g_instructionBreaks.end());
    if (g_machine != nullptr)
        g_machine->setBreakpoints(
            Memory::Vector<std::size_t>(g_instructionBreaks.begin(), g_instructionBreaks.end()));
}

/**
 * @brief 当前停下位置对应的一行是否变化了（步过用）。
 * @param event 待处理的界面事件或事件编号。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool movedToNewLine(const stackvm::DebugEvent &event)
{
    return event.location.line != g_stepLine || toStd(event.location.file) != g_stepFile;
}

/**
 * @brief 构造 `stopped` 事件体。
 * @param reason 接收释放失败原因或停止原因的对象。
 * @retval 封装处理结果的 JSON 值。
 */
json::Value stoppedBody(const std::string &reason)
{
    return json::Value::object({ { "reason", json::Value(reason) },
                                 { "threadId", json::Value(kThreadId) },
                                 { "allThreadsStopped", json::Value(true) } });
}

/**
 * @brief 把调用栈转成 DAP 的 `StackFrame` 列表（内层在前）。
 * @retval 封装处理结果的 JSON 值。
 */
json::Value stackFrames()
{
    json::Array frames;
    if (!g_hasStopped) return json::Value(frames);
    /* `g_stoppedFrames` 是外 → 内；DAP 要栈顶在前，倒着塞。
     * 编译器内部的 `.quad` 帧不展示（与命令行错误栈保持一致）。 */
    for (std::size_t index = g_stoppedFrames.size(); index > 0u; --index)
    {
        const StoppedFrame &frame = g_stoppedFrames[index - 1u];
        if (isInternalQuad(frame.file)) continue;
        json::Value source = json::Value::object({ { "name", json::Value(frame.file) } });
        if (!frame.file.empty()) source["path"] = json::Value(frame.file);
        frames.push_back(json::Value::object(
            { { "id", json::Value(static_cast<std::int64_t>(frames.size())) },
              { "name", json::Value(frame.function.empty() ? std::string("(入口)") : frame.function) },
              { "source", std::move(source) },
              { "line", json::Value(static_cast<std::int64_t>(frame.line > 0 ? frame.line : 1)) },
              { "column", json::Value(static_cast<std::int64_t>(1)) } }));
    }
    if (frames.empty())
    {
        /* 全是内部帧（例如只有 `.quad` 位置）：至少给一条入口帧，前端才不会空着。 */
        frames.push_back(json::Value::object(
            { { "id", json::Value(static_cast<std::int64_t>(0)) },
              { "name", json::Value(std::string("(入口)")) },
              { "line", json::Value(static_cast<std::int64_t>(1)) },
              { "column", json::Value(static_cast<std::int64_t>(1)) } }));
    }
    return json::Value(frames);
}

/**
 * @brief 处理一条 DAP 消息。
 * @param message 已解析的消息。
 * @param action 需要 VM 继续时写入下一步动作。
 * @return 是否"可以继续跑"（即本条命令给出了下一步动作）。
 */
bool handleMessage(const json::Value &message, stackvm::DebugAction &action)
{
    const std::string command = stringField(message, "command");
    const json::Value id = requestId(message);

    if (command == "initialize")
    {
        json::Value capabilities = json::Value::object({
            { "supportsConfigurationDoneRequest", json::Value(true) },
            { "supportsEvaluateForHovers", json::Value(false) },
            { "supportsSetVariable", json::Value(false) },
            { "supportsRestartRequest", json::Value(false) },
            { "supportsSteppingGranularity", json::Value(false) },
        });
        sendResponse(id, command, std::move(capabilities));
        sendEvent("initialized");
        return false;
    }
    if (command == "setBreakpoints")
    {
        const json::Value *arguments = message.find("arguments");
        const json::Value *source = arguments != nullptr ? arguments->find("source") : nullptr;
        const std::string path = source != nullptr ? stringField(*source, "path") : std::string();
        std::set<std::int64_t> lines;
        g_sourceBreakpoints[path].clear();
        const json::Value *list = arguments != nullptr ? arguments->find("breakpoints") : nullptr;
        if (list != nullptr && list->isArray())
        {
            const json::Array *items = list->asArray();
            for (const json::Value &item : *items)
            {
                const std::int64_t line = intField(item, "line", 0);
                if (line > 0) g_sourceBreakpoints[path].insert(line);
            }
        }
        refreshBreakpoints();
        /* 回执：每个断点给一条（这里都按"已确认"处理）。 */
        json::Array marks;
        for (const std::int64_t line : g_sourceBreakpoints[path])
            marks.push_back(json::Value::object(
                { { "id", json::Value(line) }, { "verified", json::Value(true) }, { "line", json::Value(line) } }));
        sendResponse(id, command, json::Value::object({ { "breakpoints", json::Value(marks) } }));
        return false;
    }
    if (command == "threads")
    {
        json::Array threads;
        threads.push_back(json::Value::object(
            { { "id", json::Value(kThreadId) }, { "name", json::Value(std::string("main")) } }));
        sendResponse(id, command, json::Value::object({ { "threads", json::Value(threads) } }));
        return false;
    }
    if (command == "stackTrace")
    {
        const json::Value frames = stackFrames();
        sendResponse(id, command,
                     json::Value::object(
                         { { "stackFrames", frames },
                           { "totalFrames",
                             json::Value(static_cast<std::int64_t>(frames.asArray()->size())) } }));
        return false;
    }
    if (command == "scopes" || command == "variables")
    {
        /* 变量读取要 VM 暴露栈帧槽，目前还没有这层 API：先给空列表，
         * 不加这层会让 VS Code 一直等响应。 */
        json::Array empty;
        sendResponse(id, command,
                     json::Value::object({ { command == "scopes" ? "scopes" : "variables",
                                             json::Value(empty) } }));
        return false;
    }
    if (command == "continue")
    {
        sendResponse(id, command,
                     json::Value::object({ { "allThreadsContinued", json::Value(true) } }));
        g_stepKind = StepKind::None;
        action = stackvm::DebugAction::Continue;
        return true;
    }
    if (command == "next" || command == "stepIn" || command == "stepOut")
    {
        sendResponse(id, command);
        g_stepFile = g_stoppedFile;
        g_stepLine = g_stoppedLine;
        g_stepDepth = g_stoppedDepth;
        g_stepKind = command == "next" ? StepKind::Over : command == "stepIn" ? StepKind::Into :
                                                                                StepKind::Out;
        action = stackvm::DebugAction::StepInto;
        return true;
    }
    if (command == "pause")
    {
        sendResponse(id, command);
        if (g_machine != nullptr) g_machine->setSingleStep(true);
        return false;
    }
    if (command == "disconnect" || command == "terminate")
    {
        sendResponse(id, command);
        g_stepKind = StepKind::None;
        action = stackvm::DebugAction::Abort;
        return true;
    }
    if (command.empty()) return false;
    /* 不认识 / 不需要的命令（launch、configurationDone、setExceptionBreakpoints…）：
     * 一律成功回包，别让前端挂住。 */
    sendResponse(id, command);
    return false;
}

/**
 * @brief 单步是否已经走到目标位置。
 * @param event 待处理的界面事件或事件编号。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool stepReached(const stackvm::DebugEvent &event)
{
    if (g_stepKind == StepKind::Into) return true;
    if (g_stepKind == StepKind::Over)
        return event.frames.size() <= g_stepDepth && movedToNewLine(event);
    return event.frames.size() < g_stepDepth; /* StepKind::Out */
}

}  // namespace

/**
 * @brief 挂上即将运行的 VM 与调试表。
 * @param machine 执行用的虚拟机；断点会直接写进它。
 * @param info 组装得到的调试表（指令 → 源码位置）；用于把源码行反查成指令下标。
 */
void attach(stackvm::Machine *machine, const stackvm::DebugInfo *info)
{
    g_machine = machine;
    g_debugInfo = info;
    g_finished = false;
    g_hasStopped = false;
    g_stoppedFrames.clear();
    g_sourceBreakpoints.clear();
    g_instructionBreaks.clear();
    /* 先停在入口：等 VS Code 下断点、发 configurationDone 再放行。 */
    if (machine != nullptr) machine->setSingleStep(true);
}

/**
 * @brief VM 停在断点 / 单步时调用。
 * @param event 本次停下的快照（位置、调用栈、是否断点停）。
 * @return 下一步动作：继续跑到下一断点、单步、或中止。
 */
stackvm::DebugAction onStop(const stackvm::DebugEvent &event)
{
    /* 只把 DAP 要用的字段抄成 `std::` 容器：不要留 `Memory::` 容器过夜，
     * 否则运行时 arena 收尾时会判"仍有活动块"并 abort。 */
    g_stoppedFrames.clear();
    g_stoppedFrames.reserve(event.frames.size());
    for (const stackvm::DebugFrame &frame : event.frames)
    {
        if (isInternalQuad(toStd(frame.location.file))) continue;
        g_stoppedFrames.push_back(StoppedFrame{ toStd(frame.function), toStd(frame.location.file),
                                                frame.location.line });
    }
    g_stoppedFile = toStd(event.location.file);
    g_stoppedLine = event.location.line;
    g_stoppedDepth = event.frames.size();
    g_stoppedSteps = event.steps;
    g_stoppedByBreakpoint = event.breakpoint;
    g_hasStopped = true;

    /* 单步过程：还没到目标就继续一条条走，不惊动前端。 */
    if (g_stepKind != StepKind::None)
    {
        if (!stepReached(event)) return stackvm::DebugAction::StepInto;
        g_stepKind = StepKind::None;
    }

    const std::string reason =
        g_stoppedByBreakpoint ? "breakpoint" : (g_stoppedSteps == 0u ? "entry" : "step");
    sendEvent("stopped", stoppedBody(reason));
    /* 停在断点 / 单步目标上：嵌套读命令，直到拿到一个"下一步动作"。 */
    while (true)
    {
        std::string payload;
        if (!readFrame(payload)) return stackvm::DebugAction::Continue;
        const json::ParseResult parsed = json::parse(payload);
        if (!parsed.ok()) continue;
        stackvm::DebugAction action = stackvm::DebugAction::Continue;
        if (handleMessage(*parsed.value, action)) return action;
    }
}

/**
 * @brief DAP 主循环；`launch` 时调用 `compileAndRun`（编译 + 执行）。
 * @param compileAndRun 编译并执行一次，返回编译器退出码。
 * @return 进程退出码。
 */
int serve(const std::function<int()> &compileAndRun)
{
    ensureBinaryStdio();
    /* 外层循环：initialize / launch / 断点设置都在这里处理。 */
    while (true)
    {
        std::string payload;
        if (!readFrame(payload)) break;
        const json::ParseResult parsed = json::parse(payload);
        if (!parsed.ok()) continue;
        const json::Value &message = *parsed.value;
        const std::string command = stringField(message, "command");
        const json::Value id = requestId(message);

        if (command == "launch")
        {
            sendResponse(id, command);
            const int code = compileAndRun();
            g_finished = true;
            sendEvent("exited", json::Value::object({ { "exitCode", json::Value(code) } }));
            sendEvent("terminated");
            continue;
        }
        if (command == "disconnect")
        {
            sendResponse(id, command);
            break;
        }
        stackvm::DebugAction action = stackvm::DebugAction::Continue;
        handleMessage(message, action);
    }
    return 0;
}

/**
 * @brief 转发程序输出为 `output` 事件。
 * @param text 程序写到标准输出 / 标准错误的内容。
 * @param category DAP 的 category：`stdout` / `stderr` / `console`。
 */
void emitOutput(const std::string &text, const char *category)
{
    if (text.empty()) return;
    sendEvent("output", json::Value::object({ { "category", json::Value(std::string(category)) },
                                              { "output", json::Value(text) } }));
}

/**
 * @brief 程序是否已经跑完（供执行路径判断要不要再停下）。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool finished() { return g_finished; }

}  // namespace dap
