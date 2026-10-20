"""验证花括号类型声明可省略尾分号，并保持原有声明边界。"""
from pathlib import Path
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]

def main():
    """对同一组类型分别保留和省略尾分号，核对 VM 输出。"""
    work = ROOT / 'build/type-decl-terminators'
    work.mkdir(parents=True, exist_ok=True)
    declarations = 'interface Value { int64 get(); }END\ninterface Derived : Value {}END\nstruct Cell : Derived { int64 n; int64 get(){return self.n;} }END\nstruct Box<T> { T item; }END\nenum State { Empty, Full(int64 value) }END\nenum Flag { Off, On; int64 number(){return 1;} }END\n'
    body = 'int64 main(){Cell c;c.n=7;Box<int64> b;b.item=c.get();\nState s=State::Full(b.item);println("{}",b.item);return 0;}'
    compiler = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')
    for ending in (';', ''):
        source = work / ('with.mgl' if ending else 'without.mgl')
        source.write_text(declarations.replace('END', ending) + body, encoding='utf-8')
if __name__ == '__main__':
    main()
