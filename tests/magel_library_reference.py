"""核对库参考页、声明覆盖、链接和完整示例；反射需可选的专用工具链。"""
from pathlib import Path
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import library_reference as reference
from library_reference_examples import EXAMPLES


def run(command, allowed_stderr=""):
    """执行命令，失败保留标准输出、错误与实际退出码。"""
    result=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,
                          text=True,encoding='utf-8',errors='replace',timeout=180)
    assert result.returncode==0 and result.stderr==allowed_stderr,(command,result.returncode,result.stdout,result.stderr)
    return result.stdout


def check_structure():
    """检查扫描中易遗漏的元组、嵌套泛型、枚举载荷和多行声明边界。"""
    fixture='''module fixture;
enum Error { Bad, Empty }
enum Value<T> { Some(T item), None;
    Result<T,Error> unwrapOr(T value) { return Result<T,Error>::Ok(value); }
};
struct Box<T> {
    static Box<T> create(T value) { Box<T> box; return box; }
    (int64, Option<T>) next(T fn(T) mapper) { return 1,Option<T>::None; }
};
Result<std::lang::List<T>, Error> convert<T>(
    std::lang::List<T> values, String url="http://localhost/");
int64 __hidden();
'''
    entries=reference.declarations(ROOT/'source/std/lang.mgl',fixture)
    functions=[r for r in entries if r['kind']=='function']
    assert [(r['owner'],r['name']) for r in functions]==[('Value','unwrapOr'),('Box','create'),('Box','next'),('','convert')]
    extra=reference.declarations(ROOT/'source/std/lang.mgl','module fixture; int64 fn(int64) make(String s="a,  b"); int64 constant=other();')
    assert [r['name'] for r in extra if r['kind']=='function']==['make']
    assert reference.parameters(extra[0]['signature'])==[('s','String s','"a,  b"')]
    derived=reference.declarations(ROOT/'source/std/lang.mgl','module fixture;\n#derive(Equal, Hash, Clone)\nenum Kind { Integer, Symbol };')
    assert any(r['kind']=='enum' and r['name']=='Kind' for r in derived)
    assert not any(r['kind']=='function' and r['name']=='derive' for r in derived)
    assert functions[2]['result']=='(int64, Option<T>)'
    assert reference.parameters(functions[3]['signature'])[-1]==('url','String url','"http://localhost/"')
    assert any(r['signature']=='Bad, Empty' for r in entries)
    index=json.loads((ROOT/'doc/library/api-index.json').read_text(encoding='utf-8'))
    assert len(index)==sum(r['kind']=='function' and not r['owner'].startswith('__') for entries in reference.inventory().values() for r in entries)
    assert len({(r['module'],r['name'],r['signature']) for r in index})==len(index)
    symbols=json.loads((ROOT/'doc/library/symbol-index.json').read_text(encoding='utf-8'))
    assert symbols and all(r['documentation'] for r in symbols)
    assert any(r['name']=='DateTime::year' and '公历年份' in r['documentation'] for r in symbols)
    assert any(r['name']=='LuFactorization' and r['kind']=='struct' for r in symbols)
    assert not any(r['module']=='std' and r['name']=='pause' for r in index)
    for critical in ('mysql','maria','bytesToFloat32','parseInt64','getStructInfo','zipValue','tryFold'):
        assert any(r['name'].split('::')[-1]==critical for r in index),critical
    for page in (ROOT/'doc/library').glob('*.md'):
        text=page.read_text(encoding='utf-8')
        text=re.sub(r'```.*?```','',text,flags=re.S)
        for target in re.findall(r'\]\(([^)]+)\)',text):
            file,_,fragment=target.partition('#')
            path=page.parent/file if file else page
            assert path.exists(),(page,target)
            if fragment.startswith('api-'):
                assert 'id="'+fragment+'"' in path.read_text(encoding='utf-8'),(page,target)


def main():
    """对文档代码执行真实解释运行，复用正式样例的期望输出。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=Path(os.environ.get('MAGEL_COMPILER',ROOT/'magel.exe' if os.name=='nt' else ROOT/'magel')))
    parser.add_argument('--reflection-compiler',type=Path,help='编入前端和 VM 反射支持的可选工具链')
    args=parser.parse_args()
    run([sys.executable,ROOT/'tools/library_reference.py','--check'])
    check_structure()
    passed=0
    with tempfile.TemporaryDirectory(prefix='library-reference-',dir=ROOT/'build') as temporary:
        work=Path(temporary)
        for example in sorted(set(EXAMPLES.values())):
            source=ROOT/example
            name=source.stem
            compiler=args.magel
            flags=[]
            if example==EXAMPLES['std::reflect']:
                if not args.reflection_compiler:
                    print('[跳过] reflect：需 --reflection-compiler 指定启用反射的工具链',flush=True)
                    continue
                compiler=args.reflection_compiler
                flags=['--reflection']
            expected=ROOT/'example/expected'/name
            if expected.with_suffix('.threads').exists(): flags+=['--threads']
            if expected.with_suffix('.in').exists(): flags+=['--input',expected.with_suffix('.in').read_text(encoding='utf-8')]
            if example==EXAMPLES['std::io']: flags+=['--',work/'io.txt']
            output=run([compiler.resolve(),'run',source,'--out',work/name,*flags],
                       allowed_stderr='stackvm：按回车继续…' if name=='std_pause' else '')
            expected_text=expected.with_suffix('.txt')
            if expected_text.exists(): assert output==expected_text.read_text(encoding='utf-8'),(name,output)
            elif name in ('io','sqlite','watchdog','reflect'): assert output==name+' API ok\n',(name,output)
            passed+=1
            print('[通过] '+example,flush=True)
    print(f'库参考文档、重载清单、链接与 {passed} 个完整程序验证通过。')


if __name__=='__main__':
    main()
