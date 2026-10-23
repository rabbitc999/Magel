/* list —— 动态数组 push 再遍历求和，400000 轮。Java 22，两种执行模式共用这一份 class。 */
final class list {
    static final long N = 400000;
    static final int REPS = 5;

    static long work(long seed) {
        long[] xs = new long[(int) N];
        int len = 0;
        for (long i = 0; i < N; i++) { xs[len++] = i * 3 + seed; }
        long total = seed;
        for (int i = 0; i < len; i++) {
            total = (total * 31 + xs[i]) & 2147483647L;
        }
        return total;
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
