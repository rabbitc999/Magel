"""CHALI project packages: metadata, includes/imports, origins and cache invalidation."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali')

def run(args, success=True, cwd=ROOT, input=None):
    result = subprocess.run(list(map(str, args)), cwd=cwd, input=input, capture_output=True, text=True, encoding='utf-8', env=dict(os.environ, CHALI_LANG='zh'))
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result

def main():
    global COMPILER
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=COMPILER)
    COMPILER = parser.parse_args().chali.resolve()
    (ROOT / 'tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='chali-package-', dir=ROOT / 'tmp') as directory:
        work = Path(directory)
        project = work / 'app'
        shutil.copytree(ROOT / 'tests/fixtures/package', project)
        manifest = project / 'chali.toml'
        native = project / 'include/native.c'
        native.write_text('#include <stdint.h>\n#ifdef _WIN32\n#define API __declspec(dllexport)\n#else\n#define API\n#endif\nAPI int32_t identity(int32_t n) { return n; }\n', encoding='utf-8')
        library = project / 'include' / ('native.dll' if os.name == 'nt' else 'native.so')
        run(['gcc', '-shared', *(['-fPIC'] if os.name != 'nt' else []), native, '-o', library])
        (project / 'include/nativeapi.chl').write_text('module nativeapi;\nabi "./' + library.name + '" { int32 nativeIdentity(int32 n) = "identity"; }\n', encoding='utf-8')
        original = manifest.read_text(encoding='utf-8').replace('commands = [', 'commands = [\n    \'import nativeapi;\',')
        manifest.write_text(original, encoding='utf-8')
        source = project / 'src/main.chl'
        prefix = work / 'program'
        args = [COMPILER, 'run', source, '--out', prefix]
        source.write_text('/*\nmodule hidden;\nimport absent;\n*/\n' + source.read_text(encoding='utf-8').replace('    std::lang::println("chali-ok");', '    std::lang::assert(nativeapi::nativeIdentity((int32)7) == 7);\n    std::lang::println("chali-ok");'), encoding='utf-8')
        assert run(args).stdout == 'chali-ok\n'
        emitted = [*args, '--emit', '--incremental']
        first = run(emitted)
        assert library.as_posix() in prefix.with_suffix('.abi.cpp').read_text(encoding='utf-8')
        quad = prefix.with_suffix('.quad')
        stamp = quad.stat().st_mtime_ns
        cached = run(emitted)
        assert '增量缓存命中' in cached.stderr, cached.stderr
        assert quad.stat().st_mtime_ns == stamp
        dependencies = run([*args, '--deps']).stdout.replace('\\', '/')
        for file in (manifest, source, project / 'include/shared.chl', project / 'modules/tools/math.chl'):
            assert file.as_posix() in dependencies, dependencies
        manifest.write_text(original.replace('version = "0.1.0"', 'version = "0.1.1"'), encoding='utf-8')
        rebuilt = run(emitted)
        assert '增量缓存命中' not in rebuilt.stderr, rebuilt.stderr
        (project / 'include/extra.chl').write_text('module extra;\nint64 extraValue() { return 2; }\n', encoding='utf-8')
        assert '增量缓存命中' not in run(emitted).stderr
        (project / 'include/extra.chl').write_text('module extra;\nint64 extraValue() { return 1; }\n', encoding='utf-8')
        manifest.write_text(original, encoding='utf-8')
        assert run(args).stdout == 'chali-ok\n'
        (project / 'include/extra.chl').write_text('module extra;\nint64 extraValue( { return 1; }\n', encoding='utf-8')
        failed = run(args, success=False)
        assert 'extra.chl' in failed.stderr and '第 2 行' in failed.stderr, failed.stderr
        (project / 'include/extra.chl').write_text('module extra;\nint64 extraValue() { return 1; }\n', encoding='utf-8')
        manifest.write_text(original.replace('tools::math', 'missing::module'), encoding='utf-8')
        assert 'missing::module' in run(args, success=False).stderr
        for invalid in ('[package]\nname = "a"\nname = "b"\n', '[chali]\ncommands = ["%define BAD 1"]\n', '[chali]\nimport = ["bad::"]\n', '[chali]\ninclude = ["missing.inc"]\n', '[other]\ninclude = []\n', '[package]\nversion = 42\n', '[chali]\ndebug = "yes"\n', '[chali]\nrun = "bogus"\n', '[chali]\ninclude = ["unfinished"\n'):
            manifest.write_text(invalid, encoding='utf-8')
            assert run(args, success=False).stderr
        manifest.write_text(original, encoding='utf-8')
        stdin = run([COMPILER, '-I', ROOT, '--out', work / 'stdin'], cwd=project, input='int64 main() { std::lang::println("{}", packageValue()); return 0; }\n')
        assert stdin.stdout == '40\n', stdin.stdout + stdin.stderr
        nested = project / 'src/nested'
        nested.mkdir()
        (nested / 'chali.toml').write_text('[package]\nname="nested"\nversion="1.0.0"\nauthor="A"\n', encoding='utf-8')
        nested_source = nested / 'main.chl'
        nested_source.write_text('int64 main() { return 0; }\n', encoding='utf-8')
        assert run([COMPILER, 'run', nested_source, '--out', work / 'nested']).stdout == ''
        binary = work / 'binary'
        run([COMPILER, 'run', source, '--out', binary, '--emit', '--svmb'])
        stripped = binary.with_suffix('.svmb').read_bytes()
        assert b'.quad' not in stripped, '默认输出仍带着 .quad 路径'
        assert b'main' in stripped, '调试节之外的名字不该被删'
        run([COMPILER, 'run', source, '--out', binary, '--emit', '--svmb', '--debug'])
        kept = binary.with_suffix('.svmb').read_bytes()
        assert b'.quad' in kept and b'main' in kept
        assert len(kept) > len(stripped), (len(kept), len(stripped))
        manifest.write_text(original.replace('[chali]', '[chali]\ndebug = true'), encoding='utf-8')
        run([COMPILER, 'run', source, '--out', binary, '--emit', '--svmb'])
        assert binary.with_suffix('.svmb').read_bytes() == kept
        manifest.write_text(original, encoding='utf-8')
        run([COMPILER, 'run', source, '--out', binary, '--emit', '--svmb', '--no-debug'])
        assert binary.with_suffix('.svmb').read_bytes() == stripped
        manifest.write_text(original.replace('[chali]', '[chali]\nencrypt = true'), encoding='utf-8')
        rejected = run([COMPILER, 'svmb', source, '--out', binary], success=False)
        assert 'unknown key: chali.encrypt' in rejected.stderr
        manifest.write_text(original, encoding='utf-8')
        rejected = run([COMPILER, 'svmb', source, '--out', binary, '--encrypt'], success=False)
        assert '--encrypt' in rejected.stderr
        manifest.write_text(original + '\n[embedded]\nenabled = true\nmax_memory = 1024\nrestrict_modules = ["std::net"]\n', encoding='utf-8')
        rejected = run([COMPILER, 'run', source, '--out', binary], success=False)
        assert 'unknown or repeated table: embedded' in rejected.stderr, rejected.stderr
        manifest.write_text(original, encoding='utf-8')
        probe = work / 'metadata.cpp'
        probe.write_text('#include "chali_package.hpp"\n#include <iostream>\nint main(int argc, char **argv) { chali_package::Project p; Memory::String e; if (!chali_package::read(std::filesystem::u8path(argv[1]), p, e)) return 1; std::cout << p.name << "\\n" << p.version << "\\n" << p.authors.at(0) << "\\n" << (p.debug ? "debug=1" : "debug=0") << "\\n" << p.run << "\\n"; }\n', encoding='utf-8')
        exe = work / ('metadata.exe' if os.name == 'nt' else 'metadata')
        run(['g++', '-std=c++17', '-I', ROOT / 'source', '-I', ROOT / 'library', probe, ROOT / 'source/chali_package.cpp', ROOT / 'library/toml/toml.cpp', ROOT / 'library/toml/toml_source.cpp', ROOT / 'library/json/json.cpp', '-o', exe])
        assert run([exe, manifest]).stdout == 'chali-demo\n0.1.0\n示例作者\ndebug=0\ninterpret\n'
        manifest.write_text(original.replace('[chali]', '[chali]\ndebug = true'), encoding='utf-8')
        assert run([exe, manifest]).stdout == 'chali-demo\n0.1.0\n示例作者\ndebug=1\ninterpret\n'
        manifest.write_text(original.replace('[chali]', '[chali]\nrun = "smvb"'), encoding='utf-8')
        assert run([exe, manifest]).stdout == 'chali-demo\n0.1.0\n示例作者\ndebug=0\nsvmb\n'
        manifest.write_text(original, encoding='utf-8')
    print('CHALI package checks passed')
if __name__ == '__main__':
    main()
