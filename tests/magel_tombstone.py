"""验证墓碑幂等、显式清扫、跨清扫存活对象及 VM/AOT 一致性。"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/tombstone"


def run(*args):
    """执行工具并保留完整失败上下文。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                          encoding="utf-8", errors="replace", timeout=240)


def main():
    """对标量、结构体和数组分别验证清扫前后释放。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    args = parser.parse_args()
    compiler = args.magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    cases = {
        "scalar": ("void dispose(int64 mut& value) { delete value; }", "int64 mut& value = heap int64;"),
        "struct": ("struct Item { int64 number; }; void dispose(Item mut& value) { delete value; }", "Item mut& value = heap Item;"),
        "array": ("void dispose(int64 value[]) { delete value; }", "int64 value[] = new[4];"),
    }
    for kind, (helper, allocation) in cases.items():
        for swept in (False, True):
            source = WORK / f"{kind}-{swept}.mgl"
            body = "dispose(value); dispose(value);"
            if swept:
                body += "if (std::lang::sweep() < 1) { panic(\"missing tombstone\"); } dispose(value);"
            source.write_text(helper + "\nint64 main() { " + allocation + body +
                              'std::lang::println("ok"); return 0; }\n', encoding="utf-8")
            commands = [("VM", [compiler, "run", source, "--out", WORK / "vm"])]
            exe = WORK / (source.stem + (".exe" if os.name == "nt" else ""))
            built = run(compiler, "aot", source, "checked", "out", exe, "opt", "0")
            assert built.returncode == 0, (kind, swept, built.stdout, built.stderr)
            commands.append(("AOT", [exe]))
            for backend, command in commands:
                result = run(*command)
                if swept:
                    assert result.returncode != 0 and "MGL1205" in result.stderr, (kind, backend, result)
                else:
                    assert result.returncode == 0 and result.stdout == "ok\n" and not result.stderr, (kind, backend, result)
            print(f"[通过] {kind} 清扫={swept}：VM/AOT", flush=True)
    source = WORK / "live.mgl"
    source.write_text('void dispose(int64 mut& value) { delete value; }\n'
                      'int64 main() { int64 mut& live = heap int64; std::lang::sweep(); '
                      '*live = 42; dispose(live); dispose(live); '
                      'if (std::lang::sweep() < 1) { panic("missing live tombstone"); } '
                      'if (std::lang::sweep() != 0) { panic("nonempty sweep"); } '
                      'std::lang::println("live ok"); return 0; }\n', encoding="utf-8")
    result = run(compiler, "run", source)
    assert result.returncode == 0 and result.stdout == "live ok\n" and not result.stderr, result
    print("[通过] 跨清扫活跃分配、后续幂等释放与空清扫", flush=True)
    # 小块与大块合并后反复分配大块，不能因旧槽位宽而持续扩展物理堆。
    source = WORK / "merged.mgl"
    source.write_text('int64 main() { int64 tiny[] = new[1]; delete tiny; '
                      'for (int64 i=0; i<300; i++) { int64 large[] = new[262144]; delete large; } '
                      'std::lang::println("merged ok"); return 0; }\n', encoding="utf-8")
    output = WORK / ("merged" + (".exe" if os.name == "nt" else ""))
    built = run(compiler, "aot", source, "checked", "out", output, "opt", "0")
    assert built.returncode == 0, built
    for command in ([compiler, "run", source, "--memory-size", "33554432"],
                    [output, "--memory-size", "33554432"]):
        result = run(*command)
        assert result.returncode == 0 and result.stdout == "merged ok\n" and not result.stderr, result
    print("[通过] 大小块合并后 300 次复用保持物理堆上界：VM/AOT", flush=True)



if __name__ == "__main__":
    main()
