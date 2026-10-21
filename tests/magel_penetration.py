"""渗透测试：对编译期与运行期边界构造攻击用例，收集崩溃、未定义行为与静默错误。

本脚本只负责**收集事实**：对每个用例记录编译与运行的退出码、标准输出与诊断，
归类为 UB / CRASH / TIMEOUT / FAIL / OUTPUT，供人工判定。
判定结论与修复另行登记到 doc/37-compiler-safety.md 与对应回归。
"""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# 每个用例：(名称, 是否由生成器展开, 源码)。generated 为 True 时源码取自 GENERATORS。
CASES = [
    # ---- 数值转换与字面量 ----
    ("float_to_int_overflow", False, """int64 main()
{
    float64 big = 1e300;
    int64 value = (int64)big;
    println("{}", value);
    return 0;
}
"""),
    ("float_nan_to_int", False, """int64 main()
{
    float64 nan = std::math::sqrt(-1.0);
    int64 value = (int64)nan;
    println("{}", value);
    return 0;
}
"""),
    ("float_negative_overflow_to_int", False, """int64 main()
{
    float64 big = -1e300;
    int64 value = (int64)big;
    println("{}", value);
    return 0;
}
"""),
    ("int_literal_overflow", False, """int64 main()
{
    int64 value = 99999999999999999999;
    println("{}", value);
    return 0;
}
"""),
    ("int_literal_overflow_negative", False, """int64 main()
{
    int64 value = -99999999999999999999;
    println("{}", value);
    return 0;
}
"""),
    ("float_literal_overflow", False, """int64 main()
{
    float64 value = 1e999;
    println("{}", value);
    return 0;
}
"""),
    ("int_div_min_over_minus_one", False, """int64 main()
{
    int64 low = -9223372036854775807 - 1;
    int64 value = low / (-1);
    println("{}", value);
    return 0;
}
"""),
    ("int_mod_min_over_minus_one", False, """int64 main()
{
    int64 low = -9223372036854775807 - 1;
    int64 value = low % (-1);
    println("{}", value);
    return 0;
}
"""),
    ("shift_beyond_width", False, """int64 main()
{
    int64 value = 1 << 64;
    println("{}", value);
    return 0;
}
"""),
    # ---- 字符串与容器边界 ----
    ("string_negative_slice", False, """int64 main()
{
    String text = "abcdef";
    String head = text[0:-2];
    println("{}", head);
    return 0;
}
"""),
    ("string_large_index", False, """int64 main()
{
    String text = "abcdef";
    String part = text[100:200];
    println("{}", part);
    return 0;
}
"""),
    ("utf8_mid_char_slice", False, """int64 main()
{
    String text = "中文字符串";
    String part = text[1:2];
    println("{}", part);
    return 0;
}
"""),
    ("array_negative_index", False, """int64 main()
{
    int64 data[4];
    data[0] = 10;
    int64 value = data[-1];
    println("{}", value);
    return 0;
}
"""),
    ("array_large_index", False, """int64 main()
{
    int64 data[4];
    data[0] = 10;
    int64 value = data[1000000];
    println("{}", value);
    return 0;
}
"""),
    ("new_negative_length", False, """int64 main()
{
    int64 data[] = new[-1];
    println("{}", data.len());
    return 0;
}
"""),
    ("empty_list_pop", False, """int64 main()
{
    List<int64> values;
    int64 value = values.pop();
    println("{}", value);
    return 0;
}
"""),
    ("fold_callback_mutates_list", False, """import std::algorithms;
List<int64> values;
int64 grow(int64 acc, int64 item)
{
    values.push(item);
    return acc + item;
}
int64 main()
{
    values.push(1);
    values.push(2);
    values.push(3);
    values.push(4);
    int64 total = std::algorithms::foldValue<int64>(values, 0, grow);
    println("{}", total);
    return 0;
}
"""),
    ("map_callback_clears_list", False, """import std::algorithms;
List<int64> values;
int64 clear(int64 item)
{
    values.clear();
    return item;
}
int64 main()
{
    values.push(1);
    values.push(2);
    values.push(3);
    auto mapped = std::algorithms::mapValue<int64>(values, clear);
    println("done");
    return 0;
}
"""),
    ("foreach_push", False, """int64 main()
{
    List<int64> values;
    values.push(1);
    values.push(2);
    for (int64 item in values)
    {
        values.push(item * 2);
    }
    println("{}", values.len());
    return 0;
}
"""),
    ("foreach_set", False, """int64 main()
{
    List<int64> values;
    values.push(1);
    values.push(2);
    for (int64 item in values)
    {
        values[0] = item;
    }
    println("{}", values[0]);
    return 0;
}
"""),
    ("foreach_remove", False, """int64 main()
{
    List<int64> values;
    values.push(1);
    values.push(2);
    values.push(3);
    for (int64 item in values)
    {
        values.remove(0);
    }
    println("{}", values.len());
    return 0;
}
"""),
    # ---- 所有权与错误传播 ----
    ("assignment_as_expression", False, """String bump(String text)
{
    return text + "!";
}
int64 main()
{
    String a = "x";
    String b = (a = bump(a));
    println("{} {}", a, b);
    return 0;
}
"""),
    ("try_in_logical_right", False, """Result<String, String> pick(bool ok)
{
    if (ok)
    {
        return Result<String, String>::Ok("value");
    }
    return Result<String, String>::Err("原始错误");
}
int64 main()
{
    bool flag = false && (pick(true)?.length() > 0);
    println("{}", flag);
    return 0;
}
"""),
    ("nested_try_three_levels", False, """Result<String, String> level3(bool fail)
{
    if (fail)
    {
        return Result<String, String>::Err("深层错误");
    }
    return Result<String, String>::Ok("深层值");
}
Result<String, String> level2(bool fail)
{
    return Result<String, String>::Ok(level3(fail)?);
}
Result<String, String> level1(bool fail)
{
    return Result<String, String>::Ok(level2(fail)?);
}
int64 main()
{
    for (int64 index in std::lang::range(0, 100))
    {
        assert(level1(false).unwrap() == "深层值");
        assert(level1(true).error() == "深层错误");
    }
    println("nested try OK");
    return 0;
}
"""),
    ("try_inside_container_literal", False, """Result<String, String> pick(bool ok)
{
    if (ok)
    {
        return Result<String, String>::Ok("元素");
    }
    return Result<String, String>::Err("元素错误");
}
int64 main()
{
    List<String> items = [pick(true)?, pick(true)?];
    println("{}", items.len());
    return 0;
}
"""),
    ("try_in_loop_with_break", False, """Result<String, String> pick(bool ok)
{
    if (ok)
    {
        return Result<String, String>::Ok("值");
    }
    return Result<String, String>::Err("错误");
}
Result<String, String> scan()
{
    for (int64 index in std::lang::range(0, 10))
    {
        if (index == 3)
        {
            break;
        }
        String value = pick(true)?;
    }
    return Result<String, String>::Ok("scan 完成");
}
int64 main()
{
    println("{}", scan().unwrap());
    return 0;
}
"""),
    # ---- 规模与深度预算 ----
    ("huge_struct_array", False, """struct Grid
{
    String cells[65536][65536];
};
int64 main()
{
    Grid grid;
    println("declared");
    return 0;
}
"""),
    ("huge_local_array_assign", False, """int64 main()
{
    String left[40000][40000];
    String right[40000][40000];
    left = right;
    println("assigned");
    return 0;
}
"""),
    ("deep_recursion", False, """int64 descend(int64 depth)
{
    if (depth <= 0)
    {
        return 0;
    }
    return descend(depth - 1);
}
int64 main()
{
    int64 value = descend(10000000);
    println("{}", value);
    return 0;
}
"""),
    ("deep_json_nesting", True, ""),
    ("deep_expression_chain", True, ""),
    ("deep_paren_constant", True, ""),
    ("bigfloat_huge_exponent_to_int", False, """import std::lang;
int64 main()
{
    BigDecimal value = BigDecimal("1E2000000000");
    int64 converted = (int64)value;
    println("{}", converted);
    return 0;
}
"""),
    ("bint_from_huge_float", False, """import std::lang;
int64 main()
{
    BigInteger value = bint(1e30);
    println("{}", value.str());
    return 0;
}
"""),
]


def deep_json_nesting():
    """构造超深嵌套的 JSON 文本，检验解析器是否有深度预算。"""
    depth = 20000
    return ("import std::json;\n"
            "int64 main()\n{\n"
            "    String text = \"\";\n"
            "    for (int64 index in std::lang::range(0, %d))\n    {\n"
            "        text = text + \"[\";\n    }\n"
            "    for (int64 index in std::lang::range(0, %d))\n    {\n"
            "        text = text + \"]\";\n    }\n"
            "    auto parsed = std::json::parse(text);\n"
            "    println(\"{}\", parsed.isOk());\n"
            "    return 0;\n}\n" % (depth, depth))


def deep_expression_chain():
    """构造超长左结合加法链，检验语义递归是否有深度上限。"""
    terms = " + ".join(["1"] * 20000)
    return "int64 main()\n{\n    int64 value = %s;\n    println(\"{}\", value);\n    return 0;\n}\n" % terms


def deep_paren_constant():
    """构造深嵌套括号的常量表达式，检验常量求值递归深度。"""
    depth = 20000
    return ("int64 main()\n{\n"
            "    const int64 VALUE = %s1%s;\n"
            "    println(\"{}\", VALUE);\n"
            "    return 0;\n}\n" % ("(" * depth, ")" * depth))


GENERATORS = {
    "deep_json_nesting": deep_json_nesting,
    "deep_expression_chain": deep_expression_chain,
    "deep_paren_constant": deep_paren_constant,
}

# 运行期未定义行为与崩溃的特征串。
UB_MARKERS = ("runtime error:", "AddressSanitizer", "LeakSanitizer")


def classify(code, out, err):
    """把一次编译运行的结果归类为可判定的事实。"""
    for marker in UB_MARKERS:
        if marker in err:
            return "UB", marker
    if code < 0:
        return "CRASH", "信号退出 %d" % code
    if code >= 0x80000000:
        return "CRASH", "Windows 异常码 0x%08X" % code
    if err and "terminate called" in err:
        return "CRASH", "未捕获异常导致 terminate"
    if code != 0:
        return "FAIL", "退出码 %d" % code
    return "OUTPUT", ""


def main():
    """逐个执行攻击用例并打印归类结果，最后汇总需要人工判定的用例。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))
    parser.add_argument('--only', default='', help='只跑名称包含该子串的用例')
    parser.add_argument('--timeout', type=float, default=120.0)
    args = parser.parse_args()
    compiler = args.magel.resolve()
    env = dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1')
    abnormal = []
    with tempfile.TemporaryDirectory(prefix='magel-penetration-') as directory:
        work = Path(directory)
        for name, generated, source in CASES:
            if args.only and args.only not in name:
                continue
            text = GENERATORS[name]() if generated else source
            path = work / (name + '.mgl')
            path.write_text(text, encoding='utf-8')
            try:
                result = subprocess.run([str(compiler), 'run', str(path)], cwd=work, capture_output=True,
                                        text=True, encoding='utf-8', errors='replace', env=env,
                                        timeout=args.timeout)
            except subprocess.TimeoutExpired:
                print('[TIMEOUT] %s：超过 %.0f 秒预算' % (name, args.timeout))
                abnormal.append((name, 'TIMEOUT'))
                continue
            kind, detail = classify(result.returncode, result.stdout, result.stderr)
            if kind == 'OUTPUT':
                print('[OUTPUT] %s：%s' % (name, result.stdout.strip().replace('\n', ' | ')[:120]))
                if result.stderr.strip():
                    print('         诊断：%s' % result.stderr.strip().replace('\n', ' | ')[:200])
            else:
                print('[%s] %s：%s' % (kind, name, detail))
                tail = (result.stderr or result.stdout).strip().replace('\n', ' | ')[-400:]
                if tail:
                    print('         %s' % tail)
                abnormal.append((name, kind))
    print('---')
    if abnormal:
        print('需要人工判定的用例：%d 个' % len(abnormal))
        for name, kind in abnormal:
            print('  %-32s %s' % (name, kind))
    else:
        print('所有用例均未触发崩溃或未定义行为特征')
    return 1 if abnormal else 0


if __name__ == '__main__':
    sys.exit(main())
