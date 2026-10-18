"""标准库、工程模块注释和跨文件 LSP 提示回归。"""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from mageldev import LanguageServer, position, uri_path
from magel_lsp import call_context, imports, matching


def check_smart_tokens(server,uri):
    """验证声明、引用、遮蔽、泛型、数字及未闭合词法单元的语义着色。"""
    source='''struct lower<T> { T value; };
int64 main(){
    List<int64> values;
    lower<String> item;
    int64 List=1, other=2;
    String emoji="😀"; int64 number=0xFF;
    { int64 lower=3; std::lang::println("{}",lower); }
    lower<String> second;
    List=List+other;
    return 0;
}
// lower List are comments
/* unfinished lower List
'''
    server.documents[uri]={'text':source,'version':1}
    result=server.query('textDocument/semanticTokens/full',{'textDocument':{'uri':uri}})
    legend=server.query('initialize',{})['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
    decoded=[];line=column=0
    for i in range(0,len(result['data']),5):
        dl,dc,length,kind,mods=result['data'][i:i+5]
        line+=dl;column=dc if dl else column+dc
        fragment=source.splitlines()[line].encode('utf-16-le')[column*2:(column+length)*2].decode('utf-16-le')
        decoded.append((fragment,legend[kind],line,column))
    assert any(word=='lower' and kind=='type' for word,kind,_,_ in decoded)
    assert any(word=='lower' and kind=='variable' for word,kind,_,_ in decoded)
    assert any(word=='T' and kind=='typeParameter' for word,kind,_,_ in decoded)
    assert any(word=='values' and kind=='variable' for word,kind,_,_ in decoded)
    assert any(word=='List' and kind=='type' for word,kind,_,_ in decoded)
    assert any(word=='List' and kind=='variable' for word,kind,_,_ in decoded)
    assert any(word=='0xFF' and kind=='number' for word,kind,_,_ in decoded)
    assert any('😀' in word and kind=='string' for word,kind,_,_ in decoded)
    assert all(kind=='comment' for _,kind,line,_ in decoded if line>=11)
    for word,kind,line,column in decoded:
        if word=='number':assert column==source.splitlines()[line].index('number')+1  # emoji adds one UTF-16 unit
    assert server.query('textDocument/completion',{'textDocument':{'uri':uri},'position':position(source,len(source))})==[]
    server.documents[uri]={'text':'import std::ma','version':2}
    items=server.query('textDocument/completion',{'textDocument':{'uri':uri},'position':position('import std::ma',14)})
    assert any(item['label']=='math' and item['kind']==9 for item in items),items
    source='module demo;\nint64 main(){ std::math::sq }'
    server.documents[uri]={'text':source,'version':3}
    items=server.query('textDocument/completion',{'textDocument':{'uri':uri},'position':position(source,source.index('sq')+2)})
    sqrt=next(item for item in items if item['label']=='sqrt')
    assert sqrt['additionalTextEdits'][0]['newText']=='\nimport std::math;\n',sqrt
    source='std::sql::'
    server.documents[uri]={'text':source,'version':4}
    items=server.query('textDocument/completion',{'textDocument':{'uri':uri},'position':position(source,len(source))})
    database=next(item for item in items if item['label']=='Database')
    assert database['kind']==22 and database['additionalTextEdits'][0]['newText']=='import std::sql;\n'



def check_short_completion(server, uri, root):
    """验证完整提示名与插入文本分离，覆盖重载、类型及局部遮蔽。"""
    with (root/'modules/calc.mgl').open('a',encoding='utf-8') as stream:
        stream.write('\nstruct Widget {int64 value;};\n')
    (root/'modules/left.mgl').write_text('module left;\nstruct Shared {int64 value;};\nint64 collide(){return 1;}\n',encoding='utf-8')
    (root/'modules/right.mgl').write_text('module right;\nstruct Shared {int64 value;};\nint64 collide(){return 2;}\n',encoding='utf-8')
    def items(source):
        cursor=source.index('/*cursor*/')
        server.documents[uri]={'text':source,'version':20}
        return server.query('textDocument/completion',{'textDocument':{'uri':uri},'position':position(source,cursor)})
    result=items('import std::math;\nimport calc;\nint64 main(){int64 localValue=1; /*cursor*/ return 0;}')
    for label,insert in (('std::lang::println','println'),('calc::Widget','Widget'),('std::math::sqrt','sqrt'),('calc::add','add')):
        choices=[item for item in result if item['label']==label]
        assert choices and all(item['insertText']==insert for item in choices),(label,choices)
    assert any(item['label']=='localValue' and item.get('insertText',item['label'])=='localValue' for item in result)
    result=items('module local;\nint64 overload(int64 v){return v;}\nfloat64 overload(float64 v){return v;}\nint64 main(){ /*cursor*/ return 0;}')
    overloads=[item for item in result if item['label']=='overload']
    assert overloads and all(item['insertText']=='overload' for item in overloads),overloads
    result=items('import left;\nimport right;\nint64 main(){ /*cursor*/ return 0;}')
    for label in ('left::Shared','right::Shared','left::collide','right::collide'):
        item=next(item for item in result if item['label']==label)
        assert item['insertText']==label,item
    result=items('import calc;\nint64 main(){int64 println=1; {int64 Widget=2; /*cursor*/} return 0;}')
    for label in ('std::lang::println','calc::Widget'):
        item=next(item for item in result if item['label']==label)
        assert item['insertText']==label,item
    result=items('int64 main(){{int64 println=1;} /*cursor*/ return 0;}')
    assert next(item for item in result if item['label']=='std::lang::println')['insertText']=='println'
    result=items('int64 main(){std::lang::pri/*cursor*/ return 0;}')
    assert next(item for item in result if item['label']=='println')['insertText']=='println'


def main():
    with tempfile.TemporaryDirectory(prefix='magel-lsp-api-') as directory:
        root = Path(directory)
        (root / 'modules').mkdir()
        source = root / 'main.mgl'
        module = root / 'modules' / 'calc.mgl'
        module.write_text('''module calc;
import cycle;
/** @brief 计算和。
 * @param a 左操作数。
 * @param b 右操作数。
 * @return 和。
 */
(int64, T) pair<T>(int64 a, T b) { return a, b; }
/// 整数相加。
int64 add(int64 a, int64 b = 1) { return a+b; }
/// 浮点相加。
float64 add(float64 a, float64 b) { return a+b; }
''', encoding='utf-8')
        (root / 'modules' / 'cycle.mgl').write_text('module cycle; import calc; int64 more(){return 1;}', encoding='utf-8')
        (root / 'magel.toml').write_text('[package]\nname="lsp"\nversion="0.1.0"\n[magel]\nimport_paths=["modules"]\n', encoding='utf-8')
        text = '''import std::math;
import std::sql;
import calc;
int64 main() {
    String unicode = "😀";
    calc::add(1, 2);
    std::math::sqrt(4.0);
    std::sql::Database::sqlite(":memory:");
    calc::pair<String>(1, "x");
    return 0;
}
'''
        source.write_text(text, encoding='utf-8')
        uri = source.as_uri()
        server = LanguageServer(ROOT / 'magel.exe')
        check_short_completion(server, uri, root)
        capabilities = server.query('initialize', {'rootUri': root.as_uri()})['capabilities']
        assert capabilities['signatureHelpProvider']
        server.documents[uri] = {'text': text, 'version': 1}
        def query(method, needle, delta=0):
            return server.query('textDocument/' + method, {'textDocument': {'uri': uri}, 'position': position(text, text.index(needle)+delta)})
        hover = query('hover', 'add(', 1)['contents']['value']
        assert '整数相加' in hover and '浮点相加' in hover, hover
        tips = query('signatureHelp', 'add(1, ', len('add(1, '))
        assert len(tips['signatures']) == 2 and tips['activeParameter'] == 1, tips
        tips = query('signatureHelp', 'pair<String>(1, ', len('pair<String>(1, '))
        assert 'T b' in tips['signatures'][0]['parameters'][1]['label'], tips
        assert '左操作数' in tips['signatures'][0]['documentation']['value']
        assert 'sqrt' in query('hover', 'sqrt(', 1)['contents']['value']
        tips = query('signatureHelp', 'sqlite(', len('sqlite('))
        assert 'path' in tips['signatures'][0]['label'], tips
        completion = query('completion', 'std::math::sqrt', len('std::math::'))
        assert any(c['label'] == 'sqrt' and c['documentation']['value'] for c in completion)
        assert not any('sqlite' in c['label'] for c in completion)
        target = query('definition', 'add(', 1)
        assert len(target) == 2 and all(t['uri'] == module.as_uri() for t in target), target
        # 未保存模块覆盖磁盘；不必重新打开调用文件。
        server.documents[module.as_uri()] = {'text': 'module calc; /// 新文档。\nint64 add(int64 a){return a;}\n', 'version': 2}
        assert '新文档' in query('hover', 'add(', 1)['contents']['value']
        symbols = server.query('workspace/symbol', {'query': 'calc::add'})
        assert len(symbols) == 1 and symbols[0]['location']['uri'] == module.as_uri(), symbols
        # 字符串、注释和嵌套调用的逗号不增加外层参数编号。
        assert call_context('calc::add(inner(1,2), "a,b", /* , */ ', 40)[1] == 2
        assert call_context('calc::add(inner(1, ', 18) == ('inner', 1)
        nested='calc::add(calc::pair<int64,String>(1, \"x\"), '
        assert call_context(nested,len(nested)) == ('calc::add',1)
        text+='\nstruct Controller { float64 step(float64 target,float64 measured,float64 dt){return target;} };\nController controller;\ncontroller.step(1.0, 2.0, 0.1);\n'
        server.documents[uri]['text']=text
        assert any(c['label']=='step' for c in query('completion','controller.step',len('controller.')))
        assert 'step' in query('hover','step(',1)['contents']['value']
        assert query('signatureHelp','step(1.0, ',len('step(1.0, '))['activeParameter']==1
        assert imports('String s="import fake;"; // import fake2;\nimport calc;')==['calc']
        entries=server.api_index.collect(uri,text,server.documents, uri_path)
        assert matching(entries,'controller.step','Controller mut & controller;')
        assert not matching(entries,'unknown.step','')
        # 锁定依赖可位于工程之外，且可以配置自己的 import_paths。
        dependency=root/'vendor';(dependency/'src').mkdir(parents=True)
        (dependency/'magel.toml').write_text('[package]\nname="vendor"\nversion="0.1.0"\n[magel]\nimport_paths=["src"]\n',encoding='utf-8')
        (dependency/'src'/'external.mgl').write_text('module external; /** 外部依赖说明。 */ int64 help(){return 0;}',encoding='utf-8')
        (root/'magel.lock').write_text('[dependencies]\nvendor=["path:vendor","vendor","0.1.0","hash"]\n',encoding='utf-8')
        text='import external;\nint64 main(){return external::help();}'
        server.documents[uri]['text']=text
        assert '外部依赖说明' in query('hover','help()',1)['contents']['value']
        (dependency/'src'/'external.mgl').write_text('module external; /// 磁盘更新。\nint64 help(){return 0;}',encoding='utf-8')
        assert '磁盘更新' in query('hover','help()',1)['contents']['value']
        server.documents[uri]['text']='calc::a'
        items=server.query('textDocument/completion',{'textDocument':{'uri':uri},'position':position('calc::a',7)})
        add=next(item for item in items if item['label']=='add')
        assert add['additionalTextEdits'][0]['newText']=='import calc;\n'
        check_smart_tokens(server,uri)
        text=source.read_text(encoding='utf-8')
        if '--unit-only' in sys.argv:
            print('LSP 声明与模块索引单元检查通过。')
            return
        # 真实 stdio 协议，检测内嵌资产释放和单文件部署。
        compiler = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')
        messages = [
            {'id': 1, 'method': 'initialize', 'params': {'rootUri': root.as_uri()}},
            {'method': 'textDocument/didOpen', 'params': {'textDocument': {'uri': uri, 'version': 1, 'text': text}}},
            {'id': 2, 'method': 'textDocument/signatureHelp', 'params': {'textDocument': {'uri': uri}, 'position': position(text, text.index('sqrt(')+5)}},
            {'id': 4, 'method': 'textDocument/hover', 'params': {'textDocument': {'uri': uri}, 'position': position(text,text.index('add(')+1)}},
            {'id': 5, 'method': 'workspace/symbol', 'params': {'query':'calc::pair'}},
            {'id': 6, 'method': 'textDocument/definition', 'params': {'textDocument': {'uri': uri}, 'position': position(text,text.index('add(')+1)}},
            {'id': 7, 'method': 'textDocument/semanticTokens/full', 'params': {'textDocument': {'uri': uri}}},
            {'id': 3, 'method': 'shutdown'}, {'method': 'exit'}]
        payload = b''
        for message in messages:
            data = json.dumps(dict(jsonrpc='2.0', **message), ensure_ascii=False).encode('utf-8')
            payload += f'Content-Length: {len(data)}\r\n\r\n'.encode() + data
        result = subprocess.run([str(compiler), 'lsp'], input=payload, capture_output=True, cwd=root, timeout=90)
        assert result.returncode == 0, result.stderr.decode('utf-8', errors='replace')
        assert '整数相加'.encode('utf-8') in result.stdout
        assert b'calc::pair' in result.stdout and module.as_uri().encode('utf-8') in result.stdout
        assert b'semanticTokensProvider' in result.stdout and b'typeParameter' in result.stdout
        assert b'activeParameter' in result.stdout and b'float64 sqrt' in result.stdout, result.stdout.decode('utf-8', errors='replace')
    print('标准库、泛型、重载、注释、跨模块定义、未保存模块、工程符号和内嵌 LSP 验证通过。')


if __name__ == '__main__':
    main()
