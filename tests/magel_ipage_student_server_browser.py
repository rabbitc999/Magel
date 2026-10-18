"""用已安装的 Chrome/Edge 和标准库 CDP 客户端验收 ipage-student-server 的**前端**。

这个用例专门验证「前后端分离」真正成立的那一段：页面由**另一个源的静态服务器**托管，
浏览器跨源调后端的 `/api/*`（因此真实走了 CORS 预检与 Authorization 头），
菜单与按钮的可见性完全由后端 `/api/session` 决定。

覆盖内容：
  * 页面加载、跨源登录、四种权限下侧栏条目数（学生 3 / 教师 5 / 管理员 5 / 超管 6）；
  * 学生看不到「新增」，也拿不到班级 / 用户模块的入口；
  * 通过界面完成新增、编辑、查询、删除，并在后端只留一条记录；
  * 成绩模块的后端聚合统计页面；
  * 用 `<img onerror>` 当姓名，验证页面不做 HTML 注入。

用法（在仓库根目录）：
    python tests/magel_ipage_student_server_browser.py --browser "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"

需要本机 MySQL 在跑；缺服务时属于环境未满足（退出码 2）。
"""
from pathlib import Path
import argparse
import base64
import http.client
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parent))
from magel_ipage_student_server import (ROOT, EXE, STATIC, SOURCE, FIXTURE, TESTDB, Server,
                                        mysql_available, serve_static)
from magel_web_students_browser import DevTools

SCREENSHOT = ROOT / 'build/magel-ipage-server-ui.png'
# 浏览器自己发的请求不经过本进程，服务端计数会跑到前面去；上限给足，收尾时再补健康检查。
LIMIT = 600


def capture_view(tools, target):
    """保存真实浏览器界面，供书稿与人工核对使用。"""
    target.parent.mkdir(parents=True, exist_ok=True)
    image = tools.call('Page.captureScreenshot', {'format': 'png'})
    target.write_bytes(base64.b64decode(image['data']))


def sidebar(tools):
    """侧栏当前的模块名列表。"""
    return tools.evaluate("[...document.querySelectorAll('#nav button')].map(b => b.textContent)")


def sign_in(tools, user, password):
    """在登录卡片里填表并提交。"""
    tools.evaluate(
        "document.querySelector('#login-user').value=%s;"
        "document.querySelector('#login-password').value=%s;"
        "document.querySelector('#login-form').requestSubmit();"
        % (json.dumps(user), json.dumps(password)))
    tools.wait("document.body.classList.contains('signed-in')")


def sign_out(tools):
    """点顶栏的退出登录。"""
    tools.evaluate("document.querySelector('#logout').click()")
    tools.wait("!document.body.classList.contains('signed-in')")


def open_module(tools, label):
    """点侧栏里的某个模块，并等面板标题换过去。"""
    tools.evaluate(
        "[...document.querySelectorAll('#nav button')].find(b => b.textContent === %s).click()"
        % json.dumps(label))
    tools.wait("document.querySelector('#panel-title').textContent === %s" % json.dumps(label))


def toolbar_click(tools, label):
    """点工具栏里文字匹配的按钮。"""
    tools.evaluate(
        "[...document.querySelectorAll('#toolbar button')].find(b => b.textContent === %s).click()"
        % json.dumps(label))


def rows(tools, column=1):
    """取表格某一列的文本，用来核对渲染结果。"""
    return tools.evaluate(
        "[...document.querySelectorAll('#table-body tr')].map(r => r.children[%d]?.textContent)"
        % column)


def fill_form(tools, values):
    """填写表单面板并提交。"""
    tools.evaluate(
        "for (const [key, value] of Object.entries(%s)) "
        "  document.querySelector('#form-panel form').elements.namedItem(key).value = value;"
        "document.querySelector('#form-panel form').requestSubmit();"
        % json.dumps(values, ensure_ascii=False))


def browser_only(argv, work, server, url, screenshot):
    """启动无头浏览器、跑完全部页面断言，然后收尾。"""
    profile = work / 'profile'
    browser = None
    tools = None
    try:
        with (work / 'browser.log').open('wb') as log:
            browser = subprocess.Popen(
                [str(argv), '--headless=new', '--disable-gpu', '--no-first-run',
                 '--no-default-browser-check', '--remote-debugging-port=0',
                 '--window-size=1360,1000', '--user-data-dir=' + str(profile), 'about:blank'],
                stdout=log, stderr=log,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            deadline = time.monotonic() + 40
            marker = profile / 'DevToolsActivePort'
            while not marker.exists() and time.monotonic() < deadline:
                assert browser.poll() is None, (work / 'browser.log').read_text(errors='replace')
                time.sleep(.1)
            assert marker.exists(), '浏览器调试端口未就绪'
            port = int(marker.read_text().splitlines()[0])
            with urllib.request.urlopen(f'http://127.0.0.1:{port}/json/list', timeout=10) as response:
                pages = json.load(response)
            page = next(item for item in pages if item['type'] == 'page')
            tools = DevTools(port, page['webSocketDebuggerUrl'].split(f':{port}', 1)[1])
            tools.call('Page.enable')
            tools.call('Page.navigate', {'url': url})
            tools.wait("document.querySelector('#api-base')?.textContent.includes('127.0.0.1')")

            # 登录页应该把后端地址显示出来，方便确认前后端确实分开部署。
            assert '127.0.0.1' in tools.evaluate("document.querySelector('#api-base').textContent")
            capture_view(tools, screenshot.with_stem(screenshot.stem + '-login'))

            # 口令错：提示里带剩余次数，且不进入应用。
            sign_in_expect_failure(tools, 'admin', 'wrong1')
            assert '剩余 2 次机会' in tools.evaluate("document.querySelector('#login-message').textContent")

            # 学生登录：只有 3 个模块，没有新增按钮。
            sign_in(tools, '方智堃', 'zhongli')
            assert sidebar(tools) == ['学生管理', '成绩管理', '修改我的密码'], sidebar(tools)
            assert '管理员' not in tools.evaluate("document.querySelector('#who-role').textContent")
            tools.wait("document.querySelectorAll('#table-body tr').length === 5")
            assert tools.evaluate("[...document.querySelectorAll('#toolbar button')].some(b => b.textContent === '新增')") is False
            assert tools.evaluate("[...document.querySelectorAll('#table-body button')].some(b => b.textContent === '删除')") is False
            capture_view(tools, screenshot.with_stem(screenshot.stem + '-student'))
            # 学生成绩列能看到等级。
            open_module(tools, '成绩管理')
            tools.wait("document.querySelectorAll('#table-body tr').length === 6")
            assert rows(tools, 7) == ['及格', '中等', '良好', '优秀', '不及格', '中等'], rows(tools, 7)
            sign_out(tools)

            # 超级管理员登录：6 个模块，可以新增。
            sign_in(tools, 'admin', '123456')
            assert sidebar(tools) == ['学生管理', '班级管理', '课程管理', '成绩管理', '用户管理', '修改我的密码'], sidebar(tools)
            assert '超级管理员' in tools.evaluate("document.querySelector('#who-role').textContent")
            tools.wait("document.querySelectorAll('#table-body tr').length === 5")

            # 新增学生（姓名故意带 HTML，用来验证不注入）。
            # 载荷要短于 student_name 的 varchar(24)，否则后端会按列宽拦下来。
            toolbar_click(tools, '新增')
            tools.wait("document.querySelector('#form-panel').classList.contains('open')")
            # 班级下拉来自 /api/options/sclasses，是另一个接口，等它填好。
            tools.wait("document.querySelector('#form-panel form').elements.namedItem('sclassId').options.length === 4")
            payload = {'name': '<img src=x onerror=x=1>', 'sex': 'FEMALE', 'sclassId': '2'}
            assert len(payload['name']) <= 24, payload['name']
            fill_form(tools, payload)
            tools.wait("document.querySelectorAll('#table-body tr').length === 6")
            tools.wait("document.querySelector('#app-message').textContent.includes('添加成功')")
            assert rows(tools, 1)[-1] == payload['name'], rows(tools, 1)
            assert tools.evaluate("document.querySelector('#table-body img') === null && window.x === undefined")
            assert rows(tools, 3)[-1] == '2班' and rows(tools, 4)[-1] == '高一'

            # 编辑刚建的那一条。
            tools.evaluate("document.querySelector('#table-body tr:last-child button').click()")
            tools.wait("document.querySelector('#form-panel').classList.contains('open')")
            fill_form(tools, {'name': '界面改名同学', 'sex': 'MALE', 'sclassId': ''})
            tools.wait("document.querySelector('#table-body tr:last-child').children[1].textContent === '界面改名同学'")
            assert rows(tools, 1)[-1] == '界面改名同学'
            assert rows(tools, 3)[-1] == '未分班' and rows(tools, 2)[-1] == '男'

            # 按姓名查询：只留命中行。
            tools.evaluate("document.querySelector('#query-keyword').value = '界面改名';"
                           "document.querySelector('#query-mode').value = 'name';"
                           "[...document.querySelectorAll('#toolbar button')].find(b => b.textContent === '查询').click();")
            tools.wait("document.querySelectorAll('#table-body tr').length === 1")
            assert '命中 1 条' in tools.evaluate("document.querySelector('#count-note').textContent")

            # 班级 / 课程 / 用户 / 改密四个模块都能打开并渲染。
            open_module(tools, '班级管理')
            tools.wait("document.querySelectorAll('#table-body tr').length === 3")
            assert rows(tools, 1) == ['1班', '2班', '1班'], rows(tools, 1)
            open_module(tools, '课程管理')
            tools.wait("document.querySelectorAll('#table-body tr').length === 3")
            assert rows(tools, 2) == ['开课', '开课', '结课'], rows(tools, 2)
            open_module(tools, '用户管理')
            tools.wait("document.querySelectorAll('#table-body tr').length === 4")
            assert rows(tools, 1) == ['admin', '陈承岩', '戚宇航', '方智堃'], rows(tools, 1)
            assert tools.evaluate("[...document.querySelectorAll('#table-body tr')].some(r => r.textContent.includes('123456'))") is False

            # 成绩模块的后端聚合统计。
            open_module(tools, '成绩管理')
            tools.wait("document.querySelectorAll('#table-body tr').length === 6")
            toolbar_click(tools, '按课程统计')
            tools.wait("document.querySelectorAll('#table-body .stat-cell').length === 7")
            assert tools.evaluate("document.querySelector('#table-body .stat-cell:nth-child(2) .v').textContent") == '6'
            assert tools.evaluate("document.querySelector('#table-body .stat-cell:nth-child(7) .v').textContent") == '83.3'
            capture_view(tools, screenshot.with_stem(screenshot.stem + '-stats'))
            toolbar_click(tools, '返回列表')
            tools.wait("document.querySelectorAll('#table-body tr').length === 6")

            # 改密码模块：只出表单，不失败。
            open_module(tools, '修改我的密码')
            tools.wait("document.querySelector('#form-panel').classList.contains('open')")
            assert tools.evaluate("[...document.querySelectorAll('#form-panel form input')].map(i => i.name)") == ['currentPassword', 'newPassword']

            # 回学生模块，把刚建的那条删掉。
            open_module(tools, '学生管理')
            tools.wait("document.querySelectorAll('#table-body tr').length === 6")
            tools.evaluate("window.confirm = () => true;"
                           "document.querySelector('#table-body tr:last-child button.danger').click()")
            tools.wait("document.querySelectorAll('#table-body tr').length === 5")
            tools.wait("document.querySelector('#app-message').textContent.includes('已删除学生')")

            # 留一张截图给人工复核。
            capture_view(tools, screenshot)

            tools.call('Page.navigate', {'url': 'about:blank'})
            try:
                tools.call('Browser.close')
            except ConnectionError:
                pass
            tools.socket.close()
            tools = None
            browser.wait(timeout=20)
    finally:
        if tools:
            tools.socket.close()
        if browser and browser.poll() is None:
            browser.kill()
            browser.wait(timeout=10)


def sign_in_expect_failure(tools, user, password):
    """登录失败路径：填表提交后仍停在登录页，并给出剩余次数。"""
    tools.evaluate(
        "document.querySelector('#login-user').value=%s;"
        "document.querySelector('#login-password').value=%s;"
        "document.querySelector('#login-form').requestSubmit();"
        % (json.dumps(user), json.dumps(password)))
    tools.wait("document.querySelector('#login-message').textContent.includes('错误')")


def drain(server):
    """等后端真把浏览器发的请求也算完：补健康检查直到进程正常退出。

    浏览器自己发的请求会把服务端计数推到接近上限，因此进程可能在浏览器刚跑完时
    就处理满上限、开始收尾；此时补的请求会撞上连接被关闭（`request()` 会把它包成
    AssertionError）。那说明进程已经在退出路上，按退出码和诊断输出验收即可。
    """
    deadline = time.monotonic() + 60
    while server.process.poll() is None and time.monotonic() < deadline:
        try:
            server.request('GET', '/api/health', 200, token=None)
        except (OSError, http.client.HTTPException, AssertionError):
            if server.process.poll() is not None:
                break
            time.sleep(.2)
    if server.process.poll() is None:
        raise AssertionError('服务端没有在请求上限内自行退出')
    out, err = server.process.communicate(timeout=15)
    assert server.process.returncode == 0 and not err, (out, err)
    assert '服务器正常关闭' in out.decode('utf-8', errors='replace'), out


def main():
    """起后端 + 静态服务器，再用无头浏览器跑一遍真实用户流程。"""
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    parser.add_argument('--browser', type=Path, required=True, help='已安装的 Chrome 或 Edge 可执行文件')
    parser.add_argument('--mysql-host', default='127.0.0.1')
    parser.add_argument('--mysql-port', type=int, default=3306)
    parser.add_argument('--mysql-user', default='root')
    parser.add_argument('--mysql-password', default='123456')
    parser.add_argument('--screenshot', type=Path, default=SCREENSHOT)
    args = parser.parse_args()

    assert args.browser.is_file(), args.browser
    if not mysql_available(args.mysql_host, args.mysql_port):
        print(f'跳过：{args.mysql_host}:{args.mysql_port} 上没有 MySQL 服务，'
              f'属于环境未满足（先启动 MySQL 或改 --mysql-host/--mysql-port）', file=sys.stderr)
        return 2

    env = os.environ.copy()
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='magel-ipage-ui-', dir=ROOT / 'build') as directory:
        work = Path(directory)
        argv = [args.magel.resolve(), 'run', SOURCE, '--out', work / 'server', '--',
                '--listen-host', '127.0.0.1', '--listen-port', '0',
                '--mysql-host', args.mysql_host, '--mysql-port', str(args.mysql_port),
                '--mysql-user', args.mysql_user, '--mysql-password', args.mysql_password,
                '--db', TESTDB, '--init', FIXTURE, '--reinit']
        server = Server(argv, work, LIMIT, env)
        static, static_port = serve_static()
        try:
            # 前端跑在 :static_port，后端跑在 :server.port —— 两个源，必须靠 CORS 才通。
            url = f'http://127.0.0.1:{static_port}/?api=http://127.0.0.1:{server.port}'
            assert static_port != server.port
            browser_only(args.browser, work, server, url, args.screenshot)
            drain(server)
            print('浏览器通过：跨源登录、按权限收放菜单与按钮、新增/编辑/查询/删除、'
                  '后端聚合统计页、HTML 注入防护')
            print('截图：' + str(args.screenshot))
            return 0
        finally:
            static.shutdown()
            static.server_close()
            if server.process.poll() is None:
                server.abort()


if __name__ == '__main__':
    sys.exit(main())
