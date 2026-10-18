"""验证 void main 正常退出为零，panic 在 VM/AOT 中仍失败。"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    """覆盖隐式返回、显式返回、参数入口与 panic，并拒绝其他返回类型。"""
    work = ROOT / 'build/void-main'
    work.mkdir(parents=True, exist_ok=True)
    compiler = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')
    cases = {
        'fallthrough': ('void main(){println("OK");}', False),
        'return': ('void main(){println("OK");return;}', False),
        'arguments': ('void main(std::lang::List<std::lang::String> args){println("OK");}', False),
        'panic': ('void main(){panic("void-main-panic");}', True),
    }
    for label, (text, fails) in cases.items():
        source = work / (label + '.mgl')
        source.write_text(text, encoding='utf-8')
        for backend in ('run', 'aot'):
            output = work / (label + '-aot.exe')
            result = subprocess.run([str(compiler), backend, str(source), '--out', str(output)],
                                    cwd=ROOT, capture_output=True, text=True, encoding='utf-8', timeout=300)
            if backend == 'aot':
                assert result.returncode == 0, result
                result = subprocess.run([str(output)], capture_output=True, text=True, encoding='utf-8', timeout=30)
            assert (result.returncode != 0) == fails, result
            assert ('void-main-panic' in result.stderr if fails else result.stdout.strip() == 'OK'), result
    for result_type in ('uint64', 'int32', 'float64'):
        source = work / 'invalid.mgl'
        source.write_text(result_type + ' main(){return (' + result_type + ')0;}', encoding='utf-8')
        result = subprocess.run([str(compiler), '--check', str(source)], cwd=ROOT, capture_output=True)
        assert result.returncode != 0, result
    print('void main 的正常退出、参数入口和 panic 的 VM/AOT 行为一致。')


if __name__ == '__main__':
    main()
