"""验证标准容器名称作为标识符、嵌套泛型与比较运算，覆盖 VM、VM 和 raw 前端。"""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')
SOURCE = 'int64 main(){\n    int64 List=1, Set=2, Map=3;\n    int64 a=1, b=2;\n    if ((a<b)) { a=a+1; }\n    a<b;\n    List<Set;\n    if ((List<Set)) { List=List+0; }\n    bool compared=a<b;\n    int64 shifted=Map >> 1;\n    std::lang::List<std::lang::List<int64>> nested;\n    std::lang::List<int64> inner;\n    inner.push(7);\n    nested.push(inner);\n    std::lang::List</* < ignored > */int64> commented;\n    commented.push(4);\n    if(nested[0][0]!=7 || commented[0]!=4 || shifted!=1) { return 1; }\n    if(List+Set+Map!=6 || a!=2 || compared) { return 2; }\n    return 0;\n}'

def main():
    """运行同一程序的解释执行、VM 和禁用预处理的语法检查。"""
    with tempfile.TemporaryDirectory(prefix='magel-container-identifiers-') as folder:
        work = Path(folder)
        source = work / 'containers.mgl'
        source.write_text(SOURCE, encoding='utf-8')

        def run(*args):
            result = subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=240)
            assert result.returncode == 0, (args, result.stdout, result.stderr)
        run(COMPILER, 'run', source)
        source.write_text('int64 main(){std::lang::List<int64> values;values.push(4);return values[0]-4;}', encoding='utf-8')
        run(COMPILER, '--raw', '--check', source)
if __name__ == '__main__':
    main()
