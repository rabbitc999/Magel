"""验证 CAS 与泛型高精度数学的语言接口、错误和资源寿命。"""
import argparse
import json
import os
import random
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

CASES = {
    'numeric_types': '''import std::math;
import std::cas;
int64 main() {
    BigDecimal value = BigDecimal(1);
    value.setPrecision(100);
    BigDecimal converted = Expr::parse("1").decimal(100);
    assert(sin(converted).str() == sin(value).str());
    assert(sin(BigInteger(1)) == sin(BigDecimal(1)));
    assert(pow((uint64)18446744073709551615u, BigDecimal(1)) == BigDecimal("18446744073709551615"));
    float32 power = pow((float32)2, (float32)3);
    float32 remainder = fmod((float32)5.5, (float32)2);
    assert(power == 8.0f && remainder == 1.5f);
    assert(sin((int8)0) == 0.0 && cos((uint16)0) == 1.0);
    assert(!isNaN(value) && !isInf(value));
    BigDecimal powerOfTen = BigDecimal("1e12345");
    powerOfTen.setPrecision(1);
    assert(log10(powerOfTen) == BigDecimal(10000));
    powerOfTen.setPrecision(2);
    assert(log10(powerOfTen) == BigDecimal(12000));
    BigDecimal inversePower = BigDecimal("1e-12345");
    inversePower.setPrecision(2);
    assert(log10(inversePower) == BigDecimal(-12000));
    println("ok"); return 0;
}''',
    'transforms': '''import std::cas;
int64 main() {
    Expr x = Expr::symbol("x");
    assert(Expr::parse("(x+1)^2").expand() == Expr::parse("x^2+2*x+1"));
    assert(Expr::parse("x^2-1").factor().expand() == Expr::parse("x^2-1"));
    assert(Expr::parse("x^2").substitute("x", Expr::parse("3")) == Expr::parse("9"));
    assert(Expr::parse("sin(x)/x").limit("x", Expr::parse("0")) == Expr::parse("1"));
    assert(Expr::parse("x^3").differentiate("x", 2) == Expr::parse("6*x"));
    assert(Expr::parse("sin(x)").taylor("x", Expr::parse("0"), 3) == Expr::parse("x-x^3/6"));
    assert(Expr::parse("-7/3").numerator() == BigInteger(-7));
    println("ok"); return 0;
}''',
    'thread_copy': '''import std::cas;
import std::thread;
import std::message;
void worker(Expr expression) {
    Expr result = expression.differentiate();
    std::message::sendThread<Expr>(std::thread::parent(), result);
}
int64 main() {
    Thread task;
    { Expr value = Expr::parse("123456789012345678901234567890*x^2");
      task = std::thread::createWith<Expr>(worker, value); }
    auto reply = std::message::receiveFor<Expr>(5000);
    assert(reply.isReady());
    assert(std::thread::waitResult(task).isReady());
    Expr result = reply.value;
    assert(result == Expr::parse("246913578024691357802469135780*x"));
    assert(result.childCount() > 0);
    println("ok"); return 0;
}''',
    'auto_overloads': '''float64 choose(float64 x) { return x; }
float32 choose(float32 x) { return x; }
int64 choose(int64 x) { return x; }
auto forward<T>(T x) { return choose(x); }
int64 main() { float32 single = forward(1.0f); float64 real = forward(1.0); int64 integer = forward(1); assert(single == 1.0f && real == 1.0 && integer == 1); println("ok"); return 0; }''',
}
ERRORS = {
    'parse': ('auto result = Expr::parse("x+");', '解析失败'),
    'child': ('Expr x = Expr::parse("sin(x)"); auto result = x.child(1);', '子节点'),
    'number': ('auto result = Expr::symbol("x").numerator();', '数值'),
    'decimal': ('auto result = Expr::symbol("x").decimal();', '自由变量'),
    'order': ('auto result = Expr::symbol("x").differentiate("x", -1);', '0..1000'),
    'precision': ('auto result = pi(0);', '1..100000'),
    'log_domain': ('auto result = ln(BigDecimal(-1));', '对数'),
    'asin_domain': ('auto result = asin(BigDecimal(2));', '[-1,1]'),
    'pow_domain': ('auto result = pow(BigDecimal(-2), BigDecimal("0.5"));', '负'),
    'zero_mod': ('auto result = fmod(BigDecimal(1), BigDecimal(0));', '零'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT/'magel.exe')
    args = parser.parse_args()
    work=ROOT/'tmp/cas-math-language-tests'; work.mkdir(parents=True,exist_ok=True)
    reports=[]

    def run(name, source, expected=None, error=None):
        path=work/(name+'.mgl'); path.write_text(source,encoding='utf-8')
        command=[str(args.magel.resolve()),'run',str(path)]
        if name == 'thread_copy': command.append('--threads')
        result=subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=90,env=dict(os.environ,MAGEL_LANG='zh'))
        (work/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        if error:
            assert result.returncode != 0 and error in result.stderr, (name,result)
        else:
            assert result.returncode == 0 and result.stdout == expected and not result.stderr, (name,result)
        reports.append({'name':name,'passed':True})

    for sample in ['cas','math_generic']:
        run(sample,(ROOT/f'example/samples/{sample}.mgl').read_text(encoding='utf-8'),(ROOT/f'example/expected/{sample}.txt').read_text(encoding='utf-8'))
    candidates=['std_csv','std_encoding','std_utf8','std_binary','std_collections_algorithms','std_complex_fft','std_graph','std_linalg_solve_glue']
    for sample in random.Random(241).sample(candidates,4):
        run(sample,(ROOT/f'example/samples/{sample}.mgl').read_text(encoding='utf-8'),(ROOT/f'example/expected/{sample}.txt').read_text(encoding='utf-8'))
    for name,source in CASES.items(): run(name,source,'ok\n')
    for name,(body,error) in ERRORS.items(): run(name,'import std::cas;\nimport std::math;\nint64 main() { '+body+' return 0; }',error=error)
    (work/'results.json').write_text(json.dumps(reports,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'CAS / 泛型数学语言回归通过：{len(reports)} 项')


if __name__ == '__main__':
    main()
