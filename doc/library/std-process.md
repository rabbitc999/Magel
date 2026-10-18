# std::process — 宿主进程

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::process;
```

## 公共约定与错误边界

命令交给宿主 shell；processRun 创建读取管道，processRead 读取输出，processClose 关闭并等待。system 同步执行命令，返回宿主状态码。程序自行构造和信任 command 的内容；不将外部输入直接拼入 shell 命令。

## 参考代码

完整程序：[example/samples/std_process_probe.mgl](../../example/samples/std_process_probe.mgl)。

```powershell
magel run example/samples/std_process_probe.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::process;
import std::probe;

int64 main() {
    proc process = std::process::processRun("echo magel-process-ok");
    String output = std::process::processRead(process);
    int64 status = std::process::processClose(process);
    assert(status == 0);
    assert(output == "magel-process-ok");
    std::probe::Probe value;
    value.value = std::probe::twice(21);
    println("process={} probe={}", status, value.value);
    return 0;
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

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [processRun](#api-0) | `proc processRun(std::lang::String command)` |
| [processRead](#api-1) | `std::lang::String processRead(proc process)` |
| [processClose](#api-2) | `int64 processClose(proc process)` |
| [system](#api-3) | `int64 system(std::lang::String command)` |

<a id="api-0"></a>

### processRun（1）

**声明**

```magel
proc processRun(std::lang::String command);
```

**行为**

通过 shell 启动读取管道，返回进程句柄。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `command` | `std::lang::String command` | 交给宿主 shell 的命令文本 | 无 |

**返回值**

`proc`。通过 shell 启动读取管道，返回进程句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::process::processRun(command);
```

**源码**：[source/std/process.mgl:9](../../source/std/process.mgl#L9)。

**完整示例**：[example/samples/std_process_probe.mgl](../../example/samples/std_process_probe.mgl)。

<a id="api-1"></a>

### processRead（2）

**声明**

```magel
std::lang::String processRead(proc process);
```

**行为**

读取进程管道的输出文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `process` | `proc process` | 进程管道句柄 | 无 |

**返回值**

`std::lang::String`。读取进程管道的输出文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::process::processRead(process);
```

**源码**：[source/std/process.mgl:10](../../source/std/process.mgl#L10)。

**完整示例**：[example/samples/std_process_probe.mgl](../../example/samples/std_process_probe.mgl)。

<a id="api-2"></a>

### processClose（3）

**声明**

```magel
int64 processClose(proc process);
```

**行为**

关闭管道并等待进程，返回宿主状态码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `process` | `proc process` | 进程管道句柄 | 无 |

**返回值**

`int64`。关闭管道并等待进程，返回宿主状态码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::process::processClose(process);
```

**源码**：[source/std/process.mgl:11](../../source/std/process.mgl#L11)。

**完整示例**：[example/samples/std_process_probe.mgl](../../example/samples/std_process_probe.mgl)。

<a id="api-3"></a>

### system（4）

**声明**

```magel
int64 system(std::lang::String command);
```

**行为**

同步执行 shell 命令并返回宿主状态码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `command` | `std::lang::String command` | 交给宿主 shell 的命令文本 | 无 |

**返回值**

`int64`。同步执行 shell 命令并返回宿主状态码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::process::system(command);
```

**源码**：[source/std/process.mgl:12](../../source/std/process.mgl#L12)。

**完整示例**：[example/samples/std_process_probe.mgl](../../example/samples/std_process_probe.mgl)。
