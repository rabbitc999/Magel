# std::watchdog — 软重启看门狗

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::watchdog;
```

## 公共约定与错误边界

默认关闭。enable 水位单位字节，0 只保留显式请求/分配失败路径，负水位返回 Err(InvalidWatermark)。requestRestart 在安全点结束当前轮，由宿主重置 arena、清空墓碑并重跑；watchdog 重启在清除墓碑的效果上等价于一次完整的 std::lang::sweep()，同时会结束全部旧执行上下文；未启用返回 Err(NotEnabled)。时间维度同样默认关闭：enableTimeout* 之后整整一个超时周期没有 kick() 就转成显式重启请求，非正超时返回 Err(InvalidTimeout)。不是硬件看门狗，也不承诺宿主一定重启。

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
| 模块 | <a id="symbol-507"></a>`enum WatchdogError` | 看门狗错误。 |
| WatchdogError | `NotEnabled, InvalidWatermark, InvalidTimeout` | 看门狗错误。 变体及载荷见声明。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [enable](#api-0) | `Result<int64, WatchdogError> enable(int64 watermarkBytes)` |
| [requestRestart](#api-1) | `Result<bool, WatchdogError> requestRestart()` |
| [enableTimeout](#api-2) | `Result<int64, WatchdogError> enableTimeout(int64 timeoutUnits)` |
| [enableTimeoutMillis](#api-3) | `Result<int64, WatchdogError> enableTimeoutMillis(int64 millis)` |
| [kick](#api-4) | `Result<bool, WatchdogError> kick()` |

<a id="api-0"></a>

### enable（1）

**声明**

```magel
Result<int64, WatchdogError> enable(int64 watermarkBytes);
```

**行为**

启用软重启看门狗并设置水位。

打开看门狗并给出内存水位（字节）。`watermarkBytes = 0` 表示"不按水位判"， 只等显式请求 / 分配失败。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `watermarkBytes` | `int64 watermarkBytes` | 水位。 | 无 |

**返回值**

`Result<int64, WatchdogError>`。`Ok(watermarkBytes)`；水位为负返回 `Err(InvalidWatermark)`。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::enable(watermarkBytes);
```

**源码**：[source/std/watchdog.mgl:80](../../source/std/watchdog.mgl#L80)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-1"></a>

### requestRestart（2）

**声明**

```magel
Result<bool, WatchdogError> requestRestart();
```

**行为**

请求宿主在安全点结束本轮并处理软重启。

显式请求一次软重启：写"标志 + 原因"，VM 在下一条指令边界停下本轮，宿主重置 arena 后重跑。 没 `enable()` / `enableTimeout*()` 过返回 `Err(NotEnabled)`，**不产生重启**。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<bool, WatchdogError>`。`Ok(true)` = 请求已记下（本轮马上会在安全点结束）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::requestRestart();
```

**源码**：[source/std/watchdog.mgl:96](../../source/std/watchdog.mgl#L96)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-2"></a>

### enableTimeout（3）

**声明**

```magel
Result<int64, WatchdogError> enableTimeout(int64 timeoutUnits);
```

**行为**

打开 ③ 时间维度：**整整 `timeoutUnits` 个时间单位没有 `kick()`，就请求一次软重启**。 单位与 VM 时钟一致（与 `std::event::Timer` 的周期同一个口径：宿主注入过纳秒时钟时是 纳秒，否则 1 条指令 = 1 个时间单位）。 本函数同时把看门狗本体打开（等价于 `enable(0)`，即不按水位判）；**已经 `enable()` 过 就不动原来的水位**。重复调用按最后一次的超时换掉定时器，并重新开始计时。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `timeoutUnits` | `int64 timeoutUnits` | 超时（时间单位，必须为正）。 | 无 |

**返回值**

`Result<int64, WatchdogError>`。`Ok(timeoutUnits)`；不为正返回 `Err(InvalidTimeout)`。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::enableTimeout(timeoutUnits);
```

**源码**：[source/std/watchdog.mgl:130](../../source/std/watchdog.mgl#L130)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-3"></a>

### enableTimeoutMillis（4）

**声明**

```magel
Result<int64, WatchdogError> enableTimeoutMillis(int64 millis);
```

**行为**

毫秒版：`millis * 1000000` 个时间单位（与 `std::event::Timer::everyMillis` 同一个换算）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 超时（毫秒，必须为正）。 | 无 |

**返回值**

`Result<int64, WatchdogError>`。同 `enableTimeout`。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::enableTimeoutMillis(millis);
```

**源码**：[source/std/watchdog.mgl:157](../../source/std/watchdog.mgl#L157)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。

<a id="api-4"></a>

### kick（5）

**声明**

```magel
Result<bool, WatchdogError> kick();
```

**行为**

喂一口狗：告诉时间维度"这一轮还活着"。每个超时周期之内至少调一次。 没 `enableTimeout*()` 过返回 `Err(NotEnabled)`（没开这一维时不必喂）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<bool, WatchdogError>`。`Ok(true)` = 记下了。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::watchdog::kick();
```

**源码**：[source/std/watchdog.mgl:169](../../source/std/watchdog.mgl#L169)。

**完整示例**：[doc/examples/library/watchdog.mgl](../../doc/examples/library/watchdog.mgl)。
