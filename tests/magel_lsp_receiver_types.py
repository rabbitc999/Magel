"""验证真实 LSP 的接收者类型、auto 悬停和同名类型隔离。"""
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from magel_lsp import inferred_type, ApiIndex
from magel_native_lsp_parity import NativeServer


def main():
    """在未保存文档中查询实例补全，覆盖初始化来源及词法作用域。"""
    prelude = '''struct Leaf { Leaf(){} int64 value; int64 read(){return value;} };
struct Other { bool flag; bool check(){return flag;} };
struct Wrapper { Leaf leaf; Leaf get(){Leaf result; return result;} };
struct Box<T> { T item; T get(){T result; return result;} };
Leaf makeLeaf(){Leaf result; return result;}
'''
    server = NativeServer(ROOT / 'magel.exe')
    try:
        with tempfile.TemporaryDirectory(prefix='magel-lsp-types-') as directory:
            folder = Path(directory)
            server.query('initialize', dict(rootUri=folder.as_uri()))
            uri = (folder/'main.mgl').as_uri()

            def query(body, marker, method='completion', head=prelude):
                text = head + body
                server.documents[uri] = dict(text=text, version=1)
                cursor = text.rindex(marker) + len(marker)
                before = text[:cursor]
                position = dict(line=before.count('\n'), character=len(before.rsplit('\n',1)[-1].encode('utf-16-le'))//2)
                result = server.query('textDocument/'+method, dict(textDocument=dict(uri=uri),position=position))
                return result

            cases = [
                ('void use(Leaf const& item){item.', 'item.'),
                ('int64 main(){Leaf item;item.', 'item.'),
                ('int64 main(){auto item=Leaf();item.', 'item.'),
                ('int64 main(){auto item=makeLeaf();item.', 'item.'),
                ('int64 main(){Leaf original;auto item=original;item.', 'item.'),
                ('int64 main(){Wrapper holder;auto item=holder.leaf;item.', 'item.'),
                ('int64 main(){Wrapper holder;holder.leaf.', 'holder.leaf.'),
                ('int64 main(){Wrapper holder;holder.get().', 'holder.get().'),
                ('int64 main(){makeLeaf().', 'makeLeaf().'),
                ('int64 main(){Box<Leaf> holder;holder.item.', 'holder.item.'),
                ('int64 main(){Box<Leaf> holder;auto item=holder.get();item.', 'item.'),
                ('int64 main(){Leaf item;{Other item;item.check();}item.', 'item.'),
            ]
            for body,marker in cases:
                result = query(body,marker)
                labels = {r['label'] for r in result}
                assert labels == {'read','value'}, (body,result)
            result = query('int64 main(){Leaf item;{Other item;item.', 'item.')
            assert {r['label'] for r in result}=={'check','flag'}, result
            assert query('int64 main(){auto item=missing();item.', 'item.')==[]
            for initializer, expected in [('makeLeaf()','Leaf'), ('42','int64'), ('1.25','float64'), ('true','bool'), ('"hello"','String')]:
                body=f'int64 main(){{auto item={initializer};item;}}'
                hover=query(body,'item','hover')
                assert expected+' item' in hover['contents']['value'], (body,hover)
                hover=query(body,'auto','hover')
                assert expected+' item' in hover['contents']['value'], (body,hover)
                # Python 参考实现共用同样的初始化类型约定。
                index=ApiIndex(); entries=index.scan(uri,prelude+body)
                entries=[*entries,*index.symbols[uri]]
                inferred=inferred_type(entries,'item',prelude+body,len(prelude)+body.rindex('item')+4)
                assert expected in inferred, (body,inferred)
            for module, method in [('a','onlyA'),('b','onlyB')]:
                (folder/f'{module}.mgl').write_text(f'module {module};struct Same {{int64 {method}(){{return 0;}}}};',encoding='utf-8')
            head='import a;import b;\n'
            for module,method in [('a','onlyA'),('b','onlyB')]:
                result=query(f'int64 main(){{{module}::Same item;item.', 'item.',head=head)
                assert {r['label'] for r in result}=={method},result
            result=query('int64 main(){Same item;item.', 'item.',head=head)
            assert result==[],result
    finally:
        server.close()
    print('LSP 实例成员、auto 具体类型、泛型字段与返回值、局部遮蔽及模块同名隔离全部通过。')


if __name__ == '__main__':
    main()
