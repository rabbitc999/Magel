#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""补全关键字与高亮关键字必须一致（都以 `magel.gram` 登记的字面量 token 为准）。

读四处：
  * `tools/native_development.cpp` 的 `keywords` —— 原生语言服务器关键字表；
  * `tools/mageldev.py` 的 `KEYWORDS` —— 语言服务器 `magel lsp` 补全用的关键字表；
  * `vscode/syntaxes/magel.tmLanguage.json` —— 编辑器高亮用的关键字正则；
  * `magel.gram` 里 `%token NAME "字面量"` 登记的词形 token（如 `fn` / `export`）。

判据：四处的“词”集合必须相等（运算符、标点不算）。谁加了关键字忘了同步另一处，这里就红。

用法：`python tools/check_keywords.py`（在本项目根目录跑）。
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

WORD = re.compile(r"^[A-Za-z_][A-Za-z_0-9]*$")


def completion_keywords():
    """`mageldev.py` 里的 KEYWORDS。"""
    text = (ROOT / "tools" / "mageldev.py").read_text(encoding="utf-8")
    match = re.search(r'KEYWORDS\s*=\s*"([^"]+)"\.split\(\)', text)
    if not match:
        raise SystemExit("tools/mageldev.py 里找不到 KEYWORDS")
    return set(match.group(1).split())


def highlight_keywords():
    """高亮里所有关键字 / 类型 / 字面量键的正则里出现的词。"""
    grammar = json.loads((ROOT / "vscode" / "syntaxes" / "magel.tmLanguage.json")
                         .read_text(encoding="utf-8"))
    words = set()

    def walk(patterns):
        for pattern in patterns or []:
            name = pattern.get("name", "")
            if any(part in name for part in ("keyword", "storage", "support.type", "constant.language")):
                # `%` 前处理与 `$` 宏指令不是语言关键字（补全里也不该有），这里不算。
                pattern_text = pattern.get("match", "")
                if not ("preprocessor" in name or "macro" in name or "\\$" in pattern_text or "%" in pattern_text):
                    for group in re.findall(r"\((?:\?:)?([^()]*)\)", pattern_text):
                        words.update(part for part in group.split("|") if WORD.match(part))
            walk(pattern.get("patterns"))

    walk(grammar.get("patterns"))
    for entry in (grammar.get("repository") or {}).values():
        walk([entry])
    return words


def grammar_words():
    """文法登记的词形字面量 token。"""
    text = (ROOT / "source/magel.gram").read_text(encoding="utf-8") + (ROOT / "source/magel_token.gram").read_text(encoding="utf-8")
    words = {match.group(1) for match in
             re.finditer(r'%(?:token|define)\s+[A-Za-z_][A-Za-z_0-9]*\s+"([^"]*)"', text)}
    # 编译器内部类型标签不是应用语言的补全入口。
    return {word for word in words if WORD.match(word) and word != "__typeTag" and not word.startswith("__magel_")}


def main():
    completion = completion_keywords()
    highlight = highlight_keywords()
    grammar = grammar_words()
    native_source = (ROOT / "tools/native_development.cpp").read_text(encoding="utf-8")
    native_block = re.search(r'const std::set<std::string> keywords\s*=\s*\{([^}]+)\}', native_source)
    if not native_block:
        raise SystemExit("找不到原生 LSP 的 keywords 表")
    native = set(re.findall(r'"([A-Za-z_]\w*)"', native_block[1]))
    expected = completion | grammar
    problems = []
    book_style = (ROOT / 'doc/book/latex/preamble.tex').read_text(encoding='utf-8')
    book_match = re.search(r'morekeywords\s*=\s*\{([^}]+)\}', book_style)
    if not book_match:
        problems.append('找不到入门书的 morekeywords 表')
    else:
        book_words = {word.strip() for word in book_match[1].split(',')}
        if 'lambda' in book_words or 'fn' not in book_words:
            problems.append('入门书关键字表必须使用 fn，并删除已移除的 lambda')
    if native != expected:
        problems.append("原生 LSP 关键字与文法不一致：缺少 " + " ".join(sorted(expected-native)) +
                        "；多出 " + " ".join(sorted(native-expected)))
    missing = sorted(expected - highlight)
    extra = sorted(highlight - expected)
    if missing:
        problems.append("高亮缺少关键字：" + " ".join(missing))
    if extra:
        problems.append("高亮多出（语言里已没有）：" + " ".join(extra))
    print(f"补全 {len(completion)} 个 + 文法词形 token {len(grammar)} 个，高亮 {len(highlight)} 个")
    if problems:
        raise SystemExit("\n".join(problems))
    print("关键字一致：原生与 Python 补全 / 文法 / 高亮四处对齐")
    return 0


if __name__ == "__main__":
    sys.exit(main())
