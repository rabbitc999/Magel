"""验证 VM 命令、四元式与字节码产物、未知文件和项目运行方式。"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali')
SOURCE = 'void main() { println("VM command OK"); }\n'


def run(*arguments, cwd=ROOT, success=True, language='zh'):
    """运行真实编译器，并保留失败时的退出状态和诊断。"""
    result = subprocess.run([str(COMPILER), *map(str, arguments)], cwd=cwd,
                            env=dict(os.environ, CHALI_LANG=language), capture_output=True,
                            encoding='utf-8', errors='replace', timeout=120)
    assert (result.returncode == 0) == success, (arguments, result.stdout, result.stderr)
    return result


def main():
    """检查命令选择、仅 VM 的交付路径与项目配置优先级。"""
    with tempfile.TemporaryDirectory(prefix='chali-vm-tool-') as directory:
        work = Path(directory)
        source = work / 'main.chl'
        source.write_text(SOURCE, encoding='utf-8')
        for options in [('run', source), ('interpret', source), ('--mode', 'interpret', source),
                        ('--run-mode=interpret', source)]:
            result = run(*options, '--out', work / 'run')
            assert result.stdout == 'VM command OK\n' and not result.stderr, result
        for command in ['emit', 'svmb']:
            prefix = work / command
            result = run(command, source, 'out', prefix, '--debug')
            assert not result.stdout and not result.stderr, result
            assert all(prefix.with_suffix(suffix).is_file() for suffix in ['.quad', '.data.bin', '.layout'])
            assert prefix.with_suffix('.svmb').is_file() == (command == 'svmb')
        for unknown in ['aot', 'missing.chl']:
            result = run(unknown, cwd=work, success=False)
            assert '不认识的文件' in result.stderr, result.stderr
        result = run('aot', cwd=work, success=False, language='en')
        assert 'Unknown file' in result.stderr, result.stderr
        help_text = run('--help').stdout
        assert not re.search(r'\baot\b|cpp-aot|chali-emit', help_text, re.I), help_text
        run('--mode', 'aot', source, success=False)
        project = work / 'project'
        project.mkdir()
        (project / 'main.chl').write_text(SOURCE, encoding='utf-8')
        (project / 'chali.toml').write_text('[package]\nname="vm"\nversion="0.1.0"\n[chali]\nrun="svmb"\n', encoding='utf-8')
        result = run('--out', 'default', 'main.chl', cwd=project)
        assert not result.stdout and (project / 'default.svmb').is_file(), result
        result = run('run', 'main.chl', cwd=project)
        assert result.stdout == 'VM command OK\n', result
    print('VM commands: run, check, quadruples, bytecode, unknown files and project configuration passed')


if __name__ == '__main__':
    main()
