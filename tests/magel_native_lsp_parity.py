"""把既有 LSP 能力测试逐项转发给原生服务，防止迁移时损失编辑功能。"""
from pathlib import Path
import atexit
import json
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from magel_lsp import ApiIndex
import magel_lsp_api as suite


class NativeServer:
    """仅负责测试中的传输适配；所有编辑请求由真实 C++ 服务处理。"""
    def __init__(self,compiler):
        self.documents={}
        self.api_index=ApiIndex()
        self.sent={}
        self.serial=0
        self.child=subprocess.Popen([str(compiler),'lsp'],stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,
            env=dict(os.environ,MAGEL_PYTHON='__missing_python__'))
        atexit.register(self.close)

    def send(self,message):
        data=json.dumps(dict(jsonrpc='2.0',**message),ensure_ascii=False).encode('utf-8')
        self.child.stdin.write(f'Content-Length: {len(data)}\r\n\r\n'.encode()+data)
        self.child.stdin.flush()

    def query(self,method,params):
        for uri,document in self.documents.items():
            if self.sent.get(uri)==document['text']:continue
            version=document.get('version',1)
            if uri not in self.sent:
                self.send(dict(method='textDocument/didOpen',params={'textDocument':dict(uri=uri,text=document['text'],version=version)}))
            else:
                self.send(dict(method='textDocument/didChange',params={'textDocument':dict(uri=uri,version=version),'contentChanges':[{'text':document['text']}]}))
            self.sent[uri]=document['text']
        self.serial+=1
        self.send(dict(id=self.serial,method=method,params=params))
        while True:
            headers={}
            while True:
                line=self.child.stdout.readline()
                assert line, self.child.stderr.read().decode('utf-8',errors='replace')
                if line in (b'\r\n',b'\n'):break
                key,value=line.decode('ascii').split(':',1);headers[key.lower()]=value.strip()
            size=int(headers['content-length'])
            message=json.loads(self.child.stdout.read(size))
            if message.get('id')==self.serial:
                assert 'error' not in message,message
                return message['result']

    def close(self):
        if self.child.poll() is None:
            self.query('shutdown',{})
            self.send(dict(method='exit'))
            self.child.wait(timeout=10)
        self.child.stdin.close();self.child.stdout.close();self.child.stderr.close()


if __name__=='__main__':
    suite.LanguageServer=NativeServer
    suite.main()
    print('既有标准库、泛型、作用域、工程和未保存模块用例已在原生 LSP 通过。')
