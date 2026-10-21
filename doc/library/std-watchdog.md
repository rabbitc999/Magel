# std::watchdog — 软重启看门狗

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::watchdog;
```

## 公共约定与错误边界

默认关闭。enable 设置内存水位字节数；requestRestart 请求 VM 安全点结束本轮，由宿主重置 arena 与分配身份后重跑。enableTimeout 的超时由 VM 内部截止时间检查，已注入时钟时单位为纳秒，否则为逻辑指令数；kick 推迟截止时间，disableTimeout 关闭超时。不依赖事件回调；不能抢占正在执行的阻塞宿主函数。

## 参考代码

完整程序：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

```powershell
magel run doc/examples/library/watchdog.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::watchdog;

/** @brief 验证看门狗默认关闭、启用结果，以及时间维度（超时未喂狗）同样默认关闭；不请求实际重启。 */
int64 main() {
    Result<bool,WatchdogError> disabled = std::watchdog::requestRestart();
    assert(disabled.isErr());
    Result<int64,WatchdogError> invalid = std::watchdog::enable(-1);
    assert(invalid.isErr());
    Result<int64,WatchdogError> enabled = std::watchdog::enable(0);
    assert(enabled.isOk());

    /* 时间维度默认关闭：没 enableTimeout*() 过时 kick() 报 NotEnabled，非正超时被拒。 */
    Result<bool,WatchdogError> noTimer = std::watchdog::kick();
    assert(noTimer.isErr());
    Result<int64,WatchdogError> badTimeout = std::watchdog::enableTimeout(0);
    assert(badTimeout.isErr());

    /* 打开时间维度：超时给得足够长，本程序跑完之前不会触发；喂一口证明 kick() 生效。 */
    Result<int64,WatchdogError> timer = std::watchdog::enableTimeout(1000000000);
    assert(timer.isOk());
    Result<bool,WatchdogError> fed = std::watchdog::kick();
    assert(fed.isOk());

    println("watchdog API ok");
    return 0;
}
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-525"></a>`enum WatchdogError` | VM 安全点直接检查超时，不依赖事件或用户回调。无宿主时钟时单位为逻辑指令。 |
| WatchdogError | `NotEnabled, InvalidWatermark, InvalidTimeout` | VM 安全点直接检查超时，不依赖事件或用户回调。无宿主时钟时单位为逻辑指令。 变体及载荷见声明。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [enable](#api-0) | `Result<int64, WatchdogError> enable(int64 watermarkBytes)` |
| [requestRestart](#api-1) | `Result<int64, WatchdogError> requestRestart()` |
| [enableTimeout](#api-2) | `Result<int64, WatchdogError> enableTimeout(int64 units)` |
| [enableTimeoutMillis](#api-3) | `Result<int64, WatchdogError> enableTimeoutMillis(int64 millis)` |
| [kick](#api-4) | `Result<bool, WatchdogError> kick()` |
| [disableTimeout](#api-5) | `void disableTimeout()` |

<a id="api-0"></a>

### enable（1）

**声明**

```magel
Result<int64, WatchdogError> enable(int64 watermarkBytes);
```

**行为**

启用软重启看门狗并设置水位。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `watermarkBytes` | `int64 watermarkBytes` | 内存水位字节数，0 不按水位判断 | 无 |

**返回值**

`Result<int64, WatchdogError>`。启用软重启看门狗并设置水位。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::enable(watermarkBytes);
```

**源码**：[source/std/watchdog.mgl:9](../../source/std/watchdog.mgl#L9)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-1"></a>

### requestRestart（2）

**声明**

```magel
Result<int64, WatchdogError> requestRestart();
```

**行为**

请求宿主在安全点结束本轮并处理软重启。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<int64, WatchdogError>`。请求宿主在安全点结束本轮并处理软重启。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::requestRestart();
```

**源码**：[source/std/watchdog.mgl:14](../../source/std/watchdog.mgl#L14)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-2"></a>

### enableTimeout（3）

**声明**

```magel
Result<int64, WatchdogError> enableTimeout(int64 units);
```

**行为**

启用当前 VM 的内部超时检查，单位取决于宿主时钟配置；不注册事件回调。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `units` | `int64 units` | 看门狗超时单位 | 无 |

**返回值**

`Result<int64, WatchdogError>`。启用当前 VM 的内部超时检查，单位取决于宿主时钟配置；不注册事件回调。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::enableTimeout(units);
```

**源码**：[source/std/watchdog.mgl:18](../../source/std/watchdog.mgl#L18)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-3"></a>

### enableTimeoutMillis（4）

**声明**

```magel
Result<int64, WatchdogError> enableTimeoutMillis(int64 millis);
```

**行为**

将毫秒转换为纳秒超时单位，拒绝非正值和溢出。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`Result<int64, WatchdogError>`。将毫秒转换为纳秒超时单位，拒绝非正值和溢出。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::enableTimeoutMillis(millis);
```

**源码**：[source/std/watchdog.mgl:24](../../source/std/watchdog.mgl#L24)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-4"></a>

### kick（5）

**声明**

```magel
Result<bool, WatchdogError> kick();
```

**行为**

推迟当前 VM 看门狗截止时间，未启用超时返回 NotEnabled。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<bool, WatchdogError>`。推迟当前 VM 看门狗截止时间，未启用超时返回 NotEnabled。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::kick();
```

**源码**：[source/std/watchdog.mgl:28](../../source/std/watchdog.mgl#L28)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-5"></a>

### disableTimeout（6）

**声明**

```magel
void disableTimeout();
```

**行为**

停止当前 VM 内部看门狗超时，保留内存水位和显式重启能力。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::disableTimeout();
```

**源码**：[source/std/watchdog.mgl:32](../../source/std/watchdog.mgl#L32)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。
