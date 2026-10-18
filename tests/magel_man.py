"""验证内嵌终端手册、共享索引、三级解析、搜索、双语诊断与示例执行。"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
COMPILER=Path(os.environ.get('MAGEL_BIN',str(ROOT/('magel.exe' if os.name=='nt' else 'magel'))))


def main():
    """把编译器单独复制到空目录，确认查询不读取仓库或启动 Python。"""
    data=json.loads((ROOT/'tools/magel_stdlib_api.json').read_text(encoding='utf-8'))
    with tempfile.TemporaryDirectory(prefix='magel-man-') as directory:
        work=Path(directory)
        compiler=work/COMPILER.name;shutil.copy2(COMPILER,compiler)
        env=dict(os.environ,MAGEL_PYTHON='__missing_python__',MAGEL_LANG='zh',NO_COLOR='1')
        def run(*arguments,code=0):
            result=subprocess.run([str(compiler),'man',*arguments],cwd=work,env=env,
                capture_output=True,text=True,encoding='utf-8',timeout=90)
            assert result.returncode==code,(arguments,result.stdout,result.stderr)
            return result
        def query(*arguments,code=0):
            if not arguments or arguments[0] not in ('book','keyword','skill','help'):
                arguments=('api',*arguments)
            result=run('--json',*arguments,code=code)
            assert not result.stderr,result.stderr
            value=json.loads(result.stdout)
            assert value['schemaVersion']==1 and isinstance(value['diagnostics'],list),value
            return value
        assert run().stdout==run('help').stdout==run('--help').stdout
        assert json.loads(run('--json').stdout)['kind']=='help'
        book=query('book')
        assert len(book['chapters'])==28,book
        exported=work/'book'
        assert Path(book['directory'])==exported and book['exportedFiles']==35,book
        for path in (ROOT/'doc/book').glob('*.md'):
            assert (exported/path.name).read_text(encoding='utf-8')==path.read_text(encoding='utf-8')
        for path in (ROOT/'doc/book/images').glob('*'):
            assert (exported/'images'/path.name).read_bytes()==path.read_bytes()
        sentinel=exported/'my-notes.txt';sentinel.write_text('keep',encoding='utf-8')
        query('book')
        assert sentinel.read_text(encoding='utf-8')=='keep'
        chapter=query('book','24')['chapter']
        assert chapter['content']==(ROOT/'doc/book/24-学生信息管理系统.md').read_text(encoding='utf-8')
        assert 'student-system-login.png' in chapter['content']
        rendered=run('book','24').stdout
        assert '[图片](images/student-system-login.png)' in rendered and '![' not in rendered,rendered
        assert query('book','6')['chapter']['id']=='06'
        assert query('book','A')['chapter']['id']=='A'
        assert query('book','不存在',code=1)['diagnostics'][0]['code']=='MGL1701'
        keyword=query('keyword','break')['keyword']
        assert 'break level' in keyword['documentation'] and 'yield 值;' in keyword['documentation'] and 'BREAK' in keyword['syntax'],keyword
        macro = json.loads(run('macro', '--json').stdout)['keyword']
        assert macro['name'] == 'macro' and '..Types' in macro['syntax']
        assert all(text in macro['documentation'] for text in ('跨任意多行', '宏Token分组不匹配', '$end_macro', '>>'))
        assert '多括号' in run('macro').stdout
        assert 'struct' in query('keyword')['keywords']
        assert '__typeTag' not in query('keyword')['keywords']
        assert 'break level' in query('keyword','BREAK')['keyword']['documentation']
        assert 'match' in query('keyword','_')['keyword']['documentation']
        assert 'demo_counter_from_uint' in query('keyword','impl')['keyword']['documentation']
        assert '1.0f' in query('keyword','float32')['keyword']['documentation']
        assert '1u' in query('keyword','uint64')['keyword']['documentation']
        assert '只是兼容别名' not in macro['documentation']
        assert query('keyword','not_a_keyword',code=1)['diagnostics'][0]['code']=='MGL1701'
        skill=(ROOT/'skills/magel-programming/SKILL.md').read_text(encoding='utf-8')
        assert run('skill').stdout==skill
        assert query('skill')['content']==skill
        run('book','6','--run',code=2)
        for target in ('std', 'std::text::Text'):
            missing_api=run(target,code=2)
            assert 'magel man api '+target in missing_api.stderr and not missing_api.stdout,missing_api
            report=json.loads(run('--json',target,code=2).stdout)
            assert report['kind']=='usage' and report['diagnostics'][0]['code']=='MGL1701' and 'candidates' not in report,report
        run('--search','split',code=2)
        assert query('std')['kind']=='modules'
        modules=query()
        assert {m['name'] for m in modules['modules']}==set(data['modules'])
        grouped=[m['name'] for g in modules['groups'] for m in g['modules']]
        assert len(grouped)==len(set(grouped))==len(data['modules']),grouped
        overview=query('std::text')
        assert overview['kind']=='module'
        assert overview['documentation']==(ROOT/'doc/library/std-text.md').read_text(encoding='utf-8')
        expected=[r['name'] for r in data['functions']+data['symbols'] if r['module']=='std::text']
        assert sorted(r['name'] for r in overview['symbols'])==sorted(expected)
        owner=query('std::text::Text')
        assert any(r['kind']=='struct' and r['name']=='Text' for r in owner['symbols']),owner
        assert any(r['name']=='Text::split' for r in owner['symbols'])
        symbol=query('std::text::Text::split')
        records=[r for r in data['functions'] if r['module']=='std::text' and r['name']=='Text::split']
        assert len(symbol['symbols'])==len(records)>0
        entry=symbol['symbols'][0]
        assert entry['parameters']==records[0]['parameters']
        assert entry['returns']==records[0]['returns'] and entry['errors']==records[0]['errors']
        assert entry['examples'] and 'int64 main()' in entry['examples'][0]['code']
        text=run('api','std::text::Text::split').stdout
        assert all(word in text for word in ('参数','返回值','错误与边界','完整示例')) and '\x1b' not in text,text
        assert query('--search','SPLIT')['symbols']
        exact=query('--search','split','--exact')
        assert exact['symbols'] and all(r['name'].split('::')[-1]=='split' or any(p['name']=='split' for p in r['parameters']) for r in exact['symbols'])
        assert not query('--search','SPLIT','--exact')['symbols']
        # 说明、参数名、示例文本均进入搜索范围。
        assert query('--search','delimiter')['symbols']
        assert query('--search','UTF-8')['modules']
        assert query('--search','parts[1]')['symbols']
        for language,marker in [('zh','未找到手册目标'),('en','Manual target not found')]:
            missing=query('std::text::Text::spilt','--lang',language,code=1)
            assert missing['language']==language and marker in missing['diagnostics'][0]['message'],missing
            assert 'std::text::Text::split' in missing['candidates'],missing
            diagnostic=missing['diagnostics'][0]
            assert diagnostic['code']=='MGL1701' and all(k in diagnostic for k in ('span','source','notes','severity'))
            assert language==query('--lang',language)['language']
        executed=query('std::text::Text::split','--run')
        assert executed['execution']['exitCode']==0 and not executed['execution']['stderr'],executed
        assert executed['execution']['stdout'].replace('\r\n','\n')==entry['examples'][0]['expected'],executed
        for target in ('std::lang::readLines','std::io::FileStream::readAllString'):
            example=query(target,'--run')
            assert example['execution']['exitCode']==0,example
            expected=example['symbols'][0]['examples'][0]['expected']
            assert example['execution']['stdout'].replace('\r\n','\n')==expected,example
        absent=next(r for r in data['functions'] if not r['examples'])
        target=absent['module']+'::'+absent['name']
        no_example=query(target,'--run',code=1)
        assert no_example['diagnostics'][0]['code']=='MGL1702',no_example
    result=subprocess.run(['python',str(ROOT/'tools/library_reference.py'),'--check'],cwd=ROOT,capture_output=True,text=True,encoding='utf-8',env=dict(os.environ,PYTHONIOENCODING='utf-8'))
    assert result.returncode==0,result.stdout+result.stderr
    print('man 模块、类型/方法、参数与边界、全文/精确搜索、双语、候选、JSON、示例执行和共享索引同步通过。')


if __name__=='__main__':main()
