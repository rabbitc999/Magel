"""MAGEL 简短场景指令及拖入源码参数边界的端到端回归。"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'magel.exe'


class SceneTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='magel-scenes-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.source = self.work / 'hello world.mgl'
        self.source.write_text('int64 main(List<String> args) {\n'
                               '    for (String arg in args) { std::lang::println("[{}]", arg); }\n'
                               '    return 0;\n}\n', encoding='utf-8')

    def cli(self, *arguments, code=0):
        result = subprocess.run([str(COMPILER), *map(str, arguments)], cwd=self.work,
                                capture_output=True, text=True, encoding='utf-8', errors='replace',
                                timeout=180, env=dict(os.environ, MAGEL_PYTHON=sys.executable))
        self.assertEqual(result.returncode, code, (arguments, result.stdout, result.stderr))
        return result

    def test_dragged_source_arguments(self):
        # 父配置也不能将直接运行改成仅导出。
        (self.work / 'magel.toml').write_text('[magel]\nrun="svmb"\n', encoding='utf-8')
        result = self.cli(self.source, 'Alice', 'two words', '', '--check', '--out', 'debug')
        self.assertEqual(result.stdout, '[Alice]\n[two words]\n[]\n[--check]\n[--out]\n[debug]\n')
        self.assertEqual(result.stderr, '')
        self.assertFalse((self.work / 'debug.quad').exists())
        result = self.cli(self.source, '--', 'help')
        self.assertEqual(result.stdout, '[help]\n')

    def test_short_scenes(self):
        report = self.cli('check', self.source, 'json')
        self.assertEqual(json.loads(report.stdout), {'schemaVersion': 1, 'diagnostics': []})
        self.assertEqual(list(self.work.glob('*.quad')), [])
        self.cli('new', 'app', 'name', 'help')
        self.assertIn('name = "help"', (self.work / 'app/magel.toml').read_text(encoding='utf-8'))
        self.cli('fmt', 'check', self.work / 'app/src')
        summary = json.loads(self.cli('test', self.work / 'app/tests', 'backend', 'interpret', 'json').stdout)
        self.assertEqual(summary['summary'], {'passed': 1, 'failed': 0, 'skipped': 0})
        self.cli('deps', 'lock', 'project', self.work / 'app', 'offline')
        self.cli('deps', 'check', 'project', self.work / 'app', 'offline')
        self.assertIn('check FILE', self.cli('help').stdout)
        self.cli('new', 'help')  # help 保留为动作，不能创建目录。
        self.assertFalse((self.work / 'help').exists())

    def test_explicit_run_and_aot(self):
        result = self.cli('run', self.source, 'out', 'debug', '--', 'out', '--check')
        self.assertEqual(result.stdout, '[out]\n[--check]\n')
        self.assertTrue((self.work / 'debug.quad').exists())
        self.cli('aot', self.source, 'out', 'build/app', 'opt', '0')
        executable = self.work / 'build/app'
        if not executable.exists():
            executable = executable.with_suffix('.exe')
        result = subprocess.run([str(executable), 'two words', '--check'], capture_output=True,
                                text=True, encoding='utf-8', timeout=30)
        self.assertEqual((result.returncode, result.stdout, result.stderr), (0, '[two words]\n[--check]\n', ''))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=COMPILER)
    args, rest = parser.parse_known_args()
    COMPILER = args.magel.resolve()
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)
