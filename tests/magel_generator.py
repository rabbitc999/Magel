"""Focused checks for the C++ Magel toolchain generator."""

import os
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""
GENERATOR = ROOT / ("magel" + EXE)
WORK = ROOT / "tmp" / "magel-generator-tests"


def command(arguments, *, succeeds=True):
    """通过统一工具链生成或按清单清理工程。"""
    arguments = list(arguments)
    if arguments[1] != "clear":
        arguments.insert(1, "gen")
    result = subprocess.run(
        [str(item) for item in arguments], cwd=ROOT, text=True,
        encoding="utf-8", errors="replace", capture_output=True, check=False,
    )
    if (result.returncode == 0) != succeeds:
        raise AssertionError(
            f"command returned {result.returncode}: {' '.join(map(str, arguments))}\n"
            f"{result.stdout}\n{result.stderr}"
        )
    return result


def main():
    if not GENERATOR.exists():
        raise SystemExit(f"build magel first: {GENERATOR}")
    shutil.rmtree(WORK, ignore_errors=True)
    WORK.mkdir(parents=True)
    try:
        command([GENERATOR, "compiler", "--out-dir", WORK / "removed-robot", "--module", "robot"], succeeds=False)
        assert not (WORK / "removed-robot").exists()
        command([GENERATOR, "compiler", "--out-dir", WORK / "removed-utils", "--module", "utils"], succeeds=False)
        assert not (WORK / "removed-utils").exists()
        compiler = WORK / "compiler"
        command([GENERATOR, "compiler", "--out-dir", compiler])
        required = [
            "magel_parser.cpp", "magel_main.cpp", "magel_incremental.hpp",
            "magel_borrow_checker.hpp", "magel_borrow_flow.hpp", "semantics/borrow_flow.cpp", "semantics/borrow_return_sources.cpp", "semantics/borrow_facts.cpp", "semantics/expression_effects.cpp",
            "magel_package.hpp", "magel_project.hpp", "magel_cli.hpp", "magel_i18n.hpp", "console/utf8.hpp",
            "runtime/stackvm.cpp", "runtime/execution_plan.hpp", "runtime/register_core.hpp", "runtime/allocation_index.hpp",
            "runtime/data_capacity_index.hpp", "runtime/parts/optimization_flow.inc",
            "runtime/parts/execution_fusion.inc", "runtime/parts/register_execution.inc", "runtime/magel_pool.c", "runtime/magel_pool.h",
            "runtime/magel_allocator.hpp", "magel_runtime_allocator.hpp",
            "runtime/simple_ast.cpp", "CMakeLists.txt", "std/lang.mgl",
            "glue/std_records.hpp", "glue/std_native_record.hpp", "glue/std_file_api.hpp",
            "glue/std_big_number.hpp", "sqlite/sqlite3.c", "lztext/lztext.cpp",
            "miniz/miniz_tinfl.c", "template/engine.hpp", "dap/dap_server.cpp",
            "runtime/parts/artifact_binary.cpp", "semantics/associated_types.cpp",
            ".magel-generator-manifest",
        ]
        missing = [name for name in required if not (compiler / name).is_file()]
        if missing:
            raise AssertionError(f"compiler export is incomplete: {missing}")
        cmake = (compiler / "CMakeLists.txt").read_text(encoding="utf-8")
        for option in ("MAGEL_WITH_REFLECTION", "STACKVM_WITH_REFLECTION"):
            if not any(line.startswith(f"option({option} ") and line.endswith(" ON)")
                       for line in cmake.splitlines()):
                raise AssertionError(f"exported compiler must enable {option} by default")
        if "magel_host_records" not in cmake or "magel_vm_core" not in cmake:
            raise AssertionError("exported compiler must separate VM and host records")
        if "std::regex" not in (compiler / "RESULT.md").read_text(encoding="utf-8"):
            raise AssertionError("compiler generator must use the requested std::regex lexer")
        if (compiler / "std/lang.mgl").read_bytes() != (ROOT / "source/std/lang.mgl").read_bytes():
            raise AssertionError("compiler lang.mgl does not match the referenced language library")
        assert not (compiler / "utils.mgl").exists()
        assert not (compiler / "reflect.mgl").exists()
        (compiler / "keep.txt").write_text("user\n", encoding="utf-8")
        command([GENERATOR, "clear", "--out-dir", compiler])
        remaining = sorted(
            path.relative_to(compiler).as_posix()
            for path in compiler.rglob("*") if path.is_file()
        )
        if remaining != ["keep.txt"]:
            raise AssertionError(f"compiler clear left unexpected files: {remaining}")

        unsafe = WORK / "unsafe"
        unsafe.mkdir()
        victim = WORK / "victim.txt"
        victim.write_text("keep\n", encoding="utf-8")
        (unsafe / ".magel-generator-manifest").write_text(
            "# Magel generator files v1\n../victim.txt\n", encoding="utf-8",
        )
        command([GENERATOR, "clear", "--out-dir", unsafe], succeeds=False)
        if victim.read_text(encoding="utf-8") != "keep\n":
            raise AssertionError("unsafe compiler manifest escaped the output directory")

        command([GENERATOR, "vm", "--out-dir", WORK / "removed-vm"], succeeds=False)
        assert not (WORK / "removed-vm").exists()
    finally:
        shutil.rmtree(WORK, ignore_errors=True)
    print("Magel generator checks passed")


if __name__ == "__main__":
    main()
