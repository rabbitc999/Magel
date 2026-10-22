# std::serialization — 反射字节序列化

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::serialization;
```

## 公共约定与错误边界

serialize<T> 使用反射生成精确 JSON 载荷，并封装版本、稳定类型 ID 和长度；deserialize<T> 严格检查后恢复独立拥有的值。编译器裁剪反射时 panic，模型未暴露时返回错误。

## 参考代码

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [serialize](#api-0) | `Result<List<uint8>,String> serialize<T>(T const& value)` |
| [deserialize](#api-1) | `Result<T,String> deserialize<T>(List<uint8> bytes)` |

<a id="api-0"></a>

### serialize（1）

**声明**

```chali
Result<List<uint8>,String> serialize<T>(T const& value);
```

**行为**

按具体反射类型编码独立值，失败返回诊断。

将反射值编码为带稳定类型标识和版本的独立字节流。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T const& value` | 输入值或写入的新值 | 无 |

**返回值**

`Result<List<uint8>,String>`。按具体反射类型编码独立值，失败返回诊断。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::serialization::serialize<T>(value);
```

**源码**：[source/std/serialization.chl:10](../../source/std/serialization.chl#L10)。

<a id="api-1"></a>

### deserialize（2）

**声明**

```chali
Result<T,String> deserialize<T>(List<uint8> bytes);
```

**行为**

按具体反射类型严格校验并恢复独立拥有的值。

严格验证类型、版本和长度后恢复独立拥有的值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `List<uint8> bytes` | 输入或输出字节存储 | 无 |

**返回值**

`Result<T,String>`。按具体反射类型严格校验并恢复独立拥有的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::serialization::deserialize<T>(bytes);
```

**源码**：[source/std/serialization.chl:16](../../source/std/serialization.chl#L16)。
