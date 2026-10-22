"""验证模块搜索优先级、工程导入命令和文本包含指令拒绝行为。"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ("ChaLi.exe" if os.name == "nt" else "chali")


def run(args, success=True):
    result = subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result


def main():
    (ROOT / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="chali-package-paths-", dir=ROOT / "tmp") as directory:
        work = Path(directory)
        project, first, second = work / "project", work / "first", work / "second"
        for folder in (project / "src", first, second):
            folder.mkdir(parents=True)
        for folder, value in ((first, 20), (second, 90)):
            (folder / "settings.chl").write_text(f'module settings;\nint64 value() {{ return {value}; }}\n', encoding="utf-8")
        (first / "extra.chl").write_text('module extra;\nint64 value() { return 2; }\n', encoding="utf-8")
        source = project / "src/main.chl"
        source.write_text('module app;\nvoid main() { std::lang::println("{} {}", settings::value(), extra::value()); return; }\n', encoding="utf-8")
        manifest = project / "chali.toml"
        config = '''[package]
name = "paths-demo"
version = "0.1.0"
[chali]
import_paths = ['../first', '../second']
import = ['settings']
commands = ['import extra;']
'''
        manifest.write_text(config, encoding="utf-8")
        args = [COMPILER, 'run', source, "--out", work / "program"]
        assert run(args).stdout == "20 2\n"
        emitted = [*args, "--emit", "--incremental"]
        run(emitted)
        assert "增量缓存命中" in run(emitted).stderr
        manifest.write_text(config.replace("['../first', '../second']", "['../second', '../first']"), encoding="utf-8")
        assert "增量缓存命中" not in run(emitted).stderr
        assert run(args).stdout == "90 2\n"
        (second / "settings.chl").write_text('module settings;\nint64 value() { return 88; }\n', encoding="utf-8")
        assert "增量缓存命中" not in run(emitted).stderr
        assert run(args).stdout == "88 2\n"
        for invalid in ('include = ["missing.chl"]', 'commands = [\'%include "extra.chl"\']'):
            manifest.write_text('[chali]\n' + invalid + '\n', encoding="utf-8")
            run(args, False)
        manifest.write_text(config.replace('import_paths', 'search_paths'), encoding="utf-8")
        assert run(args).stdout == "20 2\n"
        deps = run([*args, "--deps"]).stdout.replace("\\", "/")
        assert (first / "settings.chl").as_posix() in deps
        assert (first / "extra.chl").as_posix() in deps
        plain = work / "plain"
        plain.mkdir()
        main = plain / "main.chl"
        main.write_text('import settings;\nvoid main() { std::lang::println("{}", settings::value()); return; }\n', encoding="utf-8")
        cli = [COMPILER, 'run', main, "--out", work / "cli"]
        assert run([*cli, "--import-dir", first]).stdout == "20\n"
        assert run([*cli, "-I", first]).stdout == "20\n"
        assert "找不到模块 settings" in run(cli, False).stderr
        for directive in ('include', 'include_once', 'import'):
            main.write_text(f'%{directive} "missing.chl"\nvoid main() {{ return; }}\n', encoding="utf-8")
            error = run(cli, False).stderr
            assert '不支持文本包含指令' in error and 'import' in error, error
    print("CHALI module paths and removed inclusion checks passed")


if __name__ == "__main__":
    main()
