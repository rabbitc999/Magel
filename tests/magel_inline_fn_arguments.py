"""验证嵌套函数值调用直接接收 fn 实参，保留类型检查及临时闭包所有权。"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('MAGEL_COMPILER', ROOT / ('magel.exe' if os.name == 'nt' else 'magel')))
SAMPLE = ROOT / 'example/samples/fn_inline_arguments.mgl'


def invoke(*arguments):
    """固定中文诊断和非交互输入，捕获后端退出码及输出。"""
    return subprocess.run([str(value) for value in arguments], cwd=ROOT,
                          env=dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1'),
                          capture_output=True, encoding='utf-8', errors='replace', timeout=240)


def main():
    """解释执行和 AOT 验证直接、推导、命名及捕获回调，并拒绝不匹配的签名。"""
    source = SAMPLE.read_text(encoding='utf-8')
    original = 'auto tt = ff(int64 fn(int64 x) { x + 1 }, int64 fn(int64 x) { x + 2 });'
    variants = {
        'direct': original,
        'inferred': 'auto tt = ff(fn(x) { x + 1 }, fn(x) { x + 2 });',
        'variables': '''int64 fn(int64) a=int64 fn(int64 x){x+1};
            int64 fn(int64) b=int64 fn(int64 x){x+2};auto tt=ff(a,b);''',
        'mixed': '''int64 fn(int64) a=int64 fn(int64 x){x+1};
            auto tt=ff(a,int64 fn(int64 x){x+2});''',
        'named': '''int64 fn(int64) a=increment;
            auto tt=ff(increment,int64 fn(int64 x){x+2});''',
        'capture': '''int64 one=1;List<int64> increments;increments.push(2);
            auto tt=ff(int64 fn copy(int64 x){x+one},
                       int64 fn copy(int64 x){x+increments[0]});''',
    }
    with tempfile.TemporaryDirectory(prefix='magel-inline-fn-') as directory:
        work = Path(directory)
        for name, statement in variants.items():
            code = source.replace(original, statement)
            code = 'int64 increment(int64 x){return x+1;}\n' + code
            code = code.replace('int64 x = tuple.2;',
                                'assert(tuple.0(7)==8 && tuple.1(7)==9);\n    int64 x = tuple.2;')
            path = work / (name + '.mgl')
            path.write_text(code, encoding='utf-8')
            for level in (0, 2):
                result = invoke(COMPILER, 'run', path, f'--opt-level={level}', '--out', work / f'{name}-O{level}')
                assert (result.returncode, result.stdout, result.stderr) == (0, '2 5\n', ''), (name, level, result)
            executable = work / (name + ('.exe' if os.name == 'nt' else '-aot'))
            result = invoke(COMPILER, 'aot', path, '--out', executable)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            result = invoke(executable)
            assert (result.returncode, result.stdout, result.stderr) == (0, '2 5\n', ''), (name, result)
            print(f'[通过] {name}：VM O0/O2 与 AOT 默认优化', flush=True)
        for name, callback in {
            'wrong-parameter': 'int64 fn(float64 x){return 1;}',
            'wrong-result': 'float64 fn(int64 x){return 1.0;}',
            'wrong-arity': 'int64 fn(int64 x,int64 y){return x+y;}',
        }.items():
            path = work / (name + '.mgl')
            path.write_text(source.replace('int64 fn(int64 x) { x + 1 }', callback), encoding='utf-8')
            result = invoke(COMPILER, '--check', path)
            assert result.returncode != 0, (name, result)
            print(f'[通过] 拒绝 {name}', flush=True)


if __name__ == '__main__':
    main()
