#ifndef PARSE_ENGINE_STD_NET_HPP
#define PARSE_ENGINE_STD_NET_HPP

#include "../magel_glue.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/*
 * `std::net` 的宿主胶水：TCP 客户端 / 监听器。
 *
 * 与 `std::sql` 同一套做法：宿主侧的套接字（Windows 是 `SOCKET`，POSIX 是 `int fd`）
 * 收进句柄表，MAGEL 一侧只看见 `int32` 句柄 + 标量：
 *
 *   __netInit / __netError / __netStatus
 *   __netConnect / __netListen / __netAccept
 *   __netSend / __netReceive / __netReceiveLine
 *   __netClose / __netLocalPort / __netPeer
 *
 * **不链接 winsock 导入库**：Windows 下运行期 `LoadLibraryA("ws2_32.dll")` 取符号，
 * 这样解释器（magel）与 magel-emit 导出的程序都不用改链接参数；POSIX 平台直接调用 libc
 * 里的同名接口。
 *
 * 地址解析用 `getaddrinfo(name, NULL, NULL, &res)`（hints 传 NULL，避开平台相关的
 * hints 布局），只读 `ai_family`（偏移 4）、`ai_addr`、`ai_next`；`ai_addr` 的偏移
 * Windows 与 glibc 不同（winsock 把 ai_canonname 排在 ai_addr 前面），按平台取。
 * 端口不在 getaddrinfo 里要，解析完自己按网络字节序写进 sockaddr 的第 2、3 字节。
 */
namespace std_net_glue
{

using magel_glue::Registry;
using Memory::String;

/* ── 平台函数表 ───────────────────────────────────────────────────────── */

using StartupFn = int (*)(unsigned short, void *);
using CleanupFn = int (*)(void);
using CreateFn = std::intptr_t (*)(int, int, int);
using ConnectFn = int (*)(std::intptr_t, const void *, int);
using BindFn = int (*)(std::intptr_t, const void *, int);
using ListenFn = int (*)(std::intptr_t, int);
using AcceptFn = std::intptr_t (*)(std::intptr_t, void *, int *);
using SendFn = int (*)(std::intptr_t, const char *, int, int);
using RecvFn = int (*)(std::intptr_t, char *, int, int);
using CloseFn = int (*)(std::intptr_t);
using ShutdownFn = int (*)(std::intptr_t, int);
using SelectFn = int (*)(int, void *, void *, void *, void *);
using AddrInfoFn = int (*)(const char *, const char *, const void *, void **);
using FreeAddrInfoFn = void (*)(void *);
using HtonsFn = unsigned short (*)(unsigned short);
using InetAddrFn = unsigned long (*)(const char *);
using LastErrorFn = int (*)(void);
using IoctlFn = int (*)(std::intptr_t, long, unsigned long *);
using GetOptFn = int (*)(std::intptr_t, int, int, void *, int *);

struct NetApi
{
    StartupFn startup = nullptr;
    CleanupFn cleanup = nullptr;
    CreateFn create = nullptr;
    ConnectFn connect = nullptr;
    BindFn bind = nullptr;
    ListenFn listen = nullptr;
    AcceptFn accept = nullptr;
    SendFn send = nullptr;
    RecvFn recv = nullptr;
    CloseFn close = nullptr;
    ShutdownFn shutdown = nullptr;
    SelectFn select = nullptr;
    AddrInfoFn getaddrinfo = nullptr;
    FreeAddrInfoFn freeaddrinfo = nullptr;
    HtonsFn htons = nullptr;
    InetAddrFn inet_addr = nullptr;
    LastErrorFn last_error = nullptr;
    IoctlFn ioctl = nullptr;
    GetOptFn getopt = nullptr;
};

inline NetApi &api()
{
    static NetApi instance;
    return instance;
}

inline bool &apiReady()
{
    static bool instance = false;
    return instance;
}

inline char *errorText()
{
    static char buffer[512] = { 0 };
    return buffer;
}

inline void setErrorText(const char *message)
{
    std::snprintf(errorText(), 512, "%s", message != nullptr ? message : "");
}

/**
 * @brief 把最近一次套接字错误写进 errorText()。
 */
inline void captureSocketError(const char *what)
{
    const int code = api().last_error != nullptr ? api().last_error() : 0;
    const char *kind = "错误";
#if defined(_WIN32)
    switch (code)
    {
    case 10035: kind = "暂时无数据（超时）"; break;         /* WSAEWOULDBLOCK */
    case 10036: kind = "操作进行中"; break;                 /* WSAEINPROGRESS */
    case 10053: kind = "连接被本机中止"; break;             /* WSAECONNABORTED */
    case 10054: kind = "连接被对端重置"; break;             /* WSAECONNRESET */
    case 10060: kind = "连接超时"; break;                   /* WSAETIMEDOUT */
    case 10061: kind = "连接被拒绝（端口没人监听）"; break; /* WSAECONNREFUSED */
    case 10065: kind = "主机不可达"; break;                 /* WSAEHOSTUNREACH */
    case 11001: kind = "找不到主机"; break;                 /* WSAHOST_NOT_FOUND */
    default: break;
    }
    std::snprintf(errorText(), 512, "%s：%s（WSA 错误码 %d）", what, kind, code);
#else
    std::snprintf(errorText(), 512, "%s：%s（errno %d）", what, std::strerror(code), code);
#endif
}

/* ── POSIX 侧的薄包装（签名对齐 Windows） ─────────────────────────────── */

#if !defined(_WIN32)

inline int unixStartup(unsigned short, void *) { return 0; }
inline int unixCleanup(void) { return 0; }
inline std::intptr_t unixCreate(int family, int type, int protocol)
{
    return static_cast<std::intptr_t>(::socket(family, type, protocol));
}
inline int unixConnect(std::intptr_t fd, const void *address, int length)
{
    return ::connect(static_cast<int>(fd), static_cast<const struct sockaddr *>(address), static_cast<socklen_t>(length));
}
inline int unixBind(std::intptr_t fd, const void *address, int length)
{
    return ::bind(static_cast<int>(fd), static_cast<const struct sockaddr *>(address), static_cast<socklen_t>(length));
}
inline int unixListen(std::intptr_t fd, int backlog) { return ::listen(static_cast<int>(fd), backlog); }
inline std::intptr_t unixAccept(std::intptr_t fd, void *address, int *length)
{
    return static_cast<std::intptr_t>(::accept(static_cast<int>(fd),
                                               static_cast<struct sockaddr *>(address),
                                               reinterpret_cast<socklen_t *>(length)));
}
inline int unixSend(std::intptr_t fd, const char *data, int length, int flags)
{
    return static_cast<int>(::send(static_cast<int>(fd), data, static_cast<std::size_t>(length), flags));
}
inline int unixRecv(std::intptr_t fd, char *buffer, int length, int flags)
{
    return static_cast<int>(::recv(static_cast<int>(fd), buffer, static_cast<std::size_t>(length), flags));
}
inline int unixClose(std::intptr_t fd) { return ::close(static_cast<int>(fd)); }
inline int unixShutdown(std::intptr_t fd, int how) { return ::shutdown(static_cast<int>(fd), how); }
inline int unixSelect(int nfds, void *readSet, void *writeSet, void *exceptSet, void *timeout)
{
    return ::select(nfds, static_cast<fd_set *>(readSet), static_cast<fd_set *>(writeSet),
                    static_cast<fd_set *>(exceptSet), static_cast<struct timeval *>(timeout));
}
inline int unixGetAddrInfo(const char *node, const char *service, const void *hints, void **result)
{
    return ::getaddrinfo(node, service, static_cast<const struct addrinfo *>(hints),
                         reinterpret_cast<struct addrinfo **>(result));
}
inline void unixFreeAddrInfo(void *result) { ::freeaddrinfo(static_cast<struct addrinfo *>(result)); }
inline unsigned short unixHtons(unsigned short value) { return htons(value); }
inline unsigned long unixInetAddr(const char *text) { return ::inet_addr(text); }
inline int unixLastError() { return errno; }
inline int unixIoctl(std::intptr_t fd, long, unsigned long *argument)
{
    const int flags = ::fcntl(static_cast<int>(fd), F_GETFL, 0);
    if (flags < 0) return -1;
    const int updated = (argument != nullptr && *argument != 0ul) ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return ::fcntl(static_cast<int>(fd), F_SETFL, updated);
}
inline int unixGetOpt(std::intptr_t fd, int level, int name, void *value, int *length)
{
    socklen_t native = length != nullptr ? static_cast<socklen_t>(*length) : 0;
    const int result = ::getsockopt(static_cast<int>(fd), level, name, value, &native);
    if (length != nullptr) *length = static_cast<int>(native);
    return result;
}

#endif

/* ── 初始化 ───────────────────────────────────────────────────────────── */

inline bool initNet()
{
    if (apiReady()) return true;
    NetApi &table = api();
#if defined(_WIN32)
    HMODULE library = ::LoadLibraryA("ws2_32.dll");
    if (library == nullptr)
    {
        setErrorText("找不到 ws2_32.dll（Windows 套接字库）");
        return false;
    }
    table.startup = reinterpret_cast<StartupFn>(reinterpret_cast<void *>(::GetProcAddress(library, "WSAStartup")));
    table.cleanup = reinterpret_cast<CleanupFn>(reinterpret_cast<void *>(::GetProcAddress(library, "WSACleanup")));
    table.create = reinterpret_cast<CreateFn>(reinterpret_cast<void *>(::GetProcAddress(library, "socket")));
    table.connect = reinterpret_cast<ConnectFn>(reinterpret_cast<void *>(::GetProcAddress(library, "connect")));
    table.bind = reinterpret_cast<BindFn>(reinterpret_cast<void *>(::GetProcAddress(library, "bind")));
    table.listen = reinterpret_cast<ListenFn>(reinterpret_cast<void *>(::GetProcAddress(library, "listen")));
    table.accept = reinterpret_cast<AcceptFn>(reinterpret_cast<void *>(::GetProcAddress(library, "accept")));
    table.send = reinterpret_cast<SendFn>(reinterpret_cast<void *>(::GetProcAddress(library, "send")));
    table.recv = reinterpret_cast<RecvFn>(reinterpret_cast<void *>(::GetProcAddress(library, "recv")));
    table.close = reinterpret_cast<CloseFn>(reinterpret_cast<void *>(::GetProcAddress(library, "closesocket")));
    table.shutdown = reinterpret_cast<ShutdownFn>(reinterpret_cast<void *>(::GetProcAddress(library, "shutdown")));
    table.select = reinterpret_cast<SelectFn>(reinterpret_cast<void *>(::GetProcAddress(library, "select")));
    table.getaddrinfo = reinterpret_cast<AddrInfoFn>(reinterpret_cast<void *>(::GetProcAddress(library, "getaddrinfo")));
    table.freeaddrinfo = reinterpret_cast<FreeAddrInfoFn>(reinterpret_cast<void *>(::GetProcAddress(library, "freeaddrinfo")));
    table.htons = reinterpret_cast<HtonsFn>(reinterpret_cast<void *>(::GetProcAddress(library, "htons")));
    table.inet_addr = reinterpret_cast<InetAddrFn>(reinterpret_cast<void *>(::GetProcAddress(library, "inet_addr")));
    table.last_error = reinterpret_cast<LastErrorFn>(reinterpret_cast<void *>(::GetProcAddress(library, "WSAGetLastError")));
    table.ioctl = reinterpret_cast<IoctlFn>(reinterpret_cast<void *>(::GetProcAddress(library, "ioctlsocket")));
    table.getopt = reinterpret_cast<GetOptFn>(reinterpret_cast<void *>(::GetProcAddress(library, "getsockopt")));
    if (table.create == nullptr || table.connect == nullptr || table.select == nullptr || table.getaddrinfo == nullptr || table.last_error == nullptr)
    {
        setErrorText("ws2_32.dll 缺少需要的符号");
        return false;
    }
    void *data = std::calloc(1, 1024); /* WSADATA 约 400 字节，留足空间 */
    const int started = table.startup(0x0202, data);
    std::free(data);
    if (started != 0)
    {
        setErrorText("WSAStartup 失败");
        return false;
    }
#else
    table.startup = unixStartup;
    table.cleanup = unixCleanup;
    table.create = unixCreate;
    table.connect = unixConnect;
    table.bind = unixBind;
    table.listen = unixListen;
    table.accept = unixAccept;
    table.send = unixSend;
    table.recv = unixRecv;
    table.close = unixClose;
    table.shutdown = unixShutdown;
    table.select = unixSelect;
    table.getaddrinfo = unixGetAddrInfo;
    table.freeaddrinfo = unixFreeAddrInfo;
    table.htons = unixHtons;
    table.inet_addr = unixInetAddr;
    table.last_error = unixLastError;
    table.ioctl = unixIoctl;
    table.getopt = unixGetOpt;
#endif
    apiReady() = true;
    return true;
}

/* ── 套接字句柄表 ─────────────────────────────────────────────────────── */

constexpr int kMaxSockets = 64;
/*
 * fd_set 两个平台的布局不同，此处自己拼，免得为它把 winsock2.h 排在 windows.h 前面
 * （winsock2.h 在 windows.h 之后会被 #warning 提示）。
 *
 *   Windows(winsock)：`{ u_int fd_count; SOCKET fd_array[FD_SETSIZE]; }`，
 *                      SOCKET = UINT_PTR 要求 8 字节对齐 → 数组从偏移 8 开始，
 *                      FD_SETSIZE 默认 64，与 kMaxSockets 一致；
 *   POSIX            ：位图，共 FD_SETSIZE(1024) 位。
 */
#if defined(_WIN32)
struct NativeFdSet
{
    unsigned int count;
    std::uintptr_t items[kMaxSockets];
};
constexpr int kFdSetBytes = static_cast<int>(sizeof(NativeFdSet));
#else
constexpr int kFdSetBytes = static_cast<int>(sizeof(fd_set));
#endif
constexpr int kAddrBuffer = 32; /* sockaddr_in 16 / sockaddr_in6 28 */

/* 没读到数据的原因（MAGEL 侧用 __netStatus 取）。 */
constexpr int kNetOk = 0;
constexpr int kNetClosed = 1;  /* 对端关闭 */
constexpr int kNetTimeout = 2; /* 等超时 */
constexpr int kNetError = 3;

/* setsockopt / getsockopt 的层级与选项号两个平台不同 */
#if defined(_WIN32)
constexpr int kSolSocket = 0xFFFF;
constexpr int kSoError = 0x1007;
#else
constexpr int kSolSocket = 1; /* SOL_SOCKET */
constexpr int kSoError = 4;   /* SO_ERROR */
#endif

struct SockSlot
{
    bool used = false;
    bool listener = false;
    std::intptr_t fd = -1;
    std::string pending; /* 行缓冲：多收的字节留到下次 */
    char peer[80] = { 0 };
};

inline std::vector<SockSlot> &sockets()
{
    static std::vector<SockSlot> instance(kMaxSockets);
    return instance;
}

inline int &lastStatus()
{
    static int instance = kNetOk;
    return instance;
}

inline SockSlot *slotAt(std::int32_t handle)
{
    if (handle < 0 || handle >= kMaxSockets) return nullptr;
    SockSlot &slot = sockets()[static_cast<std::size_t>(handle)];
    return slot.used ? &slot : nullptr;
}

inline std::int32_t allocateSlot(bool listener, std::intptr_t fd)
{
    for (int i = 0; i < kMaxSockets; ++i)
    {
        SockSlot &slot = sockets()[static_cast<std::size_t>(i)];
        if (slot.used) continue;
        slot.used = true;
        slot.listener = listener;
        slot.fd = fd;
        slot.pending.clear();
        slot.peer[0] = '\0';
        return i;
    }
    setErrorText("套接字句柄用尽");
    return -1;
}

inline bool isInvalid(std::intptr_t fd)
{
#if defined(_WIN32)
    return fd == static_cast<std::intptr_t>(~static_cast<std::uintptr_t>(0));
#else
    return fd < 0;
#endif
}

/* ── select 用的 fd_set（两个平台布局不同，手工拼） ───────────────────── */

inline void fdSetClear(void *set)
{
    std::memset(set, 0, static_cast<std::size_t>(kFdSetBytes));
}

inline void fdSetAdd(void *set, std::intptr_t fd, int *maxFd)
{
#if defined(_WIN32)
    NativeFdSet *native = static_cast<NativeFdSet *>(set);
    if (native->count < kMaxSockets)
    {
        native->items[native->count] = static_cast<std::uintptr_t>(fd);
        native->count = native->count + 1u;
    }
    (void)maxFd;
#else
    fd_set *native = static_cast<fd_set *>(set);
    FD_SET(static_cast<int>(fd), native);
    if (static_cast<int>(fd) + 1 > *maxFd) *maxFd = static_cast<int>(fd) + 1;
#endif
}

/**
 * @brief 等 fd 可读；返回 1 就绪、0 超时、-1 出错。
 */
inline int waitReadable(std::intptr_t fd, int timeoutMs)
{
    void *readSet = std::calloc(1, kFdSetBytes);
    if (readSet == nullptr)
    {
        setErrorText("内存不足");
        return -1;
    }
    fdSetClear(readSet);
    int maxFd = 0;
    fdSetAdd(readSet, fd, &maxFd);
    const long seconds = timeoutMs / 1000;
    const long micros = (timeoutMs % 1000) * 1000;
    struct
    {
        long seconds;
        long micros;
    } timeout{ seconds, micros };
    const int ready = api().select(maxFd, readSet, nullptr, nullptr, timeoutMs < 0 ? nullptr : &timeout);
    std::free(readSet);
    if (ready > 0) return 1;
    if (ready == 0) return 0;
    captureSocketError("等待可读");
    return -1;
}

/**
 * @brief 等 fd 可写（非阻塞 connect 用）；返回 1 就绪、0 超时、-1 出错。
 */
inline int waitWritable(std::intptr_t fd, int timeoutMs)
{
    void *writeSet = std::calloc(1, kFdSetBytes);
    /* connect 失败在 Windows 上报到 except 集合、POSIX 上报到 write 集合，
     * 两边都放进去，谁先冒头都能立刻知道"连完了"。 */
    void *errorSet = std::calloc(1, kFdSetBytes);
    if (writeSet == nullptr || errorSet == nullptr)
    {
        std::free(writeSet);
        std::free(errorSet);
        setErrorText("内存不足");
        return -1;
    }
    fdSetClear(writeSet);
    fdSetClear(errorSet);
    int maxFd = 0;
    fdSetAdd(writeSet, fd, &maxFd);
    fdSetAdd(errorSet, fd, &maxFd);
    struct
    {
        long seconds;
        long micros;
    } timeout{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    const int ready = api().select(maxFd, nullptr, writeSet, errorSet, timeoutMs < 0 ? nullptr : &timeout);
    std::free(writeSet);
    std::free(errorSet);
    if (ready > 0) return 1;
    if (ready == 0) return 0;
    captureSocketError("等待可写");
    return -1;
}

/**
 * @brief 开关非阻塞模式。
 */
inline bool setNonBlocking(std::intptr_t fd, bool enabled)
{
    unsigned long mode = enabled ? 1ul : 0ul;
    return api().ioctl != nullptr && api().ioctl(fd, 0x8004667EL /* FIONBIO */, &mode) == 0;
}

/* ── 地址解析 ─────────────────────────────────────────────────────────── */

inline bool numericHost(const std::string &host)
{
    if (host.empty()) return false;
    for (std::size_t i = 0; i < host.size(); ++i)
    {
        const char c = host[i];
        if ((c < '0' || c > '9') && c != '.') return false;
    }
    return true;
}

/**
 * @brief 读 addrinfo 的 ai_family / ai_addr / ai_next（偏移见文件头注释）。
 */
inline int addrInfoFamily(const void *info)
{
    int value = 0;
    std::memcpy(&value, static_cast<const char *>(info) + 4, sizeof(int));
    return value;
}

inline const void *addrInfoAddr(const void *info)
{
    void *value = nullptr;
#if defined(_WIN32)
    std::memcpy(&value, static_cast<const char *>(info) + 32, sizeof(void *));
#else
    std::memcpy(&value, static_cast<const char *>(info) + 24, sizeof(void *));
#endif
    return value;
}

inline const void *addrInfoNext(const void *info)
{
    void *value = nullptr;
    std::memcpy(&value, static_cast<const char *>(info) + 40, sizeof(void *));
    return value;
}

inline int addressLength(int family)
{
    return family == 2 ? 16 : 28; /* AF_INET / AF_INET6 */
}

/**
 * @brief 把主机名 + 端口解析成 sockaddr 字节。
 * @return 地址长度；失败 0。
 */
inline int resolveAddress(const std::string &host, int port, unsigned char *buffer)
{
    const std::string target = (host == "*" || host == "0.0.0.0" || host == "any") ? std::string() : host;
    if (target.empty() || numericHost(target))
    {
        /* sa_family 是 2 字节小端值：AF_INET = 2 → 字节序列 02 00 */
        buffer[0] = 2;
        buffer[1] = 0;
        const unsigned short networkPort = api().htons(static_cast<unsigned short>(port));
        std::memcpy(buffer + 2, &networkPort, 2);
        const unsigned long address = target.empty() ? 0u : api().inet_addr(target.c_str());
        if (target.empty())
        {
            std::memset(buffer + 4, 0, 4);
        }
        else
        {
            std::memcpy(buffer + 4, &address, 4);
            if (address == 0xFFFFFFFFul)
            {
                setErrorText("无效的 IP 地址");
                return 0;
            }
        }
        std::memset(buffer + 8, 0, 8);
        return 16;
    }
    void *result = nullptr;
    if (api().getaddrinfo(target.c_str(), nullptr, nullptr, &result) != 0)
    {
        captureSocketError(("解析主机 " + target).c_str());
        return 0;
    }
    int length = 0;
    for (const void *info = result; info != nullptr; info = addrInfoNext(info))
    {
        const int family = addrInfoFamily(info);
        if (family != 2 && family != 10) continue; /* AF_INET / AF_INET6 */
        const int size = addressLength(family);
        std::memcpy(buffer, addrInfoAddr(info), static_cast<std::size_t>(size));
        const unsigned short networkPort = api().htons(static_cast<unsigned short>(port));
        std::memcpy(buffer + 2, &networkPort, 2); /* 端口在 sockaddr 的第 2、3 字节 */
        length = size;
        break;
    }
    api().freeaddrinfo(result);
    if (length == 0) setErrorText("找不到可用的地址");
    return length;
}

/* ── 对外函数 ─────────────────────────────────────────────────────────── */

inline std::int32_t netInit()
{
    if (!initNet()) return -1;
    lastStatus() = kNetOk;
    return 0;
}

inline String netError()
{
    return String(errorText());
}

inline std::int32_t netStatus()
{
    return lastStatus();
}

inline std::int32_t netConnect(String host, std::int32_t port, std::int32_t timeoutMs)
{
    if (!initNet()) return -1;
    unsigned char address[kAddrBuffer];
    std::memset(address, 0, sizeof(address));
    const int length = resolveAddress(host, static_cast<int>(port), address);
    if (length == 0) return -1;
    const std::intptr_t fd = api().create(2, 1, 6); /* AF_INET(2) 家族见 resolve；SOCK_STREAM, IPPROTO_TCP */
    if (isInvalid(fd))
    {
        captureSocketError("创建套接字");
        return -1;
    }
    if (timeoutMs > 0)
    {
        /* 带超时的连接：先切非阻塞，连不上就 select 等可写，再查 SO_ERROR。 */
        const bool nonBlocking = setNonBlocking(fd, true);
        if (api().connect(fd, address, length) != 0)
        {
            const int code = api().last_error != nullptr ? api().last_error() : 0;
            bool inProgress = false;
#if defined(_WIN32)
            inProgress = (code == 10035 || code == 10036); /* WSAEWOULDBLOCK / WSAEINPROGRESS */
#else
            inProgress = (code == EINPROGRESS || code == EWOULDBLOCK);
#endif
            if (!inProgress)
            {
                captureSocketError("连接");
                api().close(fd);
                return -1;
            }
            const int ready = waitWritable(fd, timeoutMs);
            if (ready < 0)
            {
                api().close(fd);
                return -1;
            }
            int pending = 0;
            int size = static_cast<int>(sizeof(pending));
            if (api().getopt != nullptr && api().getopt(fd, kSolSocket, kSoError, &pending, &size) == 0 && pending != 0)
            {
                char message[128];
                std::snprintf(message, sizeof(message), "连接失败（套接字错误码 %d）", pending);
                setErrorText(message);
                api().close(fd);
                return -1;
            }
            if (ready == 0)
            {
                setErrorText("连接超时");
                api().close(fd);
                return -1;
            }
        }
        if (nonBlocking) setNonBlocking(fd, false);
    }
    else if (api().connect(fd, address, length) != 0)
    {
        captureSocketError("连接");
        api().close(fd);
        return -1;
    }
    const std::int32_t handle = allocateSlot(false, fd);
    if (handle < 0)
    {
        api().close(fd);
        return -1;
    }
    std::snprintf(errorText(), 512, "%s", "");
    return handle;
}

inline std::int32_t netListen(String host, std::int32_t port, std::int32_t backlog)
{
    if (!initNet()) return -1;
    unsigned char address[kAddrBuffer];
    std::memset(address, 0, sizeof(address));
    const int length = resolveAddress(host, static_cast<int>(port), address);
    if (length == 0) return -1;
    const std::intptr_t fd = api().create(2, 1, 6);
    if (isInvalid(fd))
    {
        captureSocketError("创建监听套接字");
        return -1;
    }
    if (api().bind(fd, address, length) != 0)
    {
        captureSocketError("绑定端口");
        api().close(fd);
        return -1;
    }
    if (api().listen(fd, backlog > 0 ? static_cast<int>(backlog) : 8) != 0)
    {
        captureSocketError("开始监听");
        api().close(fd);
        return -1;
    }
    const std::int32_t handle = allocateSlot(true, fd);
    if (handle < 0)
    {
        api().close(fd);
        return -1;
    }
    return handle;
}

inline std::int32_t netAccept(std::int32_t listener, std::int32_t timeoutMs)
{
    SockSlot *slot = slotAt(listener);
    if (slot == nullptr || !slot->listener)
    {
        lastStatus() = kNetError;
        setErrorText("不是监听句柄");
        return -1;
    }
    const int ready = waitReadable(slot->fd, timeoutMs);
    if (ready <= 0)
    {
        lastStatus() = ready == 0 ? kNetTimeout : kNetError;
        if (ready == 0) setErrorText("accept 超时");
        return -1;
    }
    unsigned char address[kAddrBuffer];
    int length = static_cast<int>(sizeof(address));
    const std::intptr_t fd = api().accept(slot->fd, address, &length);
    if (isInvalid(fd))
    {
        lastStatus() = kNetError;
        captureSocketError("接受连接");
        return -1;
    }
    const std::int32_t handle = allocateSlot(false, fd);
    if (handle < 0)
    {
        api().close(fd);
        lastStatus() = kNetError;
        return -1;
    }
    lastStatus() = kNetOk;
    return handle;
}

inline std::int32_t netSend(std::int32_t sock, String data)
{
    SockSlot *slot = slotAt(sock);
    if (slot == nullptr || slot->listener)
    {
        setErrorText("不是连接句柄");
        return -1;
    }
    std::size_t sent = 0u;
    const std::size_t total = data.size();
    while (sent < total)
    {
        const int written = api().send(slot->fd, data.data() + sent, static_cast<int>(total - sent), 0);
        if (written <= 0)
        {
            captureSocketError("发送");
            return -1;
        }
        sent += static_cast<std::size_t>(written);
    }
    return static_cast<std::int32_t>(sent);
}

/**
 * @brief 收一段文本；结果原因写进 __netStatus。
 */
inline String netReceive(std::int32_t sock, std::int32_t maxBytes)
{
    SockSlot *slot = slotAt(sock);
    if (slot == nullptr || slot->listener)
    {
        lastStatus() = kNetError;
        setErrorText("不是连接句柄");
        return String();
    }
    if (!slot->pending.empty())
    {
        std::string out = slot->pending;
        slot->pending.clear();
        lastStatus() = kNetOk;
        return String(out);
    }
    const int want = maxBytes > 0 ? static_cast<int>(maxBytes) : 4096;
    std::vector<char> buffer(static_cast<std::size_t>(want));
    const int count = api().recv(slot->fd, buffer.data(), want, 0);
    if (count > 0)
    {
        lastStatus() = kNetOk;
        return String(buffer.data(), static_cast<std::size_t>(count));
    }
    if (count == 0)
    {
        lastStatus() = kNetClosed;
        setErrorText("对端已关闭连接");
        return String();
    }
    lastStatus() = kNetError;
    captureSocketError("接收");
    return String();
}

/**
 * @brief 收到行尾（`\n`）为止，返回不含换行的那一行。
 */
inline String netReceiveLine(std::int32_t sock, std::int32_t timeoutMs)
{
    SockSlot *slot = slotAt(sock);
    if (slot == nullptr || slot->listener)
    {
        lastStatus() = kNetError;
        setErrorText("不是连接句柄");
        return String();
    }
    while (true)
    {
        const std::size_t newline = slot->pending.find('\n');
        if (newline != std::string::npos)
        {
            std::string line = slot->pending.substr(0u, newline);
            slot->pending.erase(0u, newline + 1u);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lastStatus() = kNetOk;
            return String(line);
        }
        const int ready = waitReadable(slot->fd, timeoutMs);
        if (ready == 0)
        {
            lastStatus() = kNetTimeout;
            setErrorText("读取超时");
            return String();
        }
        if (ready < 0)
        {
            lastStatus() = kNetError;
            return String();
        }
        char buffer[4096];
        const int count = api().recv(slot->fd, buffer, static_cast<int>(sizeof(buffer)), 0);
        if (count > 0)
        {
            slot->pending.append(buffer, static_cast<std::size_t>(count));
            continue;
        }
        if (count == 0)
        {
            lastStatus() = kNetClosed;
            setErrorText("对端已关闭连接");
            /* 缓冲区里还剩最后一行没有换行时也交出去 */
            if (!slot->pending.empty())
            {
                std::string line = slot->pending;
                slot->pending.clear();
                lastStatus() = kNetOk;
                return String(line);
            }
            return String();
        }
        lastStatus() = kNetError;
        captureSocketError("接收");
        return String();
    }
}

inline std::int32_t netClose(std::int32_t handle)
{
    SockSlot *slot = slotAt(handle);
    if (slot == nullptr) return -1;
    if (!slot->listener && api().shutdown != nullptr) api().shutdown(slot->fd, 2); /* SD_BOTH / SHUT_RDWR */
    api().close(slot->fd);
    slot->used = false;
    slot->fd = -1;
    slot->listener = false;
    slot->pending.clear();
    slot->peer[0] = '\0';
    return 0;
}

/**
 * @brief 本端端口（监听器用得多：端口传 0 让系统分配）。
 */
inline std::int32_t netLocalPort(std::int32_t handle)
{
    SockSlot *slot = slotAt(handle);
    if (slot == nullptr) return 0;
#if defined(_WIN32)
    unsigned char address[kAddrBuffer];
    int length = static_cast<int>(sizeof(address));
    void *getSockName = reinterpret_cast<void *>(::GetProcAddress(::GetModuleHandleA("ws2_32.dll"), "getsockname"));
    if (getSockName == nullptr) return 0;
    using GetSockNameFn = int (*)(std::intptr_t, void *, int *);
    if (reinterpret_cast<GetSockNameFn>(getSockName)(slot->fd, address, &length) != 0) return 0;
    unsigned short networkPort = 0;
    std::memcpy(&networkPort, address + 2, 2);
    return static_cast<std::int32_t>(api().htons(networkPort));
#else
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    if (::getsockname(static_cast<int>(slot->fd), reinterpret_cast<struct sockaddr *>(&address), &length) != 0) return 0;
    return static_cast<std::int32_t>(ntohs(address.sin_port));
#endif
}

inline bool bind(Registry &registry, String *error = nullptr)
{
    return registry.bindTyped<std::int32_t>("std::net::__netInit", netInit, error) && registry.bindTyped<String>("std::net::__netError", netError, error) && registry.bindTyped<std::int32_t>("std::net::__netStatus", netStatus, error) && registry.bindTyped<std::int32_t, String, std::int32_t, std::int32_t>("std::net::__netConnect", netConnect, error) && registry.bindTyped<std::int32_t, String, std::int32_t, std::int32_t>("std::net::__netListen", netListen, error) && registry.bindTyped<std::int32_t, std::int32_t, std::int32_t>("std::net::__netAccept", netAccept, error) && registry.bindTyped<std::int32_t, std::int32_t, String>("std::net::__netSend", netSend, error) && registry.bindTyped<String, std::int32_t, std::int32_t>("std::net::__netReceive", netReceive, error) && registry.bindTyped<String, std::int32_t, std::int32_t>("std::net::__netReceiveLine", netReceiveLine, error) && registry.bindTyped<std::int32_t, std::int32_t>("std::net::__netClose", netClose, error) && registry.bindTyped<std::int32_t, std::int32_t>("std::net::__netLocalPort", netLocalPort, error);
}

}  // namespace std_net_glue

#endif
