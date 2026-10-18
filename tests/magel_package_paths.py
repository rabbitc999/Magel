# Independent include/import roots, precedence, CLI flags and cache changes.
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ("magel.exe" if os.name == "nt" else "magel")


def run(args, success=True):
    result = subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result


def main():
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="magel-package-paths-", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        project = work / "project"
        headers, second, modules = work / "headers", work / "second", work / "libraries"
        for folder in (project / "src", headers, second, modules / "tools"):
            folder.mkdir(parents=True)
        (headers / "settings.inc").write_text('%define EXTERNAL_READY 1\nint64 configuredValue() { return 20; }\n', encoding="utf-8")
        (second / "settings.inc").write_text('%define EXTERNAL_READY 1\nint64 configuredValue() { return 90; }\n', encoding="utf-8")
        (headers / "other.inc").write_text('int64 commandsValue() { return 2; }\n', encoding="utf-8")
        (headers / "local.inc").write_text('int64 sourceValue() { return 3; }\n', encoding="utf-8")
        (headers / "includeOnly.mgl").write_text('module includeOnly; int64 value() { return 9; }\n', encoding="utf-8")
        (modules / "only.inc").write_text('int64 wrongDirectory() { return 0; }\n', encoding="utf-8")
        (modules / "tools/ops.mgl").write_text('module tools::ops;\nint64 value() { return 5; }\n', encoding="utf-8")
        source = project / "src/main.mgl"
        source.write_text('module app;\n%include "local.inc"\n%if EXTERNAL_READY == 1\n'
                          'int64 main() { std::lang::println("{} {} {} {}", configuredValue(), commandsValue(), sourceValue(), tools::ops::value()); return 0; }\n'
                          '%else\n%error external include was not found\n%endif\n', encoding="utf-8")
        manifest = project / "magel.toml"
        config = '''[package]
name = "paths-demo"
version = "0.1.0"
[magel]
include_paths = ['../headers', '../second']
import_paths = ['../libraries']
include = ['settings.inc']
import = ['tools::ops']
commands = ['%include "other.inc"']
'''
        manifest.write_text(config, encoding="utf-8")
        args = [COMPILER, 'run', source, "--out", work / "program"]
        assert run(args).stdout == "20 2 3 5\n"
        emitted = [*args, "--emit", "--incremental"]
        run(emitted)
        assert "增量缓存命中" in run(emitted).stderr
        reverse = config.replace("['../headers', '../second']", "['../second', '../headers']")
        manifest.write_text(reverse, encoding="utf-8")
        assert "增量缓存命中" not in run(emitted).stderr
        assert run(args).stdout == "90 2 3 5\n"
        (project / "src/local.inc").write_text('int64 sourceValue() { return 8; }\n', encoding="utf-8")
        (project / "settings.inc").write_text('%define EXTERNAL_READY 1\nint64 configuredValue() { return 7; }\n', encoding="utf-8")
        assert run(args).stdout == "7 2 8 5\n"
        (project / "src/local.inc").unlink()
        (project / "settings.inc").unlink()
        (second / "settings.inc").write_text('%define EXTERNAL_READY 1\nint64 configuredValue() { return 88; }\n', encoding="utf-8")
        assert "增量缓存命中" not in run(emitted).stderr
        assert run(args).stdout == "88 2 3 5\n"
        manifest.write_text(config.replace("'tools::ops'", "'includeOnly'"), encoding="utf-8")
        assert "找不到模块 includeOnly" in run(args, False).stderr
        manifest.write_text(config.replace('other.inc', 'only.inc'), encoding="utf-8")
        assert "only.inc" in run(args, False).stderr
        shared = config.replace("include_paths = ['../headers', '../second']\nimport_paths = ['../libraries']", "search_paths = ['../headers', '../libraries']")
        manifest.write_text(shared, encoding="utf-8")
        assert run(args).stdout == "20 2 3 5\n"
        deps = run([*args, "--deps"]).stdout.replace("\\", "/")
        assert (headers / "settings.inc").as_posix() in deps
        assert (modules / "tools/ops.mgl").as_posix() in deps
        plain = work / "plain"
        plain.mkdir()
        main = plain / "main.mgl"
        main.write_text('import tools::ops;\n%include "local.inc"\n'
                        'int64 main() { std::lang::println("{} {}", sourceValue(), tools::ops::value()); return 0; }\n', encoding="utf-8")
        cli = [COMPILER, 'run', main, "--out", work / "cli"]
        assert run([*cli, "--include-path", headers, "--import-dir", modules]).stdout == "3 5\n"
        assert run([*cli, "-I", headers, "-I", modules]).stdout == "3 5\n"
        assert "找不到模块 tools::ops" in run([*cli, "--include-path", headers], False).stderr
        assert "local.inc" in run([*cli, "--import-dir", modules], False).stderr
    print("MAGEL package path checks passed")


if __name__ == "__main__":
    main()
