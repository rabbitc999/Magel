#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 README 使用的 ChaLi 总览图（PNG + SVG，落在 `doc/overview/`）。

运行（在本目录或仓库根都行）：

    python tools/make_overview.py

版式、字体与配色来自同目录的 `overview_canvas.py`（ChaLi 自带的一份画布库，绘制不依赖
外部仓库）；改文案后重跑一遍，图与 README 就同步了。
"""

from __future__ import annotations

from pathlib import Path

from overview_canvas import (
    CARD, CARD_EDGE, FONT_SANS, FONT_SANS_BOLD, NOTE, PANEL, PANEL_EDGE,
    PTITLE, SUBTITLE, TITLE, Canvas, font,
)


COL_X = (40, 410, 780, 1150)
COL_W = 360


def draw() -> Canvas:
    heading = font(FONT_SANS_BOLD, 31)
    subtitle = font(FONT_SANS, 15)
    panel_title = font(FONT_SANS_BOLD, 17)
    section_title = font(FONT_SANS_BOLD, 13)
    item_title = font(FONT_SANS_BOLD, 12)
    item_detail = font(FONT_SANS, 11)
    footer = font(FONT_SANS, 12)

    canvas = Canvas()
    canvas.text(40, 58, "ChaLi 语言 · 工具链与运行时总览", heading, TITLE)
    canvas.text(40, 88,
                "从 .chl 源码到四元式、StackVM 与可部署程序 · 语言 / 编译器 / 运行时 / 导出四层",
                subtitle, SUBTITLE)

    columns = (
        (
            "① CHALI 语言", "chali/source/std/lang.chl · example/samples/",
            (
                ("类型与数据", (
                    ("静态类型与值语义", "struct / enum / tuple"),
                    ("标准容器与文本", "List / Set / Map / String · zip"),
                    ("数值与引用", "int / float64 / BigInteger / BigDecimal · T mut&"),
                )),
                ("表达与控制", (
                    ("模式匹配与错误传播", "match · Result<T,E> · ?"),
                    ("循环与函数", "loop · for-in · fn"),
                )),
                ("复用与扩展", (
                    ("模块", "module / import / inline"),
                    ("宏与泛型", "$macro · name<T> · 类型实参推导"),
                    ("接口协议", "运算、索引、迭代与 std::lang::Drop"),
                )),
            ),
        ),
        (
            "② CHALI 编译器", "chali/source/chali.gram · chali",
            (
                ("编译器从哪里来", (
                    ("CHALI 文法", "chali.gram 定义语法"),
                    ("ParseEngine", "IELR(1) 生成 CHALI 前端"),
                    ("生成与构建", "gen.sh → chali / libparser.a"),
                )),
                ("编译源码", (
                    ("预处理与展开", "%if / 宏 / lambda / 泛型"),
                    ("解析与降级", "类型检查 → simple_ast → 四元式"),
                )),
                ("编译产物", (
                    ("可读中间文件", ".quad / .data.bin / .layout"),
                    ("二进制程序", ".svmb 保存指令与公开符号"),
                    ("构建入口", "chali gen compiler / clear"),
                )),
            ),
        ),
        (
            "③ 运行时与标准库", "chali/source/stackvm · source/glue/",
            (
                ("StackVM", (
                    ("虚拟机执行", "四元式 → 栈指令；调用帧隔离"),
                    ("运行时对象", "堆、对象表、字符串与大数"),
                    ("地址检查", "越界与释放后访问报运行期错误"),
                )),
                ("并发模型", (
                    ("线程", "隔离执行 · 通道 · 取消 · 显式事件"),
                    ("消息与共享", "有界邮箱 · Shared<T> · 原子更新"),
                )),
                ("标准库分层", (
                    ("核心 lang", "List / Set / Map · 查找 / 排序 · 宿主服务"),
                    ("std 模块", "math / random / regex / text / linalg …"),
                    ("集合与反射", "std::collections · std::reflect"),
                )),
            ),
        ),
        (
            "④ VM 交付与宿主", "source/stackvm · source/chali_glue.hpp",
            (
                ("部署形态", (
                    ("VM 宿主集成", "字节码 · 原生注册表 · 同步回调"),
                    ("嵌入式字节码", "固定容量 VM 字节堆"),
                    ("调试与自包含", ".svmb v5 调试节 · 标准库内嵌 exe"),
                )),
                ("宿主交互", (
                    ("C++ 胶水", "NativeRegistry + Codec<T>"),
                    ("外部动态库", "通过 C ABI 绑定导出符号"),
                )),
                ("维护与验证", (
                    ("受管产物", "manifest 登记生成文件与 clear"),
                    ("端到端样例", "example/samples/ + check.sh"),
                    ("导出回归", "tests/chali_generator.py / chali_tool.py"),
                )),
            ),
        ),
    )

    section_ys = (202, 416, 568)
    item_ys = ((216, 278, 340), (430, 492), (582, 644, 706))
    for x, (title, location, groups) in zip(COL_X, columns):
        canvas.rect(x, 118, COL_W, 700, PANEL, PANEL_EDGE, radius=14, width=1.5)
        canvas.text(x + 18, 148, title, panel_title, PTITLE)
        canvas.text(x + 18, 170, location, item_detail, NOTE)
        for section_y, ys, (group_title, entries) in zip(section_ys, item_ys, groups):
            canvas.text(x + 18, section_y, group_title, section_title, TITLE)
            for y, (name, detail) in zip(ys, entries):
                canvas.rect(x + 18, y, COL_W - 36, 52, CARD, CARD_EDGE, radius=8)
                canvas.text(x + 29, y + 21, name, item_title, PTITLE)
                canvas.text(x + 29, y + 41, detail, item_detail, NOTE)

    canvas.text(40, 859,
                "构建关系：ParseEngine 从 chali.gram 生成 CHALI 编译器；编译器将 .chl 降级为四元式，交给共享 StackVM 执行。",
                footer, NOTE)
    canvas.text(40, 885,
                "语言细节与当前支持范围见 README.md；解析器生成器（ParseEngine）总览见它自己的仓库。",
                footer, NOTE)
    return canvas


def main() -> None:
    image_dir = Path(__file__).resolve().parents[1] / "doc" / "overview"
    canvas = draw()
    canvas.save(str(image_dir / "ChaLi-overview.png"),
                str(image_dir / "ChaLi-overview.svg"))
    print("doc/overview/ChaLi-overview.png + .svg")


if __name__ == "__main__":
    main()
