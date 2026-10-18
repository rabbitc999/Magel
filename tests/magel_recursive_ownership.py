"""验证递归存储、嵌套拥有值、函数值、接口及 Set/Map 键协议。"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''
SAMPLES = ('recursive_local_ownership', 'nested_enum_ownership',
           'recursive_callable_ownership', 'recursive_reference_storage', 'set_key_protocol',
           'allocated_default_ownership')


def main():
    """真实源码在解释、AOT 和可选独立 VM 中运行，成功路径不允许资源诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    parser.add_argument('--vm', type=Path)
    parser.add_argument('--backend', choices=('all', 'direct', 'aot'), default='all')
    args = parser.parse_args()
    work = ROOT / 'build/magel-recursive-ownership'
    work.mkdir(parents=True, exist_ok=True)

    class OwnershipTests(unittest.TestCase):
        """按可观察的值、错误报告和清理结果验证，避免仅匹配实现细节。"""

        def execute(self, *command):
            """固定诊断语言，限制单次工具执行时间并捕获所有输出。"""
            result = subprocess.run([str(item) for item in command], cwd=ROOT,
                                    env=dict(os.environ, MAGEL_LANG='zh'),
                                    capture_output=True, encoding='utf-8', errors='replace', timeout=240)
            self.assertEqual(result.returncode, 0, (command, result.stdout, result.stderr))
            self.assertEqual(result.stderr, '', (command, result.stdout, result.stderr))
            return result.stdout

        def run_source(self, source, backend):
            """每种表示独立构建产物；AOT 必须实际运行。"""
            target = work / (source.stem + '-' + backend + EXE)
            if backend == 'direct':
                return self.execute(args.magel.resolve(), 'run', source, '--out', target)
            self.execute(args.magel.resolve(), 'aot', source, '--out', target, '--opt-level=0')
            return self.execute(target)

        def check_backend(self, backend):
            """检查六个模块及真实计算器的正常路径和应用错误。"""
            for name in SAMPLES:
                with self.subTest(backend=backend, source=name):
                    source = ROOT / f'example/samples/{name}.mgl'
                    expected = (ROOT / f'example/expected/{name}.txt').read_text(encoding='utf-8')
                    self.assertEqual(self.run_source(source, backend), expected)
            for name in ('calc', 'calc_nested'):
                with self.subTest(backend=backend, calculator=name):
                    output = self.run_source(ROOT / f'example/calculator/{name}.mgl', backend)
                    lines = {int(line[1:line.index(']')]): line for line in output.splitlines()
                             if re.match(r'^\[\d+\]', line)}
                    self.assertEqual(len(lines), 12, output)
                    self.assertEqual(sum('] ok ' in line for line in lines.values()), 7, output)
                    self.assertEqual(sum('] error ' in line for line in lines.values()), 5, output)
                    # 两份程序的格式化精度不同，但必须表达相同数值。
                    expected = {1: 11, 2: 116, 3: 63.5, 4: 89, 5: 13.433981, 6: 908, 8: 1368}
                    for number, value in expected.items():
                        actual = float(lines[number].rsplit('=>', 1)[1].strip())
                        self.assertAlmostEqual(actual, value, delta=max(0.05, abs(value) * 0.002))
                    for number, message in {7: 'min expects 2 arguments', 9: 'division by zero',
                                            10: 'unexpected token', 11: 'undefined variable',
                                            12: 'undefined variable'}.items():
                        self.assertIn(message, lines[number])

        @unittest.skipIf(args.backend == 'aot', '仅选择 AOT')
        @unittest.skipIf(args.backend == 'aot', '仅选择 AOT')
        def test_direct(self):
            """解释执行六个独立回归与两份真实计算器。"""
            self.check_backend('direct')

        @unittest.skipIf(args.backend == 'direct', '仅选择解释执行')
        def test_aot(self):
            """AOT 实际执行相同场景，不把成功导出当作运行通过。"""
            self.check_backend('aot')

        @unittest.skipUnless(args.vm, '未指定独立 VM')
        def test_binary(self):
            """携带数据区的 svmb 在当前独立 VM 中运行。"""
            for name in SAMPLES:
                with self.subTest(source=name):
                    prefix = work / (name + '-binary')
                    self.execute(args.magel.resolve(), 'svmb', ROOT / f'example/samples/{name}.mgl',
                                 '--out', prefix, '--debug')
                    output = self.execute(args.vm.resolve(), 'run', prefix.with_suffix('.svmb'),
                                          '--data', prefix.with_suffix('.data.bin'),
                                          '--layout', prefix.with_suffix('.layout'))
                    expected = (ROOT / f'example/expected/{name}.txt').read_text(encoding='utf-8').rstrip()
                    self.assertEqual(output.partition('\n\n;')[0], expected)
                    self.assertNotIn('没释放', output)
                    self.assertNotIn('没有 delete', output)

        def test_set_rejections(self):
            """Hash 不能代替 Equal；比较期间不能重入同一容器。"""
            for name in ('collection_set_missing_equal', 'collection_set_hash_without_equal',
                         'collection_set_reentry', 'collection_map_reentry'):
                with self.subTest(source=name):
                    result = subprocess.run([str(args.magel.resolve()), 'run',
                                             str(ROOT / f'example/samples-error/{name}.mgl'),
                                             '--out', str(work / name)], cwd=ROOT,
                                            env=dict(os.environ, MAGEL_LANG='zh'), capture_output=True,
                                            encoding='utf-8', errors='replace', timeout=120)
                    self.assertNotEqual(result.returncode, 0, result)
                    expected = (ROOT / f'example/expected-error/{name}.txt').read_text(encoding='utf-8').strip()
                    self.assertEqual(result.stderr.splitlines()[0], expected)

    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(OwnershipTests))
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
