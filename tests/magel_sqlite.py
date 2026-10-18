"""验证 SQLite 真实数据库在解释器、AOT 的行为；只写隔离临时目录。"""
from pathlib import Path
from contextlib import closing
import os
import sqlite3
import shutil
import subprocess
import tempfile
from magel_glue_edges import ROOT, MAGEL, EXE, run


def main():
    work = ROOT / 'build/magel-sqlite'
    work.mkdir(parents=True, exist_ok=True)
    # 指向不存在的动态库；内置 SQLite 必须忽略外部库配置。
    os.environ['SQLITE_LIBRARY'] = str(work / 'missing-sqlite3.dll')
    source = ROOT / 'doc/examples/sqlite/demo.mgl'
    expected = 'sqlite database checks passed\n'
    prefix = work / 'sqlite'
    with tempfile.TemporaryDirectory(prefix='database-', dir=work) as temporary:
        directory = Path(temporary)
        result = run(MAGEL, 'run', source, '--out', prefix, '--', directory / 'interpreter.db')
        assert result.stdout == expected and not result.stderr, (result.stdout, result.stderr)
        variants = {
            'cpp-aot': [],
            'make': ['--make', 'make'],
            'modules': ['--separate-modules', '--module-cache', directory / 'cache'],
            'offline': ['--magel-dir', directory / 'absent', '--vm-dir', directory / 'absent/stackvm',
                        '--bigint-dir', directory / 'absent/bignum'],
        }
        for target, options in variants.items():
            executable = work / (target + EXE)
            run(MAGEL, 'aot', source, '--out', executable, '--opt-level=0', *options)
            isolated = directory / ('isolated ' + target)
            isolated.mkdir()
            standalone = isolated / executable.name
            shutil.copy2(executable, standalone)
            path = directory / (target + '.db')
            result = subprocess.run([str(standalone), str(path)], cwd=isolated,
                                    capture_output=True, text=True, encoding='utf-8', timeout=120)
            assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), (target, result)
            assert not list(work.glob(executable.name + '.sqlite3.*')), '默认构建应清理 SQLite 中间文件'
            print(target + '：无 SQLite DLL 的隔离目录运行通过', flush=True)
        for path in directory.glob('*.db'):
            with closing(sqlite3.connect(path)) as connection:
                assert connection.execute('SELECT value FROM persisted').fetchone() == ('持久化',)
    print('静态 SQLite（无外部 DLL）：真实增删改查、事务、类型、转义、错误恢复、文件持久化解释执行与 AOT 通过')


if __name__ == '__main__':
    main()
