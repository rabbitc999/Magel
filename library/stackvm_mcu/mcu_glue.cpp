/**
 * @file mcu_glue.cpp
 * @brief 板端 HAL 胶水的最小实现：把 `std::lang::__*` 里**不依赖容器**的那几个接上。
 *
 * 这些名字**不是 VM 原生函数**，是 `lang.mgl` 里声明、由宿主提供的：
 *   * `std::lang::__tryToInt32(float64) -> int64`：向零截断 + 范围检查（越界/NaN 返回哨兵值）；
 *   * `std::lang::__toInt64(float64) -> int64`：同上，非法输入报运行错误。
 * （`std::lang::__float32ToBytes` / `std::lang::__bytesToFloat64` 返回 / 接收 `List<uint8>`，
 * 需要 VM 的胶水上下文能建容器——通用 VM 现在只有 `makeString`，见 README 的清单。）
 *
 * 板端接产品接口（串口、OLED、RTT、传感器…）时照这里的写法往下加：
 * `registry.add("你的名字", arity, 回调)`，回调里读写参数、必要时写 `context.error`。
 */

#include "mcu_glue.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace mcuGlue
{
namespace
{
/**
 * @brief `__tryToInt32` 的失败哨兵。
 *
 * 与桌面胶水（`glue/lang_numeric.hpp`）一致：`lang.mgl` 里 `toInt32` 用
 * `converted == 2147483648` 判断 None——2^31 不在 int32 有效结果里，所以能严格区分。
 */
constexpr std::int64_t kInt32NoneSentinel = 2147483648;

/**
 * @brief 向零截断到 `[first, last)` 区间内；非法返回 false。
 * @param value 输入。
 * @param first 下界（含）。
 * @param last 上界（不含）。
 * @param out 输出。
 * @return 有效返回 true。
 */
bool TruncateInRange(double value, double first, double last, std::int64_t *out)
{
    if (!std::isfinite(value)) return false;
    const double truncated = std::trunc(value);
    if (truncated < first || truncated >= last) return false;
    *out = static_cast<std::int64_t>(truncated);
    return true;
}

/**
 * @brief `std::lang::__tryToInt32(float64) -> int64`：越界给 2^31 哨兵（`toInt32` 据此返回 None）。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue TryToInt32(stackvm::NativeCallContext &, const Memory::Vector<stackvm::NativeValue> &args)
{
    std::int64_t converted = 0;
    if (!TruncateInRange(args[0].asFloat(), -2147483648.0, 2147483648.0, &converted))
        return stackvm::NativeValue::fromInt(kInt32NoneSentinel);
    return stackvm::NativeValue::fromInt(converted);
}

/**
 * @brief `std::lang::__toInt64(float64) -> int64`：非法输入报运行错误（与桌面语义一致）。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue ToInt64(stackvm::NativeCallContext &context, const Memory::Vector<stackvm::NativeValue> &args)
{
    std::int64_t converted = 0;
    if (!TruncateInRange(args[0].asFloat(), -9223372036854775808.0, 9223372036854775808.0,
                         &converted))
    {
        context.fail("std::lang::toInt64：浮点值不是有限数或超出 int64 范围");
        return stackvm::NativeValue::fromInt(0);
    }
    return stackvm::NativeValue::fromInt(converted);
}
}  // namespace

namespace
{
OutputSink gOutput = nullptr;  ///< 板端输出下沉。
void *gOutputUser = nullptr;   ///< 下沉上下文。
bool gKeepText = true;         ///< 是否同时累积进 `RunResult::text`。

/**
 * @brief VM 只认普通函数指针，这里转发到当前下沉。
 */
void SinkThunk(void *, const char *bytes, std::size_t count)
{
    if (gOutput != nullptr) gOutput(gOutputUser, bytes, count);
}

/**
 * @brief 定点缓冲格式化（不分配内存）。
 */
template <typename... Args>
void EmitFormatted(const char *format, Args... args)
{
    char buffer[64];
    const int written = std::snprintf(buffer, sizeof(buffer), format, args...);
    if (written <= 0) return;
    const std::size_t length = static_cast<std::size_t>(written) < sizeof(buffer) ? static_cast<std::size_t>(written) : sizeof(buffer) - 1u;
    Emit(buffer, length);
}

/**
 * @brief 将原生字符参数输出到板端控制台。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue PutChar(stackvm::NativeCallContext &,
                             const Memory::Vector<stackvm::NativeValue> &args)
{
    const char byte = static_cast<char>(args[0].asInt() & 0xff);
    Emit(&byte, 1u);
    return stackvm::NativeValue::fromInt(0);
}

/**
 * @brief 将原生字符串参数输出到板端控制台。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue PutText(stackvm::NativeCallContext &context,
                             const Memory::Vector<stackvm::NativeValue> &args)
{
    Memory::String text;
    if (!context.readString(args[0].asInt(), text))
    {
        context.fail("putText 收到了无效字符串");
        return stackvm::NativeValue::fromInt(0);
    }
    Emit(text.data(), text.size());
    return stackvm::NativeValue::fromInt(0);
}

/**
 * @brief 将原生整数参数格式化输出到板端控制台。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue PutInt(stackvm::NativeCallContext &,
                            const Memory::Vector<stackvm::NativeValue> &args)
{
    EmitFormatted("%lld", static_cast<long long>(args[0].asInt()));
    return stackvm::NativeValue::fromInt(0);
}

/**
 * @brief 将原生浮点参数格式化输出到板端控制台。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue PutFloat(stackvm::NativeCallContext &,
                              const Memory::Vector<stackvm::NativeValue> &args)
{
    EmitFormatted("%g", args[0].asFloat());
    return stackvm::NativeValue::fromInt(0);
}

/**
 * @brief 将原生整数参数按十六进制输出到板端控制台。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue PutHex(stackvm::NativeCallContext &,
                            const Memory::Vector<stackvm::NativeValue> &args)
{
    EmitFormatted("%llx", static_cast<unsigned long long>(args[0].asInt()));
    return stackvm::NativeValue::fromInt(0);
}

/**
 * @brief 将原生字符串参数输出并追加换行。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue PutLine(stackvm::NativeCallContext &,
                             const Memory::Vector<stackvm::NativeValue> &)
{
    Emit("\n", 1u);
    return stackvm::NativeValue::fromInt(0);
}
}  // namespace

/**
 * @brief 设置输出下沉（串口 / OLED / RTT / 内存环形缓冲）。
 * @param sink 下沉；`nullptr` = 只往 `RunResult::text` 累积。
 * @param user 上下文。
 * @param keepText 是否同时累积（板端长跑建议 false，省 RAM）。
 * @retval 对应对象的指针。
 */
void SetOutput(OutputSink sink, void *user, bool keepText)
{
    gOutput = sink;
    gOutputUser = user;
    gKeepText = keepText;
}

/**
 * @brief 直接往当前输出下沉写一段（板端自己的代码也能用）。
 * @param bytes 内存区域或数据的字节数。
 * @param count 元素数量或操作次数。
 */
void Emit(const char *bytes, std::size_t count)
{
    if (bytes == nullptr || count == 0u) return;
    if (gOutput != nullptr) gOutput(gOutputUser, bytes, count);
}

/**
 * @brief 直接往当前输出下沉写 C 字符串。
 * @param text 待处理的文本。
 */
void Emit(const char *text)
{
    if (text == nullptr) return;
    std::size_t length = 0u;
    while (text[length] != '\0') ++length;
    Emit(text, length);
}

namespace
{
/* ---- IEEE 754 字节 ↔ `List<uint8>`：靠胶水容器能力（makeList / listPush / readList） ---- */

/**
 * @brief 把浮点位模式写成 4 或 8 字节的 `List<uint8>`（默认小端）。
 */
template <typename T, typename U>
stackvm::NativeValue FloatToBytesList(stackvm::NativeCallContext &context, double value,
                                      bool bigEndian)
{
    const T typed = static_cast<T>(value);
    U bits = 0;
    std::memcpy(&bits, &typed, sizeof(T));
    const std::int64_t handle = context.makeList();
    if (handle == 0) return stackvm::NativeValue::fromInt(0);
    for (std::size_t index = 0u; index < sizeof(T); ++index)
    {
        const std::size_t shift = bigEndian ? (sizeof(T) - 1u - index) : index;
        const unsigned long long byte =
            (static_cast<unsigned long long>(bits) >> (8u * shift)) & 0xffu;
        if (!context.listPush(handle,
                              stackvm::NativeValue::fromInt(static_cast<std::int64_t>(byte))))
            return stackvm::NativeValue::fromInt(0);
    }
    return stackvm::NativeValue::fromInt(handle);
}

/**
 * @brief 从 `List<uint8>` 的前 4/8 字节还原浮点（不足报错）。
 */
template <typename T, typename U>
double BytesListToFloat(stackvm::NativeCallContext &context, std::int64_t handle, bool bigEndian)
{
    Memory::Vector<stackvm::NativeValue> items;
    if (!context.readList(handle, items) || items.size() < sizeof(T))
    {
        context.fail("IEEE 754 解码：字节列表不足");
        return 0.0;
    }
    U bits = 0;
    for (std::size_t index = 0u; index < sizeof(T); ++index)
    {
        const std::size_t shift = bigEndian ? (sizeof(T) - 1u - index) : index;
        bits |= static_cast<U>(static_cast<unsigned long long>(items[index].asInt()) & 0xffu)
                << (8u * shift);
    }
    T typed = T();
    std::memcpy(&typed, &bits, sizeof(T));
    return static_cast<double>(typed);
}

/**
 * @brief 将单精度浮点数编码为运行时字节列表。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue Float32ToBytes(stackvm::NativeCallContext &context,
                                    const Memory::Vector<stackvm::NativeValue> &args)
{
    return FloatToBytesList<float, std::uint32_t>(context, args[0].asFloat(), args[1].asInt() != 0);
}

/**
 * @brief 将双精度浮点数编码为运行时字节列表。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue Float64ToBytes(stackvm::NativeCallContext &context,
                                    const Memory::Vector<stackvm::NativeValue> &args)
{
    return FloatToBytesList<double, std::uint64_t>(context, args[0].asFloat(), args[1].asInt() != 0);
}

/**
 * @brief 从运行时字节列表解码单精度浮点数。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue BytesToFloat32(stackvm::NativeCallContext &context,
                                    const Memory::Vector<stackvm::NativeValue> &args)
{
    return stackvm::NativeValue::fromFloat(
        BytesListToFloat<float, std::uint32_t>(context, args[0].asInt(), args[1].asInt() != 0));
}

/**
 * @brief 从运行时字节列表解码双精度浮点数。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue BytesToFloat64(stackvm::NativeCallContext &context,
                                    const Memory::Vector<stackvm::NativeValue> &args)
{
    return stackvm::NativeValue::fromFloat(
        BytesListToFloat<double, std::uint64_t>(context, args[0].asInt(), args[1].asInt() != 0));
}
/* ---- 数组形态的 IEEE 754：`uint8 mut&` / `uint8 const&` 是数组槽地址，----
 * ---- 每个元素占 8 字节槽、只动低字节（与桌面 `readArraySlots` 同一约定）。 ---- */

/**
 * @brief 读 `count` 个数组槽的低字节。
 */
bool ReadArrayBytes(stackvm::NativeCallContext &context, std::int64_t address, unsigned char *out,
                    std::size_t count)
{
    Memory::Vector<long long> slots(count, 0);
    if (!context.readBytes(address, reinterpret_cast<char *>(slots.data()), count * 8u))
    {
        context.fail("IEEE 754 转换：读不到数组");
        return false;
    }
    for (std::size_t index = 0u; index < count; ++index)
        out[index] = static_cast<unsigned char>(slots[index] & 0xff);
    return true;
}

/**
 * @brief 把 `count` 个字节写进数组槽的低字节（保留各槽高位）。
 */
bool WriteArrayBytes(stackvm::NativeCallContext &context, std::int64_t address,
                     const unsigned char *bytes, std::size_t count)
{
    Memory::Vector<long long> slots(count, 0);
    if (!context.readBytes(address, reinterpret_cast<char *>(slots.data()), count * 8u))
    {
        context.fail("IEEE 754 转换：读不到数组");
        return false;
    }
    for (std::size_t index = 0u; index < count; ++index)
        slots[index] = (slots[index] & ~0xffll) | static_cast<long long>(bytes[index]);
    if (!context.writeBytes(address, reinterpret_cast<const char *>(slots.data()), count * 8u))
    {
        context.fail("IEEE 754 转换：无法写入数组");
        return false;
    }
    return true;
}

/**
 * @brief `浮点 → 数组槽`（`reverse=1` 时按大端写）。
 */
template <typename T, typename U>
stackvm::NativeValue FloatToArray(stackvm::NativeCallContext &context,
                                  const Memory::Vector<stackvm::NativeValue> &args, bool bigEndian)
{
    const T typed = static_cast<T>(args[0].asFloat());
    U bits = 0;
    std::memcpy(&bits, &typed, sizeof(T));
    unsigned char bytes[sizeof(T)] = {};
    for (std::size_t index = 0u; index < sizeof(T); ++index)
    {
        const std::size_t shift = bigEndian ? (sizeof(T) - 1u - index) : index;
        bytes[index] = static_cast<unsigned char>(
            (static_cast<unsigned long long>(bits) >> (8u * shift)) & 0xffu);
    }
    if (!WriteArrayBytes(context, args[1].asInt(), bytes, sizeof(T)))
        return stackvm::NativeValue::fromInt(0);
    return stackvm::NativeValue::fromInt(1);
}

/**
 * @brief `数组槽 → 浮点`。
 */
template <typename T, typename U>
double ArrayToFloat(stackvm::NativeCallContext &context,
                    const Memory::Vector<stackvm::NativeValue> &args, bool bigEndian)
{
    unsigned char bytes[sizeof(T)] = {};
    if (!ReadArrayBytes(context, args[0].asInt(), bytes, sizeof(T))) return 0.0;
    U bits = 0;
    for (std::size_t index = 0u; index < sizeof(T); ++index)
    {
        const std::size_t shift = bigEndian ? (sizeof(T) - 1u - index) : index;
        bits |= static_cast<U>(static_cast<unsigned long long>(bytes[index])) << (8u * shift);
    }
    T typed = T();
    std::memcpy(&typed, &bits, sizeof(T));
    return static_cast<double>(typed);
}

/**
 * @brief 将单精度浮点数编码为字节数组。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue Float32ToArray(stackvm::NativeCallContext &context,
                                    const Memory::Vector<stackvm::NativeValue> &args)
{
    return FloatToArray<float, std::uint32_t>(context, args, args[2].asInt() != 0);
}

/**
 * @brief 将双精度浮点数编码为字节数组。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue Float64ToArray(stackvm::NativeCallContext &context,
                                    const Memory::Vector<stackvm::NativeValue> &args)
{
    return FloatToArray<double, std::uint64_t>(context, args, args[2].asInt() != 0);
}

/**
 * @brief 从字节数组解码单精度浮点数。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue ArrayToFloat32(stackvm::NativeCallContext &context,
                                    const Memory::Vector<stackvm::NativeValue> &args)
{
    return stackvm::NativeValue::fromFloat(
        ArrayToFloat<float, std::uint32_t>(context, args, args[1].asInt() != 0));
}

/**
 * @brief 从字节数组解码双精度浮点数。
 * @param context 当前处理或执行上下文。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
stackvm::NativeValue ArrayToFloat64(stackvm::NativeCallContext &context,
                                    const Memory::Vector<stackvm::NativeValue> &args)
{
    return stackvm::NativeValue::fromFloat(
        ArrayToFloat<double, std::uint64_t>(context, args, args[1].asInt() != 0));
}

}  // namespace

/**
 * @brief 把板端 HAL 胶水挂到这台机器上。
 *
 * 契约：**回调不许抛异常**（板端口径 `-fno-exceptions`），要报错写
 * `NativeContext::error`。
 * @param machine 目标虚拟机（生命周期必须覆盖 `run()`）。
 * @param customRegistry 可选的宿主注册表，允许在标准板端胶水之外追加外设接口。
 */
void Register(stackvm::Machine &machine, stackvm::NativeRegistry *customRegistry)
{
    static stackvm::NativeRegistry defaultRegistry; /* 板端单线程：函数内静态不需要加锁 */
    stackvm::NativeRegistry &registry = customRegistry ? *customRegistry : defaultRegistry;
    registry.add("std::lang::__tryToInt32", 1u, TryToInt32);
    registry.add("std::lang::__toInt64", 1u, ToInt64);
    /* putXXX 系列：板端把输出直接推给自己（串口 / OLED / RTT）。 */
    registry.add("putChar", 1u, PutChar);
    registry.add("putText", 1u, PutText);
    registry.add("putInt", 1u, PutInt);
    registry.add("putFloat", 1u, PutFloat);
    registry.add("putHex", 1u, PutHex);
    registry.add("putLine", 0u, PutLine);
    /* IEEE 754：桌面在 `glue/lang_numeric.hpp` 里绑，板端在这里绑（同一套语义）。 */
    registry.add("std::lang::__float32ToBytes", 2u, Float32ToBytes);
    registry.add("std::lang::__float64ToBytes", 2u, Float64ToBytes);
    registry.add("std::lang::__bytesToFloat32", 2u, BytesToFloat32);
    registry.add("std::lang::__bytesToFloat64", 2u, BytesToFloat64);
    registry.add("std::lang::__float32ToArray", 3u, Float32ToArray);
    registry.add("std::lang::__float64ToArray", 3u, Float64ToArray);
    registry.add("std::lang::__arrayToFloat32", 2u, ArrayToFloat32);
    registry.add("std::lang::__arrayToFloat64", 2u, ArrayToFloat64);
    /* `print` / `println` / `putChar` 是语言内建（写 `result.text`）；
     * 这里把同一条文本流也接到板端下沉上。 */
    machine.setOutputSink(&SinkThunk, nullptr, gKeepText);
    machine.setNativeRegistry(&registry);
}

}  // namespace mcuGlue
