"""无 Python 运行环境下的原生开发命令、普通 LSP 宏提示和真实展开预览回归。"""
from pathlib import Path
import json
import os
import subprocess
import tempfile
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')


def main():
    """禁用解释器并用真实 JSON-RPC 校验全部迁移入口。"""
    with tempfile.TemporaryDirectory(prefix='原生开发工具 ') as directory:
        root = Path(directory)
        env = dict(os.environ, MAGEL_PYTHON=str(root / 'missing-python.exe'))
        # 运行时 PATH 去掉 Python 与 Node，只保留系统工具；测试驱动本身仍用 Python。
        if os.name == 'nt':
            runtime=Path(shutil.which('g++')).parent
            assert not (runtime/'python.exe').exists()
            env['PATH'] = os.pathsep.join([str(Path(os.environ['SystemRoot'])/'System32'),str(runtime)])
        else:
            env['PATH'] = '/usr/bin:/bin'
        assert shutil.which('python',path=env['PATH']) is None
        assert shutil.which('python3',path=env['PATH']) is None
        def run(*args, code=0, **kwargs):
            result = subprocess.run([str(COMPILER), *map(str,args)], cwd=root, env=env,
                capture_output=True, timeout=90, **kwargs)
            assert result.returncode == code, (args,result.stdout,result.stderr)
            return result
        (root/'magel.toml').write_text('[package]\nname="native"\ncompiler=">=0.9.1"\n',encoding='utf-8')
        run('deps','lock','--offline')
        run('deps','check','--offline')
        source = root/'main.mgl'
        source.write_text('int64 main(){\nreturn 0;\n}\n',encoding='utf-8')
        run('fmt',source,'--check',code=1)
        run('fmt',source,'--write');run('fmt',source,'--check')
        test=root/'test_pass.mgl'
        test.write_text('int64 main(){std::lang::println("通过");return 0;}',encoding='utf-8')
        test.with_suffix('.expected').write_text('通过\n',encoding='utf-8')
        skipped=root/'test_skip.mgl';skipped.write_text('invalid',encoding='utf-8')
        skipped.with_suffix('.test.toml').write_text('skip="测试跳过"\n',encoding='utf-8')
        if '--aot' in sys.argv:
            report=json.loads(run('test',test,'--backend','all','--timeout','90','--json').stdout)
            assert report['summary']==dict(passed=1,failed=0,skipped=0),report
        report=json.loads(run('test',root,'--json').stdout)
        assert report['summary']==dict(passed=1,failed=0,skipped=1),report
        timed=root/'test_timeout.mgl';timed.write_text('int64 main(){while(true){}return 0;}',encoding='utf-8')
        report=json.loads(run('test',timed,'--timeout','0.1','--json',code=1).stdout)
        assert 'timed out' in report['tests'][0]['message'],report
        module=root/'calc.mgl'
        module.write_text('module calc;\n/// 整数文档\nint64 add(int64 a,int64 b=1){return a+b;}\n/// 浮点文档\nfloat64 add(float64 a,float64 b){return a+b;}\nstruct Box { /// 字段文档\nint64 value; };\n',encoding='utf-8')
        included=root/'macros.mgl'
        included.write_text('/// 包含宏文档\n$macro TWICE(x)\n((x)+(x))\n$end_macro\n',encoding='utf-8')
        text='''import calc;
import std::math;
%include "macros.mgl"
/// 平方宏文档
$macro expr SQUARE((x))
x*x
$end_macro
/// 单行宏文档
$replace expr PLUS(x,(y)) x+y
$macro ANSWER()
3
$end_macro
int64 main(){
    String emoji="😀";
    calc::Box box;
    box.value;
    calc::add(1,2);
    std::math::sqrt(4.0);
    int64 answer=$SQUARE($TWICE(2))+$PLUS(1,2)+$ANSWER;
    return answer;
}
'''
        source.write_text(text,encoding='utf-8')
        def position(at):
            return dict(line=text.count('\n',0,at),character=len(text[text.rfind('\n',0,at)+1:at].encode('utf-16-le'))//2)
        requests=[dict(id=1,method='initialize',params={'rootUri':root.as_uri()}),
            dict(method='textDocument/didOpen',params={'textDocument':dict(uri=source.as_uri(),text=text,version=1)})]
        def query(id,method,needle='',delta=0):
            params={'textDocument':{'uri':source.as_uri()}}
            if needle:params['position']=position(text.index(needle)+delta)
            requests.append(dict(id=id,method=method,params=params))
        query(2,'textDocument/hover','calc::add',8)
        query(3,'textDocument/signatureHelp','calc::add(1,',12)
        query(4,'textDocument/completion','box.',4)
        query(5,'textDocument/definition','box.value',7)
        query(6,'textDocument/hover','$SQUARE($TWICE',2)
        query(7,'textDocument/signatureHelp','$SQUARE($TWICE',len('$SQUARE('))
        query(8,'textDocument/definition','$TWICE(2)',2)
        query(9,'magel/expandMacro')
        query(10,'textDocument/semanticTokens/full')
        query(11,'textDocument/documentSymbol')
        query(12,'textDocument/hover','sqrt',2)
        query(14,'textDocument/hover','$ANSWER;',3)
        query(15,'textDocument/signatureHelp','$PLUS(1,',len('$PLUS(1,'))
        requests.extend([dict(id=13,method='shutdown'),dict(method='exit')])
        payload=b''
        for request in requests:
            raw=json.dumps(dict(jsonrpc='2.0',**request),ensure_ascii=False).encode()
            payload+=f'Content-Length: {len(raw)}\r\n\r\n'.encode()+raw
        result=run('lsp',input=payload)
        frames=result.stdout;responses={}
        while frames:
            header,_,rest=frames.partition(b'\r\n\r\n')
            assert _,frames[:300]
            size=int(header.split(b':',1)[1]);message=json.loads(rest[:size]);frames=rest[size:]
            if 'id' in message:responses[message['id']]=message
        def value(id):
            assert 'error' not in responses[id],responses[id]
            return responses[id]['result']
        assert value(1)['serverInfo']['version'] == (ROOT/'VERSION').read_text().strip(), value(1)
        assert '整数文档' in value(2)['contents']['value'] and '浮点文档' in value(2)['contents']['value']
        assert value(3)['activeParameter']==1 and len(value(3)['signatures'])==2,value(3)
        assert any(item['label']=='value' for item in value(4)),value(4)
        assert value(5)[0]['uri']==module.as_uri(),value(5)
        assert '平方宏文档' in value(6)['contents']['value'],value(6)
        assert value(7)['signatures'][0]['parameters'][0]['label']=='(x)',value(7)
        assert value(8)[0]['uri']==included.as_uri(),value(8)
        assert value(9)['diagnostics']==[],value(9)
        expanded=value(9)['text']
        assert 'SQUARE(' not in expanded and 'TWICE(' not in expanded and '(2)' in expanded,expanded[-1000:]
        assert '$macro ANSWER()' in value(14)['contents']['value']
        assert value(15)['activeParameter']==1 and 'PLUS' in value(15)['signatures'][0]['label']
        assert value(10)['data'] and any(item['name']=='SQUARE' for item in value(11))
        assert 'sqrt' in value(12)['contents']['value']
        # 预览输出必须能够交给真实前端编译；其执行结果应与原文件一致。
        expandedFile=root/'expanded.mgl';expandedFile.write_text(expanded.rsplit('module __root;',1)[1],encoding='utf-8')
        run('--check',expandedFile)
    print('原生 LSP、宏提示/展开、格式化、依赖锁定及测试/超时全部通过，无外部 Python。')


if __name__=='__main__':main()
