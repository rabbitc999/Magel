/* int_chain —— 依赖链式整数乘加（Lehmer 生成器），4000000 轮。Node 26（V8 即时编译）。 */
'use strict';

const N = 4000000;
const REPS = 5;

function work(seed) {
  let acc = seed;
  for (let i = 0; i < N; i++) {
    acc = (acc * 48271 + i) % 2147483648;
  }
  return acc;
}

let sink = work(0);
for (let r = 0; r < REPS; r++) {
  const t0 = process.hrtime.bigint();
  sink += work(r);
  const t1 = process.hrtime.bigint();
  console.log('rep ' + Number(t1 - t0));
}
console.log('sink ' + sink);
