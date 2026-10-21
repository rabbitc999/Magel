/**
 * @file native_context_impl.hpp
 * @brief VM 内部 native context impl 模块的类型与接口。
 */
#pragma once
#include "native_functions.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 把私有运行时环境收窄成公开胶水接口。
 */
class NativeCallContextImpl final : public NativeCallContext
{
public:
    using Invoker = NativeInvoker;

    /**
     * @brief 为原生函数调用关联虚拟机与临时对象集合。
     * @param context 当前处理或执行上下文。
     * @param invoker 调用运行时函数的宿主回调。
     * @param invocationError 运行时回调失败的原始诊断，可为空。
     */
    explicit NativeCallContextImpl(NativeContext &context, Invoker invoker = {}, const Memory::String *invocationError = nullptr);

    /**
     * @brief 将胶水记录装入当前对象表的通用壳。
     * @param type 宿主记录的生命周期描述符。
     * @param record 成功时转移所有权的记录。
     * @return 新句柄；失败返回零。
     */
    std::int64_t makeNativeRecord(const NativeRecordType &type, void *record) override;
    /**
     * @brief 从当前对象表借用身份匹配的宿主记录。
     * @param handle 对象句柄。
     * @param type 预期描述符。
     * @return 匹配记录或空指针。
     */
    void *nativeRecord(std::int64_t handle, const NativeRecordType &type) const override;
    /**
     * @brief 释放当前对象表中的宿主记录壳。
     * @param handle 对象句柄。
     * @return 接受立即或延期释放时为 true；无效句柄或被拒绝时为 false。
     */
    bool releaseNativeRecord(std::int64_t handle) override;
    /**
     * @brief 按原始分配身份释放胶水拥有的内存块。
     * @param address 块首地址。
     * @param soft 是否跳过空地址和失效分配。
     * @return 操作成功时为 true。
     */
    bool releaseMemory(std::int64_t address, bool soft) override;
    /**
     * @brief 调用记录描述符的复制协议。
     * @param handle 待复制句柄。
     * @return 独立副本句柄，失败为零。
     */
    std::int64_t cloneNativeRecord(std::int64_t handle) override;
    /**
     * @brief 返回对象跨副本保持的身份。
     * @param handle 对象句柄。
     * @return 身份；无效句柄返回零。
     */
    std::int64_t objectIdentity(std::int64_t handle) const override;
    /**
     * @brief 登记本次运行结束时清理的临时资源。
     * @param handle 地址或对象句柄。
     * @param object 是否为对象句柄。
     */
    void trackTemporary(std::int64_t handle, bool object) override;
    /**
     * @brief 复制当前宿主入口参数。
     * @return 参数列表；未设置时为空。
     */
    Memory::Vector<Memory::String> programArguments() const override;
    /**
     * @brief 按胶水约定的身份借用不透明宿主服务。
     * @param name 服务身份，VM 不解释其含义。
     * @return 宿主服务指针；未注入时为空。
     */
    void *hostService(const Memory::String &name) const override;
    /**
     * @brief 读取输入源中的下一项文本。
     * @param text 输出文本。
     * @return 取得输入时为 true。
     */
    bool takeInputText(Memory::String &text) override;
    /**
     * @brief 查询输入源已消费的项数。
     * @return 已消费项数。
     */
    std::size_t inputPosition() const override;

    /**
     * @brief 按字节读一段（字符串是"以 `\0` 结尾的字节串"，跨 8 字节槽）。
     * @param address 地址（数据区内的偏移）。
     * @param out 输出缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 越界返回 false。
     */
    bool readBytes(std::int64_t address, char *out, std::size_t count) const override;

    /**
     * @brief 将字节数据写入指定地址。
     * @param address 运行时内存地址。
     * @param data 输入或待写入的字节数据。
     * @param count 元素数量或操作次数。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool writeBytes(std::int64_t address, const char *data, std::size_t count) override;

    /**
     * @brief 在运行时内存中创建字符串并返回其原生值。
     * @param text 待处理的文本。
     * @retval 在运行时内存中创建字符串并返回其原生值。
     */
    std::int64_t makeString(const Memory::String &text) override;

    /**
     * @brief 从原生值对应的地址读取字符串。
     * @param address 运行时内存地址。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool readString(std::int64_t address, Memory::String &out) const override;

    /**
     * @brief 通过当前虚拟机调用指定函数。
     * @param functionPointer 被调用函数的运行时地址。
     * @param arguments 按调用顺序保存的实参列表。
     * @param result 接收处理结果的对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool invoke(std::int64_t functionPointer, const Memory::Vector<NativeValue> &arguments,
                NativeValue &result) override;

    std::int64_t makeTemporary(std::size_t bytes) override;

    /**
     * @brief 报告错误；panic 会请求整个线程组停止。
     * @param message 错误信息。
     */
    void fail(const Memory::String &message) override;

    /**
     * @brief 返回弱引用事件投递器，不延长本次执行的生命周期。
     * @return 支持时返回线程安全投递器。
     */
    EventPoster eventPoster() override;

    /**
     * @brief 为 VM 安装执行钩子。
     * @param interval 间隔。
     * @param hook 钩子。
     * @return 是否支持。
     */
    bool setInstructionHook(std::size_t interval, InstructionHook hook) override;

    /**
     * @brief 当前正在执行 `run()` 的虚拟机；宿主回调之外（指令钩子 / Qt 事件循环）也能用。
     *
     * 胶水拿到它以后可以在**宿主回调返回之后**继续工作：投递显式事件
     * （`eventPoster()`）或挂每 N 条指令一次的钩子
     * （`Machine::setInstructionHook`）。只在 VM 执行线程上使用；指针在 `Machine`
     * 生命周期内有效，`run()` 结束后不要再碰。
     * @return 正在运行的虚拟机；没有（例如离线编解码）返回 nullptr。
     */
    Machine *machine() override;

private:
    NativeContext &context_;
    Invoker invoker_;
    const Memory::String *invocationError_ = nullptr;
};
}  // namespace stackvm::runtimeDetail
