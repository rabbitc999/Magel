/* int_chain —— 依赖链式整数乘加（Lehmer 生成器），4000000 轮。Rust，rustc -O。 */
use std::time::Instant;

const N: i64 = 4000000;
const REPS: usize = 5;

fn work(seed: i64) -> i64 {
    let mut acc: i64 = seed;
    let mut i: i64 = 0;
    while i < N {
        acc = (acc * 48271 + i) & 2147483647;
        i += 1;
    }
    acc
}

fn main() {
    let mut sink: i64 = work(0);
    for r in 0..REPS {
        let start = Instant::now();
        sink = sink.wrapping_add(work(r as i64));
        let elapsed = start.elapsed().as_nanos();
        println!("rep {}", elapsed);
    }
    println!("sink {}", sink);
}
