"""仅在虚拟机模式验证独立胶水模块的集合参数、返回值、复制与同步回调。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile


def main():
    """加载真实宿主模块，验证宿主和运行时使用同一集合记录协议。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', default='magel.exe')
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    compiler = Path(options.magel).resolve()
    fixture = root / 'tests/fixtures/collection_host_glue'
    def run(*arguments):
        """执行构建或 VM 命令，并保留失败时的完整诊断。"""
        result = subprocess.run([str(value) for value in arguments], cwd=root,
                                capture_output=True, encoding='utf-8', errors='replace', timeout=360)
        assert result.returncode == 0, result.stdout + result.stderr
        return result

    with tempfile.TemporaryDirectory(prefix='magel-collection-glue-') as folder:
        work = Path(folder)
        host_source = work / 'vm_host.cpp'
        text = (root / 'example/native-callbacks/vm_host.cpp').read_text(encoding='utf-8')
        text = text.replace('#include "glue.cpp"',
                            '#include "glue/std_collection.hpp"\n#include "' + fixture.with_suffix('.cpp').as_posix() + '"')
        if 'collection_glue::bind' not in text:
            text = text.replace('|| !bindMagelGlue(registry, &error)',
                                '|| !collection_glue::bind(registry.native(), &error)\n        || !bindMagelGlue(registry, &error)')
        host_source.write_text(text, encoding='utf-8')
        host = work / ('vm-host.exe' if os.name == 'nt' else 'vm-host')
        run(os.environ.get('CXX', 'g++'), '-std=c++17', '-O2', '-pthread',
            '-DMAGEL_CUSTOM_ALLOCATOR=1', '-DMAGEL_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1',
            '-Isource', '-Ilibrary', '-Ilibrary/stackvm', host_source,
            root / 'build/magel-stackvm-core/libstackvm.a', root / 'build/magel-host-records/librecords.a', '-o', host)
        prefix = work / 'program'
        run(compiler, 'emit', fixture.with_suffix('.mgl'), '--svmb', '--out', prefix)
        result = run(host, prefix.with_suffix('.svmb'), prefix.with_suffix('.data.bin'),
                     prefix.with_suffix('.layout'))
        assert result.stdout.splitlines() == ['collection host glue OK'], result.stdout
        assert not result.stderr, result.stderr
    print('VM collection host glue: parameters, returns, copies and callback passed')


if __name__ == '__main__':
    main()
