"""用本地 HTTP 服务器验证在线依赖、递归导入、共享缓存及离线执行。"""
from pathlib import Path
import argparse
import hashlib
import http.server
import io
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import threading
import zipfile
ROOT = Path(__file__).resolve().parents[1]

def archive(files, kind='zip', prefix=''):
    """在内存中构造小型 ZIP 或 TAR 包，供本地服务下载。"""
    output = io.BytesIO()
    if kind == 'zip':
        with zipfile.ZipFile(output, 'w') as package:
            for name, text in files.items():
                package.writestr(prefix + name, text)
    else:
        with tarfile.open(fileobj=output, mode='w:gz') as package:
            for name, text in files.items():
                data = text.encode('utf-8')
                member = tarfile.TarInfo(prefix + name)
                member.size = len(data)
                package.addfile(member, io.BytesIO(data))
    return output.getvalue()

def main():
    """下载三层菱形依赖；关闭服务器后复测编译、VM、LSP 与内容变更诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali'))
    parser.add_argument('--unit-only', action='store_true')
    args = parser.parse_args()
    payloads = {}
    requests = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        """只服务测试包，记录每个 URL 的实际下载次数。"""

        def do_GET(self):
            requests[self.path] = requests.get(self.path, 0) + 1
            if self.path not in payloads:
                self.send_error(404)
                return
            data = payloads[self.path]
            self.send_response(200)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f'http://127.0.0.1:{server.server_port}'

    def publish(name, files, kind='zip', prefix=''):
        data = archive(files, kind, prefix)
        payloads['/' + name] = data
        return url + '/' + name + '#sha256=' + hashlib.sha256(data).hexdigest()

    def config(name, dependencies=''):
        return '[package]\nname="' + name + '"\nversion="0.1.0"\n' + ('[dependencies]\n' + dependencies if dependencies else '')
    try:
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='chali-online-deps-') as folder, tempfile.TemporaryDirectory(prefix='online-toolchain-', dir=ROOT / 'build') as compiler_folder:
            work = Path(folder)
            toolchain = Path(compiler_folder) / 'generator'
            toolchain.mkdir()
            compiler = toolchain / args.chali.name
            if not args.unit_only:
                shutil.copy2(args.chali.resolve(), compiler)
            app = work / 'app'
            app.mkdir()
            leaf = publish('leaf.zip', {'chali.toml': config('leaf'), 'modules/lib/leaf.chl': 'module lib::leaf;\n/// 叶子依赖函数。\nint64 value(){return 21;}\n'})
            middle = publish('middle.tar.gz', {'chali.toml': config('middle', 'leaf=' + json.dumps(leaf) + '\n'), 'modules/lib/middle.chl': 'module lib::middle;\nimport lib::leaf;\nint64 value(){return lib::leaf::value()+1;}\n'}, 'tar')
            top = publish('top.zip', {'chali.toml': config('top', 'middle=' + json.dumps(middle) + '\nleaf=' + json.dumps(leaf) + '\n'), 'modules/lib/top.chl': 'module lib::top;\nimport lib::middle;\nimport lib::leaf;\nint64 value(){return lib::middle::value()+lib::leaf::value();}\n'}, prefix='top-0.1.0/')
            (app / 'chali.toml').write_text(config('app', 'top=' + json.dumps(top) + '\n'), encoding='utf-8')
            source = app / 'main.chl'
            source.write_text('import lib::top;\nint64 main(){std::lang::println("{}",lib::top::value());return 0;}\n', encoding='utf-8')
            env = dict(os.environ, CHALI_LANG='zh')

            def run(*command, success=True):
                result = subprocess.run(list(map(str, command)), cwd=work, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=240)
                assert (result.returncode == 0) == success, (command, result.stdout, result.stderr)
                return result

            def deps(action, project=app, offline=False, success=True):
                command = [compiler] if not args.unit_only else [sys.executable, ROOT / 'tools/chalidev.py', '--chali', compiler]
                return run(*command, 'deps', action, '--project', project, *(['--offline'] if offline else []), success=success)
            deps('lock', offline=True, success=False)
            assert not requests
            deps('lock')
            assert requests == {'/top.zip': 1, '/leaf.zip': 1, '/middle.tar.gz': 1}, requests
            libraries = toolchain / 'libraries'
            assert len(list(libraries.iterdir())) == 3
            assert not (app / '.chali/deps').exists()
            deps('check', offline=True)
            other = work / 'other'
            other.mkdir()
            (other / 'chali.toml').write_text((app / 'chali.toml').read_text(encoding='utf-8'), encoding='utf-8')
            deps('lock', other, offline=True)
            assert sum(requests.values()) == 3
            print('HTTP：ZIP、TAR、三层菱形依赖、唯一顶层目录及跨项目共享缓存通过。', flush=True)
            bad = work / 'bad'
            bad.mkdir()
            wrong = url + '/leaf.zip#sha256=' + '0' * 64
            (bad / 'chali.toml').write_text(config('bad', 'bad=' + json.dumps(wrong) + '\n'), encoding='utf-8')
            result = deps('lock', bad, success=False)
            assert 'SHA-256' in result.stderr
            unsafe = publish('unsafe.zip', {'chali.toml': config('unsafe'), '../../escape.chl': 'bad'})
            (bad / 'chali.toml').write_text(config('bad', 'unsafe=' + json.dumps(unsafe) + '\n'), encoding='utf-8')
            deps('lock', bad, success=False)
            assert len(list(libraries.iterdir())) == 3 and (not (work / 'escape.chl').exists())
            print('HTTP：摘要不匹配和不安全压缩路径被拒绝，未留下半成品。', flush=True)
            server.shutdown()
            thread.join()
            server.server_close()
            locked = (app / 'chali.lock').read_bytes()
            deps('check', offline=True)
            deps('lock', offline=True)
            assert (app / 'chali.lock').read_bytes() == locked
            if not args.unit_only:
                assert run(compiler, 'run', source, 'out', work / 'vm').stdout == '43\n'
            sys.path.insert(0, str(ROOT / 'tools'))
            from chalidev import LanguageServer, position
            language = LanguageServer(compiler)
            text = 'import lib::top;\nint64 main(){return lib::leaf::value();}'
            uri = source.as_uri()
            language.documents[uri] = {'text': text, 'version': 1}
            hover = language.query('textDocument/hover', {'textDocument': {'uri': uri}, 'position': position(text, text.index('value') + 1)})
            assert '叶子依赖函数' in hover['contents']['value']
            leaf_file = next(libraries.glob('leaf-*/modules/lib/leaf.chl'))
            original = leaf_file.read_text(encoding='utf-8')
            leaf_file.write_text(original.replace('21', '99'), encoding='utf-8')
            deps('check', offline=True, success=False)
            if not args.unit_only:
                run(compiler, '--check', source, success=False)
            leaf_file.write_text(original, encoding='utf-8')
            deps('check', offline=True)
            repo = work / 'git-source'
            repo.mkdir()
            (repo / 'chali.toml').write_text(config('gitlib'), encoding='utf-8')
            (repo / 'gitvalue.chl').write_text('module gitvalue;\nint64 value(){return 7;}', encoding='utf-8')
            run('git', '-C', repo, 'init')
            run('git', '-C', repo, 'add', '.')
            run('git', '-C', repo, '-c', 'user.name=模块测试', '-c', 'user.email=test@example.invalid', 'commit', '-m', '创建在线依赖夹具')
            commit = run('git', '-C', repo, 'rev-parse', 'HEAD').stdout.strip()
            gitapp = work / 'git-app'
            gitapp.mkdir()
            (gitapp / 'chali.toml').write_text(config('gitapp', 'gitlib=' + json.dumps('git+' + repo.as_uri() + '#' + commit) + '\n'), encoding='utf-8')
            deps('lock', gitapp)
            deps('check', gitapp, offline=True)
            assert (libraries / ('gitlib-' + commit)).is_dir()
            if not args.unit_only:
                gitmain = gitapp / 'main.chl'
                gitmain.write_text('import gitvalue;\nint64 main(){return gitvalue::value()-7;}', encoding='utf-8')
                run(compiler, 'run', gitmain, 'out', work / 'gitvm')
            localapp = work / 'local-app'
            localapp.mkdir()
            (localapp / 'chali.toml').write_text(config('localapp', 'local="path:../git-source"\n'), encoding='utf-8')
            deps('lock', localapp, offline=True)
            deps('check', localapp, offline=True)
            legacyapp = work / 'legacy-app'
            legacyapp.mkdir()
            legacy = legacyapp / '.chali/deps' / ('legacygit-' + commit)
            legacy.parent.mkdir(parents=True)
            shutil.copytree(libraries / ('gitlib-' + commit), legacy)
            (legacyapp / 'chali.toml').write_text(config('legacyapp', 'legacygit=' + json.dumps('git+' + repo.as_uri() + '#' + commit) + '\n'), encoding='utf-8')
            deps('lock', legacyapp, offline=True)
            deps('check', legacyapp, offline=True)
            if not args.unit_only:
                for project in (localapp, legacyapp):
                    main = project / 'main.chl'
                    main.write_text('import gitvalue;\nint64 main(){return gitvalue::value()-7;}', encoding='utf-8')
                    run(compiler, '--check', main)
            print('递归 LSP 注释、锁文件确定性、内容变更诊断、路径依赖及新旧 Git 缓存通过。', flush=True)
    finally:
        if thread.is_alive():
            server.shutdown()
            thread.join()
        server.server_close()
    print('在线依赖本地服务器回归全部通过。')
if __name__ == '__main__':
    main()
