# 执行与交付

[返回文档目录](README.md) · [执行与交付](en/25-execution-delivery.md)

Magel 支持解释执行、字节码交付和 AOT 本地程序。三种方式使用同一份源代码；选择取决于目标平台、运行环境和性能需求。

| 方式 | 命令示例 | 使用场景 |
| --- | --- | --- |
| 解释执行 | `magel run hello.mgl` | 开发、调试和快速验证 |
| 字节码 | `magel svmb hello.mgl` | 将 `.svmb` 交给匹配的 StackVM 宿主执行 |
| AOT | `magel aot hello.mgl` | 生成本地程序，减少解释执行开销 |

执行前可使用 `magel check hello.mgl` 完成预处理、解析与语义检查。创建工程使用 `magel new hello-magel`，入口由 `magel.toml` 的 `[magel] main` 指定。完整选项见[工具链使用](09-toolchain.md)。

## 条件取值

使用 `if` 表达式选择一个结果；只执行被选中的分支。`choose`、`chooseLazy` 和三目运算符 `? :` 均不属于当前语言接口。

```magel
int64 main() {
    bool ready = true;
    String label = if (ready) { "yes" } else { "no" };
    println("{}", label);
    return 0;
}
```

复杂分支可包含声明和其他语句，通过 `yield 值;` 给出结果。更多规则见[自定义错误传播与 if 表达式](26-error-propagation-if.md)。

## 部署边界

字节码和符号压缩提高直接阅读源码的门槛，不提供加密或不可逆保护。交付时应使用匹配的编译器与运行时资产，确认目标宿主提供所需的文件、网络、外设及线程服务。

嵌入式部署按板端预算裁剪运行时，桌面能力不会自动成为板端能力。AOT 需要目标平台对应的本地 C++ 工具链。执行方式的性能差异和测试口径见[性能对比](12-vm-performance.md)，具体部署方式见[跨平台部署](06-deployment.md)。

## 从源码构建

```sh
sh gen.sh
make
sh check.sh
```

`gen.sh` 需要 ParseEngine 文法生成器。完整构建依赖及平台操作见[快速开始](01-quickstart.md)和[工具链使用](09-toolchain.md)。
