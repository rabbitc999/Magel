串口命令框架

接收链路：循环 DMA -> 软件 FIFO -> UART_ProcessRx -> C++ Runtime。
HEXParser 和 StringParser 使用独立状态机、缓冲区和回调，可以分别编译链接。
cmd.c 保留 C 接口和现有业务命令注册，解析核心采用 C++17，无异常、RTTI 或 STL。

内存：Middleware/Allocator/pool.h 是与 GUI 共用的分配器实现。
命令模块使用独立的 64 KiB 静态内存池，不与 GUI 共享池实例。
帧、字符串和发送临时缓冲区通过 Command::Allocator 分配并在完成或超时后释放。
回调中的数据仅在回调执行期间有效；需要异步使用时必须自行复制。

每个串口具有独立解析状态，默认 CMD_PROTOCOL_AUTO。
CMD_RuntimeSetProtocol(COM1, CMD_PROTOCOL_HEX) 可设置纯 HEX 模式；
CMD_PROTOCOL_STRING 为纯字符串，CMD_PROTOCOL_DISABLED 停止解析。
切换协议会释放未完成的输入。AUTO 中已识别的 HEX 帧不会转交字符串解析器。
前台任务调用 CMD_RuntimePoll() 清理 1000 ms 内未完成的帧，使用 HAL_GetTick。
接口仅用于前台协作任务，不能在中断或回调中重新初始化、切换模式或嵌套解析。

HEX 格式保持：55 AA CMD FUNC LEN_BE16 TIME_BE64 DATA CRC32_BE FF FE。
时间戳单位毫秒；设备发送自动携带定时器时间，设置时间命令 0x02 使用帧头时间，数据为空。
CRC32 覆盖 CMD 到 DATA。最大接收数据长度 4084 字节。
字符串支持 LF、CRLF、引号参数和原有命令处理函数；过长行整体丢弃。

发送接口要求传输层同步复制全部字节，空间不足则整帧拒绝。
当前 USART1 TX FIFO 为 2048 字节，保留一个空位，因此单帧最大 2047 字节；
该限制与接收最大长度不同，调用方应分块发送或在之后重试。
g_CMD_RuntimeStats 提供使用量、峰值、分配失败、超时、CRC 和发送拒绝计数。

UART 接收工作缓冲区与 printf 格式化缓冲区独立，防止命令输出覆盖同一批后续输入。

验证命令（仓库父目录 G:/Desktop/REMOTE_CAR）：
python Tests/h7tool/test_uart_hex.py
python Tests/h7tool/test_command_parsers_independent.py
python Tests/h7tool/live_uart_test.py
python Tests/h7tool/live_command_runtime_test.py
最后两项依赖连接 USART1 的 H7TOOL 和已烧录的对应 ELF。
