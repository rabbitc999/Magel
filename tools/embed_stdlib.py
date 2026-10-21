#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 Magel 标准库 .mgl 源码 + 生成器运行时资产内嵌进编译器。

读取（路径都以本项目根为基准）：
  * lang.mgl、std/*.mgl —— 一张
    {模块名 → 源码} 静态表（加载器 magel_module.hpp 先查它，命中就不读文件）；
  * 生成独立程序要用的运行时资产（stackvm 独立模块、胶水、C
    分配器、大数后端）—— 一张 {仓库相对路径 → 源码} 静态表。独立编译器生成器读磁盘失败时
    回退到这张表，于是**单个生成器二进制不依赖仓库目录**也能导出完整程序。

生成 magel_stdlib_embedded.cpp（两张表 + 查询函数），由
独立编译单元编入，声明位于 magel_stdlib.hpp。

文本不再以原始字符串字面量摊在 `.rodata` 里，而是**整体拼接后用 lztext 压缩**，
只把压缩后的字节内联进来，运行时由 table() / generatorAssetTable() 解压重建
（实测 1.48 MB 资产 -> 288 KB，比率 0.185）。压缩算法只有一份 C++ 实现
（lztext/），本脚本调用 `lztext_pack` 命令行工具完成压缩，不在 Python 里重复实现。
行尾仍按 CRLF→LF 归一，与直接读文件的结果保持一致。
"""

import io
from stackvm_sources import asset_names
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

DELIM = "PGX"
CHUNK_SIZE = 8000
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAGEL = ROOT


def raw_chunks(content):
    return [
        'R"%s(%s)%s"' % (DELIM, content[i:i + CHUNK_SIZE], DELIM)
        for i in range(0, len(content), CHUNK_SIZE)
    ] or ['R"%s()%s"' % (DELIM, DELIM)]


def pack_tool_path():
    """定位 lztext_pack 命令行工具；找不到就现场编译一个。"""
    env = os.environ.get("LZTEXT_PACK")
    if env and os.path.isfile(env):
        return env
    for candidate in ("build/lztext/lztext-pack.exe", "build/lztext/lztext-pack"):
        path = os.path.join(ROOT, candidate)
        if os.path.isfile(path):
            return path
    compiler = shutil.which("g++") or shutil.which("c++") or shutil.which("clang++")
    if compiler is None:
        raise RuntimeError("找不到 lztext_pack，也没有可用的 C++ 编译器；"
                           "先跑 `make -C library/lztext pack`，或设置 LZTEXT_PACK 指向该工具")
    src_dir = os.path.join(ROOT, "library", "lztext")
    out_dir = os.path.join(ROOT, "build", "lztext")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir,
                            "lztext-pack.exe" if os.name == "nt" else "lztext-pack")
    subprocess.check_call([compiler, "-std=c++17", "-O2", "-I", src_dir,
                           os.path.join(src_dir, "lztext_pack.cpp"),
                           os.path.join(src_dir, "lztext.cpp"),
                           "-o", out_path])
    return out_path


def compress_bundle(blob):
    """用 lztext_pack 压缩一段字节；工具自带解压回读比对，产物一定可解。"""
    tool = pack_tool_path()
    with tempfile.TemporaryDirectory() as workdir:
        in_path = os.path.join(workdir, "in.bin")
        out_path = os.path.join(workdir, "out.bin")
        with open(in_path, "wb") as handle:
            handle.write(blob)
        subprocess.check_call([tool, in_path, out_path])
        with open(out_path, "rb") as handle:
            return handle.read()


def byte_array(name, data):
    """把字节数组写成 C++ 初始化列表，每行 24 个。"""
    lines = ["const unsigned char %s[] = {" % name]
    for i in range(0, len(data), 24):
        chunk = data[i:i + 24]
        lines.append("    " + ", ".join("0x%02x" % b for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def bundle(entries):
    """把 [(键, 文本)] 拼成一个大块（每段以 '\\0' 结尾），返回 (拼接结果, [(键, 偏移)])。"""
    slices = []
    parts = []
    cursor = 0
    for key, content in entries:
        data = content.encode("utf-8") + b"\0"
        slices.append((key, cursor))
        parts.append(data)
        cursor += len(data)
    return b"".join(parts), slices


def read(path):
    with open(path, "r", encoding="utf-8") as src:
        content = src.read()
    assert ')PGX"' not in content, path
    # 行尾统一 LF：C++ 编译器读源码里的原始字符串也会把 CRLF 归一成 LF，
    # 这里先归一，内联表与直接读文件的结果就一致了。
    return content.replace("\r\n", "\n").replace("\r", "\n")


# 资产键（生成工程用的逻辑路径）保持原样，只把「真仓库里的磁盘位置」映射过去：
# 搬进 source/ 与 library/ 之后，键与磁盘目录不再同名，这里显式对上。
ASSET_ROOT = {
    "stackvm/": "library/",
    "simple_ast/": "library/",
    "preprocess/": "library/",
    "bignum/": "library/",
    "json/": "library/",
    "toml/": "library/",
    "lztext/": "library/",
    "sqlite/": "library/",
    "console/": "library/",
    "glue/": "source/",
    "allocator/": "source/",
}


def asset_disk_path(relative):
    """把资产键翻成本仓库里的实际路径。"""
    if relative.startswith("tools/"):
        return relative
    for prefix, root in ASSET_ROOT.items():
        if relative.startswith(prefix):
            return root + relative
    return "source/" + relative


# 生成器（magel-generator）在运行期需要的运行时资产。相对路径键与仓库布局一致，
# 磁盘读失败时按"整段后缀"或"唯一 basename"回退到这里的文本。
GENERATOR_ASSETS = [
    "tools/magel_manual.json",
    "tools/magel_stdlib_api.json",
    "magel_glue.hpp",
    "magel_generic_name.hpp",
    "magel_runtime_allocator.hpp",
    "magel_i18n.hpp",
    "console/utf8.hpp",
    "glue/std_glue.cpp",
    "glue/std_demo.cpp",
    "glue/std_math.hpp",
    "glue/std_random.hpp",
    "glue/std_regex.hpp",
    "glue/std_signal.hpp",
    "glue/std_numeric.hpp",
    "glue/std_native_record.hpp",
    "glue/std_collection.hpp",
    "glue/std_scalar_buffer.hpp",
    "glue/std_collection_storage.hpp",
    "glue/std_big_number.hpp",
    "glue/std_file.hpp",
    "glue/std_file_api.hpp",
    "glue/std_desktop_file.hpp",
    "glue/std_structured_data.hpp",
    "glue/std_json.hpp",
    "glue/std_toml.hpp",
    "json/json.hpp",
    "json/json.cpp",
    "toml/toml.hpp",
    "toml/toml.cpp",
    "toml/toml_source.cpp",
    "toml/toml_source.hpp",
    "glue/std_binary.hpp",
    "glue/std_compress.hpp",
    "lztext/lztext.hpp",
    "lztext/lztext.cpp",
    "glue/std_linalg.hpp",
    "glue/std_parse.hpp",
    "glue/std_process.hpp",
    "glue/std_sql.hpp",
    "sqlite/sqlite3.c",
    "sqlite/sqlite3.h",
    "glue/std_net.hpp",
    "glue/std_image.hpp",
    "glue/std_tree.hpp",
    "glue/std_graph.hpp",
    "glue/std_linkedlist.hpp",
    "glue/std_kmp.hpp",
    "glue/std_registry.hpp",
    "glue/std_filesystem.hpp",
    "allocator/magel_pool.h",
    "allocator/magel_pool.c",
    "allocator/magel_allocator.hpp",
    "bignum/bigint.hpp",
    "bignum/bigint.cpp",
    "bignum/bigfloat.hpp",
    "bignum/bigfloat.cpp",
]


GENERATOR_ASSETS += ["stackvm/" + name for name in asset_names()]

# 胶水接口及实现作为同一组资产导出，固定清单不能遗漏独立编译单元。
GENERATOR_ASSETS += [
    directory + "/" + path.name
    for directory in ("macro", "literals", "syntax", "editor")
    for path in sorted(Path(MAGEL, "source", directory).iterdir())
    if path.suffix in (".cpp", ".hpp")
]
GENERATOR_ASSETS = list(dict.fromkeys(GENERATOR_ASSETS + [
    "glue/" + path.name for path in sorted(Path(MAGEL, "source/glue").iterdir())
    if path.suffix in (".cpp", ".hpp", ".h", ".c")
]))


def collect_generator_assets():
    assets = []
    for relative in GENERATOR_ASSETS:
        assets.append((relative, read(os.path.join(MAGEL, asset_disk_path(relative)))))
    return assets


def collect():
    modules = []  # (name, content)
    std_dir = os.path.join(MAGEL, "source", "std")
    for name in sorted(os.listdir(std_dir)):
        if name.endswith(".mgl"):
            modules.append(("std::" + os.path.splitext(name)[0], read(os.path.join(std_dir, name))))
    return modules


def dependencies():
    """列出内嵌资产的生成输入，供增量构建精确追踪。"""
    from pathlib import Path
    root = Path(MAGEL)
    paths = {Path(asset_disk_path(name)) for name in GENERATOR_ASSETS}
    for pattern in ("source/std/*.mgl", "doc/book/*.md", "doc/book/images/*", "doc/library/*.md"):
        paths.update(path.relative_to(root) for path in root.glob(pattern) if path.is_file())
    paths.update(map(Path, ("tools/embed_stdlib.py", "tools/stackvm_sources.py", "tools/embed_manual.py", "source/magel.gram",
                           "source/magel_token.gram", "doc/04-language-reference.md",
                           "doc/24-literals-operators-macros.md", "skills/magel-programming/SKILL.md")))
    names = sorted(path.as_posix() for path in paths)
    # Windows GNU make 无法可靠打开非 ASCII 前置文件名，改用固定 ASCII 路径
    # 记录这些文件的精确时间与大小，保持增量依赖可追踪。
    indirect = [name for name in names if not name.isascii() or any(ch.isspace() for ch in name)]
    marker = root / "build/magel-frontend/stdlib_manual_inputs.json"
    import json
    payload = json.dumps([(name, (root / name).stat().st_mtime_ns, (root / name).stat().st_size)
                          for name in indirect], ensure_ascii=False, separators=(",", ":")) + "\n"
    marker.parent.mkdir(parents=True, exist_ok=True)
    if not marker.exists() or marker.read_text(encoding="utf-8") != payload:
        marker.write_text(payload, encoding="utf-8")
    return [name for name in names if name not in indirect] + [marker.relative_to(root).as_posix()]


def main():
    from embed_manual import OUTPUT, generate
    manual = generate()
    if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != manual:
        OUTPUT.write_text(manual, encoding="utf-8")
    modules = collect()
    assets = collect_generator_assets()
    # 整体拼接后再压缩：跨模块、跨资产的重复也能被利用，比率明显好于逐个压缩
    #（实测整体 0.185 vs 逐个约 0.30）。
    modules_blob, modules_slices = bundle(modules)
    modules_packed = compress_bundle(modules_blob)
    assets_blob, assets_slices = bundle(assets)
    assets_packed = compress_bundle(assets_blob)
    out_path = os.path.join(MAGEL, "source", "magel_stdlib_embedded.cpp")
    with io.StringIO() as handle:
        handle.write("// 本文件由 tools/embed_stdlib.py 生成，请勿手工修改。\n")
        handle.write("// 生成命令：python tools/embed_stdlib.py\n\n")
        handle.write('#include "magel_stdlib.hpp"\n')
        handle.write("\n")
        handle.write("// 文本以 lztext 压缩形式内联（运行时解压），算法见 lztext/README.md。\n")
        handle.write("#include <cstddef>\n")
        handle.write("#include <string>\n")
        handle.write("#include <vector>\n\n")
        handle.write('#include "lztext/lztext.hpp"\n\n')
        handle.write("namespace magel_module\n{\nnamespace embedded\n{\n\n")

        handle.write("namespace\n{\n\n")
        handle.write("// 全部模块源码按顺序拼接（每段以 '\\0' 结尾）后压缩的结果："
                     "%d 段，原始 %d 字节，压缩后 %d 字节。\n"
                     % (len(modules), len(modules_blob), len(modules_packed)))
        handle.write(byte_array("kModulesPacked", modules_packed))
        handle.write("\n\nstruct ModuleSlice\n{\n"
                     "    const char *name;    ///< 模块名\n"
                     "    std::size_t offset;  ///< 在解压后缓冲区里的起始下标\n"
                     "};\n\n")
        handle.write("const ModuleSlice kModulesIndex[] = {\n")
        for key, offset in modules_slices:
            handle.write('    {"%s", %d},\n' % (key, offset))
        handle.write("};\n\n")
        handle.write("} // namespace\n\n")
        handle.write("""/**\n * @brief 解压一次标准库源码并返回进程内持久的模块视图。\n * @return 以空项结尾的标准库模块表。\n */\nconst Entry *table()
{
    // 解压一次，之后每个模块都指向这块缓冲区里的某段（各段以 '\\0' 结尾）。
    static const std::string k_text =
        lztext::decompressString(kModulesPacked, sizeof(kModulesPacked));
    static const std::vector<Entry> k_entries = [] {
        std::vector<Entry> entries;
        entries.reserve(sizeof(kModulesIndex) / sizeof(kModulesIndex[0]) + 1u);
        for (const ModuleSlice &slice : kModulesIndex)
        {
            if (slice.offset >= k_text.size()) continue;
            entries.push_back(Entry{slice.name, k_text.data() + slice.offset});
        }
        entries.push_back(Entry{nullptr, nullptr});
        return entries;
    }();
    return k_entries.data();
}

""")
        handle.write("/**\n * @brief 按模块限定名复制内嵌源码。\n * @param name 完整模块名。\n * @param out 成功时接收源码。\n * @return 找到模块时为 true。\n */\nbool find(const Memory::String &name, Memory::String *out)\n{\n")
        handle.write("    for (const Entry *entry = table(); entry->name != nullptr; ++entry)\n")
        handle.write("        if (name == entry->name) { *out = entry->text; return true; }\n")
        handle.write("    return false;\n}\n\n")

        handle.write("namespace\n{\n\n")
        handle.write("// 全部生成器资产按顺序拼接（每段以 '\\0' 结尾）后压缩的结果："
                     "%d 项，原始 %d 字节，压缩后 %d 字节。\n"
                     % (len(assets), len(assets_blob), len(assets_packed)))
        handle.write(byte_array("kAssetsPacked", assets_packed))
        handle.write("\n\nstruct AssetSlice\n{\n"
                     "    const char *path;     ///< 仓库相对路径\n"
                     "    std::size_t offset;   ///< 在解压后缓冲区里的起始下标\n"
                     "};\n\n")
        handle.write("const AssetSlice kAssetsIndex[] = {\n")
        for path, offset in assets_slices:
            handle.write('    {"%s", %d},\n' % (path, offset))
        handle.write("};\n\n")
        handle.write("} // namespace\n\n")
        handle.write("""/**\n * @brief 解压一次生成器资产并创建稳定的内容视图。\n * @return 以空项结尾的工程资产表。\n */\nconst GeneratorAsset *generatorAssetTable()
{
    static const std::string k_text =
        lztext::decompressString(kAssetsPacked, sizeof(kAssetsPacked));
    static const std::vector<GeneratorAsset> k_entries = [] {
        std::vector<GeneratorAsset> entries;
        entries.reserve(sizeof(kAssetsIndex) / sizeof(kAssetsIndex[0]) + 1u);
        for (const AssetSlice &slice : kAssetsIndex)
        {
            if (slice.offset >= k_text.size()) continue;
            entries.push_back(GeneratorAsset{slice.path, k_text.data() + slice.offset});
        }
        entries.push_back(GeneratorAsset{nullptr, nullptr});
        return entries;
    }();
    return k_entries.data();
}

""")
        handle.write("/**\n * @brief 取得兼容正反斜杠的路径末尾文件名。\n * @param path 原始资产路径。\n * @return 去除父目录后的文件名。\n */\nMemory::String generatorAssetTail(const Memory::String &path)\n{\n")
        handle.write("    const std::size_t slash = path.find_last_of(\"/\\\\\");\n")
        handle.write("    return (slash == Memory::String::npos) ? path : path.substr(slash + 1u);\n}\n\n")
        handle.write("""/**
 * @brief 按路径后缀或唯一文件名查找内嵌工程资产。
 * @param path 原始资产路径。
 * @param out 成功时接收资产内容。
 * @return 唯一命中时为 true；未找到或存在歧义时为 false。
 */
bool findGeneratorAssetByPath(const Memory::String &path, Memory::String *out)
{
    Memory::String want = path;
    for (char &c : want) if (c == '\\\\') c = '/';
    const Memory::String wantTail = generatorAssetTail(want);
    const GeneratorAsset *baseHit = nullptr;
    std::size_t baseMatches = 0u;
    for (const GeneratorAsset *asset = generatorAssetTable(); asset->path != nullptr; ++asset)
    {
        const Memory::String key(asset->path);
        if (want == key) { *out = asset->text; return true; }
        const Memory::String keyNeedle = Memory::String("/") + key;
        if (want.size() > keyNeedle.size() &&
            want.compare(want.size() - keyNeedle.size(), keyNeedle.size(), keyNeedle) == 0)
        { *out = asset->text; return true; }
        const Memory::String wantNeedle = Memory::String("/") + want;
        if (key.size() > wantNeedle.size() &&
            key.compare(key.size() - wantNeedle.size(), wantNeedle.size(), wantNeedle) == 0)
        { *out = asset->text; return true; }
        if (generatorAssetTail(key) == wantTail) { ++baseMatches; baseHit = asset; }
    }
    if (baseMatches == 1u && baseHit != nullptr) { *out = baseHit->text; return true; }
    return false;
}

""")
        handle.write("} // namespace embedded\n")
        handle.write("} // namespace magel_module\n")

        payload = handle.getvalue().encode("utf-8")
    try:
        with open(out_path, "rb") as existing:
            unchanged = existing.read() == payload
    except FileNotFoundError:
        unchanged = False
    if not unchanged:
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(prefix=".magel-stdlib-", suffix=".tmp",
                                             dir=os.path.dirname(out_path), delete=False) as output:
                temporary = output.name
                output.write(payload)
            os.replace(temporary, out_path)
        finally:
            if temporary is not None and os.path.exists(temporary):
                os.unlink(temporary)
    total = sum(len(content) for _, content in modules)
    asset_bytes = sum(len(content) for _, content in assets)
    print("%s %s (%d modules, %d bytes; %d generator assets, %d bytes)"
          % ("unchanged" if unchanged else "wrote", out_path, len(modules), total, len(assets), asset_bytes))
    return 0


if __name__ == "__main__":
    if sys.argv[1:] == ["--dependencies"]:
        print(" ".join(dependencies()))
    elif len(sys.argv) == 3 and sys.argv[1] == "--stamp":
        status = main()
        if status == 0:
            from pathlib import Path
            Path(sys.argv[2]).touch()
        sys.exit(status)
    else:
        sys.exit(main())
