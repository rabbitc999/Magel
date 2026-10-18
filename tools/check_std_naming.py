#!/usr/bin/env python3
"""检查标准库公开函数/方法的小驼峰、类型的大驼峰，以及 `__` 内部函数的可见性。"""
import re
from library_reference import ROOT, inventory

# `__` 内部函数的声明形态：可选 inline，返回类型里可以有 `::`、`<>`、`,`、空格。
DUNDER = re.compile(r'^[ \t]*(inline[ \t]+)?[A-Za-z_][\w \t:<>,]*?(__\w+)[ \t]*\(', re.M)


def non_inline_bodies():
    """收集带函数体却没有 `inline` 的 `__` 函数：它们在语言层面仍是可跨模块调用的。"""
    errors = []
    for path in sorted((ROOT / 'source/std').glob('*.mgl')):
        text = path.read_text(encoding='utf-8')
        for match in DUNDER.finditer(text):
            if match.group(1):
                continue
            depth, index = 1, match.end()
            while index < len(text) and depth:
                depth += (text[index] == '(') - (text[index] == ')')
                index += 1
            # 形参表之后第一个非空字符为 `{` 说明这是定义而不是宿主声明。
            if text[index:].lstrip().startswith('{'):
                line = text.count('\n', 0, match.start()) + 1
                errors.append(f'{path.relative_to(ROOT).as_posix()}:{line}：带函数体的 {match.group(2)} '
                              '必须声明为 inline（模块私有）；无函数体的宿主声明不受此限')
    return errors


def main():
    """扫描源码及编译器内建签名，拒绝不符合命名约定或可见性约定的公开符号。"""
    errors = []
    functions = types = 0
    for module, rows in inventory().items():
        for row in rows:
            name, kind = row['name'], row['kind']
            # 双下划线是内部编译器/宿主协议；同名构造器沿用类型名。
            if name.startswith('__') or (kind == 'function' and name == row['owner']):
                continue
            if kind == 'function':
                functions += 1
                if not re.fullmatch(r'[a-z][A-Za-z0-9]*', name):
                    errors.append(f'{module}::{row["owner"]}::{name}：函数/方法必须使用小驼峰')
            elif kind in ('struct', 'enum', 'interface'):
                types += 1
                if not re.fullmatch(r'[A-Z][a-z0-9]*(?:[A-Z][a-z0-9]+)*', name):
                    errors.append(f'{module}::{name}：类型必须使用大驼峰')
    errors += non_inline_bodies()
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'标准库命名通过：{functions} 个函数/方法，{types} 个类型。')


if __name__ == '__main__':
    main()
