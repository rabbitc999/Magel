/* int_chain —— 依赖链式整数乘加（Lehmer 生成器），4000000 轮。Go，go build 默认优化。 */
package main

import (
	"fmt"
	"time"
)

const N int64 = 4000000
const REPS = 5

func work(seed int64) int64 {
	acc := seed
	for i := int64(0); i < N; i++ {
		acc = (acc*48271 + i) & 2147483647
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
