#include "magel_glue.hpp"

#include <cstdint>

using IntegerFunction = magel_glue::Function<std::int64_t(std::int64_t)>;
using IntegerReference = magel_glue::Reference<std::int64_t>;

/**
 * @brief 为 VM 与 AOT 注册同步调用 Magel 函数的相同胶水实现。
 * @param registry 接收宿主函数的注册表，须覆盖程序的运行期。
 * @param error 绑定失败时接收诊断，可为空。
 * @return 所有入口绑定成功时为 true。
 * @note 函数包装仅在本次胶水调用期间有效，不缓存上下文或闭包。
 */
bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTypedWithContext<std::int64_t, IntegerFunction, std::int64_t>(
               "hostApply", [](magel_glue::Context &, const IntegerFunction &function, std::int64_t value) {
                   magel_glue::Value result;
                   if (!function.tryInvoke(result, value)) return std::int64_t{};
                   return result.asInt();
               },
               error)
           && registry.bindTyped<float, magel_glue::Function<float(float)>, float>("hostFloat", [](const magel_glue::Function<float(float)> &function, float value) { return function(value); }, error) && registry.bindTyped<bool, magel_glue::Function<bool(bool)>, bool>("hostBool", [](const magel_glue::Function<bool(bool)> &function, bool value) { return function(value); }, error) && registry.bindTyped<void, magel_glue::Function<void(std::int64_t)>, std::int64_t>("hostNotify", [](const magel_glue::Function<void(std::int64_t)> &function, std::int64_t value) { function(value); }, error) && registry.bindTyped<void, magel_glue::Function<void(IntegerReference)>, IntegerReference>("hostUpdate", [](const magel_glue::Function<void(IntegerReference)> &function, IntegerReference value) { function(value); }, error) && registry.bindTyped<std::int64_t, magel_glue::Function<std::int64_t(IntegerFunction, std::int64_t)>, IntegerFunction, std::int64_t>("hostCompose", [](const magel_glue::Function<std::int64_t(IntegerFunction, std::int64_t)> &outer, const IntegerFunction &inner, std::int64_t value) { return outer(inner, value); }, error);
}
