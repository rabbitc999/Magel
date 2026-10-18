"""MAGEL 统一工具链：子命令 / --mode / magel run 的四种运行方式与默认清理。"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ("magel.exe" if os.name == "nt" else "magel")

SOURCE = """int64 main() {
    int64 sum = 0;
    for (int64 i = 1; i <= 5; i = i + 1) { sum = sum + i; }
    std::lang::println("sum={}", sum);
    return 0;
}
"""
EXPECTED = "sum=15\n"


def run(args, success=True, cwd=ROOT, timeout=900):
    result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=timeout)
    assert (result.returncode == 0) == success, (args, result.returncode, result.stdout, result.stderr)
    return result


def run_exe(path, success=True):
    result = subprocess.run([str(path)], capture_output=True, text=True, encoding="utf-8",
                            errors="replace", timeout=120)
    assert (result.returncode == 0) == success, (path, result.returncode, result.stdout, result.stderr)
    return result


def write_project(root, run_mode):
    (root / "src").mkdir(parents=True, exist_ok=True)
    (root / "src" / "main.mgl").write_text(SOURCE, encoding="utf-8")
    (root / "magel.toml").write_text('[package]\nname = "mt"\nversion = "0.1.0"\n\n'
                                    f'[magel]\nrun = "{run_mode}"\n', encoding="utf-8")


def main():
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="magel-tool-", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        source = work / "main.mgl"
        source.write_text(SOURCE, encoding="utf-8")

        # 1) 内部解释运行：缺省 / `run` 子命令 / `--mode interpret` 全部一致。
        prefix = work / "prefix"
        interpret = run([COMPILER, 'run', source, "--out", prefix])
        assert interpret.stdout == EXPECTED, interpret.stdout
        assert run([COMPILER, "run", source]).stdout == EXPECTED
        assert run([COMPILER, "interpret", source]).stdout == EXPECTED
        assert run([COMPILER, "--mode", "interpret", source]).stdout == EXPECTED
        assert run([COMPILER, "--run-mode=interpret", source]).stdout == EXPECTED

        # 2) `.svmb` 导出：只写产物，不执行。
        svmb = work / "sv"
        exported = run([COMPILER, "svmb", source, "--out", svmb])
        assert exported.stdout == "", exported.stdout
        assert svmb.with_suffix(".svmb").exists()
        assert run([COMPILER, "--mode", "smvb", source, "--out", work / "sv2"]).stdout == ""
        assert (work / "sv2.svmb").exists()

        # 3) AOT 可运行程序：--out 是 exe；默认删除中间文件（含统一工具清单）。
        vm = work / "progvm"
        run([COMPILER, "aot", source, "--out", vm, "--opt-level=0"], timeout=1800)
        assert vm.exists()
        assert run_exe(vm).stdout == EXPECTED
        assert not (work / "progvm.quad").exists()
        assert not (work / ".magel-tool-manifest").exists()

        # 4) --keep-intermediate 保留；clear 只清登记过的中间文件，exe 与用户文件不动。
        kept = work / "progkeep"
        user_file = work / "progkeep.log"
        user_file.write_text("user\n", encoding="utf-8")
        run([COMPILER, "aot", source, "--out", kept, "--keep-intermediate", "--opt-level=0"], timeout=1800)
        assert (work / "progkeep.quad").exists()
        assert (work / ".magel-tool-manifest").exists()
        run([COMPILER, "clear", "--out-dir", work])
        assert kept.exists()
        assert user_file.read_text(encoding="utf-8") == "user\n"
        assert not (work / "progkeep.quad").exists()
        assert not (work / "progkeep.aot.cpp").exists()
        assert not (work / ".magel-tool-manifest").exists()

        # `--clean` 是 `clear` 的等价拼写（不带源文件）。
        kept2 = work / "progclean"
        run([COMPILER, "aot", source, "--out", kept2, "--keep-intermediate", "--opt-level=0"], timeout=1800)
        assert (work / "progclean.quad").exists()
        run([COMPILER, "--clean", "--out-dir", work])
        assert kept2.exists()
        assert not (work / "progclean.quad").exists()

        # 5) AOT 可运行程序：输出与解释一致，默认清理中间文件。
        aot = work / "progaot"
        run([COMPILER, "aot", source, "--out", aot, "--opt-level=0"], timeout=1800)
        assert aot.exists()
        assert run_exe(aot).stdout == EXPECTED
        assert not (work / "progaot.quad").exists()
        assert not (work / "progaot.aot.cpp").exists()
        # 默认不带调试信息；`--debug` 才写回调试信息表（产物里含 .quad 路径）。
        assert b".quad" not in aot.read_bytes()
        aot_debug = work / "progaotdbg"
        run([COMPILER, "aot", source, "--out", aot_debug, "--opt-level=0", "--debug"], timeout=1800)
        assert run_exe(aot_debug).stdout == EXPECTED
        assert b".quad" in aot_debug.read_bytes()

        # 6) magel run：项目缺省运行方式生效，命令行覆盖优先。
        project = work / "projvm"
        write_project(project, "aot")
        run([COMPILER, "--out", "prog", "--opt-level=0", "src/main.mgl"], cwd=project, timeout=1800)
        assert run_exe(project / "prog").stdout == EXPECTED
        assert run([COMPILER, 'run', "src/main.mgl", "--mode", "interpret"], cwd=project).stdout == EXPECTED
        assert run([COMPILER, "run", "src/main.mgl"], cwd=project).stdout == EXPECTED

        project_svmb = work / "projsvmb"
        write_project(project_svmb, "smvb")
        run([COMPILER, "--out", "out", "src/main.mgl"], cwd=project_svmb)
        assert (project_svmb / "out.svmb").exists()

        project_interpret = work / "projinterp"
        write_project(project_interpret, "interpret")
        assert run([COMPILER, 'run', "src/main.mgl"], cwd=project_interpret).stdout == EXPECTED

        # 7) 清单安全：`..` 越界、绝对路径、保留名都拒绝，且不碰目录外文件。
        outside = work.parent / "outside-sentinel.txt"
        outside.write_text("sentinel\n", encoding="utf-8")
        unsafe = work / "unsafe"
        unsafe.mkdir()
        (unsafe / ".magel-tool-manifest").write_text(
            "# Magel toolchain files v1\n../outside-sentinel.txt\n", encoding="utf-8")
        run([COMPILER, "clear", "--out-dir", unsafe], success=False)
        assert outside.read_text(encoding="utf-8") == "sentinel\n"
        outside_symlink = unsafe / "link"
        try:
            outside_symlink.symlink_to(unsafe, target_is_directory=True)
            run([COMPILER, "clear", "--out-dir", outside_symlink], success=False)
        except (OSError, NotImplementedError):
            pass
        outside.unlink()

    print("MAGEL toolchain checks passed")


if __name__ == "__main__":
    main()
