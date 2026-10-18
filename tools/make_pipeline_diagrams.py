#!/usr/bin/env python3
"""绘制中英文编译、运行流程图；沿用总览图主题，三倍绘制后降采样。"""
from pathlib import Path
import math
import os

os.environ["SE_IMAGE_SCALE"] = "3"
os.environ["SE_IMAGE_OUT_SCALE"] = "1"
from overview_canvas import (
    Canvas, font, FONT_SANS, FONT_SANS_BOLD, SCALE, TITLE, SUBTITLE,
    PANEL, PANEL_EDGE, STEP_FILL, STEP_EDGE, LANG_FILL, LANG_EDGE,
    PTITLE, ARROW,
)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "doc/images"


def arrow(canvas, points):
    """绘制任意方向的折线箭头，线端补圆并同步写入 SVG。"""
    scaled = [(x * SCALE, y * SCALE) for x, y in points]
    canvas.draw.line(scaled, fill=ARROW, width=3 * SCALE)
    for x, y in scaled:
        canvas.draw.ellipse((x - SCALE, y - SCALE, x + SCALE, y + SCALE), fill=ARROW)
    x, y = points[-1]
    px, py = points[-2]
    angle = math.atan2(y - py, x - px)
    head = [(x, y)] + [
        (x - 11 * math.cos(angle) + side * 5 * math.sin(angle),
         y - 11 * math.sin(angle) - side * 5 * math.cos(angle)) for side in (-1, 1)
    ]
    canvas.draw.polygon([(a * SCALE, b * SCALE) for a, b in head], fill=ARROW)
    coords = " ".join(f"{a:g},{b:g}" for a, b in points)
    tip = " ".join(f"{a:g},{b:g}" for a, b in head)
    canvas.svg.append(f'<polyline points="{coords}" fill="none" stroke="{ARROW}" '
                      'stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>')
    canvas.svg.append(f'<polygon points="{tip}" fill="{ARROW}"/>')


def box(canvas, x, y, width, heading, lines, green=False, height=120):
    """绘制节点并检查每行文字宽度，避免文案超出边界。"""
    canvas.rect(x, y, width, height, LANG_FILL if green else PANEL,
                LANG_EDGE if green else PANEL_EDGE, radius=15)
    for offset, text in enumerate((heading, *lines)):
        face = font(FONT_SANS_BOLD if offset == 0 else FONT_SANS, 23 if offset == 0 else 18)
        if canvas.draw.textlength(text, font=face) / SCALE > width - 30:
            raise ValueError(f"流程节点文字过宽：{text}")
        canvas.text_center(x + width / 2, y + 30 + offset * 30, text, face,
                           PTITLE if offset == 0 else SUBTITLE)


def title(canvas, heading, detail):
    """绘制图题与简短说明。"""
    canvas.text(40, 58, heading, font(FONT_SANS_BOLD, 33), TITLE)
    canvas.text(40, 95, detail, font(FONT_SANS, 19), SUBTITLE)


def compilation(english=False):
    """绘制编译阶段与三类交付出口。"""
    c = Canvas()
    title(c, "Magel 0.9.1 · Compilation" if english else "Magel 0.9.1 · 编译过程",
          "Source → generated front end → checked IR → execution / bytecode / AOT" if english else
          "源码 → 文法生成的前端 → 检查与中间产物 → 内存执行 / 字节码 / AOT")
    nodes = [
        (40, 145, "1  Source" if english else "1  源码输入", [".mgl + imports", "Embedded standard library"] if english else [".mgl 与导入模块", "内嵌标准库"]),
        (560, 145, "2  Preprocessing" if english else "2  预处理", ["Macros and conditional compilation", "Source locations retained"] if english else ["宏展开与条件编译", "保留源码位置"]),
        (1080, 145, "3  Parsing" if english else "3  词法与语法解析", ["magel.gram → generated parser", "Build syntax nodes"] if english else ["magel.gram → 生成的解析器", "构造语法节点"]),
        (1080, 335, "4  Semantic lowering" if english else "4  语义降级与泛型", ["Types, overloads, monomorphisation", "Nested types and tuples"] if english else ["类型、重载与泛型单态化", "嵌套类型与元组"]),
        (560, 335, "5  Borrow checking" if english else "5  独立借用检查", ["Control flow and reference origins", "Collect diagnostics"] if english else ["控制流、引用来源与生命周期", "收集诊断"]),
        (40, 335, "6  Intermediate output" if english else "6  生成中间产物", ["Quadruples and data layout", "Compiler IR, absent from .svmb"] if english else ["四元式与数据布局", "四元式不写入 .svmb"]),
    ]
    for x, y, heading, lines in nodes:
        box(c, x, y, 440, heading, lines)
    for points in [((480, 205), (560, 205)), ((1000, 205), (1080, 205)),
                   ((1300, 265), (1300, 335)), ((1080, 395), (1000, 395)),
                   ((560, 395), (480, 395)), ((260, 455), (260, 535))]:
        arrow(c, points)
    box(c, 40, 535, 1480, "7  Assemble VM instructions" if english else "7  组装 VM 指令",
        ["Bind types and labels; lower integer operations and comparison branches for bytecode" if english else
         "建立基础 VM 指令、类型与标签；字节码路径预先降低整数运算与比较跳转",
         "Keep source-PC mapping for diagnostics and precise execution" if english else
         "保留源码与原始 PC 映射，用于诊断和精确执行"], green=True)
    outputs = [
        (40, "In-memory execution" if english else "内存执行", ["Program → desktop VM", "No bytecode file required"] if english else ["Program → 桌面 VM", "无需先写字节码文件"]),
        (560, ".svmb · format 5" if english else ".svmb · 格式 5", ["VM code + internal execution sequence", "Desktop / embedded VM"] if english else ["VM 指令与内部执行序列", "桌面 / 嵌入式 VM"]),
        (1080, "AOT native program" if english else "AOT 本地程序", ["Generated C++ → native compiler", "Executable + runtime services"] if english else ["生成 C++ → 本地编译器", "可执行文件与运行时服务"]),
    ]
    for x, heading, lines in outputs:
        arrow(c, ((x + 220, 655), (x + 220, 720)))
        box(c, x, 720, 440, heading, lines, green=True)
    return c


def execution(english=False):
    """绘制 VM 快速路径、精确回退及 AOT 执行路径。"""
    c = Canvas()
    title(c, "Magel 0.9.1 · Execution" if english else "Magel 0.9.1 · 运行过程",
          "Desktop and embedded VMs share lowering rules; AOT executes native code" if english else
          "桌面与嵌入式 VM 共用内部指令规则；AOT 直接执行本地机器码")
    box(c, 40, 145, 440, ".mgl / in-memory Program" if english else ".mgl / 内存 Program",
        ["Compile source, then enter the VM", "Lower internal instructions in memory"] if english else ["源码先编译，再进入 VM", "在内存中降低内部指令"])
    box(c, 560, 145, 440, ".svmb · format 5" if english else ".svmb · 格式 5",
        ["Read and validate code + metadata", "Use the stored internal sequence"] if english else ["读取并校验指令与元数据", "使用产物携带的内部序列"])
    box(c, 1080, 145, 440, "AOT executable" if english else "AOT 可执行文件",
        ["OS / board starts native code", "No VM instruction dispatch"] if english else ["操作系统 / 板端启动机器码", "无需 VM 指令分派"])
    arrow(c, ((260, 265), (260, 320)))
    arrow(c, ((780, 265), (780, 320)))
    arrow(c, ((1300, 265), (1300, 320)))
    box(c, 40, 320, 960, "VM execution preparation" if english else "VM 执行准备",
        ["Bind symbols / jumps / natives; allocate per-function continuous slots" if english else
         "绑定符号、跳转和原生入口；按函数分配连续槽位",
         "16-bit local register IDs; wide-slot fallback for oversized functions" if english else
         "16 位局部寄存器编号；超大函数保留宽槽回退"], green=True)
    box(c, 1080, 320, 440, "Native execution" if english else "本地执行",
        ["Native arithmetic and control flow", "Native function calls"] if english else ["机器码运算与控制流", "本地函数调用"], green=True)
    arrow(c, ((260, 440), (260, 510)))
    arrow(c, ((780, 440), (780, 510)))
    box(c, 40, 510, 440, "Internal instruction path" if english else "内部指令快速路径",
        ["Integer register control-flow regions", "Independent loop; periodic safe points"] if english else
        ["整数寄存器控制流区域", "独立分派循环；定期返回安全点"], green=True)
    box(c, 560, 510, 440, "Precise fallback path" if english else "基础指令精确路径",
        ["Dynamic values / debug / hooks", "Step, stack and async boundaries"] if english else
        ["动态值、调试与指令钩子", "步数、栈容量与异步边界"])
    arrow(c, ((260, 630), (260, 665)))
    arrow(c, ((780, 630), (780, 665)))
    box(c, 40, 665, 960, "Update PC → dispatch next instruction / return / stop" if english else
        "更新 PC → 继续分派下一条指令 / 返回 / 终止", [], height=60)
    arrow(c, ((520, 725), (520, 770)))
    arrow(c, ((1300, 440), (1300, 770)))
    box(c, 40, 770, 1480, "Runtime services during execution; output and cleanup at exit" if english else
        "执行期间使用运行时服务；退出时输出结果并清理资源",
        ["Data / heap · host natives · I/O · drop / delete · error reporting" if english else
         "数据区与堆 · 宿主原生函数 · 输入输出 · drop / delete · 错误报告"], height=90)
    return c


def main():
    """生成四套 PNG/SVG 资产，所有文案与几何布局均来自本脚本。"""
    OUT.mkdir(parents=True, exist_ok=True)
    for english in (False, True):
        for name, draw in (("compilation", compilation), ("execution", execution)):
            path = OUT / f"magel-{name}{'-en' if english else ''}"
            draw(english).save(str(path.with_suffix('.png')), str(path.with_suffix('.svg')))
            print(path.name)


if __name__ == "__main__":
    main()
