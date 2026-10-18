"""编辑器类型、字段和大纲种类回归。"""
from pathlib import Path
import sys
import json
import subprocess
import tempfile
import os
COMPILER = Path(os.environ.get("MAGEL_COMPILER", "magel.exe")).resolve()
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from mageldev import LanguageServer, position
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    (root/'magel.toml').write_text('[package]\nname="editor"\n', encoding='utf-8')
    source = 'module types;\n/// 文档类型\nstruct Box {\n/// 字段说明\nint64 value;\n};\n'
    (root/'types.mgl').write_text(source, encoding='utf-8')
    uri=(root/'main.mgl').as_uri()
    text='import types;\nint64 main(){ Box box; box.value; auto narrow=1.25e-2f; auto wide=1.25; auto bits=0xff; auto unsignedValue=1u; auto hexadecimal=0xffu; auto binary=b_101u; return 0; }\n'
    server=LanguageServer(COMPILER)
    server.query('initialize', {'workspaceFolders':[{'uri':root.as_uri()}]})
    server.documents[uri]={'text':text,'version':1}
    def query(method, marker):
        return server.query('textDocument/'+method, {'textDocument':{'uri':uri},'position':position(text,text.index(marker)+len(marker))})
    definition=query('definition','Box')
    target=definition if isinstance(definition,dict) else definition[0]
    assert target['uri'].endswith('types.mgl')
    assert '字段说明' in query('hover','box.value')['contents']['value']
    for name, kind in [('narrow', 'float32'), ('wide', 'float64'), ('bits', 'int64'), ('unsignedValue', 'uint64'), ('hexadecimal', 'uint64'), ('binary', 'uint64')]:
        assert kind in query('hover', name)['contents']['value']
    items=query('completion','box.')
    assert any(item['label']=='value' and item['kind']==5 for item in items)
    symbols=server.query('textDocument/documentSymbol',{'textDocument':{'uri':uri}})
    assert any(item['name']=='main' and item['kind']==12 for item in symbols)
    assert any(item['name']=='types::Box' and item['kind']==23 for item in server.workspace_symbols('Box'))
    server.documents[uri]['text']='types::B'
    items=server.query('textDocument/completion',{'textDocument':{'uri':uri},'position':position('types::B',8)})
    assert any(item['label']=='Box' and item['kind']==22 and item.get('additionalTextEdits') for item in items)
    server.documents[uri]['text']=text
    requests=[{'id':1,'method':'initialize','params':{'rootUri':root.as_uri()}},
        {'method':'textDocument/didOpen','params':{'textDocument':{'uri':uri,'text':text,'version':1}}},
        {'id':2,'method':'textDocument/hover','params':{'textDocument':{'uri':uri},'position':position(text,text.index('box.value')+7)}},
        *[{'id': index, 'method': 'textDocument/hover', 'params': {'textDocument': {'uri': uri},
            'position': position(text, text.index(name) + 2)}} for index, name in enumerate(('narrow', 'wide', 'bits', 'unsignedValue', 'hexadecimal', 'binary'), 4)],
        {'id':3,'method':'shutdown'}, {'method':'exit'}]
    payload=b''
    for request in requests:
        data=json.dumps(dict(jsonrpc='2.0',**request),ensure_ascii=False).encode('utf-8')
        payload+=f'Content-Length: {len(data)}\r\n\r\n'.encode()+data
    result=subprocess.run([str(COMPILER),'lsp'],input=payload,capture_output=True,cwd=root,timeout=60)
    assert result.returncode==0, result.stderr
    assert '字段说明'.encode('utf-8') in result.stdout, result.stdout
    for signature in ('float32 narrow', 'float64 wide', 'int64 bits', 'uint64 unsignedValue', 'uint64 hexadecimal', 'uint64 binary'):
        assert signature.encode('utf-8') in result.stdout, result.stdout
print('编辑器类型、字段、定义、文档、大纲及工程符号回归通过。')
