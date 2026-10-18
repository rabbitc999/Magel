# -*- coding: utf-8 -*-
"""逐章试编译：给每章套一个最小壳文档，定位转换错误。"""
import glob
import io
import os
import re
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
LATEX = os.path.normpath(os.path.join(HERE, '..'))
TMP = os.path.join(LATEX, 'tmp')
os.chdir(TMP)

wrapper = r"""\documentclass[cn,ctexfont,fancy,newtx]{colorful}
\input{%s/preamble}
\begin{document}
\input{%s}
\end{document}
"""

fail = []
for tex in sorted(glob.glob(os.path.join(LATEX, 'chapters', '*.tex'))):
    name = os.path.splitext(os.path.basename(tex))[0]
    io.open(name + '_t.tex', 'w', encoding='utf-8').write(
        wrapper % (LATEX.replace('\\', '/'), tex.replace('\\', '/')))
    for f in glob.glob(name + '_t.*'):
        if f.endswith(('.aux', '.log', '.pdf')):
            os.remove(f)
    r = subprocess.run(['xelatex', '-interaction=nonstopmode', name + '_t.tex'],
                       capture_output=True, timeout=240)
    log = io.open(name + '_t.log', encoding='utf-8', errors='replace').read()
    errs = re.findall(r'^!.*$', log, re.M)
    status = 'ok' if not errs else 'FAIL(%d)' % len(errs)
    if errs:
        fail.append(name)
    print('%-14s %s %s' % (name, status, errs[:2]))

print('FAILS:', fail)
