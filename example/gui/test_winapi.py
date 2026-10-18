#!/usr/bin/env python3
"""Windows 桌面上验证 DLL 的控件、UTF-8 文本和按钮事件。"""

import ctypes
import os
from pathlib import Path
import sys
import time


def main() -> None:
    if os.name != "nt":
        raise SystemExit("这个测试需要 Windows 桌面")
    dll = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[2] / "build" / "magel-gui-winapi" / "mcgui.dll"
    )
    gui = ctypes.CDLL(str(dll))
    i32 = ctypes.c_int32
    gui.mcgui_open.argtypes = [i32, i32, ctypes.c_char_p]
    gui.mcgui_open.restype = i32
    for name in ("mcgui_label", "mcgui_button", "mcgui_text_box"):
        method = getattr(gui, name)
        method.argtypes = [i32, i32, i32, i32, i32, ctypes.c_char_p]
        method.restype = i32
    gui.mcgui_set_text.argtypes = [i32, ctypes.c_char_p]
    gui.mcgui_set_text.restype = i32
    gui.mcgui_get_text.argtypes = [i32]
    gui.mcgui_get_text.restype = ctypes.c_char_p
    gui.mcgui_poll_event.restype = i32
    gui.mcgui_close.restype = None

    user32 = ctypes.windll.user32
    user32.FindWindowW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p]
    user32.FindWindowW.restype = ctypes.c_void_p
    user32.GetDlgItem.argtypes = [ctypes.c_void_p, ctypes.c_int]
    user32.GetDlgItem.restype = ctypes.c_void_p
    user32.SendMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                     ctypes.c_size_t, ctypes.c_ssize_t]
    user32.SendMessageW.restype = ctypes.c_ssize_t

    assert gui.mcgui_open(360, 180, "MAGEL 窗口".encode()) == 1
    try:
        assert gui.mcgui_label(10, 10, 10, 250, 25, "初始".encode()) == 1
        assert gui.mcgui_set_text(10, "计数：1".encode()) == 1
        assert gui.mcgui_set_text(10, "计数：2".encode()) == 1
        assert gui.mcgui_get_text(10).decode() == "计数：2"
        assert gui.mcgui_button(1, 10, 45, 100, 30, "按钮".encode()) == 1
        assert gui.mcgui_text_box(11, 10, 90, 200, 28, "内容".encode()) == 1
        assert gui.mcgui_button(1, 120, 45, 100, 30, b"duplicate") == 0
        assert gui.mcgui_set_text(11, "你好，MAGEL".encode()) == 1
        assert gui.mcgui_get_text(11).decode() == "你好，MAGEL"
        assert gui.mcgui_set_text(11, "第二次".encode()) == 1
        assert gui.mcgui_get_text(11).decode() == "第二次"
        assert gui.mcgui_set_text(11, b"x" * 1025) == 0
        assert gui.mcgui_get_text(11).decode() == "第二次"
        window = user32.FindWindowW("MagelSimpleGuiWinApi", None)
        button = user32.GetDlgItem(window, 1)
        assert window and button
        user32.SendMessageW(window, 0x0111, 1, button)  # WM_COMMAND / BN_CLICKED
        user32.SendMessageW(window, 0x0111, 1, button)
        assert gui.mcgui_poll_event() == 1
        assert gui.mcgui_poll_event() == 1
        assert gui.mcgui_poll_event() == 0
        callback_type = ctypes.CFUNCTYPE(None, ctypes.c_int64, ctypes.c_int64, ctypes.c_void_p)
        events = []
        callback = callback_type(lambda event, source, user: events.append((event, source, user)))
        gui.mcgui_set_callback.argtypes = [callback_type, ctypes.c_void_p]
        gui.mcgui_start_timer.argtypes = [i32]
        gui.mcgui_set_callback(callback, 123)
        gui.mcgui_post_event(17)
        assert events[-1] == (17, 0, 123)
        assert gui.mcgui_poll_event() == 17
        gui.mcgui_start_timer(0)
        for _ in range(3):
            gui.mcgui_pump()
        assert events[-3:] == [(0, 1, 123), (0, 2, 123), (0, 3, 123)]
        gui.mcgui_stop_timer()
        count = len(events)
        gui.mcgui_pump()
        assert len(events) == count
        gui.mcgui_start_timer(30)
        gui.mcgui_pump()
        assert len(events) == count
        time.sleep(0.04)
        gui.mcgui_pump()
        assert events[-1] == (0, 1, 123)
        gui.mcgui_stop_timer()
        started = time.monotonic()
        assert gui.mcgui_wait_event() == 0
        assert time.monotonic() - started < 0.5
        gui.mcgui_set_callback(callback_type(), None)
        gui.mcgui_post_event(18)
        assert gui.mcgui_poll_event() == 18
        assert events[-1] == (0, 1, 123)
        for event in range(1, 259):
            gui.mcgui_post_event(event)
        assert [gui.mcgui_poll_event() for _ in range(256)] == list(range(1, 257))
        assert gui.mcgui_poll_event() == 0
    finally:
        gui.mcgui_close()
    assert gui.mcgui_poll_event() == -1
    print("WinAPI GUI DLL checks passed")


if __name__ == "__main__":
    main()
