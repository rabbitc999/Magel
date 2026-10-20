"""验证非空引用在 VM 中的读写和递归行为。"""
import os
from magel_glue_edges import ROOT, MAGEL, EXE, run

def main(names=('reference', 'foreach_reference', 'tree', 'lang_float_bytes', 'reference_glue_optional', 'fourier')):
    work = ROOT / 'build/magel-references'
    work.mkdir(parents=True, exist_ok=True)
    for name in names:
        source = ROOT / f'example/samples/{name}.mgl'
        expected = (ROOT / f'example/expected/{name}.txt').read_text(encoding='utf-8')
        prefix = work / name
        result = run(MAGEL, 'run', source, '--out', prefix, '--step-limit', '1000000')
        assert result.stdout == expected and (not result.stderr), (name, result.stdout, result.stderr)
        run(MAGEL, 'run', source, '--emit', '--out', prefix)
    checkUnboundField()
    print('引用：自动读写、非空语法、只读转换、成员访问、斐波那契与递归树VM验证通过')

def checkUnboundField():
    work = ROOT / 'build/magel-references'
    work.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'example/samples-error/reference_unbound_field.mgl'
    prefix = work / 'unbound-field'
    result = run(MAGEL, 'run', source, '--out', prefix, succeeds=False)
    assert '引用尚未绑定有效元素' in result.stderr, result.stderr
    run(MAGEL, 'run', source, '--emit', '--out', prefix)
    executable = work / ('unbound-field' + EXE)
if __name__ == '__main__':
    main()
