#!/usr/bin/env python3
"""从标准库声明生成可核对的 API 清单和参考手册；不参与语言解析或编译。"""
from pathlib import Path
import re
import json
import argparse
import sys
from library_reference_data import MODULES, BEHAVIOR, ITER, PARAMETERS, BIG_BEHAVIOR

ROOT = Path(__file__).resolve().parents[1]
from magel_api import TOKEN, without_comments, parameters, declarations as scan_declarations

def declarations(path, source=None, line_offset=0):
    """以仓库相对路径扫描标准库声明。"""
    return scan_declarations(path, source, line_offset, ROOT)


def inventory():
    """枚举工具链内置的全部源码模块。"""
    paths = sorted((ROOT / 'source/std').glob('*.mgl'))
    data = {re.search(r'\bmodule\s+([\w:]+)\s*;', p.read_text(encoding='utf-8')).group(1): declarations(p)
            for p in paths}
    path = ROOT / 'source/magel_generic.hpp'
    source = path.read_text(encoding='utf-8')
    raw = re.search(r'const Memory::String rangeBuiltinRaw = R"MAGEL\((.*?)\)MAGEL";', source, re.S)
    chunks = list(re.finditer(r'\bmodule\s+([\w:]+)\s*;', raw.group(1)))
    for i, chunk in enumerate(chunks):
        begin = raw.start(1) + chunk.start()
        end = raw.start(1) + (chunks[i+1].start() if i+1<len(chunks) else len(raw.group(1)))
        data.setdefault(chunk.group(1), []).extend(declarations(path, source[begin:end], source.count('\n', 0, begin)))
    from library_reference_builtins import BUILTINS
    for module, (origin, text) in BUILTINS.items():
        entries = declarations(ROOT / origin, 'module '+module+';\n'+text)
        for entry in entries:
            if entry['owner'] in ('List','Set','Map'):
                entry['source']='source/epilogue/aggregate_ownership.cpp'
            elif entry['name'] in ('toReference','toOptional'):
                entry['source']='source/magel_generic.hpp'
            origin_text=(ROOT/entry['source']).read_text(encoding='utf-8')
            at=origin_text.find('"'+entry['name']+'"')
            entry['line'] = origin_text.count('\n',0,at)+1 if at>=0 else 1
            entry['intrinsic'] = True
        data.setdefault(module, []).extend(entries)
    # 运算符接口由编译器循环生成，直接读取名称表，避免文档维护另一套名字。
    traits=[]
    for table in ('operators','unaryOperators'):
        block=re.search(r'\b'+table+r' = \{(.*?)\n    \};',source,re.S).group(1)
        for name,method in re.findall(r'\{\s*"(\w+)"\s*,\s*"(\w+)"\s*\}',block):
            for side in ('left','right') if table=='operators' else ('',):
                trait=name if side!='right' else 'Right'+name
                method_name=side+method if side else method
                traits.append(f'interface {trait}<R,T> {{ R {method_name}(T value); }};')
    traits += ['interface Hash<T> { uint64 hash(T value); };',
               'interface ExplicitConvert<T> { T explicitConvert(); };',
               'interface ImplicitConvert<T> { T implicitConvert(); };',
               'interface Index<R,T> { R index(T key); };',
               'interface Iterator<R> { Option<R> iterator(int64 index); };']
    for entry in declarations(path,'module std::lang;\n'+'\n'.join(traits)):
        entry['intrinsic']=True
        entry['line']=827
        data['std::lang'].append(entry)
    # 核对手工等价签名与编译器方法/算法登记表，新增内建 API 不得静默遗漏。
    container_source=(ROOT/'source/epilogue/aggregate_ownership.cpp').read_text(encoding='utf-8')
    for table,owner in (('kVecMethods','List'),('kSetMethods','Set'),('kMapMethods','Map')):
        block=re.search(table+r'\[\] = \{(.*?)\n\};',container_source,re.S).group(1)
        names=set(re.findall(r'\{\s*"(\w+)"',block))
        documented={r['name'] for r in data['std::lang'] if r['kind']=='function' and r['owner']==owner}
        if names!=documented: raise ValueError('容器内建清单不同步：'+owner+str(names^documented))
    intrinsic_names=set(re.findall(r'Memory::String\("(\w+Value)"\)',source[source.index('algorithmIntrinsics;'):source.index('const Memory::String rangeBuiltinRaw')]))
    documented={r['name'] for r in data['std::algorithms'] if r['kind']=='function'}
    if intrinsic_names-documented: raise ValueError('算法内建缺失：'+str(intrinsic_names-documented))
    # 泛型方法由语义层适配到顶层实现，公开签名独立登记为所属容器的方法。
    adapters = declarations(ROOT/'source/std/lang.mgl', 'module std::lang;\nstruct Result<T,E> { Result<T,F> mapErr<F>(F fn(E) mapper); };\nstruct Option<T> { Result<T,E> okOr<E>(E error); Result<T,E> okOrElse<E>(E fn() factory); };')
    for row in adapters:
        if row['kind']=='function':
            row['intrinsic']=True
            data['std::lang'].append(row)
    return data



def comment_text(comment):
    """将库注释中的正文转成 Markdown 段落。"""
    text = re.sub(r'^/\*+|\*/$', '', comment.strip())
    text = re.sub(r'^\s*\*\s?', '', text, flags=re.M)
    text = re.sub(r'@brief\s*', '', text)
    text = re.split(r'@param|@return', text)[0].strip()
    return re.sub(r'\s+', ' ', text)


def behavior(entry):
    """合并人工语义说明、同族宿主入口与声明注释。"""
    module, owner, name = entry['module'], entry['owner'], entry['name']
    if owner+'::'+name in BIG_BEHAVIOR: return BIG_BEHAVIOR[owner+'::'+name]
    if module=='std::iter' and name in ITER: return ITER[name]
    if module=='std::linalg' and name=='add': return '返回同维向量/矩阵逐项相加的新值。'
    if owner=='Any' and name=='clear': return '释放盒子拥有的载荷并恢复空状态；保留盒子自身的键身份。'
    if owner=='Any' and name=='set': return '以 value 的值副本和类型编号替换盒子内容。'
    if owner=='Any' and name=='get': return '依据接收上下文取得盒子内的值。'
    if entry.get('intrinsic') and owner.startswith(('Right','Add','Subtract','Multiply','Divide','Modulo','Equal','NotEqual','Less','Greater','And','Or','Xor','Shift','Negate','BitNot','Positive')):
        return '用户类型实现此运算符接口时提供 '+name+'；R 为运算结果类型，T 为操作数类型，左右方向由接口名决定。'
    if name.startswith('as') and module=='std::reflect': return '精确提取 '+name[2:]+' 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。'
    if name in ('drop', 'close'):
        return ('释放对象拥有的资源，调用后遵守本模块失效规则。' if name=='drop' else '关闭底层资源；文档节点会使同文档所有视图失效。')
    if name=='create' or name==owner:
        return {'Shared':'创建值为 value 的原子共享槽。', 'Arena':'创建独立 arena。', 'Timer':'以纳秒周期创建停止态定时器。'}.get(owner, '创建'+(owner or ('共享协程' if module=='std::coroutine' else '隔离线程'))+'，返回可供后续操作的值。')
    if name=='of': return {'Pair':'按 first、second 创建二元值。', 'Complex':'按 real、imag 创建复数值。', 'Interrupt':'组合事件句柄、所属线程和回调为中断描述值。'}.get(owner,'组合给定字段创建值。')
    if name in ('send','receive','push','pop','open','read','write','flush','seek','parse'):
        if module=='std::net': return {'send':'发送 TCP 文本并返回发送结果。','receive':'读取至多 maxBytes 字节；空串结合 status 判断原因。'}[name]
        if module in ('std::json','std::toml'): return '解析'+('JSON' if module.endswith('json') else 'TOML')+' 文本；成功返回根节点，失败返回诊断字符串。'
        if module=='std::io': return {'open':'按 path/mode 打开文件，返回流或 OpenFailed。','read':'读取一行文本，失败返回 ReadFailed。','write':'写入文本，返回写入字节数或 WriteFailed。','flush':'刷新输出缓冲，失败返回 WriteFailed。','seek':'按 origin 与 offset 定位，返回新位置或 SeekFailed。'}[name]
        if owner in ('FixedVector','RingBuffer') and name=='push': return '追加 value；容量已满时返回 false，成功为 true。'
        if owner in ('Deque','Queue','FixedVector','RingBuffer','Channel'):
            return {'push':'追加元素。','pop':'移除并返回下一元素；空容器返回 None。','send':'将 value 加入本地值队列。','receive':'取出队首值；空队列返回 None。'}[name]
        if module in ('std::thread','std::coroutine'): return '向目标任务发送字符串消息。' if name=='send' else '等待并接收当前任务队列的一条字符串消息。'
    for prefix in ('numerics','numtheory','random','signal'):
        if name.startswith(prefix) and len(name)>len(prefix):
            short=name[len(prefix):]; short=short[0].lower()+short[1:]
            return '宿主入口：'+behavior(dict(entry, name=short, owner={'random':'Random'}.get(prefix,owner)))
    if module=='std::path' and name=='join': return '将 left 与 right 组合为路径字符串。'
    if name in BEHAVIOR: return BEHAVIOR[name]
    comment=comment_text(entry['comment'])
    if comment and '迭代操作' not in comment and '搬出来' not in comment: return comment
    raise ValueError('缺少行为说明：'+module+'::'+owner+'::'+name)


def anchor(entry, index):
    """生成稳定、无需依赖 Markdown 渲染器标题规则的锚点。"""
    return 'api-'+str(index)


def filename(module):
    return module.replace('::','-')+'.md'


def table_text(value):
    """转义表格分隔符，兼容 Python 3.11 的 f-string 语法。"""
    return value.replace('|', '\\|')


def render(data):
    """生成模块页、全量符号索引与可审计的机器清单。"""
    from library_reference_examples import EXAMPLES, SUPPLEMENTAL
    output={}
    api_index=[]
    symbol_index=[]
    from library_reference_symbols import symbol_description
    missing_parameters=set()
    examples={}
    for module, paths in SUPPLEMENTAL.items():
        for path in paths:
            expected=ROOT/'example/expected'/Path(path).stem
            examples[path]=dict(id=path,source=path,code=(ROOT/path).read_text(encoding='utf-8'),
                arguments=['example.txt'] if module=='std::io' else [],flags=[],
                input=expected.with_suffix('.in').read_text(encoding='utf-8') if expected.with_suffix('.in').exists() else '',
                expected=expected.with_suffix('.txt').read_text(encoding='utf-8'),language='magel')
    module_info={}
    capability={name:group for group,names in {
        'core': ['std','std::lang','std::reflect'],
        'collections': ['std::collections','std::iter','std::algorithms'],
        'text': ['std::text','std::utf8','std::encoding','std::parse','std::regex','std::json','std::toml','std::csv','std::binary','std::compress'],
        'math': ['std::math','std::complex','std::linalg','std::numerics','std::numtheory','std::signal','std::random'],
        'io': ['std::io','std::path','std::cli','std::net','std::sql','std::process'],
        'concurrency': ['std::concurrency','std::coroutine','std::thread','std::message','std::event'],
        'system': ['std::memory','std::clock','std::datetime','std::watchdog','std::probe'],
    }.items() for name in names}
    index=['# 库函数 API 参考手册', '', '采用 C/C++ 参考手册常见的组织方式：模块 → 类型 → 函数/重载，逐项列出声明、参数、返回值、行为、边界及调用形式。语法仍为 Magel，不是 C++。', '',
           '覆盖工具链自带的 `std::lang`、`std`、`std::*` 与可选 `std::reflect`；编译器注入的算法和核心容器另有来源标记。`__` 前缀胶水是实现细节，不作为应用 API。底层 C++ 依赖库与宿主 C ABI 见[宿主集成](../08-host-integration.md)。', '',
           '## 阅读约定', '', '签名中 `static` 使用 `类型::方法`，实例方法使用 `对象.方法`。重载分别编号；泛型参数必须满足源码接口约束。`T`、`U`、`R` 等为类型参数，不能直接作为完整程序的具体类型。内建函数用等价调用签名表示，其参数检查由编译器完成。', '',
           '参数为值时遵循值语义；`const&` 只读借用，`mut&` 可写借用。修改/释放资源会影响依赖它的借用；`const&` 不能调用 `drop`。显式重复 `drop/delete` 的幂等不意味着释放后的值还能读取。', '',
           '每项的“调用形式”展示如何排列参数，参数名代表调用者已准备的值，不能单独运行。模块页链接的 `.mgl` 是带入口的完整参考代码；有输入、线程、反射或外部服务条件时以示例说明为准。错误条件既包括库显式返回的 Option/Result，也包括断言、宿主检查和借用检查诊断。', '',
           '## 模块目录', '', '| 模块 | 内容 | 函数重载数 |', '| --- | --- | ---: |']
    for module, records in data.items():
        title, contract=MODULES[module]
        # `__` 前缀是编译器/宿主协议，不是应用 API：既不进参考页，也不进编辑器索引，
        # 否则 LSP 会把 `__Range` 这类内部类型当成可补全的公开符号。
        functions=[r for r in records if r['kind']=='function' and not r['owner'].startswith('__')]
        index.append(f'| [`{module}`]({filename(module)}) | {title} | {len(functions)} |')
        lines=[f'# {module} — {title}', '', '[返回库目录](README.md) · [全部符号](symbols.md)', '', '## 引入', '', '```magel', '// std::lang 自动加载' if module=='std::lang' else '// 编译器内建，无需 import std' if module=='std' else 'import '+module+';', '```', '', '## 公共约定与错误边界', '', contract, '']
        if module=='std::lang': lines=['std::lang::sweep() 不是垃圾回收器。它不做引用追踪、不判断对象存活、不自动调用。它只做一件事：清理已释放块的墓碑记录。', '']+lines
        example=EXAMPLES.get(module)
        module_info[module]=dict(name=module,title=title,documentation=contract,capability=capability.get(module,'system'),sourceLanguage='zh',examples=([example] if example else [])+SUPPLEMENTAL.get(module,[]))
        lines+=['## 参考代码', '']
        if example:
            expected=ROOT/'example/expected'/Path(example).stem
            flags=' --threads' if expected.with_suffix('.threads').exists() else ' --reflection' if module=='std::reflect' else ''
            if module=='std::io': flags+=' -- build/library-example.txt'
            lines += [f'完整程序：[{example}](../../{example})。', '', '```powershell', 'magel run '+example+flags, '```', '']
            if module=='std::io': lines+=['先创建 `build` 目录；示例会覆盖给定路径，请使用专用测试文件。','']
            if module=='std::reflect': lines+=['需先用 `make reflection` 构建支持反射的工具链；当前二进制未编入支持时，上述命令会明确报缺失宏。','']
            path=ROOT/example
            if path.exists():
                example_flags=['--threads'] if expected.with_suffix('.threads').exists() else ['--reflection'] if module=='std::reflect' else []
                examples.setdefault(example,dict(id=example,source=example,code=path.read_text(encoding='utf-8'),
                    arguments=expected.with_suffix('.args').read_text(encoding='utf-8').splitlines() if expected.with_suffix('.args').exists() else ['example.txt'] if module=='std::io' else [],
                    flags=example_flags,input=expected.with_suffix('.in').read_text(encoding='utf-8') if expected.with_suffix('.in').exists() else '',
                    expected=expected.with_suffix('.txt').read_text(encoding='utf-8') if expected.with_suffix('.txt').exists() else '',language='magel'))
                lines+=['<details>', '<summary>展开完整参考代码</summary>', '', '```magel',path.read_text(encoding='utf-8').rstrip(), '```', '', '</details>', '']
            if expected.with_suffix('.txt').exists():
                lines+=['<details>','<summary>示例输出</summary>','','```text',expected.with_suffix('.txt').read_text(encoding='utf-8').rstrip(),'```','','</details>','']
        if module=='std::iter':
            lines+=['## 点调用与自由函数对应','','`Iter` 的适配器点调用由编译器转为同名自由函数，接收者作为第一个参数。例如 `cursor.map(mapper)` 等价于 `std::iter::map(cursor,mapper)`。下表中的方法同样适用各函数条目的参数、返回值与边界。','','| 点调用 | 对应入口 |','| --- | --- |']
            inference=(ROOT/'source/epilogue/generic_inference.cpp').read_text(encoding='utf-8')
            adapters=re.search(r'adapters\{(.*?)\};',inference,re.S).group(1)
            for adapter in re.findall(r'"(\w+)"',adapters):
                lines.append(f'| `cursor.{adapter}(...)` | `std::iter::{adapter}(cursor,...)` |')
            lines+=['| `list.iter()` / `list.iterMut()` | `listIter(&list)` / `listIterMut(&list)` |','| `list.intoIter()` | `listIntoIter(&list)`，移动元素，源容器清空 |','| `set.iter()` / `map.iter()` | `setIter(&set)` / `mapIter(&map)`，借用快照，来源容器保持不变 |','| `set.intoIter()` / `map.intoIter()` | `setIntoIter(&set)` / `mapIntoIter(&map)`，移动元素，源容器清空 |','| `text.chars()` / `range.intoIter()` | `chars(text)` / `fromRange(range)` |','']
        else:
            lines += ['聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。', '']
        types=[r for r in records if r['kind']!='function' and not r['name'].startswith('__') and not r['owner'].startswith('__')]
        if types:
            enum_types={r['name']:r for r in types if r['kind']=='enum'}
            lines+=['## 类型、枚举、常量与字段', '', '公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。', '', '| 所属类型 | 声明 | 说明 |', '| --- | --- | --- |']
            for r in types:
                if r['name'].startswith('__') or r['owner'].startswith('__'): continue
                if r['kind'] not in ('struct','enum','interface','member'): continue
                symbol=dict(r)
                if r['kind']=='member':
                    if r['owner'] in enum_types:
                        description=symbol_description(enum_types[r['owner']])+' 变体及载荷见声明。'
                        lines.append('| '+r['owner']+' | `'+table_text(r['signature'])+'` | '+table_text(description)+' |')
                        continue
                    match=re.search(r'([A-Za-z_]\w*)\s*(?:=[^;]*)?$',r['signature'])
                    if not match: continue
                    symbol['name']=match.group(1)
                    if symbol['name'].startswith('__'): continue
                    symbol['type']=r['signature'][:match.start()].strip()
                description=symbol_description(symbol)
                symbol_anchor='symbol-'+str(len(symbol_index))
                lines.append('| '+(r['owner'] or '模块')+' | <a id="'+symbol_anchor+'"></a>`'+table_text(r['signature'])+'` | '+table_text(description)+' |')
                symbol_index.append(dict(module=module,name=(r['owner']+'::' if r['owner'] else '')+symbol['name'],
                    owner=r['owner'],kind=r['kind'],type=symbol.get('type',''),signature=r['signature'],
                    documentation=description,comment='',parameters=[],source=r['source'],line=r['line'],
                    page=filename(module),anchor=symbol_anchor))
            lines.append('')
        lines+=['## 函数索引', '', '| 名称 | 声明 |', '| --- | --- |']
        for i,r in enumerate(functions):
            qualified=(r['owner']+'::' if r['owner'] else '')+r['name']
            lines.append(f'| [{qualified}](#{anchor(r,i)}) | `{table_text(r["signature"])}` |')
        lines.append('')
        for i,r in enumerate(functions):
            qualified=(r['owner']+'::' if r['owner'] else '')+r['name']
            description=behavior(r)
            params=parameters(r['signature'])
            lines += [f'<a id="{anchor(r,i)}"></a>', '', f'### {qualified}（{i+1}）', '', '**声明**', '', '```magel',r['signature']+';', '```', '', '**行为**', '', description, '']
            comment=comment_text(r['comment'])
            if comment and comment!=description and not any(t in comment for t in ('搬出来','迭代操作','已迁到','从编译器')):
                lines += [comment, '']
            lines += ['**参数**', '']
            parameter_info=[]
            if params:
                lines+=['| 参数 | 类型／声明 | 含义 | 默认值 |','| --- | --- | --- | --- |']
                for name,decl,default in params:
                    meaning=PARAMETERS.get(name)
                    param_comment=re.search(r'@param\s+'+re.escape(name)+r'\s+(.*?)(?=@\w+|\*/|$)',r['comment'],re.S)
                    if param_comment: meaning=re.sub(r'\s*\*\s*|\s+', ' ',param_comment.group(1)).strip()
                    if not meaning: missing_parameters.add(name); meaning='见本项行为及声明的类型约束'
                    parameter_info.append(dict(name=name,label=decl+(' = '+default if default else ''),documentation=meaning,default=default))
                    lines.append(f'| `{name}` | `{decl}` | {meaning} | `{default}` |' if default else f'| `{name}` | `{decl}` | {meaning} | 无 |')
            else: lines+=['无显式参数。实例方法使用当前接收者。']
            result=r['result'] or r['owner']
            returns=re.search(r'@return\s+(.*?)(?=@\w+|\*/|$)',r['comment'],re.S)
            return_text=re.sub(r'\s*\*\s*|\s+',' ',returns.group(1)).strip() if returns else ''
            if return_text in ('操作结果。','操作结果'): return_text=''
            if r['owner'] in ('List','Set','Map') and r['name'] in ('push','insert','clear','add','put') or r['owner']=='List' and r['name']=='remove': return_text='成功时返回 0；错误通过运行时诊断报告，该返回值不是成功标志。'
            if r['owner'] in ('Set','Map') and r['name']=='remove': return_text='找到并删除返回 1，键不存在返回 0。'
            if r['owner'] in ('Set','Map') and r['name'] in ('has','contains'): return_text='存在返回 1，不存在返回 0。'
            lines+=['', '**返回值**', '', '无返回值。' if result=='void' else f'`{result}`。'+(return_text or ('构造并初始化当前实例。' if not r['result'] else description)), '', '**错误与边界**', '', '遵循本页“公共约定与错误边界”。']
            assertions=re.findall(r'std::lang::assert\((.*?)(?:\);)',r.get('body',''),re.S)
            for assertion in assertions:
                lines+=['显式前置检查：`'+re.sub(r'\s+',' ',assertion)+'`。']
            if 'panic(' in r.get('body',''): lines+=['实现包含 panic 分支；调用前应处理空值或失败状态。']
            lines+=['', '**调用形式**', '', '```magel']
            generic=re.search(r'\b'+re.escape(r['name'])+r'\s*(<.*?>)?\s*\(',r['signature'])
            type_args=generic.group(1) if generic and generic.group(1) else ''
            # 约束不是实参；调用形式只保留泛型参数名。
            type_args=re.sub(r':[^,>]+','',type_args)
            owner_decl=next((t['signature'] for t in types if t['kind'] in ('struct','enum','interface') and t['name']==r['owner']), '')
            owner_type=re.match(r'(?:struct|enum|interface)\s+(\w+(?:<[^>]+>)?)',owner_decl)
            owner_type=owner_type.group(1) if owner_type else r['owner']
            prefix='' if module=='std::lang' else module+'::'
            call=(prefix+owner_type+'::' if r['owner'] and r['signature'].startswith('static ') else 'obj.' if r['owner'] else prefix)+r['name']+type_args+'('+', '.join(p[0] for p in params)+')'
            if r['name']==r['owner']: call='// 构造：'+r['owner']+' obj = new('+', '.join(p[0] for p in params)+')'
            lines += [call+';', '```', '', f'**源码**：[{r["source"]}:{r["line"]}](../../{r["source"]}#L{r["line"]})'+('（编译器内建，签名为等价调用形式）。' if r.get('intrinsic') else '。'), '']
            if example: lines += [f'**完整示例**：[{example}](../../{example})。', '']
            api_index.append(dict(module=module, name=qualified, owner=r['owner'], signature=r['signature'], documentation=description+'\n\n**返回值**：'+result+'。'+return_text, comment=r['comment'], parameters=parameter_info,
                returns=dict(type=result,documentation=return_text or ('无返回值。' if result=='void' else description)),
                errors=dict(documentation=contract,preconditions=[re.sub(r'\s+',' ',a) for a in assertions],panic='panic(' in r.get('body','')),
                usage=call+';',examples=[program for program in ([example] if example else [])+SUPPLEMENTAL.get(module,[])
                    if re.search(r'\b'+re.escape(r['name'])+r'\s*(?:<[^\n;]*>)?\s*\(',examples.get(program,{}).get('code',''))],
                sourceLanguage='zh',source=r['source'],line=r['line'],page=filename(module),anchor=anchor(r,i)))
        for extra in SUPPLEMENTAL.get(module,[]):
            lines+=['## 补充完整示例','','['+extra+'](../../'+extra+')。', '', '```magel',examples[extra]['code'].rstrip(),'```','']
        output[filename(module)]='\n'.join(lines)+'\n'
    if missing_parameters: raise ValueError('缺少参数含义：'+', '.join(sorted(missing_parameters)))
    index += ['', '## 查找与维护', '', '[全部符号索引](symbols.md)按限定名排序，适合浏览器搜索；[API 清单](api-index.json)记录每个重载、源码和锚点。', '', '参考页由 `python tools/library_reference.py` 根据声明和人工说明生成。`python tools/library_reference.py --check` 检查当前源码与入库页面是否一致；新增 API 后须同步补充行为、参数与示例。示例验证见 `python tests/magel_library_reference.py`。', '']
    index += ['[类型与字段清单](symbol-index.json)记录类型分类、字段类型、说明及源码位置，供编辑器与文档共用。', '']
    output['README.md']='\n'.join(index)
    symbols=['# 全部库函数符号', '', '[返回库目录](README.md)', '', '| 限定名 | 签名 |', '| --- | --- |']
    for r in sorted(api_index,key=lambda r:(r['module'],r['name'],r['signature'])):
        symbols.append(f'| [{r["module"]}::{r["name"]}]({r["page"]}#{r["anchor"]}) | `{table_text(r["signature"])}` |')
    output['symbols.md']='\n'.join(symbols)+'\n'
    for symbol in symbol_index:
        module=symbol['module'];example=EXAMPLES.get(module)
        symbol['examples']=[example] if example and re.search(r'\b'+re.escape(symbol['name'].split('::')[-1])+r'\b',examples.get(example,{}).get('code','')) else []
        symbol['sourceLanguage']='zh'
    output['api-index.json']=json.dumps(api_index,ensure_ascii=False,indent=2)+'\n'
    output['symbol-index.json']=json.dumps(symbol_index,ensure_ascii=False,indent=2)+'\n'
    # 聚合模块可能没有函数，仍须记录导入关系供独立部署的 LSP 使用。
    dependencies={}
    for module,records in data.items():
        candidates=[ROOT/r['source'] for r in records if r['source'].endswith('.mgl')]
        fallback=ROOT/(module.replace('::','/')+'.mgl')
        source=candidates[0] if candidates else fallback
        dependencies[module]=re.findall(r'\bimport\s+([\w:]+)\s*;',without_comments(source.read_text(encoding='utf-8'))) if source.exists() else []
    output['../../tools/magel_stdlib_api.json']=json.dumps(dict(schemaVersion=1,modules=dependencies,types=sorted({r['name'] for records in data.values() for r in records if r['kind'] in ('struct','enum','interface') and not r['name'].startswith('__')}),functions=api_index,symbols=symbol_index,
        reference=dict(modules=module_info,examples=examples,sourceLanguage='zh')),ensure_ascii=False,separators=(',', ':'))+'\n'
    # 编辑器把本清单当作公开 API 直接补全；`__` 前缀是内部协议，漏进来就等于把它公开了。
    leaked=sorted({row['name'] for row in api_index if row['name'].startswith('__') or row['owner'].startswith('__')} |
                  {row['name'] for row in symbol_index if row['name'].startswith('__') or row['owner'].startswith('__')} |
                  {name for name in json.loads(output['../../tools/magel_stdlib_api.json'])['types'] if name.startswith('__')})
    if leaked: raise ValueError('编辑器索引泄漏了内部符号：'+', '.join(leaked))
    return {name: text.rstrip()+'\n' if name.endswith('.md') else text for name,text in output.items()}


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true',help='只检查生成结果是否与入库文档一致')
    parser.add_argument('--inventory',action='store_true',help='输出声明清单供人工审核')
    args=parser.parse_args()
    data=inventory()
    if args.inventory:
        print(json.dumps(data,ensure_ascii=False,indent=2))
        sys.exit(0)
    output=render(data)
    directory=ROOT/'doc/library'
    directory.mkdir(exist_ok=True)
    stale=[]
    for name,text in output.items():
        path=directory/name
        if args.check:
            if not path.exists() or path.read_text(encoding='utf-8')!=text: stale.append(name)
        else: path.write_text(text,encoding='utf-8',newline='\n')
    if stale: sys.exit('参考文档需要更新：'+', '.join(stale))
    print(('核对通过' if args.check else '已生成')+f'：{len(data)} 个模块，'+str(sum(r['kind']=='function' and not r['owner'].startswith('__') for records in data.values() for r in records))+' 个函数重载。')
