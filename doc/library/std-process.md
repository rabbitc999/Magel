# std::process — 宿主进程

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::process;
```

## 公共约定与错误边界

命令交给宿主 shell；processRun 创建读取管道，processRead 读取输出，processClose 关闭并等待。system 同步执行命令，返回宿主状态码。程序自行构造和信任 command 的内容；不将外部输入直接拼入 shell 命令。

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

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [processRun](#api-0) | `Process processRun(std::lang::String command)` |
| [processRead](#api-1) | `std::lang::String processRead(Process const& process)` |
| [processClose](#api-2) | `int64 processClose(Process const& process)` |
| [system](#api-3) | `int64 system(std::lang::String command)` |

<a id="api-0"></a>

### processRun（1）

**声明**

```chali
Process processRun(std::lang::String command);
```

**行为**

通过 shell 启动读取管道，返回进程句柄。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `command` | `std::lang::String command` | 交给宿主 shell 的命令文本 | 无 |

**返回值**

`Process`。通过 shell 启动读取管道，返回进程句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::process::processRun(command);
```

**源码**：[source/std/process.chl:9](../../source/std/process.chl#L9)。

**完整示例**：[example/samples/std_process_probe.chl](../../example/samples/std_process_probe.chl)。

<a id="api-1"></a>

### processRead（2）

**声明**

```chali
std::lang::String processRead(Process const& process);
```

**行为**

读取进程管道的输出文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `process` | `Process const& process` | 进程管道句柄 | 无 |

**返回值**

`std::lang::String`。读取进程管道的输出文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::process::processRead(process);
```

**源码**：[source/std/process.chl:10](../../source/std/process.chl#L10)。

**完整示例**：[example/samples/std_process_probe.chl](../../example/samples/std_process_probe.chl)。

<a id="api-2"></a>

### processClose（3）

**声明**

```chali
int64 processClose(Process const& process);
```

**行为**

关闭管道并等待进程，返回宿主状态码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `process` | `Process const& process` | 进程管道句柄 | 无 |

**返回值**

`int64`。关闭管道并等待进程，返回宿主状态码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::process::processClose(process);
```

**源码**：[source/std/process.chl:11](../../source/std/process.chl#L11)。

**完整示例**：[example/samples/std_process_probe.chl](../../example/samples/std_process_probe.chl)。

<a id="api-3"></a>

### system（4）

**声明**

```chali
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

```chali
std::process::system(command);
```

**源码**：[source/std/process.chl:12](../../source/std/process.chl#L12)。

**完整示例**：[example/samples/std_process_probe.chl](../../example/samples/std_process_probe.chl)。
