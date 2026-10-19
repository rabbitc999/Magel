# std::net — TCP 与 HTTP

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::net;
```

## 公共约定与错误边界

TCP 返回句柄包装值，先检查 isOpen/lastError。receive/receiveLine 的空串需结合 status 区分关闭、超时和错误。端口 0 可让系统分配监听端口。HTTP 仅支持 http://，不支持 TLS；HTTP 状态码与传输错误分别检查。连接、监听器及客户端需关闭。

## 参考代码

完整程序：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

```powershell
magel run example/samples/std_net.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
/* std_net.mgl —— `std::net`（TCP + 极简 HTTP 客户端）用法样例。
 *
 *   ./magel samples/std_net.mgl
 *
 * 全程不需要外网、不需要第二个进程：
 *   1. 先在本进程里 listen（端口给 0 让系统分配）→ connect → accept，双向收发；
 *   2. 再单独测三个纯函数：地址解析、请求拼装、响应解析。
 *
 * 因此输出是确定的（端口号不打印）。套接字实现见 `glue/std_net.hpp`
 * （Windows 运行期加载 ws2_32.dll，POSIX 直接调用同名接口）。
 */

import std::net;

int64 main() {
    /* ---- 1. 回环 TCP：监听 / 连接 / 接受 / 收发 ---- */
    std::net::TcpListener listener = std::net::TcpListener::listen("127.0.0.1", (int32)0, (int32)8);
    println("listener open={} port>0={}", listener.isOpen(), listener.localPort() > 0);
    int64 port = listener.localPort();

    std::net::TcpSocket client = std::net::TcpSocket::connect("127.0.0.1", (int32)port, (int32)2000);
    std::net::TcpSocket peer = listener.accept((int32)2000);
    println("client open={} peer open={}", client.isOpen(), peer.isOpen());

    client.send("ping\n");
    String request = peer.receiveLine((int32)2000);
    println("server got [{}] status={}", request, peer.status());

    peer.send("pong\n");
    String reply = client.receiveLine((int32)2000);
    println("client got [{}] status={}", reply, client.status());

    peer.close();
    client.close();
    listener.close();
    println("closed client={} peer={} listener={}", client.isOpen(), peer.isOpen(), listener.isOpen());

    /* ---- 2. 纯函数：地址解析 / 请求拼装 / 响应解析 ---- */
    std::net::Url url = std::net::parseUrl("http://example.com:8080/a/b?c=1");
    println("url ok={} host={} port={} path={}", url.ok, url.host, url.port, url.path);
    std::net::Url bad = std::net::parseUrl("https://example.com/");
    println("https rejected={} error=[{}]", !bad.ok, bad.error);

    String get = std::net::buildRequest("GET", "http://example.com/x", "", "");
    println("GET request len={}", len(get));

    String raw = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Type: text/plain\r\n\r\nhello";
    std::net::HttpResponse response = std::net::parseResponse(raw);
    println("response ok={} status={} reason={} len={} type={} body={}",
    response.ok, response.status, response.reason,
    response.header("Content-Length"), response.header("content-type"), response.body);
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
listener open=1 port>0=1
client open=1 peer open=1
server got [ping] status=0
client got [pong] status=0
closed client=0 peer=0 listener=0
url ok=1 host=example.com port=8080 path=/a/b?c=1
https rejected=1 error=[只支持 http:// 开头的地址]
GET request len=101
response ok=1 status=200 reason=OK len=5 type=text/plain body=hello
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-339"></a>`struct TcpSocket` | 一条 TCP 连接（`connect` 来的，或 `accept` 到的）。 |
| TcpSocket | <a id="symbol-340"></a>`int32 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |
| TcpSocket | <a id="symbol-341"></a>`bool opened` | 外部资源是否处于打开状态。 |
| TcpSocket | <a id="symbol-342"></a>`std::lang::String error` | 最近操作的错误说明。 |
| 模块 | <a id="symbol-343"></a>`struct TcpListener` | TCP 监听器；端口传 0 时由系统分配，用 `localPort()` 查询。 |
| TcpListener | <a id="symbol-344"></a>`int32 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |
| TcpListener | <a id="symbol-345"></a>`bool opened` | 外部资源是否处于打开状态。 |
| TcpListener | <a id="symbol-346"></a>`std::lang::String error` | 最近操作的错误说明。 |
| 模块 | <a id="symbol-347"></a>`struct Url` | 拆开的 HTTP 地址。 |
| Url | <a id="symbol-348"></a>`bool ok` | 当前解析或请求是否成功。 |
| Url | <a id="symbol-349"></a>`std::lang::String host` | URL 中的主机名。 |
| Url | <a id="symbol-350"></a>`int64 port` | 网络端口。 |
| Url | <a id="symbol-351"></a>`std::lang::String path` | 请求目标路径。 |
| Url | <a id="symbol-352"></a>`std::lang::String error` | 最近操作的错误说明。 |
| 模块 | <a id="symbol-353"></a>`struct HttpResponse` | HTTP 响应：状态码、原因、头部原文、正文。 |
| HttpResponse | <a id="symbol-354"></a>`bool ok` | 当前解析或请求是否成功。 |
| HttpResponse | <a id="symbol-355"></a>`int64 status` | HTTP 响应状态码。 |
| HttpResponse | <a id="symbol-356"></a>`std::lang::String reason` | HTTP 状态原因文本。 |
| HttpResponse | <a id="symbol-357"></a>`std::lang::String headers` | HTTP 响应头。 |
| HttpResponse | <a id="symbol-358"></a>`std::lang::String body` | HTTP 响应正文。 |
| HttpResponse | <a id="symbol-359"></a>`std::lang::String error` | 最近操作的错误说明。 |
| 模块 | <a id="symbol-360"></a>`struct HttpClient` | 一条连接上的 HTTP 客户端；也提供一次性便捷方法 `fetch`/`submit`。 |
| HttpClient | <a id="symbol-361"></a>`TcpSocket socket` | HTTP 客户端使用的 TCP 连接。 |
| HttpClient | <a id="symbol-362"></a>`bool connected` | 客户端是否已建立连接。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [TcpSocket::connect](#api-0) | `static TcpSocket connect(std::lang::String host, int32 port, int32 timeoutMs)` |
| [TcpSocket::connect](#api-1) | `static TcpSocket connect(std::lang::String host, int32 port)` |
| [TcpSocket::isOpen](#api-2) | `bool isOpen()` |
| [TcpSocket::lastError](#api-3) | `std::lang::String lastError()` |
| [TcpSocket::status](#api-4) | `int64 status()` |
| [TcpSocket::close](#api-5) | `void close()` |
| [TcpSocket::send](#api-6) | `int64 send(std::lang::String data)` |
| [TcpSocket::receive](#api-7) | `std::lang::String receive(int32 maxBytes)` |
| [TcpSocket::receiveLine](#api-8) | `std::lang::String receiveLine(int32 timeoutMs)` |
| [TcpListener::listen](#api-9) | `static TcpListener listen(std::lang::String host, int32 port, int32 backlog)` |
| [TcpListener::listen](#api-10) | `static TcpListener listen(std::lang::String host, int32 port)` |
| [TcpListener::isOpen](#api-11) | `bool isOpen()` |
| [TcpListener::lastError](#api-12) | `std::lang::String lastError()` |
| [TcpListener::localPort](#api-13) | `int64 localPort()` |
| [TcpListener::accept](#api-14) | `TcpSocket accept(int32 timeoutMs)` |
| [TcpListener::accept](#api-15) | `TcpSocket accept()` |
| [TcpListener::close](#api-16) | `void close()` |
| [HttpResponse::isSuccess](#api-17) | `bool isSuccess()` |
| [HttpResponse::header](#api-18) | `std::lang::String header(std::lang::String name)` |
| [httpScheme](#api-19) | `std::lang::String httpScheme()` |
| [parseUrl](#api-20) | `Url parseUrl(std::lang::String url)` |
| [buildRequest](#api-21) | `std::lang::String buildRequest(std::lang::String method, std::lang::String url, std::lang::String body, std::lang::String contentType)` |
| [parseResponse](#api-22) | `HttpResponse parseResponse(std::lang::String raw)` |
| [HttpClient::connect](#api-23) | `bool connect(std::lang::String host, int32 port, int32 timeoutMs)` |
| [HttpClient::readAll](#api-24) | `std::lang::String readAll()` |
| [HttpClient::exchange](#api-25) | `HttpResponse exchange(std::lang::String method, std::lang::String url, std::lang::String body, std::lang::String contentType)` |
| [HttpClient::close](#api-26) | `void close()` |
| [HttpClient::fetch](#api-27) | `static HttpResponse fetch(std::lang::String url)` |
| [HttpClient::submit](#api-28) | `static HttpResponse submit(std::lang::String url, std::lang::String body, std::lang::String contentType)` |

<a id="api-0"></a>

### TcpSocket::connect（1）

**声明**

```magel
static TcpSocket connect(std::lang::String host, int32 port, int32 timeoutMs);
```

**行为**

连接远端 TCP/HTTP 服务。

连接远端；`timeoutMs > 0` 时带连接超时。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `std::lang::String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |
| `timeoutMs` | `int32 timeoutMs` | 超时时间，毫秒 | 无 |

**返回值**

`TcpSocket`。连接远端 TCP/HTTP 服务。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::TcpSocket::connect(host, port, timeoutMs);
```

**源码**：[source/std/net.mgl:58](../../source/std/net.mgl#L58)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-1"></a>

### TcpSocket::connect（2）

**声明**

```magel
static TcpSocket connect(std::lang::String host, int32 port);
```

**行为**

连接远端 TCP/HTTP 服务。

默认 5 秒连接超时。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `std::lang::String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |

**返回值**

`TcpSocket`。连接远端 TCP/HTTP 服务。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::TcpSocket::connect(host, port);
```

**源码**：[source/std/net.mgl:68](../../source/std/net.mgl#L68)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-2"></a>

### TcpSocket::isOpen（3）

**声明**

```magel
bool isOpen();
```

**行为**

判断连接或监听器是否已打开。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断连接或监听器是否已打开。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.isOpen();
```

**源码**：[source/std/net.mgl:72](../../source/std/net.mgl#L72)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-3"></a>

### TcpSocket::lastError（4）

**声明**

```magel
std::lang::String lastError();
```

**行为**

返回连接/网络操作最近的错误文本。

最近一次出错说明。 直接返回字段即可：String 是值语义，`return self.error;` 交出的是独立副本， 接收者释放后调用方手上的串仍然有效。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。返回连接/网络操作最近的错误文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.lastError();
```

**源码**：[source/std/net.mgl:80](../../source/std/net.mgl#L80)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-4"></a>

### TcpSocket::status（5）

**声明**

```magel
int64 status();
```

**行为**

返回最近一次网络读取状态码。

最近一次读的结果：NET_OK / NET_CLOSED / NET_TIMEOUT / NET_ERROR。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回最近一次网络读取状态码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.status();
```

**源码**：[source/std/net.mgl:83](../../source/std/net.mgl#L83)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-5"></a>

### TcpSocket::close（6）

**声明**

```magel
void close();
```

**行为**

关闭底层资源；文档节点会使同文档所有视图失效。

关闭连接；重复调用安全。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.close();
```

**源码**：[source/std/net.mgl:86](../../source/std/net.mgl#L86)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-6"></a>

### TcpSocket::send（7）

**声明**

```magel
int64 send(std::lang::String data);
```

**行为**

发送 TCP 文本并返回发送结果。

发送全部文本；返回发送的字节数，失败 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `std::lang::String data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |

**返回值**

`int64`。发送 TCP 文本并返回发送结果。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.send(data);
```

**源码**：[source/std/net.mgl:95](../../source/std/net.mgl#L95)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-7"></a>

### TcpSocket::receive（8）

**声明**

```magel
std::lang::String receive(int32 maxBytes);
```

**行为**

读取至多 maxBytes 字节；空串结合 status 判断原因。

收一段文本（最多 maxBytes 字节）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `maxBytes` | `int32 maxBytes` | 最多接收字节数 | 无 |

**返回值**

`std::lang::String`。读取至多 maxBytes 字节；空串结合 status 判断原因。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receive(maxBytes);
```

**源码**：[source/std/net.mgl:109](../../source/std/net.mgl#L109)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-8"></a>

### TcpSocket::receiveLine（9）

**声明**

```magel
std::lang::String receiveLine(int32 timeoutMs);
```

**行为**

读取一行网络文本；空串时检查 status。

收一行（不含行尾换行）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `timeoutMs` | `int32 timeoutMs` | 超时时间，毫秒 | 无 |

**返回值**

`std::lang::String`。读取一行网络文本；空串时检查 status。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receiveLine(timeoutMs);
```

**源码**：[source/std/net.mgl:116](../../source/std/net.mgl#L116)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-9"></a>

### TcpListener::listen（10）

**声明**

```magel
static TcpListener listen(std::lang::String host, int32 port, int32 backlog);
```

**行为**

绑定本机地址并开始监听，返回监听器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `std::lang::String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |
| `backlog` | `int32 backlog` | 监听待接受队列长度 | 无 |

**返回值**

`TcpListener`。绑定本机地址并开始监听，返回监听器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::TcpListener::listen(host, port, backlog);
```

**源码**：[source/std/net.mgl:129](../../source/std/net.mgl#L129)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-10"></a>

### TcpListener::listen（11）

**声明**

```magel
static TcpListener listen(std::lang::String host, int32 port);
```

**行为**

绑定本机地址并开始监听，返回监听器。

默认积压队列 8。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `std::lang::String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |

**返回值**

`TcpListener`。绑定本机地址并开始监听，返回监听器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::TcpListener::listen(host, port);
```

**源码**：[source/std/net.mgl:139](../../source/std/net.mgl#L139)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-11"></a>

### TcpListener::isOpen（12）

**声明**

```magel
bool isOpen();
```

**行为**

判断连接或监听器是否已打开。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断连接或监听器是否已打开。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.isOpen();
```

**源码**：[source/std/net.mgl:143](../../source/std/net.mgl#L143)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-12"></a>

### TcpListener::lastError（13）

**声明**

```magel
std::lang::String lastError();
```

**行为**

返回连接/网络操作最近的错误文本。

最近一次出错说明；String 为值语义，返回的是独立副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。返回连接/网络操作最近的错误文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.lastError();
```

**源码**：[source/std/net.mgl:146](../../source/std/net.mgl#L146)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-13"></a>

### TcpListener::localPort（14）

**声明**

```magel
int64 localPort();
```

**行为**

返回实际绑定的本地端口。

本端端口（监听器常用端口 0 让系统分配，再从这里读回来）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回实际绑定的本地端口。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.localPort();
```

**源码**：[source/std/net.mgl:149](../../source/std/net.mgl#L149)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-14"></a>

### TcpListener::accept（15）

**声明**

```magel
TcpSocket accept(int32 timeoutMs);
```

**行为**

接受一条 TCP 连接，可指定等待超时。

等一个连接；超时返回未打开的套接字。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `timeoutMs` | `int32 timeoutMs` | 超时时间，毫秒 | 无 |

**返回值**

`TcpSocket`。接受一条 TCP 连接，可指定等待超时。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.accept(timeoutMs);
```

**源码**：[source/std/net.mgl:152](../../source/std/net.mgl#L152)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-15"></a>

### TcpListener::accept（16）

**声明**

```magel
TcpSocket accept();
```

**行为**

接受一条 TCP 连接，可指定等待超时。

默认 5 秒等待。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TcpSocket`。接受一条 TCP 连接，可指定等待超时。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.accept();
```

**源码**：[source/std/net.mgl:162](../../source/std/net.mgl#L162)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-16"></a>

### TcpListener::close（17）

**声明**

```magel
void close();
```

**行为**

关闭底层资源；文档节点会使同文档所有视图失效。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.close();
```

**源码**：[source/std/net.mgl:164](../../source/std/net.mgl#L164)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-17"></a>

### HttpResponse::isSuccess（18）

**声明**

```magel
bool isSuccess();
```

**行为**

判断 HTTP 状态码是否位于 200..299。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 HTTP 状态码是否位于 200..299。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.isSuccess();
```

**源码**：[source/std/net.mgl:191](../../source/std/net.mgl#L191)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-18"></a>

### HttpResponse::header（19）

**声明**

```magel
std::lang::String header(std::lang::String name);
```

**行为**

按名称查找响应头。

按名字取响应头（不分大小写），没有给空串。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |

**返回值**

`std::lang::String`。按名称查找响应头。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.header(name);
```

**源码**：[source/std/net.mgl:194](../../source/std/net.mgl#L194)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-19"></a>

### httpScheme（20）

**声明**

```magel
std::lang::String httpScheme();
```

**行为**

返回 URL 的协议部分。

只支持 http:// 的极简地址解析。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。返回 URL 的协议部分。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::httpScheme();
```

**源码**：[source/std/net.mgl:221](../../source/std/net.mgl#L221)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-20"></a>

### parseUrl（21）

**声明**

```magel
Url parseUrl(std::lang::String url);
```

**行为**

拆分 HTTP URL 的主机、端口和请求路径。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `url` | `std::lang::String url` | HTTP URL，仅支持 http:// | 无 |

**返回值**

`Url`。拆分 HTTP URL 的主机、端口和请求路径。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::parseUrl(url);
```

**源码**：[source/std/net.mgl:223](../../source/std/net.mgl#L223)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-21"></a>

### buildRequest（22）

**声明**

```magel
std::lang::String buildRequest(std::lang::String method, std::lang::String url, std::lang::String body, std::lang::String contentType);
```

**行为**

构造 HTTP 请求报文。

拼一条 HTTP/1.1 请求文本；body 为空时按 GET 处理。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `method` | `std::lang::String method` | HTTP 方法 | 无 |
| `url` | `std::lang::String url` | HTTP URL，仅支持 http:// | 无 |
| `body` | `std::lang::String body` | HTTP 正文 | 无 |
| `contentType` | `std::lang::String contentType` | HTTP 正文媒体类型 | 无 |

**返回值**

`std::lang::String`。构造 HTTP 请求报文。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::buildRequest(method, url, body, contentType);
```

**源码**：[source/std/net.mgl:263](../../source/std/net.mgl#L263)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-22"></a>

### parseResponse（23）

**声明**

```magel
HttpResponse parseResponse(std::lang::String raw);
```

**行为**

解析原始 HTTP 响应报文。

\r\n"; request = request + "Connection: close\r\n"; if (std::lang::len(body) > 0) { std::lang::String mediaType = ""; mediaType = mediaType + contentType; if (std::lang::len(mediaType) == 0) { mediaType = "text/plain; charset=utf-8"; } std::lang::String typeLine = "Content-Type: " + mediaType + "\r\n"; request = request + typeLine; std::lang::String lengthLine = std::lang::format("Content-Length: {}\r\n", std::lang::len(body)); request = request + lengthLine; } request = request + "\r\n"; request = request + body; return request; } /** 解析一段完整的 HTTP 响应文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `raw` | `std::lang::String raw` | 原始报文文本 | 无 |

**返回值**

`HttpResponse`。解析原始 HTTP 响应报文。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::parseResponse(raw);
```

**源码**：[source/std/net.mgl:289](../../source/std/net.mgl#L289)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-23"></a>

### HttpClient::connect（24）

**声明**

```magel
bool connect(std::lang::String host, int32 port, int32 timeoutMs);
```

**行为**

连接远端 TCP/HTTP 服务。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `std::lang::String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |
| `timeoutMs` | `int32 timeoutMs` | 超时时间，毫秒 | 无 |

**返回值**

`bool`。连接远端 TCP/HTTP 服务。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.connect(host, port, timeoutMs);
```

**源码**：[source/std/net.mgl:337](../../source/std/net.mgl#L337)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-24"></a>

### HttpClient::readAll（25）

**声明**

```magel
std::lang::String readAll();
```

**行为**

持续读取 HTTP 响应原始文本。

读到对端关闭或超时为止（`Connection: close` 时正好是一整个响应）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。持续读取 HTTP 响应原始文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readAll();
```

**源码**：[source/std/net.mgl:344](../../source/std/net.mgl#L344)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-25"></a>

### HttpClient::exchange（26）

**声明**

```magel
HttpResponse exchange(std::lang::String method, std::lang::String url, std::lang::String body, std::lang::String contentType);
```

**行为**

发送构造后的 HTTP 请求并解析响应。

在已连上的连接上发请求并读回响应。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `method` | `std::lang::String method` | HTTP 方法 | 无 |
| `url` | `std::lang::String url` | HTTP URL，仅支持 http:// | 无 |
| `body` | `std::lang::String body` | HTTP 正文 | 无 |
| `contentType` | `std::lang::String contentType` | HTTP 正文媒体类型 | 无 |

**返回值**

`HttpResponse`。发送构造后的 HTTP 请求并解析响应。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.exchange(method, url, body, contentType);
```

**源码**：[source/std/net.mgl:356](../../source/std/net.mgl#L356)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-26"></a>

### HttpClient::close（27）

**声明**

```magel
void close();
```

**行为**

关闭底层资源；文档节点会使同文档所有视图失效。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.close();
```

**源码**：[source/std/net.mgl:378](../../source/std/net.mgl#L378)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-27"></a>

### HttpClient::fetch（28）

**声明**

```magel
static HttpResponse fetch(std::lang::String url);
```

**行为**

发出 HTTP GET 并返回响应。

一次性 GET：连上、发请求、读响应、关闭。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `url` | `std::lang::String url` | HTTP URL，仅支持 http:// | 无 |

**返回值**

`HttpResponse`。发出 HTTP GET 并返回响应。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::HttpClient::fetch(url);
```

**源码**：[source/std/net.mgl:384](../../source/std/net.mgl#L384)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-28"></a>

### HttpClient::submit（29）

**声明**

```magel
static HttpResponse submit(std::lang::String url, std::lang::String body, std::lang::String contentType);
```

**行为**

提交 HTTP 请求体并返回响应。

一次性 POST。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `url` | `std::lang::String url` | HTTP URL，仅支持 http:// | 无 |
| `body` | `std::lang::String body` | HTTP 正文 | 无 |
| `contentType` | `std::lang::String contentType` | HTTP 正文媒体类型 | 无 |

**返回值**

`HttpResponse`。提交 HTTP 请求体并返回响应。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::HttpClient::submit(url, body, contentType);
```

**源码**：[source/std/net.mgl:409](../../source/std/net.mgl#L409)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。
