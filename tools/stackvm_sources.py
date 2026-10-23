"""统一 StackVM 独立编译单元与内嵌资产清单。"""
from pathlib import Path
import argparse
DIRECTORY = Path(__file__).resolve().parents[1] / "source/stackvm"

def source_names():
    """读取并验证核心源文件清单。"""
    names = (DIRECTORY / "core_sources.txt").read_text(encoding="utf-8").splitlines()
    if len(names) != len(set(names)) or any(Path(n).name != n or not n.endswith(".cpp") or not (DIRECTORY / n).is_file() for n in names):
        raise ValueError("StackVM 源文件清单包含重复、无效路径或缺失文件")
    return names

def asset_names():
    """返回导出工程所需的实现、接口和清单。"""
    return sorted(set(source_names() + [p.name for p in DIRECTORY.glob("*.hpp")] + ["core_assets.hpp", "core_sources.txt"]))

def render_assets():
    """从统一清单生成 C++ 资产名称表。"""
    lines = ["#pragma once", "", "namespace stackvm::assets", "{", "/**", " * @brief 由 tools/stackvm_sources.py 生成的独立运行时资产清单。", " */", "inline constexpr const char *kCoreAssets[] = {"]
    lines += ['    "' + name + '",' for name in asset_names()]
    return "\n".join(lines + ["};", "}  // namespace stackvm::assets", ""])

def main():
    """输出构建源文件名，或生成并检查导出资产名称表。"""
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-assets", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    path = DIRECTORY / "core_assets.hpp"
    expected = render_assets()
    actual = path.read_text(encoding="utf-8") if path.exists() else ""
    if args.check:
        if actual != expected:
            raise SystemExit("StackVM 资产清单未同步：运行 python tools/stackvm_sources.py --build-assets")
    elif args.build_assets:
        if actual != expected:
            path.write_text(expected, encoding="utf-8")
    else:
        print(" ".join(source_names()))

if __name__ == "__main__":
    main()
