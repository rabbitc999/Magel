#!/usr/bin/env python3
"""验证 Python/C++ 输出一致、映射双向反查、异常拒绝和 CLI/chali 接入。"""
import argparse
import json
import re
from pathlib import Path
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from svmb_format import FEATURE_REFLECTION, Reader, fnv32, fnv64, varuint
from svmb_compress import compress
from svmb_format import read_raw

def run(*args, success=True, cwd=None):
    result = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True)
    if (result.returncode == 0) != success:
        raise AssertionError((args, result.returncode, result.stdout, result.stderr))
    return result.stdout.decode("utf-8", errors="replace")

def verify(build, chali=None):
    tool = build / "svmb_symtool.exe"
    vm = build / "chali-stackvm.exe"
    if not tool.exists():
        tool, vm = build / "svmb_symtool", build / "chali-stackvm"
    folder = build / "symbol-fixtures"
    for value in (0,127,128,16383,16384,2**32-1,2**63-1,2**64-1):
        encoded = varuint(value)
        assert Reader(encoded).varuint() == value
    for data in (b"\x80", b"\x80\x00", b"\xff"*9+b"\x02", b"\x80"*10+b"\x00"):
        try: Reader(data).varuint()
        except ValueError: pass
        else: raise AssertionError("非法 LEB128 被接受")
    rows = []
    for count in (10,1000,100000):
        raw_path = folder / f"{count}.raw.svmb"
        path = folder / f"{count}.svmb"
        output, mapping = compress(read_raw(raw_path.read_bytes()), "benchmark", "1")
        assert output == path.read_bytes(), "Python/C++ 字节不一致"
        cpp_map = json.loads((folder / f"{count}.map.json").read_text(encoding="utf-8"))
        assert mapping == cpp_map
        assert b"calculate_symbol_" not in output
        for symbol in mapping["symbols"]:
            assert int(symbol["logical_id"]) == fnv64(symbol["original"])
        original = mapping["symbols"][5]["original"]
        assert original in run(tool, "lookup", 5, "--map", folder / f"{count}.map.json")
        assert str(fnv64(original)) in run(tool, "lookup", original, "--map", folder / f"{count}.map.json")
        run(tool, "lookup", count, "--map", folder / f"{count}.map.json", success=False)
        print(run(tool, "stats", path).strip())
        refs = sum(s["ref_count"] for s in mapping["symbols"])
        ref_bytes = sum(len(varuint(s["physical_idx"]))*s["ref_count"] for s in mapping["symbols"])
        rows.append(dict(symbols=count, references=refs, raw_bytes=raw_path.stat().st_size,
            compact_bytes=len(output), fixed16_model=len(output)-ref_bytes+refs*2 if count <= 65536 else None,
            fixed32_model=len(output)-ref_bytes+refs*4))
    # 新头部、越界引用、非连续物理编号、非最短编码、尾部垃圾、CRC。
    valid = (folder / "10.svmb").read_bytes()
    table_offset = struct.unpack_from("<I", valid, 32)[0]
    mutants = []
    bad = bytearray(valid); bad[12] = 1; mutants.append(bad)
    bad = bytearray(valid); bad[16] = 1; mutants.append(bad)
    bad = bytearray(valid); bad[32:36] = struct.pack("<I", 43); mutants.append(bad)
    bad = bytearray(valid); bad[36:40] = struct.pack("<I", 2**32-1); mutants.append(bad)
    r = Reader(valid[44:table_offset]); r.varuint(); r.varuint()
    bad = bytearray(valid); bad[44+r.pos+2] = 127; mutants.append(bad)
    table = Reader(valid[table_offset:]); table.varuint()
    bad = bytearray(valid); bad[table_offset+table.pos] = 2; mutants.append(bad)
    bad = bytearray(valid); bad[table_offset] = 0; mutants.append(bad)
    mutants.extend(bytearray(valid[:length]) for length in (0,8,43,len(valid)-1))
    mutants.append(bytearray(valid+b"x"))
    for index, bad in enumerate(mutants):
        if len(bad) >= 44:
            struct.pack_into("<I", bad, 40, zlib.crc32(bad[44:]))
        path = folder / f"malformed-{index}.svmb"
        path.write_bytes(bad)
        run(tool, "stats", path, success=False)
    # 已有指令示例压缩前后的真实执行输出必须一致。
    for name in ("minimal", "01_types", "03_float", "06_muldiv", "08_call", "10_signed", "13_heap"):
        source = ROOT / f"source/stackvm/examples/{name}.quad"
        raw = folder / f"{name}.raw.svmb"
        compact = folder / f"{name}.compact.svmb"
        run(vm, "compile", source, "--out", raw)
        run(vm, "compile", source, "--out", compact, "--compress-symbols")
        input_match = re.search(r"^# 输入:\s*(.*)$", source.read_text(encoding="utf-8"), re.M)
        options = ["--input", input_match.group(1)] if input_match else []
        assert run(vm, "run", raw, *options) == run(vm, "run", compact, *options), name
    # 旧格式明确拒绝；从同一份指令重新编译当前格式，两套转换器必须一致。
    legacy = ROOT / "doc/examples/svmb/control-stripped.svmb"
    try:
        read_raw(legacy.read_bytes())
    except ValueError:
        pass
    else:
        raise AssertionError("旧格式不应被当前转换器接受")
    run(tool, "compress", legacy, "--output", folder / "legacy-rejected.svmb", success=False)
    raw = folder / "control-current.svmb"
    run(vm, "compile", ROOT / "doc/examples/svmb/control.quad", "--out", raw)
    out, mapping = compress(read_raw(raw.read_bytes()), "control", "1")
    run(tool, "compress", raw, "--output", folder / "control.cpp.svmb",
        "--map", folder / "control.cpp.map.json", "--module", "control", "--version", "1")
    assert out == (folder / "control.cpp.svmb").read_bytes()
    assert mapping == json.loads((folder / "control.cpp.map.json").read_text(encoding="utf-8"))
    print("control-stripped:", raw.stat().st_size, "->", len(out))
    (folder / "space-results.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")
    if chali:
        project = folder / "chali"; project.mkdir(exist_ok=True)
        source = project / "main.chl"
        source.write_text("int64 calculate(int64 x) { return x + 1; }\n"
            'void main() { std::lang::println("{}", calculate(41)); }\n', encoding="utf-8")
        manifest = project / "chali.toml"
        manifest.write_text('[package]\nname = "symbols"\nversion = "1"\n[chali]\n'
            'run = "svmb"\ncompress_symbols = true\nsymbol_map = "custom.json"\n', encoding="utf-8")
        prefix = project / "output"
        run(chali, "--out", prefix, source, "--incremental")
        output = project / "output.svmb"
        assert output.read_bytes().startswith(b"MCVM0005")
        assert (project / "custom.json").exists()
        metadata = json.loads((project / "custom.json").read_text(encoding="utf-8"))
        assert metadata["module"] == "symbols" and metadata["version"] == "1"
        assert b"calculate" not in output.read_bytes()
        # 缺映射必须触发重建。
        (project / "custom.json").unlink()
        run(chali, "--out", prefix, source, "--incremental")
        assert (project / "custom.json").exists()
        old_functions = {item["original"]: item["logical_id"] for item in metadata["symbols"] if item["kind"] & 2}
        source.write_text(source.read_text(encoding="utf-8") +
            "int64 calculate_added_function() { return 42; }\n", encoding="utf-8")
        run(chali, "--out", prefix, source, "--compress-symbols", "--incremental")
        updated = json.loads((project / "custom.json").read_text(encoding="utf-8"))
        current = {item["original"]: item["logical_id"] for item in updated["symbols"]}
        assert all(current.get(name) == value for name, value in old_functions.items())
        run(chali, "--out", prefix, source, "--no-compress-symbols", "--incremental")
        assert output.read_bytes().startswith(b"PESVMB")
        # CLI 开启压缩，并用自己的示例生成真实对比。
        run(chali, "svmb", ROOT / "example/samples/control.chl", "--out", folder / "current.raw")
        run(chali, "svmb", ROOT / "example/samples/control.chl", "--out", folder / "current",
            "--compress-symbols")
        raw = folder / "current.raw.svmb"; compact = folder / "current.svmb"
        raw_bytes = raw.read_bytes()
        reflected = read_raw(raw_bytes)
        assert reflected["features"] & FEATURE_REFLECTION
        assert reflected["reflection"]
        assert compress(reflected)[0] == compact.read_bytes()
        # 保留默认反射，同时拒绝损坏的反射结构、未知特性及段尾垃圾。
        metadata_start = len(raw_bytes) - len(reflected["reflection"])
        reflected_mutants = []
        bad = bytearray(raw_bytes); bad[metadata_start] ^= 1; reflected_mutants.append(bad)
        bad = bytearray(raw_bytes); bad[metadata_start + 4] = 2; reflected_mutants.append(bad)
        bad = bytearray(raw_bytes); struct.pack_into("<I", bad, metadata_start + 8, 4097); reflected_mutants.append(bad)
        bad = bytearray(raw_bytes); struct.pack_into("<H", bad, 10, 0); reflected_mutants.append(bad)
        bad = bytearray(raw_bytes); struct.pack_into("<H", bad, 10, 0x8002); reflected_mutants.append(bad)
        reflected_mutants.extend((bytearray(raw_bytes[:-1]), bytearray(raw_bytes + b"x")))
        for index, bad in enumerate(reflected_mutants):
            struct.pack_into("<Q", bad, 12, len(bad) - 24)
            struct.pack_into("<I", bad, 20, fnv32(bad[24:]))
            try:
                read_raw(bad)
            except ValueError:
                pass
            else:
                raise AssertionError("损坏的反射产物被 Python 转换器接受")
            malformed = folder / f"reflection-malformed-{index}.svmb"
            malformed.write_bytes(bad)
            run(tool, "compress", malformed, "--output", folder / f"reflection-invalid-{index}.svmb", success=False)
        baseline = run(vm, "run", raw, "--data", folder / "current.raw.data.bin",
            "--layout", folder / "current.raw.layout")
        converted = run(vm, "run", compact, "--data", folder / "current.data.bin",
            "--layout", folder / "current.layout")
        assert baseline == converted and "sum 1..5 = 15" in converted
        print("current control:", raw.stat().st_size, "->", compact.stat().st_size)
        print(run(tool, "stats", compact).strip())
    print("跨语言、异常输入、双向反查与工具链接入验证全部通过")

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--chali", type=Path)
    args = p.parse_args()
    verify(args.build.resolve(), args.chali.resolve() if args.chali else None)

if __name__ == "__main__": main()
