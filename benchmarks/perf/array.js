/* array —— 16 元素固定数组的循环读取，4000000 轮。Node 26（V8 即时编译）。 */
'use strict';

const N = 4000000;
const REPS = 5;

function work(seed) {
  const a = new Array(16);
  for (let k = 0; k < 16; k++) { a[k] = k * 3; }
  let acc = seed;
  for (let i = 0; i < N; i++) {
    acc = (acc + a[acc & 15]) % 2147483648;
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
