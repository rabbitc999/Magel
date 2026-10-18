from pathlib import Path
import shutil
import subprocess
import sys


def main():
    """在 build/work 中准备 Magel 源码副本，避免生成和编译修改源码目录。

用法：`python tools/prepare_build.py`（在本项目根目录跑）。它只读本项目自己的
跟踪文件，因此在仓库外留下一份工作副本也照样成立。"""
    root = Path(__file__).resolve().parent.parent
    work = root / 'build' / 'work'
    files = subprocess.check_output(
        ['git', '-C', str(root), 'ls-files', '--cached', '--others', '--exclude-standard', '-z', '--', '.', 'tools/embed_stdlib.py', 'tools/write_build_config.py']
    ).decode('utf-8').split('\0')
    for name in filter(None, files):
        source = root / name
        if not source.is_file():
            continue
        destination = work / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists() or source.read_bytes() != destination.read_bytes():
            shutil.copy2(source, destination)
    subprocess.run([sys.executable, str(work / 'tools' / 'embed_stdlib.py')], check=True)


if __name__ == '__main__':
    main()
