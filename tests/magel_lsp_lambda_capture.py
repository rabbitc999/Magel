"""回归截图中的 fn move 提示及 MGL1205 精确定位。"""
from pathlib import Path
import os
import tempfile
from native_lsp_transport import Transport, position

ROOT=Path(__file__).resolve().parents[1]


def main():
    """闭包捕获模式不显示普通 move 函数，移后访问定位到实际变量。"""
    with tempfile.TemporaryDirectory(prefix='magel-lsp-capture-') as directory:
        root=Path(directory);uri=(root/'main.mgl').as_uri()
        text='''int64 main() {
    String a = "1";
    int64 len = std::lang::len(a);
    String fn() b = fn move() { a };
    println("{} {}", a, len);
    return 0;
}
'''
        server=Transport(ROOT/('magel.exe' if os.name=='nt' else 'magel'),root)
        server.query('initialize')
        server.send(dict(method='textDocument/didOpen',params={'textDocument':dict(uri=uri,text=text,version=1)}))
        params=dict(textDocument=dict(uri=uri),position=position(text,text.index('move')+2))
        hover=server.query('textDocument/hover',params)
        assert 'fn move' in hover['contents']['value'] and 'move<T>' not in hover['contents']['value'],hover
        diagnostics=server.diagnostic(uri,1,lambda ds:any(d.get('code')=='MGL1205' for d in ds))['diagnostics']
        moved=next(d for d in diagnostics if d.get('code')=='MGL1205')
        assert moved['range']['start']==dict(line=4,character=text.splitlines()[4].index(', a,')+2),moved
        assert moved['range']['end']['character']==moved['range']['start']['character']+1,moved
        copied=text.replace('fn move()', 'fn copy()')
        server.send(dict(method='textDocument/didChange',params=dict(textDocument=dict(uri=uri,version=2),contentChanges=[dict(text=copied)])))
        server.diagnostic(uri,2,lambda ds:not ds)
        signature=server.query('textDocument/signatureHelp',dict(textDocument=dict(uri=uri),position=position(copied,copied.index('copy(')+5)))
        assert signature is None,signature
        server.close()
    print('lambda 捕获悬停、函数提示过滤与 MGL1205 变量定位通过。')


if __name__=='__main__':main()
