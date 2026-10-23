"""Compare decimal transcendental functions against independent mpmath values."""
import argparse
import decimal
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='tmp/bigfloat-math-driver.exe')
    parser.add_argument('--refresh-reference', action='store_true', help='使用 mpmath 重新生成独立参考值')
    args = parser.parse_args()
    binary = ROOT / args.binary
    binary.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['g++', '-std=c++17', '-O2', '-Isource/bignum',
                    'tests/bigfloat_math_driver.cpp', 'source/bignum/bigint.cpp', 'source/bignum/bigint_multiplication.cpp', 'source/bignum/bigint_division.cpp', 'source/bignum/bigint_number_theory.cpp', 'source/bignum/bigint_bits.cpp', 'source/bignum/bigint_modular.cpp', 'source/bignum/bigint_conversion.cpp',
                    'source/bignum/bigfloat.cpp', 'source/bignum/bigfloat_roots.cpp', 'source/bignum/bigfloat_transcendental.cpp',
                    'source/bignum/bigfloat_trigonometry.cpp', '-o', str(binary)], cwd=ROOT, check=True)
    cases = []
    for digits in [1, 2, 7, 17, 34, 50, 100, 250, 1000]:
        for name, values in {
            'pi': ['0'], 'sin': ['1', '-1', '1e-50', '1e50', '3.1415926535897932384626433832795028841971693993751'],
            'cos': ['0', '1', '-3.5', '1e50'],
            'tan': ['0', '0.75', '-1.5', '1.57079632679489661923132169163975144209858469968755'],
            'asin': ['0', '0.5', '-1', '0.999999999999999999999999999999'],
            'acos': ['0.5', '1', '-1', '0.999999999999999999999999999999'],
            'atan': ['0', '1', '-2', '1e50'],
            'exp': ['0', '1', '-1', '10', '1e-50'],
            'ln': ['1', '2', '1e100', '1.000000000000000000000000000001'],
            'log10': ['10', '0.001', '2', '1e12345', '1e-12345'],
        }.items():
            cases.extend((name, digits, value, '0') for value in values)
        cases.extend(('pow', digits, value, exponent) for value, exponent in [('2','0.5'), ('-2','7'), ('2','-100')])
    cases.extend(('fmod', 50, a, b) for a,b in [('5.5','2'),('-5.5','2'),('5.5','-2'),('1e100','3'),('1e-100','3')])
    errors = [('ln',50,'0','0'), ('ln',50,'-1','0'), ('asin',50,'2','0'), ('acos',50,'-2','0'), ('pow',50,'-2','0.5'), ('fmod',50,'1','0')]
    fixture = ROOT / 'tests/fixtures/bigfloat_math.json'
    if args.refresh_reference:
        import mpmath as mp
        reference=[]
        for name,digits,value,other in cases:
            with mp.workdps(digits+180):
                x,y=mp.mpf(value),mp.mpf(other)
                if name == 'pi': expected=+mp.pi
                elif name == 'pow': expected=mp.power(x,y)
                elif name == 'fmod': expected=x-mp.floor(abs(x/y))*mp.sign(x/y)*y
                else: expected=getattr(mp,{'ln':'log'}.get(name,name))(x)
                reference.append({'case':[name,digits,value,other], 'expected':mp.nstr(expected,digits+140)})
        fixture.parent.mkdir(parents=True,exist_ok=True)
        fixture.write_text(json.dumps({'oracle':'mpmath '+mp.__version__,'values':reference},indent=2)+'\n',encoding='utf-8')
        return 0
    reference=json.loads(fixture.read_text(encoding='utf-8'))
    assert [row['case'] for row in reference['values']] == [list(case) for case in cases], '参考用例需要同步'
    protocol = ''.join(f'{name} {digits} {value} {other}\n' for name,digits,value,other in cases + errors)
    run = subprocess.run([str(binary)], input=protocol, text=True, encoding='utf-8', capture_output=True, timeout=300, check=True)
    outputs = run.stdout.splitlines()
    assert len(outputs) == len(cases) + len(errors), (len(outputs), len(cases), run.stderr)
    failures = []
    for case, output, row in zip(cases, outputs, reference['values']):
        name, digits, value, other = case
        with decimal.localcontext() as context:
            context.prec=digits+180
            expected=decimal.Decimal(row['expected'])
            try:
                actual=decimal.Decimal(output)
                rounded=expected.quantize(decimal.Decimal(1).scaleb(expected.adjusted()-digits+1),rounding=decimal.ROUND_HALF_UP) if expected else decimal.Decimal(0)
                if actual != rounded: failures.append({'case':case,'actual':output,'expected':str(rounded)})
            except decimal.InvalidOperation:
                failures.append({'case':case,'actual':output})
    for case, output in zip(errors, outputs[len(cases):]):
        if not output.startswith('ERROR '): failures.append({'case':case,'actual':output,'expected':'domain error'})
    report = {'cases':len(cases)+len(errors), 'failures':failures, 'oracle':reference['oracle']+' frozen reference'}
    (ROOT/'tmp/bigfloat-math-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
