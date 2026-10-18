"""注解与反射基础模块：真实前端/VM 四配置、缓存与AOT 导出回归。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""


def run(arguments, *, success=True, cwd=ROOT, timeout=1800):
    result = subprocess.run(list(map(str, arguments)), cwd=cwd, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=timeout)
    assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout, result.stderr)
    return result


def build_matrix(work, vm_off, vm_on, cxx):
    """复用已构建驱动及运行时，编译两种真实前端并分别链接两种 VM。"""
    mc = ROOT
    disabled_parser = work / "parser-disabled.o"
    flags = ["-std=c++17", "-O1", "-pthread", "-DMAGEL_CUSTOM_ALLOCATOR=1"]
    if os.name == "nt":
        flags += ["-Wa,-mbig-obj"]
    run([cxx, *flags, "-DMAGEL_WITH_REFLECTION=0", "-DSTACKVM_WITH_REFLECTION=0",
         "-I" + str(mc), "-I" + str(mc / "source"), "-I" + str(mc / "library"),
         "-I" + str(mc / "library/simple_ast"), "-I" + str(mc / "library/stackvm"), "-I" + str(mc / "library/preprocess"),
         "-c", mc / "source/magel_parser.cpp", "-o", disabled_parser])
    entry = work / "entry.cpp"
    entry.write_text("int magel_main_entry(int, char **);\n"
                     "int main(int n, char **v) { return magel_main_entry(n, v); }\n", encoding="utf-8")
    binaries = {}
    for frontend in (0, 1):
        for vm, directory in ((0, vm_off), (1, vm_on)):
            binary = work / (f"compiler-{frontend}{vm}" + EXE)
            parser = disabled_parser if not frontend else mc / "build/magel-frontend/libparser.a"
            run([cxx, *flags, entry, mc / "build/magel-frontend/magel_main.o", parser,
                 ROOT / "build/magel-runtime/libsimple_ast.a",
                 directory / "libmagel_stackvm.a",
                 ROOT / "build/magel-runtime/libpreprocess.a", "-lm", "-o", binary])
            binaries[frontend, vm] = binary
    return binaries


def check_module_contract(work, magel):
    """模块公共声明及运行时配置切换必须失效缓存，同配置可复用。"""
    source = work / "modules.mgl"
    source.write_text('int64 main() { std::lang::println("module:42"); return 0; }\n', encoding="utf-8")
    cache = work / "module-cache"
    executable = work / ("modules" + EXE)
    for active in (False, True):
        command = [magel, "aot", source, "--separate-modules", "--module-cache", cache,
                   "--out", executable, "--opt-level=0", "--keep-intermediate"]
        if active:
            command.append("--reflection")
        for repeat in (False, True):
            run(command)
            assert run([executable]).stdout == "module:42\n"
            manifest = json.loads((cache / "modules.json").read_text(encoding="utf-8"))
            assert manifest["modules"] and all(item["reused"] == repeat for item in manifest["modules"]), manifest
    print("[通过] 模块 AOT 特性配置切换使缓存失效，同配置再次构建复用")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel" + EXE))
    parser.add_argument("--vm-off", type=Path, default=ROOT / "build/reflection-vm-off")
    parser.add_argument("--vm-on", type=Path, default=ROOT / "build/reflection-vm-on")
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--modules-only", action="store_true", help="仅运行模块 AOT 契约回归")
    args = parser.parse_args()
    magel, vm_off, vm_on = args.magel.resolve(), args.vm_off.resolve(), args.vm_on.resolve()
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="magel-reflection-features-", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        if args.modules_only:
            check_module_contract(work, magel)
            return
        matrix = build_matrix(work, vm_off, vm_on, args.cxx)
        source = work / "main.mgl"
        source.write_text('int64 main() { std::lang::println("feature:42"); return 0; }\n', encoding="utf-8")
        annotated = work / "annotated.mgl"
        annotated.write_text('@annotation Label(text:String="ok")\n@Label\nint64 main(){return 0;}\n', encoding="utf-8")
        for (frontend, vm), compiler in matrix.items():
            prefix = work / f"config-{frontend}{vm}"
            assert run([compiler, 'run', source, "--out", prefix]).stdout == "feature:42\n"
            assert prefix.with_suffix(".quad").read_bytes()[:6] == b"MGQI\x01\x00"
            result = run([compiler, 'run', source, "--reflection", "--diagnostics=json"], success=bool(frontend and vm))
            document = json.loads(result.stdout)
            assert document["schemaVersion"] == 1
            assert [item["code"] for item in document["diagnostics"]] == ([] if frontend and vm else ["MGL1102"])
            rejected = json.loads(run([compiler, 'run', annotated, "--diagnostics=json"], success=False).stdout)
            assert "功能未开启" in rejected["diagnostics"][0]["message"]
            accepted = json.loads(run([compiler, 'run', annotated, "--reflection", "--diagnostics=json"],
                                     success=bool(frontend and vm)).stdout)
            assert [item["code"] for item in accepted["diagnostics"]] == ([] if frontend and vm else ["MGL1102"])
            enabled_prefix = work / f"enabled-{frontend}{vm}"
            run([compiler, 'run', source, "--reflection", "--svmb", "--emit", "--out", enabled_prefix],
                success=bool(frontend and vm))
            assert enabled_prefix.with_suffix(".svmb").exists() == bool(frontend and vm)
        print("[通过] 前端/VM 四种组合、默认关闭、显式启用和 JSON 诊断")
        shared = work / "cross"
        run([vm_on / ("magel-feature-tests" + EXE), shared])
        run([vm_off / ("magel-feature-tests" + EXE), shared])
        print("[通过] 普通、加密与紧凑格式交叉加载，未知位与直接执行拒绝")
        enabled = matrix[1, 1]
        prefix = work / "incremental"
        command = [enabled, "run", source, "--svmb", "--emit", "--no-compress-symbols", "--incremental", "--out", prefix]
        for active in (False, True, False):
            run(command + (["--reflection"] if active else []))
            data = prefix.with_suffix(".svmb").read_bytes()
            assert int.from_bytes(data[10:12], "little") == (2 if active else 0)
        print("[通过] 增量缓存区分开关，开启后再关闭不会复用带特征位产物")
        for backend in ("aot",):
            for active in (False, True):
                executable = work / (f"{backend}-{int(active)}" + EXE)
                arguments = [magel, backend, source, "--out", executable, "--opt-level=0", "--keep-intermediate"]
                run(arguments + (["--reflection"] if active else []))
                assert run([executable]).stdout == "feature:42\n"
                emitted = executable.with_name(executable.name + ".aot.cpp")
                # 从保留的 AOT 源码中验证能力宏。
                if not emitted.exists():
                    candidates = list(work.glob(executable.stem + "*.cpp"))
                    candidates = [path for path in candidates if path.name.endswith((".aot.cpp",))]
                    assert len(candidates) == 1, candidates
                    emitted = candidates[0]
                text = emitted.read_text(encoding="utf-8")
                assert f"#define STACKVM_WITH_REFLECTION {int(active)}\n" in text
                if active:
                    rejected = run([args.cxx, "-std=c++17", "-pthread", "-O0", "-DSTACKVM_WITH_REFLECTION=0",
                                    "-fsyntax-only", emitted], success=False)
                    assert "reflection feature configuration mismatch" in rejected.stderr
        print("[通过] AOT 独立程序同步特征配置，禁用所需能力时编译拒绝")
        check_module_contract(work, magel)


if __name__ == "__main__":
    main()
