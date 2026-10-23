"""阻止已剥离的标准对象语义重新进入 VM 核心及其构建依赖。"""
from pathlib import Path
import re


def main():
    """检查核心源码、公共头和静态库定义不再引用具体宿主对象。"""
    root = Path(__file__).resolve().parents[1]
    directory = root / 'source/stackvm'
    forbidden = re.compile(r'\b(?:FileApi|FileHandle|BigInt|BigFloat|BigInteger|BigDecimal)\b|'
                           r'__std_(?:file|bint|bfloat|vec|set|map)|"__std_str_(?:lit|join|sub|of_int|of_flt|cmp|pos)"|ObjectKind::(?:File|Process|Vector|Set|Map|IteratorLease)|'
                           r'STACKVM_WITH_BIG(?:INT|FLOAT)|\bbignum\b|'
                           r'\b(?:makeList|readList|listPush|listPushStruct|readListStruct|makeMap|readMap|mapSet|makeSet|readSet|setInsert)\s*\(')
    sources = [*[directory / name for name in (directory / "core_sources.txt").read_text().splitlines()], *sorted(directory.glob("*.hpp"))]
    failures = []
    frontend = [root / 'source/chali_ast.hpp', root / 'source/chali_semantics.cpp', *sorted((root / 'source/semantics').glob('*.cpp'))]
    legacy_kinds = re.compile(r'MType::(?:String|BigInt|BigFloat|File|Proc|Container)\b')
    for path in frontend:
        source = path.read_text(encoding='utf-8')
        if legacy_kinds.search(source):
            failures.append(f'前端仍依赖已删除的具体类型 Kind：{path.relative_to(root)}')
    kinds = (root / 'source/chali_ast.hpp').read_text(encoding='utf-8').split('enum Kind', 1)[1].split('} kind', 1)[0]
    if re.search(r'^\s*(?:String|BigInt|BigFloat|File|Proc|Container)\s*,', kinds, re.M):
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
