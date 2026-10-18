#!/usr/bin/env python3
"""把含可读名字的原始 .svmb 转为无名变长符号容器。"""
import argparse
import bisect
import json
from pathlib import Path
import struct
import sys
import zlib
from svmb_format import MAX_FILE, MAX_ITEMS, fnv64, read_raw, varuint, varint

# 必须与 stackvm::Op::Call 一致；回归测试检查 Python/C++ 输出逐字节相同。
CALL = 48

def compress(program, module="", version=""):
    names = {}
    def add(name, kind=0):
        entry = names.setdefault(name, dict(original=name, kind=0, flags=0,
            offset=0, size=0, line=0, ref_count=0))
        entry["kind"] |= kind
        return entry
    for i in program["code"]:
        if i["name"]:
            kind = 8 if i["op"] == CALL and i["name"] not in program["labels"] else 1
            add(i["name"], kind)["ref_count"] += 1
        if i["name2"]: add(i["name2"], 1)["ref_count"] += 1
    locations = program["debug"]
    positions = [e[0] for e in locations]
    for name, offset in program["labels"].items():
        s = add(name, 2)
        s["offset"] = offset
        j = bisect.bisect_right(positions, offset) - 1
        if j >= 0: s["line"] = locations[j][3] or locations[j][4]
    for name, (typename, bits, flags, size) in program["types"].items():
        add(name, 1)["size"] = size
        add(typename, 4)
    for name, params in program["parameters"].items():
        add(name, 2)
        for p in params: add(p, 1)
    for name in program["exports"]: add(name, 16)
    if len(names) > MAX_ITEMS: raise ValueError("符号数超过上限")
    ids = set()
    for name, s in names.items():
        value = fnv64(name)
        if value in ids: raise ValueError("FNV-1a 稳定编号发生碰撞")
        ids.add(value)
        s["logical_id"] = str(value)
    records = sorted(names.values(), key=lambda s: (-s["ref_count"], int(s["logical_id"])))
    for index, s in enumerate(records): s["physical_idx"] = index
    def ref(name): return varuint(names[name]["physical_idx"])
    payload = bytearray(varuint(fnv64(program["name"])))
    payload += varuint(len(program["code"]))
    for index, i in enumerate(program["code"]):
        fields = [(i["format"] != 0, bytes([i["format"]])),
                  (i["flags"] != 0, bytes([i["flags"]])),
                  (i["integer"] != 0, varint(i["integer"])),
                  (i["real"] != b"\0"*8, i["real"]),
                  (i["quad"] != index, varuint(i["quad"])),
                  (bool(i["name"]), ref(i["name"]) if i["name"] else b""),
                  (bool(i["name2"]), ref(i["name2"]) if i["name2"] else b"")]
        text = i["text"].encode("utf-8")
        fields.append((bool(text), varuint(len(text)) + text))
        payload += bytes([i["op"], sum((1 << n) for n, (present, _) in enumerate(fields) if present)])
        for present, data in fields:
            if present: payload += data
    payload += varuint(len(program["labels"]))
    for name, offset in sorted(program["labels"].items()):
        payload += ref(name) + varuint(offset)
    payload += varuint(len(program["types"]))
    for name, (typename, bits, flags, size) in sorted(program["types"].items()):
        payload += ref(name) + ref(typename) + varint(bits) + bytes([flags]) + varuint(size)
    payload += varuint(len(program["parameters"]))
    for name, params in sorted(program["parameters"].items()):
        payload += ref(name) + varuint(len(params))
        for p in params: payload += ref(p)
    payload += varuint(len(program["exports"]))
    for name in sorted(program["exports"]): payload += ref(name)
    payload += b"\0"  # 调试位置仅在 PC 侧映射中保存。
    payload += varuint(len(program["internal"]))
    for origin, op, kind, span, immediate, bits, flags, target in program["internal"]:
        payload += (varuint(origin) + bytes([op, kind]) + varuint(span)
                    + varint(immediate) + varint(bits) + bytes([flags]) + varuint(target))
    table = bytearray()
    for s in records:
        table += varuint(int(s["logical_id"])) + varuint(s["physical_idx"])
        table += bytes([s["kind"], s["flags"]]) + varuint(s["offset"]) + varuint(s["size"])
    body = payload + table
    if len(body) > MAX_FILE - 44: raise ValueError("压缩结果超过文件长度上限")
    header = struct.pack("<8s9I", b"MCVM0005", 5, 0,
        sum(bool(s["kind"] & 16) for s in records), sum(bool(s["kind"] & 8) for s in records),
        len(payload), 0, 44 + len(payload), len(table), zlib.crc32(body))
    mapping = dict(module=module or program["name"], version=str(version), symbols=[
        {key:s[key] for key in ("logical_id", "physical_idx", "original", "kind", "line", "ref_count")}
        for s in records])
    return header + body, mapping

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "output", "map", "module", "version"): p.add_argument("--" + name, required=True)
    args = p.parse_args()
    try:
        paths = [Path(value).resolve() for value in (args.input, args.output, args.map)]
        if len(set(paths)) != 3: raise ValueError("输入、输出和映射路径必须不同")
        if paths[0].stat().st_size > MAX_FILE: raise ValueError("输入超过文件长度上限")
        raw = paths[0].read_bytes()
        output, mapping = compress(read_raw(raw), args.module, args.version)
        paths[2].write_text(json.dumps(mapping, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        paths[1].write_bytes(output)
        print(f"{len(raw)} -> {len(output)} bytes; saved {(1-len(output)/len(raw))*100:.2f}%")
        return 0
    except (ValueError, OSError, UnicodeError, struct.error) as e:
        print(str(e), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
