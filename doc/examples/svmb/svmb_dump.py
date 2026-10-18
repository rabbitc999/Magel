#!/usr/bin/env python3
"""最小 .svmb 解析器：打印头部、各节条目数与所有内嵌字符串。

只为文档/取证用，不做执行。用法：python tmp/svmb_dump.py <file.svmb> [--strings]
"""

import sys
from collections import Counter

MAGIC = b"PESVMB\r\n"
V1, V2, V3, V4 = 1, 2, 3, 4


class Reader:
    def __init__(self, data, pos=0):
        self.d = data
        self.p = pos

    def take(self, n):
        out = self.d[self.p:self.p + n]
        if len(out) != n:
            raise EOFError(f"越界读 {n} 字节（剩余 {len(self.d) - self.p}）")
        self.p += n
        return out

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return int.from_bytes(self.take(2), "little")

    def u32(self):
        return int.from_bytes(self.take(4), "little")

    def u64(self):
        return int.from_bytes(self.take(8), "little")

    def var_uint(self):
        shift = 0
        value = 0
        while True:
            byte = self.u8()
            value |= (byte & 0x7F) << shift
            if not byte & 0x80:
                return value
            shift += 7

    def var_int(self):
        raw = self.var_uint()
        return (raw >> 1) ^ -(raw & 1)

    def text(self):
        return self.take(self.u32()).decode("utf-8", "replace")

    def var_text(self):
        return self.take(self.var_uint()).decode("utf-8", "replace")


def parse(path):
    blob = open(path, "rb").read()
    r = Reader(blob)
    assert r.take(8) == MAGIC, "magic 不符"
    version, flags = r.u16(), r.u16()
    payload_size = r.u64()
    checksum = r.u32()
    out = {
        "size": len(blob),
        "version": version,
        "flags": flags,
        "payload_size": payload_size,
        "checksum": checksum,
        "sections": [],
        "strings": [],
        "opcodes": Counter(),
    }

    def add(name, count, start):
        out["sections"].append((name, count, r.p - start))

    start = r.p
    out["program_name"] = r.text()
    add("程序名", 1, start)

    start = r.p
    count = r.u32()
    formats = Counter()
    for i in range(count):
        op = r.u8()
        operands = r.u8()
        out["opcodes"][op] += 1
        if operands & 1:
            formats[r.u8()] += 1
        if operands & 2:
            r.u8()
        if operands & 4:
            r.var_int()
        if operands & 8:
            r.u64()
        if operands & 16:
            r.var_uint()
        for bit in (32, 64, 128):
            if operands & bit:
                out["strings"].append(r.var_text())
    add("指令", count, start)
    out["formats"] = formats

    start = r.p
    count = r.u32()
    out["labels"] = []
    for _ in range(count):
        name = r.text()
        index = r.u64()
        out["labels"].append((name, index))
        out["strings"].append(name)
    add("标签表", count, start)

    start = r.p
    count = r.u32()
    out["types"] = []
    for _ in range(count):
        variable = r.text()
        type_name = r.text()
        bits = int.from_bytes(r.take(4), "little", signed=True)
        flags_byte = r.u8()
        size = r.u64()
        out["types"].append((variable, type_name, bits, flags_byte, size))
        out["strings"] += [variable, type_name]
    add("类型表", count, start)

    start = r.p
    count = r.u32()
    out["parameters"] = []
    for _ in range(count):
        label = r.text()
        params = [r.text() for _ in range(r.u32())]
        out["parameters"].append((label, params))
        out["strings"] += [label] + params
    add("形参表", count, start)

    start = r.p
    count = r.u32()
    out["exports"] = [r.text() for _ in range(count)]
    out["strings"] += out["exports"]
    add("导出表", count, start)

    out["debug"] = []
    if version >= V4:
        start = r.p
        count = r.u32()
        for _ in range(count):
            index = r.var_uint()
            function = r.var_text()
            file_name = r.var_text()
            line = r.var_uint()
            quad_line = r.var_uint()
            out["debug"].append((index, function, file_name, line, quad_line))
            out["strings"] += [function, file_name]
        add("调试信息", count, start)

    out["parsed_bytes"] = r.p
    return out


def main():
    path = sys.argv[1]
    info = parse(path)
    print(f"文件           {path}")
    print(f"总大小         {info['size']} 字节（payload {info['payload_size']}）")
    print(f"magic/version  PESVMB\\r\\n / v{info['version']} (flags={info['flags']})")
    print(f"校验和         0x{info['checksum']:08X}")
    print(f"程序名         {info['program_name']!r}")
    print(f"解析到字节     {info['parsed_bytes']}")
    print()
    print("节                     条目数      字节       占比")
    for name, count, size in info["sections"]:
        print(f"{name:<20} {count:>8}  {size:>9}  {size / info['size'] * 100:5.1f}%")
    print()
    print(f"不同操作码     {len(info['opcodes'])} 种，共 {sum(info['opcodes'].values())} 条指令")
    print(f"打印格式分布   {dict(info['formats'])}")
    print(f"标签 {len(info.get('labels', []))}，类型 {len(info.get('types', []))}，"
          f"形参表 {len(info.get('parameters', []))}，导出 {len(info.get('exports', []))}，"
          f"调试 {len(info.get('debug', []))}")
    print()
    uniq = sorted(set(info["strings"]))
    print(f"内嵌字符串     {len(info['strings'])} 处 / {len(uniq)} 个不同")
    printable = [s for s in uniq if s.isprintable() and s]
    print(f"可直接阅读的   {len(printable)} 个，示例：")
    for s in printable[:40]:
        print(f"  {s!r}")


if __name__ == "__main__":
    main()
