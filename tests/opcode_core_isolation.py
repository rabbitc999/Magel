"""阻止已剥离的标准对象语义重新进入 VM 核心及其构建依赖。"""
from pathlib import Path
import re


def main():
    """检查核心源码、公共头和静态库定义不再引用具体宿主对象。"""
    root = Path(__file__).resolve().parents[1]
    directory = root / 'library/stackvm'
    forbidden = re.compile(r'\b(?:FileApi|FileHandle|BigInt|BigFloat|BigInteger|BigDecimal)\b|'
                           r'__vm_(?:file|bint|bfloat)|ObjectKind::(?:File|Process)|'
                           r'STACKVM_WITH_BIG(?:INT|FLOAT)|\bbignum\b')
    sources = [directory / 'stackvm.cpp', *sorted(directory.glob('*.hpp')), *sorted(directory.glob('*.inc')), *sorted((directory / 'parts').glob('*'))]
    failures = []
    frontend = [root / 'source/magel_types.h', root / 'source/magel_epilogue.cpp', *sorted((root / 'source/epilogue').glob('*.cpp'))]
    legacy_kinds = re.compile(r'MType::(?:String|BigInt|BigFloat|File|Proc)\b')
    for path in frontend:
        source = path.read_text(encoding='utf-8')
        if legacy_kinds.search(source):
            failures.append(f'前端仍依赖已删除的具体类型 Kind：{path.relative_to(root)}')
    kinds = (root / 'source/magel_types.h').read_text(encoding='utf-8').split('enum Kind', 1)[1].split('} kind', 1)[0]
    if re.search(r'^\s*(?:String|BigInt|BigFloat|File|Proc)\s*,', kinds, re.M):
        failures.append('MType::Kind 仍声明具体标准对象类型')
    for path in sources:
        if not path.is_file():
            continue
        for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            if forbidden.search(line):
                failures.append(f'{path.relative_to(root)}:{number}: {line.strip()}')
    for path in (directory / 'stackvm_glue.hpp', directory / 'stackvm_glue.cpp'):
        if path.exists():
            failures.append(f'文件适配器仍位于 VM 目录：{path.relative_to(root)}')
    cmake = (directory / 'CMakeLists.txt').read_text(encoding='utf-8')
    if '../bignum' in cmake or 'stackvm_glue.cpp' in cmake:
        failures.append('VM 构建仍直接包含具体标准对象后端')
    assert not failures, '\n'.join(failures)
    print(f'opcode VM isolation: {len(sources)} core files checked')


if __name__ == '__main__':
    main()
