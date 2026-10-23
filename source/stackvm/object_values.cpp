/**
 * @file object_values.cpp
 * @brief 对象字段、引用视图与存活范围检查。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 把一个实参当句柄取对象。
 * @param ctx 环境。
 * @param handle 句柄（`Slot`）。
 * @param kind 期望的种类。
 * @param who 谁的参数（诊断文案）。
 * @return 对象；不是句柄 / 已经释放 / 种类不对时写 `ctx.error` 并返回 nullptr。
 */
bool nativeRefreshViews(NativeContext &ctx, RuntimeObject &object);
/**
 * @brief 根据原生调用参数查找运行时对象。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @param kind 待处理对象的类别。
 * @param who 诊断信息中使用的操作名称。
 * @retval 匹配项的指针，未找到时为空指针。
 */
RuntimeObject *nativeObject(NativeContext &ctx, const Slot &handle, ObjectKind kind,
                            const char *who)
{
    if (ctx.objects == nullptr || !isObjectHandle(handle.integer))
    {
        ctx.error = Memory::String(who) + " 需要一个 " + objectKindName(kind) + " 值，拿到的是 " + Memory::toString(handle.integer) + "（不是对象句柄）";
        return nullptr;
    }
    RuntimeObject *object = ctx.objects->find(handle.integer);
    if (object == nullptr)
    {
        ctx.error = Memory::String(who) + " 用的 " + objectKindName(kind) + " 已经释放过了（句柄 " + Memory::toString(handle.integer) + "）";
        return nullptr;
    }
    if (object->kind != kind)
    {
        ctx.error = Memory::String(who) + " 要的是 " + objectKindName(kind) + "，拿到的是 " + objectKindName(object->kind);
        return nullptr;
    }
    if (object->kind != ObjectKind::Closure && !nativeRefreshViews(ctx, *object)) return nullptr;
    return object;
}

/**
 * @brief 一条实参 → 容器里的值 / 键。
 * @param ctx 环境。
 * @param value 值（数值就是 Slot 本身；字符串是它的地址）。
 * @param isText true = 按字符串读（**读成副本**）。
 * @param isObject true = 这个整数是一份由调用者持有的对象句柄。
 * @param copyObject true = 存入容器 / obj 时复制一份，false = 只用于查找比较。
 * @param out 结果。
 * @return 成功返回 true；字符串读不到时写 `ctx.error` 并返回 false。
 */
bool nativeValue(NativeContext &ctx, const Slot &value, bool isText, std::int64_t objectFlag,
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
            RuntimeObject *object = ctx.objects->find(value.integer);
            if (object == nullptr)
            {
                ctx.error = "对象键/值已经释放或不是有效句柄：" + Memory::toString(value.integer);
                return false;
            }
            if (!nativeRefreshViews(ctx, *object)) return false;
            out->objectIdentity = object->identity;
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
bool nativeSlot(NativeContext &ctx, const ObjectValue &value, Slot *out)
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
 */
bool nativeViewValue(NativeContext &ctx, const RuntimeObject::ElementView &view, ObjectValue *out)
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
 */
bool nativeRefreshViews(NativeContext &ctx, RuntimeObject &object)
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
 */
void nativeReleaseViewContents(NativeContext &ctx, const RuntimeObject::ElementView &view)
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
 */
bool nativeWriteView(NativeContext &ctx, const RuntimeObject::ElementView &view, const ObjectValue &value)
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
}  // namespace runtimeDetail
}  // namespace stackvm
