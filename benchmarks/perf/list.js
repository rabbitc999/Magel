/* list —— 动态数组 push 再遍历求和，400000 轮。Node 26（V8 即时编译）。 */
'use strict';

const N = 400000;
const REPS = 5;

function work(seed) {
  const xs = [];
  for (let i = 0; i < N; i++) { xs.push(i * 3 + seed); }
  let total = seed;
  for (let i = 0; i < xs.length; i++) {
    total = (total * 31 + xs[i]) % 2147483648;
  }
  return total;
}

let sink = work(0);
for (let r = 0; r < REPS; r++) {
  const t0 = process.hrtime.bigint();
  sink += work(r);
  const t1 = process.hrtime.bigint();
  console.log('rep ' + Number(t1 - t0));
}
console.log('sink ' + sink);
