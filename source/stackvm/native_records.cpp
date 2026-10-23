/**
 * @file native_records.cpp
 * @brief 宿主资源记录的创建、查询和关闭。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 创建仅保存描述符和不透明记录的通用对象壳。
 * @param type 复制及销毁协议，须具有静态或更长生命周期。
 * @param record 成功时转移给壳的宿主记录。
 * @return 新对象句柄，失败返回零且不接管记录。
 */
std::int64_t NativeCallContextImpl::makeNativeRecord(const NativeRecordType &type, void *record)
{
    if (!context_.objects || !record || !type.destroy)
    {
        fail("宿主记录缺少对象表、有效记录或销毁回调");
        return 0;
    }
    context_.objects->recordContext.heap = context_.heap;
    context_.objects->recordContext.data = context_.data;
    context_.objects->recordContext.objects = context_.objects;
    auto object = Memory::makeShared<RuntimeObject>();
    object->kind = ObjectKind::NativeRecord;
    object->nativeType = &type;
    object->nativeRecord = std::shared_ptr<void>(record, type.destroy);
    return context_.objects->add(std::move(object));
}
/**
 * @brief 按描述符身份借用当前运行时中的宿主记录。
 * @param handle 当前对象表的句柄。
 * @param type 预期的描述符。
 * @return 存活且身份匹配的记录，否则返回空。
 */
void *NativeCallContextImpl::nativeRecord(std::int64_t handle, const NativeRecordType &type) const
{
    auto *object = context_.objects && !context_.objects->shuttingDown ? context_.objects->find(handle) : nullptr;
    if (!object || object->kind != ObjectKind::NativeRecord || !object->nativeType) return nullptr;
    const auto *actual = object->nativeType;
    const bool sameIdentity = actual == &type || (actual->identityKey && type.identityKey && type.identityKey[0] && std::strcmp(actual->identityKey, type.identityKey) == 0);
    return sameIdentity ? object->nativeRecord.get() : nullptr;
}
/**
 * @brief 释放当前运行时的通用记录壳并调用记录析构。
 * @param handle 待释放的句柄。
 * @return 接受立即或延期释放时为 true；无效句柄或被拒绝时为 false。
 */
bool NativeCallContextImpl::releaseNativeRecord(std::int64_t handle)
{
    return context_.objects && !context_.objects->shuttingDown && context_.objects->release(handle);
}
/**
 * @brief 通过当前堆释放原始块，并结束其关联的存储借用。
 * @param address 分配块首地址。
 * @param soft 是否允许空地址和数据区成为空操作；失效堆身份仍诊断。
 * @return 释放成功时为 true，失败时保存堆的原始原因。
 */
bool NativeCallContextImpl::releaseMemory(std::int64_t address, bool soft)
{
    const auto target = static_cast<std::size_t>(address);
    if (soft && (target == 0 || !context_.heap || !context_.heap->contains(target))) return true;
    if (!context_.heap) return false;
    std::size_t capacity = 0;
    if (context_.objects && context_.heap->capacityAt(target, capacity)) context_.objects->beginStorage(target, capacity, true);
    Memory::String reason;
    if (context_.heap->release(target, &reason)) return true;
    context_.error = "[CHL1205] free 失败：" + reason;
    return false;
}
/**
 * @brief 在当前对象表执行宿主复制协议。
 * @param handle 待复制句柄。
 * @return 新副本句柄，失败返回零并保留原始诊断。
 */
std::int64_t NativeCallContextImpl::cloneNativeRecord(std::int64_t handle)
{
    return context_.objects ? context_.objects->copyHandle(handle, &context_.error) : 0;
}
/**
 * @brief 读取存活对象的稳定身份。
 * @param handle 对象句柄。
 * @return 身份编号；未找到返回零。
 */
std::int64_t NativeCallContextImpl::objectIdentity(std::int64_t handle) const
{
    const auto *object = context_.objects ? context_.objects->find(handle) : nullptr;
    return object ? object->identity : 0;
}
/**
 * @brief 将临时资源追加到所属运行时的清理队列。
 * @param handle 内存地址或对象句柄。
 * @param object 是否登记对象。
 */
void NativeCallContextImpl::trackTemporary(std::int64_t handle, bool object)
{
    if (!context_.objects || !handle) return;
    if (object)
        context_.objects->materializedObjects.push_back(handle);
    else
        context_.objects->materializedHeap.push_back(static_cast<std::size_t>(handle));
}
/**
 * @brief 从 VM 共用的输入源读取一项文本。
 * @param text 接收读取结果。
 * @return 成功读取时为 true。
 */
bool NativeCallContextImpl::takeInputText(Memory::String &text)
{
    return nextInputText(context_, &text);
}

/**
 * @brief 创建由本次原生调用管理的临时对象。
 * @param bytes 内存区域或数据的字节数。
 * @retval 创建由本次原生调用管理的临时对象。
 */
std::int64_t NativeCallContextImpl::makeTemporary(std::size_t bytes)
{
    if (!context_.heap || !context_.objects)
    {
        fail("temporary storage unavailable");
        return 0;
    }
    const auto address = nativeAllocate(context_, std::max<std::size_t>(8, bytes));
    if (!address)
    {
        fail("ABI temporary allocation failed");
        return 0;
    }
    context_.objects->materializedHeap.push_back(address);
    return static_cast<std::int64_t>(address);
}

/**
 * @brief 句柄是不是"对象表里的句柄"（不是堆地址 / 数据区地址 / 0）。
 * @param handle 待操作的运行时句柄。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
[[nodiscard]] bool isObjectHandle(std::int64_t handle) { return handle >= kObjectHandleBase; }
}  // namespace runtimeDetail
}  // namespace stackvm
