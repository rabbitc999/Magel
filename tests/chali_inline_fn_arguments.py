"""验证嵌套函数值调用直接接收 fn 实参，保留类型检查及临时闭包所有权。"""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('CHALI_COMPILER', ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali')))
SAMPLE = ROOT / 'example/samples/fn_inline_arguments.chl'

def invoke(*arguments):
    """固定中文诊断和非交互输入，捕获后端退出码及输出。"""
    return subprocess.run([str(value) for value in arguments], cwd=ROOT, env=dict(os.environ, CHALI_LANG='zh', CHALI_NO_STDIN_INPUT='1'), capture_output=True, encoding='utf-8', errors='replace', timeout=240)

def main():
    """解释执行和 VM 验证直接、推导、命名及捕获回调，并拒绝不匹配的签名。"""
    source = SAMPLE.read_text(encoding='utf-8')
    original = 'let tt = ff(int64 fn(int64 x) { x + 1 }, int64 fn(int64 x) { x + 2 });'
    variants = {'direct': original, 'inferred': 'let tt = ff(fn(x) { x + 1 }, fn(x) { x + 2 });', 'variables': 'int64 fn(int64) a=int64 fn(int64 x){x+1};\n            int64 fn(int64) b=int64 fn(int64 x){x+2};let tt=ff(a,b);', 'mixed': 'int64 fn(int64) a=int64 fn(int64 x){x+1};\n            let tt=ff(a,int64 fn(int64 x){x+2});', 'named': 'int64 fn(int64) a=increment;\n            let tt=ff(increment,int64 fn(int64 x){x+2});', 'capture': 'int64 one=1;List<int64> increments;increments.push(2);\n            let tt=ff(int64 fn copy(int64 x){x+one},\n                       int64 fn copy(int64 x){x+increments[0]});'}
    with tempfile.TemporaryDirectory(prefix='chali-inline-fn-') as directory:
        work = Path(directory)
        for name, statement in variants.items():
            code = source.replace(original, statement)
            code = 'int64 increment(int64 x){return x+1;}\n' + code
            code = code.replace('int64 x = tuple.2;', 'assert(tuple.0(7)==8 && tuple.1(7)==9);\n    int64 x = tuple.2;')
            path = work / (name + '.chl')
            path.write_text(code, encoding='utf-8')
            for level in (0, 2):
                result = invoke(COMPILER, 'run', path, '--out', work / f'{name}-O{level}')
                assert (result.returncode, result.stdout, result.stderr) == (0, '2 5\n', ''), (name, level, result)
        for name, callback in {'wrong-parameter': 'int64 fn(float64 x){return 1;}', 'wrong-result': 'float64 fn(int64 x){return 1.0;}', 'wrong-arity': 'int64 fn(int64 x,int64 y){return x+y;}'}.items():
            path = work / (name + '.chl')
            path.write_text(source.replace('int64 fn(int64 x) { x + 1 }', callback), encoding='utf-8')
            result = invoke(COMPILER, '--check', path)
            assert result.returncode != 0, (name, result)
            print(f'[通过] 拒绝 {name}', flush=True)
if __name__ == '__main__':
    main()
