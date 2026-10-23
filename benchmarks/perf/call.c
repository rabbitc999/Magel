/* call —— 小函数调用，1000000 轮。C11，gcc -O2。 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 1000000
#define REPS 5

static int64_t mix(int64_t x) { return x * 17 + 3; }

static int64_t work(int64_t seed) {
    int64_t acc = seed;
    for (int64_t i = 0; i < N; i++) {
        acc = (mix(acc) + i) & 2147483647;
    }
    return acc;
}

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

int main(void) {
    volatile int64_t sink = work(0);
    for (int r = 0; r < REPS; r++) {
        long long t0 = now_ns();
        sink += work(r);
        long long t1 = now_ns();
        printf("rep %lld\n", t1 - t0);
    }
    printf("sink %lld\n", (long long)sink);
    return 0;
}
