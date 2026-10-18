"""MAGEL 反射端到端验收：JSON 构造与拥有资源、默认覆盖分派及枚举变更。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
EXE='.exe' if os.name=='nt' else ''
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=ROOT/('magel-reflection'+EXE))
    args=parser.parse_args();checks=0
    def run(command,ok=True):
        nonlocal checks
        result=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=240)
        assert (result.returncode==0)==ok,(command,result.stdout,result.stderr)
        checks+=1;return result
    with tempfile.TemporaryDirectory(prefix='reflection-e2e-',dir=ROOT/'build') as directory:
        work=Path(directory)
        for fixture in ('json','dispatch'):
            sample=ROOT/('example/reflection/'+fixture+'.mgl')
            for backend in ('','aot'):
                output=work/(fixture+(backend or 'direct')+EXE)
                result=run([args.magel,*([backend,'--opt-level=1'] if backend else ['run']),sample,'--reflection','--out',output])
                if backend:result=run([output])
                assert result.stdout=='reflection '+fixture+' OK\n' and not result.stderr,(fixture,backend,result.stdout,result.stderr)
                print(f'[通过] {fixture}: {backend or "直接执行"}',flush=True)
        print(f'MAGEL 端到端：{checks} 项检查通过')
if __name__=='__main__':main()
