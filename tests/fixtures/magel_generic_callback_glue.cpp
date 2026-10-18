#include <cstdint>

/**
 * @brief 测试枚举的名义标签，保持与 MAGEL 的 Signal 布局一致。
 */
struct GenericSignalTag;
using GenericSignal = magel_glue::Enum<GenericSignalTag>;

/**
 * @brief 注册推导得到的泛型宿主特化以及回调入口。
 * @param registry 宿主注册表。
 * @param error 失败时接收诊断。
 * @return 所有绑定成功时返回 true。
 */
bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTyped<std::int64_t, magel_glue::Function<std::int64_t(std::int64_t)>, std::int64_t>(
               "demo_cpp_apply", [](const magel_glue::Function<std::int64_t(std::int64_t)> &function, std::int64_t value) { return function(value); }, error)
           && registry.bindTypedGeneric<GenericSignal, GenericSignal>(
               "demo_cpp_identity", { "Signal" }, [](GenericSignal value) { return value; }, error)
           && registry.bindTypedGeneric<std::int32_t, GenericSignal>(
               "demo_cpp_enum_tag", { "Signal" }, [](GenericSignal value) { return value.tag(); }, error)
           && registry.bindTypedGeneric<std::int64_t, GenericSignal>(
               "demo_cpp_enum_payload", { "Signal" },
               [](GenericSignal value) { return value.payload<std::int64_t>(1u); }, error);
}
