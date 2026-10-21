"""统一 caspp 独立编译单元和可导出资产清单。"""
from pathlib import Path
import argparse

DIRECTORY = Path(__file__).resolve().parents[1] / 'library/caspp'


def source_names():
    """验证清单仅含本库可独立编译的实现，并排除命令行入口。"""
    names = (DIRECTORY / 'core_sources.txt').read_text(encoding='utf-8').splitlines()
    if len(names) != len(set(names)):
        raise ValueError('CAS 编译单元清单重复')
    for name in names:
        if not name.startswith('src/') or '..' in Path(name).parts or name == 'src/main.cpp' or not name.endswith('.cpp') or not (DIRECTORY / name).is_file():
            raise ValueError(f'CAS 编译单元无效：{name}')
    return names


def asset_names():
    """取得源码、公开及私有接口和许可证，供内嵌和工程导出共用。"""
    return sorted(set(source_names() + ['core_sources.txt', 'core_assets.hpp', 'LICENSE'] +
                      [p.relative_to(DIRECTORY).as_posix() for p in (DIRECTORY / 'include').rglob('*') if p.suffix in ['.h', '.hpp']]))


def main():
    """输出编译清单，或生成并校验 C++ 资产表。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-assets', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    expected = '#pragma once\nnamespace Cas::Assets\n{\n/**\n * @brief 由 tools/cas_sources.py 生成的完整库资产清单。\n */\ninline constexpr const char *kCoreAssets[] = {\n'
    expected += ''.join(f'    "{name}",\n' for name in asset_names()) + '};\n}\n'
    path = DIRECTORY / 'core_assets.hpp'
    if args.check:
        assert path.read_text(encoding='utf-8') == expected, '运行 python tools/cas_sources.py --build-assets'
    elif args.build_assets:
        if not path.exists() or path.read_text(encoding='utf-8') != expected:
            path.write_text(expected, encoding='utf-8')
    else:
        print(' '.join(source_names()))


if __name__ == '__main__':
    main()
