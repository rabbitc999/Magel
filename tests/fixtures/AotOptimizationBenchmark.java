import java.util.ArrayList;
import java.util.Scanner;

/** 与 Magel AOT 对照的同算法负载；Java 数组均为堆数组。 */
public class AotOptimizationBenchmark {
    static volatile long sink;
    static long scalar(long n) { long value=1; for(long i=0;i<n;i++) value=(value*17+i)%1000000007; return value; }
    static long fixedArray(long n) { long[] values=new long[64]; long sum=0; for(long i=0;i<n;i++){int k=(int)(i%64);values[k]=i+1;sum+=values[k];}return sum; }
    static long heapArray(long n) { return fixedArray(n); }
    static long container(long n) { ArrayList<Long> values=new ArrayList<Long>(64);for(long i=0;i<64;i++) values.add(i);long sum=0;for(long i=0;i<n;i++){int k=(int)(i%64);values.set(k,i+1);sum+=values.get(k);}return sum; }
    static long callStep(long value,long i) { return (value*17+i)%1000000007; }
    static long functionCalls(long n) { long value=1;for(long i=0;i<n;i++)value=callStep(value,i);return value; }
    public static void main(String[] args) {
        Scanner input=new Scanner(System.in);long n=input.nextLong();int repeats=input.nextInt();
        for(int r=-20;r<repeats;r++) {
            long begin=System.nanoTime(),value=scalar(n),elapsed=System.nanoTime()-begin;sink=value;if(r>=0)System.out.println("scalar "+elapsed+" "+value);
            begin=System.nanoTime();value=fixedArray(n);elapsed=System.nanoTime()-begin;sink=value;if(r>=0)System.out.println("fixed_array "+elapsed+" "+value);
            begin=System.nanoTime();value=heapArray(n);elapsed=System.nanoTime()-begin;sink=value;if(r>=0)System.out.println("heap_array "+elapsed+" "+value);
            begin=System.nanoTime();value=container(n);elapsed=System.nanoTime()-begin;sink=value;if(r>=0)System.out.println("container "+elapsed+" "+value);
            begin=System.nanoTime();value=functionCalls(n);elapsed=System.nanoTime()-begin;sink=value;if(r>=0)System.out.println("function_calls "+elapsed+" "+value);
        }
    }
}
