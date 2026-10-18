# -*- coding: utf-8 -*-
"""把 doc/book/*.md 转换为 LaTeX（模板：STM32_PROGRAM/Book 的 colorful 类）。

用法：python tools/md2tex.py
读取 ../*.md（即 doc/book/），生成 ../chapters/*.tex。
可重跑、确定性输出；book.tex 手写，不归本脚本管。
"""
import io
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
BOOK = os.path.normpath(os.path.join(HERE, '..', '..'))     # doc/book
OUT = os.path.join(BOOK, 'latex', 'chapters')
REPO_WEB = 'https://github.com/rabbitc999/Magel'

FILES = [
    ('01-第一个程序.md', 'ch01.tex'),
    ('02-猜数字.md', 'ch02.tex'),
    ('03-变量与类型.md', 'ch03.tex'),
    ('04-函数.md', 'ch04.tex'),
    ('05-控制流.md', 'ch05.tex'),
    ('06-值语义.md', 'ch06.tex'),
    ('07-引用.md', 'ch07.tex'),
    ('08-资源与作用域.md', 'ch08.tex'),
    ('09-结构体.md', 'ch09.tex'),
    ('10-枚举与模式匹配.md', 'ch10.tex'),
    ('11-容器.md', 'ch11.tex'),
    ('12-字符串与文本.md', 'ch12.tex'),
    ('13-接口与运行期多态.md', 'ch13.tex'),
    ('14-泛型.md', 'ch14.tex'),
    ('15-函数值与闭包.md', 'ch15.tex'),
    ('16-错误处理.md', 'ch16.tex'),
    ('17-迭代器.md', 'ch17.tex'),
    ('18-模块与工程.md', 'ch18.tex'),
    ('19-预处理与宏.md', 'ch19.tex'),
    ('20-注解与反射.md', 'ch20.tex'),
    ('21-与C互操作.md', 'ch21.tex'),
    ('22-交付与部署.md', 'ch22.tex'),
    ('23-综合项目.md', 'ch23.tex'),
    ('24-学生信息管理系统.md', 'ch24.tex'),
    ('25-单片机解释器.md', 'ch25.tex'),
    ('A-常见错误速查.md', 'appendix-a.tex'),
    ('B-标准库速查.md', 'appendix-b.tex'),
    ('README.md', 'preface.tex'),
]

CODE_STYLE = {
    'magel': 'magel', 'text': 'out', 'sh': 'sh',
    'toml': 'cfg', 'json': 'cfg', 'c': 'c', 'cpp': 'cpp',
}


def is_cjk(ch):
    o = ord(ch)
    return (0x3000 <= o <= 0x9fff or 0xf900 <= o <= 0xfaff
            or 0xff00 <= o <= 0xffef or o in (0x2014, 0x2026, 0x201c, 0x201d,
                                              0x2018, 0x2019, 0x00b7, 0x2022))


def disp_len(s):
    return sum(2 if is_cjk(c) else 1 for c in s)


def esc(s):
    """转义 LaTeX 特殊字符。"""
    s = s.replace('\\', r'\textbackslash{}')
    for ch, rep in [('&', r'\&'), ('%', r'\%'), ('$', r'\$'), ('#', r'\#'),
                    ('_', r'\_'), ('{', r'\{'), ('}', r'\}'),
                    ('~', r'\textasciitilde{}'), ('^', r'\textasciicircum{}')]:
        s = s.replace(ch, rep)
    return s


CHARMAP = {
    '→': r'\ensuremath{\rightarrow}',
    '←': r'\ensuremath{\leftarrow}',
    '↑': r'\ensuremath{\uparrow}',
    '↓': r'\ensuremath{\downarrow}',
    '↔': r'\ensuremath{\leftrightarrow}',
    '≠': r'\ensuremath{\neq}',
    '≡': r'\ensuremath{\equiv}',
    '≥': r'\ensuremath{\geq}',
    '≤': r'\ensuremath{\leq}',
    '×': r'\ensuremath{\times}',
    '÷': r'\ensuremath{\div}',
    '✅': r'\textcolor{green!50!black}{\ding{51}}',
    '❌': r'\textcolor{red!80!black}{\ding{55}}',
    '⚠️': '',
    '⚠': '',
    '✗': r'\textcolor{red!80!black}{\ding{55}}',
}


def charmap(s):
    for k, v in CHARMAP.items():
        s = s.replace(k, v)
    return s


def chapter_label(target):
    r"""md 目标文件 -> \label 名；不是书内章节返回 None。"""
    m = re.match(r'^(\d{2})-', target)
    if m:
        return 'ch:' + m.group(1)
    m = re.match(r'^([AB])-', target)
    if m:
        return 'ch:app' + m.group(1)
    return None


def web_url(target):
    """仓库内相对路径（相对 doc/book/）-> Gitee 网络链接。"""
    path = os.path.normpath(os.path.join('doc/book', target)).replace('\\', '/')
    kind = 'tree' if (target.endswith('/') or not os.path.splitext(path)[1]) else 'blob'
    return '%s/%s/master/%s' % (REPO_WEB, kind, path)


class Converter:
    def __init__(self, mode='chapter'):
        self.mode = mode          # 'chapter' | 'appendix' | 'preface'
        self.codes = []
        self.links = []

    # ---------------- 行内转换 ----------------
    def inline(self, s):
        self.codes = []
        self.links = []
        image = re.fullmatch(r'!\[([^\]]*)\]\((images/[^)]+)\)', s.strip())
        if image:
            path = image.group(2)
            if '..' in path or not os.path.isfile(os.path.join(BOOK, path)):
                raise ValueError('无效书稿图片：' + path)
            return '\\begin{center}\n\\includegraphics[width=\\linewidth]{../' + path + '}\\par\n' + esc(image.group(1)) + '\n\\end{center}'
        # 双反引号代码段先于单反引号处理，否则 ``x`` 会被拆成 `x` 加一对裸反引号。
        s = re.sub(r'``([^`\n]+?)``', self._stash_code, s)
        s = re.sub(r'`([^`\n]+)`', self._stash_code, s)
        s = re.sub(r'\*\*(.+?)\*\*', r'\1', s)
        s = re.sub(r'\[([^\]]+)\]\(([^)\s]+)\)', self._stash_link, s)
        s = esc(s)
        s = charmap(s)
        s = self._restore_codes(s)
        s = re.sub(r'\x02(\d+)\x03', self._emit_link, s)
        return s

    def _stash_code(self, m):
        self.codes.append(m.group(1))
        return '\x00%d\x01' % (len(self.codes) - 1)

    def _emit_code(self, m):
        code=self.codes[int(m.group(1))]
        # 限定名、路径、泛型和小驼峰名称允许在边界换行，防止行内代码挤出页边。
        parts=[]
        for at,ch in enumerate(code):
            if at and ch.isupper() and code[at-1].islower(): parts.append(r'\allowbreak{}')
            parts.append(esc(ch))
            if ch in ':,./<>()[]=': parts.append(r'\allowbreak{}')
        return r'\texttt{%s}' % ''.join(parts)

    def _stash_link(self, m):
        self.links.append(self._build_link(m.group(1), m.group(2)))
        return '\x02%d\x03' % (len(self.links) - 1)

    def _build_link(self, text, target):
        text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
        label = chapter_label(target)
        if label:
            return r'\hyperref[%s]{%s}' % (label, self._restore_codes(esc(text)))
        if target.startswith('#'):
            return self._restore_codes(esc(text))
        if target.startswith('http://') or target.startswith('https://'):
            return r'\href{%s}{%s}' % (target, self._restore_codes(esc(text)))
        url = web_url(target)
        return r'\href{%s}{%s}' % (url, self._restore_codes(esc(text)))

    def _restore_codes(self, s):
        return re.sub(r'\x00(\d+)\x01', self._emit_code, s)

    def _emit_link(self, m):
        return self.links[int(m.group(1))]

    # ---------------- 块级转换 ----------------
    def join_para(self, lines):
        out = ''
        for raw in lines:
            ln = raw.strip()
            if not ln:
                continue
            if not out:
                out = ln
                continue
            # 左边界是 CJK（含标点）就不补空格；否则按 md 语义补一个空格
            out += ln if is_cjk(out[-1]) else ' ' + ln
        return out

    def conv_code(self, lang, buf):
        style = CODE_STYLE.get(lang, 'out')
        content = '\n'.join(buf).replace('✗', 'X')
        listing = '\\begin{lstlisting}[style=%s]\n%s\n\\end{lstlisting}' % (style, content)
        # 不放进 minipage/samepage；长短示例均允许自然跨页，并保留框线外的段间距。
        return '\\par\n\\medskip\n' + listing + '\n\\par\n\\medskip'

    def conv_table(self, rows_raw):
        rows = [self.split_row(r) for r in rows_raw]
        if len(rows) < 2 or len(rows[0]) != len(rows[1]) or not all(
                re.fullmatch(r':?-{3,}:?', cell.strip()) for cell in rows[1]):
            raise ValueError('Markdown 表格缺少合法的表头分隔行：' + rows_raw[0])
        header, sep, body = rows[0], rows[1], rows[2:]
        ncol = len(header)
        aligns = []
        for j in range(ncol):
            cell = sep[j] if j < len(sep) else '---'
            if cell.startswith(':') and cell.endswith(':'):
                aligns.append('c')
            elif cell.endswith(':'):
                aligns.append('r')
            else:
                aligns.append('l')
        widths = [max(disp_len((r[j] if j < len(r) else '')) for r in rows)
                  for j in range(ncol)]
        total = sum(widths) + 2 * ncol
        cells = [[self.inline(c) for c in r] for r in rows]

        if total <= 90:
            spec = ''.join(aligns)
            out = ['\\begin{center}', '\\begin{tabular}{%s}' % spec, r'\toprule']
            out.append(' & '.join(cells[0]) + r' \\')
            out.append(r'\midrule')
            for r in cells[2:]:
                first = ('{}' if r and r[0].startswith('[') else '') + ' & '.join(r)
                out.append(first + r' \\')
            out += [r'\bottomrule', r'\end{tabular}', r'\end{center}']
            return '\n'.join(out)

        # 宽表：longtable + 按内容宽度比例分列
        usable = 0.98
        fracs = [w / sum(widths) for w in widths]
        lead = {'l': r'>{\raggedright\arraybackslash}',
                'c': r'>{\centering\arraybackslash}',
                'r': r'>{\raggedleft\arraybackslash}'}
        spec = ''.join('%sp{\\dimexpr %.3f\\linewidth-2\\tabcolsep\\relax}' % (lead[a], f * usable)
                       for a, f in zip(aligns, fracs))
        out = ['{\\small\\setlength{\\tabcolsep}{4pt}',
               '\\begin{longtable}{%s}' % spec, r'\toprule']
        out.append(' & '.join(cells[0]) + r' \\')
        out.append(r'\midrule')
        out.append(r'\endhead')
        for r in cells[2:]:
            first = ('{}' if r and r[0].startswith('[') else '') + ' & '.join(r)
            out.append(first + r' \\')
        out += [r'\bottomrule', r'\end{longtable}', '}']
        return '\n'.join(out)

    def split_row(self, line):
        line = line.strip().strip('|')
        cells, buf, incode = [], '', False
        for ch in line:
            if ch == '`':
                incode = not incode
                buf += ch
            elif ch == '|' and not incode:
                cells.append(buf.strip())
                buf = ''
            else:
                buf += ch
        cells.append(buf.strip())
        return cells

    def conv_quote(self, lines):
        paras = []
        cur = []
        for ln in lines:
            if ln.strip():
                cur.append(ln)
            elif cur:
                paras.append(cur)
                cur = []
        if cur:
            paras.append(cur)
        title = '提示'
        first = paras[0][0].strip() if paras else ''
        m = re.match(r'^\*\*(.+?)\*\*[：:]\s*(.*)$', first)
        if m:
            title = m.group(1).strip()
            rest = m.group(2)
            paras[0] = [rest] + paras[0][1:] if rest else paras[0][1:]
            if not paras[0]:
                paras = paras[1:]
        elif first.startswith('⚠'):
            paras[0][0] = first.lstrip('⚠️').strip()
            title = '警告'
        body = r'\par '.join(self.inline(self.join_para(p)) for p in paras if p)
        return '\\begin{tip}{%s}\n%s\n\\end{tip}' % (self.inline(title), body)

    def conv_list(self, items, kind):
        env = 'itemize' if kind == 'list' else 'enumerate'
        out = []
        opened = False
        for index, it in enumerate(items):
            segs = self.merge_paras(it['segs'])
            started = False
            after_code = False
            for seg in segs:
                if seg[0] == 'p':
                    txt = self.inline(self.join_para(seg[1]))
                    if not txt:
                        continue
                    if after_code:
                        out.append(txt)
                        continue
                    if not opened:
                        options = '[start=%d]' % (index + 1) if env == 'enumerate' and index else ''
                        out.append('\\begin{%s}%s' % (env, options))
                        opened = True
                    out.append(('\\item ' if not started else '') + txt)
                    started = True
                else:
                    # 代码与其后的说明脱离列表环境，下一项按原编号继续。
                    if opened:
                        out.append('\\end{%s}' % env)
                        opened = False
                    out.append(self.conv_code(seg[1], seg[2]))
                    after_code = True
        if opened:
            out.append('\\end{%s}' % env)
        return '\n\n'.join(out)

    def merge_paras(self, segs):
        out = []
        for seg in segs:
            if seg[0] == 'p' and out and out[-1][0] == 'p':
                out[-1][1].extend(seg[1])
            else:
                out.append(list(seg))
        return out

    # ---------------- 标题 ----------------
    def heading(self, level, text):
        text = self.inline(text).strip()
        if level == 1:
            if self.mode == 'preface':
                return ('\\chapter*{前\\hspace{1em}言}\n'
                        '\\markboth{前\\hspace{1em}言}{前\\hspace{1em}言}\n'
                        '\\phantomsection\\label{ch:preface}')
            m = re.match(r'^第 (\d+) 章 (.+)$', text)
            if m:
                return '\\chapter{%s}\n\\label{ch:%s}' % (m.group(2), m.group(1).zfill(2))
            m = re.match(r'^附录 ([AB]) (.+)$', text)
            if m:
                return '\\chapter{%s}\n\\label{ch:app%s}' % (m.group(2), m.group(1))
            return '\\chapter{%s}' % text
        if self.mode == 'preface':
            star = '*'
        else:
            star = ''
        if level == 2:
            text = re.sub(r'^(\d+\.\d+|[AB]\.\d+)\s+', '', text)
            return '\\section%s{%s}' % (star, text)
        text = re.sub(r'^(\d+\.\d+\.\d+|[AB]\.\d+\.\d+)\s+', '', text)
        return '\\subsection%s{%s}' % (star, text)

    # ---------------- 主流程 ----------------
    def parse_blocks(self, lines):
        blocks = []
        i, n = 0, len(lines)
        while i < n:
            raw = lines[i]
            s = raw.strip()
            if s.startswith('```'):
                lang = s[3:].strip()
                i += 1
                buf = []
                while i < n and not lines[i].strip().startswith('```'):
                    buf.append(lines[i])
                    i += 1
                i += 1
                blocks.append(('code', lang, buf))
                continue
            if not s or s == '---' or re.match(r'^\s*<!--.*-->\s*$', raw):
                i += 1
                continue
            if raw.startswith('#'):
                lvl = len(raw) - len(raw.lstrip('#'))
                blocks.append(('h', lvl, raw.lstrip('#').strip()))
                i += 1
                continue
            if raw.startswith('|'):
                tbl = []
                while i < n and lines[i].strip().startswith('|'):
                    tbl.append(lines[i].strip())
                    i += 1
                blocks.append(('table', tbl))
                continue
            if raw.startswith('>'):
                q = []
                while i < n and lines[i].startswith('>'):
                    q.append(re.sub(r'^> ?', '', lines[i]))
                    i += 1
                blocks.append(('quote', q))
                continue
            m = re.match(r'^(-|\d+\.) ', raw)
            if m:
                marker = m.group(1)
                items, i = self.collect_list(lines, i, marker)
                blocks.append(('list' if marker == '-' else 'olist', items))
                continue
            para = [raw]
            i += 1
            while i < n:
                nxt = lines[i]
                st = nxt.strip()
                if (not st or st.startswith('```') or st == '---'
                        or nxt.startswith(('#', '|', '>'))
                        or re.match(r'^(-|\d+\.) ', nxt)
                        or re.match(r'^\s*<!--.*-->\s*$', nxt)):
                    break
                para.append(nxt)
                i += 1
            blocks.append(('p', para))
        return blocks

    def collect_list(self, lines, i, marker):
        items = []
        cur = None
        n = len(lines)
        pat = re.compile(r'^- ') if marker == '-' else re.compile(r'^\d+\. ')
        while i < n:
            raw = lines[i]
            s = raw.strip()
            if s.startswith('```'):
                indent = re.match(r'^\s*', raw).group(0)
                lang = s[3:].strip()
                i += 1
                buf = []
                while i < n and not lines[i].strip().startswith('```'):
                    ln = lines[i]
                    if ln.startswith(indent):
                        ln = ln[len(indent):]
                    buf.append(ln)
                    i += 1
                i += 1
                if cur is None:
                    cur = {'segs': []}
                    items.append(cur)
                cur['segs'].append(('code', lang, buf))
                continue
            if not s or re.match(r'^\s*<!--.*-->\s*$', raw):
                i += 1
                continue
            m = re.match(r'^- (.*)$', raw) if marker == '-' else re.match(r'^\d+\. (.*)$', raw)
            if m:
                cur = {'segs': [('p', [m.group(1)])]}
                items.append(cur)
                i += 1
                continue
            if cur is not None and raw.startswith((' ', '\t')):
                cur['segs'].append(('p', [s]))
                i += 1
                continue
            break
        return items, i

    def convert(self, text):
        out = []
        for blk in self.parse_blocks(text.split('\n')):
            kind = blk[0]
            if kind == 'h':
                out.append(self.heading(blk[1], blk[2]))
            elif kind == 'p':
                out.append(self.inline(self.join_para(blk[1])))
            elif kind == 'code':
                out.append(self.conv_code(blk[1], blk[2]))
            elif kind == 'table':
                out.append(self.conv_table(blk[1]))
            elif kind == 'quote':
                out.append(self.conv_quote(blk[1]))
            elif kind in ('list', 'olist'):
                out.append(self.conv_list(blk[1], kind))
        return '\n\n'.join(out) + '\n'


def main():
    os.makedirs(OUT, exist_ok=True)
    for src, dst in FILES:
        mode = 'preface' if src == 'README.md' else (
            'appendix' if re.match(r'^[AB]-', src) else 'chapter')
        text = io.open(os.path.join(BOOK, src), encoding='utf-8').read()
        tex = Converter(mode).convert(text)
        io.open(os.path.join(OUT, dst), 'w', encoding='utf-8', newline='\n').write(tex)
        print('%-28s -> %s' % (src, dst))


if __name__ == '__main__':
    main()
