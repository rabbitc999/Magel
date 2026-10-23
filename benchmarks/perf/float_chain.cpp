/* float_chain —— 依赖链式浮点乘加（双精度），1000000 轮。C++17，g++ -O2。 */
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace {
constexpr std::int64_t N = 1000000;
constexpr int REPS = 5;

double work(int64_t seed) {
    double x = static_cast<double>(seed);
    for (int64_t i = 0; i < N; i++) {
        x = x * 1.0000001 + 0.0000001;
    }
    return x;
}

std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

int main() {
    volatile double sink = work(0);
    for (int r = 0; r < REPS; ++r) {
        const std::int64_t t0 = now_ns();
        sink = sink * 0.5 + work(r);
        const std::int64_t t1 = now_ns();
        std::printf("rep %lld\n", static_cast<long long>(t1 - t0));
    }
    std::printf("sink %.9f\n", (double)sink);
    return 0;
}
