#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <math.h>

#define TURTLE_EXPORT  __declspec(dllexport)
#define TURTLE_FORWARD (WM_APP + 1)
#define TURTLE_TURN    (WM_APP + 2)
#define TURTLE_PEN     (WM_APP + 3)
#define TURTLE_COLOR   (WM_APP + 4)
#define TURTLE_CLEAR   (WM_APP + 5)
#define TURTLE_SAVE    (WM_APP + 6)

typedef struct TurtleState
{
    int width;
    int height;
    double x;
    double y;
    double heading;
    int pen_down;
    uint32_t color;
    HDC canvas;
    HBITMAP bitmap;
    HGDIOBJ old_bitmap;
    void *pixels;
} TurtleState;

typedef struct OpenRequest
{
    int width;
    int height;
    HANDLE ready;
} OpenRequest;

/* 这个教学示例一次只允许一个窗口；所有绘图命令由窗口线程串行执行。 */
static PVOID volatile turtle_window = NULL;
static HANDLE turtle_thread = NULL;

static HWND current_window(void)
{
    return (HWND)InterlockedCompareExchangePointer(&turtle_window, NULL, NULL);
}

static void set_window(HWND window)
{
    InterlockedExchangePointer(&turtle_window, window);
}

static COLORREF win_color(uint32_t rgb)
{
    return RGB((rgb >> 16) & 255u, (rgb >> 8) & 255u, rgb & 255u);
}

static void clear_canvas(TurtleState *state)
{
    PatBlt(state->canvas, 0, 0, state->width, state->height, WHITENESS);
    state->x = state->width / 2.0;
    state->y = state->height / 2.0;
    state->heading = 0.0;
}

static int save_bitmap(const TurtleState *state, const char *utf8_path)
{
    if (utf8_path == NULL || utf8_path[0] == '\0') return 0;
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                     utf8_path, -1, NULL, 0);
    if (length <= 0) return 0;
    WCHAR *path = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)length * sizeof(WCHAR));
    if (path == NULL) return 0;
    int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                        utf8_path, -1, path, length);
    if (converted <= 0)
    {
        HeapFree(GetProcessHeap(), 0, path);
        return 0;
    }
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    HeapFree(GetProcessHeap(), 0, path);
    if (file == INVALID_HANDLE_VALUE) return 0;

    DWORD bytes = (DWORD)state->width * (DWORD)state->height * 4u;
    BITMAPFILEHEADER header = { 0 };
    BITMAPINFOHEADER info = { 0 };
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(info);
    header.bfSize = header.bfOffBits + bytes;
    info.biSize = sizeof(info);
    info.biWidth = state->width;
    info.biHeight = -state->height; /* DIB 是从上到下的行顺序。 */
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    info.biSizeImage = bytes;
    GdiFlush();
    DWORD written = 0;
    int ok = WriteFile(file, &header, sizeof(header), &written, NULL) && written == sizeof(header) && WriteFile(file, &info, sizeof(info), &written, NULL) && written == sizeof(info) && WriteFile(file, state->pixels, bytes, &written, NULL) && written == bytes;
    if (!CloseHandle(file)) ok = 0;
    return ok;
}

static LRESULT CALLBACK turtle_wndproc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *created = (CREATESTRUCTW *)lparam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)created->lpCreateParams);
        return TRUE;
    }
    TurtleState *state = (TurtleState *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (state == NULL) return DefWindowProcW(window, message, wparam, lparam);
    switch (message)
    {
    case TURTLE_FORWARD:
    {
        double distance = *(const double *)lparam;
        double radians = state->heading * (3.14159265358979323846 / 180.0);
        double next_x = state->x + cos(radians) * distance;
        double next_y = state->y - sin(radians) * distance;
        if (state->pen_down)
        {
            HPEN pen = CreatePen(PS_SOLID, 2, win_color(state->color));
            if (pen != NULL)
            {
                HGDIOBJ old = SelectObject(state->canvas, pen);
                MoveToEx(state->canvas, (int)lround(state->x), (int)lround(state->y), NULL);
                LineTo(state->canvas, (int)lround(next_x), (int)lround(next_y));
                SelectObject(state->canvas, old);
                DeleteObject(pen);
                InvalidateRect(window, NULL, FALSE);
            }
        }
        state->x = next_x;
        state->y = next_y;
        return 0;
    }
    case TURTLE_TURN:
        state->heading = fmod(state->heading + *(const double *)lparam, 360.0);
        return 0;
    case TURTLE_PEN: state->pen_down = wparam != 0; return 0;
    case TURTLE_COLOR: state->color = (uint32_t)wparam; return 0;
    case TURTLE_CLEAR:
        clear_canvas(state);
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case TURTLE_SAVE: return save_bitmap(state, (const char *)lparam);
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC screen = BeginPaint(window, &paint);
        BitBlt(screen, 0, 0, state->width, state->height, state->canvas, 0, 0, SRCCOPY);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static DWORD WINAPI turtle_main(LPVOID context)
{
    OpenRequest *request = (OpenRequest *)context;
    TurtleState state = { 0 };
    state.width = request->width;
    state.height = request->height;
    state.pen_down = 1;
    state.color = 0x1d4ed8u;
    HANDLE ready = request->ready;

    BITMAPINFO format = { 0 };
    format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    format.bmiHeader.biWidth = state.width;
    format.bmiHeader.biHeight = -state.height;
    format.bmiHeader.biPlanes = 1;
    format.bmiHeader.biBitCount = 32;
    format.bmiHeader.biCompression = BI_RGB;
    state.canvas = CreateCompatibleDC(NULL);
    if (state.canvas != NULL)
        state.bitmap = CreateDIBSection(state.canvas, &format, DIB_RGB_COLORS,
                                        &state.pixels, NULL, 0);
    if (state.bitmap == NULL)
    {
        if (state.canvas != NULL) DeleteDC(state.canvas);
        SetEvent(ready);
        return 1;
    }
    state.old_bitmap = SelectObject(state.canvas, state.bitmap);
    clear_canvas(&state);

    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW window_class = { 0 };
    window_class.lpfnWndProc = turtle_wndproc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    window_class.lpszClassName = L"ChaLiTurtleWinApi";
    RegisterClassW(&window_class);
    RECT bounds = { 0, 0, state.width, state.height };
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExW(0, window_class.lpszClassName,
                                  L"ChaLi Turtle / WinAPI", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  bounds.right - bounds.left, bounds.bottom - bounds.top,
                                  NULL, NULL, instance, &state);
    set_window(window);
    if (window != NULL)
    {
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
    }
    SetEvent(ready);
    if (window != NULL)
    {
        MSG message;
        while (GetMessageW(&message, NULL, 0, 0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    set_window(NULL);
    SelectObject(state.canvas, state.old_bitmap);
    DeleteObject(state.bitmap);
    DeleteDC(state.canvas);
    return window == NULL ? 1 : 0;
}

TURTLE_EXPORT int32_t turtle_open(int32_t width, int32_t height)
{
    if (turtle_thread != NULL || width < 64 || width > 4096 || height < 64 || height > 4096) return 0;
    HANDLE ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (ready == NULL) return 0;
    OpenRequest request = { width, height, ready };
    turtle_thread = CreateThread(NULL, 0, turtle_main, &request, 0, NULL);
    if (turtle_thread == NULL)
    {
        CloseHandle(ready);
        return 0;
    }
    WaitForSingleObject(ready, INFINITE);
    CloseHandle(ready);
    if (current_window() != NULL) return 1;
    WaitForSingleObject(turtle_thread, INFINITE);
    CloseHandle(turtle_thread);
    turtle_thread = NULL;
    return 0;
}

TURTLE_EXPORT void turtle_forward(double distance)
{
    HWND window = current_window();
    if (window != NULL) SendMessageW(window, TURTLE_FORWARD, 0, (LPARAM)&distance);
}
TURTLE_EXPORT void turtle_left(double degrees)
{
    HWND window = current_window();
    if (window != NULL) SendMessageW(window, TURTLE_TURN, 0, (LPARAM)&degrees);
}
TURTLE_EXPORT void turtle_pen(int32_t down)
{
    HWND window = current_window();
    if (window != NULL) SendMessageW(window, TURTLE_PEN, (WPARAM)down, 0);
}
TURTLE_EXPORT void turtle_color(uint32_t rgb)
{
    HWND window = current_window();
    if (window != NULL) SendMessageW(window, TURTLE_COLOR, (WPARAM)rgb, 0);
}
TURTLE_EXPORT void turtle_clear(void)
{
    HWND window = current_window();
    if (window != NULL) SendMessageW(window, TURTLE_CLEAR, 0, 0);
}
TURTLE_EXPORT int32_t turtle_save_bmp(const char *path)
{
    HWND window = current_window();
    return window != NULL ? (int32_t)SendMessageW(window, TURTLE_SAVE, 0, (LPARAM)path) : 0;
}
TURTLE_EXPORT void turtle_wait(void)
{
    if (turtle_thread != NULL)
    {
        WaitForSingleObject(turtle_thread, INFINITE);
        CloseHandle(turtle_thread);
        turtle_thread = NULL;
    }
}
TURTLE_EXPORT void turtle_close(void)
{
    HWND window = current_window();
    if (window != NULL) PostMessageW(window, WM_CLOSE, 0, 0);
    turtle_wait();
}
