/* array —— 16 元素固定数组的循环读取，4000000 轮。C++17，g++ -O2。 */
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace {
constexpr std::int64_t N = 4000000;
constexpr int REPS = 5;

std::int64_t work(int64_t seed) {
    std::int64_t a[16];
    for (int64_t k = 0; k < 16; k++) { a[k] = k * 3; }
    int64_t acc = seed;
    for (int64_t i = 0; i < N; i++) {
        acc = (acc + a[acc & 15]) & 2147483647;
    }
    return acc;
}

std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

int main() {
    volatile int64_t sink = work(0);
    for (int r = 0; r < REPS; ++r) {
        const std::int64_t t0 = now_ns();
        sink += work(r);
        const std::int64_t t1 = now_ns();
        std::printf("rep %lld\n", static_cast<long long>(t1 - t0));
    }
    std::printf("sink %lld\n", (long long)sink);
    return 0;
}
