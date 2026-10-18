# 5700U（Linux + GCC x64）：Magel StackVM 池 / malloc 对照

CPU：AMD Ryzen 7 5700U with Radeon Graphics（8 核 16 线程，15 W APU；
`scaling_governor=powersave`，最高 4.372 GHz）。
系统：Ubuntu 22.04.3 LTS，Linux 6.8.0-138-generic x86_64。
编译器：g++ (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0；CMake `Release`
（`-O3 -DNDEBUG -std=gnu++17`）。默认池为 512 MiB C 池；malloc 模式在配置时加
`-DCMAKE_CXX_FLAGS=-DMAGEL_USE_MALLOC=1`。

测量：`taskset -c 3` 钉住单核；每项进程内先预热一次，再正式运行 7 次取中位数；
计时只包含 `Machine::run`，不含 Magel 编译、加载或进程启动。两种模式复用同一份
`loop.mgl` 产物（20000033 条 VM 指令，程序内 `assert(sum == 499999500000)`）。

| 构建 | 负载 | 指令数 | 池中位数 ms | malloc 中位数 ms | malloc 相对池 | 池峰值字节 | 校验 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| GCC x64 | mc | 20000033 | 883.249 | 858.314 | -2.8% | 331392 | asserted |
| GCC x64 | vm | 18000007 | 767.415 | 774.143 | +0.9% | 16288 | 499999500000 |

重复运行的离散度约 3%–4%，池 / malloc 的差距与离散度同量级，不作为结论。
第一次进程启动要把 CPU 从 `powersave` 低档拉起来，同一负载首轮中位数达 1.15–1.35 s，
稳态才落在上表数值。两次池 / malloc 交替复测中 `mc` 的 malloc 侧仍然略快（约 1%–3%），
但这与运行顺序和机器漂移混在一起，不足以当结论。

原始样本（每行一个「预热一次 + 正式 7 次」的中位数，单位 ms）：

- `mc` 池：888.753 882.106 900.912 870.889 885.070 864.599 883.249
- `mc` malloc：857.787 880.080 847.426 875.259 857.508 863.273 858.314
- `vm` 池：754.318 772.418 758.129 778.634 762.033 770.654 767.415
- `vm` malloc：779.158 774.143 762.908 762.988 783.253 756.331 776.866

复现（仓库根目录）：

```sh
cmake -S magel/stackvm -B build/mcvm-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build/mcvm-bench -j
magel/magel magel/benchmarks/loop.mgl --emit --svmb --out build/mcvm-bench/magel-loop
taskset -c 3 build/mcvm-bench/magel-stackvm-benchmark 1000000 7
taskset -c 3 build/mcvm-bench/magel-stackvm-magel-benchmark \
    build/mcvm-bench/magel-loop.svmb build/mcvm-bench/magel-loop.data.bin build/mcvm-bench/magel-loop.layout 7
# malloc 模式：配置时加 -DCMAKE_CXX_FLAGS=-DMAGEL_USE_MALLOC=1，再跑同样两条命令
```
