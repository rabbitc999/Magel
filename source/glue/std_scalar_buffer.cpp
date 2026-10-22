#include "std_scalar_buffer.hpp"

namespace scalar_buffer
{

/**
 * @brief 校验标量记录句柄并取得当前操作的宿主存储。
 * @param ctx 当前调用上下文。
 * @param handle 标量记录句柄。
 * @return 有效存储；失效或类型错误时报告诊断并返回空。
 */
Storage *get(Context &ctx, std::int64_t handle)
{
    auto *storage = static_cast<Storage *>(ctx.nativeRecord(handle, type));
    if (!storage) ctx.fail("线性代数存储 opcode 已释放或类型不匹配");
    return storage;
}

const stackvm::NativeRecordType type{ "std::linalg::ScalarStorage", clone, destroy };

/**
 * @brief 深复制标量缓冲。
 * @param pointer 源存储。
 * @param error 失败时的诊断输出。
 * @return 独立的存储副本。
 */
void *clone(const void *pointer, Memory::String &error)
{
    (void)error;
    return new Storage(*static_cast<const Storage *>(pointer));
}

/**
 * @brief 销毁标量缓冲。
 * @param pointer 待销毁的存储。
 */
void destroy(void *pointer)
{ delete static_cast<Storage *>(pointer); }

/**
 * @brief 注册数值缓冲生命周期与全部内建数值类型的访问接口。
 * @param registry 宿主函数表。
 * @param error 注册失败时接收诊断。
 * @return 所有注册均成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    bool ok = registry.bindTypedWithContext<std::int64_t>("std::linalg::__std_scalarNew", [](Context &ctx) { return ctx.makeNativeRecord(type, new Storage); }, error);
    ok = registry.bindTypedWithContext<void, std::int64_t>("std::linalg::__std_scalarDrop", [](Context &ctx, std::int64_t handle) { ctx.releaseNativeRecord(handle); }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::linalg::__std_scalarCopy", [](Context &ctx, std::int64_t handle) { if (!handle) return ctx.makeNativeRecord(type, new Storage); auto *source = get(ctx, handle); return source ? ctx.makeNativeRecord(type, new Storage(*source)) : 0; }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::linalg::__std_scalarLen", [](Context &ctx, std::int64_t handle) { auto *storage = get(ctx, handle); return storage ? static_cast<std::int64_t>(storage->values.size()) : 0; }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t>("std::linalg::__std_scalarClear", [](Context &ctx, std::int64_t reference) { auto *storage = get(ctx, native_record_glue::referenceHandle(ctx, reference, type)); if (storage) storage->values.clear(); }, error) && ok;
    ok = bindScalar<float>(registry, "float32", error) && ok;
    ok = bindScalar<double>(registry, "float64", error) && ok;
    ok = bindScalar<std::int8_t>(registry, "int8", error) && ok;
    ok = bindScalar<std::int16_t>(registry, "int16", error) && ok;
    ok = bindScalar<std::int32_t>(registry, "int32", error) && ok;
    ok = bindScalar<std::int64_t>(registry, "int64", error) && ok;
    ok = bindScalar<std::uint8_t>(registry, "uint8", error) && ok;
    ok = bindScalar<std::uint16_t>(registry, "uint16", error) && ok;
    ok = bindScalar<std::uint32_t>(registry, "uint32", error) && ok;
    ok = bindScalar<std::uint64_t>(registry, "uint64", error) && ok;
    return ok;
}

}  // namespace scalar_buffer
