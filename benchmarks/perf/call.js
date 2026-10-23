/* call —— 小函数调用，1000000 轮。Node 26（V8 即时编译）。 */
'use strict';

const N = 1000000;
const REPS = 5;

function mix(x) { return x * 17 + 3; }

function work(seed) {
  let acc = seed;
  for (let i = 0; i < N; i++) {
    acc = (mix(acc) + i) % 2147483648;
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
