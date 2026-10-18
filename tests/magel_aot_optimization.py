"""验证 AOT 标量内存、数组检查与宿主参数复用在默认优化和 O0 下的一致性。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def main():
    """运行有效数组与动态越界，核对副作用只发生一次及错误路径保留。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=ROOT/'magel.exe')
    parser.add_argument('--case',action='append',choices=['O0','default','modules','errors'])
    options=parser.parse_args()
    compiler=options.magel.resolve()
    env=dict(os.environ,MAGEL_LANG='zh',MAGEL_NO_STDIN_INPUT='0')

    def invoke(*args,**kwargs):
        """执行编译器或产物并收集标准输出与诊断。"""
        return subprocess.run(list(map(str,args)),cwd=ROOT,env=env,capture_output=True,encoding='utf-8',errors='replace',timeout=600,**kwargs)

    with tempfile.TemporaryDirectory(prefix='magel-aot-optimization-') as folder:
        work=Path(folder);source=ROOT/'tests/fixtures/aot_optimization.mgl'
        vm=invoke(compiler,'run',source);assert (vm.returncode,vm.stdout,vm.stderr)==(0,'aot optimization OK\n',''),vm
        for label,flags in [('O0',['--opt-level=0']),('default',[]),('modules',['--separate-modules'])]:
            if options.case and label not in options.case: continue
            output=work/(label+'-aot.exe')
            result=invoke(compiler,'aot',source,'--out',output,'--keep-intermediate',*flags)
            assert result.returncode==0,result
            if label=='default':assert '-O3' in result.stdout+result.stderr,result
            actual=invoke(output);assert (actual.returncode,actual.stdout,actual.stderr)==(0,vm.stdout,''),actual
            if label!='modules':
                code=Path(str(output)+'.aot.cpp').read_text(encoding='utf-8')
                for marker in ['rt.loadInteger(', 'rt.loadFloat(', 'rt.storeInteger(', 'rt.storeFloat(', 'magel_native_args.resize(']:assert marker in code,marker
                assert 'Memory::Vector<stackvm::RuntimeValue>{' not in code
                assert '#define STACKVM_WITH_ALLOCATION_GUARDS 1' in code
                assert '#define NO_MAGEL_PANIC_TRACE 1' not in code
            print('[通过] 数组、标量、容器、副作用及模块调用：'+label,flush=True)
        source=work/'invalid.mgl'
        source.write_text('void dispose(int64 mut& p){delete p;}int64 leaf(int64 & p){return p;}int64 main(){int64 index=inputInt();if(index==999){int64 mut& owner = heap int64;int64 & stale=owner;dispose(owner);return leaf(stale);}int64 values[]=new[4];int64 mut& pointer=&values[0];println("{}",pointer[index]);return 0;}',encoding='utf-8')
        output=work/'invalid-aot.exe';built=invoke(compiler,'aot',source,'checked','--out',output);assert built.returncode==0,built
        for index in [-1,4,1152921504606846976]:
            for backend,command in [('VM',[compiler,'run',source]),('AOT',[output])]:
                result=invoke(*command,input=str(index)+'\n')
                assert result.returncode!=0 and '数组下标越界' in result.stderr,(backend,index,result)
                assert '调用栈' in result.stderr,(backend,result)
        for command in [[compiler,'run',source],[output]]:
            result=invoke(*command,input='999\n')
            assert result.returncode!=0 and 'panic：' in result.stderr and 'leaf' in result.stderr,result
        print('AOT 优化回归通过：O0/默认 O3、独立模块、动态越界、乘法溢出索引及失效地址诊断')


if __name__=='__main__':main()
