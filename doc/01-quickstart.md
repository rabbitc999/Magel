# 01 五分钟快速开始

目标是让你在五分钟内跑起第一个 ChaLi 程序。不需要先理解值语义和引用，那些留给[核心概念](03-core-concepts.md)。

---

## 1. 安装

两条路，选一条：

**方式一：用发布包**

**方式二：从源码构建**

```sh
sh gen.sh      # 用 IELR 从 chali.gram 生成前端（需要 ParseEngine 文法生成器）
make           # 构建 chali 与 build/chali-frontend/libparser.a
```

`gen.sh` 需要一份 [ParseEngine](https://github.com/rabbitc999/ParseEngine) 文法生成器。
最省事是把它放到项目根目录 `./ParseEngine`（Windows 加 `.exe`）；也可以让它待在旁边的
ParseEngine 仓库里，`gen.sh` 会自动去找 `../ParseEngine/build/bin/`。

Windows 上未找到生成器时，`gen.sh` 按顺序尝试 [Gitee](https://gitee.com/rabbitc999/ParseEngine/releases/download/latest/ParseEngine.exe) 与 [GitHub](https://github.com/rabbitc999/ParseEngine/releases/download/latest/ParseEngine.exe) 两处的最新发布版，先成功的那个下载为项目根目录的 `ParseEngine.exe`；已有生成器会直接复用。其他平台请提供可运行的本机生成器，或通过 `ENGINE` 指定路径。

验证装好了：

```sh
chali help
```

> 命令行中 `chali` 在 Windows 上是 `ChaLi.exe`。Git Bash / MSYS 下写 `./chali` 能自动找到 `ChaLi.exe`，
> 光写 `chali` 会被当成 PATH 查找。

---

## 2. 第一个程序

新建 `hello.chl`：

```chali
void main() {
    println("Hello, ChaLi!");

}
```

三件事值得注意：

- 入口函数固定叫 `main`，返回 `int64` 或 `void`；`void main` 正常结束返回进程退出码 0。
- `println` 来自自动加载的核心库，完整写法是 `std::lang::println`；没有命名冲突时前缀可省。
- 分号和大括号的用法和 C 系语言一样 —— 这是刻意的，ChaLi 借用 C 的表达式与块语法。

---

## 3. 运行

```sh
chali run hello.chl
```

输出：

```text
Hello, ChaLi!
```

把源码文件直接交给 `chali` 也会解释运行，后面的内容全部作为程序参数传给 `main`：

```sh
chali hello.chl Alice "two words"
```

需要参数时把入口写成：

```chali
void main(List<String> arguments) {
    println("hello, {}", arguments[0]);

}
```

---

## 4. 发生了什么

`chali run` 做了四件事，都在**同一个程序里**完成：

1. **预处理** —— 处理 `%define`、`%if` 等条件编译指令；
2. **解析与类型检查** —— 语法、类型、引用生命周期检查都在这里，不通过就不执行；
3. **降级为四元式** —— 前端把程序变成 StackVM 的中间表示；
4. **解释执行** —— 直接在当前进程里跑。

想只看前两步（不执行、不产出文件）：

```sh
chali check hello.chl
```

---

## 5. 下一步

| 你想 | 去哪 |
| --- | --- |
| 了解语言特性 | [语言速览](02-language-tour.md) |
| 把程序部署到目标宿主 | [交付安全](05-delivery-security.md) → [跨平台部署](06-deployment.md) |
| 看看标准库有什么 | [标准库概览](07-stdlib-overview.md) |
| 建一个正式工程 | `chali new my-app`，配置见[工具链使用](09-toolchain.md) |

---

## 6. 常用命令速查

| 你想做的事 | 命令 |
| --- | --- |
| 解释运行 | `chali run src/main.chl` |
| 只检查不运行 | `chali check src/main.chl` |
| 输出 JSON 诊断（给编辑器用） | `chali check src/main.chl json` |
| 导出 `.svmb` 字节码 | `chali svmb src/main.chl out build/app` |
| 新建工程 | `chali new hello` |
| 跑测试 | `chali test tests` |
| 格式检查 / 写回 | `chali fmt check src` / `chali fmt write src` |
| 查看帮助 | `chali help` |

更多子命令、`chali.toml` 配置与编译选项见[工具链使用](09-toolchain.md)。

解析器生成源码、头文件与报告统一位于 `source/`；对象文件与解析器静态库位于 `build/chali-frontend/`，不会散落到仓库根目录。
