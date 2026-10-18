"""验收独立 Python ZIP 发布脚本、覆盖保护、故障清理及 Magel zip 指令移除。"""
from pathlib import Path
from unittest.mock import patch
import argparse
import hashlib
import importlib.util
import json
import os
import subprocess
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[1]


def main():
    """检查真实编译器归档与 Python 写入失败的清理。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',type=Path,default=ROOT/'magel.exe')
    compiler=parser.parse_args().magel.resolve()
    env=os.environ.copy();env['MAGEL_LANG']='ja';env['MAGEL_PYTHON']=os.sys.executable
    def run(*args,cwd):
        return subprocess.run([str(x) for x in args],cwd=cwd,env=env,capture_output=True,encoding='utf-8',timeout=180)
    with tempfile.TemporaryDirectory(prefix='magel-zip-') as directory:
        work=Path(directory)
        output=work/'中日 文-日本語.zip'
        script=ROOT/'tools/package_compiler.py'
        def package(*args):
            return run(os.sys.executable,script,'magel','--compiler',compiler,*args,cwd=work)
        result=package('out',output)
        assert result.returncode==0 and 'パッケージ化しました' in result.stdout,result
        original=compiler.read_bytes();before=output.read_bytes()
        name='magel.exe' if os.name=='nt' else 'magel'
        with zipfile.ZipFile(output) as archive:
            assert archive.namelist()==[name] and archive.testzip() is None
            data=archive.read(name)
            assert hashlib.sha256(data).digest()==hashlib.sha256(original).digest()
            assert archive.getinfo(name).compress_type==zipfile.ZIP_DEFLATED
            assert archive.getinfo(name).compress_size<len(original)
            extracted=work/'extracted compiler';archive.extractall(extracted)
            if os.name!='nt':(extracted/name).chmod(0o755)
        result=package('out',output)
        assert result.returncode!=0 and output.read_bytes()==before,result
        result=package('out',work/'wrong.txt')
        assert result.returncode!=0 and not (work/'wrong.txt').exists(),result
        result=package()
        assert result.returncode==0,result
        version=(ROOT/'VERSION').read_text(encoding='utf-8').strip()
        defaults=[p for p in work.glob(f'magel-{version}-*.zip')]
        assert len(defaults)==1,defaults
        source=work/'日本語 示例.mgl'
        source.write_text('import std::text;\nint64 main(){std::lang::println("ZIP 中文 / 日本語");return 0;}\n',encoding='utf-8')
        result=run(extracted/name,'lang','ja',source,cwd=extracted)
        assert result.returncode==0 and result.stdout=='ZIP 中文 / 日本語\n' and not result.stderr,result
        result=run(extracted/name,'help','lang','ja',cwd=extracted)
        assert result.returncode==0 and '使い方' in result.stdout and 'zip [out' not in result.stdout,result
        removed=work/'removed.zip'
        result=run(compiler,'zip','out',removed,cwd=work)
        assert result.returncode!=0 and not removed.exists(),result
        spec=importlib.util.spec_from_file_location('package_compiler',script)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        partial=work/'partial.zip'
        with patch.object(zipfile.ZipFile,'write',side_effect=OSError('injected failure')):
            try:module.package(compiler,partial,'magel')
            except OSError:pass
            else:raise AssertionError('写入失败应上报')
        assert not partial.exists()
        missing=work/'missing.zip'
        try:module.package(work/'missing.exe',missing,'magel')
        except FileNotFoundError:pass
        else:raise AssertionError('缺失编译器应拒绝')
        assert not missing.exists()
        print(f'Magel ZIP 验收通过：DEFLATE、UTF-8 路径、覆盖保护、故障清理、自包含运行；{len(original)} -> {len(before)} 字节')


if __name__=='__main__':main()
