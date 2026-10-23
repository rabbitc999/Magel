/**
 * @file native_protocol.cpp
 * @brief 公开原生值、调用上下文与注册表的默认实现。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 为不支持记录壳的宿主报告创建失败。
 * @param type 记录描述符。
 * @param record 尚未转移所有权的记录。
 * @return 零句柄。
 */
std::int64_t NativeCallContext::makeNativeRecord(const NativeRecordType &type, void *record)
{
    (void)type;
    (void)record;
    fail("native record creation unsupported");
    return 0;
}
/**
 * @brief 在缺省宿主中拒绝借用记录。
 * @param handle 待查询句柄。
 * @param type 预期类型。
 * @return 空指针。
 */
void *NativeCallContext::nativeRecord(std::int64_t handle, const NativeRecordType &type) const
{
    (void)handle;
    (void)type;
    return nullptr;
}
/**
 * @brief 在缺省宿主中拒绝释放记录。
 * @param handle 待释放句柄。
 * @return false，表示未释放。
 */
bool NativeCallContext::releaseNativeRecord(std::int64_t handle)
{
    (void)handle;
    return false;
}
/**
 * @brief 在缺省宿主中拒绝释放运行时原始内存。
 * @param address 待释放块地址。
 * @param soft 是否允许跳过无效块，缺省宿主不提供内存管理。
 * @return false，表示操作不受支持。
 */
bool NativeCallContext::releaseMemory(std::int64_t address, bool soft)
{
    (void)address;
    (void)soft;
    fail("runtime memory release unsupported");
    return false;
}
/**
 * @brief 在缺省宿主中拒绝记录复制。
 * @param handle 待复制句柄。
 * @return 零句柄。
 */
std::int64_t NativeCallContext::cloneNativeRecord(std::int64_t handle)
{
    (void)handle;
    fail("native record cloning unsupported");
    return 0;
}
/**
 * @brief 查询缺省宿主是否注入通用服务。
 * @param name 服务名称，缺省实现不使用。
 * @return 空指针，具体运行时应覆盖此接口。
 */
void *NativeCallContext::hostService(const Memory::String &name) const
{
    (void)name;
    return nullptr;
}
/**
 * @brief 在缺省宿主中报告没有输入项。
 * @param text 保持不变的输出文本。
 * @return false，表示没有读取输入。
 */
bool NativeCallContext::takeInputText(Memory::String &text)
{
    (void)text;
    return false;
}
/**
 * @brief 查询缺省宿主的输入位置。
 * @return 零，表示尚未消费输入。
 */
std::size_t NativeCallContext::inputPosition() const { return 0u; }

/**
 * @brief 将整数封装为宿主原生值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
NativeValue NativeValue::fromInt(std::int64_t value)
{
    NativeValue result;
    result.integer = value;
    return result;
}

/**
 * @brief 将浮点数封装为宿主原生值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
NativeValue NativeValue::fromFloat(double value)
{
    NativeValue result;
    result.kind = Kind::Floating;
    result.floating = value;
    return result;
}

/**
 * @brief 将文本封装为宿主原生值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
NativeValue NativeValue::fromText(Memory::String value)
{
    NativeValue result;
    result.kind = Kind::Text;
    result.text = std::move(value);
    return result;
}

/**
 * @brief 尝试将当前值读取为整数。
 * @retval 尝试将当前值读取为整数。
 */
std::int64_t NativeValue::asInt() const
{
    if (isText()) return 0;
    return isFloat() ? static_cast<std::int64_t>(floating) : integer;
}

/**
 * @brief 将当前运行时值转换为浮点数。
 * @retval 保存将当前运行时值转换为浮点数结果的对象。
 */
double NativeValue::asFloat() const
{
    if (isText()) return 0.0;
    return isFloat() ? floating : static_cast<double>(integer);
}

/**
 * @brief 登记原生函数的名称、参数数量与实现。
 * @param name 名称文本。
 * @param arity 原生函数接收的参数数量。
 * @param callback 接收事件或执行操作的回调函数。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeRegistry::add(const Memory::String &name, std::size_t arity, NativeCallback callback,
                         Memory::String *error)
{
    if (name.empty())
    {
        if (error != nullptr) *error = "原生函数名不能为空";
        return false;
    }
    if (!callback)
    {
        if (error != nullptr) *error = "原生函数 " + name + " 没有 C++ 回调";
        return false;
    }
    if (entries_.count(name) != 0u)
    {
        if (error != nullptr) *error = "原生函数重复注册：" + name;
        return false;
    }
    const auto key = symbols::key(symbols::logicalId(name));
    if (compactNames_.count(key))
    {
        if (error) *error = "原生函数稳定编号碰撞";
        return false;
    }
    compactNames_.emplace(key, name);
    entries_.emplace(name, Entry{ arity, std::move(callback) });
    ++revision_;
    return true;
}

/**
 * @brief 移除指定名称的原生函数绑定。
 * @param name 名称文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NativeRegistry::remove(const Memory::String &name)
{
    compactNames_.erase(symbols::key(symbols::logicalId(name)));
    const bool removed = entries_.erase(name) != 0u;
    if (removed) ++revision_;
    return removed;
}

/**
 * @brief 将名字解析为稳定整数身份，动态回调临时名称不自动驻留。
 * @param name 名字。
 * @return 已登记身份或 absent。
 */
const NativeRegistry::Entry *NativeRegistry::find(const Memory::String &name) const
{
    const auto alias = compactNames_.find(name);
    const auto found = entries_.find(alias == compactNames_.end() ? name : alias->second);
    return found == entries_.end() ? nullptr : &found->second;
}
}  // namespace stackvm
