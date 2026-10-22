/**
 * @file runtime_context.hpp
 * @brief VM 内部 runtime context 模块的类型与接口。
 */
#pragma once
#include "runtime_slot.hpp"
#include "execution_plan.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 调用帧：一层调用的局部变量存储 + 返回信息。
 *
 * 每个活动调用独占局部槽和返回信息，递归活动帧互不覆盖。
 * 返回时先写回结果并销毁局部值；执行优化可限量缓存已清空的小帧及其向量容量，
 * 下次调用重新准备布局、声明状态和参数。未缓存的帧由 UniquePtr 自动销毁。
 * 缓存只属于本次 Machine::run，不保留活动帧指针，也不延迟局部值的释放。
 */
struct Frame
{
    const execution_detail::FrameLayout *layout = nullptr;  ///< 本函数的稠密槽布局。
    Memory::Vector<Slot> slots;                             ///< 不按名称查询的局部存储。
    Memory::Vector<bool> bound;                             ///< 保留声明执行前的未定义与全局回退语义。
    Memory::Map<Memory::String, Slot> locals;               ///< 本帧的局部变量（形参 + 函数体里声明的）。
    std::size_t returnIp = 0;                               ///< 返回地址。
#if STACKVM_WITH_PANIC_TRACE
    Memory::String function;  ///< 实际被调函数的符号；压缩产物中为逻辑编号。
#endif
    Memory::String returnDest;  ///< 返回值写回调用方的哪个变量（可空）。
};

/**
 * @brief 待绑定的实参。
 */
struct PendingArg
{
    Memory::String name;  ///< 形参名。
    Slot value;           ///< 值。
};

struct TransferPacket;

/* ---------------------------------------------------------------------------
 *  内部 C++ 运行时：字符串 / 格式化
 * ---------------------------------------------------------------------------
 *
 * 运行时字符串是“8 字节长度 + UTF-8 字节数据”，不附加结尾 NUL；数据区字面量只是
 * 复制源，仍用生成期哨兵界定。**读字节 / 拼接 / 切片 / 格式化这些活儿全在这层 C++ 里干**，宿主语言
 * （chali）只发一次调用 —— 不用在四元式里手写逐字节的循环，也就能顺手用上
 * `Memory::String` / `snprintf`。
 *
 * 与数学原生函数（`kNatives`）的两点不同：
 * 1. 参数与结果都是 **Slot**：地址 / 长度必须落在**整数槽**里（`(load, 地址, …)`
 *    认的是 `Slot.integer`，浮点槽的 `integer` 是 0，塞进去就成空指针了）；
 * 2. 要能用堆（造新串）与数据区（读字面量），还要能报错 —— 所以带一个 NativeContext。
 */

using NativeInvoker = Memory::Function<bool(std::int64_t, const Memory::Vector<NativeValue> &,
                                            NativeValue &)>;

/**
 * @brief 一次 run() 里的软件定时器表（`std::event::Timer` 的后端）。
 */
struct WatchdogRuntime
{
    std::uint64_t period = 0;
    std::uint64_t deadline = 0;
    std::uint64_t now = 0;
};

/**
 * @brief 原生函数跑起来时的环境。
 */
struct NativeContext
{
    HeapAllocator *heap = nullptr;                                     ///< 堆：造出来的字符串放这儿。
    const DataArea *data = nullptr;                                    ///< 数据区：字面量所在的那片静态内存。
    Memory::String *text = nullptr;                                    ///< 文本输出桶（`printf` 往里追加）。
    OutputSink textSink = nullptr;                                     ///< 文本输出下沉（`Machine::setOutputSink`）；空 = 只进桶。
    void *textSinkUser = nullptr;                                      ///< 下沉上下文。
    const bool *textKeep = nullptr;                                    ///< 有下沉时是否仍累积进文本桶；空 = 累积。
    const Memory::Vector<Memory::String> *input = nullptr;             ///< 输入项（按顺序取）。
    std::size_t *inputIndex = nullptr;                                 ///< 已经取到第几项。
    const Memory::Vector<Memory::String> *programArguments = nullptr;  ///< 程序入口参数。
    NativeInvoker *invoker = nullptr;                                  ///< 同步调用 CHALI 函数指针。
    struct ObjectStore *objects = nullptr;                             ///< 运行期通用对象表。
    WatchdogRuntime *watchdog = nullptr;                               ///< 当前执行的内部看门狗。
    Machine *machine = nullptr;                                        ///< 正在执行 run() 的虚拟机。
    std::size_t threadId = 0u;                                         ///< 当前线程 id（定时器归属线程）。
    bool *exitRequested = nullptr;                                     ///< `exit(n)` 请求提前正常停机。
    int *exitCode = nullptr;                                           ///< `exit(n)` 提供的宿主状态码。
    const NativeServices *hostServices = nullptr;                      ///< 通用不透明宿主服务。
    Memory::String error;                                              ///< 非空 = 出错，调用方负责终止运行。
    mutable Memory::String memoryFault;                                ///< 首次非法访问，优先于回调随后生成的普通错误。
};

/**
 * @brief 扫数据区字面量时最多看多少字节（运行时堆字符串直接读取长度）。
 */
constexpr std::size_t kStringScanLimit = 1u << 20;
}  // namespace stackvm::runtimeDetail
