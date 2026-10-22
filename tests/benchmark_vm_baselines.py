"""同机比较 ChaLi VM、JVM、CPython、官方 Lua 和原生 C++，保留全部样本；可选追加旧版 ChaLi 两行。"""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile

from benchmark_vm_lua import URL, SHA256, WORK as LUA_WORK

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/vm-baselines"
CASES = [
    ("integer_loop", 10000000, 49999995000000),
    ("branches", 1000000, -500000),
    ("float64_loop", 2000000, 1000000),
    ("calls", 1000000, 17*1000000*999999//2),
    ("array_reads", 1000000, 2500000),
]


def run(command, **kwargs):
    """运行命令并在失败时报告完整诊断。"""
    result = subprocess.run(list(map(str, command)), cwd=kwargs.pop('cwd', ROOT), capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=600, **kwargs)
    if result.returncode:
        raise RuntimeError((command, result.returncode, result.stdout, result.stderr))
    return result.stdout


def prepare(name):
    """生成等价的五语言用例；所有循环上限都从运行时输入取得。"""
    floating = name == "float64_loop"
    bodies = {
        "integer_loop": ("total+=i;", "total=total+i", "total += i"),
        "branches": ("if((i&1)==0){total+=i;}else{total-=i;}", "if (i & 1)==0 then total=total+i else total=total-i end", "total += i if (i & 1)==0 else -i"),
        "float64_loop": ("total+=0.5;", "total=total+0.5", "total += 0.5"),
        "calls": ("total+=mix(i);", "total=total+mix(i)", "total += mix(i)"),
        "array_reads": ("total+=values[i%4];", "total=total+values[(i%4)+1]", "total += values[i%4]"),
    }
    body, lua_body, python_body = bodies[name]
    chl_decl = "int64 values[4];values[0]=1;values[1]=2;values[2]=3;values[3]=4;" if name == "array_reads" else ""
    (WORK / (name+".chl")).write_text("int64 mix(int64 x){return x*17;}\nvoid main(){int64 n=inputInt(),i=0;"
        + ("float64" if floating else "int64") + " total=0;"+chl_decl+"while(i<n){"+body
        +"i+=1;}println(\"{}\",total);return;}\n", encoding="utf-8")
    (WORK / (name+".lua")).write_text("local n=tonumber(arg[1])\nlocal function mix(x) return x*17 end\n"
        "local function calculate(n)\nlocal i,total=0,"+("0.0" if floating else "0")
        +"\nlocal values={1,2,3,4}\nwhile i<n do "+lua_body+";i=i+1 end\nreturn total end\n"
        "calculate(n)\nlocal start=os.clock()\nlocal result=calculate(n)\nprint((os.clock()-start)*1000,result)\n",encoding="utf-8")
    (WORK / (name+".py")).write_text("import sys,time\nn=int(sys.argv[1])\ndef mix(x): return x*17\n"
        "def calculate(n):\n    i,total=0,"+("0.0" if floating else "0")
        +"\n    values=[1,2,3,4]\n    while i<n:\n        "+python_body
        +"\n        i+=1\n    return total\ncalculate(n)\nstart=time.perf_counter_ns()\n"
        "result=calculate(n)\nprint((time.perf_counter_ns()-start)/1e6,result)\n",encoding="utf-8")
    cpp = r'''#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
static volatile __TYPE__ sink;
static std::int64_t mix(std::int64_t x) { return x*17; }
__attribute__((noinline)) static __TYPE__ calculate(std::int64_t n) {
    std::int64_t i=0; __TYPE__ total=0; __DECL__
    while(i<n) { __BODY__ ++i; }
    return total;
}
int main() {
    std::int64_t n; if(!(std::cin>>n)) return 2;
    sink=calculate(n);
    if(!(std::cin>>n)) return 2;
    const auto start=std::chrono::steady_clock::now();
    const auto value=calculate(n);
    const auto end=std::chrono::steady_clock::now();
    sink=value;
    std::cout<<std::setprecision(17)<<std::chrono::duration<double,std::milli>(end-start).count()<<' '<<value<<'\n';
}
'''
    cpp=cpp.replace('__TYPE__','double' if floating else 'std::int64_t').replace('__BODY__',body).replace('__DECL__','std::int64_t values[4]={1,2,3,4};' if name=='array_reads' else '')
    (WORK/(name+'.cpp')).write_text(cpp,encoding='utf-8')
    java = '''import java.util.Scanner;
public final class Case___NAME__ {
    private static volatile __TYPE__ sink;
    private static long mix(long x) { return x*17; }
    private static __TYPE__ calculate(long n) {
        long i=0; __TYPE__ total=0; __DECL__
        while(i<n) { __BODY__ ++i; }
        return total;
    }
    public static void main(String[] args) {
        Scanner input=new Scanner(System.in);
        int warmups=Integer.parseInt(args[0]);
        for(int i=0;i<warmups;++i) sink=calculate(input.nextLong());
        long n=input.nextLong();
        long start=System.nanoTime();
        __TYPE__ value=calculate(n);
        long elapsed=System.nanoTime()-start;
        sink=value;
        System.out.println((elapsed/1e6)+" "+value);
    }
}
'''
    java=java.replace('__NAME__',name).replace('__TYPE__','double' if floating else 'long').replace('__BODY__',body.replace('values[i%4]','values[(int)(i%4)]')).replace('__DECL__','long[] values={1,2,3,4};' if name=='array_reads' else '')
    (WORK/('Case_'+name+'.java')).write_text(java,encoding='utf-8')


def main():
    """构建后串行轮转九种配置，按两轮独立样本记录中位耗时。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali',type=Path,default=ROOT/'ChaLi.exe')
    parser.add_argument('--runner',type=Path,required=True)
    parser.add_argument('--chali-label',default='ChaLi '+(ROOT/'VERSION').read_text().strip())
    parser.add_argument('--baseline-chali',type=Path,help='旧版编译器；给出后追加旧版 VM 一行')
    parser.add_argument('--baseline-runner',type=Path)
    parser.add_argument('--baseline-label',default='ChaLi baseline')
    parser.add_argument('--repeats',type=int,default=7)
    parser.add_argument('--rounds',type=int,default=2)
    parser.add_argument('--reuse-builds',action='store_true',help='源码和构建指纹一致时复用本机已有产物')
    parser.add_argument('--output',type=Path,default=ROOT/'doc/benchmarks/vm-baselines-0.18.3.json')
    args=parser.parse_args()
    assert args.repeats>0 and args.rounds>0
    assert bool(args.baseline_chali)==bool(args.baseline_runner),'旧版编译器与运行器必须同时给出'
    targets=[('103',args.chali.resolve(),args.chali_label,args.runner.resolve())]
    if args.baseline_chali:
        targets.append(('102',args.baseline_chali.resolve(),args.baseline_label,args.baseline_runner.resolve()))
    WORK.mkdir(parents=True,exist_ok=True)
    archive=LUA_WORK/'lua-5.5.1.tar.gz'
    assert hashlib.sha256(archive.read_bytes()).hexdigest()==SHA256,'先运行 benchmark_vm_lua.py 下载并构建官方 Lua'
    lua=LUA_WORK/'lua-5.5.1/src'/('lua.exe' if os.name=='nt' else 'lua')
    suffix='.exe' if os.name=='nt' else ''
    for name,_,_ in CASES:
        prepare(name)
        run(['g++','-std=c++17','-O2','-DNDEBUG',WORK/(name+'.cpp'),'-o',WORK/(name+suffix)])
        run(['javac','-encoding','UTF-8',WORK/('Case_'+name+'.java')])
        for label,compiler,_,_ in targets:
            run([compiler,'svmb',WORK/(name+'.chl'),'out',WORK/(name+'-'+label)])
    report={'measured_at_utc':datetime.now(timezone.utc).isoformat(),'commit':run(['git','rev-parse','HEAD']).strip(),
            'platform':platform.platform(),'python':sys.version,'java':run(['java','--version']).strip(),
            'benchmark_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'compiler':run(['g++','--version']).splitlines()[0],'lua':run([lua,'-v']).strip(),
            'lua_url':URL,'lua_source_sha256':SHA256,'repeats':args.repeats,'rounds':args.rounds,
            'warmups':{'JVM JIT':20,'others':1},'flags':{'C++':'-std=c++17 -O2 -DNDEBUG; noinline calculation; volatile result sink; fresh stdin input after warmup',
            'JVM JIT':'-Xbatch; 20 full warmups; volatile result sink; fresh stdin input per invocation',
            'JVM interpreter':'-Xint; 1 full warmup',            'ChaLi':'each listed ChaLi version keeps reflection enabled and all default VM optimizations; runner built -O2 against that version static library',
            'Lua':'official mingw Makefile -O2; no JIT'},
            'timing':'all exclude startup, source compilation and loading; VM includes execution preparation and checksum output; other runtimes measure calculation only; one measured sample per process; rotate order each repeat; serial; all builds finish before measurements',
            'binaries':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(compiler for _,compiler,_,_ in targets),*(runner for _,_,_,runner in targets),lua]},
            'results':{}}
    report['generated_sources']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in WORK.iterdir() if p.suffix in ('.chl','.lua','.py','.java') or p.suffix=='.cpp'}
    if os.name=='nt':
        report['cpu']=json.loads(run(['powershell','-NoProfile','-Command','Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors | ConvertTo-Json -Compress']))
    for round_index in range(args.rounds):
        results={}
        for name,count,expected in CASES:
            modes=[(label,[runner,WORK/(name+'-'+tag),1,1,count],None) for tag,_,label,runner in targets]
            modes+=[('JVM JIT',['java','-Xbatch','-cp',WORK,'Case_'+name,20],(str(count)+'\n')*21),
                   ('JVM interpreter',['java','-Xint','-cp',WORK,'Case_'+name,1],(str(count)+'\n')*2),
                   ('Python',[sys.executable,WORK/(name+'.py'),count],None),
                   ('Lua',[lua,WORK/(name+'.lua'),count],None),
                   ('C++',[WORK/(name+suffix)],(str(count)+'\n')*2)]
            vm_labels={label for _,_,label,_ in targets}
            samples={label:{'samples_ms':[],'checksum':str(expected)} for label,_,_ in modes}
            for repeat in range(args.repeats):
                shift=(repeat+round_index)%len(modes)
                for label,command,data in modes[shift:]+modes[:shift]:
                    fields=run(command,input=data).strip().split()
                    assert float(fields[-1])==expected,(name,label,fields,expected)
                    samples[label]['samples_ms'].append(float(fields[0]))
                    if label in vm_labels:
                        steps=int(fields[1]);assert samples[label].get('logical_steps',steps)==steps
                        samples[label]['logical_steps']=steps
            for value in samples.values(): value['median_ms']=statistics.median(value['samples_ms'])
            results[name]={'iterations':count,'configurations':samples}
            print(round_index+1,name,{label:round(v['median_ms'],4) for label,v in samples.items()},flush=True)
        report['results'][str(round_index+1)]=results
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    main()
