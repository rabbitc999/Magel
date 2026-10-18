"""验收 Magel 命名、双语工具提示、系统区域选择及程序输出隔离。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/magel-i18n'


def execute(*args, language=None, extra_env=None):
    """隔离区域环境，按 UTF-8 读取本地工具输出。"""
    env=os.environ.copy()
    for key in ('MAGEL_LANG','LC_ALL','LC_MESSAGES','LANG'):env.pop(key,None)
    if language is not None:env['MAGEL_LANG']=language
    if extra_env:env.update(extra_env)
    return subprocess.run([str(x) for x in args],cwd=ROOT,env=env,capture_output=True,encoding='utf-8',errors='strict',timeout=180)


def main():
    """检查解释、AOT、工程模板和两种显示语言。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=ROOT/'magel.exe')
    compiler=parser.parse_args().magel.resolve()
    WORK.mkdir(parents=True,exist_ok=True)
    unit=WORK/'unit.exe'
    result=execute('g++','-std=c++17','-finput-charset=UTF-8','-fexec-charset=UTF-8','-I',ROOT/'library',ROOT/'tests/magel_i18n_test.cpp','-o',unit)
    assert result.returncode==0,result
    assert execute(unit).returncode==0
    expected={'zh':'用法：','en':'Usage:'}
    for language,token in expected.items():
        result=execute(compiler,'help','lang',language)
        assert result.returncode==0 and token in result.stdout,(language,result)
        assert 'lang zh|en|auto' in result.stdout
        assert 'Magic-C' not in result.stdout and '.mc ' not in result.stdout
        result=execute(compiler,'lang',language,'new','help')
        assert result.returncode==0 and token in result.stdout,result
        result=execute(compiler,'test','help','lang',language)
        assert result.returncode==0 and '--backend' in result.stdout,result
        if language=='en':assert 'Usage:' in result.stdout,result
    for environment,token in [({'LC_ALL':'ja_JP.UTF-8'},expected['en']),({'LC_MESSAGES':'zh_TW.UTF-8'},expected['zh']),({'LANG':'C'},expected['en']),({'LANG':'fr_FR'},expected['en']),({'MAGEL_LANG':'ja','LC_ALL':'zh_CN'},expected['en'])]:
        result=execute(compiler,'help',extra_env=environment)
        assert result.returncode==0 and token in result.stdout,(environment,result)
    result=execute(compiler,'help','lang','en',extra_env={'MAGEL_LANG':'ja'})
    assert expected['en'] in result.stdout,result
    result=execute(compiler,'help','lang','auto',extra_env={'MAGEL_LANG':'en','LC_ALL':'ja_JP'})
    assert expected['en'] in result.stdout,result
    result=execute(compiler,'lang','fr','new',WORK/'must-not-exist')
    assert result.returncode==2 and not (WORK/'must-not-exist').exists(),result
    rejected=execute(compiler,'help','lang','ja')
    assert rejected.returncode==2,rejected
    result=execute(compiler,'--lang')
    assert result.returncode==2,result
    source=WORK/'中日文路径.mgl'
    source.write_text('int64 main(){std::lang::println("中文 / 日本語 / Magel");return 0;}\n',encoding='utf-8')
    for language in expected:
        result=execute(compiler,source,'--lang','ja',language=language)
        assert result.returncode==0 and result.stdout=='中文 / 日本語 / Magel\n' and not result.stderr,result
    bad=WORK/'readonly.mgl'
    bad.write_text('int64 main(){int64 x=1;int64 & r=&x;r=2;return 0;}\n',encoding='utf-8')
    codes=[]
    for language,word in {'zh':'只读','en':'read-only'}.items():
        result=execute(compiler,'check',bad,'json','lang',language)
        assert result.returncode!=0,result
        report=json.loads(result.stdout)['diagnostics'][0]
        assert word in report['message'],report
        assert report['source']==bad.read_text(encoding='utf-8').strip(),report
        codes.append(report['code'])
    assert codes==['MGL1203']*2,codes
    unknown=WORK/'unknown.mgl';unknown.write_text('int64 main(){string x;return 0;}\n',encoding='utf-8')
    for language,word in {'zh':'没有声明的类型','en':'Undeclared type'}.items():
        result=execute(compiler,'check',unknown,'json','lang',language)
        assert result.returncode!=0 and word in result.stdout and 'string' in result.stdout,result
    with tempfile.TemporaryDirectory(dir=WORK) as directory:
        target=Path(directory)/'hello-magel'
        result=execute(compiler,'new',target,'lang','en')
        assert result.returncode==0 and 'Created' in result.stdout,result
        assert (target/'src/main.mgl').exists() and (target/'magel.toml').exists()
        assert 'Magel application' in (target/'README.md').read_text(encoding='utf-8')
        result=execute(compiler,'new',target,'lang','en')
        assert result.returncode!=0 and 'already exists' in result.stderr,result
    native=WORK/'native.exe'
    result=execute(compiler,'aot',source,'out',native,'opt','0','lang','en')
    assert result.returncode==0 and 'AOT' in result.stdout,result
    for language in expected:
        result=execute(native,'--lang','ja',language=language)
        assert result.returncode==0 and result.stdout=='中文 / 日本語 / Magel\n' and not result.stderr,result
    panic=WORK/'panic.mgl'
    panic.write_text('void leaf(){panic("USER 中文 / 日本語");}\nint64 main(){leaf();return 0;}\n',encoding='utf-8')
    for language,word in {'zh':'调用栈','en':'Call stack'}.items():
        result=execute(compiler,'run',panic,language=language)
        assert result.returncode!=0 and 'USER 中文 / 日本語' in result.stderr and word in result.stderr,result
    checked=WORK/'checked.exe'
    result=execute(compiler,'aot',panic,'out',checked,'opt','0','checked',language='en')
    assert result.returncode==0,result
    for language,word in {'zh':'调用栈','en':'Call stack'}.items():
        result=execute(checked,language=language)
        assert result.returncode!=0 and 'USER 中文 / 日本語' in result.stderr and word in result.stderr,result
    print('Magel 双语验收通过：区域优先级、CLI/开发工具、诊断码、工程模板、解释与 AOT 用户输出')


if __name__=='__main__':main()
