"""使用真实原生 LSP 验证命名警告、限定作用域补全、标准库内部符号隐藏与标准库类型字段文档。"""
from pathlib import Path
import json
import subprocess
import tempfile
import sys
from native_lsp_transport import Transport

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from magel_lsp import naming_warnings


def main():
    """批量发送未保存文档，核对警告级别、直接成员过滤及悬停说明。"""
    requests = [dict(id=1, method='initialize', params={})]
    documents = {}
    sequence = 2

    def document(folder, name, text, method=None, marker=None):
        nonlocal sequence
        uri = (folder/name).as_uri()
        documents[name] = uri
        requests.append(dict(method='textDocument/didOpen', params=dict(
            textDocument=dict(uri=uri, text=text, version=1))))
        if not method:
            return None
        cursor = text.index(marker) + len(marker)
        before = text[:cursor]
        position = dict(line=before.count('\n'), character=len(before.rsplit('\n',1)[-1].encode('utf-16-le'))//2)
        current = sequence
        sequence += 1
        requests.append(dict(id=current, method='textDocument/'+method,
            params=dict(textDocument=dict(uri=uri), position=position)))
        return current

    with tempfile.TemporaryDirectory(prefix='magel-lsp-scopes-') as directory:
        folder = Path(directory)
        requests[0]['params'] = dict(rootUri=folder.as_uri())
        (folder/'a').mkdir()
        (folder/'a/iter').mkdir()
        (folder/'a/iter/child.mgl').write_text('module a::iter::child; int64 hidden(){return 1;}\n', encoding='utf-8')
        (folder/'a/iter.mgl').write_text('module a::iter;\nstruct LocalType { int64 field; int64 method(){return 1;} };\nint64 localFunction(){return 1;}\n', encoding='utf-8')
        bad = '/* 😀 */ struct bad_type { int64 Bad_Method(){return 1;} }; int64 Bad_Function(){return 0;} int64 main(){return 0;}'
        document(folder, 'bad.mgl', bad)
        good = 'struct GoodType { GoodType() {} int64 goodMethod(){return 1;} }; int64 main(){return 0;}'
        requests.append(dict(method='textDocument/didChange', params=dict(
            textDocument=dict(uri=documents['bad.mgl'], version=2), contentChanges=[dict(text=good)])))
        document(folder, 'good.mgl', good)
        imports = document(folder, 'imports.mgl', 'import std::', 'completion', 'std::')
        import_root = document(folder, 'import-root.mgl', 'import ', 'completion', 'import ')
        import_local = document(folder, 'import-local.mgl', 'import a::iter::', 'completion', 'a::iter::')
        std = document(folder, 'std.mgl', 'int64 main(){std::', 'completion', 'std::')
        # `__` 前缀是编译器/宿主协议：标准库模块补全一个都不该带出来。
        private = [document(folder, 'private-%d.mgl' % index,
                            'import %s;\nint64 main(){ %s::' % (module, module), 'completion', module + '::')
                   for index, module in enumerate(('std::lang', 'std::iter', 'std::math',
                                                   'std::numtheory', 'std::numerics', 'std::text', 'std::path'))]
        local = document(folder, 'local.mgl', 'import a::iter;\nint64 main(){a::iter::', 'completion', 'a::iter::')
        unrelated = document(folder, 'unrelated.mgl', 'import a::iter;\nint64 main(){b::iter::', 'completion', 'b::iter::')
        member = document(folder, 'member.mgl', 'import a::iter;\nint64 main(){a::iter::LocalType::', 'completion', 'LocalType::')
        field = document(folder, 'field.mgl', 'import std::datetime;\nint64 main(){DateTime date; date.year; return 0;}', 'hover', 'date.year')
        type_hover = document(folder, 'type.mgl', 'import std::linalg;\nint64 main(){LuFactorization value;return 0;}', 'hover', 'LuFactorization')
        requests += [dict(id=sequence, method='shutdown'), dict(method='exit')]
        server = Transport(ROOT/'magel.exe', folder)
        try:
            for request in requests[:-2]:
                server.send(request)
                method=request.get('method')
                item=request.get('params',{}).get('textDocument',{})
                if method=='textDocument/didOpen' and item.get('uri') in (documents['bad.mgl'],documents['good.mgl']):
                    server.diagnostic(item['uri'],item['version'])
                if method=='textDocument/didChange' and item.get('uri')==documents['bad.mgl']:
                    server.diagnostic(item['uri'],item['version'])
            responses={}
            for request in requests[:-2]:
                if 'id' in request:
                    response=server.response(request['id']);assert 'error' not in response,response
                    responses[request['id']]=response['result']
            diagnostics={}
            for message in server.notifications:
                if message.get('method')=='textDocument/publishDiagnostics':
                    diagnostics.setdefault(message['params']['uri'],[]).append(message['params']['diagnostics'])
        finally:server.close()
        warnings = diagnostics[documents['bad.mgl']][0]
        assert len(warnings) == 3 and all(d['severity']==2 for d in warnings), warnings
        assert {d['code'] for d in warnings} == {'MGL2101','MGL2102'}
        assert warnings == naming_warnings(bad), (warnings, naming_warnings(bad))
        assert diagnostics[documents['bad.mgl']][-1] == [], diagnostics[documents['bad.mgl']]
        assert diagnostics[documents['good.mgl']][-1] == [], diagnostics[documents['good.mgl']]
        labels = lambda key: {r['label'] for r in responses[key]}
        assert all(r['kind']==9 for r in responses[imports]+responses[import_root]+responses[import_local])
        assert {'std','a'} <= labels(import_root), responses[import_root]
        assert labels(import_local)=={'child'}, responses[import_local]
        assert 'lang' not in labels(imports) and 'iter' in labels(imports), responses[imports]
        assert 'lang' in labels(std) and 'iter' in labels(std) and 'pause' not in labels(std)
        for index, request in enumerate(private):
            module = ('std::lang', 'std::iter', 'std::math', 'std::numtheory', 'std::numerics', 'std::text', 'std::path')[index]
            leaked = sorted(label for label in labels(request) if label.startswith('__'))
            assert not leaked, (module, leaked)
        assert 'range' in labels(private[0]) and 'abs' in labels(private[2]) and 'gcd' in labels(private[3])
        assert labels(local) == {'LocalType','localFunction','child'}, responses[local]
        assert labels(unrelated) == set(), responses[unrelated]
        assert labels(member) == {'field','method'}, responses[member]
        assert '公历年份' in responses[field]['contents']['value'], responses[field]
        assert 'LU 分解结果' in responses[type_hover]['contents']['value'], responses[type_hover]
    print('LSP 命名警告、构造器豁免、模块及类型作用域、类型与字段文档、标准库内部符号隐藏全部通过。')


if __name__ == '__main__':
    main()
