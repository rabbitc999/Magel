"""验证旧库删除及标准模块迁移后的查找、独立部署和用户同名模块。"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """从空目录运行独立编译器，确认旧库不能由磁盘或内嵌资产恢复。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / 'ChaLi.exe')
    compiler = parser.parse_args().chali.resolve()
    if not compiler.is_file() and os.name == "nt":
        compiler = compiler.with_suffix(".exe")
    env = dict(os.environ, CHALI_LANG='zh')
    with tempfile.TemporaryDirectory(prefix='chali-removed-libraries-') as directory:
        work = Path(directory)
        standalone = work / compiler.name
        shutil.copyfile(compiler, standalone)
        modules = ['robot', 'robot::types', 'robot::control', 'robot::filter',
                   'robot::kinematics', 'robot::localization', 'robot::pointcloud', 'car', 'utils', 'reflect']
        for module in modules:
            source = work / 'main.chl'
            source.write_text('import '+module+';\nvoid main(){return;}\n', encoding='utf-8')
            result = subprocess.run([str(standalone), 'check', str(source), 'json'],
                                    cwd=work, env=env, capture_output=True,
                                    encoding='utf-8', timeout=120)
            assert result.returncode != 0, (module, result.stdout, result.stderr)
            assert json.loads(result.stdout)['diagnostics'], (module, result.stdout, result.stderr)
        # 删除的是内置库，不占用或禁止用户自己定义的同名模块。
        for module in ('robot', 'car'):
            (work / (module+'.chl')).write_text('module '+module+';\nint64 value(){return 7;}\n', encoding='utf-8')
            source.write_text('import '+module+';\nvoid main(){assert('+module+'::value()==7);return;}\n', encoding='utf-8')
            result = subprocess.run([str(standalone), 'run', str(source), 'out', str(work/'program')],
                                    cwd=work, env=env, capture_output=True,
                                    encoding='utf-8', timeout=120)
            assert (result.returncode, result.stdout, result.stderr) == (0, '', ''), result
    index = json.loads((ROOT/'tools/chali_stdlib_api.json').read_text(encoding='utf-8'))
    assert not any(name in ('car', 'robot', 'utils', 'reflect') or name.startswith('robot::') for name in index['modules'])
    assert not any(row['module'] == 'car' or row['module'].startswith('robot') for row in index['functions'])
    print('已移除库验证通过：10 个旧导入拒绝、2 个用户同名模块可用、API 索引无旧库')


if __name__ == '__main__':
    main()
