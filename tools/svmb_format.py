"""原始 PESVMB v5 的受限读取；压缩转换器共享的格式原语。"""
import struct

MAX_FILE = 512 * 1024 * 1024
MAX_ITEMS = 10_000_000
MAX_TEXT = 64 * 1024 * 1024

def fnv64(text):
    value = 14695981039346656037
    for byte in text.encode("utf-8"):
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value

def fnv32(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value

def varuint(value):
    if not 0 <= value < 1 << 64:
        raise ValueError("变长整数越界")
    result = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        result.append(byte | (128 if value else 0))
        if not value:
            return result

def varint(value):
    if not -(1 << 63) <= value < 1 << 63:
        raise ValueError("有符号变长整数越界")
    return varuint((value << 1) ^ (value >> 63))

class Reader:
    """所有读取先检查剩余范围；拒绝溢出及非最短变长编码。"""
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, size):
        if size < 0 or size > len(self.data) - self.pos:
            raise ValueError("输入被截断")
        result = self.data[self.pos:self.pos + size]
        self.pos += size
        return result

    def number(self, size):
        return int.from_bytes(self.take(size), "little")

    def varuint(self):
        value = 0
        for i in range(10):
            byte = self.number(1)
            if i == 9 and byte & 128:
                raise ValueError("变长整数超过十字节")
            if i == 9 and byte & 126:
                raise ValueError("变长整数溢出")
            value |= (byte & 127) << (7 * i)
            if not byte & 128:
                if i and byte & 127 == 0:
                    raise ValueError("变长整数不是最短编码")
                return value
        raise ValueError("变长整数没有终止")

    def varint(self):
        value = self.varuint()
        return (value >> 1) ^ -(value & 1)

    def text(self, variable=False):
        size = self.varuint() if variable else self.number(4)
        if size > MAX_TEXT:
            raise ValueError("字符串超过上限")
        return self.take(size).decode("utf-8", errors="strict")

    def count(self):
        count = self.number(4)
        if count > MAX_ITEMS or count > len(self.data) - self.pos:
            raise ValueError("条目数超过上限或剩余长度")
        return count

def read_raw(data):
    if len(data) > MAX_FILE:
        raise ValueError("输入超过 512 MiB")
    header = Reader(data)
    if header.take(8) != b"PESVMB\r\n":
        raise ValueError("需要含原始名字的 PESVMB 输入")
    version, flags, size, checksum = [header.number(n) for n in (2, 2, 8, 4)]
    if version != 5 or flags:
        raise ValueError("仅支持普通 v5 输入，旧产物须重新编译")
    body = header.take(size)
    if header.pos != len(data) or fnv32(body) != checksum:
        raise ValueError("长度或校验和不匹配")
    r = Reader(body)
    result = dict(name=r.text(), code=[], labels={}, types={}, parameters={}, exports=[], debug=[])
    for index in range(r.count()):
        i = dict(op=0, format=0, flags=0, integer=0, real=b"\0"*8,
                 quad=index, name="", name2="", text="")
        if version == 1:
            i["op"], i["format"], i["flags"] = r.number(2), r.number(1), r.number(1)
            i["integer"] = int.from_bytes(r.take(8), "little", signed=True)
            i["real"], i["quad"] = r.take(8), r.number(8)
            i["name"], i["name2"], i["text"] = r.text(), r.text(), r.text()
        else:
            i["op"], mask = r.number(1), r.number(1)
            if mask & 1: i["format"] = r.number(1)
            if mask & 2: i["flags"] = r.number(1)
            if mask & 4: i["integer"] = r.varint()
            if mask & 8: i["real"] = r.take(8)
            if mask & 16: i["quad"] = r.varuint()
            if mask & 32: i["name"] = r.text(True)
            if mask & 64: i["name2"] = r.text(True)
            if mask & 128: i["text"] = r.text(True)
        if i["op"] > 53 or i["format"] > 3 or i["flags"] & ~3 or i["quad"] > MAX_ITEMS:
            raise ValueError("非法指令字段")
        result["code"].append(i)
    def insert(table, key, value):
        if key in table:
            raise ValueError("元数据表中存在重复名字")
        table[key] = value
    for _ in range(r.count()):
        name, offset = r.text(), r.number(8)
        if offset > len(result["code"]): raise ValueError("标签越界")
        insert(result["labels"], name, offset)
    for _ in range(r.count()):
        name, typename = r.text(), r.text()
        bits = int.from_bytes(r.take(4), "little", signed=True)
        flags, size = r.number(1), r.number(8)
        if flags & ~3 or size > MAX_FILE: raise ValueError("非法类型字段")
        insert(result["types"], name, (typename, bits, flags, size))
    for _ in range(r.count()):
        label = r.text()
        insert(result["parameters"], label, [r.text() for _ in range(r.count())])
    result["exports"] = [r.text() for _ in range(r.count())] if version >= 3 else list(result["labels"])
    if len(set(result["exports"])) != len(result["exports"]) or any(
            e not in result["labels"] for e in result["exports"]):
        raise ValueError("非法导出项")
    if version >= 4:
        for _ in range(r.count()):
            entry = (r.varuint(), r.text(True), r.text(True), r.varuint(), r.varuint())
            if entry[0] >= len(result["code"]) or entry[3] > MAX_ITEMS or entry[4] > MAX_ITEMS:
                raise ValueError("非法调试位置")
            if result["debug"] and entry[0] < result["debug"][-1][0]:
                raise ValueError("调试位置没有排序")
            result["debug"].append(entry)
    result["internal"] = []
    for _ in range(r.count()):
        entry = (r.varuint(), r.number(1), r.number(1), r.varuint(),
                 r.varint(), r.varint(), r.number(1), r.varuint())
        origin, op, kind, span, immediate, bits, flags, target = entry
        if (origin + span > len(result["code"]) or op > 53 or kind not in (1, 2)
                or span not in (4, 6) or flags & ~15 or target > len(result["code"]) + 1
                or not -(1 << 31) <= bits < (1 << 31)):
            raise ValueError("非法内部指令")
        result["internal"].append(entry)
    if r.pos != len(body): raise ValueError("输入末尾存在未识别数据")
    return result
