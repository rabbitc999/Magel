/* float_chain —— 依赖链式浮点乘加（双精度），1000000 轮。C11，gcc -O2。 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 1000000
#define REPS 5

static double work(int64_t seed) {
    double x = (double)seed;
    for (int64_t i = 0; i < N; i++) {
        x = x * 1.0000001 + 0.0000001;
    }
    return x;
}

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

int main(void) {
    volatile double sink = work(0);
    for (int r = 0; r < REPS; r++) {
        long long t0 = now_ns();
        sink = sink * 0.5 + work(r);
        long long t1 = now_ns();
        printf("rep %lld\n", t1 - t0);
    }
    printf("sink %.9f\n", (double)sink);
    return 0;
}
