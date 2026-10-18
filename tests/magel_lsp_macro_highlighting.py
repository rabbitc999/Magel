"""验证真实 LSP 对宏内部指令、文本形参、循环变量和整数槽的独立着色。"""
from pathlib import Path
import os
import tempfile
from native_lsp_transport import Transport, position

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('MAGEL_TEST_COMPILER', ROOT / 'magel.exe')).resolve()


def main():
    """确保宏内角色准确，字符串注释不被重新解释，宏外同名变量不继承角色。"""
    text = '''$macro example(...A)(...B)
{
    print("A=[");
    $for var in A
    print("{} ", var);
    $end_for
    println("]");
    print("B=[");
    $for var in B
    print("{} ", var);
    $end_for
    println("]");
    int64 $0 = 1;
    int64 $10 = $0;
    int64 $-123456789012345678901234567890 = $10;
    // $for var in A $end_macro
    println("$for var in A $end_macro");
}
$end_macro
int64 main(){int64 A=1;int64 var=2;$example(A)(var);return A+var;}
'''
    with tempfile.TemporaryDirectory(prefix='magel-macro-colors-') as folder:
        root = Path(folder)
        uri = (root / 'main.mgl').as_uri()
        server = Transport(COMPILER, root)
        try:
            init = server.query('initialize', dict(rootUri=root.as_uri()))
            legend = init['capabilities']['semanticTokensProvider']['legend']
            server.send(dict(method='textDocument/didOpen', params=dict(textDocument=dict(uri=uri, version=1, text=text))))
            data = server.query('textDocument/semanticTokens/full', dict(textDocument=dict(uri=uri)))['data']
            tokens = {}
            line = column = 0
            for offset in range(0, len(data), 5):
                delta_line, delta_column, length, kind, modifiers = data[offset:offset + 5]
                line += delta_line
                column = delta_column if delta_line else column + delta_column
                tokens[(line, column)] = (length, legend['tokenTypes'][kind], modifiers)

            def token(needle, start=0):
                """取得原始源码指定片段对应的 UTF-16 语义 token。"""
                at = text.index(needle, start)
                point = position(text, at)
                return tokens[(point['line'], point['character'])]

            assert token('$macro')[1] == 'keyword'
            assert token('example')[1:] == ('function', 1)
            assert token('A)')[1:] == ('parameter', 3)
            assert token('B)')[1:] == ('parameter', 3)
            assert token('$for')[1] == 'keyword'
            assert token('var in A')[1:] == ('variable', 3)
            assert token('A\n')[1:] == ('parameter', 2)
            assert token('var);')[1:] == ('variable', 2)
            assert token('var in B')[1:] == ('variable', 3)
            assert token('B\n')[1:] == ('parameter', 2)
            assert token('$end_for')[1] == 'keyword'
            assert token('$end_macro', text.index('\n$end_macro'))[1] == 'keyword'
            for slot in ['$0', '$10', '$-123456789012345678901234567890']:
                assert token(slot) == (len(slot), 'variable', 2), (slot, token(slot))
            assert token('// $for')[1] == 'comment'
            assert token('"$for var')[1] == 'string'
            outside = text.index('int64 main')
            assert token('A=1', outside)[1:] == ('variable', 1)
            assert token('var=2', outside)[1:] == ('variable', 1)
            assert token('$example', outside)[1] == 'function'
            assert not (root / 'main.mgl').exists()
        finally:
            server.close()
    print('原生语义着色的宏指令、独立参数组、循环绑定、整数槽及宏外作用域恢复通过。')


if __name__ == '__main__':
    main()
