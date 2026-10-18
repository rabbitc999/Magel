#include <cstdint>

/**
 * @brief 绑定函数类型实参的泛型宿主实例，验证复杂类型符号与前端一致。
 * @param registry 宿主注册表。
 * @param error 失败时接收诊断。
 * @return 注册成功返回 true。
 */
bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error)
{
    using Operation = magel_glue::Function<std::int64_t(std::int64_t)>;
    return registry.bindTypedGeneric<Operation, Operation>("auditHost", { "int64 fn(int64)" }, [](Operation operation) { return operation; }, error);
}
