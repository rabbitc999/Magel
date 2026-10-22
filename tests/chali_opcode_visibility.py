"""验证反射可见的 opcode 对象及隐藏的内部句柄与 LSP 实现信息。"""
import argparse
from pathlib import Path
import subprocess
import tempfile
from native_lsp_transport import Transport, position


def main():
    """核对真实 VM 元数据和原生 LSP 请求的公开结果。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali',default='ChaLi.exe')
    options = parser.parse_args()
    compiler = Path(options.chali).resolve()
    fixture = Path(__file__).resolve().parent/'fixtures/opcode_reflection.chl'
    with tempfile.TemporaryDirectory(prefix='chali-opcode-visibility-') as folder:
        root=Path(folder)
        result = subprocess.run([str(compiler),'run',str(fixture),'out',str(root/'reflection')],capture_output=True,encoding='utf-8',errors='replace',timeout=120)
        assert result.returncode == 0 and 'opcode reflection hidden' in result.stdout, result.stdout+result.stderr
        server=Transport(compiler,root)
        try:
            server.query('initialize',dict(rootUri=root.as_uri()))
            uri=(root/'main.chl').as_uri()
            text='opcode Secret {new=__std_bint_from_int;drop=__std_bint_drop;copy=__std_bint_copy;}; Secret hidden; void main(){}\n'
            server.send(dict(method='textDocument/didOpen',params=dict(textDocument=dict(uri=uri,text=text,version=1))))
            symbols=server.query('textDocument/documentSymbol',dict(textDocument=dict(uri=uri)))
            assert {item['name'] for item in symbols} == {'main'}, symbols
            completion=server.query('textDocument/completion',dict(textDocument=dict(uri=uri),position=position(text,len(text))))
            items=completion.get('items',[]) if isinstance(completion,dict) else completion
            labels={item.get('label') for item in items}
            assert not labels.intersection({'Secret','hidden','BigInteger','BigDecimal','File','Process'}),labels
        finally:
            server._kill()
    print('opcode visibility: reflection and native LSP passed')


if __name__ == '__main__':
    main()
