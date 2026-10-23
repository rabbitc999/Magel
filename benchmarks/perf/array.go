/* array —— 16 元素固定数组的循环读取，4000000 轮。Go，go build 默认优化。 */
package main

import (
	"fmt"
	"time"
)

const N int64 = 4000000
const REPS = 5

func work(seed int64) int64 {
	var a [16]int64
	for k := 0; k < 16; k++ {
		a[k] = int64(k) * 3
	}
	acc := seed
	for i := int64(0); i < N; i++ {
		acc = (acc + a[acc&15]) & 2147483647
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
