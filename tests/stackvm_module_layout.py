"""检查 StackVM 独立编译、文件组织与实现修改后的重编范围。"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    """核对清单、包含位置，并以只读构建计划验证单实现重编。"""
    directory = ROOT / "source/stackvm"
    names = (directory / "core_sources.txt").read_text(encoding="utf-8").splitlines()
    for name in names:
        assert re.fullmatch(r"[a-z][a-z0-9_]*\.cpp", name), name
        text = (directory / name).read_text(encoding="utf-8")
        assert not re.search(r'^\s*#include\s*["<][^">]*\.(?:cpp|inc)[">]', text, re.M), name
        namespace = re.search(r"^namespace\b", text, re.M)
        if namespace:
            assert not re.search(r"^\s*#include\b", text[namespace.start():], re.M), name
    target = "build/chali-stackvm-core/libstackvm.a"
    probe = directory / "native_text.cpp"
    saved = probe.stat()
    make = os.environ.get("MAKE", "make")
    try:
        os.utime(probe, ns=(saved.st_atime_ns, max(saved.st_mtime_ns, (ROOT / target).stat().st_mtime_ns) + 1_000_000))
        # 配置写入器每次都会运行但内容未变时不更新时间；干跑需保留已经验收的配置戳，
        # 避免 make 假设该伪目标改变而把全部对象误记为重编。
        result = subprocess.run([make, "-n", "-o", "build/chali-runtime/buildConfig.json", target], cwd=ROOT, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=60)
        assert result.returncode == 0, result
        units = re.findall(r"\s-c\s+source/stackvm/([a-z0-9_]+\.cpp)\s", result.stdout)
        assert units == ["native_text.cpp"], (units, result.stdout, result.stderr)
        assert "stackvm.o" not in result.stdout, result.stdout
    finally:
        os.utime(probe, ns=(saved.st_atime_ns, saved.st_mtime_ns))
    print(f"StackVM module layout: {len(names)} independent units; one implementation recompiles one core object")

if __name__ == "__main__":
    main()
