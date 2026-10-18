"""通过真实 HTTP 和 SQLite 验证 MAGEL 学生管理示例的解释与 AOT行为与持久化。"""
from pathlib import Path
from contextlib import closing
import argparse
import http.client
import json
import os
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / 'example/web-students'
EXE = '.exe' if os.name == 'nt' else ''


def sqlite_environment():
    """使用内置 SQLite，并验证程序不依赖外部动态库配置。"""
    env = os.environ.copy()
    env['SQLITE_LIBRARY'] = str(ROOT / 'build/missing-sqlite3.dll')
    return env


def run(command, env):
    """执行编译命令，保留失败诊断并拒绝非空标准错误。"""
    result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=240)
    assert result.returncode == 0 and not result.stderr, (command, result.stdout, result.stderr)
    return result


class Server:
    """以就绪文件发现系统分配的端口，按请求上限正常关闭测试服务器。"""

    def __init__(self, command, work, database, limit, env):
        """启动子进程并等待成功监听，测试不争抢预先选择的端口。"""
        self.count, self.limit = 0, limit
        ready = work / ('ready-' + str(time.time_ns()))
        self.process = subprocess.Popen(list(map(str, [*command, database, '0', limit,
                                                       EXAMPLE / 'index.html', ready])),
                                        cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                if self.process.poll() is not None:
                    raise AssertionError(self.process.communicate())
                if ready.exists() and ready.read_text(encoding='utf-8').isdigit():
                    self.port = int(ready.read_text(encoding='utf-8'))
                    return
                time.sleep(.05)
            raise AssertionError('服务器未在 60 秒内就绪')
        except BaseException:
            self.abort()
            raise

    def request(self, method, path, expected, body=None, headers=None):
        """发送普通 HTTP 请求并校验状态、字节长度与 JSON。"""
        payload = None if body is None else json.dumps(body, ensure_ascii=False).encode('utf-8')
        actual_headers = {'Content-Type': 'application/json'}
        actual_headers.update(headers or {})
        connection = http.client.HTTPConnection('127.0.0.1', self.port, timeout=15)
        self.count += 1
        try:
            connection.request(method, path, body=payload, headers=actual_headers)
            response = connection.getresponse()
            data = response.read()
            assert response.status == expected, (method, path, response.status, data)
            assert int(response.getheader('Content-Length')) == len(data)
            assert response.getheader('Connection') == 'close'
            if path == '/' and expected == 200:
                assert data == (EXAMPLE / 'index.html').read_bytes()
                assert response.getheader('Content-Type').startswith('text/html')
                return data
            assert response.getheader('Content-Type').startswith('application/json')
            result = json.loads(data)
            if expected >= 400:
                assert isinstance(result['error'], str)
            return result
        except OSError as error:
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                raise error
            if self.process.poll() is not None:
                out, err = self.process.communicate(timeout=10)
                raise AssertionError((method, path, out.decode('utf-8', errors='replace'),
                                      err.decode('utf-8', errors='replace'))) from error
            raise
        finally:
            connection.close()

    def raw(self, head, body=b'', expected=400, fragmented=False, truncate=False):
        """发送原始或分段请求，检查报文边界和截断处理。"""
        self.count += 1
        request = head.replace('{host}', f'127.0.0.1:{self.port}').encode('ascii')
        with socket.create_connection(('127.0.0.1', self.port), timeout=15) as peer:
            if fragmented:
                for part in (request[:7], request[7:], body[:1], body[1:4], body[4:]):
                    if part:
                        peer.sendall(part)
                        time.sleep(.02)
            else:
                peer.sendall(request + body)
            if truncate:
                peer.shutdown(socket.SHUT_WR)
            response = http.client.HTTPResponse(peer)
            response.begin()
            data = response.read()
            assert response.status == expected, (head, response.status, data)
            assert len(data) == int(response.getheader('Content-Length'))
            return json.loads(data)

    def finish(self):
        """补足健康请求使主循环正常返回，并检查资源诊断。"""
        assert self.count <= self.limit
        while self.count < self.limit:
            self.request('GET', '/api/health', 200)
        out, err = self.process.communicate(timeout=20)
        assert self.process.returncode == 0 and not err, (out, err)
        assert f'服务器正常关闭：{self.limit} 个请求'.encode() in out, out

    def abort(self):
        """仅在断言失败时终止测试子进程，避免遗留监听器。"""
        if self.process.poll() is None:
            self.process.kill()
        self.process.communicate(timeout=10)


def exercise(command, work, env):
    """覆盖增删改查、输入校验、SQL 转义、报文分段与重启持久化。"""
    database = work / 'students.db'
    server = Server(command, work, database, 60, env)
    try:
        assert server.request('GET', '/api/health', 200) == {'status': 'ok', 'database': 'sqlite'}
        server.request('GET', '/', 200)
        assert server.request('GET', '/api/students', 200) == []
        first = {'studentNo': "S'001", 'name': '中文 O\'Reilly\\目录\n<script>alert(1)</script>',
                 'email': 'first@example.test', 'age': 20}
        created = server.request('POST', '/api/students', 201, first)
        assert created == {'id': 1, **first}
        assert server.request('GET', '/api/students/1', 200) == created
        server.request('POST', '/api/students', 409, first)
        updated = {**first, 'name': '更新姓名', 'age': 21}
        assert server.request('PUT', '/api/students/1', 200, updated) == {'id': 1, **updated}
        assert server.request('GET', '/api/students/1', 200)['name'] == '更新姓名'
        second = {'studentNo': "X'); DROP TABLE students;--", 'name': '保留学生',
                  'email': 'persist@example.test', 'age': 0}
        persisted = server.request('POST', '/api/students', 201, second)
        assert persisted == {'id': 2, **second}
        server.request('PUT', '/api/students/2', 409, first)
        assert server.request('DELETE', '/api/students/1', 200) == {'deleted': True}
        server.request('GET', '/api/students/1', 404)
        server.request('DELETE', '/api/students/1', 404)
        server.request('PUT', '/api/students/999', 404, first)
        assert server.request('GET', '/api/students', 200) == [persisted]
        for body in ({**first, 'age': '20'}, {**first, 'age': 1.5}, {**first, 'age': 151},
                     {**first, 'age': -1}, {**first, 'name': ''}, {**first, 'name': 'bad\x00text'},
                     {**first, 'id': 7}, {'name': 'missing'}, {**first, 'email': 'invalid'},
                     {**first, 'studentNo': 'x' * 33}):
            server.request('POST', '/api/students', 400, body)
        server.request('POST', '/api/students', 415, first, {'Content-Type': 'text/plain'})
        server.request('PATCH', '/api/students', 405)
        server.request('GET', '/missing', 404)
        server.request('GET', '/api/students/0', 400)
        server.request('GET', '/api/students/1%20OR%201=1', 400)
        server.request('POST', '/api/students', 403, first, {'Origin': 'http://foreign.invalid'})
        prefix = 'POST /api/students HTTP/1.1\r\nHost: {host}\r\n'
        server.raw(prefix + 'Content-Length: 1\r\nContent-Length: 1\r\n\r\n', b'x')
        server.raw(prefix + '\r\n', expected=411)
        server.raw(prefix + 'Transfer-Encoding: chunked\r\n\r\n', expected=501)
        server.raw(prefix + 'Expect: 100-continue\r\n\r\n', expected=417)
        server.raw(prefix + 'Content-Length: 16385\r\n\r\n', expected=413)
        server.raw(prefix + 'Content-Length: -1\r\n\r\n', expected=413)
        server.raw(prefix + 'X-Large: ' + 'x' * 9000 + '\r\n\r\n', expected=431)
        server.raw('\r\n\r\n')
        server.raw('broken\r\n\r\n')
        server.raw('GET /api/health HTTP/1.1\r\n\r\n')
        server.raw('GET /api/health HTTP/1.1\r\nHost: wrong.invalid\r\n\r\n')
        server.raw(prefix + 'Content-Length: 1000\r\n\r\n', b'{}', truncate=True)
        server.raw(prefix + 'Content-Length: 1\r\nContent-Type: application/json\r\n\r\n', b'{')
        third = {'studentNo': 'FRAG', 'name': '分段\t中文"姓名', 'email': 'fragment@example.test', 'age': 150}
        payload = json.dumps(third, ensure_ascii=False).encode('utf-8')
        result = server.raw(prefix + f'Content-Length: {len(payload)}\r\nContent-Type: application/json; charset=utf-8\r\n\r\n',
                            payload, expected=201, fragmented=True)
        assert result == {'id': 3, **third}
        server.request('DELETE', '/api/students/3', 200)
        server.finish()
    finally:
        if server.process.poll() is None:
            server.abort()
    with closing(sqlite3.connect(database)) as connection:
        assert connection.execute('SELECT id,student_no,name,email,age FROM students').fetchall() == [
            (2, second['studentNo'], second['name'], second['email'], second['age'])]
        assert connection.execute('PRAGMA integrity_check').fetchone() == ('ok',)
    restarted = Server(command, work, database, 2, env)
    try:
        assert restarted.request('GET', '/api/students', 200) == [persisted]
        restarted.finish()
    finally:
        if restarted.process.poll() is None:
            restarted.abort()


def main():
    """构建并验证选定后端，默认运行解释执行与 AOT。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    parser.add_argument('--backends', nargs='+', choices=('run', 'aot'), default=['run', 'aot'])
    parser.add_argument('--make', help='可选 make 程序，用于验证 AOT 构建脚本路径')
    args = parser.parse_args()
    env = sqlite_environment()
    compiler = args.magel.resolve()
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='magel-web-students-', dir=ROOT / 'build') as directory:
        work = Path(directory)
        run([compiler, 'check', EXAMPLE / 'main.mgl'], env)
        for backend in args.backends:
            target = work / backend
            target.mkdir()
            if backend == 'run':
                command = [compiler, 'run', EXAMPLE / 'main.mgl', '--out', target / 'server', '--']
            else:
                executable = target / ('server' + EXE)
                compile_command = [compiler, backend, EXAMPLE / 'main.mgl', '--out', executable, '--opt-level=0']
                if backend == 'aot' and args.make:
                    compile_command.extend(['--make', args.make])
                run(compile_command, env)
                command = [executable]
            exercise(command, target, env)
            print(f'[通过] {backend}：62 个真实 HTTP 请求、SQLite CRUD、错误恢复、分段正文、重启持久化与空资源诊断', flush=True)
    print('MAGEL Web 学生管理示例验收通过', flush=True)


if __name__ == '__main__':
    main()
