"""Magel LSP 的模块声明、文档注释和调用参数索引。"""
from pathlib import Path
import json
import re
import tomllib
from magel_api import TOKEN, declarations, parameters, without_comments


def documentation(comment):
    """将 Doxygen 块注释及 ///、//! 注释转成安全的 Markdown 文本。"""
    text = re.sub(r'^/\*+|\*/$', '', comment.strip())
    text = re.sub(r'^\s*(?:\*|///|//!)\s?', '', text, flags=re.M)
    text = re.sub(r'@brief\s*', '', text)
    text = re.sub(r'@param\s+(\w+)\s*', r'\n\n**参数 `\1`**：', text)
    text = re.sub(r'@returns?\s*', '\n\n**返回值**：', text)
    return text.strip()


def naming_warnings(text):
    """按声明生成命名风格警告，跳过构造器、宏和内部协议名。"""
    output = []
    for row in declarations(Path('<editor>'), text):
        kind, name = row['kind'], row['name']
        is_type = kind in ('struct', 'enum', 'interface')
        if not is_type and kind != 'function':
            continue
        if name.startswith('__') or (kind == 'function' and name == row['owner'].split('::')[-1]):
            continue
        pattern = r'[A-Z][a-z0-9]*(?:[A-Z][a-z0-9]+)*' if is_type else r'[a-z][A-Za-z0-9]*'
        if re.fullmatch(pattern, name):
            continue
        start = row['offset']
        suffix = text[start:]
        match = re.search(r'\b' + re.escape(name) + (r'\b' if is_type else r'\s*(?:<[^;{}]*?>)?\s*\('), suffix)
        if match:
            start += match.start()
        before = text[:start]
        line = before.count('\n')
        column = len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2
        output.append(dict(range=dict(start=dict(line=line, character=column),
                                      end=dict(line=line, character=column + len(name))),
                           severity=2, source='magel', code='MGL2101' if is_type else 'MGL2102',
                           message=('类型名应使用大驼峰（PascalCase）：' if is_type else
                                    '函数或方法名应使用小驼峰（camelCase）：') + name))
    return output


def imports(text):
    """只读取真实 import 指令，排除注释和字符串里的伪导入。"""
    clean = TOKEN.sub(lambda m: ' ' * len(m.group()) if m.group().startswith(('/*', '//', '"', "'")) else m.group(), text)
    return re.findall(r'\bimport\s+([\w:]+)\s*;', clean)


def project_paths(path):
    """读取最近工程及递归锁定依赖的搜索路径，不执行下载或写入。"""
    paths, visited = [], set()
    def read(base):
        base=base.resolve()
        if base in visited:
            return
        visited.add(base)
        paths.extend([base, base / 'modules'])
        try:
            data=tomllib.loads((base/'magel.toml').read_text(encoding='utf-8-sig'))
            for key in ('search_paths','import_paths','include_paths'):
                paths.extend(base/p for p in data.get('magel',{}).get(key,[]))
            lock=base/'magel.lock'
            if lock.is_file():
                for value in tomllib.loads(lock.read_text(encoding='utf-8-sig')).get('dependencies',{}).values():
                    if isinstance(value,list) and len(value)>=2:
                        read(base/value[1])
        except (OSError,ValueError,TypeError):
            pass
    for base in path.parents:
        if (base/'magel.toml').is_file():
            read(base)
            break
    return paths


class ApiIndex:
    """按导入闭包提供声明与注释；未保存文档优先于磁盘，循环导入只访问一次。"""
    def __init__(self):
        data = json.loads(Path(__file__).with_name('magel_stdlib_api.json').read_text(encoding='utf-8'))
        # `__` 前缀是编译器/宿主协议，标准库里带这个前缀的函数、类型都不给编辑器补全；
        # 工程内同名符号属于用户自己的命名空间，不受此限。
        self.standard = [row for row in data['functions'] + data.get('symbols', [])
                         if not self.private(row.get('name', '')) and not self.private(row.get('owner', ''))]
        self.module_imports = data['modules']
        self.cache = {}
        self.symbols = {}
        self.standard_type_names={name for name in data.get('types', []) if not self.private(name)}
        self.type_names=set(self.standard_type_names)

    @staticmethod
    def private(name):
        """判断是否属于 `__` 前缀的标准库内部协议名。"""
        return name.startswith('__')

    def scan(self, uri, text):
        """缓存一份文档的公开声明，保留重载与声明坐标。"""
        cached = self.cache.get(uri)
        if cached and cached[0] == text:
            self.type_names.update(cached[2])
            return cached[1]
        records = declarations(Path(uri), text)
        output = []
        symbols = []
        for entry in records:
            if entry['kind'] not in ('struct', 'enum', 'interface', 'member'):
                continue
            record = dict(entry)
            if record['kind'] == 'member':
                match = re.search(r'([A-Za-z_]\w*)\s*(?:=[^;]*)?$', record['signature'])
                if not match:
                    continue
                record['name'] = match.group(1)
            record['qualified'] = '::'.join(p for p in (record['module'], record['owner'], record['name']) if p)
            record['documentation'] = documentation(record['comment'])
            record['uri'] = uri
            start = record.get('offset', 0)
            match = re.search(r'\b' + re.escape(record['name']) + r'\b', text[start:])
            record['offset'] = start + (match.start() if match else 0)
            symbols.append(record)
        self.symbols[uri] = symbols
        declared_types={r['name'] for r in records if r['kind'] in ('struct','enum','interface')}
        self.type_names.update(declared_types)
        for record in records:
            if record['kind'] != 'function':
                continue
            record = dict(record)
            record['qualified'] = '::'.join(p for p in (record['module'], record['owner'], record['name']) if p)
            record['documentation'] = documentation(record['comment'])
            record['parameters'] = [dict(name=n, label=d + (' = ' + v if v else '')) for n, d, v in parameters(record['signature'])]
            record['uri'] = uri
            # 声明起点之后查找函数名，避免返回类型同名时定位到类型。
            header = text[record['offset']:]
            match = re.search(r'\b' + re.escape(record['name']) + r'\s*(?:<[^;{}]*?>)?\s*\(', header)
            record['offset'] += match.start() if match else 0
            output.append(record)
        self.cache[uri] = (text, output, declared_types)
        return output

    def collect(self, uri, text, documents, uri_path):
        """读取当前文档和递归导入模块，并加入可见标准库声明。"""
        self.type_names=set(self.standard_type_names)
        output, visited, modules = [], set(), {'std::lang', 'std'}
        path = uri_path(uri)
        roots = project_paths(path)
        def visit(current_uri, current_text, current_path):
            if current_uri in visited:
                return
            visited.add(current_uri)
            output.extend(self.scan(current_uri, current_text))
            for module in imports(current_text):
                modules.add(module)
                if module in self.module_imports:
                    continue
                relative = Path(*module.split('::')).with_suffix('.mgl')
                for root in [current_path.parent, current_path.parent / 'modules', *roots]:
                    candidate = (root / relative).resolve()
                    child_uri = candidate.as_uri()
                    opened = documents.get(child_uri)
                    try:
                        child = opened['text'] if opened else candidate.read_text(encoding='utf-8-sig')
                    except (OSError, UnicodeError):
                        continue
                    # 文件存在但模块名不符时不向用户提供错误提示。
                    match = re.search(r'\bmodule\s+([\w:]+)\s*;', without_comments(child))
                    if match and match.group(1) == module:
                        visit(child_uri, child, candidate)
                    break
        visit(uri, text, path)
        self.visible_uris = visited
        pending=list(modules)
        while pending:
            for child in self.module_imports.get(pending.pop(), []):
                if child not in modules:
                    modules.add(child)
                    pending.append(child)
        for entry in self.standard:
            if entry['module'] not in modules:
                continue
            record = dict(entry)
            record['qualified'] = record['module'] + '::' + record['name']
            record['name'] = record['name'].split('::')[-1]
            record['documentation'] += '\n\n' + documentation(record['comment'])
            output.append(record)
        return output


def expression_name(text, cursor):
    """读取调用名或成员名以及接收者；泛型实参不属于函数名称。"""
    stream=[m.group() for m in TOKEN.finditer(text[:cursor]) if not m.group().startswith(('/*','//'))]
    if not stream: return ''
    end=len(stream)
    if stream[-1]=='>':
        depth=1;at=end-1
        while at and depth:
            at-=1
            if stream[at]=='>': depth+=1
            elif stream[at]=='<': depth-=1
        if not depth and at: end=at
    def consume(stop):
        if not stop: return stop
        start=stop-1; word=stream[start]
        if word in ('.','::','?'): return consume(start)
        if word in (')',']','>'):
            opening={')':'(',']':'[','>':'<'}[word];depth=1
            while start and depth:
                start-=1
                if stream[start]==word: depth+=1
                elif stream[start]==opening: depth-=1
            if depth: return stop
            if start and (re.fullmatch(r'[\w$]+',stream[start-1]) or stream[start-1] in (')',']','>')):
                start=consume(start)
        elif not re.fullmatch(r'[A-Za-z_$][\w$]*',word): return stop
        while start and stream[start-1] in ('.','::'):
            previous=consume(start-1)
            if previous==start-1: break
            start=previous
        return start
    start=consume(end)
    return ''.join(stream[start:end]) if start!=end else ''


def inferred_type(records, name, text, cursor=None, depth=0):
    """由可见声明、初始化式及返回声明推导编辑期类型，循环或歧义返回空串。"""
    if depth > 24:
        return ''
    cursor = len(text) if cursor is None else cursor
    name = name.strip()
    stream = [m for m in TOKEN.finditer(name) if not m.group().startswith(('/*','//'))]
    if not stream:
        return ''
    def base(value):
        return re.split(r'[< &]', value.strip(), 1)[0]
    def qualify(value, module=''):
        value = re.sub(r'^(?:(?:static|inline)\s+)+', '', value.strip())
        head = base(value)
        matches = {r['qualified'] for r in records if r.get('kind') in ('struct','enum','interface') and
                   (r['qualified']==head or r['name']==head or r['qualified'].endswith('::'+head))}
        preferred = {r['qualified'] for r in records if r.get('module')==module and r['qualified'] in matches}
        if len(preferred)==1: matches=preferred
        return next(iter(matches))+value[len(head):] if len(matches)==1 else value
    if stream[-1].group()=='?':
        source=inferred_type(records,name[:stream[-1].start()],text,cursor,depth+1)
        head=base(source)
        if head in ('std::lang::Result','std::lang::Option','Result','Option'):
            match=re.search(r'<(.*)>',source)
            if match:
                level=0;argument=''
                for word in (m.group() for m in TOKEN.finditer(match.group(1))):
                    if word==',' and not level:break
                    argument+=word
                    if word=='<':level+=1
                    elif word=='>':level-=1
                return qualify(argument)
        for record in records:
            if record.get('kind') in ('struct','enum') and record['qualified']==head:
                match=re.search(r'\bTry(?:Move)?\s*<([^,>]+)',record['signature'])
                if match:return qualify(match.group(1),record.get('module',''))
        return ''
    if name.startswith('f"'):return 'std::lang::String'
    if re.match(r'(?:if|match)\s*\(',name):
        results=set();tokens=list(TOKEN.finditer(name));i=0
        origin=text.find(name,cursor)
        if origin<0:origin=cursor
        while i<len(tokens):
            if tokens[i].group()!='{':i+=1;continue
            if name.startswith('match') and (not i or tokens[i-1].group()!='=>'):i+=1;continue
            begin=i;level=1;i+=1
            while i<len(tokens) and level:
                if tokens[i].group()=='{':level+=1
                elif tokens[i].group()=='}':level-=1
                i+=1
            if level:return ''
            body=name[tokens[begin].end():tokens[i-1].start()]
            values=[(body,origin+tokens[begin].end())] if ';' not in body else [(m.group(1),origin+tokens[begin].end()+m.start()) for m in re.finditer(r'\bbreak\s+([^;]+);',body)]
            for value,position in values:
                concrete=inferred_type(records,value,text,position,depth+1)
                if concrete:results.add(concrete)
        return next(iter(results)) if len(results)==1 else ''
    call = stream[-1].group()==')'
    if call:
        level=0
        for token in reversed(stream):
            if token.group()==')': level+=1
            elif token.group()=='(':
                level-=1
                if not level:
                    if not name[:token.start()].strip():
                        return inferred_type(records,name[token.end():stream[-1].start()],text,cursor,depth+1)
                    name=name[:token.start()].strip()
                    break
    nesting=0; dot=None
    for token in TOKEN.finditer(name):
        word=token.group()
        if word in ('(','[','<'): nesting+=1
        elif word in (')',']','>'): nesting-=1
        elif word=='.' and not nesting: dot=token.start()
    if dot is not None:
        receiver=inferred_type(records,name[:dot],text,cursor,depth+1)
        entries=matching(records,name[:dot]+'.'+name[dot+1:],text,cursor)
    elif call:
        head=base(name)
        if any(r.get('kind') in ('struct','enum','interface') and
               (r['name']==head or r['qualified']==head or r['qualified'].endswith('::'+head)) for r in records):
            return qualify(name)
        entries=matching(records,head,text,cursor)
        receiver=''
    else:
        # 用词法块编号判定可见性，避免已退出块或同名局部变量污染类型。
        tokens=[m for m in TOKEN.finditer(text) if not m.group().startswith(('/*','//'))]
        scopes={}; scope=[0]; serial=0
        for token in tokens:
            if token.group()=='{': serial+=1; scope.append(serial)
            scopes[token.start()]=tuple(scope)
            if token.group()=='}' and len(scope)>1: scope.pop()
        current=(0,)
        for token in tokens:
            if token.start()>=cursor: break
            current=scopes[token.start()]
            if token.group()=='}': current=current[:-1]
        pattern=r'\b([A-Za-z_]\w*(?:::\w+)*(?:<[^;={}()]+>)?(?:\s+(?:const|mut))?\s*&?)\s*\b'+re.escape(name)+r'\s*(?=[=;,)])'
        found=[]
        for match in re.finditer(pattern,text):
            if base(match.group(1)) in ('return','import','module'): continue
            owner=scopes.get(next((t.start() for t in tokens if t.start()>=match.start()),match.start()),(0,))
            if match.start()<=cursor and current[:len(owner)]==owner:
                found.append((len(owner),match.start(),match))
        if found:
            binding=max(found,key=lambda item:item[:2])[2]
            declared=binding.group(1).strip()
            if base(declared)!='auto': return qualify(declared)
            suffix=text[binding.end():].lstrip()
            if not suffix.startswith('='): return ''
            nesting=0; end=len(suffix)
            for token in TOKEN.finditer(suffix[1:]):
                word=token.group()
                if not nesting and word in (';',',','}'): end=token.start()+1;break
                if word in ('(','[','{'): nesting+=1
                elif word in (')',']','}'): nesting-=1
            return inferred_type(records,suffix[1:end],text,binding.start(),depth+1)
        if name in ('true','false'): return 'bool'
        if len(stream)==1:
            if name.startswith('"'): return 'std::lang::String'
            if name.startswith("'"): return 'char'
            if re.fullmatch(r'(?:\d+|0[xX][0-9A-Fa-f_]+|[bB]_?[01][01_]*)u', name): return 'uint64'
            if re.fullmatch(r'\d+(?:\.\d+)?(?:[eE][+-]?\d+)?f', name): return 'float32'
            if re.fullmatch(r'\d[\w.]*|\d+(?:\.\d+)?[eE][+-]?\d+',name):
                return 'float64' if re.search(r'[.eE]',name) and not name.lower().startswith('0x') else 'int64'
        return ''
    results=set()
    for entry in entries:
        if not call and entry.get('kind')!='member': continue
        value=entry.get('type') or entry['signature'].split(entry['name'],1)[0].strip()
        if '<' in receiver:
            head=base(receiver)
            template=next((r for r in records if r['qualified']==head and r.get('kind') in ('struct','enum','interface')),None)
            if template:
                params=re.search(r'<([^>]+)>',template['signature'])
                args=re.search(r'<(.*)>',receiver)
                if params and args:
                    for param,arg in zip(params.group(1).split(','),args.group(1).split(',')):
                        value=re.sub(r'\b'+re.escape(param.strip())+r'\b',lambda _:arg.strip(),value)
        value=qualify(value,entry.get('module',''))
        if value and base(value)!='auto': results.add(value)
    return next(iter(results)) if len(results)==1 else ''


def matching(records, name, text, cursor=None):
    """按限定名或接收者的具体类型缩小候选，未知或歧义类型不列举无关成员。"""
    if '.' in name:
        receiver, method = name.rsplit('.', 1)
        owner = re.split(r'[< &]',inferred_type(records,receiver,text,cursor),1)[0]
        if not owner:
            return []
        return [r for r in records if r['owner'] and r['name']!=r['owner'].split('::')[-1] and (not method or r['name'] == method) and
                '::'.join(p for p in (r['module'],r['owner']) if p)==owner]
    if '::' in name:
        return [r for r in records if r['qualified'] == name or r['qualified'].endswith('::' + name)]
    result = [r for r in records if r['name'] == name and not r['owner']]
    if any(r['module'] != 'std::lang' and r.get('kind','function') == 'function' for r in result):
        result = [r for r in result if r['module'] != 'std::lang' or r.get('kind','function') != 'function']
    return result


def call_context(text, cursor):
    """找到最内层未闭合调用，排除字符串、注释及嵌套表达式的逗号。"""
    stack = []
    generic_opens = set()
    stream=list(TOKEN.finditer(text[:cursor]))
    for i, token in enumerate(stream):
        if token.group() != '<' or i == 0 or not re.fullmatch(r'\w+|>', stream[i-1].group()):
            continue
        depth=1
        for j in range(i+1,len(stream)):
            word=stream[j].group()
            if word == '<': depth+=1
            elif word == '>': depth-=1
            if depth == 0:
                if j+1<len(stream) and stream[j+1].group() in ('(', '::', '>'):
                    generic_opens.add(token.start())
                break
            if word in (';', '{', '}'): break
    for token in stream:
        word = token.group()
        if word.startswith(('/*', '//', '"', "'")):
            continue
        if word in ('(', '[', '{') or token.start() in generic_opens:
            stack.append([word, token.start(), 0])
        elif word in (')', ']', '}') or word == '>' and stack and stack[-1][0]=='<':
            if stack:
                stack.pop()
        elif word == ',' and stack:
            stack[-1][2] += 1
    for word, start, argument in reversed(stack):
        if word == '(':
            name = expression_name(text, start)
            if name:
                return name, argument
    return '', 0


def signature_help(records, text, cursor):
    """保留全部重载，依据当前参数位置选择可容纳该参数的声明。"""
    name, active = call_context(text, cursor)
    candidates = matching(records, name, text) if name else []
    if not candidates:
        return None
    signatures = []
    for record in candidates:
        signatures.append({'label': record['signature'], 'documentation': {'kind': 'markdown', 'value': record['documentation']}, 'parameters': [{'label': p['label'], 'documentation': {'kind': 'markdown', 'value': p.get('documentation') or record['documentation']}} for p in record['parameters']]})
    selected = next((i for i,r in enumerate(candidates) if len(r['parameters']) > active), 0)
    return {'signatures': signatures, 'activeSignature': selected, 'activeParameter': active}


TOKEN_TYPES = ['namespace','type','typeParameter','parameter','variable','property','function','keyword','comment','string','number','operator']
PRIMITIVES = set('bool char float32 float64 int8 int16 int32 int64 uint8 uint16 uint32 uint64 void'.split())


def semantic_tokens(text, records, keywords, pos, extra_types=()):
    """根据词法、类型声明和块作用域编码 UTF-16 语义着色，不猜测标识符大小写。"""
    raw=list(TOKEN.finditer(text))
    stream=[t for t in raw if not t.group().startswith(('/*','//','"',"'"))]
    words=[t.group() for t in stream]
    types=PRIMITIVES | {'String','List','Set','Map','Option','Result','Any','BigInteger','BigDecimal','File','Process'}
    types.update(extra_types)
    types.update(r['owner'].split('::')[-1] for r in records if r.get('owner'))
    classified={}
    generic=set()
    for i,word in enumerate(words):
        if word in ('struct','enum','interface','type') and i+1<len(words):
            types.add(words[i+1]); classified[i+1]=('type',1)
            if i+2<len(words) and words[i+2]=='<':
                j=i+3
                while j<len(words) and words[j]!='>':
                    if words[j].isidentifier() and (j==i+3 or words[j-1]==','):
                        generic.add(words[j]);classified[j]=('typeParameter',1)
                    j+=1
    # 泛型函数的返回类型本身也可能是 T，先识别声明的 <T,...> 再解析返回类型。
    for i,word in enumerate(words):
        if not word.isidentifier() or i+1>=len(words) or words[i+1]!='<':continue
        end=i+2;depth=1
        while end<len(words) and depth:
            if words[end]=='<':depth+=1
            elif words[end]=='>':depth-=1
            end+=1
        if depth or end>=len(words) or words[end]!='(':continue
        # 调用处的显式类型实参不声明新类型参数。
        declared={words[g] for g in range(i+2,end-1) if words[g].isidentifier() and words[g-1] in ('<',',')}
        if not i or words[i-1] not in types|generic|declared and words[i-1] not in (')','>'):continue
        for g in range(i+2,end-1):
            if words[g].isidentifier() and words[g-1] in ('<',','):
                generic.add(words[g]);classified[g]=('typeParameter',1)
    types |= generic
    scopes=[]; current=(0,); serial=0
    for word in words:
        if word=='{': serial+=1;current=(*current,serial)
        scopes.append(current)
        if word=='}' and len(current)>1: current=current[:-1]
    bindings=[]
    parameter_regions=[]
    def end_group(start,opening,closing):
        depth=0
        for j in range(start,len(words)):
            if words[j]==opening:depth+=1
            elif words[j]==closing:
                depth-=1
                if not depth:return j+1
        return len(words)
    for i,word in enumerate(words):
        if word not in types and word!='auto':continue
        if i and words[i-1] in ('struct','enum','interface','type','.'):continue
        j=i+1
        while j+1<len(words) and words[j]=='::':j+=2
        if j<len(words) and words[j]=='<':j=end_group(j,'<','>')
        while j<len(words) and words[j] in ('const','mut','&'):j+=1
        if j>=len(words) or not words[j].isidentifier() or words[j] in keywords:continue
        following=words[j+1] if j+1<len(words) else ''
        k=j+1
        if following=='<':
            end=end_group(k,'<','>')
            for g in range(k+1,end-1):
                if words[g].isidentifier() and words[g-1] in ('<',','):
                    generic.add(words[g]);types.add(words[g]);classified[g]=('typeParameter',1)
            k=end;following=words[k] if k<len(words) else ''
        kind='function' if following=='(' else 'variable'
        if kind=='function':parameter_regions.append((k,end_group(k,'(',')')))
        elif following not in ('=', ';', ',', '[', ')', ':'):continue
        classified[j]=(kind,1)
        bindings.append((words[j],j,scopes[j],kind))
        if kind=='variable':
            k=j+1;depth=0
            while k<len(words) and not (depth==0 and words[k] in (';',')')):
                if words[k] in ('(','[','{'):depth+=1
                elif words[k] in (')',']','}'):depth-=1
                elif words[k]==',' and depth==0 and k+1<len(words) and words[k+1].isidentifier() and words[k+1] not in types:
                    classified[k+1]=('variable',1);bindings.append((words[k+1],k+1,scopes[k+1],'variable'))
                k+=1
    for i,(kind,modifiers) in list(classified.items()):
        if kind=='variable' and any(a<i<b for a,b in parameter_regions):
            classified[i]=('parameter',modifiers)
            region=next((b for a,b in parameter_regions if a<i<b),len(words))
            if region<len(words) and words[region]=='{':
                bindings=[(n,j,scopes[region] if j==i else scope,kind) for n,j,scope,kind in bindings]
    functions={r['name'] for r in records if r.get('kind', 'function') == 'function'}
    offsets={t.start():i for i,t in enumerate(stream)}
    spans=[]
    for token in raw:
        word=token.group();i=offsets.get(token.start());modifiers=0
        if word.startswith(('/*','//')):kind='comment'
        elif word.startswith(('"',"'")):kind='string'
        elif word[0].isdigit() or re.fullmatch(r'[bB]_[01_]+',word):kind='number'
        elif i in classified:kind,modifiers=classified[i]
        elif word in PRIMITIVES:kind='type'
        elif word in keywords:kind='keyword'
        elif word.isidentifier():
            previous=words[i-1] if i else '';following=words[i+1] if i+1<len(words) else ''
            visible=[b for b in bindings if b[0]==word and b[1]<=i and scopes[i][:len(b[2])]==b[2]]
            if previous=='.':kind='function' if following=='(' else 'property'
            elif visible:
                binding=max(visible,key=lambda b:(len(b[2]),b[1]));kind=classified[binding[1]][0]
            elif word in generic:kind='typeParameter'
            elif word in types:kind='type'
            elif following=='::' or previous in ('import','module'):kind='namespace'
            elif following=='(' or word in functions:kind='function'
            else:kind='variable'
        else:kind='operator'
        start=token.start()
        for part in word.splitlines(keepends=True):
            content=part.rstrip('\r\n')
            if content:
                location=pos(text,start)
                spans.append((location['line'],location['character'],len(content.encode('utf-16-le'))//2,TOKEN_TYPES.index(kind),modifiers))
            start+=len(part)
    data=[];last_line=last_column=0
    for line,column,length,kind,modifiers in spans:
        data.extend([line-last_line,column-last_column if line==last_line else column,length,kind,modifiers])
        last_line,last_column=line,column
    return {'data':data}
