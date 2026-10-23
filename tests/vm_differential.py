"""差分回归：同一份程序在后端各档开关下必须得到逐字节相同的结果。

后端改造的护栏。四个档位跑同一份源码，输出、诊断、调用栈与退出码都应当一模一样：

  ① 默认（寄存器执行核 + 槽位复用 + 机器级优化）
  ② `CHALI_NO_REGISTER_CORE=1` —— 退回通用栈解释核（P0）
  ③ `CHALI_NO_SLOT_REUSE=1`    —— 形参在前 + 声明顺序一人一槽（P3 对照）
  ④ `CHALI_NO_MACHINE_OPT=1`   —— 关掉机器级优化整层（P4 对照）
  ⑤ `CHALI_NO_OPT_INLINE=1`    —— 只关小函数内联（P4 里唯一会复制代码的一遍）

③④⑤ 是"优化前后"的对照口径：优化只能让它更快/更小，不能改结果。除了逐字节比对，
这里还会读 `--stats` 的三行后端统计，确认复用真的省了槽、选择真的覆盖了指令、
内联真的展开过调用点。
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""

# 覆盖主要执行形态的样例：整数/浮点、字符串与插值、容器、数组、匹配、泛型、大整数、块与 break。
SAMPLES = [
    "hello",
    "basic_casts",
    "bits",
    "float",
    "bigint",
    "strings",
    "print_interpolation",
    "containers",
    "array2d",
    "match_expression",
    "match_range",
    "any_generics",
    "block_expression",
    "break_level",
    "scope",
    "lambda_captures",
    "ownership_release",
]

# 四档开关：第一档是基准，其余都必须和它逐字节一致。
# `ip` 是栈指令下标：前两档只换执行核 / 只换帧布局，字节码不动，所以连 `ip` 都要一致；
# 关掉机器级优化会改变字节码长度，`ip` 自然跟着变，比对时只把这一项抹平。
MODES = [
    ("默认", {}, False),
    ("栈解释核", {"CHALI_NO_REGISTER_CORE": "1"}, False),
    ("不复用槽位", {"CHALI_NO_SLOT_REUSE": "1"}, False),
    ("关优化", {"CHALI_NO_MACHINE_OPT": "1"}, True),
    ("关内联", {"CHALI_NO_OPT_INLINE": "1"}, True),
]

# 运行期失败也要一致：诊断文字、出错位置与调用栈都由 VM 侧生成，各档不能各说一套。
RUNTIME_ERRORS = {
    "index_error": """void fail(int64 index) {
    List<int64> values;
    values.push(1);
    values.push(2);
    println("{}", values[index]);
}

void main() {
    println("start");
    fail(5);
    println("unreachable");
}
""",
    "map_missing": """void lookup(Map<String, int64> table, String key) {
    println("{}", table[key]);
}

void main() {
    Map<String, int64> table;
    table["a"] = 1;
    lookup(table, "b");
}
""",
}


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


def normalize_ip(text):
    """抹平调用栈里的 `ip N`：字节码长度变了它就会变，其余一个字节都不许动。"""
    return re.sub(r" ip \d+", " ip *", text)


def run(chali, source, stdin_text, overrides, with_stats=False):
    """按指定开关组合跑一份源码，返回退出码与标准输出 / 标准错误。"""
    environment = dict(os.environ)
    for name in ("CHALI_NO_REGISTER_CORE", "CHALI_NO_SLOT_REUSE", "CHALI_NO_MACHINE_OPT", "CHALI_NO_OPT_INLINE"):
        environment.pop(name, None)
    environment.update(overrides)
    environment["CHALI_LANG"] = "zh"
    environment["PYTHONUTF8"] = "1"
    command = [str(chali), "run", str(source)] if with_stats else [str(chali), str(source)]
    if with_stats:
        command.append("--stats")
    result = subprocess.run(command, input=stdin_text, capture_output=True,
                            text=True, encoding="utf-8", errors="replace",
                            env=environment, timeout=300)
    return result.returncode, result.stdout, result.stderr


def compare(chali, source, stdin_text):
    """跑四档开关并逐项与基准比对，返回 (失败说明, 基准的标准错误)。"""
    baseline = run(chali, source, stdin_text, MODES[0][1])
    problems = []
    labels = ("退出码", "标准输出", "标准错误")
    for label, overrides, relax_ip in MODES[1:]:
        current = run(chali, source, stdin_text, overrides)
        reference = baseline
        if relax_ip:
            reference = baseline[:2] + (normalize_ip(baseline[2]),)
            current = current[:2] + (normalize_ip(current[2]),)
        for what, left, right in zip(labels, reference, current):
            if left != right:
                problems.append(f"{source.name}：{what}在 [{label}] 下与默认档不一致\n"
                                f"  默认：{left!r}\n  {label}：{right!r}")
    return problems, baseline[2]


def check_stats(stats_text, name, expect_inline=False):
    """从 `--stats` 的三行后端统计里取出数字，确认优化真的生效。"""
    problems = []
    machine = re.search(
        r"机器级优化：四元式 (\d+) → (\d+)（折叠 (\d+).*外提 (\d+)，内联 (\d+) 处（\+(\d+) 条），删除 (\d+)）",
        stats_text)
    if machine is None:
        problems.append(f"{name}：`--stats` 没有给出机器级优化统计")
    else:
        before, after, hoisted = int(machine.group(1)), int(machine.group(2)), int(machine.group(4))
        inlined = int(machine.group(6))
        # 外提会插入前置头（一个标签 + 一条回跳），内联会复制函数体；这两处涨条数是设计内的，
        # 除此之外优化只许把四元式变少。
        if after > before + 2 * hoisted + inlined:
            problems.append(f"{name}：机器级优化把四元式变多且没有外提 / 内联兜底"
                            f"（{before} → {after}，外提 {hoisted}，内联 +{inlined}）")

        if expect_inline and int(machine.group(5)) == 0:
            problems.append(f"{name}：循环里的小调用一处都没内联（内联遍没生效）")

    selection = re.search(r"指令选择：.*覆盖栈指令 (\d+)/(\d+)", stats_text)
    if selection is None:
        problems.append(f"{name}：`--stats` 没有给出指令选择统计")
    elif int(selection.group(1)) == 0:
        problems.append(f"{name}：指令选择一条指令都没覆盖（模式表没生效）")

    allocation = re.search(r"帧槽分配：(\d+) 个函数，符号 (\d+) 个 → 槽 (\d+) 个", stats_text)
    if allocation is None:
        problems.append(f"{name}：`--stats` 没有给出帧槽分配统计")
    elif int(allocation.group(3)) > int(allocation.group(2)):
        problems.append(f"{name}：复用后槽位比符号还多（{allocation.group(2)} → {allocation.group(3)}）")
    return problems


def main():
    """跑样例与运行期错误用例的差分比对，并检查后端统计。"""
    parser = argparse.ArgumentParser()
    parser.add_argument("--chali", default="ChaLi.exe")
    args = parser.parse_args()
    chali = resolve_compiler(args.chali)

    problems = []
    checked = 0
    for name in SAMPLES:
        source = ROOT / "example/samples" / f"{name}.chl"
        assert source.is_file(), f"样例缺失：{source}"
        stdin_file = ROOT / "example/expected" / f"{name}.in"
        stdin_text = stdin_file.read_text(encoding="utf-8") if stdin_file.is_file() else ""
        failures, _ = compare(chali, source, stdin_text)
        problems += failures
        checked += 1

    with tempfile.TemporaryDirectory(prefix="chali-differential-") as directory:
        for name, text in RUNTIME_ERRORS.items():
            source = Path(directory) / f"{name}.chl"
            source.write_text(text, encoding="utf-8")
            failures, _ = compare(chali, source, "")
            problems += failures
            checked += 1

    # 优化不能"白跑"：抽一个够复杂的样例读 `--stats`，确认三层都在干活。
    for name in ("scope", "containers"):
        source = ROOT / "example/samples" / f"{name}.chl"
        _, _, stats_text = run(chali, source, "", MODES[0][1], with_stats=True)
        problems += check_stats(stats_text, name, expect_inline=(name == "scope"))

    if problems:
        print("\n".join(problems))
        return 1
    print(f"VM 差分：{checked} 个用例 × {len(MODES)} 档开关，退出码、输出、诊断与调用栈完全一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
