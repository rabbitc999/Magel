/**
 * @file vm_machine.hpp
 * @brief 虚拟机宿主配置与执行入口。
 */
#pragma once
#include "native_protocol.hpp"
#include "data_area.hpp"
#include "quad_optimizer.hpp"
#include <optional>

namespace stackvm
{
namespace runtimeDetail
{
struct ConcurrentCallState;
struct TransferPacket;
}  // namespace runtimeDetail
/**
 * @brief 共享 VM 的宿主配置、调试与执行入口。
 */
class Machine
{
public:
    /**
     * @brief 从任意宿主线程请求本虚拟机协作停止，在下一安全点生效。
     */
    void requestStop();
#if STACKVM_WITH_THREADS
    /**
     * @brief 取得当前运行的有界宿主事件投递器，运行结束后返回关闭状态。
     * @return 不捕获虚拟机裸地址的线程安全投递器。
     */
    EventPoster eventPoster();
#endif

    /**
     * @brief 构造。
     * @param trace 是否记录执行轨迹。
     */
    explicit Machine(bool trace = false);

    /**
     * @brief 挂上数据区；不挂时 `AddrOf` / `LoadAt` / `StoreAt` 会报"没有数据区"。
     * @param data 数据区（生命周期由调用方保证）。
     */
    void setData(DataArea *data) { data_ = data; }

    /**
     * @brief 挂上同步宿主调用注册表，注册表生命周期须覆盖运行。
     * @param registry 注册表；空指针取消注入。
     */
    void setNativeRegistry(const NativeRegistry *registry) { nativeRegistry_ = registry; }

    /**
     * @brief 挂上自定义字节堆，每次运行前重置分配状态。
     * @param allocator 分配器；空指针使用默认堆。
     */
    void setHeapAllocator(HeapAllocator *allocator) { heapAllocator_ = allocator; }

#if STACKVM_WITH_THREADS
    /**
     * @brief 为隔离子线程创建独立字节堆。
     * @param factory 继承到子线程的堆工厂。
     */
    void setThreadHeapFactory(Memory::Function<Memory::UniquePtr<HeapAllocator>()> factory)
    {
        threadHeapFactory_ = std::move(factory);
    }
#endif

    /**
     * @brief 限制每个执行上下文的求值栈槽数。
     * @param slots 槽数上限；零表示不限制。
     */
    void setStackLimit(std::size_t slots) { stackLimit_ = slots; }
    /**
     * @brief 选择新寄存器核心；关闭后保留原有优化执行器用于对照。
     * @param enabled 是否启用。
     */
    void setRegisterExecution(bool enabled) { registerExecution_ = enabled; }
    /**
     * @brief 开关寄存器跳转折叠，便于差分和性能对照。
     * @param enabled 是否启用。
     */
    void setRegisterJumpFolding(bool enabled) { registerJumpFolding_ = enabled; }
    /**
     * @brief 开关整数专用操作码。
     * @param enabled 是否启用。
     */
    void setRegisterSpecialization(bool enabled) { registerSpecialization_ = enabled; }
    /**
     * @brief 开关基本块批量检查与计数。
     * @param enabled 是否启用。
     */
    void setRegisterBlockExecution(bool enabled) { registerBlockExecution_ = enabled; }
    /**
     * @brief 控制整数内存、下标、参数快速路径与已返回调用帧复用。
     * @param enabled 是否启用。
     */
    void setRegisterServices(bool enabled) { registerServices_ = enabled; }
    /**
     * @brief 选择标签地址分派；不支持的编译器使用 switch。
     * @param enabled 是否启用。
     */
    void setRegisterThreadedDispatch(bool enabled) { registerThreadedDispatch_ = enabled; }
    /**
     * @brief 开关执行准备与融合，关闭时用于逐指令对照验证。
     * @param enabled 是否启用。
     */
    void setExecutionOptimization(bool enabled) { optimizeExecution_ = enabled; }

    /**
     * @brief 查询求值栈槽数限制。
     * @return 槽数上限；零表示不限制。
     */
    [[nodiscard]] std::size_t stackLimit() const { return stackLimit_; }

    /**
     * @brief 设置胶水可访问的通用宿主服务表。
     * @param services 服务表；空指针表示没有注入服务。
     * @note 调用方保证服务表及其记录覆盖当前运行时和子线程的生命周期。
     */
    void setHostServices(const NativeServices *services) { hostServices_ = services; }

    /**
     * @brief 设置入口参数，运行期间复制进 VM。
     * @param arguments 按原顺序传递给程序的文本参数。
     */
    void setProgramArguments(Memory::Vector<Memory::String> arguments)
    {
        programArguments_ = std::move(arguments);
    }

    /**
     * @brief 挂上文本输出下沉：`print` / `println` / `putChar` 的文本**实时**推给宿主。
     *
     * 目标宿主接串口 / OLED / RTT 只需在这里写一次；`keepText=false` 时不再往
     * `RunResult::text` 累积（省 RAM，也不用等 `run()` 结束才看到输出）。
     * 不挂下沉（`sink == nullptr`）＝桌面行为：文本只进 `RunResult::text`。
     * @param sink 下沉；空 = 只累积。
     * @param user 上下文。
     * @param keepText 是否同时累积进 `RunResult::text`。
     */
    void setOutputSink(OutputSink sink, void *user = nullptr, bool keepText = true)
    {
        outputSink_ = sink;
        outputUser_ = user;
        outputKeepText_ = keepText;
    }

#if STACKVM_WITH_THREADS
    /**
     * @brief 允许程序创建独立 VM 线程。
     * @param enabled 是否允许创建线程。
     */
    void setThreadsEnabled(bool enabled) { threadsEnabled_ = enabled; }
#endif

    /**
     * @brief 注入单调纳秒时钟，供 VM 精确计时。
     * @param callback 时钟回调；空回调禁用精确计时。
     */
    void setTimeCallback(TimeCallback callback) { timeCallback_ = std::move(callback); }

    /**
     * @brief 按指令间隔同步调用宿主钩子。
     * @param interval 指令间隔；零表示禁用。
     * @param hook 在当前执行线程调用的钩子，不得重入同一机器。
     */
    void setInstructionHook(std::size_t interval, InstructionHook hook)
    {
        hookInterval_ = interval;
        instructionHook_ = std::move(hook);
    }

    /**
     * @brief 设置断点及单步停机时的同步回调。
     * @param hook 调试动作回调；空回调禁用调试。
     */
    void setDebugHook(DebugHook hook) { debugHook_ = std::move(hook); }

    /**
     * @brief 设置断点集合（指令下标）。
     * @param instructions 断点位置；重复项会被忽略，空表等于清空。
     */
    void setBreakpoints(Memory::Vector<std::size_t> instructions);

    /**
     * @brief 清空全部指令断点。
     */
    void clearBreakpoints() { breakpoints_.clear(); }

    /**
     * @brief 查询当前指令断点数量。
     * @return 去重后的断点数量。
     */
    [[nodiscard]] std::size_t breakpointCount() const { return breakpoints_.size(); }

    /**
     * @brief 设置逐指令调试。
     * @param enabled 是否在每条指令前触发调试回调。
     */
    void setSingleStep(bool enabled) { singleStep_ = enabled; }

    /**
     * @brief 查询逐指令调试开关。
     * @return 开启单步时返回真。
     */
    [[nodiscard]] bool singleStep() const { return singleStep_; }

    /**
     * @brief 临时覆盖程序的调试表。
     * @param info 需覆盖运行生命周期的表；空指针恢复程序自带表。
     */
    void attachDebugInfo(const DebugInfo *info) { debugOverride_ = info; }

    /**
     * @brief 取得当前运行生效的调试表。
     * @return 有效调试表；无表时为空指针。
     */
    [[nodiscard]] const DebugInfo *debugInfo() const { return debugInfo_; }

    /**
     * @brief 控制错误详情；关闭后不记录路径或调用栈，压缩容器始终关闭。
     * @param enabled 是否允许。
     */
    void setPanicDetailsEnabled(bool enabled) { panicDetailsEnabled_ = enabled; }

    /**
     * @brief 取得最近一次调试回调或停机位置。
     * @return 原始源码文件、行列与指令位置。
     */
    [[nodiscard]] SourceLocation currentLocation() const { return currentLocation_; }

    /**
     * @brief 取得最近一次调试回调或停机的调用栈。
     * @return 从外层到内层排列的源码帧快照。
     */
    [[nodiscard]] Memory::Vector<DebugFrame> callStack() const { return debugFrames_; }

    /**
     * @brief 执行一个组装好的程序。
     * @param program 程序。
     * @param input `read` / `getc` 的输入项（按顺序取）。
     * @param stepLimit 指令条数上限，防止死循环；传 `0` 表示不设上限（缺省为 0）。
     * @return 执行结果。
     */
    [[nodiscard]] RunResult run(const Program &program,
                                const Memory::Vector<Memory::String> &input = {},
                                std::size_t stepLimit = 0u);

private:
    /**
     * @brief 处理复制载荷、有界通道、原子槽、事件和任务控制服务。
     * @param state 本次运行的借用状态与同步操作。
     * @param name 已解析的原生服务名称。
     * @param returnDest 接收结果的变量名称。
     * @param startupPacket 跨模块传递已复制的任务初始参数。
     * @param typedCreate 是否为带初始参数的任务创建。
     * @return 已处理时返回原调用结果，未匹配时返回空。
     */
    std::optional<bool> callTransferService(runtimeDetail::ConcurrentCallState &state, const Memory::String &name, const Memory::String &returnDest, std::shared_ptr<runtimeDetail::TransferPacket> &startupPacket, bool typedCreate);
    /**
     * @brief 处理类型化线程邮箱及共享值的复制交付。
     * @param state 本次运行的借用状态与同步操作。
     * @param name 已解析的原生服务名称。
     * @param returnDest 接收结果的变量名称。
     * @param startupPacket 跨模块传递已复制的任务初始参数。
     * @param typedCreate 是否为带初始参数的任务创建。
     * @return 已处理时返回原调用结果，未匹配时返回空。
     */
    std::optional<bool> callMailboxService(runtimeDetail::ConcurrentCallState &state, const Memory::String &name, const Memory::String &returnDest, std::shared_ptr<runtimeDetail::TransferPacket> &startupPacket, bool typedCreate);
    /**
     * @brief 处理隔离任务创建、等待、字符串邮箱和完成回收。
     * @param state 本次运行的借用状态与同步操作。
     * @param name 已解析的原生服务名称。
     * @param returnDest 接收结果的变量名称。
     * @param startupPacket 跨模块传递已复制的任务初始参数。
     * @param typedCreate 是否为带初始参数的任务创建。
     * @return 已处理时返回原调用结果，未匹配时返回空。
     */
    std::optional<bool> callThreadService(runtimeDetail::ConcurrentCallState &state, const Memory::String &name, const Memory::String &returnDest, std::shared_ptr<runtimeDetail::TransferPacket> &startupPacket, bool typedCreate);

    std::shared_ptr<ExecutionControl> control_;
#if STACKVM_WITH_THREADS
    std::shared_ptr<EventQueue> events_;
#endif

    bool panicDetailsEnabled_ = true;                 ///< 错误详情策略，不能覆盖压缩容器的隐私限制。
    bool trace_ = false;                              ///< 是否记录轨迹。
    OutputSink outputSink_ = nullptr;                 ///< 文本输出下沉；空 = 只累积进 `RunResult::text`。
    void *outputUser_ = nullptr;                      ///< 输出下沉的上下文。
    bool outputKeepText_ = true;                      ///< 有下沉时是否仍累积进 `RunResult::text`。
    DataArea *data_ = nullptr;                        ///< 可选的数据区。
    const NativeRegistry *nativeRegistry_ = nullptr;  ///< 可选的 C++ 胶水函数表。
    HeapAllocator *heapAllocator_ = nullptr;          ///< 可选的自定义 VM 字节堆。
#if STACKVM_WITH_THREADS
    Memory::Function<Memory::UniquePtr<HeapAllocator>()> threadHeapFactory_;
#endif
    bool registerExecution_ = true;  ///< 默认使用新寄存器分派核心。
    bool registerServices_ = true;
    bool registerJumpFolding_ = true, registerSpecialization_ = true, registerBlockExecution_ = true;
    bool registerThreadedDispatch_ = true;             ///< 支持的编译器默认使用标签分派，其余回退 switch。
    bool optimizeExecution_ = true;                    ///< 不改变字节码的运行期优化开关。
    std::size_t stackLimit_ = 0u;                      ///< 每个执行上下文的求值栈槽数上限；0 表示不限制。
    const NativeServices *hostServices_ = nullptr;     ///< 宿主注入的通用不透明服务。
    Memory::Vector<Memory::String> programArguments_;  ///< 传给语言入口的命令行参数。
#if STACKVM_WITH_THREADS
    bool threadsEnabled_ = false;
    std::shared_ptr<ThreadRuntime> threadRuntime_;
    std::shared_ptr<SharedRuntime> sharedRuntime_;
#endif
    std::size_t threadId_ = 0u;
    std::size_t parentThreadId_ = 0u;
    Memory::String entryLabel_;
    TimeCallback timeCallback_;                 ///< 可选的宿主单调纳秒时钟。
    std::size_t hookInterval_ = 0;              ///< 指令钩子间隔，0 表示禁用。
    InstructionHook instructionHook_;           ///< 可选的同步指令钩子。
    DebugHook debugHook_;                       ///< 可选的调试回调。
    Memory::Set<std::size_t> breakpoints_;      ///< 断点：指令下标集合。
    bool singleStep_ = false;                   ///< 单步开关。
    const DebugInfo *debugOverride_ = nullptr;  ///< 调用方临时指定的调试表。
    const DebugInfo *debugInfo_ = nullptr;      ///< 本次运行生效的调试表。
    SourceLocation currentLocation_;            ///< 最近一次回调 / 停机时刻的位置。
    Memory::Vector<DebugFrame> debugFrames_;    ///< 最近一次回调 / 停机时刻的调用栈快照。
};
}  // namespace stackvm
