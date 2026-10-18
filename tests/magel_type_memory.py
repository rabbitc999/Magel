"""验证 loop 类型作用域、借用返回值与默认容器初始化的类型和所有权契约。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''
FIXTURES = ('loop_owned_values', 'borrowed_aggregate_return',
            'borrowed_index_values', 'global_container_defaults')


def main():
    """运行三后端正常用例与编译期拒绝用例，每个运行产物都必须无资源泄漏诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel' + EXE))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    with tempfile.TemporaryDirectory(prefix='magel-type-memory-', dir=ROOT / 'build') as temporary:
        work = Path(temporary)

        class TypeMemoryTests(unittest.TestCase):
            """真实 MAGEL 编译和执行测试，断言可观察的值语义、类型与资源生命周期。"""

            def run_tool(self, command):
                """捕获编译器或独立程序的输出，并限制单次验证时间。"""
                return subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                                      text=True, encoding='utf-8', errors='replace', timeout=240)

            def check_backend(self, backend):
                """每个后端读取相同源码和预期输出，覆盖循环内重复构造与来源后续使用。"""
                for name in FIXTURES:
                    with self.subTest(backend=backend or 'direct', fixture=name):
                        source = ROOT / f'example/samples/{name}.mgl'
                        expected = (ROOT / f'example/expected/{name}.txt').read_text(encoding='utf-8')
                        executable = work / (name + (backend or 'direct') + EXE)
                        result = self.run_tool([compiler, *([backend, '--opt-level=0'] if backend else ['run']),
                                                source, '--out', executable])
                        self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
                        if backend:
                            result = self.run_tool([executable])
                        self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
                        self.assertEqual(result.stdout, expected)
                        self.assertEqual(result.stderr, '')

            def test_direct(self):
                """直接执行四个拥有资源和类型场景。"""
                self.check_backend('')

            def test_aot(self):
                """AOT 编译后执行相同四个场景。"""
                self.check_backend('aot')

            def test_invalid_types(self):
                """拒绝不一致的退出类型、越过块作用域的变量和只读下标写入。"""
                sources = {
                    'loop_mismatch': 'int64 main(){auto value=loop{if(true){String value="text";yield (value);}else{int32 value=7;yield (value);}};return 0;}',
                    'loop_scope': 'int64 main(){auto value=loop{{String hidden="text";}yield (hidden);};return 0;}',
                    'index_const': 'struct Source : Index<String const&,int64>{String value;String const& index(int64 key){return &self.value;}};int64 main(){Source source;source.value="old";source[0]="new";return 0;}',
                }
                for name, source in sources.items():
                    with self.subTest(fixture=name):
                        path = work / (name + '.mgl')
                        path.write_text(source + '\n', encoding='utf-8')
                        result = self.run_tool([compiler, 'run', path, '--check'])
                        self.assertNotEqual(result.returncode, 0)
                        self.assertTrue(result.stderr)
                        for expected in {'loop_mismatch': ('std::lang::String', 'int32'),
                                         'loop_scope': ('hidden',), 'index_const': ('const&',)}[name]:
                            self.assertIn(expected, result.stderr)

        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(TypeMemoryTests))
        return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
