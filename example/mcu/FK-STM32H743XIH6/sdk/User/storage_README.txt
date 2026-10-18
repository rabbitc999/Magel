W25Q64 / FatFs 调试说明

仅启用 W25Q64（逻辑盘 0:）。已删除板级 SDMMC 驱动和 SD 卡磁盘分支，
移除 CMake/MDK 的 SD、SDIO、MMC 编译项及 HAL 使能。
芯片厂商原始 SDK 中的寄存器定义和未参与编译的驱动文件保留。
USB 仅清理其 SD 卡分支，本轮未做 USB 功能测试。

W25Q64 总容量 8 MiB；文件系统从 0x00247000 开始。
前面的字体、参数区不属于文件系统。逻辑扇区 4096 字节，擦除块为 1 个逻辑扇区。
FatFs 的跨扇区写由 4 KiB 逻辑扇区读改写处理；直接调用 W25QXX_WriteBuffer 会擦除覆盖的物理扇区。
该底层接口没有磨损均衡，也不能保证突然断电时 FAT 元数据的原子更新。

USART1 字符串命令：
  storage info
    读取 JEDEC、QSPI 时钟、磁盘几何及空闲容量。
  storage bounds
    验证越界、零长度及空指针请求被磁盘层拒绝，不写入介质。
  storage guard
    只读扫描文件系统前面的保留区并计算 CRC32，约 30 秒。
  storage test 0:/NEWTEST.BIN
    仅以 FA_CREATE_NEW 创建文件，已有同名文件会拒绝。
    写入 12345 字节、关闭后回读，再在偏移 4093 覆盖 513 字节、追加 521 字节。
    再次关闭后逐字节核验整个 12866 字节文件，包括覆盖范围前后的原数据。
    使用非对齐调用缓冲区，覆盖 256 字节页边界与 4096 字节扇区边界。
    测试完成保留文件，可重启后验证。
  storage verify 0:/NEWTEST.BIN
    只读核验上述测试格式，适合复位或重新上电后执行。
  rm 0:/NEWTEST.BIN
    删除自己创建的测试文件。

测试使用 Main_Task 分步调度，不在单个命令回调里完成全部文件操作。
单次闪存擦除/写入仍为现有轮询驱动，通过 max_step_ms 记录实际阻塞时长。
运行期间不要对测试文件执行其他命令；它会占用一个 FatFs 文件锁槽位。

诊断变量：g_StoragePhase、g_StorageOffset、g_StorageResult、g_StorageMaxStepMs、g_StorageGuardCRC。
phase 为 0 表示空闲，result 为 FRESULT；运行中为 0xFFFFFFFF。

主机验证：python Tests/h7tool/test_storage.py
  使用实际 ff.c、diskio.c 和 storage.c，底层 QSPI 使用 RAM 模型。
板上验证：python Tests/h7tool/live_storage_test.py
  依赖 H7TOOL、USART1 和当前 ELF；创建独立测试文件，重启核验后删除自己创建的文件。
  此脚本不测试 SDMMC 或 USB。

若串口物理链路不通，可执行 python Tests/h7tool/live_storage_swd_test.py。
它使用 SWD 向前台软件 FIFO 提交命令并唤醒接收任务，由正常任务执行存储操作。
该方式验证实际 W25Q64/FatFs，不代表 UART 物理收发或 RX DMA 已通过。
本次板上结果保存在 Tests/h7tool/results/w25q64/swd_result.json。
实测 JEDEC EF4017，QSPI 120 MHz；文件跨扇区覆盖、追加及重启后回读均通过。
保留区 2387968 字节测试前后 CRC32 一致。写入测试单次步骤最大 182 ms。
