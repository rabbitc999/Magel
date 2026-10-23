/* float_chain —— 依赖链式浮点乘加（双精度），1000000 轮。Java 22，两种执行模式共用这一份 class。 */
final class float_chain {
    static final long N = 1000000;
    static final int REPS = 5;

    static double work(long seed) {
        double x = (double) seed;
        for (long i = 0; i < N; i++) {
            x = x * 1.0000001 + 0.0000001;
        }
        return x;
    }

    public static void main(String[] args) {
        double sink = work(0);
        for (int r = 0; r < REPS; r++) {
            long t0 = System.nanoTime();
            sink = sink * 0.5 + work(r);
            long t1 = System.nanoTime();
            System.out.println("rep " + (t1 - t0));
        }
        System.out.printf(java.util.Locale.ROOT, "sink %.9f%n", sink);
    }
}
