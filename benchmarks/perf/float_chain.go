/* float_chain —— 依赖链式浮点乘加（双精度），1000000 轮。Go，go build 默认优化。 */
package main

import (
	"fmt"
	"time"
)

const N int64 = 1000000
const REPS = 5

func work(seed int64) float64 {
	x := float64(seed)
	for i := int64(0); i < N; i++ {
		x = x*1.0000001 + 0.0000001
	}
	return x
}

func main() {
	sink := work(0)
	for r := 0; r < REPS; r++ {
		start := time.Now()
		sink = sink*0.5 + work(int64(r))
		elapsed := time.Since(start).Nanoseconds()
		fmt.Printf("rep %d\n", elapsed)
	}
	fmt.Printf("sink %.9f\n", sink)
}
