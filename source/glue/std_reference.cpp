#include "std_reference.hpp"
#include "std_native_record.hpp"
#include "../chali_glue.hpp"
#include <memory>
namespace std_reference_glue
{
namespace
{
/**
 * @brief 复制非拥有共享槽引用，保持同一线程组内的槽标识。
 * @param source 原槽标识。
 * @param error 本操作不产生可恢复诊断。
 * @return 新分配的引用记录。
 */
void *cloneReference(const void *source, Memory::String &error)
{
    (void)error;
    return new std::int64_t(*static_cast<const std::int64_t *>(source));
}
/**
 * @brief 释放引用记录，槽及槽内值仍由线程组拥有。
 * @param value 待释放的引用记录。
 */
void destroyReference(void *value) { delete static_cast<std::int64_t *>(value); }
const stackvm::NativeRecordType referenceType{ "std::concurrency::SharedReference", cloneReference, destroyReference, nullptr };
/**
 * @brief 创建可随普通拥有值传输的非拥有槽引用。
 * @param ctx 当前调用上下文。
 * @param identity 线程组拥有的共享槽标识。
 * @return 引用记录句柄。
 */
std::int64_t create(chali_glue::Context &ctx, std::int64_t identity)
{
    auto value = std::make_unique<std::int64_t>(identity);
    auto handle = ctx.makeNativeRecord(referenceType, value.get());
    if (handle) value.release();
    return handle;
}
}  // namespace
/**
 * @brief 注册共享槽引用的轻量生命周期和标识访问协议。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有入口绑定成功时返回 true。
 * @note 引用的复制不复制或销毁线程组拥有的槽内值。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTyped<std::int64_t>("std::event::__emptySource", [] { return std::int64_t{ 0 }; }, error) && registry.bindTypedWithContext<std::int64_t>("std::concurrency::__referenceNew", [](chali_glue::Context &ctx) { return create(ctx, 0); }, error) && native_record_glue::bind(registry.native(), "std::concurrency::__referenceCopy", 1, native_record_glue::copy, error) && native_record_glue::bind(registry.native(), "std::concurrency::__referenceDrop", 1, native_record_glue::drop, error) && registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::concurrency::__referenceFrom", create, error) && registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::concurrency::__referenceId", [](chali_glue::Context &ctx, std::int64_t handle) {
            auto *value = static_cast<const std::int64_t *>(ctx.nativeRecord(handle, referenceType)); return value ? *value : 0; }, error);
}
}  // namespace std_reference_glue
