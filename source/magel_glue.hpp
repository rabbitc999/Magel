#include "allocator/magel_allocator.hpp"
#ifndef PARSE_ENGINE_MAGEL_GLUE_HPP
#define PARSE_ENGINE_MAGEL_GLUE_HPP

#include "stackvm.hpp"
#include "magel_generic_name.hpp"

#include <cstddef>
#include <array>
#include <exception>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <functional>
#include <memory>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

/**
 * 由生成的 magel 前端提供；注册表生命周期必须覆盖 M_Compile。
 */
void M_SetNativeRegistry(const stackvm::NativeRegistry *registry);

namespace magel_glue
{

using Context = stackvm::NativeCallContext;
using Value = stackvm::NativeValue;
using Callback = stackvm::NativeCallback;

/**
 * C++ 胶水接收 MAGEL `T...` 尾参的类型化只读副本。
 */
template <typename T>
struct Variadic
{
    Memory::Vector<T> values;

    [[nodiscard]] std::size_t size() const { return values.size(); }
    [[nodiscard]] bool empty() const { return values.empty(); }
    const T &operator[](std::size_t index) const { return values[index]; }
    auto begin() const { return values.begin(); }
    auto end() const { return values.end(); }
};

/**
 * 任意 MAGEL 结构体的 VM 视图；只在当前宿主回调期间有效。
 */
template <typename Tag>
class Record
{
public:
    Record() = default;
    Record(Context *context, std::int64_t address) : context_(context), address_(address) {}

    [[nodiscard]] std::int64_t address() const { return address_; }
    [[nodiscard]] explicit operator bool() const { return context_ != nullptr && address_ != 0; }

    template <typename T>
    T field(std::size_t slot) const;
    /**
     * 类型化写入 `slot` 号槽（结构体字段 / 枚举载荷；枚举 0 号槽是 tag）。
     */
    template <typename T>
    void set(std::size_t slot, const T &value) const;

protected:
    Context *context_ = nullptr;
    std::int64_t address_ = 0;
};

/**
 * MAGEL 枚举与结构体使用同一槽布局；0 号槽固定为枚举标签。
 */
template <typename Tag>
class Enum : public Record<Tag>
{
public:
    using Record<Tag>::Record;
    [[nodiscard]] std::int32_t tag() const
    {
        return this->template field<std::int32_t>(0u);
    }

    template <typename T>
    T payload(std::size_t slot) const
    {
        return this->template field<T>(slot);
    }
};

/**
 * @brief 当前 VM 或 AOT 运行时中的 Magel 可调用函数值，支持具名函数与捕获闭包。
 * @note 仅在所属宿主回调期间同步使用；地址不是可直接转换的 C/C++ 函数指针。
 */
template <typename Signature>
class Function;

/**
 * C++ 侧的名义宿主句柄；Tag 区分不同 MAGEL `type Name = ABI`，Rep 决定 ABI。
 */
template <typename Tag, typename Rep = std::uint64_t>
struct Handle
{
    Rep value{};
};

/**
 * VM 地址包装；避免把虚拟地址误当成可解引用的宿主 C++ 指针。
 *
 * 地址仍是 VM 地址，只在当前宿主回调期间有效；`read` / `write` / `at` 是按
 * `Codec<T>` 编解码的**类型化**读写（因此需要当前 `Context&`）：标量读 / 写 8 字节槽，
 * `Memory::String` / 容器写回时先 encode 成新句柄再落槽；
 * `Record<Tag>` / `Enum<Tag>` 这类视图元素直接按地址构造视图，不读槽。
 * 需要按槽步进时用 `offset(n)`（1 槽 = 8 字节，数组元素与结构体字段都槽对齐）。
 */
template <typename T>
struct Reference
{
    std::int64_t address = 0;
    explicit operator bool() const { return address != 0; }

    /**
     * 读取该地址处的 T（类型化）。空地址报错并返回零值。
     */
    T read(Context &context) const;
    /**
     * 写入该地址处的 T（类型化）。空地址报错。
     */
    void write(Context &context, const T &value) const;

    /**
     * 按 8 字节槽偏移；不读取也不写入，仅移动地址。
     */
    [[nodiscard]] Reference<T> offset(std::int64_t slots) const
    {
        return Reference<T>{ address + slots * 8 };
    }
    /**
     * 读取第 `slots` 个槽处的 T（等价于 `offset(slots).read(context)`）。
     */
    T at(Context &context, std::int64_t slots) const { return offset(slots).read(context); }
    /**
     * 写入第 `slots` 个槽处的 T（等价于 `offset(slots).write(context, value)`）。
     */
    void setAt(Context &context, std::int64_t slots, const T &value) const
    {
        offset(slots).write(context, value);
    }
};

/**
 * VM 单槽与 C++ 内置类型之间的转换点；应用可为自己的平凡 ABI 类型特化它。
 */
template <typename T, typename Enable = void>
struct Codec;

template <typename T>
struct Codec<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
{
    static T decode(Context &, const Value &value) { return static_cast<T>(value.asInt()); }
    static Value encode(Context &, T value)
    {
        return Value::fromInt(static_cast<std::int64_t>(value));
    }
};

template <>
struct Codec<bool>
{
    static bool decode(Context &, const Value &value) { return value.asInt() != 0; }
    static Value encode(Context &, bool value) { return Value::fromInt(value ? 1 : 0); }
};

template <typename T>
struct Codec<T, std::enable_if_t<std::is_floating_point_v<T>>>
{
    static T decode(Context &, const Value &value) { return static_cast<T>(value.asFloat()); }
    static Value encode(Context &, T value) { return Value::fromFloat(static_cast<double>(value)); }
};

template <>
struct Codec<Memory::String>
{
    static Memory::String decode(Context &context, const Value &value)
    {
        if (value.isText()) return value.text;
        Memory::String text;
        if (!context.readString(value.asInt(), text)) context.fail("胶水层读取 string 失败");
        return text;
    }
    static Value encode(Context &context, const Memory::String &value)
    {
        return Value::fromInt(context.makeString(value));
    }
};

template <typename T>
struct Codec<Variadic<T>>
{
    static Variadic<T> decode(Context &context, const Value &value)
    {
        Memory::Vector<Value> raw;
        if (!context.readList(value.asInt(), raw))
        {
            context.fail("胶水层读取变长参数失败：不是有效的 std::lang::List 句柄");
            return {};
        }
        Variadic<T> result;
        result.values.reserve(raw.size());
        for (const Value &item : raw)
            result.values.push_back(Codec<std::decay_t<T>>::decode(context, item));
        return result;
    }
};

/**
 * 标量 `std::lang::List<T>` 的返回：encode 造一个新列表并逐元素 push（元素是标量）。
 */
template <typename T>
struct Codec<Memory::Vector<T>>
{
    static Memory::Vector<T> decode(Context &context, const Value &value)
    {
        Memory::Vector<Value> raw;
        if (!context.readList(value.asInt(), raw))
        {
            context.fail("胶水层读取 List 失败：不是有效的 std::lang::List 句柄");
            return {};
        }
        Memory::Vector<T> result;
        result.reserve(raw.size());
        for (const Value &item : raw)
            result.push_back(Codec<std::decay_t<T>>::decode(context, item));
        return result;
    }
    static Value encode(Context &context, const Memory::Vector<T> &values)
    {
        const std::int64_t list = context.makeList();
        for (const T &item : values)
        {
            // 列表持有字符串值；普通 string 返回 ABI 的堆地址不携带元素所有权。
            if constexpr (std::is_same_v<std::decay_t<T>, Memory::String> || std::is_same_v<std::decay_t<T>, std::string>)
                context.listPush(list, Value::fromText(Memory::String(item)));
            else
                context.listPush(list, Codec<std::decay_t<T>>::encode(context, item));
        }
        return Value::fromInt(list);
    }
};

/**
 * `std::lang::Map<K,V>` 的读写：decode 读参数，encode 造新映射返回。
 */
template <typename K, typename V>
struct Codec<Memory::Map<K, V>>
{
    static Memory::Map<K, V> decode(Context &context, const Value &value)
    {
        Memory::Vector<Value> keys, values;
        if (!context.readMap(value.asInt(), keys, values))
        {
            context.fail("胶水层读取 Map 失败：不是有效的 std::lang::Map 句柄");
            return {};
        }
        Memory::Map<K, V> result;
        for (std::size_t i = 0; i < keys.size() && i < values.size(); ++i)
            result[Codec<std::decay_t<K>>::decode(context, keys[i])] =
                Codec<std::decay_t<V>>::decode(context, values[i]);
        return result;
    }
    static Value encode(Context &context, const Memory::Map<K, V> &map)
    {
        const std::int64_t handle = context.makeMap();
        for (const auto &entry : map)
            context.mapSet(handle, Codec<std::decay_t<K>>::encode(context, entry.first),
                           Codec<std::decay_t<V>>::encode(context, entry.second));
        return Value::fromInt(handle);
    }
};

/**
 * `std::lang::Set<T>` 的读写：decode 读参数，encode 造新集合返回。
 */
template <typename T>
struct Codec<Memory::Set<T>>
{
    static Memory::Set<T> decode(Context &context, const Value &value)
    {
        Memory::Vector<Value> elements;
        if (!context.readSet(value.asInt(), elements))
        {
            context.fail("胶水层读取 Set 失败：不是有效的 std::lang::Set 句柄");
            return {};
        }
        Memory::Set<T> result;
        for (const Value &element : elements)
            result.insert(Codec<std::decay_t<T>>::decode(context, element));
        return result;
    }
    static Value encode(Context &context, const Memory::Set<T> &set)
    {
        const std::int64_t handle = context.makeSet();
        for (const T &element : set)
            context.setInsert(handle, Codec<std::decay_t<T>>::encode(context, element));
        return Value::fromInt(handle);
    }
};

template <typename Tag>
struct Codec<Record<Tag>>
{
    static Record<Tag> decode(Context &context, const Value &value)
    {
        return Record<Tag>{ &context, value.asInt() };
    }
    static Value encode(Context &, const Record<Tag> &value)
    {
        return Value::fromInt(value.address());
    }
};

template <typename Tag>
struct Codec<Enum<Tag>>
{
    static Enum<Tag> decode(Context &context, const Value &value)
    {
        return Enum<Tag>{ &context, value.asInt() };
    }
    static Value encode(Context &, const Enum<Tag> &value)
    {
        return Value::fromInt(value.address());
    }
};

template <typename T>
struct IsRecordView : std::false_type
{
};
template <typename Tag>
struct IsRecordView<Record<Tag>> : std::true_type
{
};
template <typename Tag>
struct IsRecordView<Enum<Tag>> : std::true_type
{
};

namespace detail
{
/**
 * 类型化读一个 VM 槽（8 字节）：视图类型按地址直接构造，浮点 / 其它按 T 的 Codec 解码。
 */
template <typename T>
T readSlot(Context &context, std::int64_t address)
{
    if constexpr (IsRecordView<T>::value)
    {
        return T{ &context, address };
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        double raw = 0.0;
        if (!context.readBytes(address, reinterpret_cast<char *>(&raw), sizeof(raw)))
        {
            context.fail("胶水层读取槽失败（越界或空地址）");
            return T{};
        }
        return Codec<T>::decode(context, Value::fromFloat(raw));
    }
    else
    {
        std::int64_t raw = 0;
        if (!context.readBytes(address, reinterpret_cast<char *>(&raw), sizeof(raw)))
        {
            context.fail("胶水层读取槽失败（越界或空地址）");
            return T{};
        }
        return Codec<T>::decode(context, Value::fromInt(raw));
    }
}

/**
 * 类型化写一个 VM 槽（8 字节）：浮点写 double，其它先把 `Codec<T>::encode` 的值落槽。
 * `Record<Tag>` / `Enum<Tag>` 视图是多槽值，单槽写会变成错误别名 —— 编译期拒绝，
 * 结构体整体写入请按字段 `field` / `set` 逐个来（或复制源地址）。
 */
template <typename T>
void writeSlot(Context &context, std::int64_t address, const T &value)
{
    static_assert(!IsRecordView<T>::value,
                  "胶水层不能把 struct/enum 视图当成单槽整体写入；请按字段 set 或复制源地址");
    if constexpr (std::is_floating_point_v<T>)
    {
        double raw = static_cast<double>(value);
        if (!context.writeBytes(address, reinterpret_cast<const char *>(&raw), sizeof(raw)))
            context.fail("胶水层写入槽失败（越界或空地址）");
    }
    else
    {
        const Value encoded = Codec<T>::encode(context, value);
        std::int64_t raw = encoded.isText() ? context.makeString(encoded.text) : encoded.asInt();
        if (!context.writeBytes(address, reinterpret_cast<const char *>(&raw), sizeof(raw)))
            context.fail("胶水层写入槽失败（越界或空地址）");
    }
}
}  // namespace detail

template <typename T>
T Reference<T>::read(Context &context) const
{
    if (address == 0)
    {
        context.fail("胶水层收到无效的零地址引用");
        return T{};
    }
    return detail::readSlot<T>(context, address);
}

template <typename T>
void Reference<T>::write(Context &context, const T &value) const
{
    if (address == 0)
    {
        context.fail("胶水层收到无效的零地址引用");
        return;
    }
    detail::writeSlot<T>(context, address, value);
}

template <typename Tag>
template <typename T>
T Record<Tag>::field(std::size_t slot) const
{
    if (context_ == nullptr || address_ == 0)
    {
        if (context_ != nullptr) context_->fail("胶水层读取了空的 struct/enum 值");
        return T{};
    }
    return detail::readSlot<T>(*context_, address_ + static_cast<std::int64_t>(slot * 8u));
}

template <typename Tag>
template <typename T>
void Record<Tag>::set(std::size_t slot, const T &value) const
{
    if (context_ == nullptr || address_ == 0)
    {
        if (context_ != nullptr) context_->fail("胶水层写入了空的 struct/enum 值");
        return;
    }
    detail::writeSlot<T>(*context_, address_ + static_cast<std::int64_t>(slot * 8u), value);
}

#ifdef MAGEL_CUSTOM_ALLOCATOR
template <>
struct Codec<std::string>
{
    static std::string decode(Context &context, const Value &value) { return Codec<Memory::String>::decode(context, value); }
    static Value encode(Context &context, const std::string &value) { return Codec<Memory::String>::encode(context, Memory::String(value)); }
};
#endif

template <typename Result, typename... Args>
class Function<Result(Args...)>
{
public:
    /**
     * @brief 构造未绑定运行时的空函数值。
     */
    Function() = default;
    /**
     * @brief 将 Magel 函数值关联到本次宿主调用。
     * @param context 所属 VM 或 AOT 的宿主上下文。
     * @param address Magel 运行时函数地址或闭包句柄，零表示空函数。
     * @note 上下文与闭包的生命周期必须覆盖每次同步调用。
     */
    Function(Context *context, std::int64_t address) : context_(context), address_(address) {}

    /**
     * @brief 编码实参并同步调用 Magel 函数，显式报告运行时调用是否成功。
     * @param result 成功时接收原始返回槽；失败时重置为空整数槽。
     * @param arguments 按声明签名传给 Magel 的实参。
     * @return 正常返回时为 true；空函数或运行时错误时为 false，错误留在运行时中。
     * @note 返回槽按 Result 的 Codec 解码；void 返回槽应忽略。自定义 Codec 的 C++ 异常沿用宿主异常处理规则。
     */
    [[nodiscard]] bool tryInvoke(Value &result, Args... arguments) const
    {
        result = Value{};
        if (context_ == nullptr || address_ == 0)
        {
            if (context_ != nullptr) context_->fail("胶水层调用了空函数指针");
            return false;
        }
        Memory::Vector<Value> encoded{
            Codec<std::decay_t<Args>>::encode(*context_, std::forward<Args>(arguments))...
        };
        if (context_->invoke(address_, encoded, result)) return true;
        result = Value{};
        return false;
    }

    /**
     * @brief 以 C++ 调用语法执行函数，并按声明签名解码返回值。
     * @param arguments 按声明签名传给 Magel 的实参。
     * @return 成功时返回解码值；失败时非 void 签名返回缺省值，运行时仍保留错误。
     * @note 需要区分合法缺省值与失败时使用 tryInvoke；void 签名不产生返回值。
     */
    Result operator()(Args... arguments) const
    {
        Value result;
        if (!tryInvoke(result, std::forward<Args>(arguments)...))
        {
            if constexpr (!std::is_void_v<Result>)
                return Result{};
            else
                return;
        }
        if constexpr (!std::is_void_v<Result>)
            return Codec<std::decay_t<Result>>::decode(*context_, result);
    }

    /**
     * @brief 取得仅供同一运行时上下文使用的函数地址或闭包句柄。
     * @return 原始 Magel 函数值，空函数为零。
     */
    [[nodiscard]] std::int64_t address() const { return address_; }
    /**
     * @brief 检查函数值是否已绑定上下文且地址非零。
     * @return 非空时为 true；不检查地址指向的实际函数签名。
     */
    [[nodiscard]] explicit operator bool() const { return context_ != nullptr && address_ != 0; }

private:
    Context *context_ = nullptr;
    std::int64_t address_ = 0;
};

/**
 * @brief 在 Magel 函数槽和当前宿主调用的类型化函数包装之间转换。
 */
template <typename Result, typename... Args>
struct Codec<Function<Result(Args...)>>
{
    /**
     * @brief 将函数地址或闭包句柄绑定到当前调用上下文。
     * @param context 当前 VM 或 AOT 的宿主上下文。
     * @param value 保存函数地址或闭包句柄的返回槽。
     * @return 当前宿主调用期间有效的函数包装。
     */
    static Function<Result(Args...)> decode(Context &context, const Value &value)
    {
        return Function<Result(Args...)>{ &context, value.asInt() };
    }
    /**
     * @brief 把现有函数包装编码回 Magel 函数槽。
     * @param context 接收函数值的同一运行时上下文。
     * @param value 生命周期覆盖本次传递的函数包装。
     * @return 保存原函数地址或闭包句柄的整数槽。
     * @note 不复制闭包，不延长其捕获环境的生命周期。
     */
    static Value encode(Context &context, const Function<Result(Args...)> &value)
    {
        (void)context;
        return Value::fromInt(value.address());
    }
};

template <typename Tag, typename Rep>
struct Codec<Handle<Tag, Rep>>
{
    static Handle<Tag, Rep> decode(Context &context, const Value &value)
    {
        return Handle<Tag, Rep>{ Codec<Rep>::decode(context, value) };
    }
    static Value encode(Context &context, Handle<Tag, Rep> value)
    {
        return Codec<Rep>::encode(context, value.value);
    }
};

template <typename T>
struct Codec<Reference<T>>
{
    static Reference<T> decode(Context &, const Value &value)
    {
        if (value.asInt() == 0) throw std::runtime_error("胶水层收到无效的零地址引用");
        return { value.asInt() };
    }
    static Value encode(Context &, Reference<T> value)
    {
        if (value.address == 0) throw std::runtime_error("胶水层不能返回零地址引用");
        return Value::fromInt(value.address);
    }
};

namespace detail
{
/**
 * @brief 解码标准 Option<引用> 的标签与载荷。
 * @param context 运行上下文。
 * @param value 枚举存储地址。
 * @return 非空元素地址或缺省值。
 */
inline std::optional<std::int64_t> decodeOptionalReference(Context &context, const Value &value)
{
    if (value.asInt() == 0) throw std::runtime_error("Option 引用缺少枚举存储");
    const auto tag = readSlot<std::int64_t>(context, value.asInt());
    if (tag == 1) return std::nullopt;
    if (tag != 0) throw std::runtime_error("Option 引用标签无效");
    const auto address = readSlot<std::int64_t>(context, value.asInt() + 8);
    if (address == 0) throw std::runtime_error("Option::Value 不能包含零地址引用");
    return address;
}

/**
 * @brief 编码标准 Option<引用>，None 不构造引用。
 * @param context 运行上下文。
 * @param reference 可选元素地址。
 * @return 枚举存储地址。
 */
inline Value encodeOptionalReference(Context &context, std::optional<std::int64_t> reference)
{
    if (reference && *reference == 0) throw std::runtime_error("Option::Value 不能包含零地址引用");
    const auto address = context.makeTemporary(16);
    if (!address) throw std::runtime_error("Option 引用存储分配失败");
    writeSlot<std::int64_t>(context, address, reference ? 0 : 1);
    writeSlot<std::int64_t>(context, address + 8, reference.value_or(0));
    return Value::fromInt(address);
}
}  // namespace detail

/**
 * @brief C++ optional 引用与 MAGEL Option<引用> 的双向转换。
 */
template <typename T>
struct Codec<std::optional<Reference<T>>>
{
    static std::optional<Reference<T>> decode(Context &context, const Value &value)
    {
        const auto address = detail::decodeOptionalReference(context, value);
        if (!address) return std::nullopt;
        return Reference<T>{ *address };
    }
    static Value encode(Context &context, const std::optional<Reference<T>> &value)
    {
        return detail::encodeOptionalReference(context,
                                               value ? std::optional<std::int64_t>(value->address) : std::nullopt);
    }
};

/* ============================================================
 * 胶水助手：有状态句柄池 / 结构体值构造 / 失败宏 / 泛型方法绑定
 * ============================================================ */

/**
 * 有状态宿主对象的句柄池：独立 Tag 防句柄类型混淆。所有有状态胶水共用这一套。
 */
template <typename Tag, typename T>
struct HandlePool
{
    std::uint64_t next = 1u;
    std::map<std::uint64_t, T> objects;

    std::uint64_t add(T value)
    {
        const std::uint64_t id = next++;
        objects.emplace(id, std::move(value));
        return id;
    }
    T *get(std::uint64_t id)
    {
        const auto found = objects.find(id);
        return found == objects.end() ? nullptr : &found->second;
    }
    void remove(std::uint64_t id) { objects.erase(id); }
};

namespace detail
{
inline std::int64_t writeStructFields(Context &ctx, const double *fields, std::size_t count)
{
    const std::int64_t address = ctx.makeTemporary(count * 8u);
    for (std::size_t i = 0; i < count; ++i)
    {
        double raw = fields[i];
        ctx.writeBytes(address + static_cast<std::int64_t>(i * 8u),
                       reinterpret_cast<const char *>(&raw), sizeof(raw));
    }
    return address;
}
}  // namespace detail

/**
 * 用一串 double 构造一个 MAGEL 值结构体，返回它的地址（bindTyped 的 Result 用 int64）。
 */
inline std::int64_t makeStruct(Context &ctx, std::initializer_list<double> fields)
{
    return detail::writeStructFields(ctx, fields.begin(), fields.size());
}

/**
 * 构造一个 MAGEL 枚举值：0 号槽是标签（int），后面是载荷（double/int 槽）。
 */
inline std::int64_t makeEnum(Context &ctx, std::int64_t tag, std::initializer_list<double> payload)
{
    const std::int64_t address = ctx.makeTemporary((payload.size() + 1u) * 8u);
    std::int64_t rawTag = tag;
    ctx.writeBytes(address, reinterpret_cast<const char *>(&rawTag), sizeof(rawTag));
    for (std::size_t i = 0; i < payload.size(); ++i)
    {
        double raw = payload.begin()[i];
        ctx.writeBytes(address + static_cast<std::int64_t>((i + 1u) * 8u),
                       reinterpret_cast<const char *>(&raw), sizeof(raw));
    }
    return address;
}

/**
 * 回调里"报错并返回零值"的统一写法，避免 `return 0;` 与 `return T{}` 混写导致的
 *  deduced-return-type 不一致。
 */
#define GLUE_FAIL(ctx, message, zeroValue) \
    do                                     \
    {                                      \
        (ctx).fail(message);               \
        return (zeroValue);                \
    } while (0)

namespace detail
{
template <typename Result, typename... Args, typename Function, std::size_t... Index>
Value invokeTyped(Function &function, Context &context, const Memory::Vector<Value> &values,
                  std::index_sequence<Index...>)
{
    if constexpr (std::is_void_v<Result>)
    {
        std::invoke(function, Codec<std::decay_t<Args>>::decode(context, values[Index])...);
        return Value::fromInt(0);
    }
    else
    {
        return Codec<std::decay_t<Result>>::encode(
            context,
            std::invoke(function, Codec<std::decay_t<Args>>::decode(context, values[Index])...));
    }
}

template <typename Result, typename... Args, typename Function, std::size_t... Index>
Value invokeTypedWithContext(Function &function, Context &context,
                             const Memory::Vector<Value> &values,
                             std::index_sequence<Index...>)
{
    if constexpr (std::is_void_v<Result>)
    {
        std::invoke(function, context,
                    Codec<std::decay_t<Args>>::decode(context, values[Index])...);
        return Value::fromInt(0);
    }
    else
    {
        return Codec<std::decay_t<Result>>::encode(
            context,
            std::invoke(function, context,
                        Codec<std::decay_t<Args>>::decode(context, values[Index])...));
    }
}
}  // namespace detail

/**
 * @brief 与 Magel 单态化器一致地生成宿主泛型实例符号。
 * @param name 函数或类型名称。
 * @param typeArguments 已归一化的类型实参。
 * @return 工具链与宿主注册表共用的实例符号。
 */
inline Memory::String specialize(const Memory::String &name,
                                 std::initializer_list<Memory::String> typeArguments)
{
    Memory::String result = name;
    for (const auto &argument : typeArguments)
        result += "__" + magel_generic_name::argumentSymbol(argument);
    return result;
}

/**
 * @brief 将模块限定名转换为 magel_ 前缀的下划线宿主符号。
 * @param mcName 源码中的模块限定名。
 * @return 约定符号；名称非法时返回空串。
 */
inline Memory::String conventionSymbol(const Memory::String &mcName)
{
    Memory::String result = "magel_";
    bool previousWasLowerOrDigit = false;
    for (std::size_t i = 0; i < mcName.size(); ++i)
    {
        const unsigned char current = static_cast<unsigned char>(mcName[i]);
        if (current == ':')
        {
            if (i + 1 >= mcName.size() || mcName[i + 1] != ':' || i == 0 || i + 2 >= mcName.size() || result.back() == '_')
                return {};
            result += '_';
            previousWasLowerOrDigit = false;
            ++i;
            continue;
        }
        if (current == '_')
        {
            if (result.back() == '_') return {};
            result += '_';
            previousWasLowerOrDigit = false;
            continue;
        }
        if (!std::isalnum(current)) return {};
        const bool upper = std::isupper(current) != 0;
        const bool nextLower = i + 1 < mcName.size() && std::islower(static_cast<unsigned char>(mcName[i + 1])) != 0;
        if (upper && result.back() != '_' && (previousWasLowerOrDigit || (i > 0 && std::isupper(static_cast<unsigned char>(mcName[i - 1])) && nextLower)))
            result += '_';
        result += static_cast<char>(std::tolower(current));
        previousWasLowerOrDigit = std::islower(current) || std::isdigit(current);
    }
    if (result.size() == 3 || result.back() == '_') return {};
    return result;
}

/* ============================================================
 * 胶水侧声明 MAGEL 类型：结构体 / 枚举 / 接口 / 宿主实现
 * ============================================================ */

/**
 * 一条字段声明：`type name`（结构体字段、枚举载荷、接口形参共用）。
 */
struct DeclarationField
{
    Memory::String type;
    Memory::String name;
};

/**
 * 枚举的一个变体：`Variant` 或 `Variant(T field, ...)`；载荷形参名参与构造器实参。
 */
struct DeclarationVariant
{
    Memory::String name;
    Memory::Vector<DeclarationField> payload;
};

/**
 * 一条函数签名：`result name(params...)`，接口方法与宿主函数共用。
 */
struct DeclarationMethod
{
    Memory::String result;
    Memory::String name;
    Memory::Vector<DeclarationField> params;
};

/**
 * 一条**宿主实现**的成员函数：`result name(params...)` 走宿主符号 `symbol`。
 */
struct DeclarationMember
{
    Memory::String result;
    Memory::String name;
    Memory::String symbol;
    Memory::Vector<DeclarationField> params;
};

namespace detail
{
/**
 * 把任意 MAGEL 名字 / 类型拼成合法的标识符片段（`std::lang::List<int64>` → `lang__List_int64_`）。
 */
inline Memory::String declarationIdentifier(const Memory::String &text)
{
    Memory::String name;
    for (const char value : text)
        name += (std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_') ? value : '_';
    return name;
}

inline Memory::String describeDeclaration(const Memory::String &result,
                                          const Memory::String &name,
                                          const Memory::Vector<DeclarationField> &params)
{
    Memory::String text = result + " " + name + "(";
    for (std::size_t index = 0; index < params.size(); ++index)
    {
        if (index != 0u) text += ", ";
        text += params[index].type + " " + params[index].name;
    }
    return text + ")";
}
}  // namespace detail

/**
 * @brief 胶水层直接声明 MAGEL 类型，省掉 `.mgl` 里的同一份定义。
 *
 * 宿主应用用 C++ 写一次结构体 / 枚举 / 接口（以及宿主 `impl`），编译器会在**合并源码
 * 末尾**追加这些声明，于是 `.mgl` 里可以直接使用，不用再写 `struct` / `enum` /
 * `interface` / `impl`：
 *
 * @code
 * magel_glue::Declarations &d = registry.declarations();
 * d.structType("MyPoint", {{"int64", "x"}, {"int64", "y"}});
 * d.enumType("MySignal", {{"Idle", {}}, {"Tone", {{"int64", "hz"}}}});
 * d.interfaceType("MyInspect", {{"int64", "area", {}}});
 * d.interfaceType("MyMeasured", {{"int64", "unit", {}}}, {"MyInspect"});   // 接口继承
 * d.implementation("MyPoint", "MyInspect", "my_inspect");
 * @endcode
 *
 * 生成的是普通 Magel 源码：结构体/枚举是**值语义**的语言类型，接口是编译期契约，
 * 成员函数由 `impl` 指向宿主符号（接收者是第一个 C++ 形参，用 `Record<Tag>` /
 * `Enum<Tag>` 查看）。
 *
 * **接口继承**：`interfaceType` 的第三个参数是基接口列表，生成
 * `interface Name : Base, ... { … };`；`interfaceInherit(name, bases)` 是"只继承、
 * 不加新方法"的简写。继承来的抽象方法一样要在 `impl` 里绑符号，默认方法一并继承。
 *
 * **只在用到时才注入**：`usedBy(源码)` 扫描程序里有没有出现这里声明过的名字，
 * 没出现的程序（绝大多数样例）一个字节都不加，类型编号（`__typeTag(...)` 的数值）
 * 与以前完全一致；用到的程序才追加，且追加在合并源码**末尾**，尽量少动编号。
 *
 * **多方法接口**：`impl` 右侧写前缀，方法 `m` 按 `<前缀>.m` 查宿主符号；单方法接口
 * 沿用原符号名（兼容既有 `impl Owner : Interface = symbol;` 写法）。绑定入口是
 * `Registry::bindTypedMethod`。
 */
class Declarations
{
public:
    /**
     * `type Name = ABI;`：名义宿主类型（ABI 必须是 bool / 整数 / 浮点 / string / 指针）。
     */
    Declarations &hostType(const Memory::String &name, const Memory::String &abi)
    {
        text_ += "type " + name + " = " + abi + ";\n";
        names_.push_back(name);
        return *this;
    }

    /**
     * `struct Name { T field; ... };`
     */
    Declarations &structType(const Memory::String &name,
                             std::initializer_list<DeclarationField> fields)
    {
        text_ += "struct " + name + " {";
        for (const DeclarationField &field : fields)
            text_ += " " + field.type + " " + field.name + ";";
        text_ += " };\n";
        names_.push_back(name);
        return *this;
    }

    /**
     * `enum Name { A, B(T payload, ...) };`
     */
    Declarations &enumType(const Memory::String &name,
                           std::initializer_list<DeclarationVariant> variants)
    {
        text_ += "enum " + name + " {";
        bool first = true;
        for (const DeclarationVariant &variant : variants)
        {
            text_ += first ? " " : ", ";
            first = false;
            text_ += variant.name;
            if (!variant.payload.empty())
            {
                text_ += "(";
                for (std::size_t index = 0; index < variant.payload.size(); ++index)
                {
                    if (index != 0u) text_ += ", ";
                    text_ += variant.payload[index].type + " " + variant.payload[index].name;
                }
                text_ += ")";
            }
        }
        text_ += " };\n";
        names_.push_back(name);
        return *this;
    }

    /**
     * `interface Name [: Base, ...] { result method(params); ... };`
     *
     * `bases` 非空时生成 `interface Name : Base, ... { … };` —— 与 `.mgl` 里手写接口继承
     * 完全等价：基接口的抽象方法与默认方法一并继承，派生接口里同名同签名的方法**覆盖**
     * 基接口的那一条（同名不同签名报错）。基接口必须先用 `interfaceType` 声明。
     */
    Declarations &interfaceType(const Memory::String &name,
                                std::initializer_list<DeclarationMethod> methods,
                                std::initializer_list<Memory::String> bases = {})
    {
        text_ += "interface " + name;
        if (bases.size() != 0u)
        {
            text_ += " : ";
            bool first = true;
            for (const Memory::String &base : bases)
            {
                text_ += first ? "" : ", ";
                first = false;
                text_ += base;
            }
        }
        text_ += " {";
        for (const DeclarationMethod &method : methods)
            text_ += " " + detail::describeDeclaration(method.result, method.name, method.params) + ";";
        text_ += " };\n";
        names_.push_back(name);
        return *this;
    }

    /**
     * `interface Name : Base, ... { };` —— 只加继承、不添新方法。
     *
     * 等价于 `interfaceType(name, {}, {bases...})`，便于"派生接口只是基接口的组合 /
     * 改名"这种声明。
     */
    Declarations &interfaceInherit(const Memory::String &name,
                                   std::initializer_list<Memory::String> bases)
    {
        return interfaceType(name, std::initializer_list<DeclarationMethod>{}, bases);
    }

    /**
     * 一次声明某类型的一批**宿主实现成员函数**（结构体 / 枚举 / 名义宿主类型都行）。
     *
     * 每条方法合成一个单方法接口 `<Owner>__glue__<方法>` 加一条 `impl`，指向该方法自带
     * 的宿主符号：
     *
     * @code
     * d.memberFunctions("MyRect", {{"int64", "area", "my_rect_area", {}},
     *                              {"int64", "area", "my_rect_area_scaled", {{"int64", "factor"}}}});
     * @endcode
     *
     * MAGEL 里照常写 `r.area()` / `r.area(2)`，胶水按各自的符号名绑定
     * （`bindTypedImplementation<返回, 接收者, 形参…>(符号, …)`；接收者是第一个 C++ 形参，
     * 宿主结构体用 `Record<Tag>`、枚举用 `Enum<Tag>`）。每条方法自带符号，所以**重载**也
     * 不会撞名 —— 这是 `<impl 符号>.<方法名>` 约定做不到的。
     */
    Declarations &memberFunctions(const Memory::String &owner,
                                  std::initializer_list<DeclarationMember> members)
    {
        for (const DeclarationMember &member : members)
        {
            const Memory::String interfaceName = glueInterfaceName(owner, member);
            text_ += "interface " + interfaceName + " { " + detail::describeDeclaration(member.result, member.name, member.params) + "; };\n";
            text_ += "impl " + owner + " : " + interfaceName + " = " + member.symbol + ";\n";
        }
        names_.push_back(owner);
        return *this;
    }

    /**
     * 无函数体的宿主函数签名：`result name(params);`（运行时按名字查注册表）。
     */
    Declarations &hostFunction(const Memory::String &result, const Memory::String &name,
                               std::initializer_list<DeclarationField> params)
    {
        const Memory::Vector<DeclarationField> fields(params);
        text_ += detail::describeDeclaration(result, name, fields) + ";\n";
        names_.push_back(name);
        return *this;
    }

    /**
     * `impl Owner : Interface = symbol;`（多方法接口见类注释里的 `<symbol>.方法` 约定）。
     */
    Declarations &implementation(const Memory::String &owner,
                                 const Memory::String &interfaceName,
                                 const Memory::String &symbol)
    {
        text_ += "impl " + owner + " : " + interfaceName + " = " + symbol + ";\n";
        names_.push_back(owner);
        names_.push_back(interfaceName);
        return *this;
    }

    [[nodiscard]] bool empty() const { return text_.empty(); }
    [[nodiscard]] const Memory::String &text() const { return text_; }

    /**
     * 程序源码里有没有出现过任何一个声明名（按完整标识符匹配）。
     */
    [[nodiscard]] bool usedBy(const Memory::String &source) const
    {
        for (const Memory::String &name : names_)
        {
            if (name.empty()) continue;
            std::size_t at = source.find(name);
            while (at != Memory::String::npos)
            {
                const bool leftFree =
                    at == 0u || !(std::isalnum(static_cast<unsigned char>(source[at - 1u])) != 0 || source[at - 1u] == '_');
                const std::size_t after = at + name.size();
                const bool rightFree =
                    after >= source.size() || !(std::isalnum(static_cast<unsigned char>(source[after])) != 0 || source[after] == '_');
                if (leftFree && rightFree) return true;
                at = source.find(name, at + 1u);
            }
        }
        return false;
    }

private:
    /**
     * 合成接口名：只留标识符字符，免得限定名（`app::Point`）拼出非法标识符；
     *  再把形参类型拼进去，同名不同参的重载各拿一个接口名。
     */
    static Memory::String glueInterfaceName(const Memory::String &owner,
                                            const DeclarationMember &member)
    {
        Memory::String name = detail::declarationIdentifier(owner) + "__glue__" + detail::declarationIdentifier(member.name);
        for (const DeclarationField &param : member.params)
            name += "__" + detail::declarationIdentifier(param.type);
        return name;
    }

    Memory::String text_;
    Memory::Vector<Memory::String> names_;
};

/**
 * @brief Magel 到 C++ 的胶水函数表。
 *
 * MAGEL 侧用无函数体的 `T name(...);` 声明同名宿主函数。字符串参数是 VM 地址，使用
 * Context::readString 读取；返回新字符串可使用 Context::makeString。
 */
class Registry
{
public:
    bool bind(const Memory::String &name, std::size_t arity, Callback callback,
              Memory::String *error = nullptr)
    {
        return native_.add(name, arity, std::move(callback), error);
    }

    bool unbind(const Memory::String &name) { return native_.remove(name); }

    /**
     * 用 Codec 自动桥接 C++ 内置类型，模板参数依次为返回类型、形参类型。
     */
    template <typename Result, typename... Args, typename Function>
    bool bindTyped(const Memory::String &name, Function &&function, Memory::String *error = nullptr)
    {
        using Stored = std::decay_t<Function>;
        return bind(
            name, sizeof...(Args),
            [callable = Stored(std::forward<Function>(function))](
                Context &context, const Memory::Vector<Value> &values) mutable {
                return detail::invokeTyped<Result, Args...>(
                    callable, context, values, std::index_sequence_for<Args...>{});
            },
            error);
    }

    /**
     * 与 bindTyped 相同，但 C++ 函数的第一个参数是 Context&。
     */
    template <typename Result, typename... Args, typename Function>
    bool bindTypedWithContext(const Memory::String &name, Function &&function,
                              Memory::String *error = nullptr)
    {
        using Stored = std::decay_t<Function>;
        return bind(
            name, sizeof...(Args),
            [callable = Stored(std::forward<Function>(function))](
                Context &context, const Memory::Vector<Value> &values) mutable {
                return detail::invokeTypedWithContext<Result, Args...>(
                    callable, context, values, std::index_sequence_for<Args...>{});
            },
            error);
    }

    bool bindGeneric(const Memory::String &name,
                     std::initializer_list<Memory::String> typeArguments,
                     std::size_t arity, Callback callback, Memory::String *error = nullptr)
    {
        return bind(specialize(name, typeArguments), arity, std::move(callback), error);
    }

    template <typename Result, typename... Args, typename Function>
    bool bindTypedGeneric(const Memory::String &name,
                          std::initializer_list<Memory::String> typeArguments,
                          Function &&function, Memory::String *error = nullptr)
    {
        return bindTyped<Result, Args...>(specialize(name, typeArguments),
                                          std::forward<Function>(function), error);
    }

    /**
     * @brief 按类型实参特化名与方法名登记泛型方法的宿主绑定。
     * @param name 泛型类型名。
     * @param typeArguments 类型实参。
     * @param method 方法名。
     * @param function 宿主函数。
     * @param error 失败诊断，可为空。
     * @return 绑定成功返回 true。前端须使用相同的特化命名约定。
     */
    template <typename Result, typename... Args, typename Function>
    bool bindTypedGenericMethod(const Memory::String &name,
                                std::initializer_list<Memory::String> typeArguments,
                                const Memory::String &method,
                                Function &&function, Memory::String *error = nullptr)
    {
        return bindTyped<Result, Args...>(specialize(name, typeArguments) + "." + method,
                                          std::forward<Function>(function), error);
    }

    /**
     * 绑定 `impl Owner : Interface = symbol;` 的宿主实现。
     * Args 的第一个类型是接口接收者，后面依次是接口方法参数。
     */
    template <typename Result, typename... Args, typename Function>
    bool bindTypedImplementation(const Memory::String &symbol, Function &&function,
                                 Memory::String *error = nullptr)
    {
        return bindTyped<Result, Args...>(symbol, std::forward<Function>(function), error);
    }

    template <typename Result, typename... Args, typename Function>
    bool bindTypedGenericImplementation(
        const Memory::String &symbol, std::initializer_list<Memory::String> typeArguments,
        Function &&function, Memory::String *error = nullptr)
    {
        return bindTypedGeneric<Result, Args...>(symbol, typeArguments,
                                                 std::forward<Function>(function), error);
    }

    /**
     * 绑定**多方法接口**的一个成员函数。
     *
     * `impl Owner : Interface = prefix;` 且接口声明了不止一个方法时，方法 `m` 在
     * 注册表里查 `prefix.m`（单方法接口仍查 `prefix` 原名，见 `Declarations`）。Args 的
     * 第一个类型是接收者（宿主结构体用 `Record<Tag>`，宿主类型/句柄用对应 Codec 类型），
     * 后面依次是接口方法参数。
     */
    template <typename Result, typename... Args, typename Function>
    bool bindTypedMethod(const Memory::String &prefix, const Memory::String &method,
                         Function &&function, Memory::String *error = nullptr)
    {
        return bindTyped<Result, Args...>(prefix + "." + method,
                                          std::forward<Function>(function), error);
    }

    /**
     * 胶水层声明的 MAGEL 类型/接口/宿主签名；编译前追加到合并源码末尾。
     */
    Declarations &declarations() { return declarations_; }
    [[nodiscard]] const Declarations &declarations() const { return declarations_; }
    [[nodiscard]] const Memory::String &declarationSource() const
    {
        return declarations_.text();
    }

    [[nodiscard]] const stackvm::NativeRegistry &native() const { return native_; }

    /**
     * 将本表用于后续源码即时执行；Registry 必须保持存活。
     */
    void install() const { M_SetNativeRegistry(&native_); }

private:
    stackvm::NativeRegistry native_;
    Declarations declarations_;
};

namespace detail
{
// MAGEL values occupy 8-byte slots; native layout is created field by field.
template <typename T>
struct CAbiStorage
{
    static constexpr std::size_t slots = 1;
    static T read(Context &context, std::int64_t address)
    {
        using Raw = std::conditional_t<std::is_floating_point_v<T>, double, std::int64_t>;
        Raw raw{};
        if (!context.readBytes(address, reinterpret_cast<char *>(&raw), 8))
            throw std::runtime_error("ABI read outside VM storage");
        return static_cast<T>(raw);
    }
    static void write(Context &context, std::int64_t address, T value)
    {
        using Raw = std::conditional_t<std::is_floating_point_v<T>, double, std::int64_t>;
        Raw raw = static_cast<Raw>(value);
        if (!context.writeBytes(address, reinterpret_cast<const char *>(&raw), 8))
            context.fail("ABI write outside VM storage");
    }
};

template <typename T>
struct CAbiPointerState
{
    Context *context;
    std::int64_t address;
    T value;
    bool writable;
    CAbiPointerState *next = nullptr;
    std::weak_ptr<CAbiPointerState> owner;
    static CAbiPointerState *&head()
    {
        static thread_local CAbiPointerState *value = nullptr;
        return value;
    }
    CAbiPointerState(Context *c, std::int64_t a, T v, bool w) : context(c), address(a), value(std::move(v)), writable(w) {}
    ~CAbiPointerState()
    {
        if (writable) CAbiStorage<T>::write(*context, address, value);
        auto **entry = &head();
        while (*entry && *entry != this) entry = &(*entry)->next;
        if (*entry) *entry = next;
    }
};
template <typename T>
struct CAbiPointer
{
    using Element = std::remove_const_t<T>;
    using State = CAbiPointerState<Element>;
    std::shared_ptr<State> state;
    T *get() const { return state ? &state->value : nullptr; }
};
template <typename T>
std::shared_ptr<CAbiPointerState<T>> cAbiPointerState(Context &context, std::int64_t address, bool writable)
{
    // Share the temporary when several arguments refer to the same VM object.
    using State = CAbiPointerState<T>;
    for (State *it = State::head(); it; it = it->next)
    {
        if (it->context == &context && it->address == address)
        {
            auto state = it->owner.lock();
            state->writable = state->writable || writable;
            return state;
        }
    }
    auto state = Memory::makeShared<State>(&context, address, CAbiStorage<T>::read(context, address), writable);
    state->owner = state;
    state->next = State::head();
    State::head() = state.get();
    return state;
}

template <typename T>
struct CAbiArray
{
};
inline constexpr std::size_t CAbiMaxBufferBytes = 64u * 1024u * 1024u;
inline std::size_t cAbiCount(std::int64_t count, std::size_t stride)
{
    if (count < 0 || !stride || static_cast<std::uint64_t>(count) > CAbiMaxBufferBytes / stride)
        throw std::runtime_error("ABI buffer length outside supported range");
    return static_cast<std::size_t>(count);
}
template <typename T>
struct CAbiArrayValue
{
    struct State
    {
        Context *context;
        std::int64_t address;
        Memory::Vector<T> values;
        State *next = nullptr;
        std::weak_ptr<State> owner;
        static State *&head()
        {
            static thread_local State *value = nullptr;
            return value;
        }
        State(Context *c, std::int64_t a, Memory::Vector<T> v) : context(c), address(a), values(std::move(v)) {}
        ~State()
        {
            for (std::size_t i = 0; i < values.size(); ++i)
                CAbiStorage<T>::write(*context, address + i * CAbiStorage<T>::slots * 8, values[i]);
            auto **entry = &head();
            while (*entry && *entry != this) entry = &(*entry)->next;
            if (*entry) *entry = next;
        }
    };
    std::shared_ptr<State> state;
    T *get() const { return state && !state->values.empty() ? state->values.data() : nullptr; }
};
template <typename T, bool Array>
struct CAbiReturnedPointer
{
    const T *pointer;
    std::size_t count;
};

/**
 * @brief C ABI 可空单对象指针的签名标记，对应 MAGEL Option<T mut&/const&>。
 */
template <typename T>
struct CAbiOptionalReference
{
};
/**
 * @brief 可空 C 指针参数持有临时副本及回写状态。
 */
template <typename T>
struct CAbiOptionalArgument
{
    CAbiPointer<T> reference;
};
/**
 * @brief 可空 C 指针返回值；离开宿主调用时复制进 MAGEL。
 */
template <typename T>
struct CAbiOptionalResult
{
    const std::remove_const_t<T> *pointer;
};

template <typename T>
struct CAbiArgument
{
    using Native = T;
    using Value = T;
    static T pass(const T &value) { return value; }
};

/**
 * @brief 标记仅在当前原生调用期间有效的同步 C 回调。
 * @tparam Signature 标量回调签名。
 */
template <typename Signature>
struct CAbiCallback
{
};
template <typename Signature>
class CAbiCallbackValue;
/**
 * @brief 使用线程局部有界跳板持有同步回调，错误延迟到 C 返回后传播。
 * @tparam R 返回类型。
 * @tparam A 参数类型列表。
 */
template <typename R, typename... A>
class CAbiCallbackValue<R(A...)>
{
    struct State
    {
        Context *context;
        std::int64_t address;
        std::size_t slot;
        std::exception_ptr error;
        /**
         * @brief 清除回调槽，使调用结束后的指针不再访问 VM。
         */
        ~State() { slots()[slot] = nullptr; }
    };
    std::shared_ptr<State> state_;
    /**
     * @brief 取得当前线程的同步回调槽。
     * @return 槽数组引用。
     */
    static std::array<State *, 32> &slots()
    {
        static thread_local std::array<State *, 32> storage{};
        return storage;
    }
    /**
     * @brief 调用槽内函数并阻止异常越过 C 边界。
     * @tparam I 槽编号。
     * @param args C 侧标量实参。
     * @return 回调结果，失败时为零值。
     */
    template <std::size_t I>
    static R invoke(A... args) noexcept
    {
        State *state = slots()[I];
        if (state && !state->error)
            try
            {
                Memory::Vector<Value> values{ Codec<std::decay_t<A>>::encode(*state->context, args)... };
                Value result;
                if (!state->context->invoke(state->address, values, result)) throw std::runtime_error("ABI callback invocation failed");
                if constexpr (!std::is_void_v<R>)
                    return Codec<R>::decode(*state->context, result);
                else
                    return;
            } catch (...) { state->error = std::current_exception(); }
        if constexpr (!std::is_void_v<R>) return R{};
    }
    /**
     * @brief 生成固定跳板表。
     * @tparam I 槽索引序列。
     * @return 原生跳板表。
     */
    template <std::size_t... I>
    static auto table(std::index_sequence<I...>)
    { return std::array<R (*)(A...), sizeof...(I)>{ &invoke<I>... }; }

public:
    /**
     * @brief 为当前调用保留一个回调槽。
     * @param context VM 调用上下文。
     * @param address VM 函数值地址。
     * @note 宿主不得长期保存或跨线程使用跳板。
     */
    CAbiCallbackValue(Context &context, std::int64_t address)
    {
        if (!address) throw std::runtime_error("ABI callback is null");
        for (std::size_t i = 0; i < slots().size(); ++i)
            if (!slots()[i])
            {
                state_ = std::shared_ptr<State>(new State{ &context, address, i, {} });
                slots()[i] = state_.get();
                return;
            }
        throw std::runtime_error("ABI synchronous callback capacity exhausted");
    }
    /**
     * @brief 返回槽对应的原生函数指针。
     * @return 当前调用期间有效的跳板。
     */
    R (*get() const)(A...)
    {
        static const auto functions = table(std::make_index_sequence<32>{});
        return functions[state_->slot];
    }
    /**
     * @brief 在原生函数返回后传播回调错误。
     */
    void check() const
    {
        if (state_->error) std::rethrow_exception(state_->error);
    }
};
/**
 * @brief 将回调持有者映射为原生函数指针。
 */
template <typename R, typename... A>
struct CAbiArgument<CAbiCallback<R(A...)>>
{
    using Native = R (*)(A...);
    using Value = CAbiCallbackValue<R(A...)>;
    /**
     * @brief 提取同步跳板。
     * @param value 回调持有者。
     * @return 原生函数指针。
     */
    static Native pass(const Value &value) { return value.get(); }
};
/**
 * @brief 普通参数没有延迟的回调错误。
 * @tparam T 参数类型。
 * @param value 已转换的参数。
 */
template <typename T>
void checkCAbiCallback(const T &value)
{ (void)value; }
/**
 * @brief 检查回调是否在原生调用期间失败。
 * @tparam Signature 回调签名。
 * @param value 回调持有者。
 */
template <typename Signature>
void checkCAbiCallback(const CAbiCallbackValue<Signature> &value)
{ value.check(); }
template <typename T>
struct CAbiArgument<T *>
{
    using Native = T *;
    using Value = CAbiPointer<T>;
    static T *pass(const Value &value) { return value.get(); }
};
template <>
struct CAbiArgument<const char *>
{
    using Native = const char *;
    using Value = Memory::String;
    static const char *pass(const Memory::String &value)
    {
        if (value.find(static_cast<char>(0)) != Memory::String::npos) throw std::runtime_error("ABI C string contains NUL; use a byte buffer");
        return value.c_str();
    }
};
template <typename T>
struct CAbiArgument<CAbiArray<T>>
{
    using Native = T *;
    using Value = CAbiArrayValue<T>;
    static T *pass(const Value &value) { return value.get(); }
};
template <typename T>
struct CAbiArgument<CAbiOptionalReference<T>>
{
    using Native = T *;
    using Value = CAbiOptionalArgument<T>;
    static Native pass(const Value &value) { return value.reference.get(); }
};

template <typename T>
struct CAbiResult
{
    using Native = T;
    using Value = T;
    static T copy(T value, std::size_t = 1) { return value; }
};
template <typename T>
struct CAbiResult<T *>
{
    using Native = T *;
    using Value = CAbiReturnedPointer<std::remove_const_t<T>, false>;
    static Value copy(T *value, std::size_t = 1) { return { value, 1 }; }
};
template <typename T>
struct CAbiResult<CAbiArray<T>>
{
    using Native = T *;
    using Value = CAbiReturnedPointer<T, true>;
    static Value copy(T *value, std::size_t count) { return { value, count }; }
};
/**
 * @brief 按 C 字符串契约复制宿主文本，并限制最大返回长度。
 * @param value 宿主提供的有效、以零结尾的字节串。
 * @return 字符串副本；空指针转为空串。
 * @note 宿主仍须保证指针有效；此限制不构成原生内存沙箱。
 */
inline Memory::String copyCAbiString(const char *value)
{
    if (!value) return {};
    std::size_t length = 0;
    while (length < CAbiMaxBufferBytes && value[length]) ++length;
    if (length == CAbiMaxBufferBytes) throw std::runtime_error("ABI C string exceeds maximum length");
    return Memory::String(value, length);
}

template <>
struct CAbiResult<const char *>
{
    using Native = const char *;
    using Value = Memory::String;
    static Memory::String copy(const char *value, std::size_t = 1) { return copyCAbiString(value); }
};
template <typename T>
struct CAbiResult<CAbiOptionalReference<T>>
{
    using Native = T *;
    using Value = CAbiOptionalResult<T>;
    static Value copy(T *value, std::size_t = 1) { return { value }; }
};

template <typename T>
struct CAbiResultCountStride
{
    static constexpr std::size_t value = 1;
};
template <typename T>
struct CAbiResultCountStride<CAbiArray<T>>
{
    static constexpr std::size_t value = CAbiStorage<T>::slots * 8;
};
template <>
struct CAbiResult<void>
{
    using Native = void;
    using Value = void;
};
}  // namespace detail

template <typename T>
struct Codec<detail::CAbiPointer<T>>
{
    static detail::CAbiPointer<T> decode(Context &context, const Value &value)
    {
        using Reference = detail::CAbiPointer<T>;
        Reference result;
        if (!value.asInt()) throw std::runtime_error("ABI reference argument is null");
        if (value.asInt())
            result.state = detail::cAbiPointerState<typename Reference::Element>(
                context, value.asInt(), !std::is_const_v<T>);
        return result;
    }
};

/**
 * @brief 将 VM 函数值转换为一次原生调用持有的同步回调。
 * @tparam Signature 回调签名。
 */
template <typename Signature>
struct Codec<detail::CAbiCallbackValue<Signature>>
{
    /**
     * @brief 为函数值分配线程局部跳板槽。
     * @param context VM 调用上下文。
     * @param value VM 函数值。
     * @return 同步回调持有者。
     */
    static detail::CAbiCallbackValue<Signature> decode(Context &context, const Value &value)
    { return { context, value.asInt() }; }
};

template <typename T>
struct Codec<detail::CAbiArrayValue<T>>
{
    static detail::CAbiArrayValue<T> decode(Context &context, const Value &value)
    {
        using Array = detail::CAbiArrayValue<T>;
        for (auto *it = Array::State::head(); it; it = it->next)
            if (it->context == &context && it->address == value.asInt()) return Array{ it->owner.lock() };
        if (!value.asInt()) throw std::runtime_error("ABI array argument is null");
        const auto length = detail::CAbiStorage<std::int64_t>::read(context, value.asInt() - 8);
        const auto count = detail::cAbiCount(length, detail::CAbiStorage<T>::slots * 8);
        Memory::Vector<T> elements;
        elements.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            elements.push_back(detail::CAbiStorage<T>::read(context,
                                                            value.asInt() + i * detail::CAbiStorage<T>::slots * 8));
        auto state = Memory::makeShared<typename Array::State>(
            &context, value.asInt(), std::move(elements));
        state->owner = state;
        state->next = Array::State::head();
        Array::State::head() = state.get();
        return Array{ std::move(state) };
    }
};
template <typename T, bool Array>
struct Codec<detail::CAbiReturnedPointer<T, Array>>
{
    static Value encode(Context &context, const detail::CAbiReturnedPointer<T, Array> &value)
    {
        const auto count = detail::cAbiCount(static_cast<std::int64_t>(value.count),
                                             detail::CAbiStorage<T>::slots * 8);
        if constexpr (!Array)
            if (!value.pointer) throw std::runtime_error("ABI returned null for nonnull reference");
        if (!value.pointer && count) throw std::runtime_error("ABI returned null for nonempty buffer");
        const auto address = context.makeTemporary(count * detail::CAbiStorage<T>::slots * 8 + (Array ? 8 : 0));
        if (address)
        {
            if constexpr (Array) detail::CAbiStorage<std::int64_t>::write(context, address, count);
            for (std::size_t i = 0; i < count; ++i)
                detail::CAbiStorage<T>::write(context,
                                              address + (Array ? 8 : 0) + i * detail::CAbiStorage<T>::slots * 8, value.pointer[i]);
        }
        return Value::fromInt(address ? address + (Array ? 8 : 0) : 0);
    }
};

/**
 * @brief 将 MAGEL Option 引用转换为可空 C 参数，Value 保持共享副本与 mut 回写。
 */
template <typename T>
struct Codec<detail::CAbiOptionalArgument<T>>
{
    static detail::CAbiOptionalArgument<T> decode(Context &context, const Value &value)
    {
        detail::CAbiOptionalArgument<T> result;
        const auto address = detail::decodeOptionalReference(context, value);
        if (address)
            result.reference.state = detail::cAbiPointerState<std::remove_const_t<T>>(
                context, *address, !std::is_const_v<T>);
        return result;
    }
};

/**
 * @brief 将 C 空指针编码为 None，非空指针复制元素后编码为 Value。
 */
template <typename T>
struct Codec<detail::CAbiOptionalResult<T>>
{
    static Value encode(Context &context, const detail::CAbiOptionalResult<T> &value)
    {
        if (!value.pointer) return detail::encodeOptionalReference(context, std::nullopt);
        const auto reference = Codec<detail::CAbiReturnedPointer<std::remove_const_t<T>, false>>::encode(
            context, { value.pointer, 1 });
        return detail::encodeOptionalReference(context, reference.asInt());
    }
};

/**
 * 加载宿主动态库，并把 C ABI 函数绑定到 MAGEL 的宿主函数声明。
 */
class DynamicLibrary
{
public:
    bool open(const Memory::String &inputPath, Memory::String *error = nullptr)
    {
        Memory::String path = inputPath;
        for (char &ch : path)
            if (ch == '\\') ch = '/';
        handle_.reset();
#if defined(_WIN32)
        HMODULE raw = nullptr;
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                               path.c_str(), -1, nullptr, 0);
        if (length > 0)
        {
            std::wstring wide(static_cast<std::size_t>(length), L'\0');
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1,
                                    wide.data(), length)
                > 0)
                raw = LoadLibraryW(wide.c_str());
        }
        if (raw != nullptr)
            handle_ = std::shared_ptr<void>(raw, [](void *handle) {
                FreeLibrary(static_cast<HMODULE>(handle));
            });
#elif defined(__unix__) || defined(__APPLE__)
        void *raw = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (raw != nullptr)
            handle_ = std::shared_ptr<void>(raw, [](void *handle) { dlclose(handle); });
#else
        (void)path;
#endif
        if (handle_) return true;
        if (error != nullptr)
        {
#if defined(__unix__) || defined(__APPLE__)
            const char *reason = dlerror();
            *error = "无法加载动态库 " + path + (reason == nullptr ? "" : ": " + Memory::String(reason));
#else
            *error = "无法加载动态库 " + path;
#endif
        }
        return false;
    }

    template <typename Signature>
    bool bind(Registry &registry, const Memory::String &mcName,
              const Memory::String &symbol, Memory::String *error = nullptr,
              std::size_t resultCountIndex = static_cast<std::size_t>(-1),
              const Memory::String &releaseSymbol = {}) const
    {
        return Binder<Signature>::bind(*this, registry, mcName, symbol, error, resultCountIndex, releaseSymbol);
    }

    template <typename Signature>
    bool bindConvention(Registry &registry, const Memory::String &mcName,
                        Memory::String *error = nullptr) const
    {
        const Memory::String symbolName = conventionSymbol(mcName);
        if (symbolName.empty())
        {
            if (error != nullptr) *error = "MAGEL 模块函数名不合法：" + mcName;
            return false;
        }
        return bind<Signature>(registry, mcName, symbolName, error);
    }

    void close() { handle_.reset(); }
    void *resolve(const Memory::String &name, Memory::String *error = nullptr) const { return symbol(name, error); }

private:
    template <typename>
    struct Binder;
    std::shared_ptr<void> handle_;

    void *symbol(const Memory::String &name, Memory::String *error) const
    {
        if (!handle_)
        {
            if (error != nullptr) *error = "动态库尚未加载";
            return nullptr;
        }
#if defined(_WIN32)
        void *address = reinterpret_cast<void *>(GetProcAddress(
            static_cast<HMODULE>(handle_.get()), name.c_str()));
#elif defined(__unix__) || defined(__APPLE__)
        dlerror();
        void *address = dlsym(handle_.get(), name.c_str());
#else
        void *address = nullptr;
#endif
        if (address == nullptr && error != nullptr) *error = "动态库中找不到 C 符号：" + name;
        return address;
    }
};

template <typename Result, typename... Args>
struct DynamicLibrary::Binder<Result(Args...)>
{
    static bool bind(const DynamicLibrary &library, Registry &registry,
                     const Memory::String &mcName, const Memory::String &symbol,
                     Memory::String *error, std::size_t resultCountIndex, const Memory::String &releaseSymbol = {})
    {
        void *address = library.symbol(symbol, error);
        if (address == nullptr) return false;
        using Function = typename detail::CAbiResult<Result>::Native (*)(typename detail::CAbiArgument<Args>::Native...);
        const Function function = reinterpret_cast<Function>(address);
        using NativeResult = typename detail::CAbiResult<Result>::Native;
        void *releaseAddress = releaseSymbol.empty() ? nullptr : library.symbol(releaseSymbol, error);
        if (!releaseSymbol.empty() && !releaseAddress) return false;
        if constexpr (!std::is_pointer_v<NativeResult>)
        {
            if (releaseAddress)
            {
                if (error) *error = "ABI release symbol requires a pointer result";
                return false;
            }
        }
        const auto keepAlive = library.handle_;
        return registry.bind(mcName, sizeof...(Args), [function, keepAlive, resultCountIndex, releaseAddress](Context &context, const Memory::Vector<Value> &raw) {
                (void)keepAlive;
                const auto count = resultCountIndex == static_cast<std::size_t>(-1) ? 1u :
                    detail::cAbiCount(raw.at(resultCountIndex).asInt(), detail::CAbiResultCountStride<Result>::value);
                struct OwnedReturn
                {
                    void *pointer = nullptr;
                    void *release = nullptr;
                    /**
                     * @brief 完成 VM 编码后归还宿主指针，异常路径也执行释放。
                     */
                    ~OwnedReturn() { if (pointer && release) reinterpret_cast<void (*)(void *)>(release)(pointer); }
                } owned{ nullptr, releaseAddress };
                auto callable = [function, count, &owned](typename detail::CAbiArgument<Args>::Value... values)
                    -> typename detail::CAbiResult<Result>::Value {
                    if constexpr (std::is_void_v<Result>) { function(detail::CAbiArgument<Args>::pass(values)...); (detail::checkCAbiCallback(values), ...); }
                    else
                    {
                        const auto returned = function(detail::CAbiArgument<Args>::pass(values)...);
                        if constexpr (std::is_pointer_v<NativeResult>)
                        {
                            owned.pointer = const_cast<void *>(reinterpret_cast<const void *>(returned));
                            (detail::checkCAbiCallback(values), ...);
                            return detail::CAbiResult<Result>::copy(returned, count);
                        }
                        else { (detail::checkCAbiCallback(values), ...); return detail::CAbiResult<Result>::copy(returned, count); }
                    }
                };
                return detail::invokeTyped<typename detail::CAbiResult<Result>::Value,
                    typename detail::CAbiArgument<Args>::Value...>(callable, context, raw,
                        std::index_sequence_for<Args...>{}); }, error);
    }
};

}  // namespace magel_glue

#endif  // PARSE_ENGINE_MAGEL_GLUE_HPP
