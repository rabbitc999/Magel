"""MAGEL 模块 AOT：独立目标文件、局部重编译、依赖失效和链接行为回归。"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run(args, *, success=True, timeout=1800):
    result = subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=timeout)
    assert (result.returncode == 0) == success, (args, result.returncode, result.stdout, result.stderr)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mc modules ", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        project = work / "project"
        shutil.copytree(ROOT / "example/module-compilation", project)
        main_file = project / "main.mgl"
        expected = run([compiler, 'run', main_file, "--out", work / "interpret"]).stdout
        assert expected == "result:42\ndrop:box\n", expected
        for backend in ("aot",):
            executable = work / (backend + (".exe" if os.name == "nt" else ""))
            run([compiler, backend, main_file, "--out", executable, "--opt-level=0"])
            actual = run([executable])
            assert actual.stdout == expected and not actual.stderr, actual
        print("[通过] 解释器 与普通 AOT 的跨模块行为一致")

        cache = work / "module cache"
        executable = work / ("modular.exe" if os.name == "nt" else "modular")
        header = work / "extra header.hpp"
        header.write_text("#define MAGEL_MODULE_TEST_VERSION 1\n", encoding="utf-8")
        glue = work / "host.cpp"
        glue.write_text('#include "' + header.as_posix() + '"\n'
                        'bool bindMagelGlue(magel_glue::Registry &, Memory::String *) { return true; }\n', encoding="utf-8")
        command = [compiler, "aot", main_file, "--separate-modules", "--module-cache", cache,
                   "--out", executable, "--opt-level=0", "--incremental", "--glue", glue]

        def build(extra=()):
            # Windows 安全扫描可能短暂持有刚运行的 exe；目标文件缓存验证不依赖旧链接产物。
            for attempt in range(50):
                try:
                    executable.unlink(missing_ok=True)
                    break
                except PermissionError:
                    if attempt == 49:
                        raise
                    time.sleep(0.1)
            result = run([*command, *extra])
            report = json.loads((cache / "modules.json").read_text(encoding="utf-8"))
            assert report["schemaVersion"] == 1 and report["backend"] == "native-aot", report
            modules = {item["name"]: item for item in report["modules"]}
            assert {"calc::left", "calc::right", "@runtime"} <= modules.keys(), modules
            return result, modules

        _, first = build()
        assert not any(item["reused"] for item in first.values()), first
        assert run([executable]).stdout == expected
        timestamps = {name: Path(item["object"]).stat().st_mtime_ns for name, item in first.items()}
        _, second = build()
        assert all(item["reused"] for item in second.values()), second
        assert timestamps == {name: Path(item["object"]).stat().st_mtime_ns for name, item in second.items()}
        print("[通过] 模块独立生成本地目标文件，第二次构建全部复用且目标文件时间不变")

        left = project / "calc/left.mgl"
        left.write_text(left.read_text(encoding="utf-8").replace("value + 1", "value + 2"), encoding="utf-8")
        _, changed = build()
        rebuilt = {name for name, item in changed.items() if not item["reused"]}
        assert rebuilt == {"calc::left"}, rebuilt
        assert run([executable]).stdout == expected.replace("42", "44")
        print("[通过] 只修改一个模块函数体，仅该模块重编译，链接结果更新")

        header.write_text("#define MAGEL_MODULE_TEST_VERSION 2\n", encoding="utf-8")
        _, headers = build()
        assert {name for name, item in headers.items() if not item["reused"]} == {"@runtime"}, headers
        right_object = Path(headers["calc::right"]["object"])
        right_object.write_bytes(b"broken object")
        _, repaired = build()
        assert {name for name, item in repaired.items() if not item["reused"]} == {"calc::right"}, repaired
        print("[通过] 外置头文件变化精确失效，损坏目标文件自动重建")

        _, options = build(["--debug"])
        assert not any(item["reused"] for item in options.values()), options
        assert run([executable]).stdout == expected.replace("42", "44")
        print("[通过] 编译选项变化使本地目标文件缓存失效")

        # 改公开函数参数后，调用者必须重新检查，不能使用旧目标文件掩盖错误。
        text = left.read_text(encoding="utf-8")
        left.write_text(text.replace("int64 add(int64 value)", "int64 add(int64 value, int64 extra)"), encoding="utf-8")
        run(command, success=False)
        left.write_text(text, encoding="utf-8")
        lock = cache / ".build-lock"
        lock.mkdir()
        try:
            result = run(command, success=False)
            assert "模块缓存正在使用" in result.stderr, result.stderr
        finally:
            lock.rmdir()
        run([compiler, "svmb", main_file, "--separate-modules", "--out", work / "bad"], success=False)
        run([compiler, "aot", main_file, "--module-cache", cache, "--out", work / "bad"], success=False)
        if shutil.which("make"):
            run([*command, "--make", shutil.which("make")], success=False)
        print("[通过] 接口变化重新检查，缓存并发锁与错误参数均明确拒绝")

        user = cache / "user-keep.txt"
        user.write_text("keep\n", encoding="utf-8")
        run([compiler, "clear", "--out-dir", cache])
        assert user.read_text(encoding="utf-8") == "keep\n"
        assert sorted(path.name for path in cache.iterdir()) == ["user-keep.txt"]
        print("[通过] 显式清理缓存只删除受管文件，保留用户文件")
    print("模块级独立 AOT 编译回归全部通过")


if __name__ == "__main__":
    main()
