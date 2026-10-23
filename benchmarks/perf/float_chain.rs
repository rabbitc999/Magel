/* float_chain —— 依赖链式浮点乘加（双精度），1000000 轮。Rust，rustc -O。 */
use std::time::Instant;

const N: i64 = 1000000;
const REPS: usize = 5;

fn work(seed: i64) -> f64 {
    let mut x: f64 = seed as f64;
    let mut i: i64 = 0;
    while i < N {
        x = x * 1.0000001 + 0.0000001;
        i += 1;
    }
    x
}

fn main() {
    let mut sink: f64 = work(0);
    for r in 0..REPS {
        let start = Instant::now();
        sink = sink * 0.5 + work(r as i64);
        let elapsed = start.elapsed().as_nanos();
        println!("rep {}", elapsed);
    }
    println!("sink {:.9}", sink);
}
