/* call —— 小函数调用，1000000 轮。Java 22，两种执行模式共用这一份 class。 */
final class call {
    static final long N = 1000000;
    static final int REPS = 5;

    static long mix(long x) { return x * 17 + 3; }

    static long work(long seed) {
        long acc = seed;
        for (long i = 0; i < N; i++) {
            acc = (mix(acc) + i) & 2147483647L;
        }
        return acc;
    }

    public static void main(String[] args) {
        long sink = work(0);
        for (int r = 0; r < REPS; r++) {
            long t0 = System.nanoTime();
            sink += work(r);
            long t1 = System.nanoTime();
            System.out.println("rep " + (t1 - t0));
        }
        System.out.println("sink " + sink);
    }
}
