#!/usr/bin/env python3
"""验证类型化元数据的真实 VM/AOT 导出；非空表来自 C++ 编译器模拟夹具。"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''


def main() -> None:
    """运行两种真实配置及自包含导出查询，不把宿主夹具当作 MAGEL 源码生成。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--vm-on', type=Path, default=ROOT/'build/reflection-vm-on'/('magel-reflection-tests'+EXE))
    parser.add_argument('--vm-off', type=Path, default=ROOT/'build/reflection-vm-off'/('magel-reflection-tests'+EXE))
    parser.add_argument('--emitter', type=Path, help='可选：已开启反射的独立 magel-emit')
    args = parser.parse_args()
    checks = 0

    def run(command: list, *, success: bool = True):
        nonlocal checks
        result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=240)
        assert (result.returncode == 0) == success, (command, result.stdout, result.stderr)
        checks += 1
        return result

    with tempfile.TemporaryDirectory(prefix='reflection-metadata-', dir=ROOT/'build') as temporary:
        work = Path(temporary)
        for vm in (args.vm_on, args.vm_off):
            result = run([vm.resolve(), work])
            print(result.stdout.strip())
        emitter = args.emitter
        if emitter is None:
            # 复用根构建中的真实 runCli 对象，测试不重新实现导出流程。
            entry = work/'emit-entry.cpp'
            entry.write_text('#include "source/magel-emit/emit_cli.hpp"\nint main(int argc,char **argv){return magel_emit::runCli(argc,argv);}\n', encoding='utf-8')
            emitter = work/('magel-emit'+EXE)
            run([args.cxx, '-std=c++17', '-pthread', '-DMAGEL_CUSTOM_ALLOCATOR=1',
                 '-I'+str(ROOT), entry, ROOT/'source/magel-emit/main.o',
                 ROOT/'source/magel-emit/aot.o', ROOT/'build/magel-stackvm/libstackvm.a', '-lm', '-o', emitter])
        emitter = emitter.resolve()
        query = (work/'query.inc').read_text(encoding='utf-8')
        flags = [args.cxx, '-std=c++17', '-O0', '-pthread', '-finput-charset=UTF-8', '-fexec-charset=UTF-8']
        if os.name == 'nt':
            flags += ['-Wa,-mbig-obj']
        quad = work/'metadata.quad'
        for backend in ('aot', 'modules'):
            folder = work/backend
            folder.mkdir()
            target = folder/('program'+EXE)
            source = folder/(target.name+'.aot.cpp')
            command = [emitter, '--quad', quad, '--opt-level=0', '--out', target]
            command += ['--target', 'cpp-aot', '--keep-intermediate']
            if backend == 'modules':
                command += ['--separate-modules', '--module-cache', folder/'cache']
            run(command)
            assert run([target]).stdout == '42'
            assert source.exists(), source
            text = source.read_text(encoding='utf-8')
            assert 'magel_reflection_metadata()' in text
            harness = folder/'query.cpp'
            scoped_query = query.replace('int main()', 'int verifyQuery()')
            scoped_query += ('\nint main() { Memory::DynamicRuntimeArena arena(8u*1024u*1024u); '
                             '{ Memory::RuntimeSession session(arena); if (verifyQuery()) return 3; } '
                             'return arena.stats().liveBlocks == 0 ? 0 : 4; }\n')
            harness.write_text('#define main generated_program_main\n#include "'+source.name+'"\n#undef main\n'+scoped_query, encoding='utf-8')
            query_exe = folder/('query'+EXE)
            # 模拟夹具没有额外函数单元，模块构建仍实际生成并编译唯一运行时单元。
            run([*flags, harness, '-o', query_exe])
            run([query_exe])
            rejected = run([*flags, '-DSTACKVM_WITH_REFLECTION=0', '-fsyntax-only', source], success=False)
            assert 'reflection feature configuration mismatch' in rejected.stderr
            print(f'[通过] {backend} 独立程序执行、非空表宿主查询及关闭宏拒绝')
        # 默认关闭产物不产生元数据定义；通过真实编译器验证空表新增段不会破坏注解路径。
        plain = work/'plain.mgl'
        plain.write_text('int64 main(){return 0;}\n', encoding='utf-8')
        magel = ROOT/('magel'+EXE)
        prefix = work/'plain'
        run([magel, 'run', plain, '--emit', '--svmb', '--no-compress-symbols', '--out', prefix])
        assert b'MCRF' not in prefix.with_suffix('.svmb').read_bytes()
        run([magel, 'run', plain, '--reflection', '--emit', '--svmb', '--no-compress-symbols', '--out', prefix])
        assert prefix.with_suffix('.svmb').read_bytes().endswith(b'MCRF'+(1).to_bytes(4, 'little')+bytes(28))
        print(f'反射元数据传输及导出：{checks} 项集成检查通过')


if __name__ == '__main__':
    main()
