"""list —— 动态数组 push 再遍历求和，400000 轮。CPython 3.12，未启用即时编译。"""
import time

N = 400000
REPS = 5


def work(seed):
    xs = []
    for i in range(N):
        xs.append(i * 3 + seed)
    total = seed
    for value in xs:
        total = (total * 31 + value) & 2147483647
    return total


sink = work(0)
for r in range(REPS):
    start = time.perf_counter_ns()
    sink += work(r)
    elapsed = time.perf_counter_ns() - start
    print("rep", elapsed)
print("sink", sink)
