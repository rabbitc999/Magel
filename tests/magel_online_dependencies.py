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

ROOT=Path(__file__).resolve().parents[1]


def archive(files,kind='zip',prefix=''):
    """在内存中构造小型 ZIP 或 TAR 包，供本地服务下载。"""
    output=io.BytesIO()
    if kind=='zip':
        with zipfile.ZipFile(output,'w') as package:
            for name,text in files.items():package.writestr(prefix+name,text)
    else:
        with tarfile.open(fileobj=output,mode='w:gz') as package:
            for name,text in files.items():
                data=text.encode('utf-8');member=tarfile.TarInfo(prefix+name);member.size=len(data)
                package.addfile(member,io.BytesIO(data))
    return output.getvalue()


def main():
    """下载三层菱形依赖；关闭服务器后复测编译、AOT、LSP 与内容变更诊断。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=ROOT/('magel.exe' if os.name=='nt' else 'magel'))
    parser.add_argument('--unit-only',action='store_true')
    args=parser.parse_args()
    payloads={};requests={}
    class Handler(http.server.BaseHTTPRequestHandler):
        """只服务测试包，记录每个 URL 的实际下载次数。"""
        def do_GET(self):
            requests[self.path]=requests.get(self.path,0)+1
            if self.path not in payloads:self.send_error(404);return
            data=payloads[self.path]
            self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers()
            self.wfile.write(data)
        def log_message(self,*args):pass
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    url=f'http://127.0.0.1:{server.server_port}'
    def publish(name,files,kind='zip',prefix=''):
        data=archive(files,kind,prefix);payloads['/'+name]=data
        return url+'/'+name+'#sha256='+hashlib.sha256(data).hexdigest()
    def config(name,dependencies=''):
        return '[package]\nname="'+name+'"\nversion="0.1.0"\n'+('[dependencies]\n'+dependencies if dependencies else '')
    try:
        (ROOT/'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='magel-online-deps-') as folder, tempfile.TemporaryDirectory(prefix='online-toolchain-',dir=ROOT/'build') as compiler_folder:
            work=Path(folder);toolchain=Path(compiler_folder)/'generator';toolchain.mkdir()
            compiler=toolchain/args.magel.name
            if not args.unit_only:shutil.copy2(args.magel.resolve(),compiler)
            app=work/'app';app.mkdir()
            leaf=publish('leaf.zip',{'magel.toml':config('leaf'),'modules/lib/leaf.mgl':'module lib::leaf;\n/// 叶子依赖函数。\nint64 value(){return 21;}\n'})
            middle=publish('middle.tar.gz',{'magel.toml':config('middle','leaf='+json.dumps(leaf)+'\n'),'modules/lib/middle.mgl':'module lib::middle;\nimport lib::leaf;\nint64 value(){return lib::leaf::value()+1;}\n'},'tar')
            top=publish('top.zip',{'magel.toml':config('top','middle='+json.dumps(middle)+'\nleaf='+json.dumps(leaf)+'\n'),'modules/lib/top.mgl':'module lib::top;\nimport lib::middle;\nimport lib::leaf;\nint64 value(){return lib::middle::value()+lib::leaf::value();}\n'},prefix='top-0.1.0/')
            (app/'magel.toml').write_text(config('app','top='+json.dumps(top)+'\n'),encoding='utf-8')
            source=app/'main.mgl';source.write_text('import lib::top;\nint64 main(){std::lang::println("{}",lib::top::value());return 0;}\n',encoding='utf-8')
            env=dict(os.environ,MAGEL_LANG='zh')
            def run(*command,success=True):
                result=subprocess.run(list(map(str,command)),cwd=work,env=env,capture_output=True,
                                      text=True,encoding='utf-8',errors='replace',timeout=240)
                assert (result.returncode==0)==success,(command,result.stdout,result.stderr)
                return result
            def deps(action,project=app,offline=False,success=True):
                command=[compiler] if not args.unit_only else [sys.executable,ROOT/'tools/mageldev.py','--magel',compiler]
                return run(*command,'deps',action,'--project',project,*(['--offline'] if offline else []),success=success)
            deps('lock',offline=True,success=False)
            assert not requests
            deps('lock')
            assert requests=={'/top.zip':1,'/leaf.zip':1,'/middle.tar.gz':1},requests
            libraries=toolchain/'libraries'
            assert len(list(libraries.iterdir()))==3
            assert not (app/'.magel/deps').exists()
            deps('check',offline=True)
            # 第二个项目复用同一生成器缓存，不再下载。
            other=work/'other';other.mkdir()
            (other/'magel.toml').write_text((app/'magel.toml').read_text(encoding='utf-8'),encoding='utf-8')
            deps('lock',other,offline=True)
            assert sum(requests.values())==3
            print('HTTP：ZIP、TAR、三层菱形依赖、唯一顶层目录及跨项目共享缓存通过。',flush=True)
            # 错误摘要与目录穿越均不得发布缓存。
            bad=work/'bad';bad.mkdir()
            wrong=url+'/leaf.zip#sha256='+'0'*64
            (bad/'magel.toml').write_text(config('bad','bad='+json.dumps(wrong)+'\n'),encoding='utf-8')
            result=deps('lock',bad,success=False)
            assert 'SHA-256' in result.stderr
            unsafe=publish('unsafe.zip',{'magel.toml':config('unsafe'),'../../escape.mgl':'bad'})
            (bad/'magel.toml').write_text(config('bad','unsafe='+json.dumps(unsafe)+'\n'),encoding='utf-8')
            deps('lock',bad,success=False)
            assert len(list(libraries.iterdir()))==3 and not (work/'escape.mgl').exists()
            print('HTTP：摘要不匹配和不安全压缩路径被拒绝，未留下半成品。',flush=True)
            server.shutdown();thread.join();server.server_close()
            locked=(app/'magel.lock').read_bytes()
            deps('check',offline=True);deps('lock',offline=True)
            assert (app/'magel.lock').read_bytes()==locked
            if not args.unit_only:
                assert run(compiler,'run',source,'out',work/'vm').stdout=='43\n'
                output=work/('aot.exe' if os.name=='nt' else 'aot')
                run(compiler,'aot',source,'out',output)
                assert run(output).stdout=='43\n'
                print('离线：关闭服务器后递归 import 的 VM 与 AOT 均输出 43。',flush=True)
            # 编辑器索引也沿锁文件解析递归依赖及注释。
            sys.path.insert(0,str(ROOT/'tools'))
            from mageldev import LanguageServer,position
            language=LanguageServer(compiler)
            text='import lib::top;\nint64 main(){return lib::leaf::value();}'
            uri=source.as_uri();language.documents[uri]={'text':text,'version':1}
            hover=language.query('textDocument/hover',{'textDocument':{'uri':uri},'position':position(text,text.index('value')+1)})
            assert '叶子依赖函数' in hover['contents']['value']
            leaf_file=next(libraries.glob('leaf-*/modules/lib/leaf.mgl'))
            original=leaf_file.read_text(encoding='utf-8');leaf_file.write_text(original.replace('21','99'),encoding='utf-8')
            deps('check',offline=True,success=False)
            if not args.unit_only:run(compiler,'--check',source,success=False)
            leaf_file.write_text(original,encoding='utf-8')
            deps('check',offline=True)
            # 固定 Git 依赖也进入同一 libraries，保持离线检查可用。
            repo=work/'git-source';repo.mkdir()
            (repo/'magel.toml').write_text(config('gitlib'),encoding='utf-8')
            (repo/'gitvalue.mgl').write_text('module gitvalue;\nint64 value(){return 7;}',encoding='utf-8')
            run('git','-C',repo,'init');run('git','-C',repo,'add','.')
            run('git','-C',repo,'-c','user.name=模块测试','-c','user.email=test@example.invalid','commit','-m','创建在线依赖夹具')
            commit=run('git','-C',repo,'rev-parse','HEAD').stdout.strip()
            gitapp=work/'git-app';gitapp.mkdir()
            (gitapp/'magel.toml').write_text(config('gitapp','gitlib='+json.dumps('git+'+repo.as_uri()+'#'+commit)+'\n'),encoding='utf-8')
            deps('lock',gitapp);deps('check',gitapp,offline=True)
            assert (libraries/('gitlib-'+commit)).is_dir()
            if not args.unit_only:
                gitmain=gitapp/'main.mgl';gitmain.write_text('import gitvalue;\nint64 main(){return gitvalue::value()-7;}',encoding='utf-8')
                run(compiler,'run',gitmain,'out',work/'gitvm')
            localapp=work/'local-app';localapp.mkdir()
            (localapp/'magel.toml').write_text(config('localapp','local="path:../git-source"\n'),encoding='utf-8')
            deps('lock',localapp,offline=True);deps('check',localapp,offline=True)
            legacyapp=work/'legacy-app';legacyapp.mkdir()
            legacy=legacyapp/'.magel/deps'/('legacygit-'+commit)
            legacy.parent.mkdir(parents=True)
            shutil.copytree(libraries/('gitlib-'+commit),legacy)
            (legacyapp/'magel.toml').write_text(config('legacyapp','legacygit='+json.dumps('git+'+repo.as_uri()+'#'+commit)+'\n'),encoding='utf-8')
            deps('lock',legacyapp,offline=True);deps('check',legacyapp,offline=True)
            if not args.unit_only:
                for project in (localapp,legacyapp):
                    main=project/'main.mgl';main.write_text('import gitvalue;\nint64 main(){return gitvalue::value()-7;}',encoding='utf-8')
                    run(compiler,'--check',main)
            print('递归 LSP 注释、锁文件确定性、内容变更诊断、路径依赖及新旧 Git 缓存通过。',flush=True)
    finally:
        if thread.is_alive():server.shutdown();thread.join()
        server.server_close()
    print('在线依赖本地服务器回归全部通过。')


if __name__=='__main__':main()
