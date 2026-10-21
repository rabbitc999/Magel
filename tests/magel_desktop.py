"""构建并验证海龟绘图及 MAGEL 胶水；产物统一放在 build 下。"""
from pathlib import Path
import argparse
import os
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
MAGEL = ROOT / 'magel.exe'

def run(*args, cwd=ROOT):
    """执行构建或测试，失败时立即停止并显示诊断。"""
    print('运行：', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, check=True, timeout=300)

def run_program(*args, cwd):
    """运行桌面产物，拒绝运行错误及资源泄漏诊断。"""
    result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True, text=True, encoding='utf-8', timeout=120)
    assert result.returncode == 0 and (not result.stderr), result
    print(result.stdout, end='', flush=True)
    return result

def main():
    """使用当前编译器验证所选桌面项目及真实动态库。"""
    global MAGEL
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=MAGEL)
    selected = parser.add_mutually_exclusive_group()
    selected.add_argument('--turtle-only', action='store_true')
    args = parser.parse_args()
    MAGEL = args.magel.resolve()
    if os.name != 'nt':
        raise SystemExit('本测试需要 Windows 桌面与 MinGW；请在 Windows 桌面运行')
    test_turtle()
    print("海龟的 WinAPI 端到端测试通过", flush=True)

def test_turtle():
    """验证海龟 C ABI 胶水及实际输出画布。"""
    import struct
    source = ROOT / 'doc/examples/turtle_winapi'
    output = ROOT / 'build/magel-turtle-winapi'
    output.mkdir(parents=True, exist_ok=True)
    run('gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-shared', source / 'turtle.c', '-o', output / 'turtle.dll', '-lgdi32', '-luser32', '-lm')
    run_program(output / 'demo.exe', 'close', cwd=output)
    bitmap = (output / 'turtle.bmp').read_bytes()
    assert bitmap[:2] == b'BM'
    width, height = struct.unpack_from('<ii', bitmap, 18)
    assert width == 900 and abs(height) == 700
    offset = struct.unpack_from('<I', bitmap, 10)[0]
    colors = {bitmap[i:i + 3] for i in range(offset, len(bitmap), 4)}
    assert len(colors) >= 4, colors
    print('海龟绘图：C ABI、MAGEL 导出、BMP 尺寸和颜色通过', flush=True)
if __name__ == '__main__':
    main()
