"""验证 SQLite 真实数据库在解释器、VM 的行为；只写隔离临时目录。"""
from pathlib import Path
from contextlib import closing
import os
import sqlite3
import shutil
import subprocess
import tempfile
from chali_glue_edges import ROOT, CHALI, EXE, run

def main():
    work = ROOT / 'build/chali-sqlite'
    work.mkdir(parents=True, exist_ok=True)
    os.environ['SQLITE_LIBRARY'] = str(work / 'missing-sqlite3.dll')
    source = ROOT / 'doc/examples/sqlite/demo.chl'
    expected = 'sqlite database checks passed\n'
    prefix = work / 'sqlite'
    with tempfile.TemporaryDirectory(prefix='database-', dir=work) as temporary:
        directory = Path(temporary)
        result = run(CHALI, 'run', source, '--out', prefix, '--', directory / 'interpreter.db')
        assert result.stdout == expected and (not result.stderr), (result.stdout, result.stderr)
        for path in directory.glob('*.db'):
            with closing(sqlite3.connect(path)) as connection:
                assert connection.execute('SELECT value FROM persisted').fetchone() == ('持久化',)
if __name__ == '__main__':
    main()
