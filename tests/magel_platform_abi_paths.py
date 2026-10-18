"""MAGEL platform defaults, user conditions and ABI separator normalization."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ("magel.exe" if os.name == "nt" else "magel")


def run(args, cwd=ROOT):
    result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True,
                            text=True, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    return result


def main():
    options = argparse.ArgumentParser()
    options.add_argument("--export", action="store_true", help="compile and run a C++ export from a different working directory")
    options.add_argument("--compiler", type=Path, default=COMPILER)
    arguments = options.parse_args()
    export = arguments.export
    compiler = arguments.compiler.resolve()
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="magel-platform-", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        folder = work / "native paths"
        folder.mkdir()
        native = folder / "native.c"
        native.write_text('#include <stdint.h>\n'
                          '#ifdef _WIN32\n#define API __declspec(dllexport)\n'
                          '#else\n#define API\n#endif\n'
                          'API int32_t identity(int32_t n) { return n; }\n', encoding="utf-8")
        library = folder / ("native.dll" if os.name == "nt" else "native.so")
        run(["gcc", "-shared", *(["-fPIC"] if os.name != "nt" else []), native, "-o", library])
        paths = [library.as_posix(), str(library).replace("/", "\\"),
                 str(library).replace("/", "\\").replace("\\", "\\\\"),
                 library.relative_to(work).as_posix().replace("/", "\\")]
        for index, path in enumerate(paths):
            source = work / f"path-{index}.mgl"
            source.write_text('abi "' + path + '" { int32 identity(int32 n) = "identity"; }\n'
                              'int64 main() { std::lang::println("{}", identity((int32)7)); return 0; }\n',
                              encoding="utf-8")
            prefix = work / f"path-{index}"
            result = run([compiler, 'run', source, "--out", prefix])
            assert result.stdout == "7\n" and not result.stderr, result.stdout + result.stderr
            run([compiler, 'run', source, "--emit", "--out", prefix])
            glue = prefix.with_suffix(".abi.cpp").read_text(encoding="utf-8")
            assert library.as_posix() in glue, glue

        # ABI declarations belong to their physical source file, even when
        # modules/includes are expanded into a different root compilation unit.
        nested = work / "nested"
        nested.mkdir()
        signature = 'abi "../native paths/' + library.name + '" { int32 identity(int32 n) = "identity"; }\n'
        (nested / "api.mgl").write_text('module nested::api;\n' + signature, encoding="utf-8")
        (nested / "api.inc").write_text(signature, encoding="utf-8")
        imported = work / "imported.mgl"
        imported.write_text('import nested::api;\nint64 main() { std::lang::println("{}", nested::api::identity((int32)8)); return 0; }\n', encoding="utf-8")
        included = work / "included.mgl"
        included.write_text('%include "nested/api.inc"\nint64 main() { std::lang::println("{}", identity((int32)8)); return 0; }\n', encoding="utf-8")
        for source in (imported, included):
            prefix = work / source.stem
            result = run([compiler, 'run', source, "--out", prefix])
            assert result.stdout == "8\n", result.stdout + result.stderr
            run([compiler, 'run', source, "--emit", "--out", prefix])
            assert library.as_posix() in prefix.with_suffix(".abi.cpp").read_text(encoding="utf-8")
        if export:
            emitter = ROOT / "build" / "magel-emit" / ("magel-emit.exe" if os.name == "nt" else "magel-emit")
            prefix = work / "imported"
            output = work / "export"
            executable = output / ("app.exe" if os.name == "nt" else "app")
            run([emitter, "--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
                 "--layout", prefix.with_suffix(".layout"), "--target", "cpp-aot", "--out", executable])
            assert run([executable], cwd=nested).stdout == "8\n"

        raw = work / "raw.mgl"
        raw.write_text('abi "./native paths/' + library.name + '" { int32 identity(int32 n) = "identity"; }\n'
                       'int64 main() { std::lang::println("{}", identity((int32)8)); return 0; }\n', encoding="utf-8")
        assert run([compiler, 'run', raw, "--raw", "--out", work / "raw"]).stdout == "8\n"

        module = work / "probe.mgl"
        module.write_text('module probe;\n%if MAGEL_OS == other && MAGEL_BITS == 32 && CUSTOM == 9\n'
                          'int64 value() { return 9; }\n%else\nint64 value() { return 1; }\n%endif\n',
                          encoding="utf-8")
        source = work / "platform.mgl"
        source.write_text('module app;\nimport probe;\nint64 main() {\n'
                          '%if MAGEL_OS == windows\nlang::println("windows");\n'
                          '%elif MAGEL_OS == linux\nlang::println("linux");\n'
                          '%elif MAGEL_OS == macos\nlang::println("macos");\n'
                          '%else\nlang::println("other");\n%endif\n'
                          '%if MAGEL_BITS == 64\nlang::println("64");\n'
                          '%elif MAGEL_BITS == 32\nlang::println("32");\n'
                          '%else\n%error unexpected bits\n%endif\n'
                          'std::lang::println("{}", probe::value()); return 0; }\n', encoding="utf-8")
        default = run([compiler, 'run', source, "--out", work / "default"])
        expected_os = "windows" if os.name == "nt" else "macos" if __import__("sys").platform == "darwin" else "linux"
        lines = default.stdout.splitlines()
        assert lines[0] == expected_os and lines[1] in ("32", "64") and lines[2] == "1", default.stdout
        override = run([compiler, 'run', source, "-DMAGEL_OS=other", "-DMAGEL_BITS=32", "-DCUSTOM=9",
                        "--out", work / "override"])
        assert override.stdout == "other\n32\n9\n", override.stdout
        include = work / "conditions.inc"
        include.write_text('%if USER_VALUE == 42 && MAGEL_OS == other && MAGEL_BITS == 32\n'
                           'int64 selected() { return 42; }\n%else\n%error user definitions missing\n%endif\n', encoding="utf-8")
        source.write_text('%define MAGEL_OS other\n%define MAGEL_BITS 32\n%define USER_VALUE 42\n'
                          '%include "conditions.inc"\n'
                          'int64 main() { std::lang::println("{}", selected()); return 0; }\n', encoding="utf-8")
        user = run([compiler, 'run', source, "--out", work / "user"])
        assert user.stdout == "42\n", user.stdout
    print("MAGEL platform and ABI path checks passed")


if __name__ == "__main__":
    main()
