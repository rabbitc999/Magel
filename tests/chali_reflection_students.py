"""反射版学生信息管理系统的端到端回归。

编译一次字节码，再用共享 VM 宿主 `tests/chali_program_runner.cpp` 逐条跑命令：
每条命令都是一个独立进程，所以这套用例顺带验证了跨进程持久化和停机零资源残留。

用法：python tests/chali_reflection_students.py [--compiler 路径] [--runner 已构建的宿主]

注意：不要用 `chali FILE.svmb 参数` 跑字节码，编译器会把它当源码解析；
字节码一律走共享 VM 宿主。用户日常直接跑源码：`chali main.chl 数据库路径 命令 [参数]`。
"""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'example' / 'reflection-students' / 'main.chl'
RUNNER_SOURCE = ROOT / 'tests' / 'chali_program_runner.cpp'

ONE = '{"id":1,"name":"张伟","age":18,"className":"高一(3)班","score":92}'
ONE_UPDATED = '{"id":1,"name":"张伟","age":19,"className":"高一(3)班","score":95}'
TWO = '{"id":2,"name":"李娜","age":17,"className":"高一(3)班","score":88}'
INJECTION = '{"id":6,"name":"\'; DROP TABLE students; --","age":10,"className":"c","score":10}'

# （传给程序的参数，期望的标准输出）	—— 按顺序在同一份数据库上执行。
# 期望值都是实测得到的真实输出，改程序行为时必须同步改这里。
CASES = [
    # ---- 先建表、新增、列出 ----
    (('schema',), 'Student: id, name, age, className, score'),
    (('add', ONE), ONE),
    (('add', TWO), TWO),
    (('list',), '[' + ONE + ',' + TWO + ']'),
    # ---- 按学号查、改、再查 ----
    (('get', '1'), '[' + ONE + ']'),
    (('update', ONE_UPDATED), ONE_UPDATED),
    (('get', '1'), '[' + ONE_UPDATED + ']'),
    # ---- 搜索与排序 ----
    (('search', '李娜'), '[' + TWO + ']'),
    (('list', 'score', 'desc'), '[' + ONE_UPDATED + ',' + TWO + ']'),
    (('list', 'name', 'asc'), '[' + ONE_UPDATED + ',' + TWO + ']'),
    # ---- 删除后仍在（跨进程持久化） ----
    (('delete', '2'), '已删除'),
    (('list',), '[' + ONE_UPDATED + ']'),
    # ---- 业务校验 ----
    (('get', '999'), '错误：学生不存在'),
    (('delete', '999'), '错误：学生不存在'),
    (('add', '{"id":0,"name":"x","age":10,"className":"c","score":10}'), '错误：学号必须是正整数'),
    (('add', '{"id":3,"name":"王强","age":200,"className":"高二(1)班","score":50}'), '错误：年龄必须在 1 到 150 之间'),
    (('add', '{"id":4,"name":"","age":10,"className":"c","score":10}'), '错误：姓名必须为 1 到 120 字节的非空文本'),
    (('add', '{"id":5,"name":"ok","age":10,"className":"c","score":101}'), '错误：成绩必须在 0 到 100 之间'),
    # ---- 模型与命令边界 ----
    (('add', 'not-json'), '错误：1:1: 这里只认 true / false / null [unexpected-token]'),
    (('list', 'badfield'), '错误：模型 Student 没有字段 badfield'),
    (('list', 'id', 'sideways'), '错误：排序方向必须是 asc 或 desc'),
    (('bogus',), '错误：命令或参数错误：add/update JSON；get/delete 学号；list [字段 [asc|desc]]；search 姓名；schema'),
    # ---- 重复学号：主键冲突要变成诊断 ----
    (('add', ONE), '错误：UNIQUE constraint failed: students.id'),
    # ---- SQL 注入：文本原样入库，表没有被删掉 ----
    (('add', INJECTION), INJECTION),
    (('list',), '[' + ONE_UPDATED + ',' + INJECTION + ']'),
    (('schema',), 'Student: id, name, age, className, score'),
]


def build_runner(cxx, target):
    """用主构建那套参数链接共享 VM 宿主。

    必须带 `-static -ffunction-sections -Wl,--gc-sections`：
    少了这些时宿主会在退出阶段段错误（实测退出码 139）。
    """
    libraries = [ROOT / 'build/chali-runtime/libsimple_ast.a', ROOT / 'build/chali-stackvm-core/libstackvm.a',
                 ROOT / 'build/chali-host-records/librecords.a', ROOT / 'build/chali-runtime/libpreprocess.a']
    objects = [ROOT / 'build/chali-frontend/lztext.o', ROOT / 'build/chali-miniz-tinfl.o']
    missing = [str(path) for path in libraries + objects if not path.exists()]
    if missing:
        raise AssertionError('缺少构建产物，先跑 mingw32-make -j4：' + ', '.join(missing))
    includes = ['source', 'library', 'source/allocator', 'source/bignum', 'source/json', 'source/toml',
                'library/sqlite', 'source/console', 'source/lztext', 'source/simple_ast',
                'source/stackvm', 'source/preprocess']
    command = [cxx, '-std=c++17', '-O2', '-DNDEBUG', '-ffunction-sections', '-fdata-sections',
               '-DCHALI_CUSTOM_ALLOCATOR=1', '-DCHALI_FRONT_END_NO_MAIN=1', '-DCHALI_WITH_REFLECTION=1',
               '-DSTACKVM_WITH_REFLECTION=1']
    command += ['-I' + name for name in includes]
    command += [str(RUNNER_SOURCE), '-o', str(target)]
    command += [str(path) for path in libraries + objects]
    command += ['-Wl,--gc-sections', '-static', '-lwinhttp', '-lm']
    subprocess.run(command, cwd=str(ROOT), check=True, timeout=600)


def main():
    """导出字节码后逐条执行用例，最后报告通过数量。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', default=str(ROOT / ('ChaLi.exe' if os.name == 'nt' else 'chali')))
    parser.add_argument('--runner', default='', help='已构建的共享 VM 宿主，缺省时现场构建')
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    options = parser.parse_args()

    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        runner = Path(options.runner) if options.runner else work / ('runner.exe' if os.name == 'nt' else 'runner')
        if not options.runner:
            build_runner(options.cxx, runner)

        prefix = work / 'students'
        subprocess.run([options.compiler, 'svmb', str(SOURCE), 'out', str(prefix)],
                       cwd=work, check=True, capture_output=True, timeout=600)
        for suffix in ('.svmb', '.data.bin', '.layout'):
            if not prefix.with_suffix(suffix).exists():
                raise AssertionError('没有导出字节码产物：' + str(prefix) + suffix)

        database = work / 'students.db'
        for arguments, expected in CASES:
            result = subprocess.run([str(runner), str(prefix), str(database), *arguments],
                                    cwd=work, capture_output=True, text=True, encoding='utf-8', timeout=300)
            label = ' '.join(arguments)
            # 业务失败也走 println，进程退出码恒为 0；宿主只在 VM 报错或停机仍有资源时返回 1。
            if result.returncode != 0:
                raise AssertionError((label, '退出码', result.returncode, result.stdout, result.stderr))
            if result.stderr.strip():
                raise AssertionError((label, '标准错误非空', result.stderr))
            if result.stdout != expected + '\n':
                raise AssertionError((label, '输出不符', repr(result.stdout), '期望', repr(expected + '\n')))
        print('反射版学生管理系统回归通过：%d 条命令（含持久化、校验、注入与资源回收）' % len(CASES))
        return 0


if __name__ == '__main__':
    sys.exit(main())
