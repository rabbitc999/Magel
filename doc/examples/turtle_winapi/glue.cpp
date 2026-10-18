#include <cstdint>

/* magel-emit --glue 会把这个文件原样编入导出的 C++ 程序。 */
/**
 * @brief 向宿主注册表绑定本模块提供的 Magel 胶水接口。
 * @param registry 接收函数绑定的宿主注册表。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error)
{
    magel_glue::DynamicLibrary library;
    if (!library.open("./turtle.dll", error)) return false;
    return library.bind<std::int32_t(std::int32_t, std::int32_t)>(
               registry, "turtle_open", "turtle_open", error)
           && library.bind<void(double)>(
               registry, "turtle_forward", "turtle_forward", error)
           && library.bind<void(double)>(
               registry, "turtle_left", "turtle_left", error)
           && library.bind<void(std::int32_t)>(
               registry, "turtle_pen", "turtle_pen", error)
           && library.bind<void(std::uint32_t)>(
               registry, "turtle_color", "turtle_color", error)
           && library.bind<void()>(
               registry, "turtle_clear", "turtle_clear", error)
           && library.bind<std::int32_t(const char *)>(
               registry, "turtle_save_bmp", "turtle_save_bmp", error)
           && library.bind<void()>(
               registry, "turtle_wait", "turtle_wait", error)
           && library.bind<void()>(
               registry, "turtle_close", "turtle_close", error);
}
