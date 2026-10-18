"""MAGEL project packages: metadata, includes/imports, origins and cache invalidation."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ("magel.exe" if os.name == "nt" else "magel")


def run(args, success=True, cwd=ROOT, input=None):
    result = subprocess.run(list(map(str, args)), cwd=cwd, input=input, capture_output=True,
                            text=True, encoding="utf-8", env=dict(os.environ, MAGEL_LANG="zh"))
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result


def main():
    global COMPILER
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=COMPILER)
    COMPILER = parser.parse_args().magel.resolve()
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="magel-package-", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        project = work / "app"
        # 夹具与测试一起维护，不依赖已迁移的文档示例目录。
        shutil.copytree(ROOT / "tests/fixtures/package", project)
        manifest = project / "magel.toml"
        native = project / "include/native.c"
        native.write_text('#include <stdint.h>\n#ifdef _WIN32\n#define API __declspec(dllexport)\n#else\n#define API\n#endif\nAPI int32_t identity(int32_t n) { return n; }\n', encoding="utf-8")
        library = project / "include" / ("native.dll" if os.name == "nt" else "native.so")
        run(["gcc", "-shared", *(["-fPIC"] if os.name != "nt" else []), native, "-o", library])
        (project / "include/abi.inc").write_text('abi "./' + library.name + '" { int32 nativeIdentity(int32 n) = "identity"; }\n', encoding="utf-8")
        original = manifest.read_text(encoding="utf-8").replace("commands = [", "commands = [\n    '%include \"include/abi.inc\"',")
        manifest.write_text(original, encoding="utf-8")
        source = project / "src/main.mgl"
        prefix = work / "program"
        args = [COMPILER, 'run', source, "--out", prefix]
        source.write_text("/*\nmodule hidden;\nimport absent;\n*/\n" + source.read_text(encoding="utf-8").replace('    std::lang::println("magel-ok");', '    std::lang::assert(nativeIdentity((int32)7) == 7);\n    std::lang::println("magel-ok");'), encoding="utf-8")
        assert run(args).stdout == "magel-ok\n"
        emitted = [*args, "--emit", "--incremental"]
        first = run(emitted)
        assert library.as_posix() in prefix.with_suffix(".abi.cpp").read_text(encoding="utf-8")
        quad = prefix.with_suffix(".quad")
        stamp = quad.stat().st_mtime_ns
        cached = run(emitted)
        assert "增量缓存命中" in cached.stderr, cached.stderr
        assert quad.stat().st_mtime_ns == stamp
        dependencies = run([*args, "--deps"]).stdout.replace("\\", "/")
        for file in (manifest, source, project / "include/shared.inc", project / "modules/tools/math.mgl"):
            assert file.as_posix() in dependencies, dependencies
        # Metadata is part of the build signature, even if MAGEL statements are unchanged.
        manifest.write_text(original.replace('version = "0.1.0"', 'version = "0.1.1"'), encoding="utf-8")
        rebuilt = run(emitted)
        assert "增量缓存命中" not in rebuilt.stderr, rebuilt.stderr
        (project / "include/extra.inc").write_text('int64 extraValue() { return 2; }\n', encoding="utf-8")
        assert "增量缓存命中" not in run(emitted).stderr
        (project / "include/extra.inc").write_text('int64 extraValue() { return 1; }\n', encoding="utf-8")
        manifest.write_text(original, encoding="utf-8")
        assert run(args).stdout == "magel-ok\n"
        # The source file of a package include must survive composition and diagnostics.
        (project / "include/extra.inc").write_text('int64 extraValue( { return 1; }\n', encoding="utf-8")
        failed = run(args, success=False)
        assert "extra.inc" in failed.stderr and "第 1 行" in failed.stderr, failed.stderr
        (project / "include/extra.inc").write_text('int64 extraValue() { return 1; }\n', encoding="utf-8")
        manifest.write_text(original.replace('tools::math', 'missing::module'), encoding="utf-8")
        assert "missing::module" in run(args, success=False).stderr
        for invalid in (
            '[package]\nname = "a"\nname = "b"\n',
            '[magel]\ncommands = ["%define BAD 1"]\n',
            '[magel]\nimport = ["bad::"]\n',
            '[magel]\ninclude = ["missing.inc"]\n',
            '[other]\ninclude = []\n',
            '[package]\nversion = 42\n',
            '[magel]\ndebug = "yes"\n',
            '[magel]\nrun = "bogus"\n',
            '[magel]\ninclude = ["unfinished"\n',
        ):
            manifest.write_text(invalid, encoding="utf-8")
            assert run(args, success=False).stderr
        # Nearest project wins; standard input uses the project of its working directory.
        manifest.write_text(original, encoding="utf-8")
        stdin = run([COMPILER, "-I", ROOT, "--out", work / "stdin"], cwd=project,
                    input='int64 main() { std::lang::println("{}", packageValue()); return 0; }\n')
        assert stdin.stdout == "40\n", stdin.stdout + stdin.stderr
        nested = project / "src/nested"
        nested.mkdir()
        (nested / "magel.toml").write_text('[package]\nname="nested"\nversion="1.0.0"\nauthor="A"\n', encoding="utf-8")
        nested_source = nested / "main.mgl"
        nested_source.write_text('int64 main() { return 0; }\n', encoding="utf-8")
        assert run([COMPILER, 'run', nested_source, "--out", work / "nested"]).stdout == ""
        # 默认不带调试节（也就没有 .quad 路径），但函数名照旧在；`--debug` /
        # `[magel] debug = true` 才写调试节；`--no-debug` 是兼容别名（等于缺省）。
        binary = work / "binary"
        run([COMPILER, 'run', source, "--out", binary, "--emit", "--svmb"])
        stripped = binary.with_suffix(".svmb").read_bytes()
        assert b".quad" not in stripped, "默认输出仍带着 .quad 路径"
        assert b"main" in stripped, "调试节之外的名字不该被删"
        run([COMPILER, 'run', source, "--out", binary, "--emit", "--svmb", "--debug"])
        kept = binary.with_suffix(".svmb").read_bytes()
        assert b".quad" in kept and b"main" in kept
        assert len(kept) > len(stripped), (len(kept), len(stripped))
        manifest.write_text(original.replace("[magel]", "[magel]\ndebug = true"), encoding="utf-8")
        run([COMPILER, 'run', source, "--out", binary, "--emit", "--svmb"])
        assert binary.with_suffix(".svmb").read_bytes() == kept
        manifest.write_text(original, encoding="utf-8")
        run([COMPILER, 'run', source, "--out", binary, "--emit", "--svmb", "--no-debug"])
        assert binary.with_suffix(".svmb").read_bytes() == stripped
        # 已删除的配置键和命令行参数必须拒绝，不能静默产生明文字节码。
        manifest.write_text(original.replace("[magel]", "[magel]\nencrypt = true"), encoding="utf-8")
        rejected = run([COMPILER, 'svmb', source, "--out", binary], success=False)
        assert "unknown key: magel.encrypt" in rejected.stderr
        manifest.write_text(original, encoding="utf-8")
        rejected = run([COMPILER, 'svmb', source, "--out", binary, "--encrypt"], success=False)
        assert "--encrypt" in rejected.stderr
        # Verify stored package metadata independently of the MAGEL language frontend.
        probe = work / "metadata.cpp"
        probe.write_text('#include "magel_package.hpp"\n#include <iostream>\n'
                         'int main(int argc, char **argv) { magel_package::Project p; Memory::String e; '
                         'if (!magel_package::read(std::filesystem::u8path(argv[1]), p, e)) return 1; '
                         'std::cout << p.name << "\\n" << p.version << "\\n" << p.authors.at(0) << "\\n" '
                         '<< (p.debug ? "debug=1" : "debug=0") << "\\n" '
                         '<< p.run << "\\n"; }\n', encoding="utf-8")
        exe = work / ("metadata.exe" if os.name == "nt" else "metadata")
        run(["g++", "-std=c++17", "-I", ROOT / "source", "-I", ROOT / "library", probe, ROOT / "library/toml/toml.cpp", ROOT / "library/json/json.cpp", "-o", exe])
        assert run([exe, manifest]).stdout == "magel-demo\n0.1.0\n示例作者\ndebug=0\ninterpret\n"
        manifest.write_text(original.replace("[magel]", "[magel]\ndebug = true"), encoding="utf-8")
        assert run([exe, manifest]).stdout == "magel-demo\n0.1.0\n示例作者\ndebug=1\ninterpret\n"
        # `[magel] run` 是项目默认运行方式；`smvb` 拼写折到 `svmb`。
        manifest.write_text(original.replace("[magel]", '[magel]\nrun = "aot"'), encoding="utf-8")
        assert run([exe, manifest]).stdout == "magel-demo\n0.1.0\n示例作者\ndebug=0\naot\n"
        manifest.write_text(original.replace("[magel]", '[magel]\nrun = "smvb"'), encoding="utf-8")
        assert run([exe, manifest]).stdout == "magel-demo\n0.1.0\n示例作者\ndebug=0\nsvmb\n"
        manifest.write_text(original, encoding="utf-8")
    print("MAGEL package checks passed")


if __name__ == "__main__":
    main()
