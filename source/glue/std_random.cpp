#include "std_random.hpp"
#include "std_native_record.hpp"
#include "../chali_glue.hpp"
#include <cstdint>
#include <memory>
#include <random>

namespace std_random
{
namespace
{
/**
 * @brief 独占当前随机序列位置的可复制状态。
 */
struct RandomState
{ std::uint64_t value = 1; };
/**
 * @brief 按当前序列位置创建独立随机状态。
 * @param source 原随机状态。
 * @param error 本操作没有可恢复诊断。
 * @return 不共享后续序列变化的新状态。
 */
void *cloneRandom(const void *source, Memory::String &error)
{
    (void)error;
    return new RandomState(*static_cast<const RandomState *>(source));
}
/**
 * @brief 释放拥有的随机状态。
 * @param value 待释放的状态。
 */
void destroyRandom(void *value) { delete static_cast<RandomState *>(value); }
const stackvm::NativeRecordType randomType{ "std::random::Random", cloneRandom, destroyRandom, nullptr };
/**
 * @brief 将指定种子的独立随机状态交给 VM 管理。
 * @param context 当前运行时上下文。
 * @param seed 初始序列种子。
 * @return 新状态的原生记录句柄。
 */
std::uint64_t createRandom(chali_glue::Context &context, std::uint64_t seed)
{
    auto value = std::make_unique<RandomState>();
    value->value = seed;
    const auto handle = context.makeNativeRecord(randomType, value.get());
    if (handle) value.release();
    return static_cast<std::uint64_t>(handle);
}
/**
 * @brief 从当前熵源创建拥有独立状态的生成器。
 * @param context 当前运行时上下文。
 * @return 新生成器的记录句柄。
 */
std::uint64_t entropyRandom(chali_glue::Context &context)
{
    std::random_device source;
    return createRandom(context, (static_cast<std::uint64_t>(source()) << 32u) ^ source());
}
/**
 * @brief 校验随机状态并推进确定性 LCG 一次。
 * @param context 当前运行时上下文。
 * @param handle 仍存活的随机状态句柄。
 * @return 下一项无符号整数；无效状态报告运行时错误。
 */
std::uint64_t nextRandom(chali_glue::Context &context, std::uint64_t handle)
{
    auto *value = static_cast<RandomState *>(context.nativeRecord(static_cast<std::int64_t>(handle), randomType));
    if (!value)
    {
        context.fail("Random 状态已经释放或类型无效");
        return 0;
    }
    value->value = value->value * 6364136223846793005ull + 1442695040888963407ull;
    return value->value;
}
}  // namespace
/**
 * @brief 注册确定性随机数、熵源和原生状态生命周期。
 * @param registry 当前宿主的函数注册表。
 * @param error 接收注册失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    bool ok = registry.bindTypedWithContext<std::uint64_t, std::uint64_t>("std::random::randomWithSeed", createRandom, error)
              && registry.bindTypedWithContext<std::uint64_t>("std::random::randomFromEntropy", entropyRandom, error)
              && registry.bindTypedWithContext<std::uint64_t, std::uint64_t>("std::random::randomNextUInt64", nextRandom, error)
              && registry.bindTypedWithContext<std::int64_t, std::uint64_t, std::int64_t, std::int64_t>("std::random::randomNextInt", [](chali_glue::Context &context, std::uint64_t handle, std::int64_t minimum, std::int64_t maximum) {
                if (maximum <= minimum) { context.fail("random.nextInt 要求 maximum > minimum"); return std::int64_t(0); }
                const auto width = static_cast<std::uint64_t>(maximum) - static_cast<std::uint64_t>(minimum);
                return static_cast<std::int64_t>(static_cast<std::uint64_t>(minimum) + nextRandom(context, handle) % width); }, error) && registry.bindTypedWithContext<double, std::uint64_t>("std::random::randomNextFloat64", [](chali_glue::Context &context, std::uint64_t handle) { return static_cast<double>(nextRandom(context, handle) >> 11u) / 9007199254740992.0; }, error) && registry.bindTypedWithContext<double, std::uint64_t, double, double>("std::random::randomNextRange", [](chali_glue::Context &context, std::uint64_t handle, double minimum, double maximum) {
                if (!(maximum > minimum)) { context.fail("random.nextRange 要求 maximum > minimum"); return 0.0; }
                return minimum + static_cast<double>(nextRandom(context, handle) >> 11u) / 9007199254740992.0 * (maximum - minimum); }, error) && registry.bindTypedWithContext<bool, std::uint64_t>("std::random::randomNextBool", [](chali_glue::Context &context, std::uint64_t handle) { return (nextRandom(context, handle) >> 63u) != 0; }, error);
    ok = native_record_glue::bind(registry.native(), "std::random::__randomCopy", 1, native_record_glue::copy, error) && ok;
    ok = native_record_glue::bind(registry.native(), "std::random::randomDrop", 1, native_record_glue::drop, error) && ok;
    return ok;
}
}  // namespace std_random
