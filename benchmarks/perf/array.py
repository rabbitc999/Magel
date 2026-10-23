"""array —— 16 元素固定数组的循环读取，4000000 轮。CPython 3.12，未启用即时编译。"""
import time

N = 4000000
REPS = 5


def work(seed):
    a = [k * 3 for k in range(16)]
    acc = seed
    for _ in range(N):
        acc = (acc + a[acc & 15]) & 2147483647
    return acc


sink = work(0)
for r in range(REPS):
    start = time.perf_counter_ns()
    sink += work(r)
    elapsed = time.perf_counter_ns() - start
    print("rep", elapsed)
print("sink", sink)
