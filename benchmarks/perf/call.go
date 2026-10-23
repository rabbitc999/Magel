/* call —— 小函数调用，1000000 轮。Go，go build 默认优化。 */
package main

import (
	"fmt"
	"time"
)

const N int64 = 1000000
const REPS = 5

func mix(x int64) int64 { return x*17 + 3 }

func work(seed int64) int64 {
	acc := seed
	for i := int64(0); i < N; i++ {
		acc = (mix(acc) + i) & 2147483647
	}
	return acc
}

func main() {
	sink := work(0)
	for r := 0; r < REPS; r++ {
		start := time.Now()
		sink += work(int64(r))
		elapsed := time.Since(start).Nanoseconds()
		fmt.Printf("rep %d\n", elapsed)
	}
	fmt.Printf("sink %d\n", sink)
}
