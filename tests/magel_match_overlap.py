"""验证整数 match 模式重叠警告、边界精度及 VM/AOT 分支选择。"""
import argparse
import json
import os
import queue
import threading
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """检查区间、单值、开闭端点、守卫和完整 64 位整数诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    compiler = parser.parse_args().magel.resolve()
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')
    cases = (
        ('ranges', 'int64 x=17;', '[15,25]=>{} [10,20]=>{}', 1),
        ('ranges_reverse', 'int64 x=17;', '[10,20]=>{} [15,25]=>{}', 1),
        ('single_first', 'int64 x=15;', '15=>{} [15,20]=>{}', 1),
        ('range_first', 'int64 x=15;', '[15,20]=>{} 15=>{}', 1),
        ('closed_touch', 'int64 x=20;', '[10,20]=>{} [20,30]=>{}', 1),
        ('open_touch', 'int64 x=20;', '[10,20)=>{} [20,30]=>{}', 0),
        ('open_single', 'int64 x=20;', '[10,20)=>{} 20=>{}', 0),
        ('empty', 'int64 x=15;', '[15,15)=>{} 15=>{}', 0),
        ('point_range', 'int64 x=15;', '[15,15]=>{} 15=>{}', 1),
        ('reversed', 'int64 x=15;', '[20,10]=>{} [15,25]=>{}', 0),
        ('negative', 'int64 x=-10;', '[-20,-10]=>{} [-15,-5]=>{}', 1),
        ('min', 'int64 x=(-9223372036854775807-1);', '[(-9223372036854775807-1),-9223372036854775807]=>{} [-9223372036854775807,0]=>{}', 1),
        ('max', 'int64 x=9223372036854775807;', '[9223372036854775806,9223372036854775807]=>{} [9223372036854775807,9223372036854775807]=>{}', 1),
        ('uint64', 'uint64 x=18446744073709551615u;', '[18446744073709551614u,18446744073709551615u]=>{} [18446744073709551615u,18446744073709551615u]=>{}', 1),
        ('uint64_open', 'uint64 x=18446744073709551615u;', '[18446744073709551614u,18446744073709551615u)=>{} [18446744073709551615u,18446744073709551615u]=>{}', 0),
        ('guard', 'int64 x=15;', '[10,20] if(false)=>{} [15,25]=>{}', 1),
        ('constant_expression', 'int64 x=10;', '[1+2,5*3]=>{} [10,20]=>{}', 1),
        ('dynamic', 'int64 x=15,lo=10;', '[lo,20]=>{} [15,25]=>{}', 0),
        ('float', 'float64 x=15.0;', '[10.0,20.0]=>{} [15.0,25.0]=>{}', 0),
    )
    with tempfile.TemporaryDirectory(prefix='magel-match-overlap-') as folder:
        work = Path(folder)

        def invoke(*args):
            """执行编译器或目标程序并保留诊断。"""
            return subprocess.run(list(map(str, args)), cwd=ROOT, env=env, capture_output=True,
                                  encoding='utf-8', errors='replace', timeout=240)

        for name, declaration, patterns, count in cases:
            source = work / (name + '.mgl')
            source.write_text('int64 main(){\n' + declaration + '\nmatch(x){\n' + patterns.replace('} ', '}\n') + '\n_=>{}\n}return 0;}\n', encoding='utf-8')
            result = invoke(compiler, 'check', source, 'json')
            assert result.returncode == 0, (name, result)
            diagnostics = json.loads(result.stdout)['diagnostics']
            assert len(diagnostics) == count, (name, diagnostics)
            for diagnostic in diagnostics:
                assert diagnostic['severity'] == 'warning' and diagnostic['code'] == 'MGL1601', diagnostic
                assert Path(diagnostic['span']['file']).resolve() == source.resolve(), diagnostic
                assert diagnostic['span']['line'] == 5, (name, diagnostic)
            print('[通过] ' + name, flush=True)

        source = work / 'execution.mgl'
        source.write_text('int64 main(){int64 hits=0;match(17){[15,25]=>{hits=1;} [10,20]=>{hits=2;} _=>{}}assert(hits==1);println("match overlap OK");return 0;}', encoding='utf-8')
        for backend in ('run', 'aot'):
            output = work / (backend + '.exe')
            result = invoke(compiler, backend, source, '--out', output)
            assert result.returncode == 0 and 'MGL1601' in result.stderr, result
            if backend == 'aot': result = invoke(output)
            assert result.returncode == 0 and result.stdout == 'match overlap OK\n', result
        source.write_text('int64 main(){match(1){[0,2]=>{} 1=>{missing();} _=>{}}return 0;}', encoding='utf-8')
        result = invoke(compiler, 'check', source, 'json')
        diagnostics = json.loads(result.stdout)['diagnostics']
        assert result.returncode != 0 and {d['severity'] for d in diagnostics} == {'warning', 'error'}, result
        # 编辑器必须保留 warning，不能把成功检查中的警告升级为 error。
        source.write_text('int64 main(){match(17){[15,25]=>{} [10,20]=>{} _=>{}}return 0;}', encoding='utf-8')
        process = subprocess.Popen([str(compiler), 'lsp'], cwd=work, env=env,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        messages = queue.Queue()

        def receive():
            """后台读取 LSP 帧，通过队列提供有界等待。"""
            try:
                while True:
                    size = None
                    while True:
                        line = process.stdout.readline()
                        if not line: return
                        if line in (b'\r\n', b'\n'): break
                        if line.lower().startswith(b'content-length:'): size = int(line.split(b':', 1)[1])
                    if size is not None: messages.put(json.loads(process.stdout.read(size)))
            except (OSError, ValueError):
                return

        def send(message):
            """按 LSP Content-Length 协议发送请求或通知。"""
            body = json.dumps(message).encode('utf-8')
            process.stdin.write(f'Content-Length: {len(body)}\r\n\r\n'.encode() + body)
            process.stdin.flush()

        threading.Thread(target=receive, daemon=True).start()
        try:
            send({'jsonrpc':'2.0', 'id':1, 'method':'initialize', 'params':{'processId':None, 'rootUri':work.as_uri(), 'capabilities':{}}})
            while messages.get(timeout=45).get('id') != 1: pass
            send({'jsonrpc':'2.0', 'method':'initialized', 'params':{}})
            send({'jsonrpc':'2.0', 'method':'textDocument/didOpen', 'params':{'textDocument':{'uri':source.as_uri(), 'languageId':'magel', 'version':1, 'text':source.read_text(encoding='utf-8')}}})
            while True:
                message = messages.get(timeout=45)
                if message.get('method') != 'textDocument/publishDiagnostics': continue
                warnings = [item for item in message['params']['diagnostics'] if item.get('code') == 'MGL1601']
                assert len(warnings) == 1 and warnings[0]['severity'] == 2, message
                break
        finally:
            process.terminate()
            process.communicate(timeout=15)
        print('match 重叠警告：静态边界、JSON 定位、错误并存、VM/AOT 顺序及 LSP 警告级别通过')


if __name__ == '__main__':
    main()
