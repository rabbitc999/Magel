"""交错重测 checked AOT 与 JDK 8/22、原生 C++、Lua、Python，保留逐次耗时与校验和。"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/aot-optimization-benchmark'
KERNELS=('scalar','fixed_array','heap_array','container','function_calls')


def main():
    """使用固定优化级别、运行时输入、独立预热与交错顺序进行对照。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=ROOT/'magel.exe')
    parser.add_argument('--baseline-executable',type=Path,help='旧版 AOT 产物；给出后追加 before 行并计算加速比')
    parser.add_argument('--baseline-commit',default='12c4dc4')
    parser.add_argument('--jdk8',type=Path,required=True)
    parser.add_argument('--jdk8-javac',type=Path,help='JDK 8 位为纯 JRE 时另行指定的 javac，将以 --release 8 编译')
    parser.add_argument('--jdk22',type=Path,required=True)
    parser.add_argument('--lua',type=Path,help='官方 Lua 解释器；给出后追加 Lua 对照')
    parser.add_argument('--python',type=Path,help='CPython 解释器；给出后追加 Python 对照')
    parser.add_argument('--cpp',type=Path,help='native C++17 源码；给出后以 -O2 -DNDEBUG 编译并追加 C++ 对照')
    parser.add_argument('--iterations',type=int,default=200000)
    parser.add_argument('--rounds',type=int,default=3)
    parser.add_argument('--repeats',type=int,default=3)
    parser.add_argument('--skip-build',action='store_true')
    parser.add_argument('--out',type=Path,default=ROOT/'doc/benchmarks/aot-optimization-2026-10-16.json')
    args=parser.parse_args();assert min(args.iterations,args.rounds,args.repeats)>0
    env=dict(os.environ,MAGEL_LANG='zh',MAGEL_NO_STDIN_INPUT='0')
    WORK.mkdir(parents=True,exist_ok=True)

    def run(command,**kwargs):
        """执行测试命令，失败时保存完整输出。"""
        result=subprocess.run(list(map(str,command)),cwd=ROOT,env=env,capture_output=True,encoding='utf-8',errors='replace',timeout=900,**kwargs)
        assert result.returncode==0,(command,result.stdout,result.stderr)
        return result

    compiler=args.magel.resolve();after=WORK/'after.exe'
    source=ROOT/'tests/fixtures/aot_optimization_benchmark.mgl'
    if not args.skip_build:
        result=run([compiler,'aot',source,'--out',after,'--opt-level=3','--checked','--keep-intermediate'])
        (WORK/'after-build.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    assert after.exists(),after
    java_source=ROOT/'tests/fixtures/AotOptimizationBenchmark.java'
    lua_source=ROOT/'tests/fixtures/AotOptimizationBenchmark.lua'
    python_source=ROOT/'tests/fixtures/AotOptimizationBenchmark.py'
    for label,jdk,javac,expected,extra in [('jdk8',args.jdk8,args.jdk8_javac,'1.8.',['--release','8']),('jdk22',args.jdk22,None,'22.',[])]:
        jdk=jdk.resolve();version=run([jdk/'bin/java.exe','-version']);assert expected in version.stderr,version
        javac=javac.resolve() if javac else jdk/'bin/javac.exe'
        dest=WORK/label/'classes';dest.mkdir(parents=True,exist_ok=True)
        run([javac,'-encoding','UTF-8',*extra,'-d',dest,java_source])
    binaries={'after':after}
    if args.baseline_executable:
        binaries={'before':args.baseline_executable.resolve(),**binaries}
    commands={label:[path] for label,path in binaries.items()}
    versions={}
    for label,jdk in [('jdk8',args.jdk8),('jdk22',args.jdk22)]:
        jdk=jdk.resolve();versions[label]=run([jdk/'bin/java.exe','-version']).stderr.strip()
        commands[label]=[jdk/'bin/java.exe','-Xms256m','-Xmx256m','-Xbatch','-cp',WORK/label/'classes','AotOptimizationBenchmark']
    script_versions={}
    if args.lua:
        lua=args.lua.resolve();result=run([lua,'-v'])
        script_versions['lua']=(result.stdout+result.stderr).strip()
        commands['lua']=[lua,lua_source]
    if args.python:
        python=args.python.resolve();result=run([python,'-V'])
        script_versions['python']=(result.stdout+result.stderr).strip()
        commands['python']=[python,'-u',python_source]
    cpp_binary=None
    if args.cpp:
        cpp_source=args.cpp.resolve();dest=WORK/'cpp';dest.mkdir(parents=True,exist_ok=True)
        cpp_binary=dest/(cpp_source.stem+'.exe')
        run(['g++','-std=c++17','-O2','-DNDEBUG',cpp_source,'-o',cpp_binary])
        script_versions['cpp']=run(['g++','--version']).stdout.splitlines()[0]
        commands['cpp']=[cpp_binary]
    value=1
    for i in range(args.iterations):value=(value*17+i)%1000000007
    expected={k:(value if k in ('scalar','function_calls') else args.iterations*(args.iterations+1)//2) for k in KERNELS}
    samples={label:{k:[] for k in KERNELS} for label in commands};raw=[]
    labels=list(commands)
    for round_index in range(args.rounds):
        order=labels[round_index%len(labels):]+labels[:round_index%len(labels)]
        for label in order:
            result=run(commands[label],input=f'{args.iterations}\n{args.repeats}\n')
            lines=result.stdout.splitlines();assert len(lines)==len(KERNELS)*args.repeats,(label,result)
            observed=[]
            for line in lines:
                kernel,nanos,checksum=line.split();nanos=int(nanos);checksum=int(checksum)
                assert checksum==expected[kernel] and nanos>0,(label,line,expected)
                samples[label][kernel].append(nanos/1e6);observed.append({'kernel':kernel,'nanoseconds':nanos,'checksum':checksum})
            raw.append({'round':round_index,'implementation':label,'samples':observed})
            print('completed',round_index,label,flush=True)
    medians={label:{k:statistics.median(v) for k,v in data.items()} for label,data in samples.items()}
    report={'date':datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).isoformat(),'version':(ROOT/'VERSION').read_text().strip(),'baseline_commit':args.baseline_commit,'candidate_base':run(['git','rev-parse','HEAD']).stdout.strip(),'candidate_dirty':run(['git','status','--porcelain']).stdout.splitlines(),'platform':platform.platform(),'cpu':platform.processor(),'host_compiler':run(['g++','--version']).stdout.splitlines()[0],'java_versions':versions,'runtime_versions':script_versions,'iterations':args.iterations,'rounds':args.rounds,'repeats_per_round':args.repeats,'configuration':{'aot':'-O3 --checked; default reflection enabled; full quad optimizer; no unsafe fast-math','java':'-Xms256m -Xmx256m -Xbatch; 20 warmup iterations per process','timing':'monotonic nanoseconds around each kernel only; excludes startup, compilation, parsing and printing; includes kernel array/list initialization and cleanup','aot_warmup':1,'java_array_note':'Java fixed_array and heap_array both use long[64]; ArrayList<Long> uses boxed elements, Magel uses List<int64>; Python and Lua back both array kernels with their native dynamic sequence, so fixed_array and heap_array are the same kernel in those two columns; results compare algorithms, not identical layouts','cpp':'native C++17 at -O2 -DNDEBUG, no -ffast-math, no LTO, no closed-form sum; the five kernels are noinline so they cannot be folded into the timing wrapper, while callStep inside function_calls may inline','lua':'official Lua 5.5.1 source built with its own mingw Makefile target at -O2, no JIT; os.clock bounds each kernel only; 1 warmup round; Windows os.clock resolution commonly lands on whole milliseconds','python':'CPython, time.perf_counter_ns bounds each kernel only; 1 warmup round; no JIT'},'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'java_source_sha256':hashlib.sha256(java_source.read_bytes()).hexdigest(),'binary_sha256':{label:hashlib.sha256(path.read_bytes()).hexdigest() for label,path in binaries.items()},'binary_sizes':{label:path.stat().st_size for label,path in binaries.items()},'expected_checksums':expected,'samples_ms':samples,'median_ms':medians,'raw':raw}
    if args.baseline_executable:
        report['speedup_before_over_after']={k:medians['before'][k]/medians['after'][k] for k in KERNELS}
    report['compiler_sha256']=hashlib.sha256(compiler.read_bytes()).hexdigest()
    report['optimization_source_sha256']={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ['source/magel-emit/aot.cpp','source/magel-emit/main.cpp','library/stackvm/stackvm.hpp','library/stackvm/parts/aot_bridge.cpp']}
    if args.lua:
        report['lua_source_sha256']=hashlib.sha256(lua_source.read_bytes()).hexdigest()
        report['lua_binary_sha256']=hashlib.sha256(args.lua.resolve().read_bytes()).hexdigest()
    if args.python:
        report['python_source_sha256']=hashlib.sha256(python_source.read_bytes()).hexdigest()
    if args.jdk8_javac:
        javac=args.jdk8_javac.resolve()
        report['jdk8_compiler']={'javac':str(javac),'sha256':hashlib.sha256(javac.read_bytes()).hexdigest(),'extra_args':['--release','8'],'note':'the JDK 8 column executes on a JRE-only install, so its single class file is produced by this javac with --release 8'}
    if args.cpp:
        report['cpp_source_sha256']=hashlib.sha256(args.cpp.resolve().read_bytes()).hexdigest()
        report['cpp_binary_sha256']=hashlib.sha256(cpp_binary.read_bytes()).hexdigest()
    release=WORK/'jdk8-release.json'
    if release.exists():
        metadata=json.loads(release.read_text(encoding='utf-8'))
        archive=next(asset for asset in metadata['assets'] if asset['name'].startswith('OpenJDK8U-jdk_x64_windows_hotspot_') and asset['name'].endswith('.zip'))
        report['jdk8_distribution']={'release':metadata['tag_name'],'url':archive['browser_download_url'],'archive_sha256':hashlib.sha256((WORK/archive['name']).read_bytes()).hexdigest(),'checksum_verified':True}
    args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'median_ms':medians,'speedup_before_over_after':report.get('speedup_before_over_after')},indent=2),flush=True)


if __name__=='__main__':main()
