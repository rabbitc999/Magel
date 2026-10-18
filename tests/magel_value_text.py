"""验证默认值文本、字符串转换互通、嵌套边界与清理，不依赖反射。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
from native_lsp_transport import Transport, position

ROOT = Path(__file__).resolve().parents[1]


def main():
    """在 VM 和默认 checked AOT 中核对同一份值文本程序。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    source = ROOT / 'example/samples/value_text.mgl'
    expected = (ROOT / 'example/expected/value_text.txt').read_text(encoding='utf-8')
    with tempfile.TemporaryDirectory(prefix='magel-value-text-') as folder:
        work = Path(folder)
        for backend in ('run', 'aot'):
            output = work / (backend + ('.exe' if os.name == 'nt' else ''))
            result = subprocess.run([str(compiler), backend, str(source), '--no-reflection', '--out', str(output)],
                                    cwd=ROOT, env=dict(os.environ, MAGEL_LANG='zh'), capture_output=True,
                                    encoding='utf-8', timeout=300)
            assert result.returncode == 0, (backend, result.stdout, result.stderr)
            if backend == 'aot':
                result = subprocess.run([str(output)], capture_output=True, encoding='utf-8', timeout=60)
            assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), (backend, result)
        negative = {
            'invalid_signature': ('struct Bad { int64 toString(){return 1;} }; int64 main(){return 0;}', 'toString()'),
            'static_method': ('struct Bad { static String toString(){return "bad";} }; int64 main(){return 0;}', 'toString()'),
            'interface_shadow': ('interface ToString { int64 required(); }; String text<T:ToString>(T value){return "bad";} int64 main(){text(42);return 0;}', 'ToString'),
        }
        for name, (code, diagnostic) in negative.items():
            file = work / (name + '.mgl')
            file.write_text(code, encoding='utf-8')
            result = subprocess.run([str(compiler), 'check', str(file)], cwd=ROOT,
                                    env=dict(os.environ, MAGEL_LANG='zh'), capture_output=True,
                                    encoding='utf-8', timeout=60)
            assert result.returncode != 0 and diagnostic in result.stdout + result.stderr, (name, result)
        for name, code, diagnostic in (
            ('mutable_nested', '''struct Mutable { int64 value; String toString(){self.value=self.value+1;return "bad";} };
int64 main(){Mutable value;List<Mutable> values={value};println("{}",values.toString());return 0;}''', 'toString 必须只读'),
            ('callback_failure', '''struct Failing { String toString(){panic("value-text-callback");return "bad";} };
int64 main(){Failing value;List<Failing> values={value};println("{}",values.toString());return 0;}''', 'value-text-callback'),
        ):
            file = work / (name + '.mgl')
            file.write_text(code, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(file), '--no-reflection', '--out', str(work / name)],
                                    cwd=ROOT, env=dict(os.environ, MAGEL_LANG='zh'), capture_output=True,
                                    encoding='utf-8', timeout=60)
            assert result.returncode != 0 and diagnostic in result.stdout + result.stderr, (name, result)
        server = Transport(compiler, work)
        try:
            server.query('initialize', dict(rootUri=work.as_uri()))
            text = 'int64 main(){int64 value=42;auto text=value.toString();value.;return 0;}'
            uri = (work / 'editor.mgl').as_uri()
            server.send(dict(method='textDocument/didOpen', params=dict(textDocument=dict(uri=uri, text=text, version=1))))
            hover = server.query('textDocument/hover', dict(textDocument=dict(uri=uri), position=position(text, text.index('text='))))
            assert hover and 'String text' in hover['contents']['value'], hover
            completions = server.query('textDocument/completion', dict(textDocument=dict(uri=uri), position=position(text, text.index('value.;') + len('value.'))))
            assert any(item['label'] == 'toString' and 'String toString()' in item['detail'] for item in completions), completions
        finally:
            server.close()
    print('默认值文本、双向字符串糖、嵌套结构与无反射 VM/AOT 清理通过。')


if __name__ == '__main__':
    main()
