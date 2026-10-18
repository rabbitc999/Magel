#include <cstdint>

/**
 * @brief 向宿主注册表绑定本模块提供的 Magel 胶水接口。
 * @param registry 接收函数绑定的宿主注册表。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error)
{
    magel_glue::DynamicLibrary library;
#if defined(_WIN32)
    const char *path = "./native.dll";
#elif defined(__APPLE__)
    const char *path = "./libnative.dylib";
#else
    const char *path = "./libnative.so";
#endif
    if (!library.open(path, error)) return false;
    return library.bind<std::int32_t(std::int32_t, std::int32_t)>(
               registry, "native_add", "abi_add", error)
           && library.bind<const char *()>(
               registry, "native_name", "abi_name", error);
}
