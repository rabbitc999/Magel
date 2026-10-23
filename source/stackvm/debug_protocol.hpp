/**
 * @file debug_protocol.hpp
 * @brief 执行快照、调试回调与完成结果。
 */
#pragma once
#include "program_model.hpp"

namespace stackvm
{
/**
 * @brief 指令钩子收到的只读执行快照。
 */
struct InstructionHookInfo
{
    std::size_t steps = 0;        ///< 本次 run() 已完成的指令总数。
    std::size_t instruction = 0;  ///< 刚执行完的程序计数器。
    Memory::String opcode;        ///< 指令操作码。
};

using TimeCallback = Memory::Function<std::uint64_t()>;
using InstructionHook = Memory::Function<void(const InstructionHookInfo &)>;

/**
 * @brief 文本输出下沉：`print` / `println` / `putChar` 产出的字节被**实时**交给它。
 *
 * 目标宿主把串口 / OLED / RTT 的写函数挂上来（见 `Machine::setOutputSink`），就不必等
 * `run()` 结束才看到输出；不挂时输出只累积进 `RunResult::text`（桌面行为不变）。
 * @param user 挂下沉时给的上下文。
 * @param bytes 字节。
 * @param count 字节数。
 */
using OutputSink = void (*)(void *user, const char *bytes, std::size_t count);

/**
 * @brief 调试回调请 VM 接下来怎么做。
 */
enum class DebugAction
{
    Continue,  ///< 继续跑到下一个断点（清掉单步状态）。
    StepInto,  ///< 单步：下一条指令执行前再回调一次。
    Abort,     ///< 中止本次运行；结果里给出中止位置与源码级调用栈。
};

/**
 * @brief 调用栈里的一帧（调试用）。
 */
struct DebugFrame
{
    Memory::String function;      ///< 函数 / 标签名；空表示根。
    std::size_t threadId = 0;     ///< 发生调用的隔离线程编号；0 为主线程。
    std::size_t instruction = 0;  ///< VM 调用点/当前指令；没有源码表也有效。
    SourceLocation location;      ///< 该帧的位置：被调函数是"当前指令"，调用者记"调用点"。
};

/**
 * @brief 调试回调收到的只读快照。
 */
struct DebugEvent
{
    std::size_t instruction = 0;        ///< **即将执行**的指令下标。
    std::size_t steps = 0;              ///< 本次 run 已经执行过的指令数。
    Memory::String opcode;              ///< 指令操作码文本。
    SourceLocation location;            ///< 该指令的源码位置。
    Memory::Vector<DebugFrame> frames;  ///< 调用栈，从外到内；最后一帧是当前函数。
    bool breakpoint = false;            ///< 是断点停的（true）还是单步停的（false）。
};

using DebugHook = Memory::Function<DebugAction(const DebugEvent &)>;

/**
 * @brief 执行结果。
 */
struct RunResult
{
    bool ok = false;         ///< 是否正常停机。
    bool cancelled = false;  ///< 是否响应协作停止而结束。
    /**
     * @brief 是不是"可恢复终止"（分配失败等来自 VM 外部的停止原因）。
     *
     * 目标宿主没有异常：分配失败在失败点只写标志（`Memory::restartSignal()`），VM 在
     * 下一条指令边界（安全点）看到它就停在这里并把本轮标成可恢复。宿主据此清标志、
     * 重置 arena（`Memory::BufferRuntimeArena::resetForRestart()` / `mcuMetadataResetForRestart()`）
     * 后重跑——这就是软重启触发器之一；`ok == false` 的普通运行错误不带这个标记。
     */
    bool recoverable = false;
    /**
     * @brief 可恢复终止的原因（`Memory::WatchdogReason` 的整数值）；不可恢复时为 0。
     */
    int recoverableReason = 0;
    int exitCode = 0;                       ///< 正常停机状态码（`exit(n)`；普通 halt 为 0）。
    Memory::String output;                  ///< `write` 的数值输出（多个值之间一个空格）。
    Memory::String text;                    ///< `print` / `putc` 的文本输出（按顺序拼接）。
    Memory::String error;                   ///< 运行时错误（越界、未定义变量等）。
    std::size_t registerDispatches = 0;     ///< 新寄存器核心实际执行的指令数。
    std::size_t registerEntries = 0;        ///< 新寄存器核心进入次数。
    std::size_t fusedGroups = 0;            ///< 执行的四指令融合组数；steps 仍按原始指令计数。
    std::size_t steps = 0;                  ///< 执行的指令条数。
    bool timingAvailable = false;           ///< 是否由宿主时钟提供了精确计时。
    std::uint64_t startedNanoseconds = 0;   ///< 开始时的宿主单调时钟值。
    std::uint64_t finishedNanoseconds = 0;  ///< 结束时的宿主单调时钟值。
    std::uint64_t elapsedNanoseconds = 0;   ///< 本次执行耗时；时钟倒退时保守记 0。
    std::size_t maxStackDepth = 0;          ///< 求值栈的最大深度。
    std::size_t heapPeakBytes = 0;          ///< 堆上同时在用字节数的峰值。
    std::size_t heapLiveBytes = 0;          ///< 停机时还有多少字节没 `free`（≠ 0 就是漏了）。
    std::size_t heapLiveBlocks = 0;         ///< 停机时还挂着几块（配合 `heapLiveBytes` 看）。
    std::size_t liveObjects = 0;            ///< 停机时还有几个运行期对象（`bint` / 容器 / `obj` / 资源句柄）没释放。
    Memory::Vector<Memory::String> trace;   ///< 打开 trace 时每条指令一行。
    SourceLocation failureLocation;         ///< 出错（或被调试器中止）处对应的源码位置。
    /**
     * 出错处的源码级调用栈，从外到内；最后一帧是出错函数。
     * STACKVM_WITH_PANIC_TRACE=1 时自动记录；无调试表则提供函数符号及指令位置。
     */
    Memory::Vector<DebugFrame> sourceTrace;
};
}  // namespace stackvm
