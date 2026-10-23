"""int_chain —— 依赖链式整数乘加（Lehmer 生成器），4000000 轮。CPython 3.12，未启用即时编译。"""
import time

N = 4000000
REPS = 5


def work(seed):
    acc = seed
    for i in range(N):
        acc = (acc * 48271 + i) & 2147483647
    return acc


sink = work(0)
for r in range(REPS):
    start = time.perf_counter_ns()
    sink += work(r)
    elapsed = time.perf_counter_ns() - start
    print("rep", elapsed)
print("sink", sink)
