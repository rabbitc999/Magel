/* list —— 动态数组 push 再遍历求和，400000 轮。Go，go build 默认优化。 */
package main

import (
	"fmt"
	"time"
)

const N int64 = 400000
const REPS = 5

func work(seed int64) int64 {
	xs := make([]int64, 0, N)
	for i := int64(0); i < N; i++ {
		xs = append(xs, i*3+seed)
	}
	total := seed
	for _, value := range xs {
		total = (total*31 + value) & 2147483647
	}
	return total
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
