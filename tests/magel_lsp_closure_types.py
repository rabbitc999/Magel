"""验证原生 LSP 对自动闭包及多层函数返回类型的完整推导。"""
import argparse
from pathlib import Path
import tempfile
import os
import subprocess
from native_lsp_transport import Transport, position


def main():
    """悬停同时查询 auto 和绑定，并验证连续调用继续保留返回类型。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--magel', type=Path, default=Path('magel.exe'))
    compiler = parser.parse_args().magel.resolve()
    cases = [
        ('fn(int64 x) { x + 1 }', 'int64 fn(int64)'),
        ('fn(bool b) { if (b) { return increment; } return increment; }', '(int64 fn(int64)) fn(bool)'),
        ('fn move(int64 x) { return x; }', 'int64 fn(int64)'),
        ('fn copy[seed](int64 x) { x }', 'int64 fn(int64)'),
        ('fn(int32 x, bool b) { x }', 'int32 fn(int32, bool)'),
        ('fn(int32 x) { x + 1 }', 'int32 fn(int32)'),
        ('fn(int32 x) { 1 + x }', 'int32 fn(int32)'),
        ('fn(int8 x) { x + 1 }', 'int8 fn(int8)'),
        ('fn(int8 x) { x + 128 }', 'int64 fn(int8)'),
        ('fn(int32 x) { x + 2147483648 }', 'int64 fn(int32)'),
        ('fn(uint8 x) { x + 1u }', 'uint8 fn(uint8)'),
        ('fn(int32 x, uint16 y) { x + y }', 'uint32 fn(int32, uint16)'),
        ('fn(int64 x, uint32 y) { x + y }', 'uint64 fn(int64, uint32)'),
        ('fn(float32 x, int64 y) { x + y }', 'float32 fn(float32, int64)'),
        ('fn(char x, char y) { x + y }', 'int64 fn(char, char)'),
        ('fn(int32 x) { x + 1 < 3 }', 'bool fn(int32)'),
        ('fn(int8 x) { x + 0xFF }', 'int8 fn(int8)'),
        ('fn<T> copy[seed](T x) { x }', 'T fn<T>(T)'),
        ('fn<T, U>(T x, U y) { x }', 'T fn<T, U>(T, U)'),
        ('fn(int64 x) { fn copy(int64 y) { x + y } }', '(int64 fn(int64)) fn(int64)'),
        ('fn() { textIdentity }', '(String fn(String)) fn()'),
        ('fn() { increment }', '(int64 fn(int64)) fn()'),
        ('fn() { return increment; }', '(int64 fn(int64)) fn()'),
        ('fn() { return fn(int64 x) { x }; }', '(int64 fn(int64)) fn()'),
        ('fn() { fn() { increment } }', '((int64 fn(int64)) fn()) fn()'),
        ('fn() { return fn() { return increment; }; }', '((int64 fn(int64)) fn()) fn()'),
        ('fn() { fn() { fn(int32 x) { x } } }', '((int32 fn(int32)) fn()) fn()'),
        ('fn() {}', None),
        ('fn(bool b) { if (b) { return 1; } return true; }', None),
    ]
    with tempfile.TemporaryDirectory(prefix='magel-lsp-closure-types-') as folder:
        programs = [
            'auto make = fn(int64 x) { fn copy(int64 y) { x + y } }; auto value = make(2); assert(value(3) == 5);',
            'auto make = fn() { increment }; auto value = make(); assert(value(3) == 4);',
            'auto make = fn() { return increment; }; auto value = make(); assert(value(3) == 4);',
            'auto make = fn() { fn() { increment } }; auto middle = make(); auto value = middle(); assert(value(3) == 4);',
            'int32 fn(int32) value = fn(int32 x) { x + 1 }; assert(value(2) == 3);',
            'bool fn(int32) value = fn(int32 x) { x + 1 < 3 }; assert(value(1));',
        ]
        for number, body in enumerate(programs):
            path = Path(folder) / f'runtime_{number}.mgl'
            path.write_text('int64 increment(int64 x) { return x + 1; }\nint64 main() { ' + body + ' return 0; }', encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(path), '--out', str(Path(folder) / f'out_{number}')], capture_output=True, text=True, encoding='utf-8', env=dict(os.environ, MAGEL_LANG='zh'))
            assert result.returncode == 0 and not result.stderr, (body, result)
        server = Transport(compiler, folder)
        initialized = server.query('initialize')
        version = (Path(__file__).resolve().parents[1] / 'VERSION').read_text(encoding='utf-8').strip()
        assert initialized['serverInfo']['version'] == version, initialized
        try:
            for number, (initializer, expected) in enumerate(cases):
                text = 'String textIdentity(String x) { return x; }\nint64 increment(int64 x) { return x + 1; }\nint64 main() { int64 seed = 1; auto closure = ' + initializer + '; closure; return 0; }'
                uri = (Path(folder) / f'case_{number}.mgl').as_uri()
                server.send(dict(method='textDocument/didOpen', params={'textDocument':dict(uri=uri, text=text, version=1)}))
                for offset in (text.index('auto') + 1, text.rindex('closure') + 2):
                    hover = server.query('textDocument/hover', dict(textDocument=dict(uri=uri), position=position(text, offset)))
                    assert hover, (initializer, expected)
                    if expected is None:
                        assert '尚不能唯一确定类型' in hover['contents']['value'], (initializer, hover)
                    else:
                        assert expected + ' closure' in hover['contents']['value'], (initializer, expected, hover)
            calls = [
                ('fn(int64 x) { fn copy(int64 y) { x + y } }', 'closure(1)', 'int64 fn(int64)'),
                ('fn(int64 x) { fn copy(int64 y) { x + y } }', 'closure(1)(2)', 'int64'),
                ('fn() { increment }', 'closure()', 'int64 fn(int64)'),
                ('fn() { increment }', 'closure()(2)', 'int64'),
                ('fn() { fn() { increment } }', 'closure()()', 'int64 fn(int64)'),
                ('fn() { fn() { increment } }', 'closure()()(3)', 'int64'),
                ('fn<T>(T x) { x }', 'closure(3)', 'int64'),
                ('fn<T>(T x) { x }', 'closure<int32>(3)', 'int32'),
            ]
            for number, (initializer, call, expected) in enumerate(calls):
                text = 'int64 increment(int64 x) { return x + 1; }\nint64 main() { auto closure = ' + initializer + '; auto result = ' + call + '; result; return 0; }'
                uri = (Path(folder) / f'call_{number}.mgl').as_uri()
                server.send(dict(method='textDocument/didOpen', params={'textDocument':dict(uri=uri, text=text, version=1)}))
                hover = server.query('textDocument/hover', dict(textDocument=dict(uri=uri), position=position(text, text.rindex('result') + 2)))
                assert hover and expected + ' result' in hover['contents']['value'], (initializer, call, expected, hover)
                completion = server.query('textDocument/completion', dict(textDocument=dict(uri=uri), position=position(text, text.rindex('result') + 2)))
                entries = completion.get('items', []) if isinstance(completion, dict) else completion
                assert any(item['label'] == 'result' and expected + ' result' == item.get('detail') for item in entries), (call, expected, entries)
            for number, (member, expected) in enumerate([
                ('fn(int32 x) { x }', 'int32 fn(int32)'),
                ('fn<T>(T x) { x }', 'T fn<T>(T)'),
                ('fn() { increment }', '(int64 fn(int64)) fn()'),
            ]):
                text = 'int64 increment(int64 x) { return x + 1; }\nstruct Holder { auto callback = ' + member + '; };\nint64 main() { Holder holder; auto result = holder.callback; result; return 0; }'
                uri = (Path(folder) / f'member_{number}.mgl').as_uri()
                server.send(dict(method='textDocument/didOpen', params={'textDocument':dict(uri=uri, text=text, version=1)}))
                hover = server.query('textDocument/hover', dict(textDocument=dict(uri=uri), position=position(text, text.rindex('result') + 2)))
                assert hover and expected + ' result' in hover['contents']['value'], (member, expected, hover)
            signature_text = 'int64 main() { auto outer = fn(int64 x) { fn(int32 y) { y } }; auto inner = outer(1); inner('
            uri = (Path(folder) / 'signature.mgl').as_uri()
            server.send(dict(method='textDocument/didOpen', params={'textDocument':dict(uri=uri, text=signature_text, version=1)}))
            signature = server.query('textDocument/signatureHelp', dict(textDocument=dict(uri=uri), position=position(signature_text, len(signature_text))))
            assert signature and signature['signatures'][0]['label'] == 'int32 fn(int32)', signature
        finally:
            server.close()
    print(f'LSP auto 闭包、泛型闭包、多层闭包和具名函数返回：{len(cases) + len(calls) + 3} 组提示、{len(programs)} 组 VM 执行通过。')


if __name__ == '__main__':
    main()
