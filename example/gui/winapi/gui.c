#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#define GUI_EXPORT         __declspec(dllexport)
#define GUI_EVENT_WAKE     (WM_APP + 1)
#define GUI_QUEUE_CAPACITY 256
#define GUI_TEXT_CAPACITY  4096

/* 此简易库只开一个窗口，MAGEL 主线程同时是 WinAPI 窗口线程。 */
static HWND gui_window = NULL;
static int32_t gui_events[GUI_QUEUE_CAPACITY];
static unsigned gui_head = 0;
static unsigned gui_count = 0;
static char gui_text[GUI_TEXT_CAPACITY];
typedef void (*GuiCallback)(int64_t event, int64_t source, void *user);
static GuiCallback gui_callback = NULL;
static void *gui_callback_user = NULL;
static int gui_timer_active = 0;
static int32_t gui_timer_interval = 0;
static ULONGLONG gui_timer_last = 0;
static int64_t gui_timer_ticks = 0;

static void push_event(int32_t event)
{
    if (gui_count < GUI_QUEUE_CAPACITY)
    {
        gui_events[(gui_head + gui_count) % GUI_QUEUE_CAPACITY] = event;
        ++gui_count;
    }
    if (gui_callback != NULL) gui_callback(event, 0, gui_callback_user);
}

static int32_t pop_event(void)
{
    if (gui_count == 0) return 0;
    int32_t result = gui_events[gui_head];
    gui_head = (gui_head + 1) % GUI_QUEUE_CAPACITY;
    --gui_count;
    return result;
}

static WCHAR *wide_copy(const char *utf8)
{
    if (utf8 == NULL) utf8 = "";
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (length <= 0) return NULL;
    WCHAR *result = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)length * sizeof(WCHAR));
    if (result == NULL) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, result, length) <= 0)
    {
        HeapFree(GetProcessHeap(), 0, result);
        return NULL;
    }
    return result;
}

static LRESULT CALLBACK gui_wndproc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED && lparam != 0)
        {
            WCHAR class_name[16];
            if (GetClassNameW((HWND)lparam, class_name, 16) > 0 && lstrcmpiW(class_name, L"Button") == 0)
            {
                int id = LOWORD(wparam);
                if (id > 0)
                {
                    push_event(id);
                    PostMessageW(window, GUI_EVENT_WAKE, 0, 0);
                }
            }
            return 0;
        }
        break;
    case GUI_EVENT_WAKE:
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        gui_window = NULL;
        push_event(-1);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

GUI_EXPORT int32_t mcgui_open(int32_t width, int32_t height, const char *title)
{
    if (gui_window != NULL || width < 100 || height < 100 || width > 4096 || height > 4096) return 0;
    WCHAR *wide_title = wide_copy(title);
    if (wide_title == NULL) return 0;
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW window_class = { 0 };
    window_class.lpfnWndProc = gui_wndproc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = L"MagelSimpleGuiWinApi";
    if (RegisterClassW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        HeapFree(GetProcessHeap(), 0, wide_title);
        return 0;
    }
    RECT bounds = { 0, 0, width, height };
    if (!AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE))
    {
        HeapFree(GetProcessHeap(), 0, wide_title);
        return 0;
    }
    gui_window = CreateWindowExW(0, window_class.lpszClassName, wide_title,
                                 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 bounds.right - bounds.left, bounds.bottom - bounds.top,
                                 NULL, NULL, instance, NULL);
    HeapFree(GetProcessHeap(), 0, wide_title);
    if (gui_window == NULL) return 0;
    gui_head = 0;
    gui_count = 0;
    ShowWindow(gui_window, SW_SHOW);
    UpdateWindow(gui_window);
    return 1;
}

static int32_t add_control(const WCHAR *class_name, DWORD style, DWORD extra_style,
                           int32_t id, int32_t x, int32_t y, int32_t width,
                           int32_t height, const char *text)
{
    if (gui_window == NULL || id < 1 || id > 65535 || GetDlgItem(gui_window, id) != NULL || x < 0 || y < 0 || width < 1 || height < 1) return 0;
    WCHAR *wide_text = wide_copy(text);
    if (wide_text == NULL) return 0;
    if (lstrlenW(wide_text) > 1024)
    {
        HeapFree(GetProcessHeap(), 0, wide_text);
        return 0;
    }
    HWND control = CreateWindowExW(extra_style, class_name, wide_text,
                                   WS_CHILD | WS_VISIBLE | style, x, y, width, height,
                                   gui_window, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    HeapFree(GetProcessHeap(), 0, wide_text);
    if (control == NULL) return 0;
    if (class_name[0] == L'E') SendMessageW(control, EM_LIMITTEXT, 1024, 0);
    return 1;
}

GUI_EXPORT int32_t mcgui_label(int32_t id, int32_t x, int32_t y, int32_t width,
                               int32_t height, const char *text)
{
    return add_control(L"STATIC", SS_LEFT, 0, id, x, y, width, height, text);
}
GUI_EXPORT int32_t mcgui_button(int32_t id, int32_t x, int32_t y, int32_t width,
                                int32_t height, const char *text)
{
    return add_control(L"BUTTON", WS_TABSTOP | BS_PUSHBUTTON, 0,
                       id, x, y, width, height, text);
}
GUI_EXPORT int32_t mcgui_text_box(int32_t id, int32_t x, int32_t y, int32_t width,
                                  int32_t height, const char *text)
{
    return add_control(L"EDIT", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                       id, x, y, width, height, text);
}

GUI_EXPORT int32_t mcgui_set_text(int32_t id, const char *text)
{
    if (gui_window == NULL) return 0;
    HWND control = GetDlgItem(gui_window, id);
    if (control == NULL) return 0;
    WCHAR *wide_text = wide_copy(text);
    if (wide_text == NULL) return 0;
    if (lstrlenW(wide_text) > 1024)
    {
        HeapFree(GetProcessHeap(), 0, wide_text);
        return 0;
    }
    int32_t ok = SetWindowTextW(control, wide_text) ? 1 : 0;
    HeapFree(GetProcessHeap(), 0, wide_text);
    return ok;
}

GUI_EXPORT const char *mcgui_get_text(int32_t id)
{
    gui_text[0] = '\0';
    if (gui_window == NULL) return gui_text;
    HWND control = GetDlgItem(gui_window, id);
    if (control == NULL) return gui_text;
    int length = GetWindowTextLengthW(control);
    if (length < 0 || length > 1024) return gui_text;
    WCHAR wide_text[1025];
    if (GetWindowTextW(control, wide_text, 1025) < 0) return gui_text;
    if (WideCharToMultiByte(CP_UTF8, 0, wide_text, -1,
                            gui_text, GUI_TEXT_CAPACITY, NULL, NULL)
        <= 0)
        gui_text[0] = '\0';
    return gui_text;
}

GUI_EXPORT void mcgui_pump(void);

GUI_EXPORT int32_t mcgui_poll_event(void)
{
    mcgui_pump();
    if (gui_count > 0) return pop_event();
    return gui_window == NULL ? -1 : 0;
}

/**
 * @brief 处理一轮窗口消息与周期回调，不阻塞调用线程。
 */
GUI_EXPORT void mcgui_pump(void)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            if (gui_window != NULL)
                DestroyWindow(gui_window);
            else
                push_event(-1);
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (gui_timer_active && gui_callback != NULL)
    {
        ULONGLONG now = GetTickCount64();
        if (gui_timer_interval == 0 || now - gui_timer_last >= (ULONGLONG)gui_timer_interval)
        {
            gui_timer_last = now;
            gui_callback(0, ++gui_timer_ticks, gui_callback_user);
        }
    }
}
/**
 * @brief 等待有限时间片，确保 VM 有机会交付中断。
 * @return 事件值或 0。
 */
GUI_EXPORT int32_t mcgui_wait_event(void)
{
    mcgui_pump();
    if (gui_count > 0) return pop_event();
    if (gui_window == NULL) return -1;
    MsgWaitForMultipleObjects(0, NULL, FALSE, 2, QS_ALLINPUT);
    mcgui_pump();
    return gui_count > 0 ? pop_event() : (gui_window == NULL ? -1 : 0);
}
/**
 * @brief 注入事件。
 * @param event 事件值。
 * @return 成功返回 1。
 */
GUI_EXPORT int32_t mcgui_post_event(int32_t event)
{
    push_event(event);
    return 1;
}
/**
 * @brief 设置周期回调。
 * @param interval 毫秒间隔，0 表示每轮一次，负数忽略。
 */
GUI_EXPORT void mcgui_start_timer(int32_t interval)
{
    if (interval < 0) return;
    gui_timer_interval = interval;
    gui_timer_last = GetTickCount64();
    gui_timer_ticks = 0;
    gui_timer_active = 1;
}
/**
 * @brief 停止周期回调。
 */
GUI_EXPORT void mcgui_stop_timer(void) { gui_timer_active = 0; }
/**
 * @brief 登记事件回调。
 * @param callback 可空回调。
 * @param user 回调上下文。
 */
GUI_EXPORT void mcgui_set_callback(GuiCallback callback, void *user)
{
    gui_callback = callback;
    gui_callback_user = user;
}

GUI_EXPORT void mcgui_close(void)
{
    if (gui_window != NULL) DestroyWindow(gui_window);
}
