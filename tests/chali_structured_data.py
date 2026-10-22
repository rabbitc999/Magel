from pathlib import Path
'验证 JSON/TOML 副本、CHALI 胶水、导出与裁剪。'
import os
from chali_glue_edges import ROOT, CHALI, EXE, run

def main():
    work = ROOT / 'build/structured-data'
    work.mkdir(parents=True, exist_ok=True)
    for name in ('json', 'toml'):
        for file in (name + '.hpp', name + '.cpp', 'bind.hpp', 'test_' + name + '.cpp'):
            assert (ROOT / 'library' / name / file).read_bytes() == (ROOT / name / file).read_bytes(), file
        executable = work / ('test-' + name + EXE)
        run('g++', '-std=c++17', '-O2', ROOT / f'{name}/test_{name}.cpp', ROOT / f'{name}/{name}.cpp', '-o', executable)
        run(executable)
    source = ROOT / 'example/samples/std_structured_data.chl'
    expected = (ROOT / 'example/expected/std_structured_data.txt').read_text(encoding='utf-8')
    prefix = work / 'data'
    result = run(CHALI, 'run', source, '--out', prefix)
    assert result.stdout == expected and (not result.stderr), (result.stdout, result.stderr)
    run(CHALI, 'run', source, '--emit', '--out', prefix)
    arguments = ['--quad', prefix.with_suffix('.quad'), '--data', prefix.with_suffix('.data.bin'), '--layout', prefix.with_suffix('.layout')]
    asset_target = work / ('data' + '-assets' + EXE)
    for name in ('JSON', 'TOML'):
        executable = work / ('trim-' + name + EXE)
        result = run(executable, succeeds=False)
        assert 'std::' + name.lower() + '::' in result.stdout + result.stderr
    result = run(CHALI, 'run', ROOT / 'example/samples-error/structured_closed.chl', '--out', work / 'closed', succeeds=False)
    assert 'JsonValue.kind' in result.stderr
if __name__ == '__main__':
    main()
