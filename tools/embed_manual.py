"""从权威文法、语言参考、书稿与 AI SKILL 生成内嵌手册数据。"""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'tools/magel_manual.json'


def generate():
    """收集实际 Markdown 与关键字对应章节，不在运行时访问源码目录。"""
    book = []
    for path in sorted((ROOT / 'doc/book').glob('*.md')):
        text = path.read_text(encoding='utf-8')
        identifier = path.stem.split('-')[0] if '-' in path.stem else 'index'
        book.append(dict(id=identifier, title=text.splitlines()[0].lstrip('# '),
                         path=path.relative_to(ROOT).as_posix(), content=text))
    reference = (ROOT / 'doc/04-language-reference.md').read_text(encoding='utf-8')
    sections = re.split(r'(?=^## )', reference, flags=re.M)
    groups = {
        '2.': 'int8 int16 int32 int64 uint8 uint16 uint32 uint64 float32 float64 bool char void true false',
        '3.': 'auto const static type', '4.': 'return new move heap', '5.': 'cast',
        '6.': 'struct', '7.': 'enum match _', '8.': 'interface',
        '10.': 'if elif else while do for loop continue in',
        '12.': 'module import export inline', '13.': 'mut',
        '15.': '@annotation', '16.': 'abi export', 'fn 捕获': 'fn copy move',
        '返回控制表达式': 'return', '具名函数声明': 'fn mut copy move', '#derive': '#derive',
        'for-in 整数': 'for auto in', '带值退出': 'break yield',
        '指定层数退出': 'break', '块表达式': 'break',
    }
    token_source = (ROOT / 'source/magel_token.gram').read_text(encoding='utf-8')
    grammar = (ROOT / 'source/magel.gram').read_text(encoding='utf-8')
    productions = re.findall(r'^\w+\s*:=.*?\s;(?=\s*(?:/\*|\w+\s*:=|$))', grammar, re.M | re.S)
    keywords = {}
    for token, word in re.findall(r'^%(?:token|define)\s+(\w+)\s+"([@#]?[A-Za-z_][A-Za-z_0-9]*)"', token_source, re.M):
        if word.startswith('__magel_') or word == '__typeTag':
            continue
        selected = [section for prefix, names in groups.items() if word in names.split()
                    for section in sections if section.startswith('## ' + prefix)]
        source = 'doc/04-language-reference.md'
        if word in ('defer', 'scope', 'drop', 'delete'):
            source = 'doc/book/08-资源与作用域.md'
            selected = [(ROOT / source).read_text(encoding='utf-8')]
        if word == 'impl':
            source = 'doc/book/21-与C互操作.md'
            selected = [(ROOT / source).read_text(encoding='utf-8')]
        assert selected, f'关键字缺少明确的文档映射：{word}'
        if word in ('return', 'fn', 'copy', 'move', 'mut', '#derive'):
            selected.append((ROOT / 'doc/28-declaration-sugars.md').read_text(encoding='utf-8'))
        if word in ('uint8', 'uint16', 'uint32', 'uint64', 'float32', 'float64'):
            chapter = (ROOT / 'doc/book/03-变量与类型.md').read_text(encoding='utf-8')
            selected.extend(part for part in re.split(r'(?=^## )', chapter, flags=re.M) if '1.0f' in part and '1u' in part)
        syntax = '\n\n'.join(part.strip() for part in productions if re.search(r'\b' + re.escape(token) + r'\b', part))
        keywords[word] = dict(name=word, token=token, syntax=syntax,
                              documentation='\n\n'.join(selected), source=source)
    macro_source = 'doc/24-literals-operators-macros.md'
    macro_sections = re.split(r'(?=^## )', (ROOT / macro_source).read_text(encoding='utf-8'), flags=re.M)
    keywords['macro'] = dict(name='macro', token='$macro', syntax='$macro NAME<..Types>[..Attrs](..Args){..Body}\n...\n$end_macro',
                             documentation='\n\n'.join(section for section in macro_sections if section.startswith(('## 宏文本拼接', '## 有序多括号'))), source=macro_source)
    assert 'break' in keywords and 'struct' in keywords and len(book) >= 28
    images = [dict(path='images/' + path.name, hex=path.read_bytes().hex())
              for path in sorted((ROOT / 'doc/book/images').glob('*')) if path.is_file()]
    api_pages = {path.name: path.read_text(encoding='utf-8') for path in sorted((ROOT / 'doc/library').glob('*.md'))}
    return json.dumps(dict(schemaVersion=1, sourceLanguage='zh', apiPages=api_pages, book=book, images=images, keywords=keywords,
                           skill=(ROOT / 'skills/magel-programming/SKILL.md').read_text(encoding='utf-8')),
                      ensure_ascii=False, indent=2) + '\n'


def main():
    """生成数据或检查已生成数据是否与文档一致。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    options = parser.parse_args()
    text = generate()
    if options.check:
        assert OUTPUT.read_text(encoding='utf-8') == text, '内嵌书稿、关键字或 SKILL 数据需要重新生成'
    else:
        OUTPUT.write_text(text, encoding='utf-8')
    print('内嵌书稿、关键字与 SKILL 数据同步。')


if __name__ == '__main__':
    main()
