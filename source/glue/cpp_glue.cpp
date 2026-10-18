/*
 * 演示用的应用胶水（`tests/magel_emit.py` 与样例拿它当"宿主类型 + 绑定"的样板）。
 *
 * **手写维护的普通 C++**：原来是 `.mcpp` 声明式 DSL（2026-09-29 删除），这里是它的
 * 等价产物。要点：名义宿主类型用 `magel_glue::Handle<Tag, Abi>` 声明，复杂宿主对象
 * 一律以整数句柄过边界；绑定走 `registry.bindTyped<...>`。
 */
#include <cstdint>
#include <string>

struct ExportCounterTag;
using ExportCounter = magel_glue::Handle<ExportCounterTag, std::uint64_t>;

/**
 * @brief 向宿主注册表绑定本模块提供的 Magel 胶水接口。
 * @param registry 接收函数绑定的宿主注册表。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTyped<ExportCounter, std::uint64_t>(
               "demo_counter_from_uint", [](std::uint64_t arg0) { return ExportCounter{ arg0 }; }, error)
           && registry.bindTyped<std::uint64_t, ExportCounter>(
               "demo_counter_to_uint", [](ExportCounter arg0) { return arg0.value; }, error)
           && registry.bindTyped<ExportCounter, ExportCounter, ExportCounter>(
               "demo_cpp_add", [](ExportCounter arg0, ExportCounter arg1) { return ExportCounter{ arg0.value + arg1.value }; }, error)
           && registry.bindTyped<std::int32_t, Memory::String>(
               "demo_cpp_text_size", [](Memory::String arg0) { return static_cast<std::int32_t>(arg0.size()); }, error)
           && registry.bindTyped<std::int32_t, std::int32_t>(
               "demo_cpp_identity__int32", [](std::int32_t arg0) { return arg0; }, error);
}
