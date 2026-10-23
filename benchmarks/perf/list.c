/* list —— 动态数组 push 再遍历求和，400000 轮。C11，gcc -O2。 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 400000
#define REPS 5

static int64_t work(int64_t seed) {
    int64_t *xs = malloc(sizeof(int64_t) * N);
    size_t len = 0;
    for (int64_t i = 0; i < N; i++) { xs[len++] = i * 3 + seed; }
    int64_t total = seed;
    for (size_t i = 0; i < len; i++) {
        total = (total * 31 + xs[i]) & 2147483647;
    }
    free(xs);
    return total;
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
