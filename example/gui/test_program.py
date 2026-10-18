#!/usr/bin/env python3
"""MAGEL 导出程序的按钮事件和文本更新端到端检查（Windows 桌面）。"""

import ctypes
import os
from pathlib import Path
import subprocess
import sys
import time


def main() -> None:
    if os.name != "nt":
        raise SystemExit("这个测试需要 Windows 桌面")
    folder = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[2] / "build" / "magel-gui-winapi"
    )
    user32 = ctypes.windll.user32
    user32.FindWindowW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p]
    user32.FindWindowW.restype = ctypes.c_void_p
    user32.GetDlgItem.argtypes = [ctypes.c_void_p, ctypes.c_int]
    user32.GetDlgItem.restype = ctypes.c_void_p
    user32.SendMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                     ctypes.c_size_t, ctypes.c_ssize_t]
    user32.SendMessageW.restype = ctypes.c_ssize_t
    user32.PostMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                     ctypes.c_size_t, ctypes.c_ssize_t]
    user32.GetWindowTextW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int]

    def label_text(window: int) -> str:
        label = user32.GetDlgItem(window, 10)
        output = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(label, output, len(output))
        return output.value

    program = subprocess.Popen([str(folder / "counter.exe")], cwd=folder,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    window = None
    try:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            window = user32.FindWindowW("MagelSimpleGuiWinApi", None)
            if window and user32.GetDlgItem(window, 1) and user32.GetDlgItem(window, 2): break
            if program.poll() is not None: break
            time.sleep(0.05)
        assert window, "MAGEL GUI 窗口没有启动"

        def click_and_expect(button_id: int, expected: str) -> None:
            button = user32.GetDlgItem(window, button_id)
            assert button, f"按钮 {button_id} 不存在"
            user32.SendMessageW(window, 0x0111, button_id, button)  # WM_COMMAND / BN_CLICKED
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if label_text(window) == expected: return
                time.sleep(0.03)
            raise AssertionError(f"按钮 {button_id} 后应显示 {expected!r}，实际 {label_text(window)!r}")

        click_and_expect(1, "计数：1")
        click_and_expect(1, "计数：2")
        click_and_expect(2, "你好，Magel")
        user32.PostMessageW(window, 0x0010, 0, 0)  # WM_CLOSE
        stdout, stderr = program.communicate(timeout=10)
        assert program.returncode == 0, (stdout, stderr)
        print("MAGEL GUI program checks passed")
    finally:
        if program.poll() is None:
            if window: user32.PostMessageW(window, 0x0010, 0, 0)
            try: program.wait(timeout=3)
            except subprocess.TimeoutExpired: program.kill()


if __name__ == "__main__":
    main()
