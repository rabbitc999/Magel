/**
 * @file object_store.cpp
 * @brief 运行时对象句柄、代次与资源清理。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * 对象句柄的起点（2^62）：编码堆地址与数据区地址均低于此边界。
 */

/**
 * @brief 种类名（诊断文案里用）。
 * @param kind 待处理对象的类别。
 * @retval 对应对象的指针。
 */
[[nodiscard]] const char *objectKindName(ObjectKind kind)
{
    switch (kind)
    {
    case ObjectKind::NativeRecord: return "native record";
    case ObjectKind::Closure: return "lambda";
    case ObjectKind::Obj: return "obj";
    case ObjectKind::Arena: return "arena";
    }
    return "?";
}

bool nativeRefreshViews(NativeContext &ctx, RuntimeObject &object);
bool nativeWriteView(NativeContext &ctx, const RuntimeObject::ElementView &view, const ObjectValue &value);

void nativeReleaseViewContents(NativeContext &ctx, const RuntimeObject::ElementView &view);  ///< 还活着的对象
/**
 * @brief 新分配或重新赋值的存储开始生命周期。
 * @param address 地址。
 * @param bytes 字节数。
 * @param fresh 是否为新分配块，需清除所有重叠的旧标记。
 */
void ObjectStore::beginStorage(std::size_t address, std::size_t bytes, bool fresh)

{
    for (auto it = movedStorage.begin(); it != movedStorage.end();)
    {
        const auto start = it->first.first;
        const bool overlap = start >= address ? start - address < bytes : address - start < it->second;
        const bool contained = start >= address && start - address < bytes && it->second <= bytes - (start - address);
        if (fresh ? overlap : contained)
            it = movedStorage.erase(it);
        else
            ++it;
    }
}

/**
 * @brief 释放对象存储中仍持有的全部资源。
 */
ObjectStore::~ObjectStore()

{
    shuttingDown = true;
    for (const auto &entry : objects)
        if (entry.second->kind == ObjectKind::NativeRecord) releaseContents(*entry.second);
    objects.clear();
}

/**
 * @brief 收下一个对象，返回它的句柄。
 * @param object 待访问的对象。
 * @retval 收下一个对象，返回它的句柄。
 */
std::int64_t ObjectStore::add(std::shared_ptr<RuntimeObject> object)

{
    const std::int64_t handle = next++;
    if (object->identity == 0) object->identity = handle;
    objects[handle] = std::move(object);
    return handle;
}

/**
 * @brief 根据名称或键查找匹配项。
 *
 * 按句柄找对象；已经释放 / 不认识返回 nullptr。
 * @param handle 待操作的运行时句柄。
 * @retval 匹配项的指针，未找到时为空指针。
 */
RuntimeObject *ObjectStore::find(std::int64_t handle)

{
    const auto found = objects.find(handle);
    return (found == objects.end()) ? nullptr : found->second.get();
}

/**
 * @brief 复制对象值并维护其引用或所有权。
 * @param value 待转换、保存或参与计算的值。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ObjectStore::copyValue(ObjectValue *value, Memory::String *error)

{
    if (value == nullptr) return true;
    for (ObjectValue &field : value->fields)
        if (!copyValue(&field, error)) return false;
    if (!value->ownsObject) return true;
    const std::int64_t nestedCopy = copyHandle(value->integer, error);
    if (nestedCopy == 0) return false;
    value->integer = nestedCopy;
    return true;
}

/**
 * @brief 释放对象值持有的资源。
 * @param value 待转换、保存或参与计算的值。
 */
void ObjectStore::releaseValue(const ObjectValue &value)

{
    for (const ObjectValue &field : value.fields) releaseValue(field);
    if (value.ownsObject) release(value.integer);
}

/**
 * @brief 释放通用对象拥有的槽值与宿主记录资源。
 * @param object 待访问的对象。
 */
void ObjectStore::releaseContents(const RuntimeObject &object)

{
    if (object.kind == ObjectKind::NativeRecord && object.nativeType && object.nativeType->dispose)
    {
        NativeCallContextImpl host(recordContext);
        object.nativeType->dispose(host, object.nativeRecord.get());
    }
    if (object.iteratorStateBox || object.closureCopy)
    {
        auto context = object.iteratorContext;
        for (const auto &entry : object.elementViews)
        {
            const auto destructor = object.captureDestructors.find(entry.first);
            if (destructor != object.captureDestructors.end() && context.invoker)
            {
                NativeValue result;
                (*context.invoker)(destructor->second, { NativeValue::fromInt(entry.second.address) }, result);
            }
            nativeReleaseViewContents(context, entry.second);
            context.heap->release(entry.second.address);
        }
    }
    for (const ObjectValue &value : object.items) releaseValue(value);
    if (object.kind == ObjectKind::Obj && object.objOwnsObject)
    {
        release(object.objValue.integer);
    }
}

/**
 * @brief 复制运行时对象并调用宿主记录的复制协议。
 * @param object 待访问的对象。
 * @param error 接收失败原因的错误信息。
 * @retval 对应对象的指针。
 */
std::shared_ptr<RuntimeObject> ObjectStore::copyObject(const RuntimeObject &object, Memory::String *error)

{
    if (object.kind == ObjectKind::NativeRecord)
    {
        const auto *type = object.nativeType;
        const auto original = object.nativeRecord;
        const auto identity = object.identity;
        if (!type || (!type->clone && !type->copy))
        {
            if (error) *error = Memory::String(type && type->name ? type->name : "宿主记录") + " 不能复制";
            return nullptr;
        }
        Memory::String reason;
        recordContext.error.clear();
        recordContext.memoryFault.clear();
        NativeCallContextImpl host(recordContext);
        if (type->prepare && !type->prepare(host, original.get()))
        {
            if (error) *error = recordContext.memoryFault.empty() ? recordContext.error : recordContext.memoryFault;
            return nullptr;
        }
        void *record = type->copy ? type->copy(host, original.get()) : type->clone(original.get(), reason);
        if (reason.empty()) reason = recordContext.memoryFault.empty() ? recordContext.error : recordContext.memoryFault;
        if (!record || record == original.get())
        {
            if (error) *error = reason.empty() ? "宿主复制回调没有返回独立记录" : reason;
            return nullptr;
        }
        auto copy = Memory::makeShared<RuntimeObject>();
        copy->kind = ObjectKind::NativeRecord;
        copy->nativeType = type;
        copy->nativeRecord = std::shared_ptr<void>(record, type->destroy);
        copy->identity = identity;
        return copy;
    }
    if (object.kind == ObjectKind::Arena)
    {
        if (error != nullptr)
        {
            *error = Memory::String(objectKindName(object.kind)) + " 句柄不能复制；请把它当作资源句柄传递并显式 close/delete";
        }
        return nullptr;
    }
    auto copy = Memory::makeShared<RuntimeObject>(object);
    copy->elementViews.clear();
    copy->refreshingViews = false;
    for (ObjectValue &value : copy->items)
    {
        if (!copyValue(&value, error)) return nullptr;
    }
    if (copy->kind == ObjectKind::Obj && copy->objOwnsObject)
    {
        if (!copyValue(&copy->objValue, error)) return nullptr;
    }
    if (object.closureCopy)
    {
        for (const auto &entry : object.elementViews)
        {
            auto view = entry.second;
            view.address = nativeAllocate(copy->iteratorContext, std::max<std::size_t>(8,
                                                                                       copy->items[entry.first].isStruct ? copy->items[entry.first].fields.size() * 8 : 8));
            if (!view.address || !nativeWriteView(copy->iteratorContext, view, copy->items[entry.first]))
            {
                if (error) *error = "闭包副本捕获存储分配失败";
                return nullptr;
            }
            copy->elementViews[entry.first] = view;
        }
    }
    return copy;
}

/**
 * @brief 复制运行时句柄并维护其对象引用。
 * @param handle 待操作的运行时句柄。
 * @param error 接收失败原因的错误信息。
 * @retval 复制运行时句柄并维护其对象引用。
 */
std::int64_t ObjectStore::copyHandle(std::int64_t handle, Memory::String *error)

{
    if (handle < kObjectHandleBase)
    {
        if (error != nullptr) *error = "不是一个能复制的对象句柄：" + Memory::toString(handle);
        return 0;
    }
    RuntimeObject *object = find(handle);
    if (object == nullptr)
    {
        if (error != nullptr) *error = "这个对象已经释放过了（句柄 " + Memory::toString(handle) + "）";
        return 0;
    }
    if ((object->iteratorStateBox || object->closureCopy) && !nativeRefreshViews(object->iteratorContext, *object))
    {
        if (error) *error = object->iteratorContext.error;
        return 0;
    }
    std::shared_ptr<RuntimeObject> copy = copyObject(*object, error);
    if (!copy) return 0;
    return add(std::move(copy));
}

/**
 * @brief 释放指定地址对应的运行时资源。
 *
 * 释放一个对象；确实释放了返回 true。
 * @param handle 待操作的运行时句柄。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ObjectStore::release(std::int64_t handle)

{
    const auto found = objects.find(handle);
    if (found == objects.end()) return false;
    if (found->second->kind == ObjectKind::NativeRecord && found->second->nativeType && found->second->nativeType->releaseDecision)
    {
        const auto decision = found->second->nativeType->releaseDecision(found->second->nativeRecord.get());
        if (decision == NativeRecordRelease::Deferred) return true;
        if (decision == NativeRecordRelease::Rejected) return false;
    }
    std::shared_ptr<RuntimeObject> object = std::move(found->second);
    objects.erase(found);
    releaseContents(*object);
    return true;
}

/**
 * @brief 还活着的对象个数（停机时 != 0 就是漏了）。
 * @retval 还活着的对象个数（停机时 != 0 就是漏了）。
 */
[[nodiscard]] std::size_t ObjectStore::liveCount() const
{ return objects.size(); }
}  // namespace runtimeDetail
}  // namespace stackvm
