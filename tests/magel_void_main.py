"""验证 void main 正常退出为零，panic 在 VM 中仍失败。"""
from pathlib import Path
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]

def main():
    """覆盖隐式返回、显式返回、参数入口与 panic，并拒绝其他返回类型。"""
    work = ROOT / 'build/void-main'
    work.mkdir(parents=True, exist_ok=True)
    compiler = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')
    cases = {'fallthrough': ('void main(){println("OK");}', False), 'return': ('void main(){println("OK");return;}', False), 'arguments': ('void main(std::lang::List<std::lang::String> args){println("OK");}', False), 'panic': ('void main(){panic("void-main-panic");}', True)}
    for label, (text, fails) in cases.items():
        source = work / (label + '.mgl')
        source.write_text(text, encoding='utf-8')
    for result_type in ('uint64', 'int32', 'float64'):
        source = work / 'invalid.mgl'
        source.write_text(result_type + ' main(){return (' + result_type + ')0;}', encoding='utf-8')
        result = subprocess.run([str(compiler), '--check', str(source)], cwd=ROOT, capture_output=True)
        assert result.returncode != 0, result
if __name__ == '__main__':
    main()
