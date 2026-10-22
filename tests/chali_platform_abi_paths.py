"""CHALI platform defaults, user conditions and ABI separator normalization."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali')

def run(args, cwd=ROOT):
    result = subprocess.run(list(map(str, args)), cwd=cwd, capture_output=True, text=True, encoding='utf-8')
    assert result.returncode == 0, result.stdout + result.stderr
    return result

def main():
    options = argparse.ArgumentParser()
    options.add_argument('--compiler', type=Path, default=COMPILER)
    arguments = options.parse_args()
    compiler = arguments.compiler.resolve()
    (ROOT / 'tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='chali-platform-', dir=ROOT / 'tmp') as directory:
        work = Path(directory)
        folder = work / 'native paths'
        folder.mkdir()
        native = folder / 'native.c'
        native.write_text('#include <stdint.h>\n#ifdef _WIN32\n#define API __declspec(dllexport)\n#else\n#define API\n#endif\nAPI int32_t identity(int32_t n) { return n; }\n', encoding='utf-8')
        library = folder / ('native.dll' if os.name == 'nt' else 'native.so')
        run(['gcc', '-shared', *(['-fPIC'] if os.name != 'nt' else []), native, '-o', library])
        paths = [library.as_posix(), str(library).replace('/', '\\'), str(library).replace('/', '\\').replace('\\', '\\\\'), library.relative_to(work).as_posix().replace('/', '\\')]
        for index, path in enumerate(paths):
            source = work / f'path-{index}.chl'
            source.write_text('abi "' + path + '" { int32 identity(int32 n) = "identity"; }\nvoid main() { std::lang::println("{}", identity((int32)7)); return; }\n', encoding='utf-8')
            prefix = work / f'path-{index}'
            result = run([compiler, 'run', source, '--out', prefix])
            assert result.stdout == '7\n' and (not result.stderr), result.stdout + result.stderr
            run([compiler, 'run', source, '--emit', '--out', prefix])
            glue = prefix.with_suffix('.abi.cpp').read_text(encoding='utf-8')
            assert library.as_posix() in glue, glue
        nested = work / 'nested'
        nested.mkdir()
        signature = 'abi "../native paths/' + library.name + '" { int32 identity(int32 n) = "identity"; }\n'
        (nested / 'api.chl').write_text('module nested::api;\n' + signature, encoding='utf-8')
        (nested / 'second.chl').write_text('module nested::second;\n' + signature, encoding='utf-8')
        imported = work / 'imported.chl'
        imported.write_text('import nested::api;\nvoid main() { std::lang::println("{}", nested::api::identity((int32)8)); return; }\n', encoding='utf-8')
        included = work / 'included.chl'
        included.write_text('import nested::second;\nvoid main() { std::lang::println("{}", nested::second::identity((int32)8)); return; }\n', encoding='utf-8')
        for source in (imported, included):
            prefix = work / source.stem
            result = run([compiler, 'run', source, '--out', prefix])
            assert result.stdout == '8\n', result.stdout + result.stderr
            run([compiler, 'run', source, '--emit', '--out', prefix])
            assert library.as_posix() in prefix.with_suffix('.abi.cpp').read_text(encoding='utf-8')
        raw = work / 'raw.chl'
        raw.write_text('abi "./native paths/' + library.name + '" { int32 identity(int32 n) = "identity"; }\nvoid main() { std::lang::println("{}", identity((int32)8)); return; }\n', encoding='utf-8')
        assert run([compiler, 'run', raw, '--raw', '--out', work / 'raw']).stdout == '8\n'
        module = work / 'probe.chl'
        module.write_text('module probe;\n%if CHALI_OS == other && CHALI_BITS == 32 && CUSTOM == 9\nint64 value() { return 9; }\n%else\nint64 value() { return 1; }\n%endif\n', encoding='utf-8')
        source = work / 'platform.chl'
        source.write_text('module app;\nimport probe;\nvoid main() {\n%if CHALI_OS == windows\nstd::lang::println("windows");\n%elif CHALI_OS == linux\nstd::lang::println("linux");\n%elif CHALI_OS == macos\nstd::lang::println("macos");\n%else\nstd::lang::println("other");\n%endif\n%if CHALI_BITS == 64\nstd::lang::println("64");\n%elif CHALI_BITS == 32\nstd::lang::println("32");\n%else\n%error unexpected bits\n%endif\nstd::lang::println("{}", probe::value()); return; }\n', encoding='utf-8')
        default = run([compiler, 'run', source, '--out', work / 'default'])
        expected_os = 'windows' if os.name == 'nt' else 'macos' if __import__('sys').platform == 'darwin' else 'linux'
        lines = default.stdout.splitlines()
        assert lines[0] == expected_os and lines[1] in ('32', '64') and (lines[2] == '1'), default.stdout
        override = run([compiler, 'run', source, '-DCHALI_OS=other', '-DCHALI_BITS=32', '-DCUSTOM=9', '--out', work / 'override'])
        assert override.stdout == 'other\n32\n9\n', override.stdout
        include = work / 'conditions.inc'
        include.write_text('%if USER_VALUE == 42 && CHALI_OS == other && CHALI_BITS == 32\nint64 selected() { return 42; }\n%else\n%error user definitions missing\n%endif\n', encoding='utf-8')
        source.write_text('%define CHALI_OS other\n%define CHALI_BITS 32\n%define USER_VALUE 42\n' + include.read_text(encoding='utf-8') + 'void main() { std::lang::println("{}", selected()); }\n', encoding='utf-8')
        user = run([compiler, 'run', source, '--out', work / 'user'])
        assert user.stdout == '42\n', user.stdout
    print('CHALI platform and ABI path checks passed')
if __name__ == '__main__':
    main()
