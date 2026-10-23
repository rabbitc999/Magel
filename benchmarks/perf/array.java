/* array —— 16 元素固定数组的循环读取，4000000 轮。Java 22，两种执行模式共用这一份 class。 */
final class array {
    static final long N = 4000000;
    static final int REPS = 5;

    static long work(long seed) {
        long[] a = new long[16];
        for (int k = 0; k < 16; k++) { a[k] = k * 3; }
        long acc = seed;
        for (long i = 0; i < N; i++) {
            acc = (acc + a[(int) (acc & 15)]) & 2147483647L;
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
