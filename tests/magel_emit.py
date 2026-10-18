"""验证仅保留 AOT/.svmb 产物以及已删除的 VM 导出入口。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''


def main():
    """通过真实统一入口与独立导出器验证目标选择、配置和产物。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT/('magel'+EXE))
    parser.add_argument('--emitter', type=Path)
    args = parser.parse_args()
    compiler = args.magel.resolve()
    checks = 0

    def run(command, success=True, cwd=ROOT):
        nonlocal checks
        result = subprocess.run(list(map(str, command)), cwd=cwd, capture_output=True, text=True,
                                encoding='utf-8', errors='replace', timeout=240)
        assert (result.returncode == 0) == success, (command, result.returncode, result.stdout, result.stderr)
        checks += 1
        return result

    (ROOT/'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='magel-export-modes-', dir=ROOT/'build') as temporary:
        work = Path(temporary)
        emitter = args.emitter
        if emitter is None:
            entry = work/'emit-entry.cpp'
            entry.write_text('#include "source/magel-emit/emit_cli.hpp"\nint main(int argc,char**argv){return magel_emit::runCli(argc,argv);}', encoding='utf-8')
            emitter = work/('magel-emit'+EXE)
            run(['g++', '-std=c++17', '-pthread', '-DMAGEL_CUSTOM_ALLOCATOR=1',
                 '-I'+str(ROOT), entry, ROOT/'source/magel-emit/main.o',
                 ROOT/'source/magel-emit/aot.o', ROOT/'library/lztext/lztext.o', ROOT/'build/magel-stackvm/libstackvm.a', '-lm', '-o', emitter])
        emitter = emitter.resolve()
        source = work/'main.mgl'
        source.write_text('int64 main(){std::lang::println("export-ok");return 0;}\n', encoding='utf-8')
        help_text = run([compiler, 'help']).stdout
        assert 'vm FILE' not in help_text
        for options in (['vm'], ['--mode', 'vm'], ['--run-mode=vm']):
            prefix = work/'rejected'
            run([compiler, *options, source, '--out', prefix], success=False)
            assert not list(work.glob('rejected*'))
        (work/'magel.toml').write_text('[magel]\nrun="vm"\n', encoding='utf-8')
        run([compiler, source, '--out', work/'invalid-project'], cwd=work, success=False)
        (work/'magel.toml').unlink()
        run([compiler, 'gen', 'vm', 'out', work/'removed-generator'], success=False)
        assert not (work/'removed-generator').exists()
        prefix = work/'bytecode'
        run([compiler, 'svmb', source, 'out', prefix])
        bytecode = prefix.with_suffix('.svmb')
        assert bytecode.is_file() and bytecode.stat().st_size > 20
        run([compiler, 'smvb', source, 'out', work/'alias'])
        assert (work/'alias.svmb').is_file()
        assert not list(work.glob('*.target.cpp'))
        # 独立导出器默认 AOT，旧 cpp 目标在读输入和写文件之前拒绝。
        run([emitter, '--target', 'cpp', '--quad', work/'missing.quad', '--out', work/'rejected.cpp'], success=False)
        assert not (work/'rejected.cpp').exists()
        prefix = work/'program'
        run([compiler, 'emit', source, 'out', prefix])
        target = work/('aot-default'+EXE)
        run([emitter, '--quad', prefix.with_suffix('.quad'), '--data', prefix.with_suffix('.data.bin'),
             '--layout', prefix.with_suffix('.layout'), '--out', target, '--cpp-output'])
        assert run([target]).stdout == 'export-ok\n'
        cpp = Path(str(target)+'.aot.cpp')
        text = cpp.read_text(encoding='utf-8')
        assert '基本块 + 局部变量 + goto' in text
        assert 'appendInstructions_' not in text
        # 脱离仓库运行编译器，AOT 所需内嵌资产仍完整。
        import shutil
        with tempfile.TemporaryDirectory(prefix='magel-export-isolated-') as directory:
            isolated = Path(directory)
            standalone = isolated/('magel'+EXE)
            shutil.copyfile(compiler, standalone)
            run([standalone, 'aot', source, 'out', isolated/('app'+EXE), 'opt', '0'], cwd=isolated)
            assert run([isolated/('app'+EXE)]).stdout == 'export-ok\n'
    print(f'导出目标验收通过：{checks} 项真实命令检查，仅 AOT/.svmb 产物')


if __name__ == '__main__':
    main()
