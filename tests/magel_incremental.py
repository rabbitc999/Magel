"""End-to-end checks for Magel's optional persistent compilation cache."""

from pathlib import Path
import subprocess
import tempfile
import os
import re
import json


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "magel.exe"
if not EXE.exists():
    EXE = ROOT


def run(*args: str, success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run([str(EXE), *map(str, args)], cwd=ROOT,
                            capture_output=True, text=True, encoding="utf-8")
    if (result.returncode == 0) != success:
        raise AssertionError(f"exit={result.returncode}\n{result.stdout}\n{result.stderr}")
    return result


def main() -> None:
    if not EXE.exists():
        raise SystemExit("build the Magel compiler first: make -C magel")
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="magel-incremental-", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        module = work / "math" / "ops.mgl"
        module.parent.mkdir()
        body = module.parent / "body.inc"
        source = work / "app.mgl"
        source.write_text("module demo;\nimport math::ops;\n"
                          "int64 main() { std::lang::println(\"{}\", math::ops::twice(2)); return 0; }\n",
                          encoding="utf-8")
        module.write_text('module math::ops;\n%include "body.inc"\n', encoding="utf-8")
        body.write_text("int64 twice(int64 x) { return x + x; }\n", encoding="utf-8")
        output = work / "build" / "app"
        output.parent.mkdir()
        args = ("run", source, "--emit", "--svmb", "--incremental", "-I", ROOT,
                "--out", output)

        first = run(*args)
        assert "2 个模块重新预处理" in first.stderr, first.stderr
        original = (output.with_suffix(".quad")).read_bytes()
        written_at = output.with_suffix(".quad").stat().st_mtime_ns
        cached = run(*args)
        assert "增量缓存命中：2 个模块复用" in cached.stderr, cached.stderr
        assert (output.with_suffix(".quad")).read_bytes() == original
        assert output.with_suffix(".quad").stat().st_mtime_ns == written_at

        for unit in Path(str(output) + ".magel-cache").glob("*.unit"):
            unit.write_bytes(b"damaged cache")
        recovered = run(*args)
        assert "增量缓存命中：0 个模块复用" in recovered.stderr, recovered.stderr

        body.write_text("int64 twice(int64 x) { return x + x + 1; }\n", encoding="utf-8")
        changed = run(*args)
        assert "1 个模块复用，1 个模块重新预处理" in changed.stderr, changed.stderr
        assert output.with_suffix(".quad").read_bytes() != original

        with output.with_suffix(".quad").open("ab") as stream:
            stream.write(b"tampered\n")
        repaired = run(*args)
        assert "2 个模块复用，0 个模块重新预处理" in repaired.stderr, repaired.stderr
        assert not output.with_suffix(".quad").read_bytes().endswith(b"tampered\n")

        module.write_text("module math::ops;\nint64 twice(int64 x) { return ; }\n",
                          encoding="utf-8")
        run(*args, success=False)
        module.write_text('module math::ops;\n%include "body.inc"\n', encoding="utf-8")
        assert "增量缓存命中" in run(*args).stderr

        # Output switches invalidate outputs, but keep module preprocessing reusable.
        plain_binary = output.with_suffix(".svmb").read_bytes()
        debug = run(*args, "--debug")
        assert "2 个模块复用，0 个模块重新预处理" in debug.stderr, debug.stderr
        assert output.with_suffix(".svmb").read_bytes() != plain_binary
        assert "增量缓存命中" not in debug.stderr
        assert "增量缓存命中" in run(*args, "--debug").stderr
        compact = run(*args, "--compress-symbols")
        assert "2 个模块复用，0 个模块重新预处理" in compact.stderr, compact.stderr
        mapping = Path(str(output) + ".symbol_map.json")
        assert mapping.exists()
        mapping.write_bytes(b"damaged map")
        repair_map = run(*args, "--compress-symbols")
        assert "2 个模块复用，0 个模块重新预处理" in repair_map.stderr, repair_map.stderr
        assert json.loads(mapping.read_text(encoding="utf-8"))
        assert "增量缓存命中" in run(*args, "--compress-symbols").stderr

        # A common include is content-hashed once per compiler invocation.
        shared = work / "shared.inc"
        shared.write_text("// shared dependency A\n", encoding="utf-8")
        source.write_text(source.read_text(encoding="utf-8") + '%include "shared.inc"\n', encoding="utf-8")
        module.write_text('module math::ops;\n%include "body.inc"\n%include "../shared.inc"\n', encoding="utf-8")
        run(*args)
        checked = run(*args, "--stats")
        counts = re.search(r"增量文件校验：(\d+) 次读取，(\d+) 次复用", checked.stderr)
        assert counts and int(counts[2]) >= 1, checked.stderr
        assert "增量缓存命中" in checked.stderr
        print("shared dependency hash reads/reuses:", counts[1], counts[2])
        # Same-size edits with restored timestamps must still invalidate across runs.
        stamp = shared.stat()
        shared.write_text("// shared dependency B\n", encoding="utf-8")
        os.utime(shared, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        edited = run(*args)
        assert "0 个模块复用，2 个模块重新预处理" in edited.stderr, edited.stderr

        runtime = run('run', source, "--incremental", "-I", ROOT,
                      "--out", work / "build" / "runtime")
        assert runtime.stdout.strip() == "5", runtime.stdout

    print("Magel incremental compilation: OK")


if __name__ == "__main__":
    main()
