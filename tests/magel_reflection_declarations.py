#!/usr/bin/env python3
"""真实 MAGEL 声明到元数据：布局、注解、稳定身份、容器与 VM/AOT 自包含查询。"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import subprocess
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = '.exe' if os.name == 'nt' else ''


def main():
    """依次验证前端限制、二进制表及两种导出的真实声明记录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT/('magel'+EXE))
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--vm-library', type=Path, default=ROOT/'build/magel-stackvm/libstackvm.a')
    args = parser.parse_args()
    checks = 0

    def run(command, success=True, cwd=ROOT):
        nonlocal checks
        result = subprocess.run(list(map(str, command)), cwd=cwd, capture_output=True, text=True,
                                encoding='utf-8', errors='replace', timeout=240)
        assert (result.returncode == 0) == success, (command, result.stdout, result.stderr)
        checks += 1
        return result

    with tempfile.TemporaryDirectory(prefix='reflection-declarations-', dir=ROOT/'build') as temporary:
        work = Path(temporary)
        sample = ROOT/'example/reflection/declarations.mgl'
        unit = work/('verify'+EXE)
        flags = [args.cxx, '-std=c++17', '-O1', '-pthread', '-DMAGEL_CUSTOM_ALLOCATOR=1',
                 '-DSTACKVM_WITH_REFLECTION=1', '-I'+str(ROOT), '-I'+str(ROOT/'library'), '-I'+str(ROOT/'source')]
        run([*flags, ROOT/'tests/magel_reflection_declarations_test.cpp',
             args.vm_library.resolve(), '-o', unit])
        for options in (['--no-compress-symbols'], []):
            prefix = work/'program'
            run([args.magel, '--reflection', '--emit', '--svmb', *options, sample, '--out', prefix])
            run([unit, prefix.with_suffix('.svmb')])
        for backend in ('aot',):
            prefix = work/(backend+EXE)
            run([args.magel, backend, '--reflection', '--opt-level=0', '--keep-intermediate', sample, '--out', prefix])
            target = prefix.with_suffix(EXE) if EXE else prefix
            assert run([target]).stdout == '42\n'
            source = Path(str(target)+'.aot.cpp')
            # 导出入口路径由统一工具确定；查找实际保留的自包含文件。
            if not source.exists():
                candidates = list(work.glob(backend+'*.cpp'))
                assert len(candidates)==1, candidates
                source = candidates[0]
            harness = work/(backend+'-query.cpp')
            harness.write_text('#define main generated_main\n#include "'+source.name+'"\n#undef main\n'
                               '#define MAGEL_REFLECTION_QUERY_ONLY\n#include "'+str(ROOT/'tests/magel_reflection_declarations_test.cpp').replace('\\','/')+'"\n'
                               'int main(){ Memory::DynamicRuntimeArena arena(8u*1024u*1024u);'
                               '{Memory::RuntimeSession session(arena);verifyDeclarations(magel_reflection_metadata());}'
                               'return arena.stats().liveBlocks==0 ? 0:1;}\n', encoding='utf-8')
            query = work/(backend+'-query'+EXE)
            run([*flags, *(['-Wa,-mbig-obj'] if os.name=='nt' else []), harness, '-o', query])
            run([query])
        source = work/'case.mgl'
        def check(text, error=None, enabled=True):
            source.write_text(text,encoding='utf-8')
            result=run([args.magel, *(['--reflection'] if enabled else ['--no-reflection']), '--check', source], success=error is None)
            if error: assert error in result.stderr, result.stderr
        check('@Reflect(1) int64 f(){return 1;} int64 main(){return 0;}', '不接受参数')
        check('@Reflect struct S{@Reflect int64 x;}; int64 main(){return 0;}', '成员随类型暴露')
        check('@Reflect int64 main(){return 0;}', '功能未开启', enabled=False)
        check('@std::reflect::Reflect int64 main(){return 0;}')
        # 用户同名注解保留原先含义，不劫持既有声明。
        check('@annotation Reflect(x:int32)\n@Reflect(1) int64 main(){return 0;}')
        check('struct Box<T>{T value;};\n@Reflect int64 read(Box<int64> x){return x.value;}\nint64 main(){return 0;}')
        # 源码无关的稳定身份及命名空间隔离，通过元数据四元式标记解码检查。
        (work/'magel.toml').write_text('[package]\nname="reflection"\nversion="0.1.0"\n[magel]\nimport_paths=["."]\n',encoding='utf-8')
        for module in ('one','two'):
            (work/(module+'.mgl')).write_text('module '+module+';\n@Reflect struct Same{int64 value;};\n',encoding='utf-8')
        source.write_text('import one;\nimport two;\nint64 main(){return 0;}\n',encoding='utf-8')
        run([args.magel, '--reflection', '--emit', source, '--out', work/'names'], cwd=work)
        def identities(prefix):
            quad=prefix.with_suffix('.quad').read_bytes()
            assert quad[:5] == b"MGQI\x01"
            position=5
            def varint():
                nonlocal position
                value=0; shift=0
                while True:
                    byte=quad[position]; position+=1
                    value|=(byte&127)<<shift
                    if byte<128: return value
                    shift+=7
            assert varint() & 2  # stackvm::kFeatureReflection
            for _ in range(4):
                size=varint(); position+=size
            size=varint(); data=quad[position:position+size]
            count=struct.unpack_from('<I',data,8)[0]
            offset=12
            result={}
            for _ in range(count):
                identity,size=struct.unpack_from('<QI',data,offset)
                offset+=12
                result[data[offset:offset+size].decode('utf-8')]=identity
                offset+=size
            return result
        names=identities(work/'names')
        assert names['struct:one::Same'] != names['struct:two::Same']
        snapshots=[]
        for index,alias in enumerate(('String','std::lang::String')):
            extra='struct Unrelated{int32 x;};\n' if index else ''
            source.write_text(extra+'struct Box<T>{T value;};\n@Reflect String read(Box<'+alias+'> value){return value.value;}\nint64 main(){return 0;}\n',encoding='utf-8')
            prefix=work/('identity'+str(index))
            run([args.magel,'--reflection','--emit',source,'--out',prefix])
            snapshots.append(identities(prefix))
        assert snapshots[0]==snapshots[1],snapshots
        assert 'struct:Box<std::lang::String>' in snapshots[0],snapshots
        assert not any('__string' in key for key in snapshots[0]), snapshots
        print(f'MAGEL 声明元数据：{checks} 项集成检查通过')


if __name__ == '__main__':
    main()
