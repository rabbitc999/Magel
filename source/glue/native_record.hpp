#ifndef MAGEL_GLUE_NATIVE_RECORD_HPP
#define MAGEL_GLUE_NATIVE_RECORD_HPP

#include "stackvm.hpp"

/**
 * @brief 基于公开运行时接口访问不透明宿主记录的胶水辅助层。
 */
namespace native_record_glue
{
using Value = stackvm::NativeValue;

/**
 * @brief 在一次胶水操作内累积诊断，绑定入口统一交回运行时。
 */
struct State
{
    stackvm::NativeCallContext &host;  ///< 仅在本次调用内有效的宿主上下文。
    Memory::String error;              ///< 当前操作的原始诊断。
};

/**
 * @brief 按通用记录协议复制对象，供 opcode 的 copy 方法绑定。
 * @param state 当前胶水调用状态。
 * @param arguments 包含待复制句柄。
 * @return 独立副本句柄。
 */
inline Value copy(State &state, const Memory::Vector<Value> &arguments)
{
    return Value::fromInt(state.host.cloneNativeRecord(arguments[0].asInt()));
}

/**
 * @brief 幂等释放通用记录，供 opcode 的 drop 方法绑定。
 * @param state 当前胶水调用状态。
 * @param arguments 包含待释放句柄，零或已释放句柄不重复销毁。
 * @return 零占位值。
 */
inline Value drop(State &state, const Memory::Vector<Value> &arguments)
{
    state.host.releaseNativeRecord(arguments[0].asInt());
    return Value::fromInt(0);
}

/**
 * @brief 为 String 的 new 绑定创建独立空值。
 * @param state 当前运行时调用状态。
 * @param arguments 无参数创建调用的参数列表。
 * @return 新分配的空字符串地址。
 */
inline Value stringNew(State &state, const Memory::Vector<Value> &arguments)
{
    (void)arguments;
    return Value::fromInt(state.host.makeString({}));
}

/**
 * @brief 为 String 的 copy 绑定按长度复制文本。
 * @param state 当前运行时调用状态。
 * @param arguments 包含被复制的 String 值。
 * @return 与源值独立的字符串地址。
 */
inline Value stringCopy(State &state, const Memory::Vector<Value> &arguments)
{
    Memory::String text;
    if (!state.host.readString(arguments[0].asInt(), text))
    {
        state.error = "String copy 读取源值失败";
        return Value::fromInt(0);
    }
    return Value::fromInt(state.host.makeString(text));
}

/**
 * @brief 为 String 的 drop 绑定释放自己拥有的原始字节块。
 * @param state 当前运行时调用状态。
 * @param arguments 包含待释放的 String 值。
 * @return 零占位值。
 */
inline Value stringDrop(State &state, const Memory::Vector<Value> &arguments)
{
    state.host.releaseMemory(arguments[0].asInt(), true);
    return Value::fromInt(0);
}

/**
 * @brief 读取原生参数的整数位模式。
 * @param value 原生参数。
 * @return 整数值或浮点向零截断结果。
 */
inline std::int64_t nativeInt(const Value &value) { return value.asInt(); }

/**
 * @brief 从当前运行时按其文本协议读取字符串。
 * @param state 胶水状态。
 * @param address 文本地址。
 * @param out 接收读取结果。
 * @return 读取成功时为 true。
 */
inline bool nativeReadCString(State &state, std::size_t address, Memory::String *out)
{
    return state.host.readString(static_cast<std::int64_t>(address), *out);
}

/**
 * @brief 创建交给语言调用方拥有的字符串副本。
 * @param state 胶水状态。
 * @param text 要复制的文本。
 * @return 当前运行时中的文本句柄。
 */
inline std::int64_t nativeAllocString(State &state, const Memory::String &text)
{
    return state.host.makeString(text);
}

/**
 * @brief 校验记录类型并借用宿主记录。
 * @tparam T 胶水自己的记录布局。
 * @param state 胶水状态。
 * @param handle 原生参数中的对象句柄。
 * @param type 预期宿主描述符。
 * @param who 诊断中的操作名称。
 * @return 有效记录，失败返回空并保存诊断。
 */
template <class T>
T *record(State &state, const Value &handle, const stackvm::NativeRecordType &type, const char *who)
{
    auto *result = static_cast<T *>(state.host.nativeRecord(handle.asInt(), type));
    if (!result)
        state.error = Memory::String(who) + " 用的 " + type.name + " 已经释放过了（句柄 " + Memory::toString(handle.asInt()) + "）";
    return result;
}

/**
 * @brief 注册只通过公开上下文访问宿主记录的原生操作。
 * @param registry 目标胶水注册表。
 * @param name 语言侧绑定名称。
 * @param arity 实参数量。
 * @param operation 操作实现。
 * @param error 注册失败的诊断输出。
 * @param referenceFirst 首个参数是否为保存对象句柄的语言引用槽。
 * @return 注册成功时为 true。
 */
inline bool bind(stackvm::NativeRegistry &registry, const char *name, std::size_t arity,
                 Value (*operation)(State &, const Memory::Vector<Value> &), Memory::String *error, bool referenceFirst = false)
{
    return registry.add(name, arity, [operation, referenceFirst](stackvm::NativeCallContext &context, const Memory::Vector<Value> &arguments) {
        State state{ context, {} };
        Memory::Vector<Value> values;
        const auto *input = &arguments;
        if (referenceFirst)
        {
            values = arguments;
            input = &values;
            char bytes[8];
            if (values.empty() || !context.readBytes(values[0].asInt(), bytes, sizeof(bytes)))
            {
                context.fail("胶水对象引用指向的句柄槽无效");
                return Value::fromInt(0);
            }
            std::uint64_t handle = 0;
            for (unsigned index = 0; index < 8; ++index)
                handle |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[index])) << (index * 8);
            values[0] = Value::fromInt(static_cast<std::int64_t>(handle));
        }
        auto result = operation(state, *input);
        if (!state.error.empty()) context.fail(state.error);
        return result; }, error);
}

/**
 * @brief 注册 String opcode 的创建、复制和销毁协议。
 * @param registry 目标原生函数表。
 * @param error 注册失败的原因。
 * @return 三个生命周期函数均绑定成功时返回 true。
 */
inline bool bindString(stackvm::NativeRegistry &registry, Memory::String *error = nullptr)
{
    return native_record_glue::bind(registry, "std::lang::__stringNew", 0, stringNew, error) && native_record_glue::bind(registry, "std::lang::__stringCopy", 1, stringCopy, error) && native_record_glue::bind(registry, "std::lang::__stringDrop", 1, stringDrop, error);
}
}  // namespace native_record_glue

#endif
