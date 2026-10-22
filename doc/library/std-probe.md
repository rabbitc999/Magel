# std::probe — 最小宿主集成探针

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::probe;
```

## 公共约定与错误边界

用于验证模块导入和调用链，twice 执行整数乘二，遵循 int64 运算语义。

## 参考代码

完整程序：[example/samples/std_process_probe.chl](../../example/samples/std_process_probe.chl)。

```powershell
chali run example/samples/std_process_probe.chl
```

<details>
<summary>展开完整参考代码</summary>

```chali
import std::process;
import std::probe;

void main() {
    Process process = std::process::processRun("echo chali-process-ok");
    String output = std::process::processRead(&process);
    int64 status = std::process::processClose(&process);
    assert(status == 0);
    assert(output == "chali-process-ok");
    std::probe::Probe value;
    value.value = std::probe::twice(21);
    println("process={} probe={}", status, value.value);

}
```

</details>

<details>
<summary>示例输出</summary>

```text
process=0 probe=42
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-338"></a>`struct Probe` | 记录值并验证复制、移动与析构行为的辅助类型。 |
| Probe | <a id="symbol-339"></a>`int64 value` | 保存的单个值。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [twice](#api-0) | `int64 twice(int64 x)` |

<a id="api-0"></a>

### twice（1）

**声明**

```chali
int64 twice(int64 x);
```

**行为**

返回 x×2。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `x` | `int64 x` | 输入横坐标/标量 | 无 |

**返回值**

`int64`。返回 x×2。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::probe::twice(x);
```

**源码**：[source/std/probe.chl:5](../../source/std/probe.chl#L5)。

**完整示例**：[example/samples/std_process_probe.chl](../../example/samples/std_process_probe.chl)。
