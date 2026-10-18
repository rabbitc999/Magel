/*
 * gui/glue.cpp —— 把 MAGEL 的 `gui::` 宿主声明绑到 GUI 共享库（C ABI）。
 *
 * `magel aot --glue` 把这个文件编进应用；共享库句柄保留在绑定闭包与指令钩子里，
 * 进程存活期间不会卸载。MAGEL 与库之间只传定宽整数和 UTF-8 字符串。
 *
 * 事件回调那条链：GUI 事件 → `mcgui_callback_t` 桥（本文件）→
 * 运行时投递器 → VM / AOT 在下一个执行边界交付 MAGEL 中断
 * 处理函数。桥只缓存运行时投递器与定时器句柄，不缓存 `NativeCallContext`，所以
 * 指令钩子（GUI 泵）在宿主回调之外触发也安全。
 */
#include <cstdint>
#include <cstdlib>
#include <string>

namespace
{

/**
 * 与 gui/qt/gui_qt.h 的 `mcgui_callback_t` 一致。
 */
using GuiCallback = void (*)(std::int64_t event, std::int64_t source, void *user);

/**
 * 当前登记的事件中断源；只在 VM 执行线程上读写。
 */
std::int64_t g_eventTimer = 0;
Memory::Function<void(std::int64_t, std::int64_t)> g_eventPost;

/**
 * @brief GUI 事件 → VM 中断：交给 MAGEL 的是**事件值**。
 *
 * 约定：按钮点击 = 控件 ID（>0），关窗 = -1，GUI 周期事件 = 0。周期事件的 `source`
 * 是第几次，C ABI 里有；MAGEL 处理函数只看事件值，所以这里原样传 `event`。
 */
void guiEventBridge(std::int64_t event, std::int64_t, void *)
{
    if (!g_eventPost || g_eventTimer == 0) return;
    g_eventPost(g_eventTimer, event);
}

}  // namespace

/* magel aot --glue 将这个文件编入应用；共享库保留在所有绑定闭包中。 */
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
    const char *defaultPath = "./mcgui.dll";
#else
    const char *defaultPath = "./libmcguiqt.so";
#endif
    const char *overridePath = std::getenv("MAGEL_GUI_LIBRARY");
    const Memory::String path = overridePath && *overridePath ? overridePath : defaultPath;
    if (!library.open(path, error)) return false;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using Context = magel_glue::Context;

    /* 事件 API 的原始符号：用法比 `bind` 灵活（有的要 Context，有的要函数指针）。 */
    auto *pump = reinterpret_cast<void (*)()>(library.resolve("mcgui_pump", error));
    auto *setCallback = reinterpret_cast<void (*)(GuiCallback, void *)>(
        library.resolve("mcgui_set_callback", error));
    if (pump == nullptr || setCallback == nullptr) return false;

    if (!library.bind<i32(i32, i32, const char *)>(
            registry, "gui::open", "mcgui_open", error)
        || !library.bind<i32(i32, i32, i32, i32, i32, const char *)>(
            registry, "gui::label", "mcgui_label", error)
        || !library.bind<i32(i32, i32, i32, i32, i32, const char *)>(
            registry, "gui::button", "mcgui_button", error)
        || !library.bind<i32(i32, i32, i32, i32, i32, const char *)>(
            registry, "gui::textBox", "mcgui_text_box", error)
        || !library.bind<i32(i32, const char *)>(
            registry, "gui::setText", "mcgui_set_text", error)
        || !library.bind<const char *(i32)>(
            registry, "gui::getText", "mcgui_get_text", error)
        || !library.bind<i32()>(
            registry, "gui::pollEvent", "mcgui_poll_event", error)
        || !library.bind<i32()>(
            registry, "gui::waitEvent", "mcgui_wait_event", error)
        || !library.bind<void()>(
            registry, "gui::close", "mcgui_close", error)
        || !library.bind<i32(i32)>(
            registry, "gui::postEvent", "mcgui_post_event", error)
        || !library.bind<void(i32)>(
            registry, "gui::startEventTimer", "mcgui_start_timer", error)
        || !library.bind<void()>(
            registry, "gui::stopEventTimer", "mcgui_stop_timer", error))
        return false;

    /* 登记 / 注销事件中断源。 */
    if (!registry.bindTypedWithContext<i32, i64>(
            "gui::bindEvents",
            [setCallback](Context &context, i64 timer) {
                g_eventTimer = timer;
                g_eventPost = context.interruptPoster();
                if (!g_eventPost)
                {
                    g_eventTimer = 0;
                    return 0;
                }
                setCallback(&guiEventBridge, nullptr);
                return 1;
            },
            error))
        return false;
    if (!registry.bindTypedWithContext<i32>(
            "gui::unbindEvents",
            [setCallback](Context &) {
                setCallback(nullptr, nullptr);
                g_eventTimer = 0;
                g_eventPost = {};
                return 1;
            },
            error))
        return false;

    /* 非阻塞 GUI 泵。 */
    if (!registry.bindTypedWithContext<i32>(
            "gui::pump",
            [pump](Context &) { pump(); return 1; },
            error))
        return false;

    /*
     * 每 N 个执行边界泵一次 GUI：指令钩子在 VM 执行线程、执行边界同步触发（VM 指令 / AOT 基本块），所以这里能直接
     * 通过统一运行时接口安装钩子；钩子里派发的 GUI 事件再经 `guiEventBridge` 投递中断。
     */
    if (!registry.bindTypedWithContext<i32, i64>(
            "gui::pumpEvery",
            [pump](Context &context, i64 instructions) {
                if (instructions <= 0)
                {
                    return context.setInstructionHook(0, stackvm::InstructionHook{}) ? 1 : 0;
                }
                return context.setInstructionHook(
                           static_cast<std::size_t>(instructions),
                           stackvm::InstructionHook(
                               [pump](const stackvm::InstructionHookInfo &) { pump(); })) ?
                           1 :
                           0;
            },
            error))
        return false;

    return true;
}
