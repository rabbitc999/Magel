"""验证区间、集合字面量及带 else 的循环值表达式。"""
import argparse
from pathlib import Path
import subprocess
import tempfile

def main():
    """检查 VM、checked VM、值复制及必须拒绝的语法与协议使用。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', default='ChaLi.exe')
    options = parser.parse_args()
    compiler = Path(options.chali).resolve()
    fixture = Path(__file__).resolve().parent / 'fixtures/range_loop_values.chl'
    negatives = {'do_loop': 'void main(){do { } while(false);}', 'counted_for': 'void main(){for(int64 i=0;i<3;i+=1){}}', 'half_open_brackets': 'void main(){for(let item in [1,3)){}}', 'int64_endpoint_overflow': 'void main(){for(let item in std::lang::rangeEq<int64>(0,18446744073709551615)){}}', 'decimal_endpoint_overflow': 'void main(){for(let item in std::lang::rangeEq<int64>(0,18446744073709551616)){}}', 'constant_endpoint_overflow': 'void main(){const int64 end=18446744073709551615;for(let item in std::lang::rangeEq<int64>(0,end)){}}', 'expression_endpoint_overflow': 'void main(){for(let item in std::lang::rangeEq<int64>(0,18446744073709551615+0)){}}', 'match_endpoint_overflow': 'void main(){match(0){std::lang::rangeEq<int64>(0,18446744073709551615)=>{} _=>{}}}', 'list_curly': 'void main(){List<int64> value={1,2};}', 'set_square': 'void main(){Set<int64> value=[1,2];}', 'empty_list_curly': 'void main(){List<int64> value={};}', 'float_range_pattern': 'void main(){match(1.5){std::lang::range(1.0,2.0)=>{} _=>{}}}', 'string_range_pattern': 'void main(){match("b"){std::lang::range("a","c")=>{} _=>{}}}', 'no_next_value': 'struct Item {int64 value;};void main(){Item start;Item end;for(let item in std::lang::range<Item>(start,end)){}}', 'no_equal': 'struct Item {int64 value;};void main(){Item value;Item other;match(value){other=>{} _=>{}}}', 'custom_range_pattern': 'struct Item {int64 value;};void main(){Item value;Item end;match(value){std::lang::range<Item>(value,end)=>{} _=>{}}}', 'no_else': 'void main(){int64 value=while(false){yield 1;};}', 'unvalued_else': 'void main(){int64 value=while(false){yield 1;}else{};}', 'incompatible_results': 'void main(){let value=while(false){yield 1;}else{true};}'}
    with tempfile.TemporaryDirectory(prefix='chali-range-loop-') as directory:
        root = Path(directory)

        def invoke(*arguments):
            """运行真实工具命令并保留诊断，拒绝泄漏提示。"""
            result = subprocess.run([str(compiler), *map(str, arguments)], capture_output=True, encoding='utf-8', errors='replace', timeout=360)
            assert result.returncode == 0, result.stdout + result.stderr
            assert '程序结束时' not in result.stderr, result.stderr
            return result.stdout
        assert 'range and loop values ok' in invoke('run', fixture, 'out', root / 'values')
        ownership = root / 'ownership.chl'
        filename = (root / 'owned.txt').as_posix()
        ownership.write_text('import std::io;\nvoid main() {\n    List<String> source = ["first", "second"];\n    String selected = for (let item in source) { item } else { "none" };\n    source[0] = "changed";\n    assert(selected == "first");\n    File first = for (let item in std::lang::range(0,1)) { File::new("' + filename + '", "w") } else { File::new("' + filename + '", "w") };\n    File finalOwner = move(&first);\n    assert(fileWrite(&finalOwner,"moved") == 5);\n    fileClose(&finalOwner);\n    File fallback = while (false) { File::new("' + filename + '", "w") } else { File::new("' + filename + '", "r") };\n    assert(fileRead(&fallback) == "moved");\n    println("loop ownership ok");\n    return;\n}\n', encoding='utf-8')
        assert 'loop ownership ok' in invoke('run', ownership, 'out', root / 'ownership-vm')
        for name, text in negatives.items():
            source = root / (name + '.chl')
            source.write_text(text, encoding='utf-8')
            result = subprocess.run([str(compiler), '--check', str(source)], capture_output=True, encoding='utf-8', errors='replace', timeout=60)
            assert result.returncode != 0, f'{name}: invalid source was accepted'
    print(f'range and loop values: VM, ownership and {len(negatives)} rejection checks passed')
if __name__ == '__main__':
    main()
