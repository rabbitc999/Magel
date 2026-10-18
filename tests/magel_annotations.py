"""注解前端回归：声明/绑定、常量范围、位置、关闭开关及真实 VM/AOT 执行。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT/('magel.exe' if os.name == 'nt' else 'magel'))
    parser.add_argument('--front-end-only', action='store_true')
    parser.add_argument('--cxx', default='g++')
    args = parser.parse_args()
    compiler = args.magel.resolve()
    work = ROOT/'build/magel-annotations'
    work.mkdir(parents=True, exist_ok=True)
    checks = 0
    with tempfile.TemporaryDirectory(prefix='suite-', dir=work,
                                     ignore_cleanup_errors=(os.name == 'nt')) as temporary:
        directory = Path(temporary)
        source = directory/'case.mgl'
        def run(*arguments, expected=0):
            result = subprocess.run([str(compiler), *map(str, arguments)], cwd=directory, capture_output=True,
                                    text=True, encoding='utf-8', errors='replace', timeout=180)
            assert result.returncode == expected, (arguments, result.returncode, result.stdout, result.stderr)
            return result
        def check(text, error=None, reflection=True, location=None, filename=None):
            nonlocal checks
            path = filename or source
            path.write_text(text, encoding='utf-8')
            options = [] if reflection is None else ['--reflection' if reflection else '--no-reflection']
            result = run(*options, '--diagnostics=json', path, '--out', directory/'absent', expected=1 if error else 0)
            report = json.loads(result.stdout)
            if error:
                diagnostic = report['diagnostics'][0]
                if error == '语法错误':
                    assert diagnostic['code'] == 'MGL1001', diagnostic
                else:
                    assert error in diagnostic['message'], diagnostic
                if location:
                    assert (diagnostic['span']['line'], diagnostic['span']['column']) == location, diagnostic
                    assert Path(diagnostic['span']['file']).resolve() == path.resolve(), diagnostic
            else:
                assert report == {'schemaVersion': 1, 'diagnostics': []}, report
            assert not list(directory.glob('absent*')), '检查模式生成了产物'
            checks += 1
            return result
        main_fn = 'int64 main(){return 0;}\n'
        check('@annotation DefaultEnabled\n@DefaultEnabled\n'+main_fn, reflection=None)
        check('@annotation Tag\n@Tag\n'+main_fn)
        check('@annotation Empty()\n@Empty()\n'+main_fn)
        check('@annotation Route(path: String, retries: int32=3, cache: bool=true)\n@Route("/user", cache:false)\n'+main_fn)
        check('@annotation Auth(role: String, level: int32)\n@Auth(level:2, role:"admin")\n'+main_fn)
        check('@annotation A(x:int32=2, y:int32)\n@A(y:3)\n'+main_fn)
        check('@annotation http::Route(path:String)\n@http::Route("/user")\n'+main_fn)
        check('@annotation A(value:String="a\\n\\t\\0")\n@A\n'+main_fn)
        check('@annotation A(value:char=\'\\n\')\n@A\n'+main_fn)
        for kind, low, high in [('int8',-128,127),('int16',-32768,32767),('int32',-2147483648,2147483647),
                                ('int64',-9223372036854775808,9223372036854775807),('uint8',0,255),
                                ('uint16',0,65535),('uint32',0,4294967295),('uint64',0,18446744073709551615)]:
            def literal(value):
                return ('-' if value < 0 else '')+hex(abs(value)) if abs(value)>9223372036854775807 else str(value)
            for value in (low,high): check(f'@annotation A(v:{kind})\n@A({literal(value)})\n'+main_fn)
            for value in (low-1,high+1):
                error = '整数位数超出 64 位' if abs(value)>18446744073709551615 else '超出声明类型范围'
                check(f'@annotation A(v:{kind})\n@A({literal(value)})\n'+main_fn,error)
        check('@annotation A(v:uint64)\n@A(0xFFFFFFFFFFFFFFFF)\n'+main_fn)
        check('@annotation A(v:uint8)\n@A(b_11111111)\n'+main_fn)
        check('@annotation A(v:float32=-1.5)\n@A\n'+main_fn)
        check('@annotation A(v:float64=2.0e308)\n'+main_fn, '超出声明类型范围')
        check('@annotation A(v:float32=3.5e38)\n'+main_fn, '超出声明类型范围')
        check('@annotation A(v:int32)\n@A((int32)2)\n'+main_fn)
        check('@annotation A(v:int32)\n@A((int64)2)\n'+main_fn, '类型不匹配')
        for kind,value in [('bool','1'),('int32','true'),('String','2'),('float64','2'),('int64','2.0'),('char','"a"')]:
            check(f'@annotation A(v:{kind})\n@A({value})\n'+main_fn,'类型不匹配')
        for declaration in ['@annotation A(v:void)','struct S{};\n@annotation A(v:S)',
                            'interface I{};\n@annotation A(v:I)', '@annotation A(v:int64 mut&)',
                            '@annotation A(v:int64 fn())', 'enum E{X};\n@annotation A(v:E)',
                            '@annotation A(v:List<int64>)']:
            check(declaration+'\n'+main_fn, '只支持基础类型')
        for value in ('1+2','read()','n'):
            check('const int64 n=2; int64 read(){return 2;}\n@annotation A(v:int64='+value+')\n'+main_fn,'编译期常量字面量')
        check('@annotation A(v:char=\'ab\')\n'+main_fn,'一个字节')
        check('@annotation A(v:String="\\q")\n'+main_fn,'不认识的转义')
        check('@annotation A(x:int32,x:int32)\n'+main_fn,'参数重名')
        check('@annotation A\n@annotation A\n'+main_fn,'不可重新声明',location=(2,1))
        check('@annotation A\nstruct A{};\n'+main_fn,'名称与类型冲突')
        check('struct A{};\n@annotation A\n'+main_fn,'名称与类型冲突')
        check('@annotation String\n'+main_fn,'名称与类型冲突')
        check('  @Unknown\n'+main_fn,'先声明后使用',location=(1,3))
        check('@A\nint64 f(){return 0;}\n@annotation A\n'+main_fn,'先声明后使用',location=(1,1))
        check('@annotation A(x:int32,y:String="ok")\n@A\n'+main_fn,'缺少必填参数')
        check('@annotation A\n@A(1)\n'+main_fn,'参数个数过多')
        check('@annotation A(x:int32)\n@A(z:2)\n'+main_fn,'没有这个命名参数')
        check('@annotation A(x:int32)\n@A(1,x:2)\n'+main_fn,'参数重复赋值')
        check('@annotation A(x:int32,y:int32)\n@A(x:1,2)\n'+main_fn,'位置参数不能')
        check('@annotation A\n@A int64 n=1;\n'+main_fn,'不支持注解')
        check('@annotation A\n'+main_fn, '功能未开启', reflection=False, location=(1,1))
        check('@A\n'+main_fn, '功能未开启', reflection=False, location=(1,1))
        for text in ('@annotation A\nint64 main(){@A int64 x=1;return 0;}',
                     'int64 main(){@annotation A return 0;}', '@annotation A\nstruct S{@annotation B int64 x;};\n'+main_fn,
                     '@annotation A\nint64 main(){@A {return 0;}}'):
            check(text, '语法错误')
        # 泛型模板首版不支持前置注解，保留原文报错；普通泛型与被注解的具体声明共存。
        check('@annotation A\nstruct Box<T>{T value;};\n@A\nint64 f(Box<int64> x){return x.value;}\n'+main_fn)
        check('@annotation A\n@A\nstruct Box<T>{T value;};\n'+main_fn,'语法错误')
        check('@annotation A\n@A()\n(int64,int64) pair(){return (1,2);}\n'+main_fn)
        (directory/'magel.toml').write_text('[package]\nname="annotations"\nversion="0.1.0"\n[magel]\nimport_paths=["."]\n',encoding='utf-8')
        (directory/'one.mgl').write_text('module one;\n@annotation A(v:String="one")\n@A\nstruct S{};\n',encoding='utf-8')
        (directory/'two.mgl').write_text('module two;\n@annotation A(v:String="two")\n',encoding='utf-8')
        check('import one;\n@one::A("root")\n'+main_fn)
        check('import one;\nimport two;\n@A\n'+main_fn,'有歧义')
        check('import one;\n@A\n'+main_fn)
        included = directory/'included.mgl'
        included.write_text('   @Missing\nint64 f(){return 0;}\n',encoding='utf-8')
        source.write_text('%INCLUDE "included.mgl"\n'+main_fn,encoding='utf-8')
        diagnostic=json.loads(run('--reflection','--diagnostics=json',source,expected=1).stdout)['diagnostics'][0]
        assert Path(diagnostic['span']['file']).resolve() == included.resolve(),diagnostic
        assert (diagnostic['span']['line'],diagnostic['span']['column'])==(1,4),diagnostic
        checks += 1
        # 成功的字符串常量不得把精确列号泄露到普通语义诊断。
        check('@annotation A(v:String="ok")\n@A\nint64 main(){const int64 x=1;x=2;return 0;}','不可变变量')
        check('@annotation A(v:String="ok")\n@A\n'+main_fn)
        check('@annotation A(v:std::lang::String="ok")\n@A\n'+main_fn)
        check('type F = uint64;\n@annotation F\n'+main_fn,'名称与类型冲突')
        unit=directory/('annotation-unit.exe' if os.name=='nt' else 'annotation-unit')
        command=[args.cxx,'-std=c++17','-O1','-pthread','-DMAGEL_CUSTOM_ALLOCATOR=1',
                 '-DMAGEL_WITH_REFLECTION=1','-DSTACKVM_WITH_REFLECTION=1',
                 '-I'+str(ROOT), '-I'+str(ROOT/'source'), '-I'+str(ROOT/'library'),
                 '-I'+str(ROOT/'library/simple_ast'), '-I'+str(ROOT/'library/stackvm'),
                 '-I'+str(ROOT/'library/preprocess'), ROOT/'tests/magel_annotations_test.cpp',
                 ROOT/'library/json/json.cpp',
                 ROOT/'build/magel-frontend/libparser.a', ROOT/'build/magel-runtime/libsimple_ast.a',
                 ROOT/'build/magel-stackvm/libstackvm.a',ROOT/'build/magel-runtime/libpreprocess.a',
                 '-lm','-o',unit]
        compiled=subprocess.run(list(map(str,command)),capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
        assert compiled.returncode==0,(compiled.stdout,compiled.stderr)
        tested=subprocess.run([str(unit)],cwd=directory,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
        assert tested.returncode==0,(tested.stdout,tested.stderr)
        assert '注解 AST 单元测试通过' in tested.stdout,tested.stdout
        checks+=1
        sample=ROOT/'example/reflection/annotations.mgl'
        check(sample.read_text(encoding='utf-8'))
        if not args.front_end_only:
            for backend in ('', 'aot'):
                target=directory/((backend or 'direct')+('.exe' if os.name=='nt' else ''))
                options=[backend, '--opt-level=0'] if backend else ['run']
                result=run(*options,'--reflection',sample,'--out',target)
                if backend:
                    result=subprocess.run([str(target)],cwd=directory,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
                    assert result.returncode==0,(backend,result.stdout,result.stderr)
                assert result.stdout=='annotations 42 7\n', (backend,result.stdout,result.stderr)
                checks+=1
        print(f'注解前端：{checks} 项检查通过')


if __name__=='__main__':
    main()
