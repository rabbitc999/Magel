"""检查提示并行、增量同步、依赖失效、缓存恢复和自动清理。"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import time
from native_lsp_transport import Transport, position

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')


def main():
    """在隔离的程序目录运行真实服务，验证缓存不会写到工程或系统临时目录。"""
    with tempfile.TemporaryDirectory(prefix='magel-lsp-performance-') as directory:
        work = Path(directory)
        binary = work / 'compiler directory' / COMPILER.name
        binary.parent.mkdir()
        shutil.copy2(COMPILER, binary)
        cache = binary.parent / 'build/lsp'
        cache.mkdir(parents=True)
        old = cache / 'expired.json'; old.write_text('{}')
        timestamp = time.time() - 14 * 86400
        os.utime(old, (timestamp, timestamp))
        preserved = binary.parent / 'build/user-file.txt'; preserved.write_text('preserve')
        # 超额清理只处理缓存目录；三个较旧的大文件应淘汰到 192 MiB 以下。
        quota = []
        for index in range(3):
            file = cache / f'quota-{index}.bin'
            with file.open('wb') as output: output.truncate(90 * 1024 * 1024)
            os.utime(file, (time.time()-86400-index, time.time()-86400-index))
            quota.append(file)
        project = work / 'project'; project.mkdir()
        dependency = project / 'dep.mgl'
        dependency.write_text('module dep;\nint64 read(){return 1;}', encoding='utf-8')
        path = project / 'main.mgl'; uri = path.as_uri()
        text = 'import dep;\nint64 main(){String emoji="😀";int64 value=dep::read();return 0;}\n'
        path.write_text(text, encoding='utf-8')
        server = Transport(binary, project)
        server.query('initialize', dict(rootUri=project.as_uri()))
        server.send(dict(method='textDocument/didOpen', params={'textDocument':dict(uri=uri,text=text,version=1)}))
        cursor = position(text, text.index('value') + 2)
        started = time.monotonic()
        hover = server.query('textDocument/hover', dict(textDocument=dict(uri=uri),position=cursor))
        latency = time.monotonic() - started
        assert 'int64 value' in hover['contents']['value'], hover
        assert not any(n.get('method') == 'textDocument/publishDiagnostics' for n in server.notifications)
        server.diagnostic(uri, 1, lambda values: not values)
        stats = server.query('magel/lspStats')
        assert Path(stats['cacheDirectory']) == cache and stats['removed'] >= 1, stats
        assert not old.exists() and preserved.read_text() == 'preserve'
        assert sum(f.stat().st_size for f in quota if f.exists()) <= 192 * 1024 * 1024
        assert not (project / 'build/lsp').exists()
        scanned = stats['scans']
        for _ in range(4):
            server.query('textDocument/completion', dict(textDocument=dict(uri=uri),position=cursor))
            server.query('textDocument/semanticTokens/full', dict(textDocument=dict(uri=uri)))
        assert server.query('magel/lspStats')['scans'] == scanned
        before = len(server.notifications)
        server.send(dict(method='textDocument/didSave', params={'textDocument':dict(uri=uri)}))
        server.diagnostic(uri, 1, after=before)
        reused = server.query('magel/lspStats')
        assert reused['checks'] == stats['checks'] and reused['diagnosticHits'] > stats['diagnosticHits'], reused
        # UTF-16 范围在 emoji 后面；无需发送整份文档也能得到新类型。
        begin = text.index('int64 value'); end = text.index(';return')
        replacement = 'bool value=true'
        server.send(dict(method='textDocument/didChange',params=dict(textDocument=dict(uri=uri,version=2),
            contentChanges=[dict(range=dict(start=position(text,begin),end=position(text,end)),text=replacement)])))
        text = text[:begin] + replacement + text[end:]
        cursor = position(text, text.index('value') + 2)
        hover = server.query('textDocument/hover', dict(textDocument=dict(uri=uri),position=cursor))
        assert 'bool value' in hover['contents']['value'], hover
        server.diagnostic(uri, 2, lambda values: not values)
        # 未保存依赖改变返回类型，未改动的入口也必须重新诊断。
        text = 'import dep;\nint64 main(){int64 value=dep::read();return 0;}\n'
        server.send(dict(method='textDocument/didChange',params=dict(textDocument=dict(uri=uri,version=3),contentChanges=[dict(text=text)])))
        server.diagnostic(uri, 3, lambda values: not values)
        before = len(server.notifications)
        server.send(dict(method='textDocument/didOpen',params={'textDocument':dict(uri=dependency.as_uri(),
            text='module dep;\nString read(){return "changed";}',version=1)}))
        changed = server.diagnostic(uri, 3, lambda values: any(d['severity']==1 for d in values), after=before)
        assert any('String' in d['message'] for d in changed['diagnostics']), changed
        before = len(server.notifications)
        server.send(dict(method='textDocument/didClose',params={'textDocument':dict(uri=dependency.as_uri())}))
        server.diagnostic(uri, 3, lambda values: not values, after=before)
        # 新版本合并；关闭后的诊断不允许回写。
        for version in range(4, 24):
            server.send(dict(method='textDocument/didChange',params=dict(textDocument=dict(uri=uri,version=version),contentChanges=[dict(text=text)])))
        server.diagnostic(uri, 23, lambda values: not values)
        assert path.read_text(encoding='utf-8').startswith('import dep;\nint64 main(){String emoji='), '未保存内容写回磁盘'
        cached_files = list((cache/'index').glob('*.json'))
        assert cached_files
        server.close()
        restored = Transport(binary, project)
        restored.query('initialize')
        restored.send(dict(method='textDocument/didOpen',params={'textDocument':dict(uri=uri,text=text,version=24)}))
        restored.query('textDocument/hover',dict(textDocument=dict(uri=uri),position=position(text,text.index('value')+2)))
        assert restored.query('magel/lspStats')['restored'] >= 1
        restored.diagnostic(uri,24,lambda values: not values)
        restored.close()
        # 持久化记录损坏必须回退分析，不能导致服务退出或显示旧声明。
        for file in (cache/'index').glob('*.json'): file.write_text('corrupt', encoding='utf-8')
        corrupted = Transport(binary, project)
        corrupted.query('initialize')
        corrupted.send(dict(method='textDocument/didOpen',params={'textDocument':dict(uri=uri,text=text,version=25)}))
        result = corrupted.query('textDocument/hover',dict(textDocument=dict(uri=uri),position=position(text,text.index('value')+2)))
        assert 'int64 value' in result['contents']['value'], result
        assert corrupted.query('magel/lspStats')['scans'] >= 1
        corrupted.close()
        print(json.dumps(dict(firstHoverSeconds=round(latency,4), indexScans=scanned, cache=str(cache)),ensure_ascii=False))
    print('LSP 并行提示、增量 UTF-16、未保存依赖、缓存复用/恢复、版本合并与清理通过。')


if __name__ == '__main__': main()
