/* list —— 动态数组 push 再遍历求和，400000 轮。Rust，rustc -O。 */
use std::time::Instant;

const N: i64 = 400000;
const REPS: usize = 5;

fn work(seed: i64) -> i64 {
    let mut xs: Vec<i64> = Vec::with_capacity(N as usize);
    let mut i: i64 = 0;
    while i < N {
        xs.push(i * 3 + seed);
        i += 1;
    }
    let mut total: i64 = seed;
    for value in &xs {
        total = (total * 31 + *value) & 2147483647;
    }
    total
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
