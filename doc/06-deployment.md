# 06 跨平台部署

同一份 Magel 逻辑，有三种跑法。这一篇讲怎么选、怎么配、以及嵌入式板端和宿主内嵌怎么接。

---

## 1. 三种执行方式

| 方式 | 命令 | 启动速度 | 需要运行时 | 适合 |
| --- | --- | --- | --- | --- |
| **解释执行** | `magel run app.mgl` | 每次都编译 | Magel 工具链 | 开发、脚本、工具 |
| **字节码 + StackVM** | `magel svmb …` → VM 加载 | 快 | StackVM | 交付、嵌入式、程序映像更新 |
| **AOT 本地程序** | `magel aot …` | 不经过 VM 装载，实际启动耗时由程序决定 | 运行时服务编入产物 | 桌面分发、性能敏感 |

### 解释执行

```sh
magel run src/main.mgl
```

预处理 → 解析与检查 → 降级为四元式 → 在同一个进程里跑。改完立刻看结果。

### 字节码

```sh
magel svmb src/main.mgl --out build/app
```

产出 `build/app.svmb`，交给 StackVM 加载。交付形态的选择与加固见[交付安全](05-delivery-security.md)。

### AOT 本地程序

除可执行程序外，AOT 可输出 `.dll` / `.so` 共享库和 `.o` 可重定位文件，生成 C 头文件。
库源码可无 `main`，至少通过一个 `export` 声明公开函数；不支持静态库。
目标格式、链接和调用步骤见[库输出](09-toolchain.md#aot-库输出)。

```sh
magel aot src/main.mgl --out build/app --opt-level=2
```

生成 C++ 再编成目标平台的本地程序。产物名沿用 `--out` 给的名字；Windows 上没有扩展名时补成 `build/app.exe`，因为系统只按扩展名认可执行映像。默认 `checked`，开启额外分配护栏和 panic 调用记录；需要显式关闭这些可选检查时写 `unchecked`。显式指定检查模式的例子：

```sh
magel aot src/main.mgl --out build/app checked opt 2
```

| 选项 | 作用 |
| --- | --- |
| `--out 路径` | 产物前缀 |
| `opt LEVEL` / `--opt-level=N` | 优化级别 |
| `checked` / `unchecked` | 是否保留运行时检查（默认 `checked`） |
| `--separate-modules` | 按模块缓存本地目标文件并统一链接（增量更快） |
| `--module-cache DIR` | 指定模块目标文件缓存目录 |

---

## 2. 桌面

三种方式都能用。分发建议：

- 内部工具、需要频繁更新的 → 字节码 + VM；
- 给最终用户、追求启动速度 → AOT；
- 需要跨平台且不想维护多份构建 → 字节码（同一份 `.svmb` 在各平台的 VM 上都能跑）。

默认运行内存池 **512 MiB**，可用 `--memory-size 字节` 调整（最小 64）。

---

## 3. 嵌入式：前端配置与独立 VM

`magel.toml` 的 `[embedded]` 是前端配置：

```toml
[embedded]
enabled = true
restrict_modules = ["std::math"]     # 在内置禁用之外再追加
max_memory = 1048576                 # 运行内存上限（字节）
```

该配置固定拒绝 std::net、std::process、std::sql、std::io、std::thread、std::coroutine 和
std::concurrency，restrict_modules 只追加。它控制编译期导入与内存上限，命令行覆盖配置。

独立 [`source/stackvm_mcu/`](../source/stackvm_mcu/README.md) 是共享核心的另一种构建消费者：
以无异常、无 RTTI、无 OS 线程等宏配置编译，并注入内存池、文件系统和板级胶水。
不能从 `[embedded]` 的禁用清单推断独立 VM 一定不支持文件系统或协作式协程，
也不能认为缩小内存上限就自动补齐了所有板级后端。

H743 的独立工程已接 SDRAM、FatFS、USART1、LCD 和按键，使用 2/16/4 MiB 三类池。
生成其默认无反射程序时必须传 `--no-reflection`。语言在共同支持的范围内保持一致，
可用模块、特征位、胶水和预算仍须匹配；见[板端工程](../example/mcu/FK-STM32H743XIH6/README.md)。

---

## 4. 嵌入到宿主程序

AOT 生成的 C++ 里带着可直接调用的入口：

```cpp
static int runMagel(int argumentCount, const char *const *arguments,
                    void *buffer, std::size_t memoryBytes);
```

| 参数 | 含义 |
| --- | --- |
| `argumentCount` / `arguments` | 传给 Magel `main` 的参数（不含程序名） |
| `buffer` | 运行内存缓冲区；传 `nullptr` 由程序自行分配 |
| `memoryBytes` | 缓冲区大小，也就是运行内存上限 |

生成的文件里有 `#ifndef MAGEL_NO_MAIN` 包着 `int main(...)`——**定义 `MAGEL_NO_MAIN` 后再编译，就只剩 `runMagel` 可供你的固件调用**，不带自己的 `main`。

该入口具有 translation-unit 内部链接，供集成生成 C++ 的宿主调用；需要稳定 C 接口时使用 AOT 的 `export` 和生成头文件。
AOT 逻辑改变后须重编本地程序；要通过替换字节码更新设备程序，使用独立 VM 加载新映像，不承诺迁移运行中的栈与对象。

更完整的胶水绑定、类型映射、`abi` 块见[宿主集成与 C ABI](08-host-integration.md)。

---

## 5. 内存与看门狗

| 选项 | 作用 |
| --- | --- |
| `--memory-size 字节` | 运行时堆容量（缺省 512 MiB，最小 64） |
| `--watchdog` | 打开软重启看门狗 |
| `--no-watchdog` | 关闭（缺省） |
| `--max-restarts N` | 最多软重启几次（缺省 3，隐含 `--watchdog`） |
| `--watchdog-watermark N` | 内存水位（字节，隐含 `--watchdog`） |
| `--step-limit N` | 本次执行的指令条数上限 |

看门狗在**分配失败 / 显式请求 / 超过水位 / 开启超时后未喂狗**时重跑程序，而不是让进程崩掉。长时间运行的服务可以用它把一次偶发故障限制在"重启一次逻辑"的范围内，见 `std::watchdog`。

---

## 6. 服务端

Magel 自身就能当服务端跑：`std::net` 监听 HTTP、`std::sql` 访问真实 SQLite，浏览器调同源 JSON 接口，不需要 Web 框架。

完整可运行例子见 [`example/web-students/`](../example/web-students/)（学生信息管理，新增 / 查看 / 编辑 / 删除 + 过滤）。

同源只是其中一种摆法。**前后端分离**也做得到：后端只出 `/api/*` 的 JSON（页面一律 404）、
前端是一套独立静态资源、由另一个进程托管，两边靠 CORS 通信——见
[`example/sql-magel/example/ipage-student-server/`](../example/sql-magel/example/ipage-student-server)
（真实 MySQL 库、登录会话、四角色权限）。两种摆法的取舍见[示例项目 §4](13-projects.md#4-web-与服务端)。

注意：嵌入式模式下 `std::net` 与 `std::sql` 是被禁的，服务端场景不要用 `[embedded]`。

---

## 7. 选型速查

| 场景 | 推荐 |
| --- | --- |
| 开发调试 | 解释执行 |
| 桌面交付给最终用户 | AOT（`checked` 或 `unchecked` 按性能需要） |
| 需要隐藏源码 | 压缩字节码或 AOT，见[交付安全](05-delivery-security.md) |
| MCU / 板端 | 字节码 + 共享核心的独立 MCU 宿主 |
| 固件里调业务逻辑 | AOT + `MAGEL_NO_MAIN` + `runMagel` |
| 长期运行的服务 | 字节码或 AOT + `--watchdog` |

---

## 8. 下一步

| 你想 | 去哪 |
| --- | --- |
| 加固交付产物 | [交付安全](05-delivery-security.md) |
| 与 C/C++ 互调 | [宿主集成与 C ABI](08-host-integration.md) |
| 查全部命令行选项 | [工具链使用](09-toolchain.md) |
| 看板端虚拟机宿主 | [`source/stackvm_mcu/`](../source/stackvm_mcu) |
