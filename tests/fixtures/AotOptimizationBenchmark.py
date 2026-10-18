"""与 Magel AOT 对照的同算法负载；标准输入给出迭代次数与重复轮数。"""
import sys
import time

MODULUS = 1000000007


def scalar(n):
    """整数递推：value=(value*17+i)%1000000007。"""
    value = 1
    for i in range(n):
        value = (value * 17 + i) % MODULUS
    return value


def fixed_array(n):
    """预分配定长列表的下标写入与回读。"""
    values = [0] * 64
    total = 0
    for i in range(n):
        k = i % 64
        values[k] = i + 1
        total += values[k]
    return total


def heap_array(n):
    """独立分配的动态列表，Python 无栈数组，语义同 fixed_array。"""
    values = list([0] * 64)
    total = 0
    for i in range(n):
        k = i % 64
        values[k] = i + 1
        total += values[k]
    return total


def container(n):
    """容器：先装入 64 个元素，再做同样的下标写入与回读。"""
    values = [i for i in range(64)]
    total = 0
    for i in range(n):
        k = i % 64
        values[k] = i + 1
        total += values[k]
    return total


def call_step(value, i):
    """函数调用算例的被调函数。"""
    return (value * 17 + i) % MODULUS


def function_calls(n):
    """每轮循环都经过一次 Python 函数调用。"""
    value = 1
    for i in range(n):
        value = call_step(value, i)
    return value


def main():
    """预热一轮后按 repeats 轮输出 `<名称> <纳秒> <校验值>`。"""
    n, repeats = (int(field) for field in sys.stdin.read().split()[:2])
    write = sys.stdout.write
    for r in range(-1, repeats):
        begin = time.perf_counter_ns(); value = scalar(n); elapsed = time.perf_counter_ns() - begin
        if r >= 0:
            write("scalar %d %d\n" % (elapsed, value))
        begin = time.perf_counter_ns(); value = fixed_array(n); elapsed = time.perf_counter_ns() - begin
        if r >= 0:
            write("fixed_array %d %d\n" % (elapsed, value))
        begin = time.perf_counter_ns(); value = heap_array(n); elapsed = time.perf_counter_ns() - begin
        if r >= 0:
            write("heap_array %d %d\n" % (elapsed, value))
        begin = time.perf_counter_ns(); value = container(n); elapsed = time.perf_counter_ns() - begin
        if r >= 0:
            write("container %d %d\n" % (elapsed, value))
        begin = time.perf_counter_ns(); value = function_calls(n); elapsed = time.perf_counter_ns() - begin
        if r >= 0:
            write("function_calls %d %d\n" % (elapsed, value))


if __name__ == "__main__":
    main()
