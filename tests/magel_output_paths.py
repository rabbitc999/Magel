"""MAGEL 导出路径 / 工具链选项：cpp_output / execute_output / opt_level / make / gcc / g++。

覆盖 magel-emit 的 `--cpp-output` / `--cpp-output-dir` / `--execute-output` / `--make`
与 `magel.toml` 里同名的 `[magel]` 键，确认「导出程序时默认不写代码」这条语义，
并验证单文件生成器（`magel-emit` 找不到仓库资产时回退到内嵌资产表）。
"""
from pathlib import Path
import os
import json
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


def run(args, success=True, cwd=ROOT, timeout=1800):
    result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=timeout)
    assert (result.returncode == 0) == success, (args, result.returncode, result.stdout, result.stderr)
    return result


def run_exe(path, success=True):
    result = subprocess.run([str(path)], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=120)
    assert (result.returncode == 0) == success, (path, result.returncode, result.stdout, result.stderr)
    return result


def main():
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="magel-outpaths-", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        source = work / "main.mgl"
        source.write_text(SOURCE, encoding="utf-8")

        # 1) 缺省：AOT 导出只留可执行文件，不写/不留 C++ 源码。
        plain = work / "plain"
        run([COMPILER, "aot", source, "--out", plain, "--opt-level=0"])
        assert plain.exists() and run_exe(plain).stdout == EXPECTED
        assert not (work / "plain.aot.cpp").exists()
        assert not (work / ".magel-aot-manifest").exists()

        # 2) `--cpp-output-dir DIR`：把生成的 C++ 写到指定目录且保留（交付物，不是中间文件）。
        cppdir = work / "gen"
        cppdir.mkdir()
        with_cpp = work / "withcpp"
        run([COMPILER, "aot", source, "--out", with_cpp, "--opt-level=0",
             "--cpp-output-dir", cppdir])
        assert run_exe(with_cpp).stdout == EXPECTED
        assert (cppdir / "withcpp.aot.cpp").exists()
        assert not (work / "withcpp.aot.cpp").exists()
        # 保留的源码里是基本块 + goto 的 AOT 形态，不是大 switch 解释循环。
        text = (cppdir / "withcpp.aot.cpp").read_text(encoding="utf-8", errors="replace")
        assert "基本块 + 局部变量 + goto" in text and "goto L_" in text

        # 4) `--execute-output FILE`：可执行文件写到指定路径（目录不存在也会建）。
        exe_path = work / "build" / "the-app"
        run([COMPILER, "aot", source, "--execute-output", exe_path, "--opt-level=0"])
        assert exe_path.exists() and run_exe(exe_path).stdout == EXPECTED

        # 5) magel.toml：run=aot + opt_level + cpp_output(_dir) + execute_output 一起生效。
        project = work / "proj"
        (project / "src").mkdir(parents=True)
        (project / "src" / "main.mgl").write_text(SOURCE, encoding="utf-8")
        (project / "magel.toml").write_text(
            '[package]\nname = "outpaths"\nversion = "0.1.0"\n\n'
            '[magel]\nrun = "aot"\nopt_level = "1"\ncpp_output = true\n'
            'cpp_output_dir = "gen"\nexecute_output = "build/app"\n', encoding="utf-8")
        run([COMPILER, "--no-debug", "src/main.mgl"], cwd=project)
        assert run_exe(project / "build" / "app").stdout == EXPECTED
        assert (project / "gen" / "app.aot.cpp").exists()
        # 编译日志属于中间文件（`--cpp-output` 只保留源码），默认清理会把 `.aot.log` 删掉。
        assert not (project / "build" / "app.aot.log").exists()
        # 优化级别透传：再跑一次并保留中间文件，编译日志里应出现 -O1。
        kept = project / "build" / "kept"
        run([COMPILER, "aot", "src/main.mgl", "--out", kept, "--opt-level=1",
             "--keep-intermediate"], cwd=project)
        assert "-O1" in (project / "build" / "kept.aot.log").read_text(encoding="utf-8")
        assert run_exe(kept).stdout == EXPECTED

        # 6) magel 里 cpp_output 缺省关闭：不写代码。
        project2 = work / "proj2"
        (project2 / "src").mkdir(parents=True)
        (project2 / "src" / "main.mgl").write_text(SOURCE, encoding="utf-8")
        (project2 / "magel.toml").write_text(
            '[package]\nname = "outpaths2"\nversion = "0.1.0"\n\n'
            '[magel]\nrun = "aot"\nopt_level = "0"\n', encoding="utf-8")
        run([COMPILER, "--no-debug", "src/main.mgl"], cwd=project2)
        assert run_exe(project2 / "main").stdout == EXPECTED
        assert not (project2 / "main.aot.cpp").exists()

        # 7) `--make` 驱动构建；`--gcc` / `--g++` 是 `--cc` / `--cxx` 的别名。
        make = shutil.which("make")
        gxx = shutil.which("g++")
        gcc = shutil.which("gcc")
        if not (make and gxx and gcc):
            print("MAGEL output path checks passed（跳过 --make / gcc / g++：工具链不全）")
            return
        made = work / "made"
        run([COMPILER, "aot", source, "--out", made, "--opt-level=1", "--debug",
             "--keep-intermediate", "--make", make, "--gcc", gcc, "--g++", gxx])
        assert run_exe(made).stdout == EXPECTED
        makefile = (work / "made.aot.mk")
        assert makefile.exists(), "`--keep-intermediate` 应保留构建脚本"
        mk_text = makefile.read_text(encoding="utf-8")
        assert "CXX = " + gxx in mk_text and "CC = " + gcc in mk_text
        log_text = (work / "made.aot.log").read_text(encoding="utf-8")
        assert "构建驱动：make" in log_text and "make 命令：" in log_text
        assert "-O1" in mk_text and "-O1" in log_text
        # 缺省（不带 --keep-intermediate）：构建脚本也是中间文件，编译成功后删除。
        made2 = work / "made2"
        run([COMPILER, "aot", source, "--out", made2, "--opt-level=0", "--make", make])
        assert run_exe(made2).stdout == EXPECTED
        assert not (work / "made2.aot.mk").exists()
        assert not (work / "made2.aot.log").exists()
        # 8) magel：make / gcc / g++（外加 cxx 别名）都能写，且被构建设置使用。
        project3 = work / "proj3"
        (project3 / "src").mkdir(parents=True)
        (project3 / "src" / "main.mgl").write_text(SOURCE, encoding="utf-8")
        (project3 / "magel.toml").write_text(
            '[package]\nname = "outpaths3"\nversion = "0.1.0"\n\n'
            '[magel]\nrun = "aot"\nopt_level = "2"\n'
            'make = %s\ngcc = %s\ng++ = %s\n' % tuple(json.dumps(path) for path in (make, gcc, gxx)), encoding="utf-8")
        run([COMPILER, "--keep-intermediate", "src/main.mgl"], cwd=project3)
        assert run_exe(project3 / "main").stdout == EXPECTED
        assert "-O2" in (project3 / "main.aot.mk").read_text(encoding="utf-8")

        # 9) 单文件生成器：把 magel-emit 拷到没有仓库资产的目录，缺 --vm-dir 也能导出。
        emitter_name = "magel-emit.exe" if os.name == "nt" else "magel-emit"
        built_emitter = ROOT / "build" / "magel-emit" / emitter_name
        if not built_emitter.exists():
        print("MAGEL output path checks passed（跳过单文件生成器：先 make -C magel-emit）")
            return
        tools = work / "tools"
        tools.mkdir()
        emitter = tools / emitter_name
        shutil.copy2(built_emitter, emitter)
        quad = work / "single"
        run([COMPILER, 'run', source, "--emit", "--out", quad])
        single = work / "single-app"
        run([emitter, "--quad", str(quad) + ".quad", "--data", str(quad) + ".data.bin",
             "--layout", str(quad) + ".layout", "--target", "cpp-aot",
             "--out", single, "--opt-level=0"])
        assert run_exe(single).stdout == EXPECTED

    print("MAGEL output path checks passed")


if __name__ == "__main__":
    main()
