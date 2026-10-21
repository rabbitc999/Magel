/**
 * @file native_context.cpp
 * @brief 同步原生调用上下文与重入适配。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 为原生函数调用关联虚拟机与临时对象集合。
 * @param context 当前处理或执行上下文。
 * @param invoker 调用运行时函数的宿主回调。
 * @param invocationError 运行时回调失败的原始诊断，可为空。
 */
NativeCallContextImpl::NativeCallContextImpl(NativeContext &context, Invoker invoker, const Memory::String *invocationError) : context_(context), invoker_(std::move(invoker)), invocationError_(invocationError)
{
}

/**
 * @brief 复制当前宿主入口参数。
 * @return 参数列表；未设置时为空。
 */
Memory::Vector<Memory::String> NativeCallContextImpl::programArguments() const
{ return context_.programArguments ? *context_.programArguments : Memory::Vector<Memory::String>{}; }

/**
 * @brief 按胶水约定的身份借用不透明宿主服务。
 * @param name 服务身份，VM 不解释其含义。
 * @return 宿主服务指针；未注入时为空。
 */
void *NativeCallContextImpl::hostService(const Memory::String &name) const
{
    if (!context_.hostServices) return nullptr;
    const auto found = context_.hostServices->find(name);
    return found == context_.hostServices->end() ? nullptr : found->second;
}

/**
 * @brief 查询输入源已消费的项数。
 * @return 已消费项数。
 */
std::size_t NativeCallContextImpl::inputPosition() const
{ return context_.inputIndex ? *context_.inputIndex : 0u; }

/**
 * @brief 按字节读一段（字符串是"以 `\0` 结尾的字节串"，跨 8 字节槽）。
 * @param address 地址（数据区内的偏移）。
 * @param out 输出缓冲（至少 count 字节）。
 * @param count 字节数。
 * @return 越界返回 false。
 */
bool NativeCallContextImpl::readBytes(std::int64_t address, char *out, std::size_t count) const
{
    return address >= 0 && nativeRead(context_, static_cast<std::size_t>(address), out, count);
}

/**
 * @brief 将字节数据写入指定地址。
 * @param address 运行时内存地址。
 * @param data 输入或待写入的字节数据。
 * @param count 元素数量或操作次数。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::writeBytes(std::int64_t address, const char *data, std::size_t count)
{
    return address >= 0 && nativeWrite(context_, static_cast<std::size_t>(address), data, count);
}

/**
 * @brief 在运行时内存中创建字符串并返回其原生值。
 * @param text 待处理的文本。
 * @retval 在运行时内存中创建字符串并返回其原生值。
 */
std::int64_t NativeCallContextImpl::makeString(const Memory::String &text)
{
    return static_cast<std::int64_t>(nativeAllocString(context_, text));
}

/**
 * @brief 从原生值对应的地址读取字符串。
 * @param address 运行时内存地址。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::readString(std::int64_t address, Memory::String &out) const
{
    return address >= 0 && nativeReadCString(context_, static_cast<std::size_t>(address), &out);
}

/**
 * @brief 通过当前虚拟机调用指定函数。
 * @param functionPointer 被调用函数的运行时地址。
 * @param arguments 按调用顺序保存的实参列表。
 * @param result 接收处理结果的对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeCallContextImpl::invoke(std::int64_t functionPointer, const Memory::Vector<NativeValue> &arguments,
                                   NativeValue &result)
{
    if (invoker_ && invoker_(functionPointer, arguments, result)) return true;
    if (context_.error.empty())
        context_.error = invocationError_ && !invocationError_->empty() ? *invocationError_ : "当前运行时无法调用指定函数指针";
    return false;
}

/**
 * @brief 报告错误；panic 会请求整个线程组停止。
 * @param message 错误信息。
 */
void NativeCallContextImpl::fail(const Memory::String &message)
{ context_.error = message; }

/**
 * @brief 返回弱引用事件投递器，不延长本次执行的生命周期。
 * @return 支持时返回线程安全投递器。
 */
EventPoster NativeCallContextImpl::eventPoster()
{
#if STACKVM_WITH_THREADS
    if (context_.machine) return context_.machine->eventPoster();
#endif
    return {};
}

/**
 * @brief 为 VM 安装执行钩子。
 * @param interval 间隔。
 * @param hook 钩子。
 * @return 是否支持。
 */
bool NativeCallContextImpl::setInstructionHook(std::size_t interval, InstructionHook hook)
{
    if (context_.machine)
    {
        context_.machine->setInstructionHook(interval, std::move(hook));
        return true;
    }
    return false;
}

/**
 * @brief 当前正在执行 `run()` 的虚拟机；宿主回调之外（指令钩子 / Qt 事件循环）也能用。
 *
 * 胶水拿到它以后可以在**宿主回调返回之后**继续工作：投递显式事件
 * （`eventPoster()`）或挂每 N 条指令一次的钩子
 * （`Machine::setInstructionHook`）。只在 VM 执行线程上使用；指针在 `Machine`
 * 生命周期内有效，`run()` 结束后不要再碰。
 * @return 正在运行的虚拟机；没有（例如离线编解码）返回 nullptr。
 */
Machine *NativeCallContextImpl::machine()
{ return context_.machine; }
}  // namespace runtimeDetail
}  // namespace stackvm
