"""规范化中间表示（`quad_ir`）的结构回归：基本块覆盖、控制流对称、回边与函数划分。

`CHALI_DUMP_IR=1` 会让编译器在优化后、组装前把 IR 打到 stderr。这里不比对全文（后端一改就会变），
只校验结构性不变量：块区间必须无缝覆盖全部四元式、前驱后继必须互相对得上、
标了回边的块必须真有一条跳回前面块的后继、标了循环头的块必须真的被回边指向。
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""

# 覆盖分支、循环、容器与字符串路径的样例；IR 结构要在这几种形态上都成立。
SAMPLES = ["hello", "containers", "match_expression"]

COUNTS = re.compile(r"^ir counts functions (\d+) blocks (\d+) values (\d+) backedges (\d+)$")
BLOCK = re.compile(r"^  b(\d+) quads\[(\d+),(\d+)\) preds (\S+) succs (\S+)( backedge)?( loop)?$")
FUNCTION = re.compile(r"^function (\S+) entry b(\d+) blocks (\d+)")


def parse(text):
    """把转储文本解析成块表、函数表与计数。"""
    counts = None
    blocks = []
    functions = []
    for line in text.splitlines():
        matched = COUNTS.match(line)
        if matched:
            counts = tuple(int(value) for value in matched.groups())
            continue
        matched = FUNCTION.match(line)
        if matched:
            functions.append((matched.group(1), int(matched.group(2)), int(matched.group(3))))
            continue
        matched = BLOCK.match(line)
        if matched:
            blocks.append({
                "index": int(matched.group(1)),
                "begin": int(matched.group(2)),
                "end": int(matched.group(3)),
                "preds": [] if matched.group(4) == "-" else [int(x[1:]) for x in matched.group(4).split(",")],
                "succs": [] if matched.group(5) == "-" else [int(x[1:]) for x in matched.group(5).split(",")],
                "back": bool(matched.group(6)),
                "loop": bool(matched.group(7)),
            })
    assert counts is not None, "转储里没有计数行"
    return counts, blocks, functions


def check(name, text):
    """校验一份转储的结构不变量，返回问题列表。"""
    counts, blocks, functions = parse(text)
    _, blockCount, valueCount, backEdges = counts
    problems = []
    if len(blocks) != blockCount:
        problems.append(f"{name}：块行 {len(blocks)} 与计数 {blockCount} 不符")
    if valueCount == 0:
        problems.append(f"{name}：值表为空")
    if not blocks:
        problems.append(f"{name}：没有基本块")
        return problems

    position = 0
    for block in blocks:
        if block["begin"] != position:
            problems.append(f"{name}：b{block['index']} 从 {block['begin']} 开始，期望 {position}（块未无缝覆盖）")
        if block["end"] <= block["begin"]:
            problems.append(f"{name}：b{block['index']} 是空块")
        position = block["end"]

    byIndex = {block["index"]: block for block in blocks}
    for block in blocks:
        for successor in block["succs"]:
            target = byIndex.get(successor)
            if target is None:
                problems.append(f"{name}：b{block['index']} 的后继 b{successor} 不存在")
            elif block["index"] not in target["preds"]:
                problems.append(f"{name}：b{block['index']}→b{successor} 缺前驱记录")
        for predecessor in block["preds"]:
            source = byIndex.get(predecessor)
            if source is None:
                problems.append(f"{name}：b{block['index']} 的前驱 b{predecessor} 不存在")
            elif block["index"] not in source["succs"]:
                problems.append(f"{name}：b{predecessor}→b{block['index']} 缺后继记录")

    headers = {block["index"] for block in blocks if block["loop"]}
    marked = [block for block in blocks if block["back"]]
    if len(marked) != backEdges:
        problems.append(f"{name}：回边标记 {len(marked)} 条与计数 {backEdges} 不符")
    for block in marked:
        if not any(successor <= block["index"] for successor in block["succs"]):
            problems.append(f"{name}：b{block['index']} 标了回边却没有指向前面的后继")
        if not any(successor in headers for successor in block["succs"]):
            problems.append(f"{name}：b{block['index']} 的回边目标没有标成循环头")

    covered = sum(entry[2] for entry in functions)
    if covered and covered != len(blocks) - 1:
        problems.append(f"{name}：函数区块数合计 {covered}，另有 {len(blocks) - covered} 个块无归属（期望只有引导块）")
    return problems


def check_operands(name, opt_text, ir_text):
    """IR 里每条指令记下的操作数必须与原四元式一致：本层不改写四元式。"""
    quads = [line.strip() for line in opt_text.splitlines() if line.strip()]
    printed = []
    for line in ir_text.splitlines():
        matched = re.match(r"^\s+q(\d+) (.*?) defs ", line)
        if matched:
            printed.append((int(matched.group(1)), matched.group(2).strip()))
    problems = []
    if len(printed) != len(quads):
        problems.append(f"{name}：IR 指令 {len(printed)} 条与四元式 {len(quads)} 条不符")
    for index, text in printed:
        if index >= len(quads):
            problems.append(f"{name}：q{index} 超出四元式范围")
        elif quads[index] != text:
            problems.append(f"{name}：q{index} 操作数不一致\n  四元式：{quads[index]}\n  IR：{text}")
    return problems


def resolve_compiler(name):
    """把 `--chali` 解析成实际可执行文件，兼容 `chali`、`ChaLi.exe` 与绝对路径。"""
    candidate = Path(name)
    if candidate.is_absolute():
        assert candidate.is_file(), f"找不到编译器：{candidate}"
        return candidate
    for option in (ROOT / candidate, ROOT / (candidate.name + EXE)):
        if option.is_file():
            return option
    found = shutil.which(str(candidate)) or shutil.which(candidate.name + EXE)
    assert found, f"找不到编译器：{name}"
    return Path(found)


def compile_and_dump(chali, source):
    """带四元式与 IR 转储跑一次源码，返回 (退出码, 标准输出, 标准错误)。"""
    environment = dict(os.environ)
    environment["CHALI_LANG"] = "zh"
    environment["CHALI_DUMP_IR"] = "1"
    environment["CHALI_DUMP_OPT"] = "1"
    result = subprocess.run([str(chali), str(source)], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", env=environment, timeout=120)
    return result.returncode, result.stdout, result.stderr


def main():
    """对样例逐个校验 IR 结构，确认转储不影响输出，且 IR 未改写四元式。"""
    parser = argparse.ArgumentParser()
    parser.add_argument("--chali", default="ChaLi.exe")
    args = parser.parse_args()
    chali = resolve_compiler(args.chali)

    problems = []
    for name in SAMPLES:
        source = ROOT / "example/samples" / f"{name}.chl"
        assert source.is_file(), f"样例缺失：{source}"
        code, output, error = compile_and_dump(chali, source)
        if code != 0:
            problems.append(f"{name}：退出码 {code}\n{error[-400:]}")
            continue
        marker = "=== 规范化中间表示 ==="
        if marker not in error:
            problems.append(f"{name}：stderr 里没有 IR 转储")
            continue
        text = error.split(marker, 1)[1]
        expected = ROOT / "example/expected" / f"{name}.txt"
        if expected.is_file() and expected.read_text(encoding="utf-8") != output:
            problems.append(f"{name}：开转储后标准输出与期望文件不一致")
        problems += check(name, text)
        opt = error.split("=== 化简后的四元式 ===", 1)[1].split(marker, 1)[0]
        problems += check_operands(name, opt, text)

    if problems:
        print("\n".join(problems))
        return 1
    print(f"IR 结构回归：{len(SAMPLES)} 个样例的块覆盖、前驱后继、回边、函数划分与操作数一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
