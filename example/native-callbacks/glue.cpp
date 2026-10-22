#include "chali_glue.hpp"

#include <cstdint>

using IntegerFunction = chali_glue::Function<std::int64_t(std::int64_t)>;
using IntegerReference = chali_glue::Reference<std::int64_t>;

/**
 * @brief 为 VM 注册同步调用 ChaLi 函数的相同胶水实现。
 * @param registry 接收宿主函数的注册表，须覆盖程序的运行期。
 * @param error 绑定失败时接收诊断，可为空。
 * @return 所有入口绑定成功时为 true。
 * @note 函数包装仅在本次胶水调用期间有效，不缓存上下文或闭包。
 */
bool bindChaLiGlue(chali_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTypedWithContext<std::int64_t, IntegerFunction, std::int64_t>(
               "hostApply", [](chali_glue::Context &, const IntegerFunction &function, std::int64_t value) {
                   chali_glue::Value result;
                   if (!function.tryInvoke(result, value)) return std::int64_t{};
                   return result.asInt();
               },
               error)
           && registry.bindTyped<float, chali_glue::Function<float(float)>, float>("hostFloat", [](const chali_glue::Function<float(float)> &function, float value) { return function(value); }, error) && registry.bindTyped<bool, chali_glue::Function<bool(bool)>, bool>("hostBool", [](const chali_glue::Function<bool(bool)> &function, bool value) { return function(value); }, error) && registry.bindTyped<void, chali_glue::Function<void(std::int64_t)>, std::int64_t>("hostNotify", [](const chali_glue::Function<void(std::int64_t)> &function, std::int64_t value) { function(value); }, error) && registry.bindTyped<void, chali_glue::Function<void(IntegerReference)>, IntegerReference>("hostUpdate", [](const chali_glue::Function<void(IntegerReference)> &function, IntegerReference value) { function(value); }, error) && registry.bindTyped<std::int64_t, chali_glue::Function<std::int64_t(IntegerFunction, std::int64_t)>, IntegerFunction, std::int64_t>("hostCompose", [](const chali_glue::Function<std::int64_t(IntegerFunction, std::int64_t)> &outer, const IntegerFunction &inner, std::int64_t value) { return outer(inner, value); }, error);
}
