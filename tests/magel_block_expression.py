"""验证块表达式、变长列表宏及插值输出在解释与 AOT 后端的一致性。"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """运行成功样例及缺少出口、错误类型、非法引用等诊断检查。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    parser.add_argument('--front-end-only', action='store_true')
    parser.add_argument('--sample', action='append', help='只运行指定正常样例，可重复；错误诊断仍全部检查')
    options = parser.parse_args()
    compiler = options.magel.resolve()
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')
    with tempfile.TemporaryDirectory(prefix='magel-block-') as directory:
        work = Path(directory)

        def invoke(command):
            """执行编译或测试程序并收集输出。"""
            return subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                  capture_output=True, text=True, encoding='utf-8',
                                  errors='replace', timeout=240)

        examples = {
            'block_expression': 'block expression OK\n',
            'break_values': 'break values OK\n',
            'break_move': 'break move OK\n',
            'break_level': 'break level OK\n',
            'print_interpolation': 'value=3;next=1 2;literal={x};percent=100%\nnested=value 3\nsaved=3\ninterpolation print OK\n',
        }
        for name, expected in examples.items():
            if options.sample and name not in options.sample:
                continue
            for backend in ('run',) if options.front_end_only else ('run', 'aot'):
                for level in (0, 2):
                    output = work / (name + backend + str(level) + '.exe')
                    result = invoke([compiler, backend, ROOT / 'example/samples' / (name + '.mgl'),
                                     '--out', output, f'--opt-level={level}'])
                    assert result.returncode == 0, (name, backend, result.stdout, result.stderr)
                    if backend == 'aot':
                        result = invoke([output])
                    assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), result
                    print(f'[通过] {name} {backend} O{level}', flush=True)
        for body in (
            '{int64 y=loop{int64 z=loop{yield (5);};yield (z+1);};yield (y+1);};',
            '({int64 y=loop{int64 z=loop{yield (5);};yield (z+1);};yield (y+1);});',
            '{yield (1);};',
            '{int64 y=loop{int64 z=loop{yield (5);};yield (z+1);};yield (y+1);}',
            'auto f=int64 fn(){loop{int64 x={break all;yield (1);};};};f();',
            'break level 1;',
            'break all;',
            'loop { break level 0; };',
            'loop { break level -1; };',
            'loop { break level 2; };',
            'loop { break level 999999999999999999999999; };',
            'loop { int64 n=1; break level n; };',
            'loop { break level 1+1; };',
            'loop { auto f=fn(){break all;}; };',
            'auto result=loop { if(true){yield (1);} loop{break level 2;}; };',
            'auto f=int64 fn(){loop{loop{break level 2;};};};f();',
            'auto f=int64 fn(){loop{loop{break all;};};};f();',
            'int32 x = { int32 y = 3; };',
            'int32 x = { if (true) { yield (3); } };',
            'int32 x = { break; };',
            'int32 x = { yield ("wrong"); };',
            'int32 x = { int32 local = 1; yield (local); }; local = 2;',
            'int32 const& x = { int32 local = 1; yield (&local); };',
            'while (true) { yield (1); }',
            'int64 x = { loop { break; }; };',
            'int64 x = { while (false) { yield (1); } };',
            '(int64,String) x = { loop { yield (1,2); }; };',
            '(int64,int64) x = { loop { yield (1,2,3); }; };',
            'int64 x = { loop { auto f = int64 fn(){yield (1);}; yield (2); }; };',
            'auto f = int64 fn(){ (if(true){loop{yield (1);};}else{2}); };f();',
            'int64 const& x = { loop { int64 local = 1; yield (&local); }; };',
            'int64 const& x = { for (int64 local=0;local<1;local++) { yield (&local); } int64 local=2;yield (&local); };',
            'println(f"value={1}", 2);',
            'auto result=loop{String source="x";yield (source,source);};',
            '(int64 const&,int64) result={loop{int64 local=1;yield (&local,2);};};',
        ):
            source = work / 'invalid.mgl'
            source.write_text('int64 main(){' + body + 'return 0;}', encoding='utf-8')
            result = invoke([compiler, '--check', source])
            assert result.returncode != 0, (body, result.stdout, result.stderr)
            if body in ('{yield (1);};', '{int64 y=loop{int64 z=loop{yield (5);};yield (z+1);};yield (y+1);};'):
                for backend in ('run', 'aot'):
                    orphan = invoke([compiler, backend, source, '--out', work / 'orphan'])
                    assert orphan.returncode != 0, orphan
            print(f'[通过] 拒绝 {body}', flush=True)
        equivalents = (
            ('int64', '1'),
            ('String', '"x"'),
        )
        for type_name, initial in equivalents:
            diagnostics = []
            for form in ('yield (source,source);', 'yield (std::lang::move(&source),std::lang::move(&source));'):
                source = work / 'equivalent.mgl'
                source.write_text('int64 main(){auto result=loop{' + type_name + ' source=' + initial + ';' + form + '};return 0;}', encoding='utf-8')
                result = invoke([compiler, 'check', source, 'json'])
                assert result.returncode != 0, (form, result)
                diagnostic = json.loads(result.stdout)['diagnostics'][0]
                diagnostics.append((diagnostic['code'], diagnostic['message']))
                for backend in ('run', 'aot'):
                    result = invoke([compiler, backend, source, '--out', work / 'invalid-output'])
                    assert result.returncode != 0 and diagnostic['message'] in result.stderr + result.stdout, (backend, form, result)
            assert diagnostics[0] == diagnostics[1], diagnostics
            print('[通过] 隐式移动与显式移动的诊断及 VM/AOT 拒绝行为一致', flush=True)


if __name__ == '__main__':
    main()
