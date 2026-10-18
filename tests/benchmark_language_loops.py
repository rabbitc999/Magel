"""下载并编译官方 Lua，与 CPython 和真实 Magel 字节码执行同一 while 循环。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import time
import subprocess
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/lua-benchmark"
URL = "https://www.lua.org/ftp/lua-5.5.1.tar.gz"
SHA256 = "1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce"


def pick_make():
    """Windows 上通常只有 mingw32-make，Python 的 subprocess 又跑不了 shell shim。"""
    import shutil
    if os.name == "nt" and shutil.which("make") is None:
        for candidate in ("mingw32-make", "gmake"):
            if shutil.which(candidate):
                return candidate
    return "make"


MAKE = pick_make()
MACHINE = (ROOT / "tmp/machine.txt").read_text(encoding="utf-8", errors="replace").strip() \
    if (ROOT / "tmp/machine.txt").exists() else ""

def run(command, **kwargs):
    """执行子进程并保留失败输出。"""
    result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", **kwargs)
    if result.returncode:
        raise RuntimeError(f"{command}\n{result.stdout}\n{result.stderr}")
    return result.stdout

def main():
    """按固定输入、一次预热和重复中位数输出完整可复现报告。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    parser.add_argument("--runner", type=Path, default=ROOT / "build/vm-program-benchmark.exe")
    parser.add_argument("--baseline-runner", type=Path)
    parser.add_argument("--skip-aot", action="store_true")
    parser.add_argument("--skip-reference", action="store_true", help="跳过未优化逐条执行；仍测旧优化核心")
    parser.add_argument("--skip-legacy", action="store_true", help="跳过关闭寄存器核心的旧优化路径")
    parser.add_argument("--core-variants", action="store_true", help="额外比较标签分派、跳转折叠、专用码与块执行")
    parser.add_argument("--iterations", type=int, default=1000000)
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()
    assert args.iterations > 0 and args.repeats > 0
    WORK.mkdir(parents=True, exist_ok=True)
    archive = WORK / "lua-5.5.1.tar.gz"
    if not archive.exists():
        urllib.request.urlretrieve(URL, archive)
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == SHA256, "Lua 源码摘要不同"
    source = WORK / "lua-5.5.1"
    if not source.exists():
        with tarfile.open(archive) as package:
            package.extractall(WORK, filter="data")
    suffix = ".exe" if os.name == "nt" else ""
    lua = source / "src" / ("lua" + suffix)
    if not lua.exists():
        run([MAKE, "-C", source / "src", "mingw" if os.name == "nt" else "linux", "-j2"])
    count, repeats = args.iterations, args.repeats
    expected = str(count * (count - 1) // 2)
    python_source = f"""import time
def loop():
    i = 0
    total = 0
    while i < {count}:
        total = total + i
        i = i + 1
    return total
loop()
for _ in range({repeats}):
    start = time.perf_counter_ns()
    value = loop()
    elapsed = (time.perf_counter_ns()-start)/1e6
    print(elapsed, value)
"""
    lua_source = f"""local function loop()
    local i, total = 0, 0
    while i < {count} do
        total = total + i
        i = i + 1
    end
    return total
end
loop()
for r=1,{repeats} do
    local start = os.clock()
    local value = loop()
    local elapsed = (os.clock()-start)*1000
    print(elapsed, value)
end
"""
    magel_source = f"""int64 calculate(int64 limit) {{
    int64 i = 0, total = 0;
    while(i < limit) {{
        total = total + i;
        i = i + 1;
    }}
    return total;
}}
int64 main() {{ std::lang::println("{{}}", calculate(std::lang::inputInt())); return 0; }}
"""
    pyfile, luafile, mglfile = (WORK / ("loop" + ext) for ext in (".py", ".lua", ".mgl"))
    pyfile.write_text(python_source, encoding="utf-8")
    luafile.write_text(lua_source, encoding="utf-8")
    mglfile.write_text(magel_source, encoding="utf-8")
    prefix = WORK / "loop-program"
    run([args.magel.resolve(), "svmb", mglfile, "out", prefix])
    results = {}
    def record(name, output, vm=False):
        values = []
        for line in output.splitlines():
            fields = line.split()
            if not fields:
                continue
            assert fields[-1] == expected, (name, line, expected)
            values.append(float(fields[0]))
        assert len(values) == repeats, (name, values, output)
        results[name] = {"median_ms": statistics.median(values), "samples_ms": values,
                         "checksum": expected}
        if vm:
            results[name]["logical_steps"] = int(output.splitlines()[0].split()[1])
    record("Python", run([sys.executable, pyfile]))
    record("Lua", run([lua, luafile]))
    record("Magel VM optimized", run([args.runner.resolve(), prefix, repeats, "1", count]), True)
    core_variants = {
        "Magel register switch": ["1", "0", "1", "1", "1"],
        "Magel register labels": ["1", "1", "1", "1", "1"],
        "Magel register no jump folding": ["1", "0", "0", "1", "1"],
        "Magel register generic opcodes": ["1", "0", "1", "0", "1"],
        "Magel register per instruction": ["1", "0", "1", "1", "0"],
        "Magel register compact only": ["1", "0", "0", "0", "0"],
    }
    if args.core_variants:
        for name, switches in core_variants.items():
            record(name, run([args.runner.resolve(), prefix, repeats, "1", count, *switches]), True)
    if not args.skip_legacy:
        record("Magel VM legacy optimized", run([args.runner.resolve(), prefix, repeats, "1", count, "0"]), True)
    if not args.skip_reference:
        record("Magel VM reference", run([args.runner.resolve(), prefix, repeats, "0", count]), True)
    if args.baseline_runner:
        record("Magel VM before", run([args.baseline_runner.resolve(), prefix, repeats, "1", count]), True)
    if not args.skip_aot:
        aot_source = "import std::clock;\n" + magel_source[:magel_source.index("int64 main()")]
        aot_source += f"""int64 main() {{
    int64 n=std::lang::inputInt();
    int64 warmup=calculate(n);
    for(int64 r=0;r<{repeats};r++) {{
        int64 limit=std::lang::inputInt();
        int64 start=std::clock::nowNanos();
        int64 value=calculate(limit);
        int64 elapsed=std::clock::nowNanos()-start;
        std::lang::println(\"{{}} {{}}\", elapsed/1000000.0, value);
    }}
    if(warmup<0) {{ return 1; }}
    return 0;
}}
"""
        aot_file = WORK / "loop-aot.mgl"
        aot_file.write_text(aot_source, encoding="utf-8")
        aot_exe = WORK / ("loop-aot" + suffix)
        run([args.magel.resolve(), "aot", aot_file, "--out", aot_exe,
             "--opt-level=2", "--keep-intermediate"], timeout=600)
        record("Magel AOT", run([aot_exe], input=(str(count)+"\n")*(repeats+1)))
    java_file = WORK / "LoopBenchmark.java"
    java_file.write_text("""public final class LoopBenchmark {
    private static volatile long sink;
    private static long calculate(long n) {
        long i=0, total=0;
        while(i<n) { total=total+i; i=i+1; }
        return total;
    }
    public static void main(String[] args) {
        long n=Long.parseLong(args[0]);
        int repeats=Integer.parseInt(args[1]), warmups=Integer.parseInt(args[2]);
        for(int r=0;r<warmups;r++) sink=calculate(n);
        for(int r=0;r<repeats;r++) {
            long start=System.nanoTime();
            long value=calculate(n);
            long elapsed=System.nanoTime()-start;
            sink=value;
            System.out.println((elapsed/1000000.0)+" "+value);
        }
    }
}
""", encoding="utf-8")
    run(["javac", "-encoding", "UTF-8", java_file])
    record("Java HotSpot interpreter (-Xint)", run(["java", "-Xint", "-cp", WORK, "LoopBenchmark", count, repeats, 1]))
    record("Java HotSpot JIT", run(["java", "-Xbatch", "-cp", WORK, "LoopBenchmark", count, repeats, 20]))
    cold_start = time.perf_counter_ns()
    cold_output = run(["java", "-Xbatch", "-cp", WORK, "LoopBenchmark", count, 1, 0])
    java_cold_ms = (time.perf_counter_ns()-cold_start)/1e6
    assert cold_output.split()[-1] == expected
    report = {"iterations": count, "warmups": 1, "repeats": repeats,
              "java": run(["java", "--version"]).strip(),
              "java_flags": "-Xbatch; 20 full-loop warmups; runtime long input; volatile result sink",
              "java_interpreter_flags": "-Xint; JIT disabled; 1 full-loop warmup; runtime long input; volatile result sink",
              "register_core_variants": core_variants if args.core_variants else {},
              "java_cold_single_run_process_ms": java_cold_ms,
              "java_cold_single_loop_ms": float(cold_output.split()[0]),
              "platform": platform.platform(), "python": sys.version,
              "lua": run([lua, "-v"]).strip(), "lua_url": URL, "lua_sha256": SHA256,
              "compiler": run(["gcc", "--version"]).splitlines()[0],
              "make": MAKE,
              "machine": MACHINE,
              "magel_version": (ROOT / "VERSION").read_text(encoding="utf-8").strip()
              if (ROOT / "VERSION").exists() else "",
              "magel_commit": run(["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"]).strip(),
              "compiler_flags": "Lua -O2; Magel -O2 -DNDEBUG MAGEL_CUSTOM_ALLOCATOR=1",
              "aot_flags": "-O2 unchecked; fresh runtime input per sample prevents folding and cross-sample hoisting; generated source retained",
              "timing": "Java System.nanoTime; AOT std::clock::nowNanos around calculate only; Python perf_counter_ns; Lua os.clock; Magel steady_clock; exclude parsing and file loading; Magel includes execution-plan setup and final checksum output",
              "results": results}
    (WORK / "results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()
