"""验证真实原生 LSP 的推断、宏分组、绑定引用、安全重命名及关联诊断。"""
from pathlib import Path
import os
import tempfile
import time
from native_lsp_transport import Transport, position

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('CHALI_TEST_COMPILER', ROOT / 'ChaLi.exe')).resolve()


def main():
    """通过未保存缓冲区覆盖各能力共享的声明身份与类型信息。"""
    with tempfile.TemporaryDirectory(prefix='chali-lsp-semantics-') as folder:
        root = Path(folder)
        server = Transport(COMPILER, root)
        try:
            capabilities = server.query('initialize', dict(rootUri=root.as_uri()))['capabilities']
            assert capabilities['referencesProvider'] and capabilities['renameProvider']['prepareProvider']
            assert capabilities['inlayHintProvider']
            uri = (root / 'main.chl').as_uri()
            version = 0

            def open_text(text):
                """更新同一份未保存快照，确保查询不依赖磁盘内容。"""
                nonlocal version
                version += 1
                if version == 1:
                    server.send(dict(method='textDocument/didOpen', params=dict(textDocument=dict(uri=uri, text=text, version=version))))
                else:
                    server.send(dict(method='textDocument/didChange', params=dict(textDocument=dict(uri=uri, version=version), contentChanges=[dict(text=text)])))

            def query(method, text, offset, **extra):
                """使用 UTF-16 光标查询当前缓冲区。"""
                return server.query('textDocument/' + method, dict(textDocument=dict(uri=uri), position=position(text, offset), **extra))

            def rejected(method, text, offset, **extra):
                """重命名拒绝必须返回协议错误，不能给出部分编辑。"""
                serial = server.serial + 1
                server.send(dict(id=serial, method='textDocument/' + method, params=dict(textDocument=dict(uri=uri), position=position(text, offset), **extra)))
                response = server.response(serial)
                assert 'error' in response and response['error']['code'] == -32602, response

            text = '''struct Box<T> { T item; };
    void main() {
        List<int32> values;
        for(let item in values) { println("{}", item); }
        Box<int32> box;
        let number = box.item;
        let combine = (int64 fn(int64)) fn(int64 fn(int64) a, int64 fn(int64) b) {
            int64 fn copy(int64 x) { a(b(x)) }
        };
        let c3 = combine(fn(x) { x * 5 }, fn(x) { x - 1 });

    }
    '''
            open_text(text)
            for marker, expected in [('item);', 'int32 item'), ('number =', 'int32 number'),
                                     ('combine =', 'fn(int64 fn(int64), int64 fn(int64))'), ('c3 =', 'int64 fn(int64) c3')]:
                hover = query('hover', text, text.index(marker) + 1)
                assert hover and expected in hover['contents']['value'], (marker, hover)
            x = text.index('x * 5')
            assert 'int64 x' in query('hover', text, x)['contents']['value']
            definition = query('definition', text, text.index('item);'))
            if isinstance(definition, list): definition = definition[0]
            assert definition['range']['start'] == position(text, text.index('item in'))
            hints = server.query('textDocument/inlayHint', dict(textDocument=dict(uri=uri), range=dict(start=dict(line=0, character=0), end=position(text, len(text)))))
            labels = {(h['position']['line'], h['label']) for h in hints}
            assert (3, ': int32') in labels and (5, ': int32') in labels and (9, ': int64 fn(int64)') in labels, hints
            assert sum(h['label'] == ': int64' and h['position']['line'] == 9 for h in hints) == 2, hints
            detail = query('hover', text, text.index('combine ='))['contents']['value']
            assert '完整类型' in detail and '参数 1' in detail and '返回的函数' in detail, detail
            detail = query('hover', text, text.index('let combine'))['contents']['value']
            assert '完整类型' in detail and '返回的函数' in detail, detail
            completions = query('completion', text, text.index('item);'))
            assert any(c['label'] == 'item' and c['detail'] == 'int32 item' for c in completions), completions

            generic = '''T identity<T>(T value){return value;}
    int64 pick(int64 value){return value;}
    String pick(String value){return value;}
    void main(){int32 small=1; let first=identity<int32>(small); let second=identity(small); let selected=pick(1);}
    '''
            open_text(generic)
            for marker, expected in [('first=', 'int32 first'), ('second=', 'int32 second'), ('selected=', 'int64 selected')]:
                hover = query('hover', generic, generic.index(marker))
                assert expected in hover['contents']['value'], (marker, hover)
            refs = query('references', generic, generic.index('pick(int64'), context=dict(includeDeclaration=True))
            assert len(refs) == 2, refs
            detailed = 'void main(){let                                       values=List<List<int32>>(); let unknown=missing();}'
            open_text(detailed)
            detail = query('hover', detailed, detailed.index('let'))['contents']['value']
            assert 'List<List<int32>>' in detail and '泛型实参 1' in detail, detail
            detail = query('hover', detailed, detailed.index('unknown='))['contents']['value']
            assert '尚不能唯一确定类型' in detail, detail
            incomplete = 'struct Entry{int64 value;}; void main(){List<Entry> values;for(let item in values){item.'
            open_text(incomplete)
            completions = query('completion', incomplete, len(incomplete))
            assert any(item['label'] == 'value' for item in completions), completions

            macro = '''$macro example(...A)(...B)
    {
        $for var in A
        println("{}", var);
        $end_for
        $for var in B
        println("{}", var);
        $end_for
    }
    $end_macro
    void main() { $example(1,2,3,4,5,6)(3,2,4,1); }
    '''
            open_text(macro)
            help = query('signatureHelp', macro, macro.index('(3,2,4,1)') + 5)
            assert help['activeParameter'] == 1 and [g['count'] for g in help['chaliGroups']] == [6, 4], help
            assert '当前分组：B' in help['signatures'][0]['documentation']['value']
            signature = help['signatures'][0]
            begin, end = signature['parameters'][help['activeParameter']]['label']
            assert signature['label'][begin:end] == 'B', signature
            preview = server.query('chali/expandMacro', dict(textDocument=dict(uri=uri)))
            assert preview['text'] and not preview['diagnostics'] and preview['mappings'], preview
            partial = macro[:macro.index('(3,2,4,1)')] + '(3,2,'
            open_text(partial)
            help = query('signatureHelp', partial, len(partial))
            assert help['activeParameter'] == 1, help
            mismatch = macro.replace('(3,2,4,1)', '[3,2,4,1]')
            open_text(mismatch)
            help = query('signatureHelp', mismatch, mismatch.index('[3,2,4,1]') + 3)
            assert '期待 ()' in help['signatures'][0]['documentation']['value'], help
            preview = server.query('chali/expandMacro', dict(textDocument=dict(uri=uri)))
            assert any('参数组' in message for message in preview['diagnostics']), preview
            hygienic = """$macro make()
    {
        int64 temporary = 1;
        int64 next = temporary;
        yield next;
    }
    $end_macro
    void main(){int64 value=$make();}
    """
            open_text(hygienic)
            preview = server.query('chali/expandMacro', dict(textDocument=dict(uri=uri)))
            assert not preview['diagnostics'], preview
            assert preview['format'] == 'json', preview
            assert 'temporary' in preview['text'] and 'bindingIdentity' not in preview['text'], preview
            assert 'temporaries' not in preview, preview

            text = '''int64 inc(int64 x) { return x + 1; }
    void main() {
        // 😀 x inc "x"
        int64 x = 1;
        { int64 x = 2; println("{}", x); }
        println("{}", x);
        let callback = int64 fn(int64 x) { x + 1 };
        let __boom = inc(x);
    }
    '''
            open_text(text)
            at = text.index('x = 1')
            refs = query('references', text, at, context=dict(includeDeclaration=True))
            assert len(refs) == 3 and {r['range']['start']['line'] for r in refs} == {3, 5, 7}, refs
            assert query('prepareRename', text, at)['placeholder'] == 'x'
            edit = query('rename', text, at, newName='input')
            assert len(edit['documentChanges']) == 1 and edit['documentChanges'][0]['textDocument']['version'] == version
            assert len(edit['documentChanges'][0]['edits']) == 3, edit
            rejected('rename', text, at, newName='return')
            rejected('rename', text, at, newName='inc')
            duplicate = 'void main(){int64 source=1; println("{}",source); int64 later=2;}'
            open_text(duplicate)
            rejected('rename', duplicate, duplicate.index('source'), newName='later')
            loops = 'void main(){{\n    int64 index=0;\n    while (index<2) {\nprintln("{}",index);\n        index++;\n    }\n} int64 index=9; let __boom = index;}'
            open_text(loops)
            refs = query('references', loops, loops.index('index=0'), context=dict(includeDeclaration=True))
            assert len(refs) == 4, refs
            refs = query('references', loops, loops.index('index=9'), context=dict(includeDeclaration=True))
            assert len(refs) == 2, refs
            prototype = 'int64 declared(int64 value); void main(){int64 value=3; let __boom = value;}'
            open_text(prototype)
            refs = query('references', prototype, prototype.index('value);'), context=dict(includeDeclaration=True))
            assert len(refs) == 1, refs

            fields = 'struct Counter{int64 value; int64 read(){return value;}}; void main(){Counter c; c.value=1; Counter d; d.value=2; int64 value=3; let __boom = c.read()+value;}'
            open_text(fields)
            refs = query('references', fields, fields.index('value;'), context=dict(includeDeclaration=True))
            assert len(refs) == 4, refs
            edit = query('rename', fields, fields.index('value;'), newName='count')
            assert len(edit['documentChanges'][0]['edits']) == 4, edit
            refs = query('references', fields, fields.index('Counter'), context=dict(includeDeclaration=True))
            assert len(refs) == 3, refs
            refs = query('references', fields, fields.index('read()'), context=dict(includeDeclaration=True))
            assert len(refs) == 2, refs
            captures = 'void main(){String label="😀";int64 number=1;let callback=int64 fn(){number+1};let __boom = callback()+number;}'
            open_text(captures)
            refs = query('references', captures, captures.index('number=1'), context=dict(includeDeclaration=True))
            assert len(refs) == 3 and refs[0]['range']['start'] == position(captures, captures.index('number=1')), refs
            opaque = '$macro use(x)\nprintln("{}",number);\n$end_macro\nvoid main(){int64 number=1;$use(number);return;}'
            open_text(opaque)
            rejected('rename', opaque, opaque.index('number=1'), newName='input')
            opaque_macro = '$macro first(x)\nx\n$end_macro\n$macro second(x)\n$first(x)\n$end_macro\nvoid main(){int64 value=$second(1);return;}'
            open_text(opaque_macro)
            rejected('rename', opaque_macro, opaque_macro.index('first(x)'), newName='renamed')
            simple_macro = '$macro only(x)\nx\n$end_macro\nvoid main(){int64 value=$only(1);return;}'
            open_text(simple_macro)
            edit = query('rename', simple_macro, simple_macro.index('only(x)'), newName='renamed')
            assert len(edit['documentChanges'][0]['edits']) == 2, edit

            module = root / 'alpha.chl'
            module.write_text('module alpha; int64 compute(int64 value) { return value; }', encoding='utf-8')
            text = 'import alpha; void main(){ let __boom = alpha::compute(1); }'
            open_text(text)
            refs = query('references', text, text.index('compute'), context=dict(includeDeclaration=True))
            assert len(refs) == 2 and {r['uri'] for r in refs} == {uri, module.as_uri()}, refs
            edit = query('rename', text, text.index('compute'), newName='calculate')
            assert len(edit['documentChanges']) == 2, edit
            assert any(c['textDocument']['uri'] == module.as_uri() and c['textDocument']['version'] is None for c in edit['documentChanges'])
            unsaved = 'module alpha;\n// 未保存修改\nint64 compute(int64 value){return value+1;}'
            server.send(dict(method='textDocument/didOpen', params=dict(textDocument=dict(uri=module.as_uri(), text=unsaved, version=7))))
            edit = query('rename', text, text.index('compute'), newName='calculate')
            assert any(c['textDocument']['uri'] == module.as_uri() and c['textDocument']['version'] == 7 and c['edits'][0]['range']['start']['line'] == 2 for c in edit['documentChanges']), edit
            assert '未保存修改' not in module.read_text(encoding='utf-8')

            text = '''int64 pick(int64 value){return value;}
    String pick(String value){return value;}
    void main(){let callback = pick;}
    '''
            open_text(text)
            rejected('rename', text, text.rindex('pick'), newName='choose')

            text = '''void main() {
        String value = "owned";
        String fn() closure = fn move() { value };
        println("{}", value);

    }
    '''
            open_text(text)
            diagnostic = server.diagnostic(uri, version, lambda ds: any(d.get('code') == 'CHL1205' for d in ds))
            moved = next(d for d in diagnostic['diagnostics'] if d.get('code') == 'CHL1205')
            assert {r['location']['range']['start']['line'] for r in moved['relatedInformation']} == {1, 2}, moved
            text = '''String build() {
        return {
            String value = "owned";
            defer { println("{}", value); }
            yield (value);
        };
    }
    void main(){}
    '''
            open_text(text)
            diagnostic = server.diagnostic(uri, version, lambda ds: any('延迟清理仍借用' in d['message'] for d in ds))
            conflict = next(d for d in diagnostic['diagnostics'] if '延迟清理仍借用' in d['message'])
            assert any('defer' in item['message'] for item in conflict.get('relatedInformation', [])), conflict
            large = 'void main(){int64 target=1;\n' + 'target = target + 1;\n' * 1000 + '}'
            open_text(large)
            started = time.monotonic()
            refs = query('references', large, large.index('target'), context=dict(includeDeclaration=True))
            elapsed = time.monotonic() - started
            assert len(refs) == 2001 and elapsed < 5, (len(refs), elapsed)
            assert not (root / 'main.chl').exists()
            server.close()
        finally:
            server._kill()
    print('原生 LSP 的共享类型推断、分组宏签名、原始映射、引用、版本化安全重命名及关联诊断通过。')


if __name__ == '__main__':
    main()
