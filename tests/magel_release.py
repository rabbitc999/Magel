"""验证发布版本检查、归档内容及失败时的原子替换。"""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
import hashlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('magel_release', ROOT / 'tools/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    """使用独立目录校验发布产物，不触及仓库标签及远端。"""

    def test_version_drift(self):
        """插件版本漂移和非法 VERSION 必须在执行构建前失败。"""
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'vscode').mkdir()
            (root / 'VERSION').write_text('1.0.0', encoding='utf-8')
            (root / 'vscode/package.json').write_text('{"version":"1.0.0"}', encoding='utf-8')
            self.assertEqual(release.version_of(root), '1.0.0')
            (root / 'vscode/package.json').write_text('{"version":"0.9.0"}', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'VERSION'):
                release.version_of(root)
            (root / 'VERSION').write_text('../bad', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'VERSION'):
                release.version_of(root)

    def test_archive_and_failed_replacement(self):
        """归档包含源码身份、样例和正确摘要；输入丢失不破坏原 ZIP。"""
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'doc/book/latex').mkdir(parents=True)
            (root / 'example/samples').mkdir(parents=True)
            (root / 'doc/book/latex/book.pdf').write_bytes(b'%PDF-1.7\n')
            for name in ('VERSION', 'LICENSE', 'CHANGELOG.md'):
                (root / name).write_text('1.0.0', encoding='utf-8')
            for name in release.SAMPLES:
                (root / f'example/samples/{name}.mgl').write_text('int64 main(){return 0;}', encoding='utf-8')
            binary, vsix = root / 'magel.exe', root / 'magel-editor-1.0.0.vsix'
            binary.write_bytes(b'compiler')
            vsix.write_bytes(b'extension')
            output = root / 'output with spaces'
            archive = release.package_assets(root, output, binary, vsix, '1.0.0', 'windows-x64', 'abc123', True)
            with zipfile.ZipFile(archive) as packed:
                self.assertIsNone(packed.testzip())
                metadata = json.loads(packed.read('release.json'))
                self.assertEqual(metadata['commit'], 'abc123')
                self.assertTrue(metadata['workingTreeDirty'])
                self.assertEqual(packed.read('magel.exe'), b'compiler')
                self.assertIn('examples/any_generics.mgl', packed.namelist())
            for line in (output / 'SHA256SUMS.txt').read_text().splitlines():
                digest, name = line.split('  ', 1)
                self.assertEqual(digest, hashlib.sha256((output / name).read_bytes()).hexdigest())
            original = archive.read_bytes()
            (root / f'example/samples/{release.SAMPLES[-1]}.mgl').unlink()
            with self.assertRaises(OSError):
                release.package_assets(root, output, binary, vsix, '1.0.0', 'windows-x64', 'new', False)
            self.assertEqual(archive.read_bytes(), original)
            self.assertFalse(list(output.glob('*.zip.tmp')))


if __name__ == '__main__':
    unittest.main(verbosity=2)
