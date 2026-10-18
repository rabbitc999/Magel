二进制文件传输（HEX 命令 0x12）

用途：向 W25Q64 的 0: 文件系统上传文件、下载文件和查询大小。
保持已有 HEX 外层帧、64 位毫秒发送时间戳以及独立字符串命令。
数据块支持任意二进制字节，不以换行符或 00 结束；最大块长 512 字节。
路径目前支持 ASCII，必须以 0:/ 开头，不接受 ..、重复 /、控制字符和通配符。

外层帧：55 AA 12 FUNC LEN_BE16 TIME_BE64 PAYLOAD CRC32_BE FF FE。
CRC32 覆盖 CMD 到 PAYLOAD，标准 reflected CRC32（Python zlib.crc32）。
0x12 文件指令始终验证外层 CRC，不受旧的宽松解析开关影响。
电脑发送时间戳填 0；设备应答携带当前 64 位毫秒时间戳。
所有整数字段均为大端。

每个请求首先携带 request_id:uint32。应答 FUNC = 请求 FUNC | 0x80。
每个应答首先携带 request_id:uint32、status:uint8、FatFs_result:uint8。
成功时 status/result 为 0/0；错误时不携带后续成功数据。

功能与 request_id 后面的请求字段：
  0 STAT       path（剩余字节，不附加 NUL）
    成功数据：size:uint32、attributes:uint8。
  1 PUT_BEGIN  size:uint32、whole_crc32:uint32、path
    成功数据：session_id:uint32、size:uint32、max_chunk:uint16。
  2 GET_BEGIN  path
    成功数据：session_id:uint32、size:uint32、max_chunk:uint16。
  3 PUT_CHUNK  session_id:uint32、offset:uint32、chunk_crc32:uint32、data
    成功数据：next_offset:uint32。数据长度必须在 1..512，按顺序发送。
  4 GET_CHUNK  session_id:uint32、offset:uint32、count:uint16
    成功数据：session_id:uint32、offset:uint32、chunk_crc32:uint32、data。
    请求超出剩余长度会返回剩余数据；文件末尾返回空数据及空数据 CRC。
  5 CLOSE      session_id:uint32
    上传：核验完整长度、完整 CRC，sync/close 后将临时文件改为目标名。
    下载：关闭文件。成功数据为 size:uint32。
  6 ABORT      session_id:uint32
    关闭会话；上传时删除本会话创建的临时文件。成功数据为空。

status：0 成功、1 格式错误、2 忙、3 会话无效、4 偏移错误、5 CRC 错误、
6 长度错误、7 文件系统错误（详细原因见 FatFs_result）。

采用停等传输：收到一个应答后再发送下一个请求。
超时重发时保持整个请求和 request_id 不变；最后一次请求的应答会缓存，
重复 PUT_CHUNK/CLOSE 不会重复写入/提交，发送 FIFO 空间不足时可重试。
同一时间只允许一个文件会话，并绑定发起串口。
30 秒无请求会关闭文件并释放锁；未完成上传的 .part 留在设备上，不发布目标文件。
后续可通过已有 rm 命令删除该临时文件再重新上传；当前不提供断点续传。

上传默认拒绝覆盖：目标路径存在，或 path.part 已存在，均返回 FR_EXIST。
只在完整校验成功后改为目标名，普通 FAT 元数据更新没有突然断电的原子性保证。
下载时使用 FatFs 文件锁；传输期间其他打开文件操作可能因唯一锁槽位而被拒绝。
静态会话工作区占用约 1.4 KiB，发送帧使用命令框架的自定义分配器。
未将整个文件加载到 RAM；文件擦写仍为现有同步 QSPI 驱动。

电脑端：Tests/h7tool/file_transfer.py，依赖 pyserial。
  python -m pip install pyserial
  python Tests/h7tool/file_transfer.py --port COM5 stat 0:/config/example.bin
  python Tests/h7tool/file_transfer.py --port COM5 put ./example.bin 0:/example.bin
  python Tests/h7tool/file_transfer.py --port COM5 get 0:/example.bin ./download.bin
默认 115200、8N1；COM5 是示例，请改为实际直连板子的串口号。
H7TOOL 的 HID 接口不是 COM 口；调试器通道不应当填作 --port。
电脑端上传会预先计算完整 CRC；下载先写本地 .part，成功后改为目标名。
电脑端已有目标文件也会拒绝覆盖。传输失败时本地 .part 保留，避免误认为完整文件。

验证：
  python Tests/h7tool/test_file_transfer.py
  python Tests/h7tool/test_file_transfer_client.py
  python Tests/h7tool/test_uart_hex.py
  python Tests/h7tool/live_file_transfer_swd_test.py
最后一项使用 SWD 向前台 FIFO 注入实际 HEX 帧，从真实 UART TX DMA 工作区读回应答。
它验证真实 W25Q64、FatFs、C++ 命令框架和电脑客户端，不代表 UART 接线/RX DMA 验证。
USB、SDMMC 不在本功能范围内。
