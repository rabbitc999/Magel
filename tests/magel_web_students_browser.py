"""使用已安装的 Chrome/Edge 和标准库 CDP 客户端验证学生管理页面，无第三方 Python 依赖。"""
from pathlib import Path
import argparse
import base64
import hashlib
import http.client
import json
import os
import socket
import sqlite3
import struct
import subprocess
import tempfile
import time
import urllib.request
from contextlib import closing
from magel_web_students import ROOT, EXAMPLE, EXE, Server, sqlite_environment


class DevTools:
    """实现测试需要的 WebSocket 帧和顺序 CDP 命令。"""

    def __init__(self, port, path):
        """建立本机调试通道并校验 WebSocket 握手。"""
        self.socket = socket.create_connection(('127.0.0.1', port), timeout=15)
        self.sequence = 0
        key = base64.b64encode(os.urandom(16)).decode()
        self.socket.sendall((f'GET {path} HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n'
                             f'Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n'
                             'Sec-WebSocket-Version: 13\r\n\r\n').encode())
        head = b''
        while not head.endswith(b'\r\n\r\n'):
            head += self.read(1)
        expected = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
        assert head.startswith(b'HTTP/1.1 101') and expected in head, head

    def read(self, count):
        """精确读取帧数据，关闭连接时明确失败。"""
        out = b''
        while len(out) < count:
            chunk = self.socket.recv(count - len(out))
            if not chunk:
                raise ConnectionError('浏览器调试连接关闭')
            out += chunk
        return out

    def send(self, payload, opcode=1):
        """发送客户端掩码帧，支持控制帧和长消息。"""
        size = len(payload)
        head = bytes([0x80 | opcode])
        if size < 126:
            head += bytes([0x80 | size])
        elif size < 65536:
            head += bytes([0x80 | 126]) + struct.pack('!H', size)
        else:
            head += bytes([0x80 | 127]) + struct.pack('!Q', size)
        mask = os.urandom(4)
        self.socket.sendall(head + mask + bytes(value ^ mask[i % 4] for i, value in enumerate(payload)))

    def message(self):
        """读取完整消息，处理分帧及 ping，忽略无关通知交给调用方。"""
        result = b''
        while True:
            first, second = self.read(2)
            length = second & 127
            if length == 126:
                length = struct.unpack('!H', self.read(2))[0]
            if length == 127:
                length = struct.unpack('!Q', self.read(8))[0]
            assert length <= 16 * 1024 * 1024
            mask = self.read(4) if second & 128 else None
            payload = self.read(length)
            if mask:
                payload = bytes(value ^ mask[i % 4] for i, value in enumerate(payload))
            opcode = first & 15
            if opcode == 8:
                raise ConnectionError('浏览器关闭 WebSocket')
            if opcode == 9:
                self.send(payload, 10)
                continue
            if opcode in (0, 1):
                result += payload
                if first & 128:
                    return json.loads(result)

    def call(self, method, params=None):
        """发送一条 CDP 命令并等待对应响应。"""
        self.sequence += 1
        self.send(json.dumps({'id': self.sequence, 'method': method, 'params': params or {}}).encode())
        while True:
            message = self.message()
            if message.get('id') == self.sequence:
                assert 'error' not in message, message
                return message.get('result', {})

    def evaluate(self, expression):
        """运行测试脚本并拒绝 JavaScript 异常。"""
        result = self.call('Runtime.evaluate', {'expression': expression, 'awaitPromise': True, 'returnByValue': True})
        assert 'exceptionDetails' not in result, result
        return result.get('result', {}).get('value')

    def wait(self, expression):
        """等待页面异步网络操作完成；不以固定延迟代替断言。"""
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if self.evaluate(expression):
                return
            time.sleep(.1)
        raise AssertionError('页面条件未满足：' + expression)


def main():
    """启动隔离服务器和无头浏览器，验证页面增改查删及防止 HTML 注入。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    parser.add_argument('--browser', type=Path, required=True, help='已安装的 Chrome 或 Edge 可执行文件')
    args = parser.parse_args()
    assert args.browser.is_file(), args.browser
    env = sqlite_environment()
    screenshot = ROOT / 'build/magel-web-students-browser.png'
    with tempfile.TemporaryDirectory(prefix='magel-web-ui-', dir=ROOT / 'build') as directory:
        work = Path(directory)
        database = work / 'students.db'
        command = [args.magel.resolve(), 'run', EXAMPLE / 'main.mgl', '--out', work / 'server', '--']
        server = Server(command, work, database, 60, env)
        profile = work / 'profile'
        browser = None
        tools = None
        try:
            with (work / 'browser.log').open('wb') as log:
                browser = subprocess.Popen([str(args.browser), '--headless=new', '--disable-gpu', '--no-first-run',
                                            '--no-default-browser-check', '--remote-debugging-port=0',
                                            '--window-size=1280,1000', '--user-data-dir=' + str(profile), 'about:blank'],
                                           stdout=log, stderr=log,
                                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                deadline = time.monotonic() + 30
                marker = profile / 'DevToolsActivePort'
                while not marker.exists() and time.monotonic() < deadline:
                    assert browser.poll() is None, (work / 'browser.log').read_text(errors='replace')
                    time.sleep(.1)
                assert marker.exists(), '浏览器调试端口未就绪'
                port = int(marker.read_text().splitlines()[0])
                with urllib.request.urlopen(f'http://127.0.0.1:{port}/json/list', timeout=10) as response:
                    pages = json.load(response)
                page = next(item for item in pages if item['type'] == 'page')
                path = page['webSocketDebuggerUrl'].split(f':{port}', 1)[1]
                tools = DevTools(port, path)
                tools.call('Page.enable')
                tools.call('Page.navigate', {'url': f'http://127.0.0.1:{server.port}/'})
                tools.wait("document.querySelector('#count')?.textContent === '(0 / 0)'")
                # 浏览器产生的 HTTP 请求由 SQL 断言及页面断言验证；退出时换用独立计数方式。
                student = {'studentNo': 'UI-001', 'name': '<img src=x onerror=window.xss=1>', 'email': 'ui@example.test', 'age': '19'}
                fill = "for(const [key,value] of Any.entries(%s)) document.querySelector('#student-form').elements.namedItem(key).value=value;document.querySelector('#student-form').requestSubmit();"
                tools.evaluate(fill % json.dumps(student))
                tools.wait("document.querySelector('#message').textContent === '学生信息已保存。'")
                assert tools.evaluate("document.querySelector('#students tr td:nth-child(2)').textContent") == student['name']
                assert tools.evaluate("document.querySelector('#students img') === null && window.xss === undefined")
                tools.evaluate(fill % json.dumps(student))
                tools.wait("document.querySelector('#message').classList.contains('error')")
                assert '学号已存在' in tools.evaluate("document.querySelector('#message').textContent")
                tools.evaluate("document.querySelector('#students .secondary').click();document.querySelector('[name=name]').value='李明';document.querySelector('#student-form').requestSubmit();")
                tools.wait("document.querySelector('#students tr td:nth-child(2)').textContent === '李明'")
                tools.evaluate("document.querySelector('#search').value='不存在';document.querySelector('#search').dispatchEvent(new Event('input'));")
                assert tools.evaluate("document.querySelector('#students').children.length === 0 && !document.querySelector('#empty').hidden")
                tools.evaluate("document.querySelector('#search').value='';document.querySelector('#search').dispatchEvent(new Event('input'));")
                image = tools.call('Page.captureScreenshot', {'format': 'png'})
                screenshot.write_bytes(base64.b64decode(image['data']))
                tools.evaluate("window.confirm=()=>true;document.querySelector('#students .danger').click();")
                tools.wait("document.querySelector('#message').textContent === '学生已删除。'")
                assert tools.evaluate("document.querySelector('#students').children.length === 0")
                with closing(sqlite3.connect(database)) as connection:
                    assert connection.execute('SELECT COUNT(*) FROM students').fetchone() == (0,)
                # 用受控的测试请求上限结束服务：读取浏览器 resource timing 不能准确统计所有请求，
                # 因此发送健康检查直到服务器自然退出，并以正常退出及空诊断验收。
                tools.call('Page.navigate', {'url': 'about:blank'})
                try:
                    tools.call('Browser.close')
                except ConnectionError:
                    pass
                tools.socket.close()
                browser.wait(timeout=15)
                while server.process.poll() is None:
                    try:
                        server.request('GET', '/api/health', 200)
                    except (OSError, http.client.HTTPException):
                        if server.process.wait(timeout=10) != 0:
                            raise
                        break
                out, err = server.process.communicate(timeout=10)
                assert server.process.returncode == 0 and not err, (out, err)
            print('浏览器通过：页面加载、新增、重复学号提示、编辑、搜索、删除、HTML 注入防护、真实 SQL 检查')
            print('截图：' + str(screenshot))
        finally:
            if tools:
                tools.socket.close()
            if browser and browser.poll() is None:
                browser.kill()
                browser.wait(timeout=10)
            if server.process.poll() is None:
                server.abort()


if __name__ == '__main__':
    main()
