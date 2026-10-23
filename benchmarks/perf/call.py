"""call —— 小函数调用，1000000 轮。CPython 3.12，未启用即时编译。"""
import time

N = 1000000
REPS = 5


def mix(x):
    return x * 17 + 3


def work(seed):
    acc = seed
    for i in range(N):
        acc = (mix(acc) + i) & 2147483647
    return acc


sink = work(0)
for r in range(REPS):
    start = time.perf_counter_ns()
    sink += work(r)
    elapsed = time.perf_counter_ns() - start
    print("rep", elapsed)
print("sink", sink)
