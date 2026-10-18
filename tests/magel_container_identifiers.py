"""验证标准容器名称作为标识符、嵌套泛型与比较运算，覆盖 VM、AOT 和 raw 前端。"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')
SOURCE = '''int64 main(){
    int64 List=1, Set=2, Map=3;
    int64 a=1, b=2;
    if ((a<b)) { a=a+1; }
    a<b;
    List<Set;
    if ((List<Set)) { List=List+0; }
    bool compared=a<b;
    int64 shifted=Map >> 1;
    std::lang::List<std::lang::List<int64>> nested;
    std::lang::List<int64> inner;
    inner.push(7);
    nested.push(inner);
    std::lang::List</* < ignored > */int64> commented;
    commented.push(4);
    if(nested[0][0]!=7 || commented[0]!=4 || shifted!=1) { return 1; }
    if(List+Set+Map!=6 || a!=2 || compared) { return 2; }
    return 0;
}'''


def main():
    """运行同一程序的解释执行、AOT 和禁用预处理的语法检查。"""
    with tempfile.TemporaryDirectory(prefix='magel-container-identifiers-') as folder:
        work=Path(folder)
        source=work/'containers.mgl'
        source.write_text(SOURCE,encoding='utf-8')
        def run(*args):
            result=subprocess.run(list(map(str,args)),cwd=ROOT,capture_output=True,text=True,
                                  encoding='utf-8',errors='replace',timeout=240)
            assert result.returncode==0,(args,result.stdout,result.stderr)
        run(COMPILER,'run',source)
        output=work/('aot.exe' if os.name=='nt' else 'aot')
        run(COMPILER,'aot',source,'out',output)
        run(output)
        source.write_text('int64 main(){std::lang::List<int64> values;values.push(4);return values[0]-4;}',encoding='utf-8')
        run(COMPILER,'--raw','--check',source)
    print('容器标识符、比较、右移、嵌套泛型、注释：VM / AOT / raw 通过。')


if __name__=='__main__':
    main()
