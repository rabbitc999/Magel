#!/usr/bin/env python3
"""真实 MAGEL 嵌套 panic、magel/CLI 隐私覆盖及 解释执行/AOT 导出回归。"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel',default='magel')
    parser.add_argument('--exports',action='store_true')
    args=parser.parse_args()
    compiler=Path(args.magel).resolve()
    if not compiler.exists(): compiler=compiler.with_suffix('.exe')
    with tempfile.TemporaryDirectory(prefix='magel-panic-') as folder:
        directory=Path(folder)
        source=directory/'nested.mgl'
        source.write_text('int64 inner() { std::lang::panic("nested failure"); return 0; }\n'
                          'int64 outer() { return inner(); }\nint64 main() { return outer(); }\n',encoding='utf-8')
        def command(items):
            result=subprocess.run([str(x) for x in items],cwd=directory,capture_output=True,timeout=15 if len(items)>1 and items[1]=='run' else 180)
            return result,result.stderr.decode('utf-8',errors='replace')
        for compressed in (False,True):
            prefix=directory/('hidden' if compressed else 'plain')
            cmd=[compiler,'run',source,'--out',prefix]
            if compressed: cmd+=['--compress-symbols']
            result,stderr=command(cmd)
            assert result.returncode!=0 and 'nested failure' in stderr
            assert stderr.splitlines()[0].startswith('magel：运行期错误：panic：'),stderr
            assert ('调用栈' in stderr)==(not compressed),stderr
            if compressed: assert 'inner' not in stderr and '出错位置' not in stderr and 'nested.mgl' not in stderr
        project=directory/'magel.toml'
        project.write_text('[package]\nname = "panic_privacy"\n[magel]\ncompress_symbols = true\n',encoding='utf-8')
        result,stderr=command([compiler,'run',source])
        assert result.returncode!=0 and 'nested failure' in stderr and '调用栈' not in stderr,stderr
        result,stderr=command([compiler,'run',source,'--no-compress-symbols'])
        assert result.returncode!=0 and '调用栈' in stderr
        if args.exports:
            for mode in ('aot',):
                for compressed in (False,True):
                    executable=directory/(mode+('-hidden' if compressed else '-plain')+'.exe')
                    cmd=[compiler,mode,source,'checked','--execute-output',executable,'--opt-level','0','--debug']
                    cmd+=['--compress-symbols' if compressed else '--no-compress-symbols']
                    result,stderr=command(cmd)
                    assert result.returncode==0,stderr
                    result,stderr=command([executable])
                    assert result.returncode!=0 and 'nested failure' in stderr,stderr
                    assert ('调用栈' in stderr)==(not compressed),stderr
                    if compressed: assert 'inner' not in stderr and '出错位置' not in stderr and 'nested.mgl' not in stderr
        # 一个线程忙循环、一个线程阻塞接收；第三个线程 panic，主线程也忙循环。
        threaded=directory/'threaded.mgl'
        threaded.write_text('import std::thread;\n'
            'void spin() { while (true) {} }\n'
            'void blocked() { std::lang::String x = std::thread::receive(); }\n'
            'void leaf() { std::lang::panic("worker failure"); }\n'
            'void crash() { leaf(); }\n'
            'int64 main() { IsolatedThreadHandle a = std::thread::create(spin); '
            'IsolatedThreadHandle b = std::thread::create(blocked); '
            'IsolatedThreadHandle c = std::thread::create(crash); while (true) {} return 0; }\n',encoding='utf-8')
        for compressed in (False,True):
            result,stderr=command([compiler,'run',threaded,'--threads',
                                  '--compress-symbols' if compressed else '--no-compress-symbols'])
            assert result.returncode!=0 and 'worker failure' in stderr and '线程 3' in stderr,stderr
            assert ('调用栈' in stderr)==(not compressed),stderr
            if not compressed: assert 'leaf' in stderr and '[thread 3' in stderr,stderr
        # 主线程 panic 必须停止忙循环子线程；无显式 wait 的 panic 不可被自动收尾吞掉。
        for mode,body in (
            ('mainpanic','IsolatedThreadHandle a = std::thread::create(spin); std::lang::panic("main failure"); return 0;'),
            ('autojoin','IsolatedThreadHandle a = std::thread::create(spin); IsolatedThreadHandle c = std::thread::create(crash); return 0;'),
            ('wait','IsolatedThreadHandle a = std::thread::create(spin); IsolatedThreadHandle c = std::thread::create(crash); std::thread::wait(c); return 0;'),
            ('receive','IsolatedThreadHandle a = std::thread::create(spin); IsolatedThreadHandle c = std::thread::create(crash); std::lang::String x = std::thread::receive(); return 0;')):
            threaded.write_text('import std::thread;\nvoid spin() { while (true) {} }\n'
                'void leaf() { std::lang::panic("worker failure"); }\nvoid crash() { leaf(); }\n'
                'int64 main() { '+body+' }\n',encoding='utf-8')
            result,stderr=command([compiler,'run',threaded,'--threads','--no-compress-symbols'])
            assert result.returncode!=0 and ('main failure' if mode=='mainpanic' else 'worker failure') in stderr,stderr
        # 导出版本使用相同线程任务，测试 main 自动收尾与兄弟线程忙循环取消。
        if args.exports:
            for target in ('aot',):
                for compressed in (False,True):
                    executable=directory/(target+'-thread-'+str(compressed)+'.exe')
                    result,stderr=command([compiler,target,threaded,'checked','--threads','--execute-output',executable,
                                          '--opt-level','0','--debug',
                                          '--compress-symbols' if compressed else '--no-compress-symbols'])
                    assert result.returncode==0,stderr
                    result=subprocess.run([str(executable)],cwd=directory,capture_output=True,timeout=10)
                    stderr=result.stderr.decode('utf-8',errors='replace')
                    assert result.returncode!=0 and 'worker failure' in stderr and '线程 2' in stderr,stderr
                    assert ('调用栈' in stderr)==(not compressed),stderr
                    if not compressed: assert 'leaf' in stderr and '[thread 2' in stderr,stderr
    print('MAGEL panic + magel override + compression privacy' + (' + AOT exports' if args.exports else '') + ': PASS')


if __name__=='__main__':
    main()
