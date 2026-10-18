"""P1/P2 编译器与开发工具集成回归：真实编译、锁文件失效和 stdio LSP 协议。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'build/work/magel' / ('magel.exe' if os.name == 'nt' else 'magel')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=DEFAULT)
    args = parser.parse_args()
    compiler = args.magel.resolve()
    os.environ.setdefault('MAGEL_PYTHON', sys.executable)
    work = ROOT / 'build/magel-development'
    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='suite-', dir=work) as temporary:
        directory = Path(temporary)
        def run(*arguments, expected=0, cwd=directory, **kwargs):
            result = subprocess.run([str(compiler), *map(str, arguments)], cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180, **kwargs)
            assert result.returncode == expected, (arguments, result.returncode, result.stdout, result.stderr)
            return result
        source = directory / 'check.mgl'
        source.write_text('int64 main() { while(true) {} return 0; }\n', encoding='utf-8')
        report = json.loads(run('--check', '--diagnostics=json', source, '--out', directory/'absent').stdout)
        assert report == {'schemaVersion': 1, 'diagnostics': []}
        assert not list(directory.glob('absent*')), '检查模式生成了产物'
        run('aot', '--check', source)
        source.write_text('int64 main() {\n const int64 n=3; n=4; return 0;\n}\n', encoding='utf-8')
        error = json.loads(run('--diagnostics=json', source, expected=1).stdout)['diagnostics'][0]
        assert error['code'] == 'MGL1203' and Path(error['span']['file']).resolve() == source.resolve()
        source.write_text('int64 main() { int64 x = ; return 0; }\n', encoding='utf-8')
        syntax = json.loads(run('--diagnostics=json', source, expected=1).stdout)['diagnostics'][0]
        assert syntax['code'] == 'MGL1001' and syntax['span']['column'] > 1
        for body in (
            'int64 main(){int64 a[(int64)(false && 3)+1];return 0;}',
            'int64 main(){int64 a[(int64)(false < true)+1];return 0;}',
            'const int64 n=1; int64 main(){int64 a[(int64)(false && (bool)&n)+1];return 0;}',
            'bool side(){return true;} int64 main(){int64 a[(int64)(false && side())+1];return 0;}',
            'int64 main(){const int64 n=3;{const int64 n=n;int64 a[n];}return 0;}',
        ):
            source.write_text(body,encoding='utf-8');run('--check',source,expected=1)
        for name in ('constant_expressions', 'destructuring_guards', 'generic_inherited_constraint'):
            file = ROOT / f'example/samples/{name}.mgl'
            result = run('run', file, '--out', directory/name)
            expected = (ROOT / f'example/expected/{name}.txt').read_text(encoding='utf-8')
            assert result.stdout == expected and not result.stderr, (name, result.stdout, result.stderr)
        source.write_text('int64 main(){std::lang::println("{} {}",1);return 0;}', encoding='utf-8')
        formatting_error=json.loads(run('--diagnostics=json',source,expected=1).stdout)['diagnostics'][0]
        assert formatting_error['code']=='MGL1400',formatting_error
        cli_project=directory/'cli-project';cli_project.mkdir()
        (cli_project/'magel.toml').write_text('[magel]\nrun="svmb"\n',encoding='utf-8')
        cli_source=cli_project/'main.mgl';cli_source.write_text('int64 main(){std::lang::println("executed");return 0;}',encoding='utf-8')
        cli_prefix=directory/'cli-arguments'
        result=run('--out',cli_prefix,cli_source,'--','--check')
        assert result.stdout=='' and cli_prefix.with_suffix('.svmb').exists(),result
        print('[通过] 检查模式、JSON 诊断、格式检查、程序参数边界与三个功能样例', flush=True)

        # 格式化保留字符串、注释、预处理行，写回后幂等。
        source.write_text('int64 main() {\nlang::println("{}", "{ literal }"); // } comment\nif (true) {\nreturn 0;\n}\nreturn 1;\n}\n', encoding='utf-8')
        run('fmt', source, '--check', expected=1)
        run('fmt', source, '--write')
        run('fmt', source, '--check')
        formatted = source.read_text(encoding='utf-8')
        assert '    std::lang::println("{}", "{ literal }"); // } comment' in formatted
        assert '        return 0;' in formatted
        run('run', source, '--out', directory/'formatted')

        # 测试发现与跳过必须实际执行文件，失败返回非零。
        tests = directory / 'tests'
        tests.mkdir()
        (tests/'test_pass.mgl').write_text('int64 main(){ std::lang::println("ok"); return 0; }', encoding='utf-8')
        (tests/'test_pass.expected').write_text('ok\n', encoding='utf-8')
        (tests/'skip_test.mgl').write_text('invalid code', encoding='utf-8')
        (tests/'skip_test.test.toml').write_text('skip = "fixture"\n', encoding='utf-8')
        summary = json.loads(run('test', tests, '--json').stdout)['summary']
        assert summary == {'passed': 1, 'failed': 0, 'skipped': 1}, summary
        summary = json.loads(run('test', tests, '--json', '--filter', 'test_pass').stdout)['summary']
        assert summary == {'passed': 1, 'failed': 0, 'skipped': 0}
        (tests/'test_fail.mgl').write_text('int64 main(){ std::lang::assert(false); return 0; }', encoding='utf-8')
        summary = json.loads(run('test', tests, '--json', expected=1).stdout)['summary']
        assert summary['failed'] == 1
        (tests/'timeout_test.mgl').write_text('int64 main(){while(true){}return 0;}',encoding='utf-8')
        summary=json.loads(run('test',tests,'--filter','timeout_test','--timeout','0.2','--json',expected=1).stdout)
        assert summary['summary']['failed']==1 and 'timed out' in summary['tests'][0]['message'],summary
        print('[通过] 格式化幂等、测试发现、过滤、跳过、超时与失败退出', flush=True)

        # 真实编译器通过锁文件提供依赖模块路径，并拒绝被修改的来源。
        library = directory/'lib'
        app = directory/'app'
        library.mkdir(); app.mkdir()
        (library/'magel.toml').write_text('[package]\nname="lib"\nversion="0.1.0"\n', encoding='utf-8')
        (library/'values.mgl').write_text('module values;\nint64 answer() { return 42; }\n', encoding='utf-8')
        (app/'magel.toml').write_text('[package]\nname="app"\nversion="0.1.0"\ncompiler=">=0.7.0"\n[dependencies]\nvalues="path:../lib"\n', encoding='utf-8')
        app_source = app/'main.mgl'
        app_source.write_text('import values;\nint64 main(){std::lang::assert(values::answer()==42);return 0;}\n', encoding='utf-8')
        run('--check', app_source, expected=1)
        run('deps', 'lock', '--offline', '--project', app)
        lock = (app/'magel.lock').read_bytes()
        run('deps', 'lock', '--offline', '--project', app)
        assert (app/'magel.lock').read_bytes() == lock, '锁文件不确定'
        run('deps', 'check', '--offline', '--project', app)
        run('--check', app_source)
        (library/'values.mgl').write_text('module values;\nint64 answer() { return 43; }\n', encoding='utf-8')
        run('deps', 'check', '--offline', '--project', app, expected=1)
        error = json.loads(run('--diagnostics=json', app_source, expected=1).stdout)['diagnostics'][0]
        assert error['code'] == 'MGL1101' and 'dependency changed' in error['message']
        run('deps','lock','--offline','--project',app)
        cache_prefix=directory/'cached'
        run('run', app_source,'--emit','--incremental','--out',cache_prefix)
        cached=run('run', app_source,'--emit','--incremental','--out',cache_prefix)
        assert '增量缓存命中' in cached.stderr,cached.stderr
        (library/'values.mgl').write_text('module values;\nint64 answer() { return 42; }\n',encoding='utf-8')
        run('deps','lock','--offline','--project',app)
        rebuilt=run('run', app_source,'--emit','--incremental','--out',cache_prefix)
        assert '增量缓存命中' not in rebuilt.stderr,rebuilt.stderr
        print('[通过] 离线路径依赖、锁定确定性、内容失效与增量重建', flush=True)

        # Git 用本地仓库作真实夹具，不需要网络服务。
        git_library=directory/'git-library'
        git_library.mkdir()
        (git_library/'magel.toml').write_text('[package]\nname="gitlib"\nversion="0.1.0"\n',encoding='utf-8')
        (git_library/'gitvalue.mgl').write_text('module gitvalue;\nint64 answer(){return 7;}\n',encoding='utf-8')
        def git(*items):
            result=subprocess.run(['git','-C',str(git_library),*items],capture_output=True,text=True,encoding='utf-8',errors='replace')
            assert result.returncode==0,(items,result.stderr)
            return result.stdout.strip()
        git('init');git('add','.')
        git('-c','user.name=MC测试','-c','user.email=magel-tests@example.invalid','commit','-m','创建依赖测试夹具')
        commit=git('rev-parse','HEAD')
        git_app=directory/'git-app';git_app.mkdir()
        (git_app/'magel.toml').write_text('[package]\nname="gitapp"\nversion="0.1.0"\n[dependencies]\ngitlib="git+'+git_library.as_uri()+'#'+commit+'"\n',encoding='utf-8')
        git_source=git_app/'main.mgl';git_source.write_text('import gitvalue;\nint64 main(){return 0;}\n',encoding='utf-8')
        run('deps','lock','--offline','--project',git_app,expected=1)
        run('deps','lock','--project',git_app)
        run('deps','check','--offline','--project',git_app)
        run('--check',git_source)
        print('[通过] 固定 Git 提交、本地下载及离线缓存',flush=True)

        # LSP 使用未保存内容进行真实前端检查，不修改源文件。
        source.write_text('int64 main(){ return 0; }\n', encoding='utf-8')
        uri = source.as_uri()
        messages = [
            {'id': 1, 'method': 'initialize', 'params': {}},
            {'method': 'textDocument/didOpen', 'params': {'textDocument': {'uri': uri, 'version': 1, 'languageId': 'magel', 'text': 'int64 main() {\n int64 value=3;\n return value;\n}\n'}}},
            {'id': 2, 'method': 'textDocument/completion', 'params': {'textDocument': {'uri': uri}, 'position': {'line': 2, 'character': 8}}},
            {'id': 3, 'method': 'textDocument/definition', 'params': {'textDocument': {'uri': uri}, 'position': {'line': 2, 'character': 9}}},
            {'id': 5, 'method': 'textDocument/hover', 'params': {'textDocument': {'uri': uri}, 'position': {'line': 2, 'character': 9}}},
            {'id': 6, 'method': 'textDocument/formatting', 'params': {'textDocument': {'uri': uri}, 'options': {'tabSize': 4, 'insertSpaces': True}}},
            {'id': 7, 'method': 'textDocument/documentSymbol', 'params': {'textDocument': {'uri': uri}}},
            {'method': 'textDocument/didChange', 'params': {'textDocument': {'uri': uri, 'version': 2}, 'contentChanges': [{'text': 'int64 main(){ int64 value = ; return 0; }'}]}},
            {'id': 4, 'method': 'shutdown', 'params': {}},
            {'method': 'exit', 'params': {}},
        ]
        payload = b''
        for message in messages:
            body = json.dumps({'jsonrpc': '2.0', **message}).encode('utf-8')
            payload += f'Content-Length: {len(body)}\r\n\r\n'.encode('ascii') + body
        result = subprocess.run([str(compiler), 'lsp'], input=payload, capture_output=True, cwd=directory, timeout=180)
        assert result.returncode == 0, result.stderr.decode('utf-8', errors='replace')
        responses = []
        output = result.stdout
        while output:
            header, output = output.split(b'\r\n\r\n', 1)
            size = int(header.split(b':', 1)[1])
            responses.append(json.loads(output[:size])); output = output[size:]
        replies = {r['id']: r['result'] for r in responses if 'id' in r}
        assert any(item['label']=='value' for item in replies[2])
        assert replies[3]['range']['start']['line'] == 1, replies[3]
        assert 'int64 value=3;' in replies[5]['contents']['value']
        assert '    int64 value=3;' in replies[6][0]['newText']
        assert [s['name'] for s in replies[7]]==['main','value']
        diagnostics = [r['params'] for r in responses if r.get('method')=='textDocument/publishDiagnostics']
        assert diagnostics[0]['diagnostics'] == []
        assert diagnostics[-1]['version'] == 2 and diagnostics[-1]['diagnostics'][0]['code'] == 'MGL1001'
        assert source.read_text(encoding='utf-8') == 'int64 main(){ return 0; }\n'
        print('[通过] LSP 初始化、未保存诊断、补全、定义与关闭协议', flush=True)
    print('P1/P2 开发工具集成回归全部通过')


if __name__ == '__main__':
    main()
