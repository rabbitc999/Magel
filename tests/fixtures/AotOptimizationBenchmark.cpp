/* 五个算例的原生 C++ 对照。
 * 口径与 doc/12-vm-performance.md 一致：原生 C++17，-O2 -DNDEBUG，
 * 不使用 -ffast-math / LTO / 手写闭式求和；五个算例禁止内联进测量包装层，
 * 函数调用算例内部的 callStep 仍允许正常内联。
 * 输入：`n repeats`，先预热一轮（r=-1），随后每轮输出五个算例的
 *       `<名称> <纳秒> <校验值>`，与 Magel AOT / Java 的握手格式一致。
 */
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

static volatile std::int64_t sink;

static std::int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

__attribute__((noinline)) static std::int64_t scalar(std::int64_t n) {
    std::int64_t value = 1;
    for (std::int64_t i = 0; i < n; i++) { value = (value * 17 + i) % 1000000007; }
    return value;
}

__attribute__((noinline)) static std::int64_t fixedArray(std::int64_t n) {
    std::int64_t values[64];
    std::int64_t sum = 0;
    for (std::int64_t i = 0; i < n; i++) { std::int64_t k = i % 64; values[k] = i + 1; sum += values[k]; }
    return sum;
}

__attribute__((noinline)) static std::int64_t heapArray(std::int64_t n) {
    std::vector<std::int64_t> values(64);
    std::int64_t sum = 0;
    for (std::int64_t i = 0; i < n; i++) { std::int64_t k = i % 64; values[k] = i + 1; sum += values[k]; }
    return sum;
}

__attribute__((noinline)) static std::int64_t container(std::int64_t n) {
    std::vector<std::int64_t> values;
    values.reserve(64);
    for (std::int64_t i = 0; i < 64; i++) { values.push_back(i); }
    std::int64_t sum = 0;
    for (std::int64_t i = 0; i < n; i++) { std::int64_t k = i % 64; values[k] = i + 1; sum += values[k]; }
    return sum;
}

static std::int64_t callStep(std::int64_t value, std::int64_t i) { return (value * 17 + i) % 1000000007; }

__attribute__((noinline)) static std::int64_t functionCalls(std::int64_t n) {
    std::int64_t value = 1;
    for (std::int64_t i = 0; i < n; i++) { value = callStep(value, i); }
    return value;
}

int main() {
    std::int64_t n = 0;
    long long repeats = 0;
    if (std::scanf("%lld %lld", &n, &repeats) != 2) { return 2; }
    for (long long r = -1; r < repeats; r++) {
        std::int64_t begin, elapsed, value;
        begin = nowNanos(); value = scalar(n);        elapsed = nowNanos() - begin;
        sink = value; if (r >= 0) { std::printf("scalar %lld %lld\n", (long long)elapsed, (long long)value); }
        begin = nowNanos(); value = fixedArray(n);    elapsed = nowNanos() - begin;
        sink = value; if (r >= 0) { std::printf("fixed_array %lld %lld\n", (long long)elapsed, (long long)value); }
        begin = nowNanos(); value = heapArray(n);     elapsed = nowNanos() - begin;
        sink = value; if (r >= 0) { std::printf("heap_array %lld %lld\n", (long long)elapsed, (long long)value); }
        begin = nowNanos(); value = container(n);     elapsed = nowNanos() - begin;
        sink = value; if (r >= 0) { std::printf("container %lld %lld\n", (long long)elapsed, (long long)value); }
        begin = nowNanos(); value = functionCalls(n); elapsed = nowNanos() - begin;
        sink = value; if (r >= 0) { std::printf("function_calls %lld %lld\n", (long long)elapsed, (long long)value); }
    }
    return 0;
}
