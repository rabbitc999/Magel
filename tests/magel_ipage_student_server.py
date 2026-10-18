"""通过真实 HTTP 与 MySQL 验证 MAGEL 学生成绩信息管理系统后端（前后端分离版）。

覆盖内容：
  * 登录 / 注销 / 会话令牌，以及「无令牌、坏令牌」的拒绝路径；
  * 四种权限账号的模块可见性与接口门槛（学生 3、教师 5、管理员 5、超管 6）；
  * 五个模块的增删查改、聚合统计、校验分支与 admin 约束；
  * 前后端分离特有的部分：CORS 预检与实际响应头、后端不提供页面；
  * HTTP 报文的边界处理（Host 校验、重复 Content-Length、分段正文、头部超长等）。

自检**不碰真实库**：在独立测试库 `ipage_server_check` 里用固定夹具
`example/sql-magel/tests/fixture_ipage.sql` 配合 `--reinit` 重建，自增主键每次从 1 开始。

用法（在仓库根目录）：
    python tests/magel_ipage_student_server.py
    python tests/magel_ipage_student_server.py --backends run
    python tests/magel_ipage_student_server.py --mysql-password 你的口令

需要本机 MySQL 在跑；缺服务时属于环境未满足（退出码 2），不代表示例失败。
"""
from pathlib import Path
from contextlib import closing
from functools import partial
import argparse
import http.client
import http.server
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
STATIC = ROOT / 'example/sql-magel/example/ipage-student-server/web'
SOURCE = 'example/sql-magel/example/ipage-student-server/main.mgl'
FIXTURE = 'example/sql-magel/tests/fixture_ipage.sql'
TESTDB = 'ipage_server_check'
EXE = '.exe' if os.name == 'nt' else ''

_DEFAULT = object()

# 夹具里的四个账号（fixture_ipage.sql）。
ACCOUNTS = {
    'student': ('方智堃', 'zhongli', 'STUDENT', 0),
    'teacher': ('戚宇航', 'klee', 'TEACHER', 2),
    'administrator': ('陈承岩', 'nahida', 'ADMINISTRATOR', 3),
    'super': ('admin', '123456', 'SUPER_ADMINISTRATOR', 4),
}
# 主菜单可见模块数：学生 3、教师 5、管理员 5、超级管理员 6。
VISIBLE = {'student': 3, 'teacher': 5, 'administrator': 5, 'super': 6}


def mysql_available(host, port):
    """MySQL 是否在监听；不在就属于环境未满足。"""
    try:
        with socket.create_connection((host, port), timeout=3):
            return True
    except OSError:
        return False


def run(command, env):
    """执行编译命令，保留失败诊断并拒绝非空标准错误。"""
    result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=600)
    assert result.returncode == 0 and not result.stderr, (command, result.stdout, result.stderr)
    return result


class Server:
    """以就绪文件发现系统分配的端口，按请求上限正常关闭测试服务器。"""

    def __init__(self, argv, work, limit, env):
        """启动子进程并等待成功监听；测试不争抢预先选择的端口。"""
        self.count, self.limit = 0, limit
        self.token = None
        ready = work / ('ready-' + str(time.time_ns()))
        # `--max-requests` 让服务端处理够 limit 个请求就自己收尾退出，测试不必杀进程。
        self.process = subprocess.Popen(list(map(str, [*argv, '--max-requests', limit, '--ready', ready])),
                                       cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.ready = ready
        try:
            deadline = time.monotonic() + 120
            while time.monotonic() < deadline:
                if self.process.poll() is not None:
                    raise AssertionError(self.process.communicate())
                if ready.exists() and ready.read_text(encoding='utf-8').strip().isdigit():
                    self.port = int(ready.read_text(encoding='utf-8').strip())
                    return
                time.sleep(.05)
            raise AssertionError('服务器未在 120 秒内就绪')
        except BaseException:
            self.abort()
            raise

    def request(self, method, path, expected, body=None, headers=None, token=_DEFAULT):
        """发送普通 HTTP 请求并校验状态、字节长度、CORS 头与 JSON。"""
        if token is _DEFAULT:
            token = self.token
        payload = None if body is None else json.dumps(body, ensure_ascii=False).encode('utf-8')
        actual_headers = {}
        if body is not None:
            actual_headers['Content-Type'] = 'application/json'
        if token:
            actual_headers['Authorization'] = 'Bearer ' + token
        actual_headers.update(headers or {})
        connection = http.client.HTTPConnection('127.0.0.1', self.port, timeout=20)
        self.count += 1
        try:
            connection.request(method, path, body=payload, headers=actual_headers)
            response = connection.getresponse()
            data = response.read()
            assert response.status == expected, (method, path, response.status, data)
            assert int(response.getheader('Content-Length')) == len(data), (method, path)
            assert response.getheader('Connection') == 'close', (method, path)
            # 跨源必备的三个响应头，每个响应都要有。
            assert response.getheader('Access-Control-Allow-Origin') == '*', (method, path)
            assert 'POST' in response.getheader('Access-Control-Allow-Methods'), (method, path)
            assert 'Authorization' in response.getheader('Access-Control-Allow-Headers'), (method, path)
            if expected == 204:
                assert data == b'', (method, path, data)
                return None
            assert response.getheader('Content-Type').startswith('application/json'), (method, path)
            result = json.loads(data)
            if expected >= 400:
                assert isinstance(result['error'], str), (method, path, result)
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

    def login(self, who):
        """以夹具里的某个账号登录，并把令牌存为后续请求的默认令牌。"""
        name, password, permission, level = ACCOUNTS[who]
        result = self.request('POST', '/api/login', 200,
                              {'userName': name, 'password': password}, token=None)
        assert result['user']['permission'] == permission and result['user']['level'] == level, result
        assert len(result['token']) == 64 and all(c in '0123456789abcdef' for c in result['token'])
        keys = [module['key'] for module in result['modules']]
        assert len(keys) == VISIBLE[who], (who, keys)
        assert keys == [module for module in ['student', 'sclass', 'course', 'selection', 'user', 'password']
                        if module in keys], keys
        self.token = result['token']
        return result

    def raw(self, head, body=b'', expected=400, fragmented=False, truncate=False, headers=True):
        """发送原始或分段请求，检查报文边界与截断处理。"""
        self.count += 1
        request = head.replace('{host}', f'127.0.0.1:{self.port}').replace('{port}', str(self.port)).encode('ascii')
        with socket.create_connection(('127.0.0.1', self.port), timeout=20) as peer:
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
            if headers:
                assert response.getheader('Access-Control-Allow-Origin') == '*'
            if expected == 204:
                return None
            return json.loads(data)

    def finish(self):
        """补足健康请求使主循环正常返回，并检查资源诊断。"""
        assert self.count <= self.limit, (self.count, self.limit)
        while self.count < self.limit:
            self.request('GET', '/api/health', 200, token=None)
        out, err = self.process.communicate(timeout=30)
        assert self.process.returncode == 0 and not err, (out, err)
        assert f'服务器正常关闭：{self.limit} 个请求'.encode() in out, out

    def abort(self):
        """仅在断言失败时终止测试子进程，避免遗留监听器。"""
        if self.process.poll() is None:
            self.process.kill()
        self.process.communicate(timeout=10)


def serve_static():
    """用线程内的标准库静态服务器托管 web/，模拟「前端独立部署」。"""
    class QuietHandler(http.server.SimpleHTTPRequestHandler):
        """关掉逐条访问日志，免得盖住验收输出。"""

        def log_message(self, *args):
            pass

    handler = partial(QuietHandler, directory=str(STATIC))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, server.server_address[1]


def fetch(port, path):
    """取回静态资源（模拟浏览器加载前端）。"""
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=15)
    try:
        connection.request('GET', path)
        response = connection.getresponse()
        return response.status, response.read()
    finally:
        connection.close()


def check_static():
    """前端是一个独立的静态包：四个文件可独立取回，且 index.html 引用了其余三个。"""
    missing = [name for name in ('index.html', 'style.css', 'app.js', 'config.js')
               if not (STATIC / name).exists()]
    assert not missing, ('缺少前端文件', missing)
    server, port = serve_static()
    try:
        status, page = fetch(port, '/index.html')
        assert status == 200, status
        text = page.decode('utf-8')
        for name in ('style.css', 'app.js', 'config.js'):
            assert name in text, ('index.html 未引用', name)
            status, body = fetch(port, '/' + name)
            assert status == 200 and len(body) > 0, (name, status)
        # 前端不把权限写死在页面里，而是读后端 /api/session 的返回。
        app = (STATIC / 'app.js').read_bytes().decode('utf-8')
        assert '/api/session' in app and 'needLevel' in app
        assert 'innerHTML =' not in app and 'innerHTML=' not in app, '前端不得用 innerHTML 渲染后端数据'
        assert 'textContent' in app
    finally:
        server.shutdown()
        server.server_close()
    # 有 node 就顺带做一次语法检查（没装 node 不算失败）。
    if which('node'):
        result = subprocess.run([which('node'), '--check', str(STATIC / 'app.js')],
                                capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stderr


def which(name):
    """在 PATH 里找一个可执行文件。"""
    from shutil import which as find
    return find(name)


def check_frontend_contracts(server):
    """前后端分离的硬约束：后端不提供页面，且跨源预检放行。"""
    server.token = None
    # 后端只是 API；页面由静态服务器托管，所以根路径与常见静态路径都必须 404。
    server.request('GET', '/', 404, token=None)
    server.request('GET', '/index.html', 404, token=None)
    server.request('GET', '/web/app.js', 404, token=None)
    # 预检：浏览器发 JSON + Authorization 前必发 OPTIONS。
    preflight = server.raw(
        'OPTIONS /api/students HTTP/1.1\r\nHost: {host}\r\n'
        'Origin: http://127.0.0.1:5500\r\n'
        'Access-Control-Request-Method: POST\r\n'
        'Access-Control-Request-Headers: content-type,authorization\r\n\r\n',
        expected=204)
    assert preflight is None


def exercise_http(server):
    """HTTP 报文的边界处理（不依赖业务状态）。"""
    prefix = 'POST /api/login HTTP/1.1\r\nHost: {host}\r\n'
    server.raw(prefix + 'Content-Length: 1\r\nContent-Length: 1\r\n\r\n', b'x')
    server.raw(prefix + '\r\n', expected=411)
    server.raw(prefix + 'Transfer-Encoding: chunked\r\n\r\n', expected=501)
    server.raw(prefix + 'Expect: 100-continue\r\n\r\n', expected=417)
    server.raw(prefix + 'Content-Length: 65537\r\n\r\n', expected=413)
    server.raw(prefix + 'Content-Length: -1\r\n\r\n', expected=413)
    server.raw(prefix + 'X-Large: ' + 'x' * 9000 + '\r\n\r\n', expected=431)
    server.raw('\r\n\r\n')
    server.raw('broken\r\n\r\n')
    server.raw('GET /api/health HTTP/1.1\r\n\r\n')
    server.raw('GET /api/health HTTP/1.1\r\nHost: wrong.invalid:1\r\n\r\n')
    server.raw('GET /api/health HTTP/1.1\r\nHost: 127.0.0.1:1\r\n\r\n')
    server.raw(prefix + 'Content-Length: 1000\r\n\r\n', b'{}', truncate=True)
    # Host 允许 localhost 与 127.0.0.1 两种写法（前端可能用任一种访问后端）。
    server.raw('GET /api/health HTTP/1.1\r\nHost: localhost:{port}\r\n\r\n', expected=200)
    # 分段正文：请求行、头部、正文分五次发出，必须仍能正确组装。
    payload = json.dumps({'userName': 'admin', 'password': '123456'}).encode('ascii')
    result = server.raw(prefix + f'Content-Length: {len(payload)}\r\n'
                                 'Content-Type: application/json\r\n\r\n',
                        payload, expected=200, fragmented=True)
    assert result['user']['permission'] == 'SUPER_ADMINISTRATOR'


def exercise_auth(server):
    """登录、注销、令牌校验与四种账号的可见模块。"""
    server.token = None
    server.request('GET', '/api/health', 200, token=None)
    # 没有令牌、令牌不认：所有受保护接口一律 401。
    server.request('GET', '/api/session', 401, token=None)
    server.request('GET', '/api/students', 401, token=None)
    server.request('GET', '/api/students', 401, token='0' * 64)
    # 鉴权在方法判定之前：没令牌时先撞 401，不看方法。
    server.request('GET', '/api/logout', 401, token=None)
    # 登录失败：口令错、账号不存在；缺字段属于请求本身不合法，给 400。
    server.request('POST', '/api/login', 401, {'userName': 'admin', 'password': 'wrong1'}, token=None)
    server.request('POST', '/api/login', 401, {'userName': 'nobody', 'password': '123456'}, token=None)
    server.request('POST', '/api/login', 400, {'userName': '', 'password': ''}, token=None)
    server.request('POST', '/api/login', 400, {'userName': 'admin', 'password': ''}, token=None)
    server.request('POST', '/api/login', 400, {'userName': 'admin'}, token=None)
    server.request('POST', '/api/login', 415, {'userName': 'admin', 'password': '123456'},
                   {'Content-Type': 'text/plain'}, token=None)

    for who in ('student', 'teacher', 'administrator', 'super'):
        data = server.login(who)
        assert data['user']['permissionLabel'] in ('学生', '教师', '管理员', '超级管理员')
        session = server.request('GET', '/api/session', 200)
        assert session['user']['userName'] == data['user']['userName']
        # 每个模块的动作表都自带门槛，前端照它决定按钮可见性。
        for module in session['modules']:
            for action in module['actions']:
                assert isinstance(action['needLevel'], int)

    # 学生只有 3 个模块，且拿不到班级 / 用户 / 选项列表。
    server.login('student')
    keys = {module['key'] for module in server.request('GET', '/api/session', 200)['modules']}
    assert keys == {'student', 'selection', 'password'}, keys
    server.request('GET', '/api/sclasses', 403)
    server.request('GET', '/api/courses', 403)
    server.request('GET', '/api/users', 403)
    server.request('GET', '/api/options/sclasses', 403)
    server.request('POST', '/api/students', 403, {'name': '越权', 'sex': 'MALE'})
    server.request('DELETE', '/api/students/1', 403)
    server.request('POST', '/api/selections', 403,
                   {'studentId': 1, 'courseId': 1, 'testDate': '2025-06-01', 'grade': 60})
    # 但学生能看学生、成绩与统计。
    assert server.request('GET', '/api/students', 200)['count'] == 5
    assert server.request('GET', '/api/selections', 200)['count'] == 6
    assert server.request('GET', '/api/selections/stats/course', 200)['studentCount'] == 6
    assert server.request('GET', '/api/selections/stats/class', 200)['count'] == 3

    # 教师能改不能删。
    server.login('teacher')
    server.request('DELETE', '/api/students/5', 403)
    server.request('DELETE', '/api/sclasses/3', 403)
    server.request('GET', '/api/users', 403)
    server.request('GET', '/api/options/sclasses', 200)
    server.request('GET', '/api/options/courses', 200)

    # 管理员能删，但用户管理仍然只有超级管理员能看。
    server.login('administrator')
    server.request('GET', '/api/users', 403)
    server.request('POST', '/api/users', 403,
                   {'userName': 'x', 'password': 'abcd', 'permission': 'STUDENT'})

    # 注销后令牌立即失效。
    server.login('super')
    token = server.token
    server.request('POST', '/api/logout', 200)
    server.request('GET', '/api/session', 401, token=token)
    server.token = token
    server.request('GET', '/api/students', 401)


def exercise_students(server):
    """学生模块：新增、校验、修改、查询、删除与外键连坐。"""
    server.login('super')
    first = server.request('GET', '/api/students', 200)
    assert first['total'] == 5 and first['count'] == 5 and first['limit'] == 200
    rows = first['items']
    assert [row['name'] for row in rows] == ['李航超', '陈茜莉', '孙永轩', '陶婉萌', '宇文莎'], rows
    assert rows[0]['sexLabel'] == '男' and rows[0]['sclassName'] == '1班' and rows[0]['gradeLabel'] == '高一'
    assert rows[0]['courseCount'] == 3 and float(rows[0]['averageScore']) == 74.3
    assert rows[4]['sclassName'] == '未分班' and rows[4]['courseCount'] == 0

    # 校验分支。
    server.request('POST', '/api/students', 400, {'sex': 'MALE'})
    server.request('POST', '/api/students', 400, {'name': '缺性别'})
    server.request('POST', '/api/students', 400, {'name': '张三', 'sex': '未知'})
    server.request('POST', '/api/students', 400, {'name': '张三', 'sex': '男', 'sclassId': 99})
    # 新增：带班与不带班两种。
    created = server.request('POST', '/api/students', 201, {'name': '测试同学', 'sex': '女', 'sclassId': 2})
    assert created['id'] == 6 and '新学生ID：6' in created['message'], created
    assert server.request('POST', '/api/students', 201, {'name': '未分班同学', 'sex': '男'})['id'] == 7

    # 查询：按姓名、按班级。
    by_name = server.request('POST', '/api/students/query', 200, {'mode': 'name', 'keyword': '测试'})
    assert [row['name'] for row in by_name['items']] == ['测试同学'], by_name
    by_class = server.request('POST', '/api/students/query', 200, {'mode': 'class', 'keyword': '2班'})
    assert [row['name'] for row in by_class['items']] == ['孙永轩', '测试同学'], by_class
    assert server.request('POST', '/api/students/query', 200, {'mode': 'name', 'keyword': '不存在的人'})['count'] == 0

    # 修改：PUT 全量替换，不给班级就是不分班。
    server.request('PUT', '/api/students/6', 200, {'name': '改名同学', 'sex': '男', 'sclassId': 3})
    updated = server.request('POST', '/api/students/query', 200, {'mode': 'name', 'keyword': '改名同学'})['items'][0]
    assert updated['sclassName'] == '1班' and updated['gradeLabel'] == '高二' and updated['sexLabel'] == '男', updated
    server.request('PUT', '/api/students/6', 200, {'name': '改名同学', 'sex': '男'})
    assert server.request('POST', '/api/students/query', 200, {'mode': 'name', 'keyword': '改名同学'})['items'][0]['sclassName'] == '未分班'
    server.request('PUT', '/api/students/6', 400, {'name': '改名同学', 'sex': '男', 'sclassId': 99})
    server.request('PUT', '/api/students/6', 400, {'name': '', 'sex': '男'})
    server.request('PUT', '/api/students/999', 404, {'name': '不存在', 'sex': '男'})

    # 删除：带成绩的会连坐删掉 selection 行（外键 ON DELETE CASCADE）。
    before = server.request('GET', '/api/selections', 200)['total']
    removed = server.request('DELETE', '/api/students/1', 200)
    assert removed['deleted'] is True and removed['cascade'] == 3, removed
    after = server.request('GET', '/api/selections', 200)['total']
    assert after == before - 3, (before, after)
    server.request('DELETE', '/api/students/1', 404)
    server.request('DELETE', '/api/students/7', 200)
    server.request('DELETE', '/api/students/6', 200)
    assert server.request('GET', '/api/students', 200)['count'] == 4

    # 列宽护栏：上限按**字符**算，不是字节 —— 24 个汉字（72 字节）能进库，
    # 25 个要在写库前拦成 400，而不是漏成 MySQL 的 "Data too long" 500。
    boundary = server.request('POST', '/api/students', 201, {'name': '一' * 24, 'sex': '男'})
    server.request('DELETE', f"/api/students/{boundary['id']}", 200)
    too_long = server.request('POST', '/api/students', 400, {'name': '一' * 25, 'sex': '男'})
    assert '学生姓名不能超过 24 个字符' in too_long['error'], too_long
    # 同一道护栏也要管 PUT。
    put_long = server.request('PUT', '/api/students/2', 400, {'name': '一' * 25, 'sex': '男'})
    assert '学生姓名不能超过 24 个字符' in put_long['error'], put_long
    assert server.request('GET', '/api/students', 200)['count'] == 4


def exercise_sclasses_and_courses(server):
    """班级 / 课程模块，重点是外键的 SET NULL 与 CASCADE 两种行为。"""
    server.login('super')
    classes = server.request('GET', '/api/sclasses', 200)
    assert classes['count'] == 3 and classes['items'][0]['name'] == '1班'
    assert classes['items'][0]['administrator'] == '高瑶' and classes['items'][1]['gradeLabel'] == '高一'
    assert classes['items'][2]['gradeLabel'] == '高二'

    server.request('POST', '/api/sclasses', 400, {'name': '测试班', 'grade': '高一'})
    server.request('POST', '/api/sclasses', 400, {'name': '测试班', 'administrator': '老师', 'grade': '大一'})
    # 列宽护栏：班级名 36 字符、负责人 24 字符。
    name_long = server.request('POST', '/api/sclasses', 400,
                               {'name': '班' * 37, 'administrator': '老师', 'grade': '高一'})
    assert '班级名称不能超过 36 个字符' in name_long['error'], name_long
    admin_long = server.request('POST', '/api/sclasses', 400,
                                {'name': '测试班', 'administrator': '师' * 25, 'grade': '高一'})
    assert '班级负责人不能超过 24 个字符' in admin_long['error'], admin_long
    created = server.request('POST', '/api/sclasses', 201,
                             {'name': '测试班', 'administrator': '测试老师', 'grade': '高三'})
    new_id = created['id']
    assert server.request('POST', '/api/sclasses/query', 200, {'keyword': '测试班'})['count'] == 1
    server.request('PUT', f'/api/sclasses/{new_id}', 200,
                   {'name': '测试班2', 'administrator': '新老师', 'grade': '高二'})
    row = server.request('POST', '/api/sclasses/query', 200, {'keyword': '测试班2'})['items'][0]
    assert row['gradeLabel'] == '高二' and row['studentCount'] == 0
    # 删班级：学生变成未分班（SET NULL），不删学生。
    detached = server.request('DELETE', '/api/sclasses/2', 200)
    assert detached['detached'] == 1 and detached['cascade'] == 0, detached
    assert server.request('POST', '/api/students/query', 200, {'mode': 'name', 'keyword': '孙永轩'})['items'][0]['sclassName'] == '未分班'
    assert server.request('GET', '/api/students', 200)['count'] == 5
    server.request('DELETE', f'/api/sclasses/{new_id}', 200)
    server.request('DELETE', '/api/sclasses/999', 404)

    courses = server.request('GET', '/api/courses', 200)
    assert courses['count'] == 3 and courses['items'][0]['name'] == '语文'
    assert courses['items'][0]['stateLabel'] == '开课' and courses['items'][2]['stateLabel'] == '结课'
    server.request('POST', '/api/courses', 400, {'name': '测试课'})
    server.request('POST', '/api/courses', 400, {'name': '测试课', 'state': '上课中'})
    # 列宽护栏：课程名 36 字符。
    course_long = server.request('POST', '/api/courses', 400, {'name': '课' * 37, 'state': '开课'})
    assert '课程名称不能超过 36 个字符' in course_long['error'], course_long
    course_id = server.request('POST', '/api/courses', 201, {'name': '测试课', 'state': '开课'})['id']
    server.request('PUT', f'/api/courses/{course_id}', 200, {'name': '测试课2', 'state': '结课'})
    assert server.request('POST', '/api/courses/query', 200, {'keyword': '测试课2'})['items'][0]['stateLabel'] == '结课'
    # 删课程：连带删掉选课成绩（CASCADE）。
    scores = server.request('GET', '/api/selections', 200)['total']
    removed = server.request('DELETE', '/api/courses/1', 200)
    assert removed['cascade'] == 3, removed
    assert server.request('GET', '/api/selections', 200)['total'] == scores - 3
    server.request('DELETE', f'/api/courses/{course_id}', 200)
    server.request('DELETE', '/api/courses/999', 404)


def exercise_selections(server):
    """成绩模块：录入、重复拦截、范围校验、统计。"""
    server.login('super')
    rows = server.request('GET', '/api/selections', 200)['items']
    assert rows[0]['studentName'] == '李航超' and rows[0]['courseName'] == '语文'
    assert rows[0]['testDate'] == '2025-06-01' and rows[0]['grade'] == 63 and rows[0]['gradeLabel'] == '及格'
    # 等级边界：63 及格 / 71 中等 / 89 良好 / 95 优秀 / 58 不及格。
    assert [row['gradeLabel'] for row in rows] == ['及格', '中等', '良好', '优秀', '不及格', '中等'], rows

    server.request('POST', '/api/selections', 400, {'studentId': 0, 'courseId': 1, 'testDate': '2025-06-01', 'grade': 60})
    server.request('POST', '/api/selections', 400, {'studentId': 1, 'courseId': 1, 'testDate': '2025/06/01', 'grade': 60})
    server.request('POST', '/api/selections', 400, {'studentId': 1, 'courseId': 1, 'testDate': '2025-06-01', 'grade': 101})
    server.request('POST', '/api/selections', 400, {'studentId': 1, 'courseId': 1, 'testDate': '2025-06-01', 'grade': -1})
    server.request('POST', '/api/selections', 400, {'studentId': 999, 'courseId': 1, 'testDate': '2025-06-01', 'grade': 60})
    server.request('POST', '/api/selections', 400, {'studentId': 1, 'courseId': 999, 'testDate': '2025-06-01', 'grade': 60})
    # 同一学生同一课程重复录入：提示改用修改成绩。
    duplicated = server.request('POST', '/api/selections', 409,
                                {'studentId': 1, 'courseId': 1, 'testDate': '2025-06-01', 'grade': 60})
    assert '已有成绩' in duplicated['error'], duplicated

    new_id = server.request('POST', '/api/selections', 201,
                            {'studentId': 3, 'courseId': 3, 'testDate': '2025-07-01', 'grade': 88})['id']
    server.request('PUT', f'/api/selections/{new_id}', 200, {'grade': 92})
    server.request('PUT', f'/api/selections/{new_id}', 400, {'grade': 200})
    server.request('PUT', '/api/selections/999', 404, {'grade': 60})
    assert server.request('GET', '/api/selections', 200)['items'][-1]['gradeLabel'] == '优秀'
    assert server.request('DELETE', f'/api/selections/{new_id}', 200)['deleted'] is True
    server.request('DELETE', f'/api/selections/{new_id}', 404)

    # 按学生 / 按课程查询：学号走等值，姓名走模糊。
    by_id = server.request('POST', '/api/selections/query', 200, {'mode': 'student', 'keyword': '1'})
    assert [row['studentName'] for row in by_id['items']] == ['李航超'] * 3, by_id
    by_name = server.request('POST', '/api/selections/query', 200, {'mode': 'student', 'keyword': '莎'})
    assert by_name['count'] == 0, by_name
    by_course = server.request('POST', '/api/selections/query', 200, {'mode': 'course', 'keyword': '数学'})
    assert [row['studentName'] for row in by_course['items']] == ['李航超', '孙永轩'], by_course

    # 统计：全部与单课程；及格率按 >= 60 计。
    all_stat = server.request('GET', '/api/selections/stats/course', 200)
    assert all_stat['courseName'] == '全部课程' and all_stat['studentCount'] == 6, all_stat
    assert float(all_stat['maxScore']) == 95 and float(all_stat['minScore']) == 58
    assert all_stat['passCount'] == 5 and float(all_stat['passRate']) == 83.3, all_stat
    one = server.request('GET', '/api/selections/stats/course?courseId=2', 200)
    assert one['courseName'] == '数学' and one['studentCount'] == 2, one
    assert float(one['averageScore']) == 64.5, one
    assert server.request('GET', '/api/selections/stats/course?courseId=-1', 400)
    assert server.request('GET', '/api/selections/stats/course?courseId=abc', 400)

    by_class = server.request('GET', '/api/selections/stats/class', 200)
    assert by_class['count'] == 3
    assert [row['name'] for row in by_class['items']] == ['1班', '2班', '1班']
    assert by_class['items'][1]['studentCount'] == 1 and by_class['items'][1]['selectionCount'] == 1


def exercise_users(server):
    """用户模块：admin 约束、口令规则、改权限与重置密码。"""
    server.login('super')
    users = server.request('GET', '/api/users', 200)
    assert [row['userName'] for row in users['items']] == ['admin', '陈承岩', '戚宇航', '方智堃']
    assert users['items'][0]['permissionLabel'] == '超级管理员'
    assert 'password' not in json.dumps(users), '用户列表绝不能返回口令'

    # 口令与用户名规则。
    server.request('POST', '/api/users', 400, {'userName': '', 'password': 'abcd', 'permission': 'STUDENT'})
    server.request('POST', '/api/users', 400, {'userName': 'x' * 19, 'password': 'abcd', 'permission': 'STUDENT'})
    server.request('POST', '/api/users', 400, {'userName': '测试账号', 'password': 'ab', 'permission': 'STUDENT'})
    server.request('POST', '/api/users', 400, {'userName': '测试账号', 'password': '中文密码', 'permission': 'STUDENT'})
    # 口令上限 18 位（user_password 列宽），超了同样是 400，不是 MySQL 的 500。
    pass_long = server.request('POST', '/api/users', 400,
                               {'userName': '测试账号', 'password': 'a' * 19, 'permission': 'STUDENT'})
    assert '4~18' in pass_long['error'], pass_long
    server.request('POST', '/api/users', 400, {'userName': '测试账号', 'password': 'test1234', 'permission': 'BOSS'})
    # 除 admin 以外不能是超级管理员。
    server.request('POST', '/api/users', 400,
                   {'userName': '测试账号', 'password': 'test1234', 'permission': 'SUPER_ADMINISTRATOR'})
    server.request('POST', '/api/users', 409,
                   {'userName': 'admin', 'password': 'test1234', 'permission': 'STUDENT'})
    new_id = server.request('POST', '/api/users', 201,
                            {'userName': '测试账号', 'password': 'test1234', 'permission': 'TEACHER'})['id']
    # 新账号能登录，且可见模块数按教师算。
    server.request('POST', '/api/login', 200, {'userName': '测试账号', 'password': 'test1234'}, token=None)
    server.token = None
    server.login('super')

    # 改权限的两条规则。
    server.request('PUT', f'/api/users/{new_id}/permission', 400, {'permission': 'SUPER_ADMINISTRATOR'})
    server.request('PUT', '/api/users/1/permission', 400, {'permission': 'TEACHER'})
    server.request('PUT', f'/api/users/{new_id}/permission', 400, {'permission': '怪东西'})
    changed = server.request('PUT', f'/api/users/{new_id}/permission', 200, {'permission': 'ADMINISTRATOR'})
    assert '管理员' in changed['message'], changed
    assert server.request('PUT', '/api/users/1/permission', 200, {'permission': 'SUPER_ADMINISTRATOR'})
    assert server.request('PUT', '/api/users/999/permission', 404, {'permission': 'STUDENT'})

    # 重置密码。
    server.request('PUT', f'/api/users/{new_id}/password', 400, {'password': 'ab'})
    server.request('PUT', f'/api/users/{new_id}/password', 400, {'password': 'a' * 19})
    server.request('PUT', f'/api/users/{new_id}/password', 200, {'password': 'reset1234'})
    server.request('POST', '/api/login', 200, {'userName': '测试账号', 'password': 'reset1234'}, token=None)
    server.token = None
    server.login('super')

    # 删除限制：admin 不能删、管理员没有用户模块权限、不存在的用户 404。
    denied = server.request('DELETE', '/api/users/1', 400)
    assert 'admin' in denied['error'], denied
    server.request('DELETE', '/api/users/999', 404)
    server.login('administrator')
    server.request('DELETE', '/api/users/2', 403)
    server.request('PUT', '/api/users/2/password', 403, {'password': 'hacked99'})
    server.login('super')
    server.request('DELETE', f'/api/users/{new_id}', 200)


def exercise_own_password(server):
    """任何登录用户都能改自己的口令：先验旧口令，改完会话不失效。"""
    server.login('student')
    server.request('POST', '/api/password', 400, {'currentPassword': 'wrong', 'newPassword': 'newpass1'})
    server.request('POST', '/api/password', 400, {'currentPassword': 'zhongli', 'newPassword': 'ab'})
    # 新口令也有 18 位上限，且超长不该漏成 500。
    over = server.request('POST', '/api/password', 400,
                          {'currentPassword': 'zhongli', 'newPassword': 'a' * 19})
    assert '4~18' in over['error'], over
    result = server.request('POST', '/api/password', 200, {'currentPassword': 'zhongli', 'newPassword': 'newpass1'})
    assert '成功' in result['message'], result
    # 旧口令失效、新口令可用。
    server.request('POST', '/api/login', 401, {'userName': '方智堃', 'password': 'zhongli'}, token=None)
    server.request('POST', '/api/login', 200, {'userName': '方智堃', 'password': 'newpass1'}, token=None)
    # 改口令不影响已发出的令牌，所以还能用同一个会话改回去。
    server.request('POST', '/api/password', 200, {'currentPassword': 'newpass1', 'newPassword': 'zhongli'})
    server.request('POST', '/api/login', 401, {'userName': '方智堃', 'password': 'newpass1'}, token=None)
    server.token = None


def exercise_routing(server):
    """路由与方法的兜底：不存在的接口、错误的主键、不支持的方法。"""
    server.login('super')
    server.request('GET', '/api/unknown', 404)
    server.request('GET', '/api/', 404)
    server.request('GET', '/api/students/0', 400)
    server.request('GET', '/api/students/-1', 400)
    server.request('GET', '/api/students/1%20OR%201=1', 400)
    server.request('GET', '/api/students/9999999999999', 400)
    server.request('PATCH', '/api/students', 405)
    server.request('GET', '/api/login', 405)
    server.request('GET', '/api/logout', 405)
    server.request('GET', '/api/password', 405)
    server.request('DELETE', '/api/options/sclasses', 405)
    server.request('GET', '/api/options/unknown', 404)
    server.request('GET', '/api/students/1/extra', 400)
    server.request('POST', '/api/users/1/something', 405, {})
    server.request('GET', '/api/selections/stats/unknown', 400)
    # 健康检查用的库名要在返回里说清楚是 mysql 而不是 sqlite。
    assert server.request('GET', '/api/health', 200)['database'] == 'mysql'


def exercise(argv, work, env, limit, steps):
    """跑一组用例，返回本组实际发出的请求数。

    每一组用**独立的一次服务启动**：服务端带 `--reinit`，每次启动都从夹具重建测试库，
    所以组内可以放心做破坏性操作，组与组之间互不影响。
    """
    server = Server(argv, work, limit, env)
    try:
        assert server.request('GET', '/api/health', 200, token=None) == {'status': 'ok', 'database': 'mysql'}
        for step in steps:
            step(server)
        used = server.count
        server.finish()
        return used
    finally:
        if server.process.poll() is None:
            server.abort()


# 分组：(名字, 请求上限, 用例)。每一组都是一份全新的夹具，因此破坏性用例可以放胆写；
# 请求上限略高于实测值，多出来的部分由 finish() 用健康检查补齐，用来验证「处理够
# 指定数量后正常收尾」这条路径。
SCENARIOS = (
    ('只读接口与报文边界', 90, (check_frontend_contracts, exercise_http, exercise_auth, exercise_routing)),
    ('学生模块', 35, (exercise_students,)),
    ('班级与课程模块', 45, (exercise_sclasses_and_courses,)),
    ('成绩模块', 45, (exercise_selections,)),
    ('用户模块与改密', 55, (exercise_users, exercise_own_password)),
)


def main():
    """构建并验证选定后端，默认运行解释执行与 AOT。"""
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    parser.add_argument('--backends', nargs='+', choices=('run', 'aot'), default=['run', 'aot'])
    parser.add_argument('--make', help='可选 make 程序，用于验证 AOT 构建脚本路径')
    parser.add_argument('--mysql-host', default='127.0.0.1')
    parser.add_argument('--mysql-port', type=int, default=3306)
    parser.add_argument('--mysql-user', default='root')
    parser.add_argument('--mysql-password', default='123456')
    args = parser.parse_args()

    if not mysql_available(args.mysql_host, args.mysql_port):
        print(f'跳过：{args.mysql_host}:{args.mysql_port} 上没有 MySQL 服务，'
              f'属于环境未满足（先启动 MySQL 或改 --mysql-host/--mysql-port）', file=sys.stderr)
        return 2

    env = os.environ.copy()
    compiler = args.magel.resolve()
    check_static()

    connection = ['--listen-host', '127.0.0.1', '--listen-port', '0',
                  '--mysql-host', args.mysql_host, '--mysql-port', str(args.mysql_port),
                  '--mysql-user', args.mysql_user, '--mysql-password', args.mysql_password,
                  '--db', TESTDB, '--init', FIXTURE, '--reinit']

    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='magel-ipage-server-', dir=ROOT / 'build') as directory:
        work = Path(directory)
        run([compiler, 'check', SOURCE], env)
        for backend in args.backends:
            target = work / backend
            target.mkdir()
            if backend == 'run':
                base = [compiler, 'run', SOURCE, '--out', target / 'server', '--']
            else:
                executable = target / ('server' + EXE)
                build = [compiler, 'aot', SOURCE, '--out', executable, '--opt-level=0']
                if args.make:
                    build.extend(['--make', args.make])
                run(build, env)
                base = [executable]
            for name, limit, steps in SCENARIOS:
                used = exercise([*base, *connection], target, env, limit, steps)
                print(f'[通过] {backend} · {name}（{used} 个真实 HTTP 请求）', flush=True)
    print('MAGEL ipage-student-server 验收通过', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
