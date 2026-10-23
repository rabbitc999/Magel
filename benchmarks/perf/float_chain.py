"""float_chain —— 依赖链式浮点乘加（双精度），1000000 轮。CPython 3.12，未启用即时编译。"""
import time

N = 1000000
REPS = 5


def work(seed):
    x = float(seed)
    for _ in range(N):
        x = x * 1.0000001 + 0.0000001
    return x


sink = work(0)
for r in range(REPS):
    start = time.perf_counter_ns()
    sink = sink * 0.5 + work(r)
    elapsed = time.perf_counter_ns() - start
    print("rep", elapsed)
print("sink", format(sink, ".9f"))
