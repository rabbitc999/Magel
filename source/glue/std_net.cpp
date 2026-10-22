#include "std_net.hpp"

#include "../chali_glue.hpp"
#include "std_native_record.hpp"

#include <algorithm>
#include <chrono>
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
 * 收进句柄表，CHALI 一侧只看见 `int32` 句柄 + 标量：
 *
 *   __netInit / __netError / __netStatus
 *   __netConnect / __netListen / __netAccept
 *   __netSend / __netReceive / __netReceiveLine
 *   __netClose / __netLocalPort / __netPeer
 *
 * **不链接 winsock 导入库**：Windows 下运行期 `LoadLibraryA("ws2_32.dll")` 取符号，
 * 这样ChaLi VM 与独立 VM 宿主都不用改链接参数；POSIX 平台直接调用 libc
 * 里的同名接口。
 *
 * 地址解析用 `getaddrinfo(name, NULL, NULL, &res)`（hints 传 NULL，避开平台相关的
 * hints 布局），只读 `ai_family`（偏移 4）、`ai_addr`、`ai_next`；`ai_addr` 的偏移
 * Windows 与 glibc 不同（winsock 把 ai_canonname 排在 ai_addr 前面），按平台取。
 * 端口不在 getaddrinfo 里要，解析完自己按网络字节序写进 sockaddr 的第 2、3 字节。
 */
namespace std_net_glue
{

using chali_glue::Registry;
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
    HtonsFn hostToNetwork16 = nullptr;
    InetAddrFn inet_addr = nullptr;
    LastErrorFn last_error = nullptr;
    IoctlFn ioctl = nullptr;
    GetOptFn getopt = nullptr;
};

/**
 * @brief 取得网络系统调用入口表。
 * @return 进程内持久保存的网络函数表。
 */
inline NetApi &api()
{
    static NetApi instance;
    return instance;
}

/**
 * @brief 取得网络系统调用表的初始化标志。
 * @return 可读写的初始化状态引用。
 */
inline bool &apiReady()
{
    static bool instance = false;
    return instance;
}

/**
 * @brief 取得网络操作的诊断文本缓冲。
 * @return 进程内持久保存的可写诊断缓冲。
 */
inline char *errorText()
{
    static char buffer[512] = { 0 };
    return buffer;
}

/**
 * @brief 在固定诊断缓冲内保存并终止错误文本。
 * @param message 要保存到错误缓冲的诊断文本。
 */
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

/**
 * @brief 为统一套接字接口提供 POSIX 平台的空初始化适配。
 * @param version 兼容 Windows 初始化接口的版本参数；POSIX 不使用。
 * @param data 兼容 Windows 初始化接口的输出参数；POSIX 不使用。
 * @return 固定返回零，POSIX 套接字不需要启动调用。
 */
inline int unixStartup([[maybe_unused]] unsigned short version, [[maybe_unused]] void *data) { return 0; }
/**
 * @brief 为统一套接字接口提供 POSIX 平台的空清理适配。
 * @return 固定返回零。
 */
inline int unixCleanup(void) { return 0; }
/**
 * @brief 按指定协议创建 POSIX 套接字。
 * @param family 系统套接字地址族。
 * @param type 套接字类型。
 * @param protocol 系统套接字协议编号。
 * @return 新文件描述符；失败返回负值。
 */
inline std::intptr_t unixCreate(int family, int type, int protocol)
{
    return static_cast<std::intptr_t>(::socket(family, type, protocol));
}
/**
 * @brief 通过 POSIX 接口连接指定套接字地址。
 * @param fd 宿主套接字文件描述符。
 * @param address 系统套接字地址结构，可由接收接口回填。
 * @param length 输入或输出缓冲的字节长度。
 * @return 系统 connect 的状态码。
 */
inline int unixConnect(std::intptr_t fd, const void *address, int length)
{
    return ::connect(static_cast<int>(fd), static_cast<const struct sockaddr *>(address), static_cast<socklen_t>(length));
}
/**
 * @brief 通过 POSIX 接口为套接字绑定本地地址。
 * @param fd 宿主套接字文件描述符。
 * @param address 系统套接字地址结构，可由接收接口回填。
 * @param length 输入或输出缓冲的字节长度。
 * @return 系统 bind 的状态码。
 */
inline int unixBind(std::intptr_t fd, const void *address, int length)
{
    return ::bind(static_cast<int>(fd), static_cast<const struct sockaddr *>(address), static_cast<socklen_t>(length));
}
/**
 * @brief 通过 POSIX 接口将套接字设为监听状态。
 * @param fd 宿主套接字文件描述符。
 * @param backlog 监听队列的容量上限。
 * @return 系统 listen 的状态码。
 */
inline int unixListen(std::intptr_t fd, int backlog) { return ::listen(static_cast<int>(fd), backlog); }
/**
 * @brief 通过 POSIX 接口接收连接并回填对端地址。
 * @param fd 宿主套接字文件描述符。
 * @param address 系统套接字地址结构，可由接收接口回填。
 * @param length 输入地址或选项缓冲容量，并接收实际字节长度。
 * @return 接收连接的文件描述符；失败返回负值。
 */
inline std::intptr_t unixAccept(std::intptr_t fd, void *address, int *length)
{
    return static_cast<std::intptr_t>(::accept(static_cast<int>(fd),
                                               static_cast<struct sockaddr *>(address),
                                               reinterpret_cast<socklen_t *>(length)));
}
/**
 * @brief 通过 POSIX 接口发送指定字节区间。
 * @param fd 宿主套接字文件描述符。
 * @param data 输入数值或待发送字节的起始地址。
 * @param length 输入或输出缓冲的字节长度。
 * @param flags 传递给系统收发接口的标志位。
 * @return 实际发送字节数；失败返回负值。
 */
inline int unixSend(std::intptr_t fd, const char *data, int length, int flags)
{
    return static_cast<int>(::send(static_cast<int>(fd), data, static_cast<std::size_t>(length), flags));
}
/**
 * @brief 通过 POSIX 接口接收字节到调用方缓冲。
 * @param fd 宿主套接字文件描述符。
 * @param buffer 接收字节或诊断文本的输出缓冲。
 * @param length 输入或输出缓冲的字节长度。
 * @param flags 传递给系统收发接口的标志位。
 * @return 实际接收字节数；零表示关闭，负值表示失败。
 */
inline int unixRecv(std::intptr_t fd, char *buffer, int length, int flags)
{
    return static_cast<int>(::recv(static_cast<int>(fd), buffer, static_cast<std::size_t>(length), flags));
}
/**
 * @brief 通过 POSIX 接口关闭套接字文件描述符。
 * @param fd 宿主套接字文件描述符。
 * @return 系统 close 的状态码。
 */
inline int unixClose(std::intptr_t fd) { return ::close(static_cast<int>(fd)); }
/**
 * @brief 通过 POSIX 接口关闭套接字的指定通信方向。
 * @param fd 宿主套接字文件描述符。
 * @param how 需要关闭的套接字通信方向。
 * @return 系统 shutdown 的状态码。
 */
inline int unixShutdown(std::intptr_t fd, int how) { return ::shutdown(static_cast<int>(fd), how); }
/**
 * @brief 等待 POSIX 描述符集合中的可读、可写或异常事件。
 * @param nfds 最大描述符编号加一。
 * @param readSet 输入并回填可读描述符集合，可为空。
 * @param writeSet 输入并回填可写描述符集合，可为空。
 * @param exceptSet 输入并回填异常描述符集合，可为空。
 * @param timeout 系统调用的等待期限，可为空。
 * @return 就绪描述符数；零表示超时，负值表示失败。
 */
inline int unixSelect(int nfds, void *readSet, void *writeSet, void *exceptSet, void *timeout)
{
    return ::select(nfds, static_cast<fd_set *>(readSet), static_cast<fd_set *>(writeSet),
                    static_cast<fd_set *>(exceptSet), static_cast<struct timeval *>(timeout));
}
/**
 * @brief 通过 POSIX 名称解析接口创建地址链表。
 * @param node 待解析的主机名或地址文本。
 * @param service 服务名称或端口文本，可为空。
 * @param hints 名称解析的地址族和协议约束，可为空。
 * @param result 接收解析结果链表或需要释放的结果对象。
 * @return 名称解析状态码，零表示成功。
 */
inline int unixGetAddrInfo(const char *node, const char *service, const void *hints, void **result)
{
    return ::getaddrinfo(node, service, static_cast<const struct addrinfo *>(hints),
                         reinterpret_cast<struct addrinfo **>(result));
}
/**
 * @brief 释放 POSIX 名称解析返回的地址链表。
 * @param result 名称解析成功后取得的地址链表。
 */
inline void unixFreeAddrInfo(void *result) { ::freeaddrinfo(static_cast<struct addrinfo *>(result)); }
/**
 * @brief 将主机字节序的 16 位整数转换为网络字节序。
 * @param value 需要转换为网络字节序的主机整数。
 * @return 网络字节序的整数位模式。
 */
inline unsigned short unixHtons(unsigned short value) { return htons(value); }
/**
 * @brief 将 IPv4 文本地址转换为网络字节序整数。
 * @param text 待解析或转换的完整输入文本。
 * @return 系统 inet_addr 返回的地址位模式。
 */
inline unsigned long unixInetAddr(const char *text) { return ::inet_addr(text); }
/**
 * @brief 读取最近一次 POSIX 网络调用的线程错误码。
 * @return 当前 errno 值。
 */
inline int unixLastError() { return errno; }
/**
 * @brief 使用 POSIX fcntl 设置套接字的非阻塞状态。
 * @param command 兼容 ioctl 接口的命令编号；本适配器只实现非阻塞切换。
 * @param fd 宿主套接字文件描述符。
 * @param argument 输入所需的非阻塞状态。
 * @return 修改成功返回零，查询或修改失败返回负值。
 */
inline int unixIoctl(std::intptr_t fd, [[maybe_unused]] long command, unsigned long *argument)
{
    const int flags = ::fcntl(static_cast<int>(fd), F_GETFL, 0);
    if (flags < 0) return -1;
    const int updated = (argument != nullptr && *argument != 0ul) ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return ::fcntl(static_cast<int>(fd), F_SETFL, updated);
}
/**
 * @brief 读取 POSIX 套接字选项并回填实际长度。
 * @param fd 宿主套接字文件描述符。
 * @param level 套接字选项所属的协议层。
 * @param name 需要绑定或解析的入口名称。
 * @param value 读取成功时接收解析值，可为空。
 * @param length 输入地址或选项缓冲容量，并接收实际字节长度。
 * @return 系统 getsockopt 的状态码。
 */
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
    table.hostToNetwork16 = reinterpret_cast<HtonsFn>(reinterpret_cast<void *>(::GetProcAddress(library, "htons")));
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
    table.hostToNetwork16 = unixHtons;
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

/* 没读到数据的原因（CHALI 侧用 __netStatus 取）。 */
constexpr int kNetOk = 0;
constexpr int kNetClosed = 1;  /* 对端关闭 */
constexpr int kNetTimeout = 2; /* 等超时 */
constexpr int kNetError = 3;
constexpr int kNetPending = 4;

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

/**
 * @brief 取得网络胶水独占的套接字槽表。
 * @return 进程内持久保存的可写槽表。
 */
inline std::vector<SockSlot> &sockets()
{
    static std::vector<SockSlot> instance(kMaxSockets);
    return instance;
}

/**
 * @brief 取得最近一次网络操作的状态槽。
 * @return 可供网络操作更新的状态引用。
 */
inline int &lastStatus()
{
    static int instance = kNetOk;
    return instance;
}

/**
 * @brief 校验网络句柄并读取仍有效的套接字槽。
 * @param handle 当前模块中的资源句柄。
 * @return 有效槽地址；句柄无效或已关闭时返回空。
 */
inline SockSlot *slotAt(std::int32_t handle)
{
    if (handle < 0 || handle >= kMaxSockets) return nullptr;
    SockSlot &slot = sockets()[static_cast<std::size_t>(handle)];
    return slot.used ? &slot : nullptr;
}

/**
 * @brief 复用空闲槽或追加槽以保存新套接字。
 * @param listener 监听句柄或需要保存的监听状态标志。
 * @param fd 宿主套接字文件描述符。
 * @return 用于语言侧访问该套接字的句柄。
 */
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

/**
 * @brief 按当前平台检查套接字的无效哨兵值。
 * @param fd 宿主套接字文件描述符。
 * @return 描述符无效时为 true。
 */
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

/**
 * @brief 向平台 select 集合加入描述符并更新最大编号。
 * @param set 需要加入描述符的平台集合。
 * @param fd 宿主套接字文件描述符。
 * @param maxFd 输入并更新当前最大描述符编号。
 */
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

/**
 * @brief 按当前平台布局读取解析结果中的套接字地址。
 * @param info 当前平台名称解析链表中的节点。
 * @return 地址结构的借用指针。
 */
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

/**
 * @brief 按当前平台布局读取名称解析链表的下一节点。
 * @param info 当前平台名称解析链表中的节点。
 * @return 下一节点地址；链尾返回空。
 */
inline const void *addrInfoNext(const void *info)
{
    void *value = nullptr;
    std::memcpy(&value, static_cast<const char *>(info) + 40, sizeof(void *));
    return value;
}

/**
 * @brief 取得指定地址族对应的套接字地址结构长度。
 * @param family 系统套接字地址族。
 * @return 适用于 IPv4 或 IPv6 系统调用的字节数。
 */
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
        const unsigned short networkPort = api().hostToNetwork16(static_cast<unsigned short>(port));
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
        const unsigned short networkPort = api().hostToNetwork16(static_cast<unsigned short>(port));
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

/**
 * @brief 复制最近一次网络胶水诊断。
 * @return 最近一次保存的错误文本。
 */
inline String netError()
{
    return String(errorText());
}

/**
 * @brief 读取最近一次网络操作的可恢复状态。
 * @return 语言层 Net 状态协议使用的状态码。
 */
inline std::int32_t netStatus()
{
    return lastStatus();
}

/**
 * @brief 解析主机并在指定期限内建立 TCP 连接。
 * @param host 主机名或地址文本。
 * @param port 目标或本地 TCP 端口号。
 * @param timeoutMs 允许等待的毫秒数，按模块约定处理无限等待。
 * @return 连接句柄；失败返回无效句柄并保存状态。
 */
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

/**
 * @brief 解析本地地址并创建 TCP 监听套接字。
 * @param host 主机名或地址文本。
 * @param port 目标或本地 TCP 端口号。
 * @param backlog 监听队列的容量上限。
 * @return 监听句柄；失败返回无效句柄并保存状态。
 */
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

/**
 * @brief 在指定期限内从监听器接收一个 TCP 连接。
 * @param listener 监听句柄或需要保存的监听状态标志。
 * @param timeoutMs 允许等待的毫秒数，按模块约定处理无限等待。
 * @return 新连接句柄；超时或失败返回无效句柄。
 */
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

/**
 * @brief 发送字符串字节并保留短写及网络错误状态。
 * @param sock 待发送数据的连接句柄。
 * @param data 包含待发送原始字节的字符串。
 * @return 实际发送字节数；失败返回负值。
 */
inline std::int32_t netSend(std::int32_t sock, String data)
{
    lastStatus() = kNetOk;
    SockSlot *slot = slotAt(sock);
    if (slot == nullptr || slot->listener)
    {
        lastStatus() = kNetError;
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
            lastStatus() = kNetError;
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

/**
 * @brief 释放网络句柄对应的套接字并清空槽。
 * @param handle 当前模块中的资源句柄。
 * @return 关闭操作的状态码。
 */
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
    return static_cast<std::int32_t>(api().hostToNetwork16(networkPort));
#else
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    if (::getsockname(static_cast<int>(slot->fd), reinterpret_cast<struct sockaddr *>(&address), &length) != 0) return 0;
    return static_cast<std::int32_t>(ntohs(address.sin_port));
#endif
}

/**
 * @brief 持有一项平台连接及该连接最近一次操作的诊断。
 */

/**
 * @brief 持有平台连接及该连接的独立状态、错误和连接超时。
 */
struct SocketResource
{
    std::int32_t slot = -1;
    String error;
    std::int64_t status = kNetOk;
    bool connecting = false;
    bool timed = false;
    std::chrono::steady_clock::time_point deadline;
};

/**
 * @brief 销毁独占套接字资源，尚未关闭时同时归还系统句柄。
 * @param pointer 待销毁的 SocketResource。
 */
inline void destroySocketResource(void *pointer)
{
    auto *resource = static_cast<SocketResource *>(pointer);
    if (resource->slot >= 0) netClose(resource->slot);
    delete resource;
}

inline const stackvm::NativeRecordType socketType{ "std::net::SocketResource", nullptr, destroySocketResource };

/**
 * @brief 将平台连接收进当前 VM 的不透明记录。
 * @param ctx 当前胶水调用上下文。
 * @param slot 平台连接表位置，负值表示未连接。
 * @return 独占记录句柄。
 */
inline std::int64_t makeSocketResource(chali_glue::Context &ctx, std::int32_t slot)
{
    auto *resource = new SocketResource;
    resource->slot = slot;
    resource->status = lastStatus();
    if (slot < 0) resource->error = netError();
    return ctx.makeNativeRecord(socketType, resource);
}

/**
 * @brief 校验并借用当前运行时拥有的套接字资源。
 * @param ctx 当前胶水调用上下文。
 * @param handle 不透明记录句柄。
 * @return 有效资源；已经释放或类型不匹配时报告错误并返回空。
 */
inline SocketResource *socketResource(chali_glue::Context &ctx, std::int64_t handle)
{
    auto *resource = static_cast<SocketResource *>(ctx.nativeRecord(handle, socketType));
    if (!resource) ctx.fail("网络 opcode 已释放或类型不匹配");
    return resource;
}

/**
 * @brief 保存本次平台操作的结果，避免其他连接覆盖该连接的状态。
 * @param resource 刚完成操作的连接。
 */
inline void saveSocketStatus(SocketResource &resource)
{
    resource.status = lastStatus();
    if (resource.status == kNetError) resource.error = netError();
}

/**
 * @brief 检查平台错误是否表示非阻塞操作尚未就绪。
 * @return 尚未就绪或正在连接时返回 true。
 */
inline bool wouldBlock()
{
    const int code = api().last_error ? api().last_error() : 0;
#if defined(_WIN32)
    return code == 10035 || code == 10036 || code == 10037;
#else
    return code == EWOULDBLOCK || code == EAGAIN || code == EINPROGRESS || code == EALREADY;
#endif
}

/**
 * @brief 保存异步操作错误，不将可恢复网络错误升级为 VM panic。
 * @param resource 操作所属连接。
 * @param message 错误说明。
 * @return 通用失败数值 -1。
 */
inline std::int64_t asyncError(SocketResource &resource, const String &message)
{
    resource.status = kNetError;
    resource.error = message;
    return -1;
}

/**
 * @brief 创建非阻塞 IPv4 连接，地址解析仅接受数值 IP。
 * @param ctx 当前胶水上下文。
 * @param host 数值 IPv4 地址。
 * @param port 远端端口。
 * @param timeoutMs 正数表示连接超时毫秒数，非正数表示无内建超时。
 * @return 由当前 VM 拥有的连接记录，status 表示完成、进行中或失败。
 */
inline std::int64_t connectAsync(chali_glue::Context &ctx, String host, std::int32_t port, std::int32_t timeoutMs)
{
    auto handle = makeSocketResource(ctx, -1);
    auto *resource = socketResource(ctx, handle);
    if (!resource) return 0;
    resource->error.clear();
    if (!initNet())
    {
        asyncError(*resource, netError());
        return handle;
    }
    if (!numericHost(host) || port <= 0 || port > 65535)
    {
        asyncError(*resource, "connectAsync 需要数值 IPv4 地址及 1..65535 端口；域名请使用 connect");
        return handle;
    }
    unsigned char address[kAddrBuffer]{};
    int length = resolveAddress(host, port, address);
    if (!length)
    {
        asyncError(*resource, netError());
        return handle;
    }
    auto fd = api().create(2, 1, 6);
    if (isInvalid(fd))
    {
        captureSocketError("创建异步连接");
        asyncError(*resource, netError());
        return handle;
    }
    if (!setNonBlocking(fd, true))
    {
        captureSocketError("设置非阻塞连接");
        api().close(fd);
        asyncError(*resource, netError());
        return handle;
    }
    int result = api().connect(fd, address, length);
    const bool pending = result != 0 && wouldBlock();
    if (result != 0 && !pending)
    {
        captureSocketError("异步连接");
        api().close(fd);
        asyncError(*resource, netError());
        return handle;
    }
    auto slot = allocateSlot(false, fd);
    if (slot < 0)
    {
        api().close(fd);
        asyncError(*resource, netError());
        return handle;
    }
    resource->slot = slot;
    resource->connecting = pending;
    resource->status = pending ? kNetPending : kNetOk;
    resource->timed = timeoutMs > 0;
    resource->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs : 0);
    if (!pending && !setNonBlocking(fd, false))
    {
        netClose(slot);
        resource->slot = -1;
        asyncError(*resource, "恢复连接模式失败");
    }
    return handle;
}

/**
 * @brief 以零等待轮询异步连接，完成后恢复同步 API 可用的连接模式。
 * @param resource 正在连接或已经完成的连接记录。
 * @return NET_PENDING、NET_OK、NET_TIMEOUT 或 NET_ERROR。
 */
inline std::int64_t pollConnect(SocketResource &resource)
{
    if (!resource.connecting) return resource.status;
    auto *slot = slotAt(resource.slot);
    if (!slot)
    {
        resource.connecting = false;
        return asyncError(resource, "异步连接已经关闭"), resource.status;
    }
    int ready = waitWritable(slot->fd, 0);
    if (ready == 0 && (!resource.timed || std::chrono::steady_clock::now() < resource.deadline)) return resource.status = kNetPending;
    int pending = 0, size = sizeof(pending);
    const bool failed = ready < 0 || (ready > 0 && (!api().getopt || api().getopt(slot->fd, kSolSocket, kSoError, &pending, &size) != 0 || pending != 0));
    resource.connecting = false;
    if (ready == 0 || failed || !setNonBlocking(slot->fd, false))
    {
        netClose(resource.slot);
        resource.slot = -1;
        resource.status = ready == 0 ? kNetTimeout : kNetError;
        resource.error = ready == 0 ? "异步连接超时" : "异步连接失败（套接字错误码 " + Memory::toString(pending) + "）";
        return resource.status;
    }
    resource.error.clear();
    return resource.status = kNetOk;
}

/**
 * @brief 以零等待接受一条连接，不就绪时保留监听器并返回 pending 状态记录。
 * @param ctx 当前胶水上下文。
 * @param resource 监听器记录。
 * @return 新连接或 pending/错误结果的记录句柄。
 */
inline std::int64_t acceptAsync(chali_glue::Context &ctx, SocketResource &resource)
{
    auto *slot = slotAt(resource.slot);
    if (!slot || !slot->listener)
    {
        asyncError(resource, "不是有效监听器");
        auto handle = makeSocketResource(ctx, -1);
        auto *result = socketResource(ctx, handle);
        if (result)
        {
            result->status = resource.status;
            result->error = resource.error;
        }
        return handle;
    }
    if (!setNonBlocking(slot->fd, true))
    {
        asyncError(resource, "设置非阻塞监听失败");
        auto handle = makeSocketResource(ctx, -1);
        auto *result = socketResource(ctx, handle);
        if (result)
        {
            result->status = resource.status;
            result->error = resource.error;
        }
        return handle;
    }
    const auto accepted = netAccept(resource.slot, 0);
    const auto status = accepted >= 0 ? kNetOk : lastStatus() == kNetTimeout || wouldBlock() ? kNetPending :
                                                                                               kNetError;
    setNonBlocking(slot->fd, false);
    const auto handle = makeSocketResource(ctx, accepted);
    auto *result = socketResource(ctx, handle);
    resource.status = status;
    if (result)
    {
        result->status = status;
        if (status == kNetPending) result->error.clear();
    }
    if (status == kNetError) resource.error = netError();
    return handle;
}

/**
 * @brief 执行一次有界非阻塞发送，保留部分发送的字节数。
 * @param resource 连接记录。
 * @param data 完整待发送文本。
 * @param offset 本次发送在文本中的字节偏移。
 * @return 本次发送字节数；未就绪为零，失败为 -1。
 */
inline std::int64_t sendAsync(SocketResource &resource, const String &data, std::int64_t offset)
{
    if (resource.connecting && pollConnect(resource) != kNetOk) return resource.status == kNetPending ? 0 : -1;
    auto *slot = slotAt(resource.slot);
    if (!slot || slot->listener) return asyncError(resource, "不是有效连接");
    if (offset < 0 || static_cast<std::uint64_t>(offset) > data.size()) return asyncError(resource, "异步发送偏移越界");
    resource.status = kNetOk;
    if (static_cast<std::size_t>(offset) == data.size()) return 0;
    if (!setNonBlocking(slot->fd, true)) return asyncError(resource, "设置非阻塞发送失败");
    const int count = static_cast<int>((std::min)(data.size() - static_cast<std::size_t>(offset), std::size_t{ 65536 }));
    int flags = 0;
#if defined(MSG_NOSIGNAL)
    flags = MSG_NOSIGNAL;
#endif
    const int sent = api().send(slot->fd, data.data() + offset, count, flags);
    const bool pending = sent < 0 && wouldBlock();
    if (sent < 0 && !pending) captureSocketError("异步发送");
    setNonBlocking(slot->fd, false);
    if (pending)
    {
        resource.status = kNetPending;
        return 0;
    }
    if (sent <= 0) return asyncError(resource, netError());
    return sent;
}

/**
 * @brief 读取一次非阻塞网络数据或交出已有行缓冲中的字节。
 * @param resource 连接记录。
 * @param maxBytes 字节读取上限，每次最多 65536。
 * @param line true 表示等待完整行，false 表示读取任意字节。
 * @return 本次结果；空串通过 status 区分 pending、关闭、空行或错误。
 */
inline String receiveAsync(SocketResource &resource, std::int32_t maxBytes, bool line)
{
    if (resource.connecting && pollConnect(resource) != kNetOk) return {};
    auto *slot = slotAt(resource.slot);
    if (!slot || slot->listener)
    {
        asyncError(resource, "不是有效连接");
        return {};
    }
    if (maxBytes <= 0)
    {
        asyncError(resource, "异步接收长度必须为正");
        return {};
    }
    resource.status = kNetOk;
    bool closed = false;
    const auto readyLine = [&]() { return slot->pending.find('\n') != std::string::npos; };
    if ((line && !readyLine()) || (!line && slot->pending.empty()))
    {
        if (!setNonBlocking(slot->fd, true))
        {
            asyncError(resource, "设置非阻塞接收失败");
            return {};
        }
        char bytes[65536];
        int count = api().recv(slot->fd, bytes, line ? 4096 : (std::min)(maxBytes, std::int32_t{ 65536 }), 0);
        const bool pending = count < 0 && wouldBlock();
        if (count < 0 && !pending) captureSocketError("异步接收");
        setNonBlocking(slot->fd, false);
        if (pending)
        {
            resource.status = kNetPending;
            return {};
        }
        if (count < 0)
        {
            asyncError(resource, netError());
            return {};
        }
        closed = count == 0;
        if (count > 0) slot->pending.append(bytes, static_cast<std::size_t>(count));
    }
    if (line)
    {
        const auto newline = slot->pending.find('\n');
        if (newline == std::string::npos && !closed)
        {
            if (slot->pending.size() > 1048576)
            {
                slot->pending.clear();
                asyncError(resource, "异步行缓冲超过 1 MiB");
                return {};
            }
            resource.status = kNetPending;
            return {};
        }
        if (newline != std::string::npos)
        {
            auto result = slot->pending.substr(0, newline);
            slot->pending.erase(0, newline + 1);
            if (!result.empty() && result.back() == '\r') result.pop_back();
            return String(result);
        }
    }
    const auto count = (std::min)(slot->pending.size(), static_cast<std::size_t>(maxBytes));
    auto result = slot->pending.substr(0, count);
    slot->pending.erase(0, count);
    if (closed && result.empty()) resource.status = kNetClosed;
    return String(result);
}

/**
 * @brief 注册网络 opcode 的独占生命周期及所有连接操作。
 * @param registry 宿主函数注册表。
 * @param error 注册失败时接收诊断。
 * @return 全部入口注册成功时返回 true。
 */
bool bind(Registry &registry, String *error)
{
    using Context = chali_glue::Context;
    bool ok = registry.bindTypedWithContext<std::int64_t>("std::net::__std_netNew", [](Context &ctx) { return makeSocketResource(ctx, -1); }, error);
    ok = registry.bindTypedWithContext<void, std::int64_t>("std::net::__std_netDrop", [](Context &ctx, std::int64_t handle) { ctx.releaseNativeRecord(handle); }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, String, std::int32_t, std::int32_t>("std::net::__std_netConnect", [](Context &ctx, String host, std::int32_t port, std::int32_t timeout) { return makeSocketResource(ctx, netConnect(host, port, timeout)); }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, String, std::int32_t, std::int32_t>("std::net::__std_netListen", [](Context &ctx, String host, std::int32_t port, std::int32_t backlog) { return makeSocketResource(ctx, netListen(host, port, backlog)); }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int32_t>("std::net::__std_netAccept", [](Context &ctx, std::int64_t reference, std::int32_t timeout) {
        auto *resource = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType));
        if (!resource) return std::int64_t{};
        auto slot = netAccept(resource->slot, timeout);
        saveSocketStatus(*resource);
        return makeSocketResource(ctx, slot); }, error) && ok;
    ok = registry.bindTypedWithContext<bool, std::int64_t>("std::net::__std_netIsOpen", [](Context &ctx, std::int64_t handle) { auto *r = socketResource(ctx, handle); return r && r->slot >= 0 && !r->connecting; }, error) && ok;
    ok = registry.bindTypedWithContext<String, std::int64_t>("std::net::__std_netError", [](Context &ctx, std::int64_t handle) { auto *r = socketResource(ctx, handle); return r ? r->error : String{}; }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::net::__std_netStatus", [](Context &ctx, std::int64_t handle) { auto *r = socketResource(ctx, handle); return r ? r->status : kNetError; }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t>("std::net::__std_netCloseSocket", [](Context &ctx, std::int64_t reference) { auto *r = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); if (r && r->slot >= 0) { netClose(r->slot); r->slot = -1; r->connecting = false; r->status = kNetClosed; } }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t>("std::net::__std_netCloseListener", [](Context &ctx, std::int64_t reference) { auto *r = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); if (r && r->slot >= 0) { netClose(r->slot); r->slot = -1; r->connecting = false; r->status = kNetClosed; } }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::net::__std_netLocalPort", [](Context &ctx, std::int64_t handle) { auto *r = socketResource(ctx, handle); return r ? static_cast<std::int64_t>(netLocalPort(r->slot)) : 0; }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t, String>("std::net::__std_netSend", [](Context &ctx, std::int64_t reference, String data) { auto *r = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); if (!r) return std::int64_t{-1}; auto count = netSend(r->slot, data); saveSocketStatus(*r); return static_cast<std::int64_t>(count); }, error) && ok;
    ok = registry.bindTypedWithContext<String, std::int64_t, std::int32_t>("std::net::__std_netReceive", [](Context &ctx, std::int64_t reference, std::int32_t count) { auto *r = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); if (!r) return String{}; auto text = netReceive(r->slot, count); saveSocketStatus(*r); return text; }, error) && ok;
    ok = registry.bindTypedWithContext<String, std::int64_t, std::int32_t>("std::net::__std_netReceiveLine", [](Context &ctx, std::int64_t reference, std::int32_t timeout) { auto *r = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); if (!r) return String{}; auto text = netReceiveLine(r->slot, timeout); saveSocketStatus(*r); return text; }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, String, std::int32_t, std::int32_t>("std::net::__std_netConnectAsync", connectAsync, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::net::__std_netPollConnect", [](Context &ctx, std::int64_t reference) { auto *resource = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); return resource ? pollConnect(*resource) : std::int64_t{kNetError}; }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::net::__std_netAcceptAsync", [](Context &ctx, std::int64_t reference) { auto *resource = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); return resource ? acceptAsync(ctx, *resource) : 0; }, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t, String, std::int64_t>("std::net::__std_netSendAsync", [](Context &ctx, std::int64_t reference, String data, std::int64_t offset) { auto *resource = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); return resource ? sendAsync(*resource, data, offset) : std::int64_t{-1}; }, error) && ok;
    ok = registry.bindTypedWithContext<String, std::int64_t, std::int32_t>("std::net::__std_netReceiveAsync", [](Context &ctx, std::int64_t reference, std::int32_t count) { auto *resource = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); return resource ? receiveAsync(*resource, count, false) : String{}; }, error) && ok;
    ok = registry.bindTypedWithContext<String, std::int64_t>("std::net::__std_netReceiveLineAsync", [](Context &ctx, std::int64_t reference) { auto *resource = socketResource(ctx, native_record_glue::referenceHandle(ctx, reference, socketType)); return resource ? receiveAsync(*resource, 1048576, true) : String{}; }, error) && ok;
    return ok;
}

}  // namespace std_net_glue
