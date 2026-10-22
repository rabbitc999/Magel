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
    """区分条件预处理、语义期宏展开、四元式优化及 VM 装配出口。"""
    c = Canvas()
    title(c, "ChaLi · Compilation" if english else "ChaLi · 编译过程",
          "Generated front end → checked and optimized quadruples → shared VM" if english else
          "文法生成前端 → 语义与借用检查 → 四元式生成及优化 → 共享 VM")
    nodes = [
        (40, 145, "1  Source" if english else "1  源码与模块", [".chl + imports", "Embedded standard library"] if english else [".chl 与模块依赖", "内嵌标准库"]),
        (560, 145, "2  Preprocessing" if english else "2  条件预处理", ["%define / %if / %ifdef", "Source locations retained"] if english else ["%define / %if / %ifdef", "保留源文件与行号"]),
        (1080, 145, "3  Lexing and parsing" if english else "3  词法与语法解析", ["Generated from chali.gram", "Build AST, including macro templates"] if english else ["由 chali.gram 生成的前端", "构建 AST 与宏模板"]),
        (1080, 315, "4  Semantic analysis" if english else "4  语义分析与宏展开", ["Hygienic expansion + type inference", "Overloads and monomorphisation"] if english else ["卫生 AST 宏展开与类型推导", "名称、重载与泛型单态化"]),
        (560, 315, "5  Borrow checking" if english else "5  独立借用检查", ["Control flow and reference origins", "Ownership and lifetime diagnostics"] if english else ["控制流与引用来源", "拥有关系及生命周期诊断"]),
        (40, 315, "6  Generate quadruples" if english else "6  生成四元式", ["Compiler IR + data + layout", "Source locations retained"] if english else ["四元式 IR、数据区与布局", "保留诊断来源"]),
    ]
    for x, y, heading, lines in nodes:
        box(c, x, y, 440, heading, lines)
    for points in [((480, 205), (560, 205)), ((1000, 205), (1080, 205)),
                   ((1300, 265), (1300, 315)), ((1080, 375), (1000, 375)),
                   ((560, 375), (480, 375)), ((260, 435), (260, 485))]:
        arrow(c, points)
    box(c, 40, 485, 720, "7  Optimize quadruples" if english else "7  四元式优化",
        ["Full stackvm::optimize pipeline" if english else "完整 stackvm::optimize 优化流程",
         "Standard iteration budget before VM assembly" if english else "标准迭代预算；优化结果送入 VM 装配"], green=True)
    arrow(c, ((760, 545), (800, 545)))
    box(c, 800, 485, 720, "8  Assemble VM instructions" if english else "8  组装 VM 指令",
        ["Types, labels, integer and comparison lowering" if english else "类型与标签、整数及比较跳转降低",
         "Keep source / PC mapping; prepare execution" if english else "保留源码 / PC 映射，准备执行"], green=True)
    outputs = [
        (40, "Quadruple artifacts" if english else "四元式产物", ["Generated IR + data + layout", "Optimized view: CHALI_DUMP_OPT"] if english else ["生成的四元式、数据区与布局", "优化检查：CHALI_DUMP_OPT"]),
        (560, "In-memory execution" if english else "内存执行", ["Program → shared VM", "No bytecode file required"] if english else ["Program → 共享 VM", "无需先写字节码文件"]),
        (1080, ".svmb bytecode" if english else ".svmb 字节码", ["VM code + execution metadata", "Loaded by a compatible VM"] if english else ["VM 指令与执行元数据", "由兼容版本的共享 VM 加载"]),
    ]
    arrow(c, ((40, 375), (20, 375), (20, 675), (260, 675), (260, 720)))
    arrow(c, ((1100, 605), (1100, 665), (780, 665), (780, 720)))
    arrow(c, ((1300, 605), (1300, 720)))
    for x, heading, lines in outputs:
        box(c, x, 720, 440, heading, lines, green=True)
    return c


def execution(english=False):
    """绘制共享 VM 的执行准备、快速路径和精确回退。"""
    c = Canvas()
    title(c, "ChaLi · Execution" if english else "ChaLi · 运行过程",
          "Desktop and embedded Linux hosts share the same VM core" if english else
          "桌面与嵌入式 Linux 宿主使用同一份 VM 执行核心")
    box(c, 40, 145, 440, ".chl / in-memory Program" if english else ".chl / 内存 Program",
        ["Compile source, then enter the VM", "Lower internal instructions in memory"] if english else ["源码先编译，再进入 VM", "在内存中降低内部指令"])
    box(c, 560, 145, 440, ".svmb · format 5" if english else ".svmb · 格式 5",
        ["Read and validate code + metadata", "Use the stored internal sequence"] if english else ["读取并校验指令与元数据", "使用产物携带的内部序列"])
    box(c, 1080, 145, 440, "VM host configuration" if english else "VM 宿主配置",
        ["Native registry and memory budget", "Input / output and peripherals"] if english else ["原生注册表与内存预算", "输入输出与外设适配"])
    arrow(c, ((260, 265), (260, 320)))
    arrow(c, ((780, 265), (780, 320)))
    arrow(c, ((1300, 265), (1300, 320)))
    box(c, 40, 320, 1480, "VM execution preparation" if english else "VM 执行准备",
        ["Bind symbols / jumps / natives; allocate per-function continuous slots" if english else
         "绑定符号、跳转和原生入口；按函数分配连续槽位",
         "16-bit local register IDs; wide-slot fallback for oversized functions" if english else
         "16 位局部寄存器编号；超大函数保留宽槽回退"], green=True)
    arrow(c, ((400, 440), (400, 510)))
    arrow(c, ((1160, 440), (1160, 510)))
    box(c, 40, 510, 720, "Internal instruction path" if english else "内部指令快速路径",
        ["Integer register control-flow regions", "Independent loop; periodic safe points"] if english else
        ["整数寄存器控制流区域", "独立分派循环；定期返回安全点"], green=True)
    box(c, 800, 510, 720, "Precise fallback path" if english else "基础指令精确路径",
        ["Dynamic values / debug / hooks", "Step, stack and async boundaries"] if english else
        ["动态值、调试与指令钩子", "步数、栈容量与异步边界"])
    arrow(c, ((400, 630), (400, 665)))
    arrow(c, ((1160, 630), (1160, 665)))
    box(c, 40, 665, 1480, "Update PC → dispatch next instruction / return / stop" if english else
        "更新 PC → 继续分派下一条指令 / 返回 / 终止", [], height=60)
    arrow(c, ((780, 725), (780, 770)))
    box(c, 40, 770, 1480, "Runtime services during execution; output and cleanup at exit" if english else
        "执行期间使用运行时服务；退出时输出结果并清理资源",
        ["Data / heap · host natives · I/O · drop · error reporting" if english else
         "数据区与堆 · 宿主原生函数 · 输入输出 · drop · 错误报告"], height=90)
    return c


def main():
    """生成四套 PNG/SVG 资产，所有文案与几何布局均来自本脚本。"""
    OUT.mkdir(parents=True, exist_ok=True)
    for english in (False, True):
        for name, draw in (("compilation", compilation), ("execution", execution)):
            path = OUT / f"chali-{name}{'-en' if english else ''}"
            draw(english).save(str(path.with_suffix('.png')), str(path.with_suffix('.svg')))
            if not english:
                book_path = ROOT / 'doc/book/images' / path.with_suffix('.png').name
                book_path.parent.mkdir(parents=True, exist_ok=True)
                book_path.write_bytes(path.with_suffix('.png').read_bytes())
            print(path.name)


if __name__ == "__main__":
    main()
