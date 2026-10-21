"""在 Windows 无窗口验证真实 GUI 后端到显式事件队列的桥接与背压。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """编译真实后端和 VM 宿主，验证队列满、关闭及标签传播。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / 'magel.exe')
    compiler = parser.parse_args().magel.resolve()
    if not compiler.exists():
        compiler = compiler.with_suffix('.exe')
    if os.name != 'nt':
        print('GUI WinAPI bridge requires Windows; Qt has example/gui/test_qt.py')
        return
    environment = dict(os.environ, MAGEL_LANG='zh')
    with tempfile.TemporaryDirectory(prefix='magel-gui-events-') as folder:
        work = Path(folder)

        def run(command):
            """在隔离目录执行带超时命令并保留原始诊断。"""
            result = subprocess.run(list(map(str, command)), cwd=ROOT, env=environment,
                                    capture_output=True, encoding='utf-8', timeout=180)
            assert result.returncode == 0, (command, result.stdout, result.stderr)
            return result

        library = work / 'mcgui.dll'
        run(['gcc', '-std=c11', '-O2', '-shared', ROOT / 'example/gui/winapi/gui.c',
             '-o', library, '-luser32', '-lgdi32'])
        host_source = work / 'host.cpp'
        code = (ROOT / 'example/native-callbacks/vm_host.cpp').read_text(encoding='utf-8')
        code = code.replace('#include "glue.cpp"', '#include "' + (ROOT / 'example/gui/glue.cpp').as_posix() + '"')
        host_source.write_text(code, encoding='utf-8')
        host = work / 'host.exe'
        run(['g++', '-std=c++17', '-O2', '-pthread', '-DMAGEL_CUSTOM_ALLOCATOR=1',
             '-DMAGEL_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1',
             '-Isource', '-Ilibrary', '-Ilibrary/stackvm', host_source,
             ROOT / 'build/magel-stackvm-core/libstackvm.a',
             ROOT / 'build/magel-host-records/librecords.a', '-lwinhttp', '-o', host])
        source = work / 'events.mgl'
        source.write_text('''import gui;
import std::event;
int64 main() {
    assert(gui::bindEvents(99) == 1);
    for (int64 i in std::lang::range(0, 300)) { gui::postEvent((int32)7); }
    assert(gui::droppedEvents() == 44);
    int64 count = 0;
    while (true) {
        auto next = std::event::tryReceive();
        if (!next.isReady()) { break; }
        assert(next.value.tag == 99 && next.value.source == 7);
        count = count + 1;
    }
    assert(count == 256);
    std::event::close();
    gui::postEvent((int32)9);
    assert(gui::droppedEvents() == 45);
    gui::unbindEvents();
    println("gui events ok");
    return 0;
}
''', encoding='utf-8')
        prefix = work / 'program'
        run([compiler, 'svmb', source, '-I', ROOT / 'example/gui', '--out', prefix,
             '--no-compress-symbols'])
        environment['MAGEL_GUI_LIBRARY'] = str(library)
        result = run([host, prefix.with_suffix('.svmb'), prefix.with_suffix('.data.bin'),
                      prefix.with_suffix('.layout')])
        assert result.stdout.strip() == 'gui events ok' and not result.stderr, result
    print('GUI explicit event bridge passed without creating windows')


if __name__ == '__main__':
    main()
