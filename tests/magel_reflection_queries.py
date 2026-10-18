"""MAGEL 类型化查询真实解释与 AOT回归，包含缺失、遮蔽、所有权与功能开关。"""
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
    with tempfile.TemporaryDirectory(prefix='reflection-queries-',dir=ROOT/'build') as directory:
        work=Path(directory);sample=ROOT/'example/reflection/queries.mgl'
        rejected=run([args.magel,'run', sample,'--no-reflection','--check'],False)
        assert '未开启' in rejected.stderr,rejected.stderr
        for body in ('std::reflect::getTypeId<Unknown>()','std::reflect::getTypeId<Box<T>>()'):
            source=work/'invalid.mgl';source.write_text('import std::reflect;\nstruct Box<T>{T value;};\nint64 main(){'+body+';return 0;}\n',encoding='utf-8')
            run([args.magel,'run', source,'--reflection','--check'],False)
        for backend in ('','aot'):
            output=work/((backend or 'direct')+EXE)
            command=[args.magel,*([backend,'--opt-level=0'] if backend else ['run']),sample,'--reflection','--out',output]
            result=run(command)
            if backend:result=run([output])
            assert result.stdout=='reflection queries OK\n' and not result.stderr,(backend,result.stdout,result.stderr)
        print(f'MAGEL 类型化查询：{checks} 项检查通过')
if __name__=='__main__':main()
