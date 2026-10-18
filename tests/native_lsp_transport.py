"""原生 LSP 的异步测试传输；分别观察响应与诊断，所有读取有超时。"""
from pathlib import Path
import json
import atexit
import os
import queue
import subprocess
import threading
import time


class Transport:
    """后台读取 JSON-RPC 帧，允许多个不同类别请求同时在途。"""
    def __init__(self, compiler, cwd=None):
        self.child = subprocess.Popen([str(compiler), 'lsp'], cwd=cwd, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=dict(os.environ, MAGEL_LANG='zh', MAGEL_PYTHON='__missing_python__'))
        self.messages = queue.Queue()
        self.responses = {}
        self.notifications = []
        self.serial = 0
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()
        atexit.register(self._kill)

    def _kill(self):
        """测试失败时结束子进程，避免残留运行中的临时编译器。"""
        if self.child.poll() is None:
            self.child.kill()
            self.child.wait(timeout=10)

    def _read(self):
        """读取完整帧，进程退出时唤醒等待方。"""
        try:
            while True:
                headers = {}
                while True:
                    line = self.child.stdout.readline()
                    if not line:
                        self.messages.put(None)
                        return
                    if line in (b'\r\n', b'\n'):
                        break
                    key, value = line.decode('ascii').split(':', 1)
                    headers[key.lower()] = value.strip()
                length = int(headers['content-length'])
                payload = self.child.stdout.read(length)
                assert len(payload) == length
                self.messages.put(json.loads(payload))
        except BaseException as error:
            self.messages.put(error)

    def send(self, message):
        """提交请求或通知，不等待服务器。"""
        if isinstance(message.get('id'), int): self.serial = max(self.serial, message['id'])
        payload = json.dumps(dict(jsonrpc='2.0', **message), ensure_ascii=False).encode('utf-8')
        self.child.stdin.write(f'Content-Length: {len(payload)}\r\n\r\n'.encode() + payload)
        self.child.stdin.flush()

    def _next(self, deadline):
        """接收下一帧，并按 ID 保存响应。"""
        value = self.messages.get(timeout=max(0.001, deadline - time.monotonic()))
        assert value is not None and not isinstance(value, BaseException), value
        if 'id' in value:
            self.responses[value['id']] = value
        else:
            self.notifications.append(value)
        return value

    def response(self, serial, timeout=30):
        """等待特定响应，同时记录期间收到的其他帧。"""
        deadline = time.monotonic() + timeout
        while serial not in self.responses:
            self._next(deadline)
        return self.responses[serial]

    def query(self, method, params=None, timeout=30):
        """发送一个请求并取得成功结果。"""
        self.serial += 1
        serial = self.serial
        self.send(dict(id=serial, method=method, params=params or {}))
        response = self.response(serial, timeout)
        assert 'error' not in response, response
        return response['result']

    def diagnostic(self, uri, version=None, predicate=lambda _: True, after=0, timeout=30):
        """等待指定文档和版本的诊断。"""
        deadline = time.monotonic() + timeout
        cursor = after
        while True:
            while cursor < len(self.notifications):
                event = self.notifications[cursor]
                cursor += 1
                params = event.get('params', {})
                if event.get('method') == 'textDocument/publishDiagnostics' and params.get('uri') == uri:
                    if (version is None or params.get('version') == version) and predicate(params.get('diagnostics', [])):
                        return params
            self._next(deadline)

    def close(self):
        """正常关闭，并确认工作线程完成协议退出。"""
        if self.child.poll() is None:
            self.query('shutdown')
            self.send(dict(method='exit'))
            self.child.wait(timeout=30)
        assert self.child.returncode == 0, self.child.stderr.read().decode('utf-8', errors='replace')
        self.reader.join(timeout=2)
        self.child.stdin.close(); self.child.stdout.close(); self.child.stderr.close()


def position(text, offset):
    """字节以外的编辑器位置按 UTF-16 计算。"""
    before = text[:offset]
    return dict(line=before.count('\n'), character=len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2)
