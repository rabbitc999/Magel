"""执行书本中标记的新增完整程序，并核对 LaTeX 生成内容。"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
COMPILER=ROOT/('magel.exe' if os.name=='nt' else 'magel')
sys.path.insert(0,str(ROOT/'doc/book/latex/tools'))
from md2tex import Converter, FILES


def main():
    """新章节示例须产生所列输出，生成章节须与 Markdown 一致。"""
    book=(ROOT/'doc/book/latex/book.tex').read_text(encoding='utf-8')
    assert not re.search(r'^\s*\\part\*?\{',book,re.M), '书稿直接按章节排列，不使用 part 分组'
    assert len(re.findall(r'\\input\{chapters/ch\d+\}',book))==25
    fixture = "1. 先声明：\n\n   ```magel\n   int64 value = 1;\n   ```\n\n2. 再读取：\n\n   ```magel\n   println(\"{}\", value);\n   ```\n\n- 无序说明：\n\n  ```text\n  output\n  ```"
    rendered = Converter().convert(fixture)
    assert r'\begin{enumerate}[start=2]' in rendered
    assert 'minipage' not in rendered and 'samepage' not in rendered
    environments=[]
    for tag, name in re.findall(r'\\(begin|end)\{([^}]+)\}', rendered):
        if tag == 'begin':
            if name == 'lstlisting':
                assert not any(item in ('enumerate', 'itemize') for item in environments)
            environments.append(name)
        else:
            assert environments.pop() == name
    assert not environments
    try:
        Converter().convert('| chapter | title |\n| 24 | project |\n| 25 | MCU |')
    except ValueError as error:
        assert '分隔行' in str(error)
    else:
        raise AssertionError('没有表头分隔行的表格不能静默丢行')
    # 双反引号代码段要整体识别，拆成单反引号会在 PDF 里漏出裸反引号。
    double = Converter().convert('| 指令 | 作用 |\n| --- | --- |\n| `` $macro N((a)) `` | 说明 |')
    assert '`' not in double and r'\texttt{ \$macro N(' in double, double
    preface=(ROOT/'doc/book/latex/chapters/preface.tex').read_text(encoding='utf-8')
    for chapter in range(1, 26):
        assert preface.count(r'\hyperref[ch:%02d]{%02d} &' % (chapter, chapter)) == 1, chapter
    count=0
    with tempfile.TemporaryDirectory(prefix='magel-book-') as folder:
        work=Path(folder)
        for source,target in FILES:
            path=ROOT/'doc/book'/source
            text=path.read_text(encoding='utf-8')
            mode='preface' if source=='README.md' else 'appendix' if source.startswith(('A-','B-')) else 'chapter'
            expected=Converter(mode).convert(text)
            assert (ROOT/'doc/book/latex/chapters'/target).read_text(encoding='utf-8')==expected,target
            assert 'minipage' not in expected and 'samepage' not in expected, target
            list_depth=0
            for match in re.finditer(r'\\(begin|end)\{(enumerate|itemize|lstlisting)\}', expected):
                direction, environment=match.groups()
                if environment == 'lstlisting':
                    assert list_depth == 0, (target, match.start())
                else:
                    list_depth += 1 if direction == 'begin' else -1
            assert list_depth == 0, target
            for code,output in re.findall(r'<!-- verify-book -->\s*```magel\n(.*?)```\s*```text\n(.*?)```',text,re.S):
                count+=1;file=work/f'example{count}.mgl';file.write_text(code,encoding='utf-8')
                result=subprocess.run([str(COMPILER),'run',str(file),'--out',str(work/f'output{count}')],cwd=work,
                    env=dict(os.environ,MAGEL_LANG='zh',MAGEL_NO_STDIN_INPUT='1'),capture_output=True,encoding='utf-8',timeout=60)
                assert result.returncode==0 and result.stdout.replace('\r\n','\n')==output,(source,result.stdout,result.stderr,output)
    assert count>=26,count
    print(f'新增 {count} 个书本完整程序输出与全部 LaTeX 生成章节一致。')


if __name__=='__main__':main()
