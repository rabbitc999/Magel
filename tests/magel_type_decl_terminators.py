"""验证花括号类型声明可省略尾分号，并保持原有声明边界。"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    """对同一组类型分别保留和省略尾分号，核对 VM 与 AOT 输出。"""
    work = ROOT / 'build/type-decl-terminators'
    work.mkdir(parents=True, exist_ok=True)
    declarations = '''interface Value { int64 get(); }END
interface Derived : Value {}END
struct Cell : Derived { int64 n; int64 get(){return self.n;} }END
struct Box<T> { T item; }END
enum State { Empty, Full(int64 value) }END
enum Flag { Off, On; int64 number(){return 1;} }END
'''
    body = '''int64 main(){Cell c;c.n=7;Box<int64> b;b.item=c.get();
State s=State::Full(b.item);println("{}",b.item);return 0;}'''
    compiler = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')
    for ending in (';', ''):
        source = work / ('with.mgl' if ending else 'without.mgl')
        source.write_text(declarations.replace('END', ending) + body, encoding='utf-8')
        for backend in ('run', 'aot'):
            output = work / (source.stem + '-aot.exe')
            result = subprocess.run([str(compiler), backend, str(source), '--out', str(output)],
                                    cwd=ROOT, capture_output=True, text=True, encoding='utf-8', timeout=300)
            assert result.returncode == 0, (source, backend, result.stdout, result.stderr)
            if backend == 'aot':
                result = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert result.returncode == 0 and result.stdout.strip() == '7', result
    print('类型声明带分号与省略分号的 VM/AOT 行为一致。')


if __name__ == '__main__':
    main()
