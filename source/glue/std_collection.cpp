#include "std_collection.hpp"

#include "stackvm.hpp"
#include "std_collection_storage.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

/**
 * @brief 标准集合的数组存储、键策略和原生操作；只使用公开宿主上下文。
 */
namespace collection_glue
{
using NativeValue = stackvm::NativeValue;
using ObjectValue = stackvm::NativeStoredValue;

/**
 * @brief 胶水内部的数值槽，保持浮点位模式与整数句柄分离。
 */
struct Slot
{
    bool isFloat = false;
    std::int64_t integer = 0;
    double real = 0;
    /**
     * @brief 保存整数、地址或对象句柄。
     * @param value 原始整数槽。
     * @return 对应的内部槽。
     */
    static Slot fromInt(std::int64_t value) { return { false, value, 0 }; }
    /**
     * @brief 保存浮点数并保留其数值种类。
     * @param value 双精度数值。
     * @return 浮点槽。
     */
    static Slot fromDouble(double value) { return { true, 0, value }; }
};

/**
 * @brief 仅供标准集合胶水内部区分记录布局，VM 不解释此枚举。
 */
enum class StorageKind
{
    Vector,
    Set,
    Map,
    IteratorLease
};
struct State;
struct Record;
/**
 * @brief 借用由标准集合胶水维护的静态描述符身份。
 * @param kind 胶水内部的存储类别。
 * @return 对应的不透明记录描述符。
 */
inline const stackvm::NativeRecordType &descriptor(StorageKind kind);
/**
 * @brief 将元素引用单元中的当前值同步到记录快照。
 * @param ctx 本次调用的公开运行时适配状态。
 * @param object 待同步的记录。
 * @return 所有活跃单元均同步成功时返回 true。
 */
inline bool nativeRefreshViews(State &ctx, Record &object);

/**
 * @brief 释放记录的拥有值，记录本体由 destroyRecord 销毁。
 * @param host 当前运行时上下文。
 * @param record 待清理的记录。
 */
inline void disposeRecord(stackvm::NativeCallContext &host, void *record);

/**
 * @brief 多个游标副本共享一次来源租约与稳定元素单元。
 */
struct Lease
{
    std::int64_t source = 0;
    Memory::Map<std::size_t, std::size_t> cells;
};

/**
 * @brief ArrayList、集合及映射独占的宿主数据布局。
 */
struct Record
{
    StorageKind kind = StorageKind::Vector;
    Memory::Vector<ObjectValue> items;
    Entries entries;
    /**
     * @brief 借用元素的稳定内存槽与嵌套借用次数。
     */
    struct ElementView
    {
        std::size_t address = 0;
        std::size_t users = 1;
        bool isText = false;
        bool isFloat = false;
        std::int64_t objectFlag = 0;
    };
    Memory::Map<std::size_t, ElementView> elementViews;
    bool refreshingViews = false;
    std::size_t iteratorLeases = 0;
    bool pendingRelease = false;
    std::shared_ptr<Lease> iteratorLease;
    std::int64_t mapEqual = 0, mapHash = -1;
    bool mapComparing = false;
};

/**
 * @brief 通过公开运行时接口读写与释放胶水分配的内存。
 */
struct Heap
{
    stackvm::NativeCallContext &host;
    /**
     * @brief 为稳定引用或临时聚合申请受运行时追踪的内存。
     * @param bytes 请求字节数。
     * @return 块首地址，失败返回零。
     */
    std::size_t allocate(std::size_t bytes) { return static_cast<std::size_t>(host.makeTemporary(bytes)); }
    /**
     * @brief 释放拥有的运行时内存，已释放地址可重复清理。
     * @param address 块首地址。
     * @return 释放或跳过成功时返回 true。
     */
    bool release(std::size_t address) { return host.releaseMemory(static_cast<std::int64_t>(address), true); }
    /**
     * @brief 在运行时边界检查后写入原始槽字节。
     * @param address 写入起点。
     * @param bytes 原始数据。
     * @param count 数据长度。
     * @return 完整写入时返回 true。
     */
    bool writeBytes(std::size_t address, const char *bytes, std::size_t count) { return host.writeBytes(static_cast<std::int64_t>(address), bytes, count); }
};

/**
 * @brief 为转换产生的临时值登记最终清理。
 */
struct Temporaries
{
    stackvm::NativeCallContext &host;
    bool object;
    /**
     * @brief 将临时资源加入运行时清理队列。
     * @param handle 对象句柄或内存地址。
     */
    void push_back(std::int64_t handle) { host.trackTemporary(handle, object); }
};

/**
 * @brief 通过通用记录协议维护集合值的递归所有权。
 */
struct Store
{
    stackvm::NativeCallContext &host;
    Temporaries materializedHeap{ host, false }, materializedObjects{ host, true };
    /**
     * @brief 将集合记录交给运行时通用壳管理。
     * @param record 待移交的独立记录。
     * @return 新句柄，失败返回零。
     */
    std::int64_t add(std::shared_ptr<Record> record)
    {
        auto *owned = new Record(std::move(*record));
        const auto handle = host.makeNativeRecord(descriptor(owned->kind), owned);
        if (!handle) delete owned;
        return handle;
    }
    /**
     * @brief 查找任意标准集合胶水记录。
     * @param handle 当前运行时的句柄。
     * @return 匹配的记录；其他对象或失效句柄返回空。
     */
    Record *find(std::int64_t handle)
    {
        for (auto kind : { StorageKind::Vector, StorageKind::Set, StorageKind::Map, StorageKind::IteratorLease })
            if (auto *record = host.nativeRecord(handle, descriptor(kind))) return static_cast<Record *>(record);
        return nullptr;
    }
    /**
     * @brief 复制任意拥有型句柄，具体类型由描述符处理。
     * @param handle 源对象句柄。
     * @param error 可选的本地诊断输出。
     * @return 独立副本句柄，失败返回零并保留运行时诊断。
     */
    std::int64_t copyHandle(std::int64_t handle, Memory::String *error)
    {
        (void)error;
        return host.cloneNativeRecord(handle);
    }
    /**
     * @brief 释放句柄所拥有的记录及嵌套资源。
     * @param handle 待释放对象。
     * @return 接受立即或延期释放时返回 true。
     */
    bool release(std::int64_t handle) { return host.releaseNativeRecord(handle); }
    /**
     * @brief 递归释放槽快照拥有的子对象。
     * @param value 拥有型值快照。
     */
    void releaseValue(const ObjectValue &value)
    {
        for (const auto &field : value.fields) releaseValue(field);
        if (value.ownsObject) release(value.integer);
    }
    /**
     * @brief 深复制槽快照及其嵌套拥有值。
     * @param value 接收独立副本的值。
     * @param error 可选的诊断输出。
     * @return 所有子值复制成功时返回 true。
     */
    bool copyValue(ObjectValue *value, Memory::String *error)
    {
        ObjectValue copy = *value;
        copy.fields.clear();
        copy.ownsObject = false;
        for (auto field : value->fields)
        {
            if (!copyValue(&field, error))
            {
                releaseValue(copy);
                return false;
            }
            copy.fields.push_back(std::move(field));
        }
        if (value->ownsObject)
        {
            copy.integer = copyHandle(value->integer, error);
            if (!copy.integer)
            {
                releaseValue(copy);
                return false;
            }
            copy.ownsObject = true;
        }
        *value = std::move(copy);
        return true;
    }
    /**
     * @brief 释放键快照独占的嵌套对象。
     * @param key 拥有型键快照。
     */
    void releaseKey(const ObjectKey &key)
    {
        for (const auto &field : key.fields) releaseKey(field);
        if (key.ownsObject) release(key.ownedInteger);
    }
    /**
     * @brief 将查询键变为独立拥有的稳定键。
     * @param key 待复制的键及其嵌套字段。
     * @param error 可选的诊断输出。
     * @return 所有对象复制成功时返回 true。
     */
    bool ownKey(ObjectKey *key, Memory::String *error)
    {
        for (auto &field : key->fields)
            if (!ownKey(&field, error)) return false;
        if (!key->isObject || key->ownsObject) return true;
        key->ownedInteger = copyHandle(key->ownedInteger, error);
        key->ownsObject = key->ownedInteger != 0;
        return key->ownsObject;
    }
};

/**
 * @brief 单次集合胶水调用的适配状态，不持有运行时内部地址。
 */
struct State
{
    stackvm::NativeCallContext &host;
    Memory::String error;
    Heap memory{ host };
    Store storage{ host };
    Heap *heap = &memory;
    Store *objects = &storage;
    using Invoker = Memory::Function<bool(std::int64_t, const Memory::Vector<NativeValue> &, NativeValue &)>;
    Invoker invoke;
    Invoker *invoker = &invoke;
    /**
     * @brief 将同步回调接到当前 VM 调用帧。
     * @param context 仅本次胶水调用有效的公开上下文。
     */
    explicit State(stackvm::NativeCallContext &context) : host(context), invoke([&context](std::int64_t function, const Memory::Vector<NativeValue> &arguments, NativeValue &result) { return context.invoke(function, arguments, result); }) {}
};

/**
 * @brief 读取整数参数，浮点输入按语言规则截断。
 * @param value 内部数值槽。
 * @return 整数参数。
 */
inline std::int64_t nativeInt(const Slot &value) { return value.isFloat ? static_cast<std::int64_t>(value.real) : value.integer; }
/**
 * @brief 判定整数是否属于运行时对象句柄区间。
 * @param handle 待查询整数。
 * @return 对象句柄编码范围内返回 true。
 */
inline bool isObjectHandle(std::int64_t handle) { return handle >= 4611686018427387904LL; }
/**
 * @brief 通过运行时检查读取原始内存。
 * @param ctx 本次胶水状态。
 * @param address 读取起点。
 * @param out 接收字节。
 * @param count 字节数。
 * @return 完整读取返回 true。
 */
inline bool nativeRead(const State &ctx, std::size_t address, char *out, std::size_t count) { return ctx.host.readBytes(static_cast<std::int64_t>(address), out, count); }
/**
 * @brief 通过运行时检查写入原始内存。
 * @param ctx 本次胶水状态。
 * @param address 写入起点。
 * @param bytes 原始字节。
 * @param count 字节数。
 * @return 完整写入返回 true。
 */
inline bool nativeWrite(State &ctx, std::size_t address, const char *bytes, std::size_t count) { return ctx.host.writeBytes(static_cast<std::int64_t>(address), bytes, count); }
/**
 * @brief 读取运行时文本副本。
 * @param ctx 本次胶水状态。
 * @param address 字符串地址。
 * @param out 接收文本。
 * @return 读取成功返回 true。
 */
inline bool nativeReadCString(const State &ctx, std::size_t address, Memory::String *out) { return ctx.host.readString(static_cast<std::int64_t>(address), *out); }
/**
 * @brief 创建交给语言调用者拥有的文本。
 * @param ctx 本次胶水状态。
 * @param text 文本内容。
 * @return 新文本地址。
 */
inline std::size_t nativeAllocString(State &ctx, const Memory::String &text) { return static_cast<std::size_t>(ctx.host.makeString(text)); }
/**
 * @brief 申请稳定元素引用或聚合取值所需的内存。
 * @param ctx 本次胶水状态。
 * @param bytes 申请字节数。
 * @return 内存地址；失败返回零。
 */
inline std::size_t nativeAllocate(State &ctx, std::size_t bytes) { return ctx.heap->allocate(bytes); }
/**
 * @brief 为标准集合创建空的宿主记录。
 * @param ctx 本次胶水状态。
 * @param kind 胶水内部存储类别。
 * @return 新对象句柄。
 */
inline Slot nativeNewObject(State &ctx, StorageKind kind)
{
    auto object = Memory::makeShared<Record>();
    object->kind = kind;
    return Slot::fromInt(ctx.objects->add(std::move(object)));
}
/**
 * @brief 校验记录身份、回调重入状态并同步借用元素。
 * @param ctx 本次胶水状态。
 * @param handle 对象句柄。
 * @param kind 预期存储类别。
 * @param who 诊断中的操作名。
 * @return 可访问记录，校验失败返回空。
 */
inline Record *nativeObject(State &ctx, const Slot &handle, StorageKind kind, const char *who)
{
    auto *object = static_cast<Record *>(ctx.host.nativeRecord(handle.integer, descriptor(kind)));
    if (!object)
    {
        ctx.error = Memory::String(who) + " 用的集合已经释放过了（句柄 " + Memory::toString(handle.integer) + "）";
        return nullptr;
    }
    if (object->mapComparing)
    {
        ctx.error = Memory::String("Equal / Hash 回调期间不能重入同一个 ") + (kind == StorageKind::Set ? "Set" : "Map");
        return nullptr;
    }
    return nativeRefreshViews(ctx, *object) ? object : nullptr;
}

/**
 * @brief 将拥有值快照物化为可返回给语言调用者的运行时槽。
 * @param ctx 当前运行时适配状态。
 * @param value 待物化的值快照。
 * @param out 接收数值槽或新分配的拥有型地址。
 * @return 物化成功返回 true。
 */
inline bool nativeSlot(State &ctx, const ObjectValue &value, Slot *out);
/**
 * @brief 向稳定元素单元写入独立拥有的值。
 * @param ctx 当前运行时适配状态。
 * @param view 目标单元的地址与叶子布局。
 * @param value 待复制的值快照。
 * @return 写入成功返回 true。
 */
inline bool nativeWriteView(State &ctx, const Record::ElementView &view, const ObjectValue &value);
/**
 * @brief 释放稳定元素单元拥有的资源叶子，保留单元本身。
 * @param ctx 当前运行时适配状态。
 * @param view 待清理的元素单元及其叶子布局。
 */
inline void nativeReleaseViewContents(State &ctx, const Record::ElementView &view);
/**
 * @brief 一条实参 → 容器里的值 / 键。
 * @param ctx 环境。
 * @param value 值（数值就是 Slot 本身；字符串是它的地址）。
 * @param isText true = 按字符串读（**读成副本**）。
 * @param copyObject true = 存入容器 / obj 时复制一份，false = 只用于查找比较。
 * @param out 结果。
 * @return 成功返回 true；字符串读不到时写 `ctx.error` 并返回 false。
 * @param objectFlag 对象标记；零为标量，一为拥有型对象，负一为函数值，其余为字段布局地址。
 */
inline bool nativeValue(State &ctx, const Slot &value, bool isText, std::int64_t objectFlag,
                        bool copyObject, ObjectValue *out)
{
    if (objectFlag > 1)
    {
        Memory::String descriptor;
        if (!nativeReadCString(ctx, static_cast<std::size_t>(objectFlag), &descriptor) || descriptor.empty() || descriptor[0] != '@')
        {
            ctx.error = "容器结构体布局描述无效";
            return false;
        }
        out->isStruct = true;
        for (std::size_t index = 1; index < descriptor.size(); ++index)
        {
            std::int64_t bits = 0;
            const std::size_t address = static_cast<std::size_t>(value.integer) + (index - 1u) * 8u;
            if (!nativeRead(ctx, address, reinterpret_cast<char *>(&bits), sizeof(bits)))
            {
                ctx.error = "容器结构体字段地址无效";
                if (ctx.objects != nullptr) ctx.objects->releaseValue(*out);
                out->fields.clear();
                return false;
            }
            ObjectValue field;
            const char kind = descriptor[index];
            if (kind != 'i' && kind != 's' && kind != 'o' && kind != 'f' && kind != 'c')
            {
                ctx.error = "容器结构体字段布局无效";
                if (ctx.objects != nullptr) ctx.objects->releaseValue(*out);
                out->fields.clear();
                return false;
            }
            Slot fieldValue = Slot::fromInt(bits);
            if (kind == 'f')
            {
                double real = 0.0;
                std::memcpy(&real, &bits, sizeof(real));
                fieldValue = Slot::fromDouble(real);
            }
            /* 未激活枚举载荷和移动后的拥有槽为零；保留空槽，不能把它实体化为空字符串。 */
            if (bits == 0 && (kind == 's' || kind == 'o' || kind == 'c'))
            {
                out->fields.push_back(std::move(field));
                continue;
            }
            if (!nativeValue(ctx, fieldValue, kind == 's', kind == 'c' ? -1 : kind == 'o' ? 1 :
                                                                                            0,
                             copyObject, &field))
            {
                if (ctx.objects != nullptr) ctx.objects->releaseValue(*out);
                out->fields.clear();
                return false;
            }
            out->fields.push_back(std::move(field));
        }
        return true;
    }
    out->isText = isText;
    out->ownsObject = false;
    if (!isText)
    {
        out->isFloat = value.isFloat;
        out->integer = value.integer;
        out->objectIdentity = 0;
        out->real = value.real;
        if (objectFlag == 1 || (objectFlag == -1 && isObjectHandle(value.integer)))
        {
            const auto identity = ctx.host.objectIdentity(value.integer);
            if (identity == 0)
            {
                ctx.error = "对象键/值已经释放或不是有效句柄：" + Memory::toString(value.integer);
                return false;
            }
            out->objectIdentity = identity;
            out->isFloat = false;
            out->real = 0.0;
            if (copyObject)
            {
                Memory::String error;
                const std::int64_t copy = ctx.objects->copyHandle(value.integer, &error);
                if (copy == 0)
                {
                    ctx.error = error;
                    return false;
                }
                out->integer = copy;
                out->ownsObject = true;
            }
        }
        return true;
    }
    if (!nativeReadCString(ctx, static_cast<std::size_t>(value.integer), &out->text))
    {
        ctx.error = "读容器元素里的字符串失败（地址 " + Memory::toString(value.integer) + "）";
        return false;
    }
    return true;
}

/**
 * @brief 容器里的值 / 键 → 一个 Slot（字符串会在堆上复制一份新的）。
 * @param ctx 环境。
 * @param value 值。
 * @param out 结果。
 * @return 成功返回 true。
 */
inline bool nativeSlot(State &ctx, const ObjectValue &value, Slot *out)
{
    if (value.isStruct)
    {
        if (ctx.heap == nullptr || ctx.objects == nullptr) return false;
        const std::size_t bytes = std::max<std::size_t>(8u, value.fields.size() * 8u);
        const std::size_t address = nativeAllocate(ctx, bytes);
        if (address == 0u)
        {
            ctx.error = "容器结构体取值时堆内存不足";
            return false;
        }
        ctx.objects->materializedHeap.push_back(address);
        for (std::size_t index = 0; index < value.fields.size(); ++index)
        {
            Slot field;
            if (!nativeSlot(ctx, value.fields[index], &field)) return false;
            if (value.fields[index].isText)
                ctx.objects->materializedHeap.push_back(static_cast<std::size_t>(field.integer));
            if (value.fields[index].ownsObject || value.fields[index].objectIdentity != 0)
                ctx.objects->materializedObjects.push_back(field.integer);
            const std::int64_t bits = field.isFloat ? [&]() {
                std::int64_t raw = 0;
                std::memcpy(&raw, &field.real, sizeof(raw));
                return raw;
            }() :
                                                      field.integer;
            if (!ctx.heap->writeBytes(address + index * 8u,
                                      reinterpret_cast<const char *>(&bits), sizeof(bits)))
            {
                ctx.error = "容器结构体取值时写入字段失败";
                return false;
            }
        }
        *out = Slot::fromInt(static_cast<std::int64_t>(address));
        return true;
    }
    if (!value.isText)
    {
        if (value.ownsObject || value.objectIdentity != 0)
        {
            Memory::String error;
            const std::int64_t copy = ctx.objects->copyHandle(value.integer, &error);
            if (copy == 0)
            {
                ctx.error = error;
                return false;
            }
            *out = Slot::fromInt(copy);
            return true;
        }
        *out = value.isFloat ? Slot::fromDouble(value.real) : Slot::fromInt(value.integer);
        return true;
    }
    const std::size_t address = nativeAllocString(ctx, value.text);
    if (address == 0u) return false;
    *out = Slot::fromInt(static_cast<std::int64_t>(address));
    return true;
}

/**
 * @brief 从活跃元素单元读取当前值；聚合单元直接保存摊平后的字段。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param view 保存元素地址、引用计数和叶子布局的稳定单元。
 * @param out 接收转换后的值或查询结果。
 * @return 操作成功时返回 true，失败返回 false 并保留运行时诊断。
 */
inline bool nativeViewValue(State &ctx, const Record::ElementView &view, ObjectValue *out)
{
    if (view.objectFlag > 1)
        return nativeValue(ctx, Slot::fromInt(static_cast<std::int64_t>(view.address)), false,
                           view.objectFlag, true, out);
    std::int64_t bits = 0;
    if (!nativeRead(ctx, view.address, reinterpret_cast<char *>(&bits), sizeof(bits)))
    {
        ctx.error = "列表迭代引用读取失败";
        return false;
    }
    Slot slot = Slot::fromInt(bits);
    if (view.isFloat)
    {
        double real = 0;
        std::memcpy(&real, &bits, sizeof(real));
        slot = Slot::fromDouble(real);
    }
    return nativeValue(ctx, slot, view.isText, view.objectFlag, true, out);
}
/**
 * @brief 将活跃引用中的当前值同步到列表快照，所有列表读取与值复制共用该入口。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param object 已校验的集合宿主记录。
 * @return 操作成功时返回 true，失败返回 false 并保留运行时诊断。
 */
inline bool nativeRefreshViews(State &ctx, Record &object)
{
    if (object.refreshingViews || object.elementViews.empty()) return true;
    object.refreshingViews = true;
    for (const auto &entry : object.elementViews)
    {
        if (entry.first >= object.items.size())
        {
            ctx.error = "迭代期间列表结构已改变";
            object.refreshingViews = false;
            return false;
        }
        ObjectValue value;
        if (!nativeViewValue(ctx, entry.second, &value))
        {
            object.refreshingViews = false;
            return false;
        }
        ctx.objects->releaseValue(object.items[entry.first]);
        object.items[entry.first] = std::move(value);
    }
    object.refreshingViews = false;
    return true;
}
/**
 * @brief 释放元素单元中当前拥有的字符串与对象叶子，不释放单元本身。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param view 保存元素地址、引用计数和叶子布局的稳定单元。
 */
inline void nativeReleaseViewContents(State &ctx, const Record::ElementView &view)
{
    Memory::String descriptor;
    if (view.objectFlag > 1)
    {
        if (!nativeReadCString(ctx, static_cast<std::size_t>(view.objectFlag), &descriptor)) return;
    }
    else
        descriptor = Memory::String("@") + (view.isText ? 's' : view.objectFlag == -1 ? 'c' :
                                                            view.objectFlag == 1      ? 'o' :
                                                                                        'i');
    for (std::size_t i = 1; i < descriptor.size(); ++i)
    {
        std::int64_t bits = 0;
        if (!nativeRead(ctx, view.address + (i - 1u) * 8u, reinterpret_cast<char *>(&bits), sizeof(bits))) continue;
        if (descriptor[i] == 's' && bits != 0 && ctx.heap != nullptr) ctx.heap->release(static_cast<std::size_t>(bits));
        if ((descriptor[i] == 'o' || descriptor[i] == 'c') && isObjectHandle(bits)) ctx.objects->release(bits);
    }
}
/**
 * @brief 将容器快照复制到引用单元；字符串与对象叶子由单元独占。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param view 保存元素地址、引用计数和叶子布局的稳定单元。
 * @param value 待读取、保存或转换的值快照。
 * @return 操作成功时返回 true，失败返回 false 并保留运行时诊断。
 */
inline bool nativeWriteView(State &ctx, const Record::ElementView &view, const ObjectValue &value)
{
    const std::size_t count = value.isStruct ? value.fields.size() : 1u;
    for (std::size_t i = 0; i < count; ++i)
    {
        Slot slot;
        if (!nativeSlot(ctx, value.isStruct ? value.fields[i] : value, &slot)) return false;
        std::int64_t bits = slot.integer;
        if (slot.isFloat) std::memcpy(&bits, &slot.real, sizeof(bits));
        if (!nativeWrite(ctx, view.address + i * 8u, reinterpret_cast<const char *>(&bits), sizeof(bits)))
        {
            ctx.error = "列表迭代引用写入失败";
            return false;
        }
    }
    return true;
}

/**
 * @brief 按下标取第 `index` 个（越界报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param object 待访问的对象。
 * @param index 待访问的元素索引。
 * @param out 接收处理结果的输出对象。
 * @param who 诊断信息中使用的操作名称。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
inline bool vecIndex(State &ctx, Record *object, std::int64_t index, std::size_t *out,
                     const char *who)
{
    if (index < 0 || static_cast<std::size_t>(index) >= object->items.size())
    {
        ctx.error = Memory::String(who) + " 下标越界：" + Memory::toString(index) + "（长度 " + Memory::toString(object->items.size()) + "）";
        return false;
    }
    *out = static_cast<std::size_t>(index);
    return true;
}

/**
 * @brief 创建空的运行时列表。
 *
 * `__std_vec_new() -> vec`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 * @param args 按此原生操作的绑定约定排列的实参槽；无参操作为空。
 */
inline Slot vecNew(State &ctx, const Memory::Vector<Slot> &args)
{
    (void)args;
    return nativeNewObject(ctx, StorageKind::Vector);
}

/**
 * @brief 把宿主入口参数复制成 `std::lang::List<std::lang::String>`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 * @param args 按此原生操作的绑定约定排列的实参槽；无参操作为空。
 */
inline Slot programArguments(State &ctx, const Memory::Vector<Slot> &args)
{
    (void)args;
    const Slot handle = nativeNewObject(ctx, StorageKind::Vector);
    Record *object = ctx.objects == nullptr ? nullptr : ctx.objects->find(handle.integer);
    if (object == nullptr) return Slot::fromInt(0);
    const auto arguments = ctx.host.programArguments();
    object->items.reserve(arguments.size());
    for (const Memory::String &argument : arguments)
    {
        ObjectValue value;
        value.isText = true;
        value.text = argument;
        object->items.push_back(std::move(value));
    }
    return handle;
}

/**
 * @brief 复制文本的原始 UTF-8 字节，避免字符边界索引影响二进制算法。
 * @param ctx 上下文。
 * @param args 文本地址。
 * @return 拥有型字节列表。
 */
inline Slot stringBytes(State &ctx, const Memory::Vector<Slot> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text)) return Slot::fromInt(0);
    const auto result = nativeNewObject(ctx, StorageKind::Vector);
    auto *vector = ctx.objects->find(nativeInt(result));
    if (!vector) return Slot::fromInt(0);
    vector->items.reserve(text.size());
    for (unsigned char byte : text)
    {
        ObjectValue value;
        value.integer = byte;
        vector->items.push_back(std::move(value));
    }
    return result;
}

/**
 * @brief `__std_vec_pack(isText, isObject, values...) -> vec`：CHALI 变长参数的调用点打包。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecPack(State &ctx, const Memory::Vector<Slot> &args)
{
    if (args.size() < 2u)
    {
        ctx.error = "变长参数打包至少需要两个类型标志";
        return Slot::fromInt(0);
    }
    auto object = Memory::makeShared<Record>();
    object->kind = StorageKind::Vector;
    const bool isText = nativeInt(args[0]) != 0;
    const std::int64_t isObject = nativeInt(args[1]);
    for (std::size_t index = 2u; index < args.size(); ++index)
    {
        ObjectValue value;
        if (!nativeValue(ctx, args[index], isText, isObject, true, &value))
        {
            for (const ObjectValue &stored : object->items) ctx.objects->releaseValue(stored);
            return Slot::fromInt(0);
        }
        object->items.push_back(std::move(value));
    }
    return Slot::fromInt(ctx.objects->add(std::move(object)));
}

/**
 * @brief `__std_vec_push(vec, 值, 值是不是字符串, 值是不是对象句柄) -> 0`：追加到末尾。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecPush(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "vec.push");
    if (object == nullptr) return Slot::fromInt(0);
    /* 与 vec.insert / vec.remove 同一口径：push 可能让底层存储扩容，已有引用会失效；
     * 而按长度每轮重取的 for-in 会因此无限增长，直到耗尽运行内存。 */
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), true,
                     &value)) return Slot::fromInt(0);
    object->items.push_back(std::move(value));
    return Slot::fromInt(0);
}

/**
 * @brief `__std_vec_insert(vec, 下标, 值, 是不是字符串, 是不是对象句柄) -> 0`：插到下标处（允许 == 长度）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecInsert(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "vec.insert");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    const std::int64_t index = nativeInt(args[1]);
    if (index < 0 || static_cast<std::size_t>(index) > object->items.size())
    {
        ctx.error = "vec.insert 的位置越界：" + Memory::toString(index) + "（长度 " + Memory::toString(object->items.size()) + "，只能落在 0..长度 之间）";
        return Slot::fromInt(0);
    }
    ObjectValue value;
    if (!nativeValue(ctx, args[2], nativeInt(args[3]) != 0, nativeInt(args[4]), true,
                     &value)) return Slot::fromInt(0);
    object->items.insert(object->items.begin() + index, std::move(value));
    return Slot::fromInt(0);
}

/**
 * @brief 借用列表元素的稳定地址，嵌套迭代同一元素共享单元并计数。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param args 按此原生操作的绑定约定排列的实参槽；无参操作为空。
 * @return 原生返回槽；失败时返回零并保留运行时诊断。
 */
inline Slot vecRef(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "foreach");
    if (object == nullptr) return Slot::fromInt(0);
    std::size_t index = 0;
    if (!vecIndex(ctx, object, nativeInt(args[1]), &index, "foreach")) return Slot::fromInt(0);
    const auto found = object->elementViews.find(index);
    if (found != object->elementViews.end())
    {
        ++found->second.users;
        return Slot::fromInt(found->second.address);
    }
    Record::ElementView view;
    view.isText = nativeInt(args[2]) != 0;
    view.objectFlag = nativeInt(args[3]);
    view.isFloat = nativeInt(args[4]) != 0;
    view.address = nativeAllocate(ctx, std::max<std::size_t>(8u,
                                                             object->items[index].isStruct ? object->items[index].fields.size() * 8u : 8u));
    if (view.address == 0u)
    {
        ctx.error = "列表迭代引用内存不足";
        return Slot::fromInt(0);
    }
    if (!nativeWriteView(ctx, view, object->items[index]))
    {
        nativeReleaseViewContents(ctx, view);
        ctx.heap->release(view.address);
        return Slot::fromInt(0);
    }
    object->elementViews[index] = view;
    return Slot::fromInt(static_cast<std::int64_t>(view.address));
}
/**
 * @brief 结束列表元素借用，写回当前值，最后一个借用者释放单元及资源叶子。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param args 按此原生操作的绑定约定排列的实参槽；无参操作为空。
 * @return 原生返回槽；失败时返回零并保留运行时诊断。
 */
inline Slot vecUnref(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "foreach 结束");
    if (object == nullptr) return Slot::fromInt(0);
    for (auto it = object->elementViews.begin(); it != object->elementViews.end(); ++it)
    {
        if (it->second.address != static_cast<std::size_t>(nativeInt(args[1]))) continue;
        if (--it->second.users == 0u)
        {
            nativeReleaseViewContents(ctx, it->second);
            ctx.heap->release(it->second.address);
            object->elementViews.erase(it);
        }
        if (object->pendingRelease && !object->iteratorLeases && object->elementViews.empty())
            ctx.objects->release(nativeInt(args[0]));
        return Slot::fromInt(0);
    }
    ctx.error = "列表迭代引用已释放";
    return Slot::fromInt(0);
}

/**
 * @brief 原子转移容器句柄并写回同策略空容器，禁止移动活跃借用来源。
 * @param ctx 运行上下文。
 * @param args 来源容器句柄槽的地址。
 * @return 移出的容器句柄。失败时记录运行错误。
 */
inline Slot iterTake(State &ctx, const Memory::Vector<Slot> &args)
{
    std::int64_t handle = 0;
    const auto address = static_cast<std::size_t>(nativeInt(args[0]));
    if (!nativeRead(ctx, address, reinterpret_cast<char *>(&handle), sizeof(handle)))
    {
        ctx.error = "intoIter 来源引用无效";
        return Slot::fromInt(0);
    }
    auto *source = ctx.objects->find(handle);
    if (!source || (source->kind != StorageKind::Vector && source->kind != StorageKind::Set && source->kind != StorageKind::Map))
    {
        ctx.error = "intoIter 来源不是容器";
        return Slot::fromInt(0);
    }
    if (source->iteratorLeases || !source->elementViews.empty() || source->mapComparing)
    {
        ctx.error = "活跃引用或比较回调期间不能 intoIter 转移容器";
        return Slot::fromInt(0);
    }
    auto empty = Memory::makeShared<Record>();
    empty->kind = source->kind;
    empty->mapEqual = source->mapEqual;
    empty->mapHash = source->mapHash;
    const auto replacement = ctx.objects->add(std::move(empty));
    if (!nativeWrite(ctx, address, reinterpret_cast<const char *>(&replacement), sizeof(replacement)))
    {
        ctx.objects->release(replacement);
        ctx.error = "intoIter 来源引用不可写";
        return Slot::fromInt(0);
    }
    return Slot::fromInt(handle);
}

/**
 * @brief 创建列表引用迭代租约，副本共享单元但游标位置彼此独立。
 * @param ctx 运行上下文。
 * @param args 源列表句柄。
 * @return 新租约句柄。失败时记录运行错误。
 */
inline Slot iterLeaseNew(State &ctx, const Memory::Vector<Slot> &args)
{
    auto *source = nativeObject(ctx, args[0], StorageKind::Vector, "iter");
    if (!source) return Slot::fromInt(0);
    ++source->iteratorLeases;
    auto object = Memory::makeShared<Record>();
    object->kind = StorageKind::IteratorLease;
    object->iteratorLease = Memory::makeShared<Lease>();
    object->iteratorLease->source = nativeInt(args[0]);
    return Slot::fromInt(ctx.objects->add(std::move(object)));
}

/**
 * @brief 返回借用来源当前长度。
 * @param ctx 运行上下文。
 * @param args 租约句柄。
 * @return 来源元素数量。失败时记录运行错误。
 */
inline Slot iterLeaseLen(State &ctx, const Memory::Vector<Slot> &args)
{
    auto *lease = nativeObject(ctx, args[0], StorageKind::IteratorLease, "iter sizeHint");
    if (!lease) return Slot::fromInt(0);
    auto *source = nativeObject(ctx, Slot::fromInt(lease->iteratorLease->source), StorageKind::Vector, "iter 来源");
    return Slot::fromInt(source ? static_cast<std::int64_t>(source->items.size()) : 0);
}

/**
 * @brief 返回租约持有的稳定元素地址，写入会通过列表的共享单元同步到来源。
 * @param ctx 运行上下文。
 * @param args 租约、索引、字符串标志、叶子布局与浮点标志。
 * @return 稳定元素地址。失败时记录运行错误。
 */
inline Slot iterLeaseRef(State &ctx, const Memory::Vector<Slot> &args)
{
    auto *lease = nativeObject(ctx, args[0], StorageKind::IteratorLease, "iter next");
    if (!lease) return Slot::fromInt(0);
    auto &state = *lease->iteratorLease;
    const auto index = static_cast<std::size_t>(nativeInt(args[1]));
    const auto found = state.cells.find(index);
    if (found != state.cells.end()) return Slot::fromInt(static_cast<std::int64_t>(found->second));
    const auto result = vecRef(ctx, { Slot::fromInt(state.source), args[1], args[2], args[3], args[4] });
    if (ctx.error.empty()) state.cells[index] = static_cast<std::size_t>(result.integer);
    return result;
}

/**
 * @brief `__std_vec_at(vec, 下标, 是不是字符串, 是不是对象句柄) -> 值`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecAt(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "vec 取元素");
    if (object == nullptr) return Slot::fromInt(0);
    std::size_t index = 0u;
    if (!vecIndex(ctx, object, nativeInt(args[1]), &index, "vec")) return Slot::fromInt(0);
    Slot out;
    if (!nativeSlot(ctx, object->items[index], &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief `__std_vec_set(vec, 下标, 值, 是不是字符串, 是不是对象句柄) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecSet(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "vec 改元素");
    if (object == nullptr) return Slot::fromInt(0);
    /* 迭代租约期间改写元素会让正在遍历的游标看到不一致的内容；
     * 元素视图（下标引用）由下面的分支按引用写入，不在这一条限制内。 */
    if (object->iteratorLeases)
    {
        ctx.error = "活跃 for-in 引用期间不能改写列表元素";
        return Slot::fromInt(0);
    }
    std::size_t index = 0u;
    if (!vecIndex(ctx, object, nativeInt(args[1]), &index, "vec")) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[2], nativeInt(args[3]) != 0, nativeInt(args[4]), true,
                     &value)) return Slot::fromInt(0);
    const auto view = object->elementViews.find(index);
    if (view != object->elementViews.end())
    {
        nativeReleaseViewContents(ctx, view->second);
        if (!nativeWriteView(ctx, view->second, value))
        {
            ctx.objects->releaseValue(value);
            return Slot::fromInt(0);
        }
    }
    ctx.objects->releaseValue(object->items[index]);
    object->items[index] = std::move(value);
    return Slot::fromInt(0);
}

/**
 * @brief `__std_vec_remove(vec, 下标) -> 0`：删掉下标处那个。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecRemove(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "vec.remove");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    std::size_t index = 0u;
    if (!vecIndex(ctx, object, nativeInt(args[1]), &index, "vec.remove")) return Slot::fromInt(0);
    ctx.objects->releaseValue(object->items[index]);
    object->items.erase(object->items.begin() + static_cast<std::ptrdiff_t>(index));
    return Slot::fromInt(0);
}

/**
 * @brief `__std_vec_pop(vec, 是不是字符串, 是不是对象句柄) -> 值`：弹出末尾（空表报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecPop(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "vec.pop");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    if (object->items.empty())
    {
        ctx.error = "vec.pop：表是空的";
        return Slot::fromInt(0);
    }
    Slot out;
    if (!nativeSlot(ctx, object->items.back(), &out)) return Slot::fromInt(0);
    ctx.objects->releaseValue(object->items.back());
    object->items.pop_back();
    return out;
}

/**
 * @brief `__std_vec_len(vec) -> 元素个数`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecLen(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "len(vec)");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(object->items.size()));
}

/**
 * @brief 释放运行时列表的全部元素。
 *
 * `__std_vec_clear(vec) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecClear(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "vec.clear");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    for (const ObjectValue &value : object->items) ctx.objects->releaseValue(value);
    object->items.clear();
    return Slot::fromInt(0);
}

/**
 * @brief List 的稳定迭代归并排序：O(n log n) 时间、O(n) 辅助空间，不使用递归栈。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecStableSort(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::sort");
    if (object == nullptr) return Slot::fromInt(0);
    if (object->iteratorLeases || !object->elementViews.empty())
    {
        ctx.error = "活跃 for-in 引用期间不能移动或删除列表元素";
        return Slot::fromInt(0);
    }
    const std::size_t count = object->items.size();
    if (count < 2u) return Slot::fromInt(0);

#ifdef SIMPLE_SORT
    /* 希尔排序：就地、不额外分配，间距逐步缩到 1；代价是**不稳定**。
     * 定义 `SIMPLE_SORT` 时替换掉默认的自底向上归并，见 README 的构建开关一节。 */
    for (std::size_t gap = count / 2u; gap > 0u; gap /= 2u)
    {
        for (std::size_t index = gap; index < count; ++index)
        {
            ObjectValue held = std::move(object->items[index]);
            std::size_t slot = index;
            while (slot >= gap && valueKey(held) < valueKey(object->items[slot - gap]))
            {
                object->items[slot] = std::move(object->items[slot - gap]);
                slot -= gap;
            }
            object->items[slot] = std::move(held);
        }
    }
    return Slot::fromInt(0);
#else
    Memory::Vector<ObjectValue> source = std::move(object->items);
    Memory::Vector<ObjectValue> target(count);
    for (std::size_t width = 1u; width < count;)
    {
        for (std::size_t left = 0u; left < count; left += width * 2u)
        {
            const std::size_t middle = std::min(left + width, count);
            const std::size_t end = std::min(left + width * 2u, count);
            std::size_t first = left;
            std::size_t second = middle;
            std::size_t output = left;
            while (first < middle && second < end)
            {
                /* 相等时取左段，稳定性由这一处分支保证。 */
                if (valueKey(source[second]) < valueKey(source[first]))
                    target[output++] = std::move(source[second++]);
                else
                    target[output++] = std::move(source[first++]);
            }
            while (first < middle) target[output++] = std::move(source[first++]);
            while (second < end) target[output++] = std::move(source[second++]);
        }
        source.swap(target);
        if (width > count / 2u) break;
        width *= 2u;
    }
    object->items = std::move(source);
    return Slot::fromInt(0);
#endif
}

/**
 * @brief `std::lang::algorithms::sortValue` 的值语义入口：复制 List，再对副本做稳定排序。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecSortedCopy(State &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.objects == nullptr)
    {
        ctx.error = "std::lang::algorithms::sort 缺少对象表";
        return Slot::fromInt(0);
    }
    Memory::String error;
    const std::int64_t copy = ctx.objects->copyHandle(nativeInt(args[0]), &error);
    if (copy == 0)
    {
        ctx.error = error;
        return Slot::fromInt(0);
    }
    vecStableSort(ctx, { Slot::fromInt(copy) });
    if (!ctx.error.empty())
    {
        ctx.objects->release(copy);
        return Slot::fromInt(0);
    }
    return Slot::fromInt(copy);
}

/**
 * @brief `__std_vec_find(vec, 值, 是不是字符串, 是不是对象句柄) -> 下标 / -1`（线性找第一个相等的）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecFind(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector, "vec 查找");
    if (object == nullptr) return Slot::fromInt(-1);
    ObjectValue wanted;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &wanted)) return Slot::fromInt(-1);
    const ObjectKey key = valueKey(wanted);
    for (std::size_t index = 0; index < object->items.size(); ++index)
    {
        if (valueKey(object->items[index]) < key) continue;
        if (key < valueKey(object->items[index])) continue;
        return Slot::fromInt(static_cast<std::int64_t>(index));
    }
    return Slot::fromInt(-1);
}

/**
 * @brief 将列表项转换为算法回调可接收的值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param value 待转换、保存或参与计算的值。
 * @param out 接收处理结果的输出对象。
 * @param temporaryString 接收回调使用的临时字符串。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
inline bool vecCallbackValue(State &ctx, const ObjectValue &value, NativeValue *out,
                             std::size_t *temporaryString)
{
    *temporaryString = 0u;
    if (value.isStruct)
    {
        Slot materialized;
        if (!nativeSlot(ctx, value, &materialized)) return false;
        *out = NativeValue::fromInt(materialized.integer);
    }
    else if (value.isText)
    {
        *temporaryString = nativeAllocString(ctx, value.text);
        if (*temporaryString == 0u) return false;
        *out = NativeValue::fromInt(static_cast<std::int64_t>(*temporaryString));
    }
    else if (value.isFloat)
        *out = NativeValue::fromFloat(value.real);
    else
        *out = NativeValue::fromInt(value.integer);
    return true;
}

/**
 * @brief 迭代租约守卫：进入时给容器加一份租约，离开时无论成败都释放。
 *
 * 高阶算法（fold / map / filter / all / any）持引用遍历元素并**重入 VM 调用回调**，
 * 回调里 push / clear / remove 会让引用与游标失效；持有租约后这些操作会撞上各自的
 * for-in 守卫并给出明确错误，而不是读到已经失效的内存。
 */
class IteratorLeaseScope
{
public:
    /**
     * @brief 给容器加一份迭代租约。
     * @param object 待保护的容器；为空时不做任何事。
     */
    explicit IteratorLeaseScope(Record *object) : object_(object)
    {
        if (object_ != nullptr) ++object_->iteratorLeases;
    }

    /**
     * @brief 释放迭代租约。
     */
    ~IteratorLeaseScope()
    {
        if (object_ != nullptr && object_->iteratorLeases > 0u) --object_->iteratorLeases;
    }

    IteratorLeaseScope(const IteratorLeaseScope &) = delete;
    IteratorLeaseScope &operator=(const IteratorLeaseScope &) = delete;

private:
    Record *object_;  ///< 被保护的容器。
};

/**
 * @brief 调用谓词回调并取得布尔结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @param all 是否要求全部列表项满足谓词。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecPredicate(State &ctx, const Memory::Vector<Slot> &args, bool all)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  all ? "std::lang::algorithms::all" : "std::lang::algorithms::any");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    IteratorLeaseScope lease(object);
    for (const ObjectValue &item : object->items)
    {
        NativeValue argument;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &argument, &temporary)) return Slot::fromInt(0);
        NativeValue result;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { argument }, result);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return Slot::fromInt(0);
        const bool accepted = result.asInt() != 0;
        if (all && !accepted) return Slot::fromInt(0);
        if (!all && accepted) return Slot::fromInt(1);
    }
    return Slot::fromInt(all ? 1 : 0);
}

/**
 * @brief 判断列表中的每一项是否都满足谓词。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecAll(State &ctx, const Memory::Vector<Slot> &args)
{
    return vecPredicate(ctx, args, true);
}

/**
 * @brief 判断列表中是否至少有一项满足谓词。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecAny(State &ctx, const Memory::Vector<Slot> &args)
{
    return vecPredicate(ctx, args, false);
}

/**
 * @brief 按顺序调用归约回调合并列表项。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecFold(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::fold");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    NativeValue accumulated = args[1].isFloat ? NativeValue::fromFloat(args[1].real) : NativeValue::fromInt(args[1].integer);
    IteratorLeaseScope lease(object);
    for (const ObjectValue &item : object->items)
    {
        NativeValue value;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &value, &temporary)) return Slot::fromInt(0);
        NativeValue next;
        const bool invoked = (*ctx.invoker)(nativeInt(args[2]), { accumulated, value }, next);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return Slot::fromInt(0);
        accumulated = std::move(next);
    }
    return accumulated.isFloat() ? Slot::fromDouble(accumulated.floating) : Slot::fromInt(accumulated.integer);
}

/**
 * @brief 算法回调的返回值 → 容器里的值。
 *
 * `kind`：0 = 整数（`char` / `bool` 也在这一档），1 = 浮点（f64），2 = 字符串，
 * 3 = 对象句柄。回调只能传整数与浮点（`invoker` 的约定），所以字符串是**地址**、
 * 对象是**句柄**，都装在 `integer` 里。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param produced 语言回调返回的原生值。
 * @param kind 回调结果的存储类别标记。
 * @param layout 聚合字段布局的运行时文本地址。
 * @param out 接收转换后的值或查询结果。
 * @return 操作成功时返回 true，失败返回 false 并保留运行时诊断。
 */
inline bool nativeCallbackResult(State &ctx, const NativeValue &produced, std::int64_t kind,
                                 std::int64_t layout,
                                 ObjectValue *out)
{
    out->isText = false;
    out->isFloat = false;
    out->ownsObject = false;
    out->objectIdentity = 0;
    out->integer = 0;
    out->real = 0.0;
    if (kind == 2)
    {
        out->isText = true;
        const std::size_t address = static_cast<std::size_t>(produced.asInt());
        const bool read = nativeReadCString(ctx, address, &out->text);
        /* 回调返回的堆串所有权归调用方（CHALI 的 `return` 是 move）：内容已经复制进
         * `out->text`，这块堆串就地还掉，否则每个字符串元素漏一块。 */
        if (ctx.heap != nullptr) ctx.heap->release(address);
        if (!read)
        {
            ctx.error = "算法回调返回的字符串读不到（地址 " + Memory::toString(produced.asInt()) + "）";
            return false;
        }
        return true;
    }
    if (kind == 3)
        return nativeValue(ctx, Slot::fromInt(produced.asInt()), false, true, true, out);
    if (kind == 4)
        return nativeValue(ctx, Slot::fromInt(produced.asInt()), false, layout, true, out);
    if (kind == 1)
    {
        out->isFloat = true;
        out->real = produced.floating;
        return true;
    }
    out->integer = produced.asInt();
    return true;
}

/**
 * @brief 容器里的值复制一份：`filter` 留下的元素要和原表**各自**持有。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param source 提供输入数据的来源。
 * @param isText 是否将该值作为文本处理。
 * @param isObject 是否将该值作为对象句柄处理。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
inline bool nativeCopyValue(State &ctx, const ObjectValue &source, bool isText, bool isObject,
                            ObjectValue *out)
{
    if (source.isStruct)
    {
        *out = source;
        Memory::String error;
        if (!ctx.objects->copyValue(out, &error))
        {
            ctx.error = error;
            return false;
        }
        return true;
    }
    out->isText = isText;
    out->ownsObject = false;
    out->isFloat = false;
    out->objectIdentity = 0;
    out->integer = 0;
    out->real = 0.0;
    if (isText)
    {
        out->text = source.text; /* 字符串在 ObjectValue 里是值语义 */
        return true;
    }
    out->isFloat = source.isFloat;
    out->real = source.real;
    out->integer = source.integer;
    if (!isObject) return true;
    Memory::String error;
    const std::int64_t copy = ctx.objects->copyHandle(source.integer, &error);
    if (copy == 0)
    {
        ctx.error = error;
        return false;
    }
    out->integer = copy;
    out->objectIdentity = ctx.host.objectIdentity(copy);
    out->ownsObject = true;
    return true;
}

/**
 * @brief 算法内建中途失败的统一收尾：释放已经攒下的元素，返回 0。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param items 待保存的元素集合。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot algorithmAbort(State &ctx, Memory::Vector<ObjectValue> &items)
{
    for (const ObjectValue &value : items) ctx.objects->releaseValue(value);
    if (ctx.error.empty()) ctx.error = "算法内建执行失败";
    return Slot::fromInt(0);
}

/**
 * @brief `__std_vec_map(vec, 回调, 结果种类) -> 新 vec`：逐元素换成回调的返回值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecMap(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::map");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    const std::int64_t kind = nativeInt(args[2]);
    Memory::Vector<ObjectValue> items;
    items.reserve(object->items.size());
    IteratorLeaseScope lease(object);
    for (const ObjectValue &item : object->items)
    {
        NativeValue argument;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &argument, &temporary))
            return algorithmAbort(ctx, items);
        NativeValue produced;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { argument }, produced);
        /* 字符串实参的临时块同时也是回调形参的存储：回调 `return 形参` 时结果
         * 就是它（CHALI 的 return 是 move），这时不能先还，交给 `nativeCallbackResult`
         * 统一释放；结果不是它才在这里还。 */
        if (temporary != 0u && (kind != 2 || produced.asInt() != static_cast<std::int64_t>(temporary)))
            ctx.heap->release(temporary);
        if (!invoked) return algorithmAbort(ctx, items);
        ObjectValue value;
        if (!nativeCallbackResult(ctx, produced, kind, nativeInt(args[3]), &value))
            return algorithmAbort(ctx, items);
        items.push_back(std::move(value));
    }
    auto result = Memory::makeShared<Record>();
    result->kind = StorageKind::Vector;
    result->items = std::move(items);
    return Slot::fromInt(ctx.objects->add(std::move(result)));
}

/**
 * @brief `__std_vec_filter(vec, 回调, 是不是字符串, 是不是对象) -> 新 vec`：留下回调为真的元素。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecFilter(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::filter");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    const bool isText = nativeInt(args[2]) != 0;
    const bool isObject = nativeInt(args[3]) != 0;
    Memory::Vector<ObjectValue> items;
    IteratorLeaseScope lease(object);
    for (const ObjectValue &item : object->items)
    {
        NativeValue argument;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &argument, &temporary))
            return algorithmAbort(ctx, items);
        NativeValue produced;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { argument }, produced);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return algorithmAbort(ctx, items);
        if (produced.asInt() == 0) continue;
        ObjectValue value;
        if (!nativeCopyValue(ctx, item, isText, isObject, &value))
            return algorithmAbort(ctx, items);
        items.push_back(std::move(value));
    }
    auto result = Memory::makeShared<Record>();
    result->kind = StorageKind::Vector;
    result->items = std::move(items);
    return Slot::fromInt(ctx.objects->add(std::move(result)));
}

/**
 * @brief `__std_vec_reduce(vec, 回调) -> 值`：拿**第一个元素**当初值往后折叠；空表报错。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecReduce(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::reduce");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    if (object->items.empty())
    {
        ctx.error = "std::lang::algorithms::reduce 不能作用在空表上（要给初值就用 fold）";
        return Slot::fromInt(0);
    }
    NativeValue accumulated;
    std::size_t seedTemporary = 0u;
    if (!vecCallbackValue(ctx, object->items[0], &accumulated, &seedTemporary))
        return Slot::fromInt(0);
    for (std::size_t index = 1u; index < object->items.size(); ++index)
    {
        NativeValue value;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, object->items[index], &value, &temporary))
            return Slot::fromInt(0);
        NativeValue next;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { accumulated, value }, next);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return Slot::fromInt(0);
        accumulated = std::move(next);
    }
    /* 初值那份临时串和回调返回的那份都**不在这里还**：可能是数据区字面量，
     * 也可能是只有一个元素时直接当结果用的那份（跟 `vecFold` 一个口径）。 */
    return accumulated.isFloat() ? Slot::fromDouble(accumulated.floating) : Slot::fromInt(accumulated.integer);
}

/**
 * @brief `__std_vec_sum(vec, 是不是浮点) -> 值`：数值求和（空表给 0 / 0.0）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecSum(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::sum");
    if (object == nullptr) return Slot::fromInt(0);
    if (nativeInt(args[1]) != 0)
    {
        double total = 0.0;
        for (const ObjectValue &item : object->items)
            total += item.isFloat ? item.real : static_cast<double>(item.integer);
        return Slot::fromDouble(total);
    }
    std::int64_t total = 0;
    for (const ObjectValue &item : object->items)
        total += item.isFloat ? static_cast<std::int64_t>(item.real) : item.integer;
    return Slot::fromInt(total);
}

/**
 * @brief `__std_vec_count_if(vec, 回调) -> 整数`：回调为真的元素个数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecCountIf(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::count");
    if (object == nullptr) return Slot::fromInt(0);
    if (ctx.invoker == nullptr)
    {
        ctx.error = "当前 VM 上下文不能从算法胶水调用函数指针";
        return Slot::fromInt(0);
    }
    std::int64_t total = 0;
    for (const ObjectValue &item : object->items)
    {
        NativeValue argument;
        std::size_t temporary = 0u;
        if (!vecCallbackValue(ctx, item, &argument, &temporary)) return Slot::fromInt(0);
        NativeValue produced;
        const bool invoked = (*ctx.invoker)(nativeInt(args[1]), { argument }, produced);
        if (temporary != 0u) ctx.heap->release(temporary);
        if (!invoked) return Slot::fromInt(0);
        if (produced.asInt() != 0) ++total;
    }
    return Slot::fromInt(total);
}

/**
 * @brief `__std_vec_count_value(vec, 值, 是不是字符串, 是不是对象) -> 整数`：等于该值的元素个数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecCountValue(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::count");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue wanted;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &wanted)) return Slot::fromInt(0);
    const ObjectKey key = valueKey(wanted);
    std::int64_t total = 0;
    for (const ObjectValue &item : object->items)
    {
        const ObjectKey candidate = valueKey(item);
        if (candidate < key) continue;
        if (key < candidate) continue;
        ++total;
    }
    return Slot::fromInt(total);
}

/**
 * @brief 已排序 List 的 lower_bound 二分查找；相等元素返回第一项。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecBinarySearch(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::binarySearch");
    if (object == nullptr) return Slot::fromInt(-1);
    ObjectValue wanted;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &wanted))
        return Slot::fromInt(-1);
    const ObjectKey key = valueKey(wanted);
    std::size_t low = 0u;
    std::size_t high = object->items.size();
    while (low < high)
    {
        const std::size_t middle = low + (high - low) / 2u;
        if (valueKey(object->items[middle]) < key)
            low = middle + 1u;
        else
            high = middle;
    }
    if (low >= object->items.size() || valueKey(object->items[low]) < key || key < valueKey(object->items[low]))
        return Slot::fromInt(-1);
    return Slot::fromInt(static_cast<std::int64_t>(low));
}

/**
 * @brief 使用比较回调查找列表中的极值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @param maximum 是否选取最大值。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecExtreme(State &ctx, const Memory::Vector<Slot> &args, bool maximum)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Vector,
                                  maximum ? "std::lang::algorithms::max" : "std::lang::algorithms::min");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue first;
    const std::int64_t objectValue = nativeInt(args[3]);
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, objectValue, false, &first))
        return Slot::fromInt(0);
    const ObjectValue *selected = &first;
    for (const ObjectValue &item : object->items)
    {
        const bool replace = maximum ? valueKey(*selected) < valueKey(item) : valueKey(item) < valueKey(*selected);
        if (replace) selected = &item;
    }
    if (selected == &first && objectValue == 1) first.ownsObject = true;
    Slot result;
    if (!nativeSlot(ctx, *selected, &result)) return Slot::fromInt(0);
    return result;
}

/**
 * @brief 查找列表中的最小值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecMin(State &ctx, const Memory::Vector<Slot> &args)
{
    return vecExtreme(ctx, args, false);
}

/**
 * @brief 查找列表中的最大值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecMax(State &ctx, const Memory::Vector<Slot> &args)
{
    return vecExtreme(ctx, args, true);
}

/**
 * @brief 接收 `[low, high]` List 与 value，返回落在闭区间内的值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot vecClamp(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *bounds = nativeObject(ctx, args[0], StorageKind::Vector,
                                  "std::lang::algorithms::clamp");
    if (bounds == nullptr) return Slot::fromInt(0);
    if (bounds->items.size() != 2u)
    {
        ctx.error = "std::lang::algorithms::clamp 的边界胶水需要 low/high 两项";
        return Slot::fromInt(0);
    }
    if (valueKey(bounds->items[1]) < valueKey(bounds->items[0]))
    {
        ctx.error = "std::lang::algorithms::clamp 要求 low <= high";
        return Slot::fromInt(0);
    }
    ObjectValue value;
    const std::int64_t objectValue = nativeInt(args[3]);
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, objectValue, false, &value))
        return Slot::fromInt(0);
    const ObjectValue *selected = &value;
    if (valueKey(value) < valueKey(bounds->items[0]))
        selected = &bounds->items[0];
    else if (valueKey(bounds->items[1]) < valueKey(value))
        selected = &bounds->items[1];
    if (selected == &value && objectValue == 1) value.ownsObject = true;
    Slot result;
    if (!nativeSlot(ctx, *selected, &result)) return Slot::fromInt(0);
    return result;
}

/**
 * @brief 创建空的运行时集合。
 *
 * `__std_set_new() -> set`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 * @param args 按此原生操作的绑定约定排列的实参槽；无参操作为空。
 */
inline Slot setNew(State &ctx, const Memory::Vector<Slot> &args)
{
    (void)args;
    return nativeNewObject(ctx, StorageKind::Set);
}

/**
 * @brief 为自定义 Set 元素绑定 Equal 与可选 Hash，复制容器时保留策略。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param args 按此原生操作的绑定约定排列的实参槽；无参操作为空。
 * @return 原生返回槽；失败时返回零并保留运行时诊断。
 */
inline Slot setNewPolicy(State &ctx, const Memory::Vector<Slot> &args)
{
    const Slot result = nativeNewObject(ctx, StorageKind::Set);
    if (auto *object = ctx.objects->find(result.integer))
    {
        object->mapEqual = nativeInt(args[0]);
        object->mapHash = nativeInt(args[1]);
    }
    return result;
}

/**
 * @brief Set 与 Map 共用精确 Equal、可选 Hash 及回调重入护栏。
 * @param ctx 本次集合胶水调用的运行时适配状态。
 * @param object 已校验的集合宿主记录。
 * @param query 传给用户 Equal 或 Hash 的原始查询键槽。
 * @param lookup 查询键快照；哈希策略启用时接收已计算的哈希。
 * @return 原生返回槽；失败时返回零并保留运行时诊断。
 */
inline auto mapFind(State &ctx, Record &object, const Slot &query, ObjectKey *lookup)
    -> Entries::iterator;

/**
 * @brief `__std_set_add(set, 值, 是不是字符串, 是不是对象句柄) -> 0`（已经在里面就什么都不做）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot setAdd(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Set, "set.add");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &value)) return Slot::fromInt(0);
    ObjectKey key = valueKey(value);
    const auto found = mapFind(ctx, *object, args[1], &key);
    if (!ctx.error.empty()) return Slot::fromInt(0);
    if (found == object->entries.end())
    {
        ObjectValue stored;
        if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), true,
                         &stored)) return Slot::fromInt(0);
        if (object->mapEqual && !ctx.objects->ownKey(&key, &ctx.error))
        {
            ctx.objects->releaseValue(stored);
            return Slot::fromInt(0);
        }
        if (!object->mapEqual)
        {
            auto storedKey = valueKey(stored);
            storedKey.hashed = key.hashed;
            storedKey.hash = key.hash;
            key = std::move(storedKey);
        }
        object->entries.emplace(std::move(key), std::move(stored));
    }
    return Slot::fromInt(0);
}

/**
 * @brief `__std_set_has(set, 值, 是不是字符串, 是不是对象句柄) -> 0 / 1`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot setHas(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Set, "set.has");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &value)) return Slot::fromInt(0);
    ObjectKey key = valueKey(value);
    const auto found = mapFind(ctx, *object, args[1], &key);
    return Slot::fromInt(found != object->entries.end() ? 1 : 0);
}

/**
 * @brief `__std_set_remove(set, 值, 是不是字符串, 是不是对象句柄) -> 0 / 1`（在不在）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot setRemove(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Set, "set.remove");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &value)) return Slot::fromInt(0);
    ObjectKey key = valueKey(value);
    const auto found = mapFind(ctx, *object, args[1], &key);
    if (!ctx.error.empty()) return Slot::fromInt(0);
    if (found == object->entries.end()) return Slot::fromInt(0);
    ctx.objects->releaseKey(found->first);
    ctx.objects->releaseValue(found->second);
    object->entries.erase(found);
    return Slot::fromInt(1);
}

/**
 * @brief `__std_set_at(set, 第几个, 是不是字符串, 是不是对象句柄) -> 值`（遍历顺序不保证稳定）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot setAt(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Set, "set.at");
    if (object == nullptr) return Slot::fromInt(0);
    const std::int64_t wanted = nativeInt(args[1]);
    if (wanted < 0 || static_cast<std::size_t>(wanted) >= object->entries.size())
    {
        ctx.error = "set.at 的位置越界：" + Memory::toString(wanted) + "（元素个数 " + Memory::toString(object->entries.size()) + "）";
        return Slot::fromInt(0);
    }
    auto it = object->entries.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(wanted));
    /* 集合里存进去的"值"就是元素本身（`setAdd` 用的是 `entries[key] = value`），
     * 所以直接取 `second` 就行。 */
    Slot out;
    if (!nativeSlot(ctx, it->second, &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief `__std_set_len(set) -> 元素个数`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot setLen(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Set, "len(set)");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(object->entries.size()));
}

/**
 * @brief 释放运行时集合的全部元素。
 *
 * `__std_set_clear(set) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot setClear(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Set, "set.clear");
    if (object == nullptr) return Slot::fromInt(0);
    for (const auto &entry : object->entries)
    {
        ctx.objects->releaseKey(entry.first);
        ctx.objects->releaseValue(entry.second);
    }
    object->entries.clear();
    return Slot::fromInt(0);
}

/**
 * @brief 创建空的运行时映射。
 *
 * `__std_map_new() -> map`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 * @param args 按此原生操作的绑定约定排列的实参槽；无参操作为空。
 */
inline Slot mapNew(State &ctx, const Memory::Vector<Slot> &args)
{
    (void)args;
    return nativeNewObject(ctx, StorageKind::Map);
}

/**
 * @brief 为自定义键 Map 绑定等于与可选哈希函数。
 * @param ctx 运行时上下文。
 * @param args Equal 和 Hash 函数地址；Hash 为零表示顺序等于查找。
 * @return 新建 Map 句柄。
 */
inline Slot mapNewPolicy(State &ctx, const Memory::Vector<Slot> &args)
{
    const Slot result = nativeNewObject(ctx, StorageKind::Map);
    if (auto *object = ctx.objects->find(result.integer))
    {
        object->mapEqual = nativeInt(args[0]);
        object->mapHash = nativeInt(args[1]);
    }
    return result;
}

/**
 * @brief 把存储键还原为回调可借用的槽，临时文本和聚合内存在回调后回收。
 * @param ctx 运行时上下文。
 * @param key 存入 Map 的键快照。
 * @param out 接收可传给 Equal 回调的键槽。
 * @param temporaries 登记回调结束时需要释放的堆地址。
 * @return 还原成功返回 true；失败设置运行时错误。
 */
inline bool mapCallbackKey(State &ctx, const ObjectKey &key, Slot *out,
                           Memory::Vector<std::size_t> *temporaries)
{
    if (key.isStruct)
    {
        const auto address = nativeAllocate(ctx, std::max<std::size_t>(8u, key.fields.size() * 8u));
        if (!address)
        {
            ctx.error = "Map 比较键时堆内存不足";
            return false;
        }
        temporaries->push_back(address);
        for (std::size_t i = 0; i < key.fields.size(); ++i)
        {
            Slot field;
            if (!mapCallbackKey(ctx, key.fields[i], &field, temporaries)) return false;
            std::int64_t bits = field.integer;
            if (field.isFloat) std::memcpy(&bits, &field.real, sizeof(bits));
            if (!ctx.heap->writeBytes(address + i * 8u, reinterpret_cast<const char *>(&bits), 8u))
            {
                ctx.error = "Map 比较键时写入临时槽失败";
                return false;
            }
        }
        *out = Slot::fromInt(static_cast<std::int64_t>(address));
    }
    else if (key.isText)
    {
        const auto address = nativeAllocString(ctx, key.text);
        if (!address) return false;
        temporaries->push_back(address);
        *out = Slot::fromInt(static_cast<std::int64_t>(address));
    }
    else if (key.isFloat)
        *out = Slot::fromDouble(key.real);
    else
        *out = Slot::fromInt(key.isObject && key.ownsObject ? key.ownedInteger : key.integer);
    return true;
}

/**
 * @brief 通过内建比较或自定义 Equal 查找键，Hash 可选并缓存到存储键。
 * @param ctx 运行时上下文。
 * @param object Map 对象。
 * @param query 原始查询键槽，传递给用户回调。
 * @param lookup 查询键快照，Hash 启用时补写缓存信息。
 * @return 找到的迭代器；未找到或回调失败返回 end。
 */
inline auto mapFind(State &ctx, Record &object, const Slot &query, ObjectKey *lookup)
    -> Entries::iterator
{
    if (!object.mapEqual)
    {
        if (object.mapHash != 0)
        {
            lookup->hashed = true;
            lookup->hash = static_cast<std::int64_t>(intrinsicHash(*lookup));
        }
        return object.entries.find(*lookup);
    }
    if (!ctx.invoker)
    {
        ctx.error = "当前后端不支持 Map 键回调";
        return object.entries.end();
    }
    struct Guard
    {
        Record &object;
        /**
         * @brief 保存并切换当前运行时回调的临时状态。
         * @param value 待转换、保存或参与计算的值。
         */
        explicit Guard(Record &value) : object(value) { object.mapComparing = true; }
        /**
         * @brief 恢复当前运行时回调的临时状态。
         */
        ~Guard() { object.mapComparing = false; }
    } guard(object);
    const auto nativeQuery = query.isFloat ? NativeValue::fromFloat(query.real) : NativeValue::fromInt(query.integer);
    if (object.mapHash)
    {
        NativeValue hash;
        if (!(*ctx.invoker)(object.mapHash, { nativeQuery }, hash)) return object.entries.end();
        lookup->hashed = true;
        lookup->hash = hash.asInt();
    }
    const auto *candidates = lookup->hashed ? &object.entries.bucket(lookup->hash) : nullptr;
    const auto count = candidates ? candidates->size() : object.entries.size();
    for (std::size_t position = 0; position < count; ++position)
    {
        const auto index = candidates ? (*candidates)[position] : position;
        auto it = object.entries.begin() + index;
        Memory::Vector<std::size_t> temporaries;
        struct Release
        {
            State &ctx;
            Memory::Vector<std::size_t> &addresses;
            /**
             * @brief 释放当前作用域持有的临时运行时对象。
             */
            ~Release()
            {
                for (auto it = addresses.rbegin(); it != addresses.rend(); ++it) ctx.heap->release(*it);
            }
        } release{ ctx, temporaries };
        Slot stored;
        if (!mapCallbackKey(ctx, it->first, &stored, &temporaries)) return object.entries.end();
        const auto nativeStored = stored.isFloat ? NativeValue::fromFloat(stored.real) : NativeValue::fromInt(stored.integer);
        NativeValue equal;
        if (!(*ctx.invoker)(object.mapEqual, { nativeStored, nativeQuery }, equal)) return object.entries.end();
        if (equal.asInt()) return it;
    }
    return object.entries.end();
}

/**
 * @brief `__std_map_put(map, 键, 键是不是字符串, 键是不是对象句柄, 值, 值是不是字符串, 值是不是对象句柄) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot mapPut(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Map, "map.put");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue key;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &key)) return Slot::fromInt(0);
    ObjectValue value;
    if (!nativeValue(ctx, args[4], nativeInt(args[5]) != 0, nativeInt(args[6]), true,
                     &value)) return Slot::fromInt(0);
    ObjectKey lookup = valueKey(key);
    const auto found = mapFind(ctx, *object, args[1], &lookup);
    if (!ctx.error.empty())
    {
        ctx.objects->releaseValue(value);
        return Slot::fromInt(0);
    }
    if (found != object->entries.end())
    {
        ctx.objects->releaseValue(found->second);
        found->second = std::move(value);
        return Slot::fromInt(0);
    }
    ObjectKey storedKey = lookup;
    if (storedKey.isObject || storedKey.isStruct)
    {
        Memory::String error;
        if (!ctx.objects->ownKey(&storedKey, &error))
        {
            ctx.error = error;
            ctx.objects->releaseValue(value);
            return Slot::fromInt(0);
        }
    }
    object->entries.emplace(std::move(storedKey), std::move(value));
    return Slot::fromInt(0);
}

/**
 * @brief `__std_map_get(map, 键, 键是不是字符串, 键是不是对象句柄, 值是不是字符串, 值是不是对象句柄) -> 值`（没有这个键直接报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot mapGet(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Map, "map.get");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue key;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &key)) return Slot::fromInt(0);
    ObjectKey lookup = valueKey(key);
    const auto found = mapFind(ctx, *object, args[1], &lookup);
    if (!ctx.error.empty()) return Slot::fromInt(0);
    if (found == object->entries.end())
    {
        /* 没有这个键：**直接报错**，不再给"零值"。
         * 静默返回 0 / 空串会把"键拼错了"变成一个看不出错的结果 —— 初学者最难查的那类坑。
         * 要"不在就走另一条路"，先用 `has()` 判断。 */
        const Memory::String shown = key.isText ? key.text
                                    : key.isStruct ? Memory::String("<复合键>")
                                                   : Memory::toString(key.integer);
        ctx.error = "map 下标键不存在：" + shown;
        return Slot::fromInt(0);
    }
    Slot out;
    if (!nativeSlot(ctx, found->second, &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief `__std_map_has(map, 键, 键是不是字符串, 键是不是对象句柄) -> 0 / 1`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot mapHas(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Map, "map.has");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue key;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &key)) return Slot::fromInt(0);
    ObjectKey lookup = valueKey(key);
    const auto found = mapFind(ctx, *object, args[1], &lookup);
    return Slot::fromInt(found != object->entries.end() ? 1 : 0);
}

/**
 * @brief `__std_map_remove(map, 键, 键是不是字符串, 键是不是对象句柄) -> 0 / 1`（在不在）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot mapRemove(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Map, "map.remove");
    if (object == nullptr) return Slot::fromInt(0);
    ObjectValue key;
    if (!nativeValue(ctx, args[1], nativeInt(args[2]) != 0, nativeInt(args[3]), false,
                     &key)) return Slot::fromInt(0);
    ObjectKey lookup = valueKey(key);
    const auto found = mapFind(ctx, *object, args[1], &lookup);
    if (!ctx.error.empty()) return Slot::fromInt(0);
    if (found == object->entries.end()) return Slot::fromInt(0);
    ctx.objects->releaseKey(found->first);
    ctx.objects->releaseValue(found->second);
    object->entries.erase(found);
    return Slot::fromInt(1);
}

/**
 * @brief `__std_map_len(map) -> 键值对个数`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot mapLen(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Map, "len(map)");
    if (object == nullptr) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(object->entries.size()));
}

/**
 * @brief 释放运行时映射的全部键值。
 *
 * `__std_map_clear(map) -> 0`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot mapClear(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Map, "map.clear");
    if (object == nullptr) return Slot::fromInt(0);
    for (const auto &entry : object->entries)
    {
        ctx.objects->releaseKey(entry.first);
        ctx.objects->releaseValue(entry.second);
    }
    object->entries.clear();
    return Slot::fromInt(0);
}

/**
 * @brief `__std_map_key_at(map, 第几个, 键是不是字符串, 键是不是对象句柄) -> 键`（遍历顺序不保证稳定）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot mapKeyAt(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Map, "map.keyAt");
    if (object == nullptr) return Slot::fromInt(0);
    const std::int64_t wanted = nativeInt(args[1]);
    if (wanted < 0 || static_cast<std::size_t>(wanted) >= object->entries.size())
    {
        ctx.error = "map.keyAt 的位置越界：" + Memory::toString(wanted) + "（键值对个数 " + Memory::toString(object->entries.size()) + "）";
        return Slot::fromInt(0);
    }
    auto it = object->entries.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(wanted));
    const auto fromKey = [&](const auto &self, const ObjectKey &source) -> ObjectValue {
        ObjectValue value;
        value.isStruct = source.isStruct;
        value.isText = source.isText;
        value.isFloat = source.isFloat;
        value.integer = source.ownsObject ? source.ownedInteger : source.integer;
        value.objectIdentity = source.isObject ? source.integer : 0;
        value.real = source.real;
        value.text = source.text;
        value.ownsObject = source.ownsObject;
        for (const ObjectKey &field : source.fields) value.fields.push_back(self(self, field));
        return value;
    };
    ObjectValue key = fromKey(fromKey, it->first);
    Slot out;
    if (!nativeSlot(ctx, key, &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief `__std_map_val_at(map, 第几个, 值是不是字符串, 值是不是对象句柄) -> 值`（遍历顺序不保证稳定）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Slot mapValAt(State &ctx, const Memory::Vector<Slot> &args)
{
    Record *object = nativeObject(ctx, args[0], StorageKind::Map, "map.valAt");
    if (object == nullptr) return Slot::fromInt(0);
    const std::int64_t wanted = nativeInt(args[1]);
    if (wanted < 0 || static_cast<std::size_t>(wanted) >= object->entries.size())
    {
        ctx.error = "map.valAt 的位置越界：" + Memory::toString(wanted) + "（键值对个数 " + Memory::toString(object->entries.size()) + "）";
        return Slot::fromInt(0);
    }
    auto it = object->entries.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(wanted));
    Slot out;
    if (!nativeSlot(ctx, it->second, &out)) return Slot::fromInt(0);
    return out;
}

/**
 * @brief 创建原生调用可用的运行时列表。
 * @retval 创建原生调用可用的运行时列表。
 * @param host 当前调用所属的公开运行时上下文。
 */
std::int64_t makeList(stackvm::NativeCallContext &host)
{
    State state(host);
    if (state.objects == nullptr)
    {
        host.fail("没有接上对象表，建不了列表");
        return 0;
    }
    auto object = Memory::makeShared<Record>();
    object->kind = StorageKind::Vector;
    return static_cast<std::int64_t>(state.objects->add(std::move(object)));
}

/**
 * @brief 将一个原生值追加到运行时列表。
 * @param listHandle 目标运行时列表的句柄。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool listPush(stackvm::NativeCallContext &host, std::int64_t listHandle, const NativeValue &value)
{
    State state(host);
    if (state.objects == nullptr)
    {
        host.fail("没有接上对象表");
        return false;
    }
    Record *object = state.objects->find(listHandle);
    if (object == nullptr || object->kind != StorageKind::Vector)
    {
        host.fail("listPush 的句柄不是 List");
        return false;
    }
    ObjectValue out;
    if (value.isText())
    {
        out.isText = true;
        out.text = value.text;
    }
    else if (value.isFloat())
    {
        out.isFloat = true;
        out.real = value.asFloat();
    }
    else
    {
        out.integer = value.asInt();
    }
    object->items.push_back(std::move(out));
    return true;
}

/**
 * @brief 将结构体数据追加到运行时列表。
 * @param listHandle 目标运行时列表的句柄。
 * @param structAddress 结构体数据的运行时地址。
 * @param slotCount 结构体数据包含的值槽数量。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool listPushStruct(stackvm::NativeCallContext &host, std::int64_t listHandle, std::int64_t structAddress,
                    std::size_t slotCount)
{
    State state(host);
    if (state.objects == nullptr)
    {
        host.fail("没有接上对象表");
        return false;
    }
    Record *object = state.objects->find(listHandle);
    if (object == nullptr || object->kind != StorageKind::Vector)
    {
        host.fail("listPushStruct 的句柄不是 List");
        return false;
    }
    ObjectValue out;
    out.isStruct = true;
    out.fields.reserve(slotCount);
    for (std::size_t i = 0; i < slotCount; ++i)
    {
        /* 每个槽按 double 的位模式拷贝：CHALI 结构体字段都是 8 字节对齐，
         * 数值型结构体（如 Complex 的 re/im）这样拷最省事。 */
        double raw = 0.0;
        if (!host.readBytes(structAddress + static_cast<std::int64_t>(i * 8u),
                            reinterpret_cast<char *>(&raw), sizeof(raw)))
        {
            host.fail("listPushStruct 读结构体槽失败");
            return false;
        }
        ObjectValue field;
        field.isFloat = true;
        field.real = raw;
        out.fields.push_back(std::move(field));
    }
    object->items.push_back(std::move(out));
    return true;
}

/**
 * @brief 从列表项读取结构体数据。
 * @param handle 待操作的运行时句柄。
 * @param slotCount 结构体数据包含的值槽数量。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool readListStruct(stackvm::NativeCallContext &host, std::int64_t handle, std::size_t slotCount,
                    Memory::Vector<double> &out)
{
    State state(host);
    out.clear();
    if (state.objects == nullptr) return false;
    Record *object = state.objects->find(handle);
    if (object == nullptr || object->kind != StorageKind::Vector) return false;
    if (!nativeRefreshViews(state, *object))
    {
        if (!state.error.empty()) host.fail(state.error);
        return false;
    }
    for (const ObjectValue &item : object->items)
    {
        if (!item.isStruct) return false;
        for (std::size_t i = 0; i < slotCount; ++i)
        {
            if (i < item.fields.size())
            {
                const ObjectValue &field = item.fields[i];
                out.push_back(field.isFloat ? field.real : static_cast<double>(field.integer));
            }
            else
            {
                out.push_back(0.0);
            }
        }
    }
    return true;
}

/**
 * @brief 将运行时列表转换为原生值数组。
 * @param handle 待操作的运行时句柄。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool readList(stackvm::NativeCallContext &host, std::int64_t handle,
              Memory::Vector<NativeValue> &out)
{
    State state(host);
    out.clear();
    if (state.objects == nullptr) return false;
    Record *object = state.objects->find(handle);
    if (object == nullptr || object->kind != StorageKind::Vector) return false;
    if (!nativeRefreshViews(state, *object))
    {
        if (!state.error.empty()) host.fail(state.error);
        return false;
    }
    out.reserve(object->items.size());
    for (const ObjectValue &item : object->items)
    {
        if (item.isText)
            out.push_back(NativeValue::fromText(item.text));
        else if (item.isFloat)
            out.push_back(NativeValue::fromFloat(item.real));
        else
            out.push_back(NativeValue::fromInt(item.integer));
    }
    return true;
}

/**
 * @brief 创建原生调用可用的运行时映射。
 * @retval 创建原生调用可用的运行时映射。
 * @param host 当前调用所属的公开运行时上下文。
 */
std::int64_t makeMap(stackvm::NativeCallContext &host)
{
    State state(host);
    if (state.objects == nullptr)
    {
        host.fail("没有接上对象表，建不了映射");
        return 0;
    }
    auto object = Memory::makeShared<Record>();
    object->kind = StorageKind::Map;
    return static_cast<std::int64_t>(state.objects->add(std::move(object)));
}

/**
 * @brief 创建原生调用可用的运行时集合。
 * @retval 创建原生调用可用的运行时集合。
 * @param host 当前调用所属的公开运行时上下文。
 */
std::int64_t makeSet(stackvm::NativeCallContext &host)
{
    State state(host);
    if (state.objects == nullptr)
    {
        host.fail("没有接上对象表，建不了集合");
        return 0;
    }
    auto object = Memory::makeShared<Record>();
    object->kind = StorageKind::Set;
    return static_cast<std::int64_t>(state.objects->add(std::move(object)));
}

/**
 * @brief 将运行时值转换为容器键。
 * @param value 待转换、保存或参与计算的值。
 * @retval 保存将运行时值转换为容器键结果的对象。
 */
inline ObjectKey keyFromValue(const NativeValue &value)
{
    ObjectKey key;
    if (value.isText())
    {
        key.isText = true;
        key.text = value.text;
    }
    else if (value.isFloat())
    {
        key.isFloat = true;
        key.real = value.asFloat();
    }
    else
    {
        key.integer = value.asInt();
    }
    return key;
}
/**
 * @brief 将宿主原生值转换为对象存储值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 保存将宿主原生值转换为对象存储值结果的对象。
 */
inline ObjectValue valueFromNative(const NativeValue &value)
{
    ObjectValue out;
    if (value.isText())
    {
        out.isText = true;
        out.text = value.text;
    }
    else if (value.isFloat())
    {
        out.isFloat = true;
        out.real = value.asFloat();
    }
    else
    {
        out.integer = value.asInt();
    }
    return out;
}

/**
 * @brief 向运行时映射写入一个键值对。
 * @param mapHandle 目标运行时映射的句柄。
 * @param key 查找或写入使用的键。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool mapSet(stackvm::NativeCallContext &host, std::int64_t mapHandle, const NativeValue &key,
            const NativeValue &value)
{
    State state(host);
    if (state.objects == nullptr)
    {
        host.fail("没有接上对象表");
        return false;
    }
    Record *object = state.objects->find(mapHandle);
    if (object == nullptr || object->kind != StorageKind::Map)
    {
        host.fail("mapSet 的句柄不是 Map");
        return false;
    }
    object->entries[keyFromValue(key)] = valueFromNative(value);
    return true;
}

/**
 * @brief 将原生值插入运行时集合。
 * @param setHandle 目标运行时集合的句柄。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool setInsert(stackvm::NativeCallContext &host, std::int64_t setHandle, const NativeValue &value)
{
    State state(host);
    if (state.objects == nullptr)
    {
        host.fail("没有接上对象表");
        return false;
    }
    Record *object = state.objects->find(setHandle);
    if (object == nullptr || object->kind != StorageKind::Set)
    {
        host.fail("setInsert 的句柄不是 Set");
        return false;
    }
    /* Set 的值不用，键存元素本身。 */
    object->entries[keyFromValue(value)] = valueFromNative(value);
    return true;
}

/**
 * @brief 将容器键转换为宿主原生值.
 * @param key 查找或写入使用的键。
 * @retval 封装为原生值的运行时结果。
 */
inline NativeValue nativeFromKey(const ObjectKey &key)
{
    if (key.isText) return NativeValue::fromText(key.text);
    if (key.isFloat) return NativeValue::fromFloat(key.real);
    return NativeValue::fromInt(key.integer);
}
/**
 * @brief 将对象存储值转换为宿主原生值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
inline NativeValue nativeFromValue(const ObjectValue &value)
{
    if (value.isText) return NativeValue::fromText(value.text);
    if (value.isFloat) return NativeValue::fromFloat(value.real);
    return NativeValue::fromInt(value.integer);
}

/**
 * @brief 读取运行时映射的全部键值。
 * @param handle 待操作的运行时句柄。
 * @param keys 接收映射键的输出列表。
 * @param values 输入值列表或接收读取值的输出列表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool readMap(stackvm::NativeCallContext &host, std::int64_t handle, Memory::Vector<NativeValue> &keys,
             Memory::Vector<NativeValue> &values)
{
    State state(host);
    keys.clear();
    values.clear();
    if (state.objects == nullptr) return false;
    Record *object = state.objects->find(handle);
    if (object == nullptr || object->kind != StorageKind::Map) return false;
    for (const auto &entry : object->entries)
    {
        keys.push_back(nativeFromKey(entry.first));
        values.push_back(nativeFromValue(entry.second));
    }
    return true;
}

/**
 * @brief 读取运行时集合中的原生值。
 * @param handle 待操作的运行时句柄。
 * @param elements 元素类型或元素值列表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool readSet(stackvm::NativeCallContext &host, std::int64_t handle, Memory::Vector<NativeValue> &elements)
{
    State state(host);
    elements.clear();
    if (state.objects == nullptr) return false;
    Record *object = state.objects->find(handle);
    if (object == nullptr || object->kind != StorageKind::Set) return false;
    for (const auto &entry : object->entries) elements.push_back(nativeFromKey(entry.first));
    return true;
}

/**
 * @brief 不解释运行时句柄地复制记录快照，供跨运行时传输使用。
 * @param source 原始记录。
 * @param error 接收失败原因；成功时保持为空。
 * @return 独立的宿主记录快照。
 */
inline void *cloneRecord(const void *source, Memory::String &error)
{
    (void)error;
    auto *copy = new Record(*static_cast<const Record *>(source));
    copy->elementViews.clear();
    copy->iteratorLeases = 0;
    copy->pendingRelease = false;
    copy->refreshingViews = false;
    copy->mapComparing = false;
    return copy;
}
/**
 * @brief 销毁已由 dispose 清理拥有值的记录本体。
 * @param record 待销毁的宿主记录。
 */
inline void destroyRecord(void *record) { delete static_cast<Record *>(record); }
/**
 * @brief 同步活跃元素引用后再供运行时复制或格式化。
 * @param host 当前运行时上下文。
 * @param record 待同步记录。
 * @return 同步成功时返回 true。
 */
inline bool prepareRecord(stackvm::NativeCallContext &host, void *record)
{
    State state(host);
    const bool ok = nativeRefreshViews(state, *static_cast<Record *>(record));
    if (!state.error.empty()) host.fail(state.error);
    return ok;
}
/**
 * @brief 将键快照还原为通用值字段，供文本与消息传输访问。
 * @param key 键快照。
 * @return 保持值种类及拥有关系的通用槽。
 */
inline ObjectValue keyValue(const ObjectKey &key)
{
    ObjectValue value;
    value.isStruct = key.isStruct;
    value.isText = key.isText;
    value.isFloat = key.isFloat;
    value.ownsObject = key.ownsObject;
    value.integer = key.isObject ? key.ownedInteger : key.integer;
    value.objectIdentity = key.isObject ? key.integer : 0;
    value.real = key.real;
    value.text = key.text;
    for (const auto &field : key.fields) value.fields.push_back(keyValue(field));
    return value;
}
/**
 * @brief 递归复制已拥有的键值句柄，保留键身份及 Equal/Hash 策略。
 * @param host 目标运行时的上下文。
 * @param record 已同步的源记录。
 * @return 独立副本；失败返回空并保留原始运行时诊断。
 */
inline void *copyRecord(stackvm::NativeCallContext &host, const void *record)
{
    State state(host);
    Memory::String error;
    const auto &source = *static_cast<const Record *>(record);
    auto *copy = new Record;
    copy->kind = source.kind;
    copy->mapEqual = source.mapEqual;
    copy->mapHash = source.mapHash;
    copy->iteratorLease = source.iteratorLease;
    for (auto item : source.items)
    {
        if (!state.objects->copyValue(&item, &error))
        {
            disposeRecord(host, copy);
            delete copy;
            return nullptr;
        }
        copy->items.push_back(std::move(item));
    }
    for (const auto &entry : source.entries)
    {
        auto key = keyValue(entry.first);
        auto value = entry.second;
        if (!state.objects->copyValue(&key, &error))
        {
            disposeRecord(host, copy);
            delete copy;
            return nullptr;
        }
        if (!state.objects->copyValue(&value, &error))
        {
            state.objects->releaseValue(key);
            disposeRecord(host, copy);
            delete copy;
            return nullptr;
        }
        auto stored = source.kind == StorageKind::Set && !source.mapEqual ? valueKey(value) : valueKey(key, true);
        stored.hashed = entry.first.hashed;
        stored.hash = entry.first.hash;
        copy->entries.emplace(std::move(stored), std::move(value));
    }
    return copy;
}
/**
 * @brief 对活跃元素借用接受延期释放，在 Equal/Hash 回调期间拒绝释放。
 * @param record 待释放记录。
 * @return 可立即释放、已接受延期或当前回调拒绝释放的决定。
 */
inline stackvm::NativeRecordRelease requestRelease(void *record)
{
    auto &value = *static_cast<Record *>(record);
    if (value.mapComparing) return stackvm::NativeRecordRelease::Rejected;
    if (value.iteratorLeases || !value.elementViews.empty())
    {
        value.pendingRelease = true;
        return stackvm::NativeRecordRelease::Deferred;
    }
    return stackvm::NativeRecordRelease::Immediate;
}
/**
 * @brief 清理记录拥有的元素、键值及最后一个游标副本的借用单元。
 * @param host 所属运行时上下文。
 * @param record 待释放记录。
 */
inline void disposeRecord(stackvm::NativeCallContext &host, void *record)
{
    State state(host);
    auto &value = *static_cast<Record *>(record);
    if (value.iteratorLease && value.iteratorLease.use_count() == 1)
    {
        auto &lease = *value.iteratorLease;
        for (const auto &cell : lease.cells)
        {
            if (!state.objects->find(lease.source)) break;
            state.error.clear();
            vecUnref(state, { Slot::fromInt(lease.source), Slot::fromInt(static_cast<std::int64_t>(cell.second)) });
        }
        auto *source = state.objects->find(lease.source);
        if (source && source->iteratorLeases) --source->iteratorLeases;
        if (source && source->pendingRelease && !source->iteratorLeases && source->elementViews.empty()) state.objects->release(lease.source);
    }
    for (const auto &view : value.elementViews)
    {
        nativeReleaseViewContents(state, view.second);
        state.heap->release(view.second.address);
    }
    for (const auto &item : value.items) state.objects->releaseValue(item);
    for (const auto &entry : value.entries)
    {
        state.objects->releaseKey(entry.first);
        state.objects->releaseValue(entry.second);
    }
}
/**
 * @brief 暴露拥有型子值供 VM 的通用消息快照协议遍历。
 * @param record 宿主记录。
 * @param visitor 处理并可重写子值句柄的访问器。
 * @return 所有字段处理成功时返回 true。
 */
inline bool visitRecord(void *record, const Memory::Function<bool(ObjectValue &)> &visitor)
{
    auto &value = *static_cast<Record *>(record);
    for (auto &item : value.items)
        if (!visitor(item)) return false;
    Entries entries;
    for (auto entry : value.entries)
    {
        ObjectKey stored;
        if (value.kind == StorageKind::Set && !value.mapEqual)
        {
            if (!visitor(entry.second)) return false;
            stored = valueKey(entry.second);
        }
        else
        {
            auto key = keyValue(entry.first);
            if (!visitor(key) || !visitor(entry.second)) return false;
            stored = valueKey(key, true);
        }
        stored.hashed = entry.first.hashed;
        stored.hash = entry.first.hash;
        entries.emplace(std::move(stored), std::move(entry.second));
    }
    value.entries = std::move(entries);
    return true;
}
/**
 * @brief 由集合胶水提供标点及字段，运行时仅按静态布局渲染通用值。
 * @param host 当前运行时上下文。
 * @param record 待格式化的记录快照。
 * @param view 接收分隔符和元素字段。
 * @return 成功构造文本视图时返回 true。
 */
inline bool textRecord(stackvm::NativeCallContext &host, const void *record, stackvm::NativeRecordTextView &view)
{
    (void)host;
    const auto &value = *static_cast<const Record *>(record);
    const bool list = value.kind == StorageKind::Vector;
    view.prefix = list ? "[" : "{";
    view.suffix = list ? "]" : "}";
    if (list)
        for (const auto &item : value.items) view.rows.push_back({ item });
    else
        for (const auto &entry : value.entries)
        {
            if (value.kind == StorageKind::Map)
                view.rows.push_back({ keyValue(entry.first), entry.second });
            else
                view.rows.push_back({ entry.second });
        }
    return true;
}
/**
 * @brief 返回胶水独占的描述符身份，VM 不根据名称选择存储实现。
 * @param kind 胶水存储类别。
 * @return 静态生命周期的通用记录描述符。
 */
inline const stackvm::NativeRecordType &descriptor(StorageKind kind)
{
    static const stackvm::NativeRecordType list{ "ArrayList", cloneRecord, destroyRecord, nullptr, copyRecord, disposeRecord, prepareRecord, requestRelease, visitRecord, textRecord, "chali.std.ArrayList.1" };
    static const stackvm::NativeRecordType set{ "Set", cloneRecord, destroyRecord, nullptr, copyRecord, disposeRecord, prepareRecord, requestRelease, visitRecord, textRecord, "chali.std.Set.1" };
    static const stackvm::NativeRecordType map{ "Map", cloneRecord, destroyRecord, nullptr, copyRecord, disposeRecord, prepareRecord, requestRelease, visitRecord, textRecord, "chali.std.Map.1" };
    static const stackvm::NativeRecordType lease{ "iterator lease", nullptr, destroyRecord, nullptr, copyRecord, disposeRecord, nullptr, nullptr, nullptr, nullptr, "chali.std.ListLease.1" };
    return kind == StorageKind::Vector ? list : kind == StorageKind::Set ? set :
                                            kind == StorageKind::Map     ? map :
                                                                           lease;
}
/**
 * @brief 将集合胶水的内部槽函数绑定到公开的原生注册表。
 * @param registry 目标注册表。
 * @param error 接收注册失败原因。
 * @return 全部绑定成功时返回 true。
 */
bool bind(stackvm::NativeRegistry &registry, Memory::String *error)
{
    struct Binding
    {
        const char *name;
        std::size_t arity;
        Slot (*apply)(State &, const Memory::Vector<Slot> &);
    };
    static const Binding bindings[] = {
        { "__std_program_arguments", 0, programArguments },
        { "__std_vec_new", 0, vecNew },
        { "__std_vec_pack", static_cast<std::size_t>(-1), vecPack },
        { "__std_vec_push", 4, vecPush },
        { "__std_vec_insert", 5, vecInsert },
        { "__std_vec_ref", 5, vecRef },
        { "__std_iter_take", 1, iterTake },
        { "__std_iter_lease", 1, iterLeaseNew },
        { "__std_iter_lease_len", 1, iterLeaseLen },
        { "__std_iter_lease_ref", 5, iterLeaseRef },
        { "__std_vec_unref", 2, vecUnref },
        { "__std_vec_at", 4, vecAt },
        { "__std_vec_set", 5, vecSet },
        { "__std_vec_remove", 2, vecRemove },
        { "__std_vec_pop", 3, vecPop },
        { "__std_vec_len", 1, vecLen },
        { "__std_vec_clear", 1, vecClear },
        { "__std_vec_stable_sort", 1, vecStableSort },
        { "__std_vec_sorted_copy", 1, vecSortedCopy },
        { "__std_vec_find", 4, vecFind },
        { "__std_vec_binary_search", 4, vecBinarySearch },
        { "__std_vec_min", 4, vecMin },
        { "__std_vec_max", 4, vecMax },
        { "__std_vec_clamp", 4, vecClamp },
        { "__std_vec_all", 2, vecAll },
        { "__std_vec_any", 2, vecAny },
        { "__std_vec_fold", 3, vecFold },
        { "__std_vec_map", 4, vecMap },
        { "__std_vec_filter", 4, vecFilter },
        { "__std_vec_reduce", 2, vecReduce },
        { "__std_vec_sum", 2, vecSum },
        { "__std_vec_count_if", 2, vecCountIf },
        { "__std_vec_count_value", 4, vecCountValue },
        { "__std_set_new", 0, setNew },
        { "__std_set_new_policy", 2, setNewPolicy },
        { "__std_set_add", 4, setAdd },
        { "__std_set_has", 4, setHas },
        { "__std_set_remove", 4, setRemove },
        { "__std_set_at", 4, setAt },
        { "__std_set_len", 1, setLen },
        { "__std_set_clear", 1, setClear },
        { "__std_map_new", 0, mapNew },
        { "__std_map_new_policy", 2, mapNewPolicy },
        { "__std_map_put", 7, mapPut },
        { "__std_map_get", 6, mapGet },
        { "__std_map_has", 4, mapHas },
        { "__std_map_remove", 4, mapRemove },
        { "__std_map_len", 1, mapLen },
        { "__std_map_clear", 1, mapClear },
        { "__std_map_key_at", 4, mapKeyAt },
        { "__std_map_val_at", 4, mapValAt },
        { "std::lang::__stringBytes", 1, stringBytes },
    };
    for (const auto &binding : bindings)
        if (!registry.add(binding.name, binding.arity, [apply = binding.apply](stackvm::NativeCallContext &host, const Memory::Vector<NativeValue> &arguments) {
            State state(host);
            Memory::Vector<Slot> slots;
            for (const auto &value : arguments) slots.push_back(value.isFloat() ? Slot::fromDouble(value.asFloat()) : Slot::fromInt(value.asInt()));
            const auto result = apply(state, slots);
            if (!state.error.empty()) host.fail(state.error);
            return result.isFloat ? NativeValue::fromFloat(result.real) : NativeValue::fromInt(result.integer); }, error)) return false;
    return true;
}
}  // namespace collection_glue
