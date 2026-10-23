"""验证 AOT 后端：四元式 → 自包含 C → 原生产物，输出与解释执行逐行一致。

覆盖五件事：

1. `chali aot` 编出的 exe 与 `chali run` 的 stdout 一致；
2. **默认 UNCHECKED**——产物里没有越界 / 空指针检查，`--checked` 才有；
3. `--emit-c` 只出 C 源码，不调编译器；
4. **AOT 调外部 C**：`abi` 块声明的函数在产物里变成 extern 调用并链接动态库；
5. **外部 C 调 AOT**：`--shared --export` 出动态库与头文件，C 驱动能调进去。
"""
import argparse
import os
from pathlib import Path
import subprocess
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]

NATIVE_C = r"""
#if defined(_WIN32)
#define CHALI_ABI_EXPORT __declspec(dllexport)
#else
#define CHALI_ABI_EXPORT
#endif
CHALI_ABI_EXPORT int aot_add(int left, int right) { return left + right; }
"""

ABI_SOURCE = """abi "{}"
{{
    int32 aot_add(int32 left, int32 right) = "aot_add";
}}

void main()
{{
    int64 got = (int64)aot_add((int32)20, (int32)22);
    println("aot -> c: {{}}", got);
}}
"""

EXPORT_SOURCE = """int64 add(int64 left, int64 right) { return left + right; }

void main()
{
    println("self check: {}", add(3, 4));
}
"""

DRIVER_C = r"""
#include <stdio.h>
#include "exported.h"
int main(void)
{
    chali_init();
    printf("c -> aot: %lld\n", (long long)f_add(20, 22));
    chali_finish();
    return 0;
}
"""


def main():
    """跑 AOT 后端回归。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali'))
    options = parser.parse_args()
    compiler = options.chali.resolve()
    env = dict(os.environ, CHALI_LANG='zh', CHALI_NO_STDIN_INPUT='1')
    cc = shutil.which(os.environ.get('CHALI_AOT_CC', '')) or shutil.which('gcc') or shutil.which('cc') or shutil.which('clang')
    with tempfile.TemporaryDirectory(prefix='chali-aot-') as directory:
        work = Path(directory)

        def invoke(command, **kwargs):
            """执行编译或测试程序，捕获可比较的输出。"""
            return subprocess.run(list(map(str, command)), cwd=ROOT, env=env, capture_output=True,
                                  text=True, encoding='utf-8', errors='replace', timeout=300, **kwargs)

        # 1) 产物与解释执行一致
        expected = (ROOT / 'example/expected/aot_basic.txt').read_text(encoding='utf-8')
        sample = ROOT / 'example/samples/aot_basic.chl'
        result = invoke([compiler, 'run', sample])
        assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), result
        print('[通过] aot_basic 解释执行', flush=True)
        if cc is None:
            print('[跳过] 没有 C 编译器，AOT 产物相关检查不跑', flush=True)
            return
        exe = work / 'aot_basic'
        result = invoke([compiler, 'aot', sample, '--out', exe, '--cc', cc])
        assert result.returncode == 0, (result.stdout, result.stderr)
        product = exe.with_suffix('.exe' if os.name == 'nt' else '')
        assert product.exists(), 'AOT 没产出可执行文件'
        result = invoke([product])
        assert (result.returncode, result.stdout) == (0, expected), result
        print('[通过] aot_basic AOT 产物与解释执行逐行一致', flush=True)

        # 2) 默认 UNCHECKED：产物里没有检查调用，`--checked` 才有
        unchecked = (exe.with_suffix('.c')).read_text(encoding='utf-8')
        assert '关（AOT 默认 UNCHECKED）' in unchecked
        checked = work / 'aot_checked'
        result = invoke([compiler, 'aot', sample, '--out', checked, '--cc', cc, '--checked'])
        assert result.returncode == 0, (result.stdout, result.stderr)
        opened = (checked.with_suffix('.c')).read_text(encoding='utf-8')
        assert '开（--checked）' in opened
        # 运行时里本来就定义着 `chali_bounds`，所以比的是**调用点**的条数
        assert opened.count('chali_bounds(') > unchecked.count('chali_bounds('), '--checked 应该多出检查调用'
        print('[通过] AOT 默认 UNCHECKED，`--checked` 才开检查', flush=True)

        # 3) `--emit-c` 只出源码
        emit_only = work / 'aot_emit'
        result = invoke([compiler, 'aot', sample, '--out', emit_only, '--cc', cc, '--emit-c'])
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert (emit_only.with_suffix('.c')).exists()
        assert not (emit_only.with_suffix('.exe' if os.name == 'nt' else '')).exists(), '--emit-c 不该出可执行文件'
        print('[通过] --emit-c 只出 C 源码', flush=True)

        # 4) AOT 调外部 C：abi 块 → extern 调用 + 链接动态库
        native = work / 'native.c'
        native.write_text(NATIVE_C, encoding='utf-8')
        library = work / ('native.dll' if os.name == 'nt' else 'native.so')
        build = subprocess.run([cc, '-shared', '-O2', '-o', str(library), str(native)],
                               capture_output=True, text=True)
        assert build.returncode == 0, build.stderr
        abi = work / 'abi_use.chl'
        abi.write_text(ABI_SOURCE.format(str(library).replace('\\', '/')), encoding='utf-8')
        result = invoke([compiler, 'run', abi])
        assert result.returncode == 0 and result.stdout.strip() == 'aot -> c: 42', result
        abi_exe = work / 'abi_use'
        result = invoke([compiler, 'aot', abi, '--out', abi_exe, '--cc', cc])
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert 'extern int aot_add(' in (abi_exe.with_suffix('.c')).read_text(encoding='utf-8'), 'abi 函数没生成 extern 声明'
        result = invoke([abi_exe.with_suffix('.exe' if os.name == 'nt' else '')])
        assert result.stdout.strip() == 'aot -> c: 42', result
        print('[通过] AOT 产物调用外部 C 函数', flush=True)

        # 5) 外部 C 调 AOT：--shared --export 出动态库与带形参的头文件
        exported = work / 'exported.chl'
        exported.write_text(EXPORT_SOURCE, encoding='utf-8')
        shared = work / 'exported'
        result = invoke([compiler, 'aot', exported, '--out', shared, '--cc', cc, '--shared', '--export', 'add'])
        assert result.returncode == 0, (result.stdout, result.stderr)
        header = (shared.with_suffix('.h')).read_text(encoding='utf-8')
        assert 'int64_t f_add(int64_t' in header, header
        assert 'void chali_init(void);' in header
        driver = work / 'driver.c'
        driver.write_text(DRIVER_C, encoding='utf-8')
        driver_exe = work / ('driver.exe' if os.name == 'nt' else 'driver')
        suffix = '.dll' if os.name == 'nt' else '.so'
        build = subprocess.run([cc, '-O2', '-std=c99', '-I', str(work), '-o', str(driver_exe),
                                str(driver), str(shared.with_suffix(suffix))],
                               capture_output=True, text=True)
        assert build.returncode == 0, build.stderr
        result = subprocess.run([str(driver_exe)], capture_output=True, text=True, timeout=120)
        assert result.stdout.strip() == 'c -> aot: 42', result
        print('[通过] 外部 C 调用 AOT 导出的函数', flush=True)


if __name__ == '__main__':
    main()
