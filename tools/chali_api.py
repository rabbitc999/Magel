"""编辑期和参考手册共用的声明索引；不参与编译器语法解析。"""
from pathlib import Path
import re

# 编辑中的未闭合注释/字符串也作为一个词法单元，避免正文被误当成代码。
TOKEN = re.compile(r'/\*.*?(?:\*/|\Z)|//[^\n]*|"(?:\\.|[^"\\\n])*(?:"|(?=\n)|$)|\'(?:\\.|[^\'\\\n])*(?:\'|(?=\n)|$)|(?:0[xX][0-9A-Fa-f_]+u?|0[bB][01_]+u?|[bB]_?[01][01_]*u?|\d[\d_]*u|\d[\d_]*(?:\.\d[\d_]*)?(?:[eE][+-]?\d[\d_]*)?f?)|[A-Za-z_$][\w$]*|::|->|=>|\.\.\.|==|!=|<=|>=|\+\+|--|\+=|-=|\*=|/=|&&|\|\||[^\s]', re.S)


def without_comments(text):
    """仅去掉注释 token，保留默认字符串中的 URL 和转义。"""
    return TOKEN.sub(lambda m: ' ' if m.group().startswith(('/*','//')) else m.group(),text)


def declarations(path, source=None, line_offset=0, root=None):
    """扫描声明边界，跳过函数体，保留泛型、嵌套类型、重载及原始签名。"""
    source = path.read_text(encoding='utf-8') if source is None else source
    tokens = [(m.group(), m.start(), m.end()) for m in TOKEN.finditer(source)
              if not m.group().startswith(('/*', '//'))]
    records = []
    module_match = re.search(r'\bmodule\s+([\w:]+)\s*;', without_comments(source))
    module = module_match.group(1) if module_match else ''
    origin = path.relative_to(root).as_posix() if root else str(path)

    def visit(begin, end, owner='', owner_kind=''):
        i = begin
        while i < end:
            if tokens[i][0] == '@':
                # 注解定义及使用没有分号；其括号不能并入后续函数声明。
                i += 1
                if i < end and tokens[i][0] == 'annotation':
                    i += 1
                if i < end:
                    i += 1
                while i + 1 < end and tokens[i][0] == '::':
                    i += 2
                if i < end and tokens[i][0] == '(':
                    depth = 1
                    i += 1
                    while i < end and depth:
                        if tokens[i][0] == '(':
                            depth += 1
                        elif tokens[i][0] == ')':
                            depth -= 1
                        i += 1
                continue
            if tokens[i][0] == ';':
                i += 1
                continue
            start = i
            parens = brackets = 0
            while i < end:
                word = tokens[i][0]
                if word == '(' : parens += 1
                elif word == ')': parens -= 1
                elif word == '[': brackets += 1
                elif word == ']': brackets -= 1
                if parens == 0 and brackets == 0 and word in (';', '{'):
                    break
                i += 1
            if i == end:
                if owner and start < end:
                    tail=source[tokens[start][1]:tokens[end-1][2]].strip()
                    tail=without_comments(tail)
                    records.append(dict(module=module,owner=owner,kind='member',name=tail,
                                        signature=re.sub(r'\s+',' ',tail).strip(),comment='',
                                        source=origin,
                                        line=source.count('\n',0,tokens[start][1])+1+line_offset))
                break
            header = source[tokens[start][1]:tokens[i][1]].strip()
            header = without_comments(header).strip()
            signature = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\s+', lambda m: ' ' if m.group().isspace() else m.group(), header)
            body_end = i
            if tokens[i][0] == '{':
                depth = 1
                body_end = i + 1
                while body_end < end and depth:
                    if tokens[body_end][0] == '{': depth += 1
                    elif tokens[body_end][0] == '}': depth -= 1
                    body_end += 1
            kind_match = re.match(r'(?:(?:#\s*NonCopyable|#\s*derive\s*\([^)]*\))\s+)*(struct|enum|interface|opcode)\s+(\w+)', signature)
            function = None
            declaration_matches = list(TOKEN.finditer(signature))
            declaration_tokens = [m.group() for m in declaration_matches]
            depth = 0
            for n, word in enumerate(declaration_tokens):
                if word == '(' and depth == 0 and n > 0:
                    k = n - 1
                    if declaration_tokens[k] == '>':
                        angle = 1
                        k -= 1
                        while k >= 0 and angle:
                            if declaration_tokens[k] == '>': angle += 1
                            elif declaration_tokens[k] == '<': angle -= 1
                            k -= 1
                    if k >= 0 and declaration_tokens[k] != 'fn' and re.fullmatch(r'\w+', declaration_tokens[k]):
                        name = declaration_tokens[k]
                        prefix = signature[:declaration_matches[k].start()]
                        function = (prefix, name)
                        break
                if word == '(': depth += 1
                elif word == ')': depth -= 1
            preceding = source[:tokens[start][1]].rstrip()
            comment = re.search(r'/\*([^*]|\*(?!/))*\*/\s*$|(?:(?:///|//!)[^\n]*\n?\s*)+$', preceding, re.S)
            description = comment.group() if comment else ''
            entry = dict(module=module, owner=owner, signature=signature,
                         source=origin,
                         line=source.count('\n', 0, tokens[start][1]) + 1 + line_offset,
                         comment=description, offset=tokens[start][1])
            if kind_match:
                kind, name = kind_match.groups()
                entry.update(kind=kind, name=name)
                records.append(entry)
                visit(i + 1, body_end - 1, (owner + '::' if owner else '') + name, kind)
            elif owner_kind == 'opcode' and re.match(r'^(?:new|drop|copy)\s*=', signature):
                pass
            elif function and not signature.startswith(('import ', 'module ', 'type ', '$')):
                result, name = function
                # 枚举的带载荷变体没有返回类型，不属于函数声明。
                if '=' not in result and ((result.strip() and not re.match(r'^\w+\s*,', result.strip())) or name == owner.split('::')[-1]):
                    if not name.startswith('__'):
                        entry.update(kind='function', name=name,
                                     result=result.removeprefix('static ').strip(),
                                     body=source[tokens[i][2]:tokens[body_end-1][1]].strip()
                                     if tokens[i][0] == '{' else '')
                        records.append(entry)
                else:
                    entry.update(kind='member', name=signature)
                    records.append(entry)
            elif signature and not signature.startswith(('module ', 'import ', '$')):
                entry.update(kind='member' if owner else 'declaration', name=signature)
                records.append(entry)
            i = body_end if tokens[i][0] == '{' else i + 1
    visit(0, len(tokens))
    return records


def parameters(signature):
    """读取函数形参，括号与泛型嵌套内的逗号不拆分。"""
    # 函数名之后的括号：返回元组或泛型内部的括号均跳过。
    matches = list(TOKEN.finditer(signature))
    depth = 0
    opening = None
    for i, match in enumerate(matches):
        word = match.group()
        if word == '(' and depth == 0 and i and matches[i-1].group() != 'fn' and (re.fullmatch(r'\w+', matches[i-1].group()) or matches[i-1].group()=='>'):
            opening = match.end()
            break
        if word == '(': depth += 1
        elif word == ')': depth -= 1
    if opening is None: return []
    text = signature[opening:signature.rfind(')')]
    parts, start, depth = [], 0, 0
    for match in TOKEN.finditer(text):
        word = match.group()
        if word in ('(', '<', '['): depth += 1
        elif word in (')', '>', ']'): depth -= 1
        elif word == ',' and depth == 0:
            parts.append(text[start:match.start()].strip()); start=match.end()
    if text[start:].strip(): parts.append(text[start:].strip())
    result=[]
    for part in parts:
        declaration, _, default = part.partition('=')
        names = re.findall(r'\b\w+\b', declaration)
        result.append((names[-1], declaration.strip(), default.strip()))
    return result
