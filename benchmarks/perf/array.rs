/* array —— 16 元素固定数组的循环读取，4000000 轮。Rust，rustc -O。 */
use std::time::Instant;

const N: i64 = 4000000;
const REPS: usize = 5;

fn work(seed: i64) -> i64 {
    let mut a = [0i64; 16];
    let mut k = 0;
    while k < 16 { a[k] = (k as i64) * 3; k += 1; }
    let mut acc: i64 = seed;
    let mut i: i64 = 0;
    while i < N {
        acc = (acc + a[(acc & 15) as usize]) & 2147483647;
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
