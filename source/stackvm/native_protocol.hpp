/**
 * @file native_protocol.hpp
 * @brief 同步原生调用、资源记录和注册表协议。
 */
#pragma once
#include "heap_allocator.hpp"
#include "debug_protocol.hpp"

namespace stackvm
{
/**
 * @brief C++ 胶水函数与虚拟机交换的单槽值。字符串、指针和宿主句柄使用整数地址/句柄。
 */
struct NativeValue
{
    enum class Kind
    {
        Integer,
        Floating,
        Text
    } kind = Kind::Integer;
    std::int64_t integer = 0;
    double floating = 0.0;
    Memory::String text;

    /**
     * @brief 构造整数参数槽。
     * @param value 整数值。
     * @return 保存该整数的参数槽。
     */
    static NativeValue fromInt(std::int64_t value);
    /**
     * @brief 构造浮点参数槽。
     * @param value 双精度值。
     * @return 保存该浮点数的参数槽。
     */
    static NativeValue fromFloat(double value);
    /**
     * @brief 构造宿主文本参数槽。
     * @param value 待传递的 UTF-8 文本。
     * @return 拥有该文本的参数槽。
     */
    static NativeValue fromText(Memory::String value);
    /**
     * @brief 判断参数槽是否保存浮点数。
     * @return 浮点槽返回真。
     */
    [[nodiscard]] bool isFloat() const { return kind == Kind::Floating; }
    /**
     * @brief 判断参数槽是否保存宿主文本。
     * @return 文本槽返回真。
     */
    [[nodiscard]] bool isText() const { return kind == Kind::Text; }
    /**
     * @brief 按整数方式读取参数槽。
     * @return 整数值；浮点槽转换为整数，文本槽返回零。
     */
    [[nodiscard]] std::int64_t asInt() const;
    /**
     * @brief 按浮点方式读取参数槽。
     * @return 浮点值；整数槽转换为浮点，文本槽返回零。
     */
    [[nodiscard]] double asFloat() const;
};

/**
 * @brief 宿主记录与动态值共享的槽快照，递归保存聚合字段与拥有型句柄。
 */
struct NativeStoredValue
{
    bool isStruct = false;
    bool isText = false;
    bool isFloat = false;
    bool ownsObject = false;
    std::int64_t integer = 0;
    std::int64_t objectIdentity = 0;
    double real = 0.0;
    Memory::String text;
    Memory::Vector<NativeStoredValue> fields;
};

class NativeCallContext;
/**
 * @brief 宿主记录提供的通用文本视图，分隔符和字段内容由宿主决定。
 */
struct NativeRecordTextView
{
    Memory::String prefix, suffix, separator = ", ", fieldSeparator = ": ";
    Memory::Vector<Memory::Vector<NativeStoredValue>> rows;
};

/**
 * @brief 宿主记录对释放请求的通用处理结果。
 */
enum class NativeRecordRelease
{
    Immediate,  ///< 可立即清理并销毁记录。
    Deferred,   ///< 已接受请求，由宿主记录在借用结束后重新请求释放。
    Rejected,   ///< 当前操作期间拒绝释放，调用方仍拥有记录。
};

/**
 * @brief 不透明宿主记录的生命周期协议，VM 不解释记录布局。
 * @note 描述符须比关联运行时活得更久；回调不得抛出异常。
 */
struct NativeRecordType
{
    const char *name = nullptr;                                                             ///< 诊断名称，不参与语义分派。
    void *(*clone)(const void *, Memory::String &) = nullptr;                               ///< 复制不访问运行时的记录快照；与 copy 均为空时不可复制。
    void (*destroy)(void *) = nullptr;                                                      ///< 销毁一条记录，不负责 VM 壳。
    Memory::String (*toString)(const void *) = nullptr;                                     ///< 可选的宿主值文本，不解释记录布局。
    void *(*copy)(NativeCallContext &, const void *) = nullptr;                             ///< 使用目标运行时深复制拥有值。
    void (*dispose)(NativeCallContext &, void *) = nullptr;                                 ///< 壳释放前清理运行时资源，不销毁记录本身。
    bool (*prepare)(NativeCallContext &, void *) = nullptr;                                 ///< 读取或复制前同步借用存储。
    NativeRecordRelease (*releaseDecision)(void *) = nullptr;                               ///< 区分立即释放、已接受的延期释放与拒绝释放。
    bool (*visit)(void *, const Memory::Function<bool(NativeStoredValue &)> &) = nullptr;   ///< 遍历可跨运行时转移的拥有值。
    bool (*textView)(NativeCallContext &, const void *, NativeRecordTextView &) = nullptr;  ///< 提供结构化文本字段。
    const char *identityKey = nullptr;                                                      ///< 可选的跨动态库布局身份；非空时同键描述符必须遵守同一记录 ABI。
};

/**
 * @brief 宿主服务身份到不透明适配器的映射；VM 不解释服务名称与对象布局。
 */
using NativeServices = Memory::Map<Memory::String, void *>;
/**
 * @brief 一次 C++ 胶水调用可访问的 VM 上下文，仅在回调期间有效。
 */
class NativeCallContext
{
public:
    /**
     * @brief 释放本次宿主调用的上下文接口。
     */
    virtual ~NativeCallContext() = default;
    /**
     * @brief 为宿主不透明记录创建由当前运行时管理的通用壳。
     * @param type 记录的类型标识及复制、销毁协议。
     * @param record 胶水拥有的记录指针，成功时所有权交给运行时。
     * @return 新句柄；失败返回零且调用方仍负责销毁记录。
     * @note 与销毁通知不同，记录指针和复制回调均保存在壳内，复制壳必调用 clone。
     */
    virtual std::int64_t makeNativeRecord(const NativeRecordType &type, void *record);
    /**
     * @brief 校验句柄存活及类型身份后借用宿主记录。
     * @param handle 同一运行时的对象句柄。
     * @param type 创建记录时的描述符，或具有相同非空 identityKey 与记录布局的描述符。
     * @return 匹配的记录指针；已释放或类型不同返回空。
     * @note 返回值只供胶水使用，VM 不解引用其内容。
     */
    [[nodiscard]] virtual void *nativeRecord(std::int64_t handle, const NativeRecordType &type) const;
    /**
     * @brief 显式释放一个通用运行时对象及其宿主记录。
     * @param handle 当前运行时的句柄。
     * @return 接受立即或延期释放时为 true；句柄无效或请求被拒绝时为 false。
     */
    virtual bool releaseNativeRecord(std::int64_t handle);
    /**
     * @brief 释放胶水通过运行时分配的原始内存块。
     * @param address 分配块的起始地址。
     * @param soft 是否跳过零地址和不属于堆的数据区地址；失效堆身份仍报错。
     * @return 释放成功或软释放跳过时返回 true，失败保留原始诊断。
     */
    virtual bool releaseMemory(std::int64_t address, bool soft = false);
    /**
     * @brief 通过壳内的宿主复制回调生成独立记录。
     * @param handle 待复制的存活句柄。
     * @return 新句柄；不可复制或回调失败时返回零并保存诊断。
     */
    virtual std::int64_t cloneNativeRecord(std::int64_t handle);
    /**
     * @brief 查询拥有型对象跨值复制保持不变的身份。
     * @param handle 当前运行时的存活对象句柄。
     * @return 身份编号；句柄无效返回零。
     */
    virtual std::int64_t objectIdentity(std::int64_t handle) const
    {
        (void)handle;
        return 0;
    }
    /**
     * @brief 将临时拥有对象或内存登记到本次运行结束时的清理队列。
     * @param handle 临时对象句柄或内存地址。
     * @param object true 表示对象句柄，false 表示内存地址。
     */
    virtual void trackTemporary(std::int64_t handle, bool object)
    {
        (void)handle;
        (void)object;
    }
    /**
     * @brief 读取宿主传入的程序入口参数副本。
     * @return 入口参数；未设置时为空。
     */
    virtual Memory::Vector<Memory::String> programArguments() const { return {}; }
    /**
     * @brief 查询宿主注入的通用不透明服务。
     * @param name 胶水约定的服务身份，VM 不解释该名称。
     * @return 注入的指针；服务不存在时返回空指针。
     */
    [[nodiscard]] virtual void *hostService(const Memory::String &name) const;
    /**
     * @brief 从当前输入源读取下一项文本。
     * @param text 成功时接收文本。
     * @return 读取成功时为 true，输入耗尽时为 false。
     */
    virtual bool takeInputText(Memory::String &text);
    /**
     * @brief 查询当前输入源已经消费的项数。
     * @return 已消费项数，用于胶水诊断。
     */
    [[nodiscard]] virtual std::size_t inputPosition() const;
    /**
     * @brief 从当前 VM 的存活地址读取原始字节。
     * @param address VM 逻辑地址。
     * @param out 宿主接收缓冲区。
     * @param count 读取字节数。
     * @return 完整区间读取成功时为真。
     */
    [[nodiscard]] virtual bool readBytes(std::int64_t address, char *out,
                                         std::size_t count) const = 0;
    /**
     * @brief 向当前 VM 的存活地址写入原始字节。
     * @param address VM 逻辑地址。
     * @param data 宿主源缓冲区。
     * @param count 写入字节数。
     * @return 完整区间写入成功时为真。
     */
    virtual bool writeBytes(std::int64_t address, const char *data, std::size_t count) = 0;
    /**
     * @brief 读取以零结尾的 VM 字节文本。
     * @param address 首字节的 VM 逻辑地址。
     * @param out 接收 UTF-8 字节文本。
     * @return 完整字符串可读时为真。
     */
    [[nodiscard]] virtual bool readString(std::int64_t address, Memory::String &out) const = 0;
    /**
     * @brief 在当前 VM 运行时中同步调用 ChaLi 函数或捕获闭包。
     * @param functionPointer 同一运行时的函数地址或闭包句柄。
     * @param arguments 按声明顺序编码的参数槽，字符串等对象须使用其运行时地址。
     * @param result 成功时接收返回槽；void 函数的返回槽应忽略。
     * @return 函数正常返回时为 true；无效函数、参数数量错误或运行时失败时为 false。
     * @note 仅在宿主回调期间由执行线程同步使用；共享当前堆、对象表和全局状态。
     */
    virtual bool invoke(std::int64_t functionPointer,
                        const Memory::Vector<NativeValue> &arguments,
                        NativeValue &result) = 0;
    /**
     * @brief 在当前 VM 分配以零结尾的文本。
     * @param text 待复制的 UTF-8 文本。
     * @return VM 字符串地址；失败返回零并保留诊断。
     */
    [[nodiscard]] virtual std::int64_t makeString(const Memory::String &text) = 0;
    /**
     * @brief 取得当前运行的事件投递器，宿主回调结束后仍可安全使用。
     * @return 不支持时返回空函数，目标运行结束后投递返回关闭。
     */
    virtual EventPoster eventPoster() { return {}; }
    /**
     * @brief 安装执行边界钩子；按 VM 指令边界计数。
     * @param interval 调用间隔，零表示取消。
     * @param hook 同步执行的钩子，不得递归执行运行时。
     * @return 当前运行时支持钩子时返回 true。
     */
    virtual bool setInstructionHook(std::size_t interval, InstructionHook hook)
    {
        (void)interval;
        (void)hook;
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
    virtual Machine *machine() { return nullptr; }
    /**
     * @brief 申请随本次运行结束回收的临时内存。
     * @param bytes 临时内存字节数。
     * @return 分配地址；未实现时报告失败并返回零。
     */
    virtual std::int64_t makeTemporary(std::size_t bytes)
    {
        (void)bytes;
        fail("temporary storage unsupported");
        return 0;
    }
    /**
     * @brief 设置宿主调用的原始失败诊断。
     * @param message 需要传播给 VM 的错误文本。
     */
    virtual void fail(const Memory::String &message) = 0;
};

using NativeCallback =
    Memory::Function<NativeValue(NativeCallContext &, const Memory::Vector<NativeValue> &)>;

/**
 * @brief 宿主原生函数表；可同时挂到源码即时执行和加载后的 `.svmb` 程序。
 */
class NativeRegistry
{
public:
    /**
     * @brief 原生函数的实参数量及同步调用入口。
     */
    struct Entry
    {
        std::size_t arity = 0;  ///< 固定实参数量；size_t(-1) 表示由胶水检查的变长参数。
        NativeCallback callback;
    };

    /**
     * @brief 注册一个同步宿主原生函数。
     * @param name 原生函数完整名称。
     * @param arity 固定参数数量；最大 size_t 表示变参。
     * @param callback 检查值和执行调用的同步函数。
     * @param error 可选注册失败诊断接收器。
     * @return 名称和回调有效且未重复时为真。
     */
    bool add(const Memory::String &name, std::size_t arity, NativeCallback callback,
             Memory::String *error = nullptr);
    /**
     * @brief 移除原生函数并使预绑定缓存失效。
     * @param name 已注册函数名称。
     * @return 找到并移除时为真。
     */
    bool remove(const Memory::String &name);
    /**
     * @brief 按完整名称或压缩符号查询原生函数。
     * @param name 待查询的名称。
     * @return 注册项指针；不存在时为空。
     */
    [[nodiscard]] const Entry *find(const Memory::String &name) const;
    /**
     * @brief 注册表修改版本，用于使预绑定入口缓存失效。
     * @return 单调版本号。
     */
    [[nodiscard]] std::size_t revision() const { return revision_; }

private:
    std::size_t revision_ = 0;  ///< 注册或移除成功后递增。
    Memory::Map<Memory::String, Entry> entries_;
    Memory::Map<Memory::String, Memory::String> compactNames_;  ///< 编号键到宿主原名。
};

}  // namespace stackvm
