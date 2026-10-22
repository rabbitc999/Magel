#ifndef CHALI_SCALAR_BUFFER_HPP
#define CHALI_SCALAR_BUFFER_HPP
#include "../chali_glue.hpp"
#include "std_native_record.hpp"

/**
 * @brief 线性代数数值缓冲的胶水存储，不包含语言对象或 VM 专用布局。
 */
namespace scalar_buffer
{
using Context = chali_glue::Context;
using Value = chali_glue::Value;

/**
 * @brief 保存整数或浮点标量的连续缓冲。
 */
struct Storage
{ Memory::Vector<Value> values; };
/**
 * @brief 深复制标量缓冲。
 * @param pointer 源存储。
 * @param error 失败时的诊断输出。
 * @return 独立的存储副本。
 */
void *clone(const void *pointer, Memory::String &error);
/**
 * @brief 销毁标量缓冲。
 * @param pointer 待销毁的存储。
 */
void destroy(void *pointer);
extern const stackvm::NativeRecordType type;
/**
 * @brief 校验并读取标量 opcode 记录。
 * @param ctx 当前调用上下文。
 * @param handle 标量记录句柄。
 * @return 有效存储；失败时报告错误并返回空。
 */
Storage *get(Context &ctx, std::int64_t handle);
/**
 * @brief 从宿主存储复制指定精度的数值序列。
 * @tparam Scalar 内建数值类型。
 * @param ctx 当前调用上下文。
 * @param handle 标量记录句柄。
 * @return 独立数值序列。
 */
template <class Scalar>
Memory::Vector<Scalar> read(Context &ctx, std::int64_t handle)
{
    Memory::Vector<Scalar> values;
    if (!handle) return values;
    auto *storage = get(ctx, handle);
    if (!storage) return values;
    values.reserve(storage->values.size());
    for (const auto &value : storage->values) values.push_back(chali_glue::Codec<Scalar>::decode(ctx, value));
    return values;
}
/**
 * @brief 将宿主数值序列复制到新的不透明存储。
 * @tparam Values 提供标准顺序访问的数值容器。
 * @param ctx 当前调用上下文。
 * @param values 源数值序列。
 * @return 新记录句柄。
 */
template <class Values>
std::int64_t make(Context &ctx, const Values &values)
{
    auto *storage = new Storage;
    storage->values.reserve(values.size());
    for (const auto &value : values) storage->values.push_back(chali_glue::Codec<typename Values::value_type>::encode(ctx, value));
    return ctx.makeNativeRecord(type, storage);
}
/**
 * @brief 注册一种标量的读取、替换与追加操作。
 * @tparam Scalar 内建数值类型。
 * @param registry 宿主函数表。
 * @param name 语言中的标量类型名。
 * @param error 注册失败时接收诊断。
 * @return 三项操作均注册成功时返回 true。
 */
template <class Scalar>
bool bindScalar(chali_glue::Registry &registry, const Memory::String &name, Memory::String *error)
{
    bool ok = registry.bindTypedWithContext<Scalar, std::int64_t, std::int64_t>(chali_glue::specialize("std::linalg::__std_scalarAt", { name }), [](Context &ctx, std::int64_t handle, std::int64_t index) {
        auto *storage = get(ctx, handle);
        if (!storage) return Scalar{};
        if (index < 0 || static_cast<std::uint64_t>(index) >= storage->values.size()) { ctx.fail("线性代数存储下标越界"); return Scalar{}; }
        return chali_glue::Codec<Scalar>::decode(ctx, storage->values[static_cast<std::size_t>(index)]); }, error);
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, Scalar>(chali_glue::specialize("std::linalg::__std_scalarSet", { name }), [](Context &ctx, std::int64_t reference, std::int64_t index, Scalar value) {
        auto *storage = get(ctx, native_record_glue::referenceHandle(ctx, reference, type)); if (!storage) return;
        if (index < 0 || static_cast<std::uint64_t>(index) >= storage->values.size()) { ctx.fail("线性代数存储下标越界"); return; }
        storage->values[static_cast<std::size_t>(index)] = chali_glue::Codec<Scalar>::encode(ctx, value); }, error) && ok;
    return registry.bindTypedWithContext<void, std::int64_t, Scalar>(chali_glue::specialize("std::linalg::__std_scalarPush", { name }), [](Context &ctx, std::int64_t reference, Scalar value) { auto *storage = get(ctx, native_record_glue::referenceHandle(ctx, reference, type)); if (storage) storage->values.push_back(chali_glue::Codec<Scalar>::encode(ctx, value)); }, error) && ok;
}
/**
 * @brief 注册数值缓冲生命周期与全部内建数值类型的访问接口。
 * @param registry 宿主函数表。
 * @param error 注册失败时接收诊断。
 * @return 所有注册均成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace scalar_buffer
#endif
