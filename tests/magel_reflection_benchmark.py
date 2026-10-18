"""编译真实 MAGEL AOT 递推循环，报告自由函数/实例/接口的静态及反射进程时间。"""
from pathlib import Path
import argparse
import json
import os
import platform
import statistics
import subprocess
import time
ROOT=Path(__file__).resolve().parents[1]
EXE='.exe' if os.name=='nt' else ''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=ROOT/('magel-reflection'+EXE))
    parser.add_argument('--iterations',type=int,default=1000)
    parser.add_argument('--repeats',type=int,default=5)
    parser.add_argument('--out',type=Path,default=ROOT/'build/reflection-benchmark')
    args=parser.parse_args()
    assert args.iterations>0 and args.repeats>=3
    work=args.out.resolve();work.mkdir(parents=True,exist_ok=True)
    expected=7
    for _ in range(args.iterations):expected=(expected*1664525+1013904223)&0x7fffffff
    report={'iterations':args.iterations,'repeats':args.repeats,'backend':'AOT','platform':platform.platform(),
            'timing':'Python perf_counter, whole process wall time; same executable, startup included; empty process baseline reported separately',
            'compiler':str(args.magel.resolve()),'results':{}}
    for category in ('free','instance','interface'):
        declaration='@Reflect int64 evolve(int64 value){return (value*1664525+1013904223)&0x7fffffff;}'
        setup='';cleanup='';static='evolve(value)';dynamic='std::reflect::call("evolve",values)'
        if category!='free':
            declaration='@Reflect interface Step {int64 step(int64 value);};\n@Reflect struct Kernel : Step {int64 bias;Kernel(){self.bias=1013904223;}int64 step(int64 value){return (value*1664525+self.bias)&0x7fffffff;}};'
            setup='Kernel mut& kernel=new();';cleanup='delete kernel;'
            static='kernel.step(value)';dynamic='std::reflect::callMethod<Kernel>(kernel,"step",values)'
            if category=='interface':
                setup+='Any boxed;boxed.set(&kernel);Step mut& view=boxed.getReference();'
                static='view.step(value)';dynamic='std::reflect::callInterface<Step>(view,"step",values)'
        source='import std::reflect;\n'+declaration+'\nint64 main(List<String> arguments){'+setup+'int64 value=7;List<std::reflect::ReflectValue> values;values.push(std::reflect::ReflectValue::I64(value));'
        source+='if(arguments[0]=="static"){for(int64 i=0;i<'+str(args.iterations)+';i++){value='+static+';}}'
        source+='if(arguments[0]=="reflect"){for(int64 i=0;i<'+str(args.iterations)+';i++){values[0]=std::reflect::ReflectValue::I64(value);value='+dynamic+'.unwrap().asI64().unwrap();}}'
        source+=cleanup+'println("{}",value);return 0;}\n'
        path=work/(category+'.mgl');path.write_text(source,encoding='utf-8');executable=work/(category+EXE)
        print('[构建] '+category,flush=True)
        build=subprocess.run([str(args.magel.resolve()),'aot',str(path),'--reflection','--opt-level=2','--out',str(executable)],cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=300)
        assert build.returncode==0,(category,build.stdout,build.stderr)
        samples={mode:[] for mode in ('empty','static','reflect')}
        def measure(mode):
            start=time.perf_counter();result=subprocess.run([str(executable),mode],cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
            elapsed=time.perf_counter()-start
            assert result.returncode==0 and not result.stderr and result.stdout==str(7 if mode=='empty' else expected)+'\n',(category,mode,result.stdout,result.stderr)
            return elapsed
        for mode in samples:measure(mode)
        # 交替运行以减少固定模式次序对温度和频率的影响。
        for repeat in range(args.repeats):
            for mode in (('empty','static','reflect') if repeat%2==0 else ('reflect','static','empty')):samples[mode].append(measure(mode))
        medians={mode:statistics.median(values) for mode,values in samples.items()}
        report['results'][category]={'samples_seconds':samples,'median_seconds':medians,
          'static_ns_per_call_including_startup':medians['static']*1e9/args.iterations,
          'reflect_ns_per_call_including_startup':medians['reflect']*1e9/args.iterations,
          'ratio_including_startup':medians['reflect']/medians['static'],
          'static_ns_per_call_minus_empty_baseline':max(0,medians['static']-medians['empty'])*1e9/args.iterations,
          'reflect_ns_per_call_minus_empty_baseline':max(0,medians['reflect']-medians['empty'])*1e9/args.iterations,
          'checksum':expected}
        print(category+': '+json.dumps(report['results'][category],ensure_ascii=False),flush=True)
        (work/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('报告：'+str(work/'results.json'),flush=True)
if __name__=='__main__':main()
