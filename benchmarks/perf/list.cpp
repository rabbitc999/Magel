/* list —— 动态数组 push 再遍历求和，400000 轮。C++17，g++ -O2。 */
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace {
constexpr std::int64_t N = 400000;
constexpr int REPS = 5;

std::int64_t work(int64_t seed) {
    std::int64_t *xs = new std::int64_t[N];
    std::size_t len = 0;
    for (int64_t i = 0; i < N; i++) { xs[len++] = i * 3 + seed; }
    int64_t total = seed;
    for (std::size_t i = 0; i < len; i++) {
        total = (total * 31 + xs[i]) & 2147483647;
    }
    delete[] xs;
    return total;
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
