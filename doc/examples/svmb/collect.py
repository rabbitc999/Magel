#!/usr/bin/env python3
"""为 [24 `.svmb` 二进制解剖与逆向难度](../../24-svmb-binary-and-reverse-engineering.md)
重新生成证据文件，写在本目录的 evidence-*.txt 里。

设计口径：**文件里的每个数字都必须由这个脚本跑出来**，不手写。看得到的输入是
`control.svmb`（标本，随本目录入库）和可选的 VM / 导出 exe 路径；看不到的输入一律
不采信。工具缺失时对应那一节写成“跳过（缺 xxx）”，不伪造内容。

用法（在仓库根目录）：
    python magel/doc/examples/svmb/collect.py \
        --parser magel/magel.exe \
        --vm tmp/vm-9700x-pool/x64/Release/magel-stackvm.exe \
        --vm-opcode tmp/vm-9700x-pool/gcc64/magel-stackvm.exe \
        --exported tmp/svmb-study/export/program.exe
"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
sys.path.insert(0, HERE)
import svmb_dump  # noqa: E402  （同目录的最小解析器）

MAGIC = b"PESVMB\r\n"
HEADER_SIZE = 24
EXE = ".exe" if os.name == "nt" else ""


def run(args, cwd=ROOT):
    """跑一条命令，返回 (退出码, 合并后的输出文本)。"""
    try:
        done = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True)
    except OSError as error:
        return 127, f"（执行失败：{error}）"
    text = (done.stdout + done.stderr).decode("utf-8", "replace")
    return done.returncode, text


def fnv1a(data):
    """与 stackvm.cpp 的 binaryChecksum 相同：FNV-1a 32 位。"""
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def sha256(path):
    with open(path, "rb") as handle:
        return hashlib.sha256(handle.read()).hexdigest().upper()


def write(name, lines):
    path = os.path.join(HERE, name)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(str(line) for line in lines).rstrip("\n") + "\n")
    print(f"  写出 {os.path.relpath(path, ROOT)}（{len(lines)} 行）")
    return path


def section_header(lines, title):
    lines.append("")
    lines.append("=" * 78)
    lines.append(title)
    lines.append("=" * 78)


def evidence_container(specimen, parser):
    """标本的头部、SHA-256，以及在能重新编译时用同一份源码复现它的比对结果。"""
    data = open(specimen, "rb").read()
    import struct

    version, flags = struct.unpack_from("<HH", data, 8)
    payload_size = struct.unpack_from("<Q", data, 12)[0]
    checksum = struct.unpack_from("<I", data, 20)[0]
    lines = [
        f"标本            {os.path.relpath(specimen, ROOT)}",
        f"字节数          {len(data)}",
        f"SHA-256         {sha256(specimen)}",
        "",
        "头部（24 字节）：",
        f"  0x00  8  magic        {data[:8]!r}",
        f"  0x08  2  version      {version}",
        f"  0x0A  2  flags        {flags}",
        f"  0x0C  8  payload_size {payload_size}   （文件长度 - 24 = {len(data) - HEADER_SIZE}）",
        f"  0x14  4  checksum     0x{checksum:08X}（FNV-1a 32，实测重算 "
        f"= 0x{fnv1a(data[HEADER_SIZE:]) & 0xFFFFFFFF:08X}）",
        "",
        "原始前 32 字节：",
        "  " + " ".join(f"{b:02x}" for b in data[:32]),
    ]

    if parser and os.path.exists(parser):
        with tempfile.TemporaryDirectory() as work:
            prefix = os.path.join(work, "control")
            code, output = run([parser, "magel/example/samples/control.mgl", "--emit", "--svmb",
                                "--out", prefix])
            rebuilt = prefix + ".svmb"
            lines += ["", f"用 {os.path.relpath(parser, ROOT)} 重新编译 "
                          "magel/example/samples/control.mgl 的结果："]
            if code != 0 or not os.path.exists(rebuilt):
                lines.append(f"  编译失败（退出码 {code}）：{output.strip()[:400]}")
            else:
                lines += compare_with_rebuild(specimen, rebuilt, prefix)
    else:
        lines += ["", "（未提供 --parser，跳过“同源码复现”比对）"]
    return write("evidence-01-container.txt", lines)


def compare_with_rebuild(specimen, rebuilt, prefix):
    """同源码重编的产物 vs 入库标本：逐节比对，并解释体积差。

    调试节里每条都存着编译时的 `.quad` **路径字符串**（`varText`），所以换个目录
    重编，只有调试节会变长——体积差恰好等于“路径长度差 × 调试条数”时，就反过来
    证明了这一点。
    """
    old_raw = open(specimen, "rb").read()
    new_raw = open(rebuilt, "rb").read()
    old = svmb_dump.parse(specimen)
    new = svmb_dump.parse(rebuilt)
    lines = [f"  重新生成 {len(new_raw)} 字节（标本 {len(old_raw)} 字节），SHA-256 "
             f"{'一致' if sha256(rebuilt) == sha256(specimen) else '不一致'}"]

    lines.append("  逐节条目数比对（程序名 / 指令 / 标签 / 类型 / 形参 / 导出 / 调试）：")
    pairs = [("程序名", 1, 1), ("指令", sum(old["opcodes"].values()), sum(new["opcodes"].values())),
             ("标签表", len(old["labels"]), len(new["labels"])),
             ("类型表", len(old["types"]), len(new["types"])),
             ("形参表", len(old["parameters"]), len(new["parameters"])),
             ("导出表", len(old["exports"]), len(new["exports"])),
             ("调试信息", len(old["debug"]), len(new["debug"]))]
    same_counts = all(a == b for _, a, b in pairs)
    for name, before, after in pairs:
        lines.append(f"    {name:<6} {before:>7} → {after:>7}"
                     f"{'' if before == after else '   ← 不同'}")
    lines.append(f"    结论：{'七节条目数完全一致' if same_counts else '有节的条目数变了'}")

    def debug_offset(info):
        """调试节之前的字节数：头部 + 前面各节的字节数。"""
        total = HEADER_SIZE
        for name, _, size in info["sections"]:
            if name == "调试信息":
                break
            total += size
        return total

    old_cut, new_cut = debug_offset(old), debug_offset(new)
    body_same = old_raw[HEADER_SIZE:old_cut] == new_raw[HEADER_SIZE:new_cut]
    lines.append(f"  payload 里调试节之前的 {old_cut - HEADER_SIZE} 字节（程序名 + 指令 + 标签 "
                 f"+ 类型 + 形参 + 导出）：{'逐字节一致' if body_same else '不一致'}")
    lines.append("  （头部前 24 字节里的 payload 长度与校验和必然不同——调试节长度变了；"
                 "剩下的 payload 逐字节一致，说明程序本体没变）")

    old_files = sorted({d[2] for d in old["debug"]})
    new_files = sorted({d[2] for d in new["debug"]})
    old_length = sum(len(f) for f in old_files)
    new_length = sum(len(f) for f in new_files)
    predicted = (new_length - old_length) * len(new["debug"])
    actual = len(new_raw) - len(old_raw)
    lines.append(f"  调试节里的 file 字段：标本 {old_files}")
    lines.append(f"                       重编 {new_files}")
    lines.append(f"  体积差：实测 {actual:+d} 字节；按“路径长度差 "
                 f"({new_length - old_length}) × 调试条数 ({len(new['debug'])})”预测 "
                 f"{predicted:+d} 字节"
                 f"{'——吻合' if abs(actual - predicted) <= len(old_files) else '——不吻合'}")
    lines.append("  也就是说：**调试节给每条指令都存了一份编译时的 .quad 路径**，"
                 "换个目录编译就能让产物变大几百 KB。")
    return lines


def evidence_sections(specimen):
    """直接复用 svmb_dump 的完整输出 + 一份名字样本。"""
    info = svmb_dump.parse(specimen)
    lines = [f"$ python svmb_dump.py {os.path.relpath(specimen, ROOT)}", ""]
    lines.append(f"payload 七节的条目数与体积（总 {info['size']} 字节）")
    lines.append(f"{'节':<8} {'条目':>8} {'字节':>10} {'占比':>7}")
    for name, count, size in info["sections"]:
        lines.append(f"{name:<8} {count:>8} {size:>10} {size / info['size'] * 100:>6.1f}%")
    lines.append("")
    lines.append(f"指令总数        {sum(info['opcodes'].values())}")
    lines.append(f"不同操作码      {len(info['opcodes'])}（枚举共 54 个）")
    lines.append(f"标签 / 类型 / 形参表 / 导出 / 调试："
                 f"{len(info['labels'])} / {len(info['types'])} / "
                 f"{len(info['parameters'])} / {len(info['exports'])} / {len(info['debug'])}")
    uniq = sorted(set(info["strings"]))
    lines.append(f"内嵌字符串      {len(info['strings'])} 处 / {len(uniq)} 个不同")
    lines.append("")
    lines.append("导出表（全部）：")
    lines += [f"  {name}" for name in info["exports"]]
    lines.append("")
    lines.append("标签表前 20 项（名字 → 指令下标）：")
    lines += [f"  {name!r:<44} @{index}" for name, index in info["labels"][:20]]
    lines.append("")
    lines.append("类型表前 20 项（变量名 / 类型名 / 位宽 / 标志 / 字节数）：")
    lines += [f"  {row}" for row in info["types"][:20]]
    lines.append("")
    lines.append("类型表里与用户源码同名的条目：")
    lines += [f"  {row}" for row in info["types"] if row[0] in ("i", "n", "sum", "value")]
    lines.append("")
    lines.append("形参表前 5 项：")
    lines += [f"  {row}" for row in info["parameters"][:5]]
    lines.append("")
    lines.append("调试节前 10 条（指令下标 → 函数 / 文件 / 行 / 四元式行）：")
    lines += [f"  {row}" for row in info["debug"][:10]]
    lines.append(f"  file 取值集合：{sorted({d[2] for d in info['debug']})}")
    lines.append(f"  line 取值分布：{{0: {sum(1 for d in info['debug'] if d[3] == 0)}}}"
                 f"（非零共 {sum(1 for d in info['debug'] if d[3] != 0)} 条）")
    lines.append(f"  四元式行范围：{min(d[4] for d in info['debug'])} ~ "
                 f"{max(d[4] for d in info['debug'])}")
    lines.append(f"  出现过的函数名：{len({d[1] for d in info['debug']})} 个")
    lines.append("")
    lines.append("字符串样本（前 30 个不同的）：")
    lines += [f"  {s!r}" for s in uniq[:30]]
    return write("evidence-02-sections.txt", lines)


def evidence_instructions(specimen):
    """逐字节解码前 12 条指令记录，并给出操作码直方图与变量名样本。"""
    data = open(specimen, "rb").read()
    info = svmb_dump.parse(specimen)
    names = opcode_names()

    import struct

    lines = ["指令区起点 = 24（头部）+ 程序名长度 + 4（计数）"]
    program_name_len = struct.unpack_from("<I", data, HEADER_SIZE)[0]
    start = HEADER_SIZE + 4 + program_name_len + 4
    lines.append(f"程序名 {data[HEADER_SIZE + 4:HEADER_SIZE + 4 + program_name_len]!r}，"
                 f"指令记录从文件偏移 {start} 开始")
    lines.append("")
    lines.append("前 12 条记录的逐字段解码：")
    lines.append(f"  {'下标':>4} {'偏移':>10}  解码")

    position = start
    for index in range(12):
        begin = position
        op = data[position]
        mask = data[position + 1]
        position += 2
        parts = [f"op={op}({names[op] if op < len(names) else '?'})",
                 f"mask=0b{mask:08b}"]
        if mask & 1:
            parts.append(f"format={data[position]}"); position += 1
        if mask & 2:
            parts.append(f"flags={data[position]}"); position += 1
        if mask & 4:
            value = shift = 0
            while True:
                byte = data[position]; position += 1
                value |= (byte & 0x7F) << shift
                if not byte & 0x80:
                    break
                shift += 7
            parts.append(f"intValue={value}")
        if mask & 8:
            bits = int.from_bytes(data[position:position + 8], "little")
            position += 8
            parts.append(f"doubleBits=0x{bits:016x}")
        if mask & 16:
            position += 1
            parts.append("quadIndex=<独立值>")
        for bit, label in ((32, "name"), (64, "name2"), (128, "textValue")):
            if mask & bit:
                length = shift = 0
                while True:
                    byte = data[position]; position += 1
                    length |= (byte & 0x7F) << shift
                    if not byte & 0x80:
                        break
                    shift += 7
                parts.append(f"{label}={data[position:position + length]!r}")
                position += length
        lines.append(f"  {index:>4} {begin:>5}..{position - 1:<5}  " + "  ".join(parts))

    lines.append("")
    lines.append("操作码直方图（出现次数，取前 15）：")
    for op, count in info["opcodes"].most_common(15):
        lines.append(f"  op {op:>2} = {names[op] if op < len(names) else '?':<14} {count:>6}")
    lines.append("")
    lines.append("指令里出现的用户变量名（带 $ 作用域编号）：")
    mangled = sorted({s for s in info["strings"] if re.fullmatch(r"[A-Za-z_][\w]*\$\d+", s)})
    lines.append(f"  共 {len(mangled)} 个，样本：{mangled[:20]}")
    return write("evidence-03-instructions.txt", lines)


def opcode_names():
    """从 stackvm.hpp 的 Op 枚举里读出操作码名（顺序即编号）。"""
    path = os.path.join(ROOT, "magel", "library", "stackvm", "stackvm.hpp")
    if not os.path.exists(path):
        return []
    source = open(path, encoding="utf-8", newline=None).read()
    block = source.split("enum class Op\n{", 1)[1].split("\n};", 1)[0]
    body = re.sub(r"///[^\n]*", "", block)
    return [m.strip() for m in body.split(",")
            if re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", m.strip())]


def evidence_runtime(vm, specimen, disasm_lines=40):
    """disasm / run --trace / run --debug 的实际输出，以及完整性检查实验。"""
    lines = []
    if not vm or not os.path.exists(vm):
        return write("evidence-04-runtime.txt", [f"（未提供可用的 --vm：{vm}）"])

    rel = os.path.relpath
    prefix = specimen[:-len(".svmb")]
    data = prefix + ".data.bin"
    layout = prefix + ".layout"
    base = [vm, "run", specimen, "--data", data, "--layout", layout]

    section_header(lines, f"$ magel-stackvm disasm {rel(specimen, ROOT)}")
    code, output = run([vm, "disasm", specimen])
    lines += output.rstrip("\n").splitlines()[:disasm_lines]
    lines.append(f"...（退出码 {code}）")
    section_header(lines, "disasm 里与用户代码有关的位置")
    for pattern in ("CALL classify", "PARAM value$", "STORE sum$", "DECLARE sum$"):
        hits = [line for line in output.splitlines() if pattern in line]
        lines.append(f"  含 {pattern!r} 的前 3 行：{hits[:3]}")

    section_header(lines, f"$ magel-stackvm run {rel(specimen, ROOT)} --limit 12 --trace")
    code, trace = run(base + ["--limit", "12", "--trace"])
    lines += trace.rstrip("\n").splitlines()[:12]
    section_header(lines, f"$ magel-stackvm run {rel(specimen, ROOT)} --limit 12 --debug")
    code, debug = run(base + ["--limit", "12", "--debug"])
    lines += debug.rstrip("\n").splitlines()[:8]
    section_header(lines, f"$ magel-stackvm run {rel(specimen, ROOT)}（完整输出）")
    code, full = run(base + ["--limit", "1000000"])
    lines += full.rstrip("\n").splitlines()[:12]

    section_header(lines, "完整性检查：损坏、截断、以及“改完重算校验和”")
    raw = open(specimen, "rb").read()
    with tempfile.TemporaryDirectory() as work:
        broken = os.path.join(work, "tampered.svmb")
        payload = bytearray(raw)
        payload[100] ^= 0x5A
        open(broken, "wb").write(bytes(payload))
        code, output = run([vm, "check", broken])
        lines.append(f"  改掉 payload 第 100 字节 → check 退出码 {code}，输出：{output.strip()}")

        cut = os.path.join(work, "truncated.svmb")
        open(cut, "wb").write(raw[:-10])
        code, output = run([vm, "check", cut])
        lines.append(f"  截掉尾部 10 字节 → check 退出码 {code}，输出：{output.strip()}")

        rewritten = os.path.join(work, "repacked.svmb")
        body = bytearray(raw[HEADER_SIZE:])
        body[39 - HEADER_SIZE] = 50          # 第 0 条指令：Call main → Halt
        file_bytes = bytearray(raw[:HEADER_SIZE]) + body
        file_bytes[20:24] = (fnv1a(bytes(body)) & 0xFFFFFFFF).to_bytes(4, "little")
        open(rewritten, "wb").write(bytes(file_bytes))
        code, output = run([vm, "check", rewritten])
        lines.append(f"  把第 0 条指令的 op 从 48(Call) 改成 50(Halt) 并重算 FNV-1a → "
                     f"check 退出码 {code}，输出：{output.strip() or '（无输出，校验通过）'}")
        code, output = run([vm, "run", rewritten, "--data", data, "--layout", layout,
                            "--limit", "1000000"])
        lines.append(f"    再运行它：退出码 {code}，stdout={output.strip() or '（空）'}")
        lines.append("    对照：原样运行会打印 “sum 1..5 = 15” 等 7 行。"
                     "校验和只挡损坏，不挡改写。")
    return write("evidence-04-runtime.txt", lines)


def evidence_native(vm_msvc, vm_gcc):
    """两份 VM 可执行文件的符号、原生反汇编片段、以及 .rdata 里的助记符表。"""
    lines = []
    for tag, path in (("GCC x64", vm_gcc), ("MSVC x64", vm_msvc)):
        section_header(lines, f"{tag}：{path}")
        if not path or not os.path.exists(path):
            lines.append("  （缺失，跳过）")
            continue
        lines.append(f"  文件大小      {os.path.getsize(path)} 字节")
        code, output = run(["nm", "-C", "--defined-only", path])
        symbols = [line for line in output.splitlines() if line.strip()]
        lines.append(f"  nm -C --defined-only 行数  {len(symbols)}")
        code, output = run(["objdump", "-t", path])
        table = [line for line in output.splitlines() if line.strip()]
        lines.append(f"  objdump -t 行数            {len(table)}")
        for line in symbols:
            if "Machine::run" in line and " T " in line:
                lines.append(f"  入口符号      {line.strip()}")
                break
        code, output = run(["objdump", "-h", path])
        for line in output.splitlines():
            if re.match(r"\s+\d+\s+\.text\s", line):
                lines.append(f"  .text 段      {line.split()[2]} 字节")
                break
        mnemonics, total, distinct = extract_mnemonics(path)
        lines.append(f"  助记符串      命中 {total} 处；最长连续段 {len(mnemonics)} 项")
        lines.append("    连续段（顺序即操作码编号顺序）：")
        lines.append(f"      {mnemonics}")

    section_header(lines, "GCC 档 Machine::run 的原生反汇编开头（objdump -d）")
    if vm_gcc and os.path.exists(vm_gcc):
        code, output = run(["nm", "-C", vm_gcc])
        address = None
        for line in output.splitlines():
            if re.search(r" T stackvm::Machine::run\(", line):
                address = int(line.split()[0], 16)
                break
        if address is None:
            lines.append("  （找不到 Machine::run 符号）")
        else:
            step = 0xE0
            lines.append(f"  符号地址 0x{address:x}")
            code, output = run(["objdump", "-d", f"--start-address=0x{address:x}",
                                f"--stop-address=0x{address + step:x}", vm_gcc])
            body = output.splitlines()
            lines += body[7:7 + 30]
    else:
        lines.append("  （缺失，跳过）")
    return write("evidence-05-vm-native.txt", lines)


MNEMONICS = ("PUSH", "LOAD", "STORE", "DECLARE", "ALLOC", "FREE", "DROP", "CHECK",
             "CHECKPTR", "CALLPTR", "NEG", "NOT", "ADD", "SUB", "MUL", "DIV", "MOD", "SHL",
             "SAR", "SHR", "AND", "OR", "XOR", "EQ", "NE", "LT", "LE", "GT", "GE", "LTU",
             "LEU", "GTU", "GEU", "I2F", "F2I", "MULWIDE", "DIVWIDE", "JMP", "JZ", "JNZ",
             "PRINT", "READ", "GETC", "PUTC", "PARAM", "CALL", "RET", "HALT", "ADDR",
             "LOADAT", "STOREAT")


def extract_mnemonics(path):
    """从可执行文件里抽出反汇编器用的助记符串。

    这些字面量是**字符串常量**（例如 `"PUSH "`、`"DECLARE "`、`"PRINT text"`），
    所以按“可读串”扫描即可，不需要解析 PE。返回 (顺序表, 命中总数, 不同命中数)：
    顺序表只在字面量确实连成一片时才给出（GCC 档是这样，表里顺序就是操作码编号顺序）。
    """
    data = open(path, "rb").read()
    runs = [(m.start(), m.group().decode("latin1"))
            for m in re.finditer(rb"[ -~]{2,}", data)]
    hits = [(offset, text) for offset, text in runs
            if text.strip().strip('"').split(" ")[0] in MNEMONICS]

    ordered, cluster = [], []
    for offset, text in hits:
        name = text.strip().strip('"').split(" ")[0]
        if cluster and offset - cluster[-1][0] > 256:
            if len(cluster) > len(ordered):
                ordered = cluster
            cluster = []
        cluster.append((offset, name))
    if len(cluster) > len(ordered):
        ordered = cluster
    return [name for _, name in ordered], len(hits), len({name for _, name in hits})


def evidence_exported(paths, specimen):
    """独立 exe 里还能捞到哪些名字；带调试表与 `--no-debug` 两份一起看。"""
    lines = []
    available = [(label, path) for label, path in paths if path and os.path.exists(path)]
    if not available:
        return write("evidence-06-exported-exe.txt",
                     ["（未提供可用的 --exported / --exported-strip）"])

    probes = ["main", "classify", "i$451", "sum$452", "value$194",
              "std::lang::Math::abs$double", "__magel_loop_101", "else_1", "endif_2",
              "control.quad", "PUSH ", "DECLARE ", "sum 1..5 = %d"]
    for label, path in available:
        data = open(path, "rb").read()
        section_header(lines, f"{label}：{os.path.relpath(path, ROOT)}")
        lines.append(f"  字节数          {len(data)}")
        runs = re.findall(rb"[ -~]{6,}", data)
        lines.append(f"  长度 >= 6 的可读串 {len(runs)} 条，其中不同 {len(set(runs))} 条")
        lines.append("  探测串是否出现在 exe 里（直接字节搜索，不依赖 PE 解析）：")
        for probe in probes:
            position = data.find(probe.encode())
            lines.append(f"    {probe:<26} {'@ ' + str(position) if position >= 0 else '未命中'}")
        for probe in ("control.quad",):
            present = probe.encode() in data
            lines.append(f"  生成期路径 {probe!r}：{'在（调试表写了位置）' if present else '不在'}")
    if len(available) >= 2:
        (first, second) = available[0][1], available[1][1]
        lines.append("")
        lines.append(f"  两份差 {os.path.getsize(first)} - {os.path.getsize(second)} = "
                     f"{os.path.getsize(first) - os.path.getsize(second)} 字节")

    lines.append("")
    lines.append("对照：这些名字在 .svmb 里出现的位置")
    info = svmb_dump.parse(specimen)
    in_strings = set(info["strings"])
    exported = set(info["exports"])
    labels = {name for name, _ in info["labels"]}
    for probe in probes[:6]:
        where = []
        if probe in exported:
            where.append("导出表")
        if probe in labels:
            where.append("标签表")
        if probe in in_strings:
            where.append("指令操作数/字符串")
        lines.append(f"  {probe:<26} {'、'.join(where) if where else '（不在，属于数据区或代码生成器产物）'}")
    return write("evidence-06-exported-exe.txt", lines)


def evidence_strip(specimen, stripped, parser, vm):
    """`--no-debug` 前后：体积差来自哪一节、还剩什么、丢的是什么。

    剥离后的产物**不含路径字符串**，所以它与构建目录无关：换个地方重新生成，字节应当完全一致。
    这条在这里被当成断言来用。
    """
    lines = []
    section_header(lines, "两份入库标本")
    for tag, path in (("带调试节", specimen), ("--no-debug", stripped)):
        if not path or not os.path.exists(path):
            lines.append(f"  {tag}: 缺失（{path}）")
            return write("evidence-07-strip-debug.txt", lines)
        lines.append(f"  {tag:<12} {os.path.relpath(path, ROOT)}  "
                     f"{os.path.getsize(path)} 字节  SHA-256 {sha256(path)}")

    full_raw = open(specimen, "rb").read()
    strip_raw = open(stripped, "rb").read()
    full = svmb_dump.parse(specimen)
    hand = svmb_dump.parse(stripped)

    section_header(lines, "逐节对照")
    lines.append(f"  {'节':<8}{'带调试节':>12}{'--no-debug':>13}")
    for (n1, c1, s1), (n2, c2, s2) in zip(full["sections"], hand["sections"]):
        lines.append(f"  {n1:<8}{s1:>12}{s2:>13}   （{c1} → {c2} 条）")
    saved = len(full_raw) - len(strip_raw)
    lines.append(f"  合计 {len(full_raw)} → {len(strip_raw)} 字节，净省 {saved}"
                 f"（{saved / len(full_raw) * 100:.1f}%）")
    debug_bytes = [size for name, _, size in full["sections"] if name == "调试信息"][0]
    lines.append(f"  调试节 {debug_bytes} 字节；{debug_bytes} - 4（0 条的计数）= {debug_bytes - 4}"
                 f"，与实测净省 {saved} {'吻合' if debug_bytes - 4 == saved else '不符'}")

    def debug_offset(info):
        total = HEADER_SIZE
        for name, _, size in info["sections"]:
            if name == "调试信息":
                break
            total += size
        return total

    full_cut, strip_cut = debug_offset(full), debug_offset(hand)
    same_body = full_raw[HEADER_SIZE:full_cut] == strip_raw[HEADER_SIZE:strip_cut]
    lines.append(f"  调试节之前的 payload（{full_cut - HEADER_SIZE} 字节）"
                 f"{'逐字节一致' if same_body else '不一致'}")
    lines.append(f"  剥离版尾部只剩调试节的计数：{strip_raw[strip_cut:]!r}"
                 f"（{len(strip_raw) - strip_cut} 字节）")

    section_header(lines, "剩下什么：五节一个不少")
    lines.append(f"  指令 {sum(hand['opcodes'].values())} 条、标签 {len(hand['labels'])} 个、"
                 f"类型 {len(hand['types'])} 条、形参表 {len(hand['parameters'])} 组、"
                 f"导出 {len(hand['exports'])} 个")
    names = [s for s in ("i$451", "sum$452", "value$194", "std::lang::Math::abs$double",
                         "classify", "main") if s in set(hand["strings"])]
    lines.append(f"  剥离后仍在字符串里的名字：{names}")
    missing = [s for s in ("i$451", "sum$452", "classify") if s not in set(hand["strings"])]
    lines.append(f"  {'名字全部保留' if not missing else '缺: ' + str(missing)}")

    section_header(lines, "丢的是什么：位置")
    if vm and os.path.exists(vm):
        data = specimen[:-len(".svmb")] + ".data.bin"
        layout = specimen[:-len(".svmb")] + ".layout"
        code, output = run([vm, "run", stripped, "--data", data, "--layout", layout,
                            "--limit", "6", "--debug"])
        lines.append(f"  $ magel-stackvm run control-stripped.svmb --limit 6 --debug → 退出码 {code}")
        for line in output.rstrip("\n").splitlines()[:4]:
            lines.append(f"    {line}")
        code, output = run([vm, "disasm", stripped])
        body = [line for line in output.splitlines() if line.startswith("3:") or
                line.startswith("4:") or line.startswith("7:")]
        lines.append(f"  同一份剥离产物 disasm 仍给出：{body}")
    else:
        lines.append("  （未提供 --vm，跳过运行期对照）")

    section_header(lines, "剥离版与构建目录无关（重新生成应当逐字节一致）")
    if parser and os.path.exists(parser):
        with tempfile.TemporaryDirectory() as work:
            prefix = os.path.join(work, "anything-longer-name")
            code, output = run([parser, "magel/example/samples/control.mgl", "--emit", "--svmb",
                                "--no-debug", "--out", prefix])
            rebuilt = prefix + ".svmb"
            if code != 0 or not os.path.exists(rebuilt):
                lines.append(f"  重新生成失败（退出码 {code}）：{output.strip()[:300]}")
            else:
                lines.append(f"  在 {os.path.basename(work)} 下重新生成 {os.path.getsize(rebuilt)} 字节")
                lines.append(f"  SHA-256 {sha256(rebuilt)}")
                lines.append(f"  与入库标本{'一致（剥离后不带任何路径信息）' if sha256(rebuilt) == sha256(stripped) else '不一致'}（对照：带调试节的产物换个目录就会变长）")
    else:
        lines.append("  （未提供 --parser，跳过重新生成比对）")
    return write("evidence-07-strip-debug.txt", lines)


def main():
    parser = argparse.ArgumentParser(description="重新生成 doc/16 的证据文件")
    parser.add_argument("--svmb", default=os.path.join(HERE, "control.svmb"))
    parser.add_argument("--stripped", default=os.path.join(HERE, "control-stripped.svmb"))
    parser.add_argument("--parser", default="", help="magel，用来做同源码复现比对")
    parser.add_argument("--vm", default="", help="magel-stackvm，用来跑 disasm/run/check")
    parser.add_argument("--vm-opcode", default="", help="第二份 magel-stackvm（GCC 档，带符号）")
    parser.add_argument("--exported", default="", help="magel-emit 导出的独立 exe（带调试表）")
    parser.add_argument("--exported-strip", default="", help="同上但 magel-emit --no-debug")
    parser.add_argument("--tests", default="", help="magel-stackvm-tests 可执行文件")
    args = parser.parse_args()

    if not os.path.exists(args.svmb):
        raise SystemExit(f"找不到标本：{args.svmb}")
    print(f"仓库根目录 {ROOT}")
    evidence_container(args.svmb, args.parser)
    evidence_sections(args.svmb)
    evidence_instructions(args.svmb)
    evidence_runtime(args.vm, args.svmb)
    evidence_native(args.vm, args.vm_opcode)
    evidence_exported([("导出 exe（带调试表）", args.exported),
                       ("导出 exe（--no-debug）", args.exported_strip)], args.svmb)
    evidence_strip(args.svmb, args.stripped, args.parser, args.vm)
    cache = os.path.join(HERE, "__pycache__")
    if os.path.isdir(cache):
        shutil.rmtree(cache, ignore_errors=True)
    print("完成。")


if __name__ == "__main__":
    main()
