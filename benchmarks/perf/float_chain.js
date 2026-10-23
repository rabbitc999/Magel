/* float_chain —— 依赖链式浮点乘加（双精度），1000000 轮。Node 26（V8 即时编译）。 */
'use strict';

const N = 1000000;
const REPS = 5;

function work(seed) {
  let x = seed;
  for (let i = 0; i < N; i++) {
    x = x * 1.0000001 + 0.0000001;
  }
  return x;
}

let sink = work(0);
for (let r = 0; r < REPS; r++) {
  const t0 = process.hrtime.bigint();
  sink = sink * 0.5 + work(r);
  const t1 = process.hrtime.bigint();
  console.log('rep ' + Number(t1 - t0));
}
console.log('sink ' + sink.toFixed(9));
