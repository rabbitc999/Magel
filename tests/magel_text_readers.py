"""验证行输入及文件文本接口的 VM、AOT、错误与边界行为。"""
from pathlib import Path
import json
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
COMPILER=ROOT/('magel.exe' if os.name=='nt' else 'magel')


def main():
    """真实 stdin 保留空格和空行；文件预算、零字节与关闭错误沿用 Result。"""
    with tempfile.TemporaryDirectory(prefix='magel-text-readers-') as folder:
        work=Path(folder)
        env=dict(os.environ,MAGEL_LANG='zh',MAGEL_NO_STDIN_INPUT='0')
        def run(code,stdin='',arguments=(),options=(),aot=False):
            file=work/'main.mgl';file.write_text(code,encoding='utf-8')
            command=[str(COMPILER),'run',str(file),'--out',str(work/'output'),*options]
            if aot:
                executable=work/('program.exe' if os.name=='nt' else 'program')
                built=subprocess.run([str(COMPILER),'aot',str(file),'out',str(executable)],cwd=work,env=env,capture_output=True,encoding='utf-8',timeout=180)
                assert built.returncode==0,built.stderr
                command=[str(executable)]
            if arguments:command+=['--',*map(str,arguments)] if not aot else list(map(str,arguments))
            return subprocess.run(command,input=stdin,cwd=work,env=env,capture_output=True,encoding='utf-8',timeout=60)
        code=(ROOT/'example/samples/std_read_lines.mgl').read_text(encoding='utf-8')
        expected='[hello world]\n[\n中文 空格\nlast]\n'
        for aot in (False,True):
            result=run(code,'hello world\r\n\r\n中文 空格\r\nlast',aot=aot)
            assert result.returncode==0 and result.stdout.replace('\r\n','\n')==expected,(result.stdout,result.stderr)
            result=run('int64 main(){assert(readLines(0)=="");assert(readLines(9)=="one\\n\\ntwo");assert(readLine()=="");return 0;}','one\n\ntwo',aot=aot)
            assert result.returncode==0,result.stderr
            result=run((ROOT/'example/samples/std_file_strings.mgl').read_text(encoding='utf-8'),arguments=[work/'file.txt'],aot=aot)
            assert result.returncode==0 and result.stdout.strip()=='file strings ok',result.stderr
        result=run('int64 main(){assert(readLine()=="one");assert(readLines(2)=="two\\nthree");return 0;}',options=['--input','one two three'])
        assert result.returncode==0,result.stderr
        result=run('int64 main(){readLines(-1);return 0;}')
        assert result.returncode!=0 and 'nonnegative' in result.stderr,result.stderr
        (work/'raw.bin').write_bytes(b'ab\x00cd')
        (work/'exact.txt').write_bytes(b'abcd')
        code='''import std::io;
int64 main(){
 FileStream exact=FileStream::open("exact.txt","rb").unwrap();
 assert(exact.readAllString(4).unwrap()=="abcd");exact.close();
 FileStream short=FileStream::open("exact.txt","rb").unwrap();assert(short.readAllString(3).isErr());short.close();
 FileStream zero=FileStream::open("raw.bin","rb").unwrap();assert(zero.readString(5).isErr());zero.close();
 FileStream zeroLine=FileStream::open("raw.bin","rb").unwrap();assert(zeroLine.readLine().isErr());zeroLine.close();
 FileStream bad=FileStream::open("exact.txt","rb").unwrap();assert(bad.readString(-1).isErr());bad.close();assert(bad.readLine().isErr());
 return 0;
}'''
        result=run(code);assert result.returncode==0,result.stderr
        result=subprocess.run([str(COMPILER),'man','api','--json','std::lang::readLines'],capture_output=True,encoding='utf-8')
        item=json.loads(result.stdout)['symbols'][0]
        assert item['parameters'][0]['name']=='N' and '非负' in item['parameters'][0]['documentation'],item
    print('行输入与文件文本读取 VM/AOT、CRLF、空行、EOF、零读取、预算、零字节及手册通过。')


if __name__=='__main__':main()
