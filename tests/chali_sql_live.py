"""验证本机真实 MySQL/PostgreSQL；仅使用会话临时表，密码从环境读取并通过标准输入传递。"""
import os
import subprocess
from urllib.parse import quote
from chali_glue_edges import ROOT, CHALI, EXE, run

def main():
    """对照解释执行与 VM 的真实读写、事务、类型和错误恢复。"""
    password = os.environ['CHALI_SQL_TEST_PASSWORD']
    work = ROOT / 'build/sql-live'
    work.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'tests/fixtures/sql_live.chl'
    pg = 'postgresql://postgres:' + quote(password, safe='') + '@127.0.0.1:5432/postgres?connect_timeout=5'
    binary = work / ('live' + EXE)
    for backend, credential in (('mysql', password), ('postgresql', pg)):
        for label, command in (('VM', [CHALI, 'run', source, '--out', work / backend]),):
            result = subprocess.run(list(map(str, command)), cwd=ROOT, input=backend + '\n' + credential + '\n', capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)
            assert result.returncode == 0 and (not result.stderr) and result.stdout.startswith(backend + ' live database checks passed:'), (backend, label, result.stdout, result.stderr)
            print(label + ': ' + result.stdout.strip(), flush=True)
if __name__ == '__main__':
    main()
