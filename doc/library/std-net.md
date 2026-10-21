# std::net — TCP 与 HTTP

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::net;
```

## 公共约定与错误边界

TcpSocket、TcpListener 是不可复制的 opcode，宿主记录独占连接，作用域退出自动关闭；所有权通过 move 转移，先检查 isOpen/lastError。receive/receiveLine 的空串需结合 status 区分关闭、超时和错误。端口 0 可让系统分配监听端口。HTTP 仅支持 http://，不支持 TLS；HTTP 状态码与传输错误分别检查。close 可提前关闭，重复调用安全；每条连接独立保存最近读状态与错误。connectAsync/pollConnect、acceptAsync、sendAsync、receiveAsync、receiveLineAsync 采用非阻塞轮询，未就绪返回 NET_PENDING；调用方可用 std::thread::sleepFor 有界暂停后重试。connectAsync 只接受数值 IPv4，默认五秒超时，域名使用同步 connect；sendAsync 返回本次部分发送字节数，调用方推进 offset；异步行缓冲上限 1 MiB，待完成数据保留至下次轮询。

## 参考代码

完整程序：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

```powershell
magel run example/samples/std_net.mgl
```

非阻塞与可取消线程配合的完整程序：[std_net_async.mgl](../../example/samples/std_net_async.mgl)。

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
| 模块 | <a id="symbol-316"></a>`#NonCopyable opcode TcpSocket` | 独占 TCP 连接；复制被拒绝，显式 move 转移所有权。 |
| 模块 | <a id="symbol-317"></a>`#NonCopyable opcode TcpListener` | 独占 TCP 监听器；端口为零时由系统分配。 |
| 模块 | <a id="symbol-318"></a>`struct Url` | 拆开的 HTTP 地址。 |
| Url | <a id="symbol-319"></a>`bool ok` | 当前解析或请求是否成功。 |
| Url | <a id="symbol-320"></a>`std::lang::String host` | URL 中的主机名。 |
| Url | <a id="symbol-321"></a>`int64 port` | 网络端口。 |
| Url | <a id="symbol-322"></a>`std::lang::String path` | 请求目标路径。 |
| Url | <a id="symbol-323"></a>`std::lang::String error` | 最近操作的错误说明。 |
| 模块 | <a id="symbol-324"></a>`struct HttpResponse` | HTTP 响应：状态码、原因、头部原文、正文。 |
| HttpResponse | <a id="symbol-325"></a>`bool ok` | 当前解析或请求是否成功。 |
| HttpResponse | <a id="symbol-326"></a>`int64 status` | HTTP 响应状态码。 |
| HttpResponse | <a id="symbol-327"></a>`std::lang::String reason` | HTTP 状态原因文本。 |
| HttpResponse | <a id="symbol-328"></a>`std::lang::String headers` | HTTP 响应头。 |
| HttpResponse | <a id="symbol-329"></a>`std::lang::String body` | HTTP 响应正文。 |
| HttpResponse | <a id="symbol-330"></a>`std::lang::String error` | 最近操作的错误说明。 |
| 模块 | <a id="symbol-331"></a>`struct HttpClient` | 一条连接上的 HTTP 客户端；也提供一次性便捷方法 `fetch`/`submit`。 |
| HttpClient | <a id="symbol-332"></a>`TcpSocket socket` | HTTP 客户端使用的 TCP 连接。 |
| HttpClient | <a id="symbol-333"></a>`bool connected` | 客户端是否已建立连接。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [TcpSocket::connect](#api-0) | `static TcpSocket connect(String host, int32 port, int32 timeoutMs)` |
| [TcpSocket::connect](#api-1) | `static TcpSocket connect(String host, int32 port)` |
| [TcpSocket::connectAsync](#api-2) | `static TcpSocket connectAsync(String host, int32 port, int32 timeoutMs)` |
| [TcpSocket::connectAsync](#api-3) | `static TcpSocket connectAsync(String host, int32 port)` |
| [TcpSocket::pollConnect](#api-4) | `int64 pollConnect()` |
| [TcpSocket::sendAsync](#api-5) | `int64 sendAsync(String data, int64 offset)` |
| [TcpSocket::sendAsync](#api-6) | `int64 sendAsync(String data)` |
| [TcpSocket::receiveAsync](#api-7) | `String receiveAsync(int32 maxBytes)` |
| [TcpSocket::receiveLineAsync](#api-8) | `String receiveLineAsync()` |
| [TcpSocket::isOpen](#api-9) | `bool isOpen()` |
| [TcpSocket::lastError](#api-10) | `String lastError()` |
| [TcpSocket::status](#api-11) | `int64 status()` |
| [TcpSocket::close](#api-12) | `void close()` |
| [TcpSocket::send](#api-13) | `int64 send(String data)` |
| [TcpSocket::receive](#api-14) | `String receive(int32 maxBytes)` |
| [TcpSocket::receiveLine](#api-15) | `String receiveLine(int32 timeoutMs)` |
| [TcpListener::listen](#api-16) | `static TcpListener listen(String host, int32 port, int32 backlog)` |
| [TcpListener::listen](#api-17) | `static TcpListener listen(String host, int32 port)` |
| [TcpListener::isOpen](#api-18) | `bool isOpen()` |
| [TcpListener::lastError](#api-19) | `String lastError()` |
| [TcpListener::localPort](#api-20) | `int64 localPort()` |
| [TcpListener::accept](#api-21) | `TcpSocket accept(int32 timeoutMs)` |
| [TcpListener::accept](#api-22) | `TcpSocket accept()` |
| [TcpListener::acceptAsync](#api-23) | `TcpSocket acceptAsync()` |
| [TcpListener::close](#api-24) | `void close()` |
| [HttpResponse::isSuccess](#api-25) | `bool isSuccess()` |
| [HttpResponse::header](#api-26) | `std::lang::String header(std::lang::String name)` |
| [httpScheme](#api-27) | `std::lang::String httpScheme()` |
| [parseUrl](#api-28) | `Url parseUrl(std::lang::String url)` |
| [buildRequest](#api-29) | `std::lang::String buildRequest(std::lang::String method, std::lang::String url, std::lang::String body, std::lang::String contentType)` |
| [parseResponse](#api-30) | `HttpResponse parseResponse(std::lang::String raw)` |
| [HttpClient::connect](#api-31) | `bool connect(std::lang::String host, int32 port, int32 timeoutMs)` |
| [HttpClient::readAll](#api-32) | `std::lang::String readAll()` |
| [HttpClient::exchange](#api-33) | `HttpResponse exchange(std::lang::String method, std::lang::String url, std::lang::String body, std::lang::String contentType)` |
| [HttpClient::close](#api-34) | `void close()` |
| [HttpClient::fetch](#api-35) | `static HttpResponse fetch(std::lang::String url)` |
| [HttpClient::submit](#api-36) | `static HttpResponse submit(std::lang::String url, std::lang::String body, std::lang::String contentType)` |

<a id="api-0"></a>

### TcpSocket::connect（1）

**声明**

```magel
static TcpSocket connect(String host, int32 port, int32 timeoutMs);
```

**行为**

连接远端 TCP/HTTP 服务。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `String host` | 主机名或 IP 地址 | 无 |
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

**源码**：[source/std/net.mgl:67](../../source/std/net.mgl#L67)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-1"></a>

### TcpSocket::connect（2）

**声明**

```magel
static TcpSocket connect(String host, int32 port);
```

**行为**

连接远端 TCP/HTTP 服务。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |

**返回值**

`TcpSocket`。连接远端 TCP/HTTP 服务。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::TcpSocket::connect(host, port);
```

**源码**：[source/std/net.mgl:70](../../source/std/net.mgl#L70)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-2"></a>

### TcpSocket::connectAsync（3）

**声明**

```magel
static TcpSocket connectAsync(String host, int32 port, int32 timeoutMs);
```

**行为**

发起数值 IPv4 的非阻塞连接；通过 pollConnect 推进，NET_PENDING 表示进行中。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |
| `timeoutMs` | `int32 timeoutMs` | 超时时间，毫秒 | 无 |

**返回值**

`TcpSocket`。发起数值 IPv4 的非阻塞连接；通过 pollConnect 推进，NET_PENDING 表示进行中。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::TcpSocket::connectAsync(host, port, timeoutMs);
```

**源码**：[source/std/net.mgl:74](../../source/std/net.mgl#L74)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-3"></a>

### TcpSocket::connectAsync（4）

**声明**

```magel
static TcpSocket connectAsync(String host, int32 port);
```

**行为**

发起默认五秒超时的非阻塞数值 IPv4 连接。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |

**返回值**

`TcpSocket`。发起默认五秒超时的非阻塞数值 IPv4 连接。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::TcpSocket::connectAsync(host, port);
```

**源码**：[source/std/net.mgl:78](../../source/std/net.mgl#L78)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-4"></a>

### TcpSocket::pollConnect（5）

**声明**

```magel
int64 pollConnect();
```

**行为**

立即检查连接结果，返回 NET_PENDING、NET_OK、NET_TIMEOUT 或 NET_ERROR。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。立即检查连接结果，返回 NET_PENDING、NET_OK、NET_TIMEOUT 或 NET_ERROR。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pollConnect();
```

**源码**：[source/std/net.mgl:82](../../source/std/net.mgl#L82)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-5"></a>

### TcpSocket::sendAsync（6）

**声明**

```magel
int64 sendAsync(String data, int64 offset);
```

**行为**

非阻塞发送，从 offset 开始；返回本次发送字节数，未就绪返回零并设置 NET_PENDING。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `String data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `offset` | `int64 offset` | 字节偏移或 seek 相对位移 | 无 |

**返回值**

`int64`。非阻塞发送，从 offset 开始；返回本次发送字节数，未就绪返回零并设置 NET_PENDING。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sendAsync(data, offset);
```

**源码**：[source/std/net.mgl:84](../../source/std/net.mgl#L84)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-6"></a>

### TcpSocket::sendAsync（7）

**声明**

```magel
int64 sendAsync(String data);
```

**行为**

从零偏移开始执行一次非阻塞发送；长消息可能只发送一部分。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `String data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |

**返回值**

`int64`。从零偏移开始执行一次非阻塞发送；长消息可能只发送一部分。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sendAsync(data);
```

**源码**：[source/std/net.mgl:86](../../source/std/net.mgl#L86)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-7"></a>

### TcpSocket::receiveAsync（8）

**声明**

```magel
String receiveAsync(int32 maxBytes);
```

**行为**

非阻塞读取至多 maxBytes 字节；空串结合 status 区分未就绪、关闭及错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `maxBytes` | `int32 maxBytes` | 最多接收字节数 | 无 |

**返回值**

`String`。非阻塞读取至多 maxBytes 字节；空串结合 status 区分未就绪、关闭及错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receiveAsync(maxBytes);
```

**源码**：[source/std/net.mgl:88](../../source/std/net.mgl#L88)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-8"></a>

### TcpSocket::receiveLineAsync（9）

**声明**

```magel
String receiveLineAsync();
```

**行为**

非阻塞读取一行；未完成的内容保留，NET_PENDING 不丢失行缓冲。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。非阻塞读取一行；未完成的内容保留，NET_PENDING 不丢失行缓冲。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receiveLineAsync();
```

**源码**：[source/std/net.mgl:90](../../source/std/net.mgl#L90)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-9"></a>

### TcpSocket::isOpen（10）

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

**源码**：[source/std/net.mgl:91](../../source/std/net.mgl#L91)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-10"></a>

### TcpSocket::lastError（11）

**声明**

```magel
String lastError();
```

**行为**

返回连接/网络操作最近的错误文本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。返回连接/网络操作最近的错误文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.lastError();
```

**源码**：[source/std/net.mgl:92](../../source/std/net.mgl#L92)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-11"></a>

### TcpSocket::status（12）

**声明**

```magel
int64 status();
```

**行为**

返回最近一次网络读取状态码。

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

**源码**：[source/std/net.mgl:93](../../source/std/net.mgl#L93)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-12"></a>

### TcpSocket::close（13）

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

**源码**：[source/std/net.mgl:94](../../source/std/net.mgl#L94)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-13"></a>

### TcpSocket::send（14）

**声明**

```magel
int64 send(String data);
```

**行为**

发送 TCP 文本并返回发送结果。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `String data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |

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

<a id="api-14"></a>

### TcpSocket::receive（15）

**声明**

```magel
String receive(int32 maxBytes);
```

**行为**

读取至多 maxBytes 字节；空串结合 status 判断原因。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `maxBytes` | `int32 maxBytes` | 最多接收字节数 | 无 |

**返回值**

`String`。读取至多 maxBytes 字节；空串结合 status 判断原因。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receive(maxBytes);
```

**源码**：[source/std/net.mgl:96](../../source/std/net.mgl#L96)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-15"></a>

### TcpSocket::receiveLine（16）

**声明**

```magel
String receiveLine(int32 timeoutMs);
```

**行为**

读取一行网络文本；空串时检查 status。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `timeoutMs` | `int32 timeoutMs` | 超时时间，毫秒 | 无 |

**返回值**

`String`。读取一行网络文本；空串时检查 status。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receiveLine(timeoutMs);
```

**源码**：[source/std/net.mgl:97](../../source/std/net.mgl#L97)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-16"></a>

### TcpListener::listen（17）

**声明**

```magel
static TcpListener listen(String host, int32 port, int32 backlog);
```

**行为**

绑定本机地址并开始监听，返回监听器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `String host` | 主机名或 IP 地址 | 无 |
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

**源码**：[source/std/net.mgl:105](../../source/std/net.mgl#L105)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-17"></a>

### TcpListener::listen（18）

**声明**

```magel
static TcpListener listen(String host, int32 port);
```

**行为**

绑定本机地址并开始监听，返回监听器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `host` | `String host` | 主机名或 IP 地址 | 无 |
| `port` | `int32 port` | 端口号 | 无 |

**返回值**

`TcpListener`。绑定本机地址并开始监听，返回监听器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::net::TcpListener::listen(host, port);
```

**源码**：[source/std/net.mgl:108](../../source/std/net.mgl#L108)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-18"></a>

### TcpListener::isOpen（19）

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

**源码**：[source/std/net.mgl:111](../../source/std/net.mgl#L111)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-19"></a>

### TcpListener::lastError（20）

**声明**

```magel
String lastError();
```

**行为**

返回连接/网络操作最近的错误文本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。返回连接/网络操作最近的错误文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.lastError();
```

**源码**：[source/std/net.mgl:112](../../source/std/net.mgl#L112)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-20"></a>

### TcpListener::localPort（21）

**声明**

```magel
int64 localPort();
```

**行为**

返回实际绑定的本地端口。

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

**源码**：[source/std/net.mgl:113](../../source/std/net.mgl#L113)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-21"></a>

### TcpListener::accept（22）

**声明**

```magel
TcpSocket accept(int32 timeoutMs);
```

**行为**

接受一条 TCP 连接，可指定等待超时。

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

**源码**：[source/std/net.mgl:114](../../source/std/net.mgl#L114)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-22"></a>

### TcpListener::accept（23）

**声明**

```magel
TcpSocket accept();
```

**行为**

接受一条 TCP 连接，可指定等待超时。

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

**源码**：[source/std/net.mgl:115](../../source/std/net.mgl#L115)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-23"></a>

### TcpListener::acceptAsync（24）

**声明**

```magel
TcpSocket acceptAsync();
```

**行为**

非阻塞接受连接；返回未打开且状态为 NET_PENDING 的值表示暂无连接。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TcpSocket`。非阻塞接受连接；返回未打开且状态为 NET_PENDING 的值表示暂无连接。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.acceptAsync();
```

**源码**：[source/std/net.mgl:117](../../source/std/net.mgl#L117)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-24"></a>

### TcpListener::close（25）

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

**源码**：[source/std/net.mgl:118](../../source/std/net.mgl#L118)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-25"></a>

### HttpResponse::isSuccess（26）

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

**源码**：[source/std/net.mgl:139](../../source/std/net.mgl#L139)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-26"></a>

### HttpResponse::header（27）

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

**源码**：[source/std/net.mgl:142](../../source/std/net.mgl#L142)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-27"></a>

### httpScheme（28）

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

**源码**：[source/std/net.mgl:169](../../source/std/net.mgl#L169)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-28"></a>

### parseUrl（29）

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

**源码**：[source/std/net.mgl:171](../../source/std/net.mgl#L171)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-29"></a>

### buildRequest（30）

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

**源码**：[source/std/net.mgl:211](../../source/std/net.mgl#L211)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-30"></a>

### parseResponse（31）

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

**源码**：[source/std/net.mgl:237](../../source/std/net.mgl#L237)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-31"></a>

### HttpClient::connect（32）

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

**源码**：[source/std/net.mgl:285](../../source/std/net.mgl#L285)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-32"></a>

### HttpClient::readAll（33）

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

**源码**：[source/std/net.mgl:292](../../source/std/net.mgl#L292)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-33"></a>

### HttpClient::exchange（34）

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

**源码**：[source/std/net.mgl:304](../../source/std/net.mgl#L304)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-34"></a>

### HttpClient::close（35）

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

**源码**：[source/std/net.mgl:326](../../source/std/net.mgl#L326)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-35"></a>

### HttpClient::fetch（36）

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

**源码**：[source/std/net.mgl:332](../../source/std/net.mgl#L332)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。

<a id="api-36"></a>

### HttpClient::submit（37）

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

**源码**：[source/std/net.mgl:357](../../source/std/net.mgl#L357)。

**完整示例**：[example/samples/std_net.mgl](../../example/samples/std_net.mgl)。
