#!/usr/bin/env python3
"""核心字符串唯一规范名、限定省略、旧关键字移除及解释与 AOT值语义回归。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
EXE='.exe' if os.name=='nt' else ''


def main():
    """检查语法、名字遮蔽和复合类型身份，运行同一值语义样例。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=ROOT/('magel'+EXE))
    args=parser.parse_args()
    count=0
    def run(command,success=True):
        nonlocal count
        r=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True,
                         encoding='utf-8',errors='replace',timeout=240)
        assert (r.returncode==0)==success,(command,r.stdout,r.stderr)
        count+=1
        return r
    with tempfile.TemporaryDirectory(prefix='string-type-',dir=ROOT/'build') as temporary:
        work=Path(temporary)
        source=work/'case.mgl'
        for declaration in ('string value="old";', 'List<string> values;',
                            '(string,int64) value=("old",(int64)1);',
                            'string const& value=&"old";'):
            source.write_text('int64 main(){'+declaration+'return 0;}\n',encoding='utf-8')
            result=run([args.magel,'--diagnostics=json',source],False)
            report=json.loads(result.stdout)
            assert report['diagnostics'],report
        for expression in ('(int64,2)', '(std::lang::String,2)', '(1,2,3]'):
            source.write_text('int64 main(){auto invalid='+expression+';return 0;}\n',encoding='utf-8')
            run([args.magel,'--check',source],False)
        source.write_text('struct string{int64 value;}; int64 main(){string x;x.value=4;assert(x.value==4);return 0;}\n',encoding='utf-8')
        run([args.magel,'run', source,'--out',work/'ordinary'])
        source.write_text('struct String{int64 value;int64 extra;}; struct Box<T>{T value;};\n'
                          'int64 main(){String local;local.value=7;std::lang::String core="text";'
                          'Box<String> first;first.value=local;Box<std::lang::String> second;second.value=core;'
                          'assert(first.value.value==7 && second.value=="text");assert(isType<String>(&first.value) && !isType<std::lang::String>(&second.value));return 0;}\n',encoding='utf-8')
        run([args.magel,'run', source,'--out',work/'shadow'])
        source.write_text('struct Box<T>{T value;}; '
                          'int64 identity(int64 value){return value;} '
                          'int64 main(){Box<int64 fn(int64)> box;box.value=identity;'
                          'assert(box.value(7)==7);return 0;}\n',encoding='utf-8')
        run([args.magel,'run', source,'--out',work/'function-type'])
        sample=ROOT/'example/samples/string_type_name.mgl'
        for backend in ('','aot'):
            target=work/((backend or 'direct')+EXE)
            options=[backend,'--opt-level=0'] if backend else ['run']
            result=run([args.magel,*options,sample,'--out',target])
            if backend:result=run([target])
            assert result.stdout=='String type OK\n' and not result.stderr,(backend,result.stdout,result.stderr)
        print(f'字符串类型命名：{count} 项检查通过')


if __name__=='__main__':
    main()
