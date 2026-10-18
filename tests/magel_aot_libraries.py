"""验证没有 main 的 AOT 共享库、可重定位文件及实际 C 客户端调用。"""
import argparse
import ctypes
from contextlib import nullcontext
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(command, *, success=True):
    """执行真实工具链，断言退出状态并在失败时保留诊断。"""
    result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=900,
                            env=dict(os.environ, MAGEL_LANG="zh", MAGEL_NO_STDIN_INPUT="1"))
    assert (result.returncode == 0) == success, (command, result.returncode, result.stdout, result.stderr)
    return result


def main():
    """验证输出类型、入口隔离、模块缓存和 C ABI 上下文。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel.exe" if os.name == "nt" else "magel"))
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--case", action="append", dest="cases", help="只运行指定构建场景，可重复")
    args = parser.parse_args()
    compiler = args.magel.resolve()
    suffix = ".dll" if os.name == "nt" else ".so"
    source = ROOT / "doc/examples/abi_contracts/exports.mgl"
    assert "main(" not in source.read_text(encoding="utf-8")
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="aot libraries ", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        cases = [("shared", suffix, []), ("object", ".o", []),
                 ("explicit-shared", "", ["--shared"]), ("explicit-object", "", ["--object"]),
                 ("modules-shared", suffix, ["--separate-modules"]),
                 ("modules-object", ".o", ["--separate-modules"]),
                 ("sqlite-object", ".o", [])]
        make = shutil.which("mingw32-make") or shutil.which("make")
        if make:
            cases.append(("make-object", ".o", ["--make", make]))
        if args.cases:
            assert set(args.cases) <= {case[0] for case in cases}, args.cases
            cases = [case for case in cases if case[0] in args.cases]
        for name, extension, flags in cases:
            folder = work / name
            folder.mkdir()
            output = folder / ("counter" + extension)
            case_source = source
            if name == "explicit-object":
                case_source = folder / "with-main.mgl"
                case_source.write_text(source.read_text(encoding="utf-8") +
                                       'int64 main(){panic("library main must not execute");return 0;}\n',
                                       encoding="utf-8")
                flags = [*flags, "--release"]
            if name == "sqlite-object":
                case_source = folder / "with-sqlite.mgl"
                case_source.write_text('import std::sql;\n' + source.read_text(encoding="utf-8") +
                                       'export int32 probe(){return std::sql::__sqlClose((int32)0);}\n',
                                       encoding="utf-8")
            result = run([compiler, "aot", case_source, "--out", output,
                          "--cxx", args.cxx, "--cc", args.cc, "--opt-level=0", *flags])
            artifact = output if extension else output.with_suffix(suffix if "shared" in name else ".o")
            assert artifact.is_file() and (folder / "counter.h").is_file(), result.stdout
            assert not list(folder.glob("*.aot.*")), list(folder.iterdir())
            nm = shutil.which("nm")
            if nm:
                symbols = run([nm, "--defined-only", artifact]).stdout
                assert re.search(r"\bmagel_add$", symbols, re.M), symbols[-2000:]
                assert not re.search(r"\b[Tt] _?main$", symbols, re.M), symbols[-2000:]
            client_object = folder / "client.o"
            run([args.cc, "-std=c11", "-c", ROOT / "doc/examples/abi_contracts/client.c",
                 "-I", folder, "-o", client_object])
            client = folder / ("client.exe" if os.name == "nt" else "client")
            extra = [] if os.name == "nt" else ["-ldl", "-Wl,-rpath," + str(folder)]
            run([args.cxx, client_object, artifact, "-pthread", *extra, "-o", client])
            assert run([client]).stdout.strip() == "C exports OK"
            dynamic_artifact = artifact if artifact.suffix == suffix else None
            if name in ("object", "sqlite-object"):
                dynamic_artifact = folder / ("from-object" + suffix)
                run([args.cxx, "-shared", artifact, "-pthread", *extra, "-o", dynamic_artifact])
            if dynamic_artifact:
                # 实际动态加载和按符号获取函数，验证不依赖宿主的链接期入口。
                compiler_path = shutil.which(args.cxx) or args.cxx
                search = os.add_dll_directory(str(Path(compiler_path).resolve().parent)) if os.name == "nt" else nullcontext()
                with search:
                    library = ctypes.CDLL(str(dynamic_artifact))
                create = library.magel_create
                create.argtypes, create.restype = [ctypes.c_size_t], ctypes.c_void_p
                add = library.magel_add
                add.argtypes, add.restype = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32,
                                           ctypes.POINTER(ctypes.c_int32)], ctypes.c_int
                destroy = library.magel_destroy
                destroy.argtypes, destroy.restype = [ctypes.c_void_p], None
                context = create(8 * 1024 * 1024)
                assert context
                try:
                    value = ctypes.c_int32()
                    assert add(context, 1, 2, ctypes.byref(value)) == 0 and value.value == 11
                    if name == "sqlite-object":
                        probe = library.magel_probe
                        probe.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int32)]
                        probe.restype = ctypes.c_int
                        assert probe(context, ctypes.byref(value)) == 0 and value.value == -1
                finally:
                    destroy(context)
                if os.name == "nt":
                    import _ctypes
                    _ctypes.FreeLibrary(library._handle)
                del library
            if "modules" in name:
                repeated = run([compiler, "aot", source, "--out", output,
                                "--cxx", args.cxx, "--opt-level=0", *flags])
                assert "0 个编译" in repeated.stdout, repeated.stdout
                assert run([client]).stdout.strip() == "C exports OK"
            print(f"[通过] {name}：无 main，C 客户端、上下文与错误传播", flush=True)

        for extension in (".lib", ".a"):
            result = run([compiler, "aot", source, "--out", work / ("bad" + extension)], success=False)
            assert "不支持静态库" in result.stderr, result.stderr
        for flags, extension in ((["--shared", "--object"], ".o"), (["--object"], suffix),
                                 (["--shared"], ".o")):
            result = run([compiler, "aot", source, "--out", work / ("bad" + extension), *flags], success=False)
            assert "冲突" in result.stderr, result.stderr
        wrong_target = ".so" if os.name == "nt" else ".dll"
        result = run([compiler, "aot", source, "--out", work / ("bad" + wrong_target)], success=False)
        assert "目标不匹配" in result.stderr, result.stderr
        ordinary = work / "ordinary.mgl"
        ordinary.write_text("int64 main(){return 0;}\n", encoding="utf-8")
        result = run([compiler, "aot", ordinary, "--out", work / ("bad" + suffix)], success=False)
        assert "至少一个 export" in result.stderr, result.stderr
        result = run([compiler, "run", ordinary, "--object"], success=False)
        assert "只支持 aot" in result.stderr, result.stderr
        run([compiler, "clear", "--out-dir", artifact.parent])
        assert artifact.is_file() and (artifact.parent / "counter.h").is_file()
        print("[通过] 拒绝静态库、冲突与错误目标；clear 保留交付产物", flush=True)


if __name__ == "__main__":
    main()
