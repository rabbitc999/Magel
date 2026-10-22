"""从权威 ChaLi 文法导出 EBNF 附录，校验文档与所有产生式同步。"""
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parents[1]
GRAMMAR = ROOT / "source/chali.gram"
# `%token` / `%define` 的字面量声明与产生式分在两个文件里（`check_keywords.py` 也是两份都读），
# 只扫 `chali.gram` 会把 `PLUS`、`ABI_KW` 这类词形 token 当成未定义符号。
TOKEN_GRAMMAR = ROOT / "source/chali_token.gram"
DOCUMENT = ROOT / "doc/14-grammar-ebnf.md"
BEGIN = "<!-- BEGIN GENERATED CHALI EBNF -->"
END = "<!-- END GENERATED CHALI EBNF -->"
LEXEME = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|:=|%[A-Za-z_]+|[A-Za-z_]\w*|\S')


def generate(source):
    """剥离语义动作和注释，保留每条正式产生式及词法终结符。"""
    source = source.split("\n%code", 1)[0]
    tokens = []
    depth = 0
    for match in LEXEME.finditer(source):
        value = match.group()
        if value.startswith(("//", "/*")):
            continue
        if value == "{":
            depth += 1
        elif value == "}":
            depth -= 1
            if depth < 0:
                raise ValueError("语义动作大括号不平衡")
        elif depth == 0:
            tokens.append(value)
    if depth:
        raise ValueError("语义动作大括号未闭合")
    literals = {}
    regexes = {}
    declarations = source + "\n" + TOKEN_GRAMMAR.read_text(encoding="utf-8")
    for match in re.finditer(r'^%(define|token)\s+(?:(regex)\s+)?(\w+)\s+("(?:\\.|[^"\\])*")', declarations, re.M):
        value = json.loads(match[4].replace(r"\|", "|"))
        (regexes if match[2] else literals)[match[3]] = value
    rules = []
    used = set()
    for index in range(len(tokens) - 1):
        if tokens[index + 1] != ":=":
            continue
        name = tokens[index]
        alternatives = [[]]
        at = index + 2
        while tokens[at] != ";":
            value = tokens[at]
            if value == "|":
                alternatives.append([])
            elif value == "%prec":
                at += 1
            elif value == "%empty":
                alternatives[-1].append('""')
            else:
                used.add(value)
                alternatives[-1].append(json.dumps(literals[value], ensure_ascii=False) if value in literals else value)
            at += 1
        rhs = [", ".join(alt) for alt in alternatives]
        rules.append(name + " = " + ("\n    | ".join(rhs)) + " ;")
    definitions = {rule.split(" = ", 1)[0] for rule in rules}
    unknown = used - definitions - literals.keys() - regexes.keys() - {"VARIABLE", "NUMBER"}
    if unknown:
        raise ValueError(f"未定义文法符号：{sorted(unknown)}")
    assert "function_type" in definitions and "fptr_part" not in definitions and "fptrdef_decl" not in definitions
    literal_lines = [f"| `{name}` | `{value.replace('|', '&#124;')}` |" for name, value in literals.items()]
    regex_lines = [f"{name} = ? 正则 {json.dumps(value, ensure_ascii=False)} ? ;" for name, value in regexes.items()]
    # 附录里写的是文法的**当前仓库相对路径**，从 GRAMMAR 推出来，搬目录时不用再改这里。
    grammar_rel = GRAMMAR.relative_to(ROOT).as_posix()
    return (f"{BEGIN}\n\n以下 {len(rules)} 条产生式由 `{grammar_rel}` 导出，语义动作及解析优先级指令省略。\n\n"
            + "### 终结符登记\n\n| Token | 源码词素 |\n| --- | --- |\n" + "\n".join(literal_lines)
            + "\n\n### 显式正则词法规则\n\n```ebnf\n" + "\n".join(regex_lines) + "\n```\n\n"
            + "### 全部解析产生式\n\n```ebnf\n" + "\n\n".join(rules) + f"\n```\n\n{END}")


def main():
    """更新附录，或以 --check 只检查是否同步。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    text = DOCUMENT.read_text(encoding="utf-8")
    start = text.index(BEGIN)
    end = text.index(END, start) + len(END)
    result = text[:start] + generate(GRAMMAR.read_text(encoding="utf-8")) + text[end:]
    if args.check:
        if result != text:
            raise SystemExit("ChaLi EBNF 文档未同步；运行 python tools/export_chali_ebnf.py")
        print("ChaLi EBNF：全部词法登记与产生式同步检查通过")
    else:
        DOCUMENT.write_text(result, encoding="utf-8", newline="\n")
        print(f"已更新 {DOCUMENT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
