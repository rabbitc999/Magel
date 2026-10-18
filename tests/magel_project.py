"""MAGEL 新建应用工程回归：参数、文件保护与生成工程实际运行。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys
import tempfile
import tomllib
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')


class ProjectTests(unittest.TestCase):
    """使用真实统一工具链验证工程创建。"""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='magel-project-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def run_cli(self, *arguments, expected=0, cwd=None, env=None):
        result = subprocess.run(
            [str(COMPILER), *map(str, arguments)], cwd=cwd or self.directory,
            capture_output=True, text=True, encoding='utf-8', errors='replace',
            timeout=180, env=env)
        self.assertEqual(result.returncode, expected, (arguments, result.stdout, result.stderr))
        return result

    def test_generated_project(self):
        # 新建指令无需 Python；配置文件须由真实 TOML 解析器验证。
        env = dict(os.environ, MAGEL_PYTHON='missing-python-for-project-test')
        self.run_cli('new', 'hello-magel', env=env)
        project = self.directory / 'hello-magel'
        self.assertEqual(
            {p.relative_to(project).as_posix() for p in project.rglob('*') if p.is_file()},
            {'magel.toml', 'src/main.mgl', 'tests/test_main.mgl', 'README.md', '.gitignore'})
        config = tomllib.loads((project / 'magel.toml').read_text(encoding='utf-8'))
        self.assertEqual(config['package'], {'name': 'hello-magel', 'version': '0.1.0'})
        self.assertEqual(config['magel']['run'], 'interpret')
        self.run_cli('--check', 'src/main.mgl', cwd=project)
        self.assertFalse((project / 'build').exists())
        output = self.run_cli('run', 'src/main.mgl', cwd=project)
        self.assertEqual(output.stdout, 'Hello, Magel!\n')
        self.assertEqual(output.stderr, '')
        env = dict(os.environ, MAGEL_PYTHON=sys.executable)
        report = self.run_cli('test', 'tests', '--json', cwd=project, env=env)
        self.assertEqual(json.loads(report.stdout)['summary'], {'passed': 1, 'failed': 0, 'skipped': 0})
        self.run_cli('aot', 'src/main.mgl', '--out', 'build/main', cwd=project)
        native = project / 'build/main'
        if not native.exists():
            native = native.with_suffix('.exe')
        execution = subprocess.run([str(native)], cwd=project, capture_output=True,
                                   text=True, encoding='utf-8', timeout=30)
        self.assertEqual((execution.returncode, execution.stdout, execution.stderr),
                         (0, 'Hello, Magel!\n', ''))

    def test_paths_and_names(self):
        (self.directory / 'my projects').mkdir()
        self.run_cli('new', 'my projects/hello app/', '--name', 'Hello_2-app')
        project = self.directory / 'my projects/hello app'
        config = tomllib.loads((project / 'magel.toml').read_text(encoding='utf-8'))
        self.assertEqual(config['package']['name'], 'Hello_2-app')
        self.run_cli('new', '--name', 'demo', '--', '-demo')
        self.run_cli('new', self.directory / 'absolute-app')
        self.run_cli('new', 'absent-parent/app', expected=1)
        self.assertFalse((self.directory / 'absent-parent').exists())

    def test_existing_targets(self):
        for kind in ('empty', 'populated', 'file'):
            with self.subTest(kind=kind):
                target = self.directory / kind
                if kind == 'file':
                    target.write_bytes(b'keep-file')
                else:
                    target.mkdir()
                    if kind == 'populated':
                        (target / 'magel.toml').write_bytes(b'keep-config')
                self.run_cli('new', target, expected=1)
                if kind == 'file':
                    self.assertEqual(target.read_bytes(), b'keep-file')
                else:
                    self.assertEqual(list(target.iterdir()), [] if kind == 'empty' else [target / 'magel.toml'])
                    if kind == 'populated':
                        self.assertEqual((target / 'magel.toml').read_bytes(), b'keep-config')
        self.run_cli('new', '.', '--name', 'demo', expected=1)
        self.run_cli('new', '..', '--name', 'demo', expected=1)

    def test_invalid_arguments(self):
        cases = [(), ('app', 'extra'), ('app', '--unknown'), ('app', '--name'),
                 ('app', '--name', 'one', '--name', 'two'),
                 ('app', '--name', ''), ('app', '--name', '2bad'),
                 ('app', '--name', 'bad/name'), ('app', '--name', 'bad"name'),
                 ('bad name',), ('app', '--check')]
        for arguments in cases:
            with self.subTest(arguments=arguments):
                self.run_cli('new', *arguments, expected=2)
                self.assertEqual(list(self.directory.iterdir()), [])
        self.run_cli('new', '--help')
        self.run_cli('new', '-h')
        self.assertEqual(list(self.directory.iterdir()), [])
        self.assertIn('new DIR', self.run_cli('help').stdout)

    def test_symlinks(self):
        source = self.directory / 'source'
        source.mkdir()
        link = self.directory / 'link'
        dangling = self.directory / 'dangling'
        try:
            link.symlink_to(source, target_is_directory=True)
            dangling.symlink_to(self.directory / 'missing', target_is_directory=True)
        except OSError as error:
            self.skipTest(f'当前环境无法创建符号链接：{error}')
        self.run_cli('new', link, expected=1)
        self.run_cli('new', dangling, expected=1)
        self.assertTrue(link.is_symlink())
        self.assertTrue(dangling.is_symlink())
        self.assertEqual(list(source.iterdir()), [])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=COMPILER)
    args, remaining = parser.parse_known_args()
    COMPILER = args.magel.resolve()
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
