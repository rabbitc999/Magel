#include "magel_board.h"
#include "board_io.h"
#include "ff.h"
#include "stackvm.hpp"
#include "mcu_arenas.hpp"
#include "mcu_console.hpp"
#include "mcu_filesystem.hpp"
#include "mcu_glue.hpp"
#include "mcu_metadata_heap.hpp"
#include "builtin_program.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief 供 SWD 读取的单次实机样本，所有字段使用定宽 32 位小端表示。
 */
struct MagelBenchSample
{
    std::uint32_t iterations, vmCycles, nativeCycles, checksum;
};
/**
 * @brief 不依赖串口接线的性能测试快照；count 最后发布，status 为最终执行状态。
 */
volatile struct MagelBenchSnapshot
{
    std::uint32_t magic, clockHz, count, status;
    MagelBenchSample samples[5];
} g_MagelBenchmark __attribute__((aligned(32)));
/**
 * @brief 供 SWD 复核程序退出及 panic 后已返回选择页的最近三次记录。
 */
volatile struct MagelUiTrace
{
    std::uint32_t magic, completed, statuses[3], programs[3];
} g_MagelUiTrace __attribute__((aligned(32)));
}

namespace
{
using Args = Memory::Vector<stackvm::NativeValue>;
using Context = stackvm::NativeCallContext;
using Value = stackvm::NativeValue;
constexpr std::uint32_t metadataBytes = 2u * 1024u * 1024u;
constexpr std::uint32_t runtimeBytes = 16u * 1024u * 1024u;
constexpr std::uint32_t bytePoolBytes = 4u * 1024u * 1024u;
FATFS filesystem;
struct ProgramEntry
{
    char name[80];
    unsigned builtin;
};
ProgramEntry entries[32];
unsigned entryCount;

/**
 * @brief 枚举 FatFS 启动目录中的字节码文件，并始终保留内置测试入口。
 */
void ScanPrograms()
{
    entryCount = 3;
    std::snprintf(entries[0].name, sizeof(entries[0].name), "Built-in benchmark");
    entries[0].builtin = 1;
    std::snprintf(entries[1].name, sizeof(entries[1].name), "Built-in hello / exit");
    entries[1].builtin = 2;
    std::snprintf(entries[2].name, sizeof(entries[2].name), "Built-in panic test");
    entries[2].builtin = 3;
    DIR directory;
    if (f_opendir(&directory, "0:/magel") != FR_OK) return;
    FILINFO info;
    while (entryCount < 32 && f_readdir(&directory, &info) == FR_OK && info.fname[0])
    {
        const std::size_t length = std::strlen(info.fname);
        if ((info.fattrib & AM_DIR) || length <= 5 || length >= sizeof(entries[0].name)) continue;
        const char *extension = info.fname + length - 5;
        if (std::strcmp(extension, ".svmb") && std::strcmp(extension, ".SVMB")) continue;
        bool ascii = true;
        for (std::size_t i = 0; i < length; ++i)
            if (static_cast<unsigned char>(info.fname[i]) < 32 || static_cast<unsigned char>(info.fname[i]) > 126) ascii = false;
        if (!ascii) continue;
        auto &entry = entries[entryCount++];
        std::memcpy(entry.name, info.fname, length - 5);
        entry.name[length - 5] = 0;
        entry.builtin = 0;
    }
    f_closedir(&directory);
}

/**
 * @brief 用 BSP 字体绘制经过截断的菜单文本，避免自动换行越界。
 * @param y 行起始纵坐标。
 * @param text 可打印 ASCII 文本。
 * @param color RGB565 字体颜色。
 */
void MenuText(unsigned y, const char *text, std::uint16_t color)
{
    if (y + 16 > FkLcdHeight()) return;
    for (unsigned i = 0; text[i] && 8 + (i + 1) * 8 <= FkLcdWidth(); ++i)
        FkLcdChar(8 + i * 8, y, text[i], color, 0);
}

/**
 * @brief 绘制分页程序选择列表与最近一次运行状态。
 * @param selected 当前选择的程序索引。
 * @param lastStatus 最近运行状态，负数表示尚未运行。
 */
void DrawMenu(unsigned selected, int lastStatus)
{
    FkLcdFill(0, 0, FkLcdWidth(), FkLcdHeight(), 0);
    MenuText(8, "Magel program selector", 65535);
    MenuText(28, "UP/DOWN: select  ENTER: run", 65504);
    MenuText(48, "MODE: rescan  0:/magel/*.svmb", 65504);
    const unsigned rows = FkLcdHeight() > 110 ? (FkLcdHeight() - 110) / 20 : 1;
    const unsigned first = (selected / rows) * rows;
    for (unsigned i = first; i < entryCount && i < first + rows; ++i)
    {
        char label[84];
        std::snprintf(label, sizeof(label), "%c %s", i == selected ? '>' : ' ', entries[i].name);
        MenuText(76 + (i - first) * 20, label, i == selected ? 2016 : 65535);
    }
    if (lastStatus >= 0)
        MenuText(FkLcdHeight() - 22, lastStatus == 0 ? "Last run: exited OK" : "Last run: panic/error (see USART1)", lastStatus == 0 ? 2016 : 63488);
}

/**
 * @brief 经 USART1 的同步发送路径输出文本，避免 DMA 缓冲尚未排空即结束测试。
 * @param bytes 待输出字节。
 * @param count 字节数量。
 */
void SerialWrite(void *, const char *bytes, std::size_t count)
{
    while (count)
    {
        const auto chunk = static_cast<std::uint16_t>(std::min<std::size_t>(count, 512));
        FkSerialWrite(bytes, chunk);
        bytes += chunk;
        count -= chunk;
    }
}

/**
 * @brief 从 USART1 接收 FIFO 非阻塞读取输入，暂无字节时返回零。
 * @param buffer 接收缓冲区。
 * @param length 缓冲容量。
 * @return 实际取得的字节数。
 */
unsigned SerialRead(void *, unsigned char *buffer, unsigned length)
{
    return FkSerialRead(buffer, length);
}

/**
 * @brief 检查胶水的整数参数范围，失败时记录 VM 原生调用错误。
 * @param context 当前原生调用。
 * @param value 待检查的值。
 * @param maximum 允许的最大值。
 * @return 值在零至 maximum 内时为 true。
 */
bool InRange(Context &context, std::int64_t value, std::int64_t maximum)
{
    if (value >= 0 && value <= maximum) return true;
    context.fail("board argument outside allowed range");
    return false;
}

/**
 * @brief 用 RGB565 颜色清屏。
 * @param context 当前原生调用。
 * @param args 依次为颜色。
 * @return 无返回值接口的占位零。
 */
Value LcdClear(Context &context, const Args &args)
{
    if (InRange(context, args[0].asInt(), 65535))
        FkLcdFill(0, 0, FkLcdWidth(), FkLcdHeight(), args[0].asInt());
    return Value::fromInt(0);
}

/**
 * @brief 在屏幕范围内填充矩形，宽或高为零时不绘制。
 * @param context 当前原生调用。
 * @param args 依次为横坐标、纵坐标、宽、高及 RGB565 颜色。
 * @return 无返回值接口的占位零。
 */
Value LcdFill(Context &context, const Args &args)
{
    const auto x = args[0].asInt(), y = args[1].asInt();
    const auto width = args[2].asInt(), height = args[3].asInt();
    if (InRange(context, x, FkLcdWidth()) && InRange(context, y, FkLcdHeight()) && InRange(context, width, FkLcdWidth() - x) && InRange(context, height, FkLcdHeight() - y) && InRange(context, args[4].asInt(), 65535) && width && height)
        FkLcdFill(x, y, width, height, args[4].asInt());
    return Value::fromInt(0);
}

/**
 * @brief 绘制受长度与屏幕边界约束的单行 ASCII 文本。
 * @param context 当前原生调用。
 * @param args 依次为横纵坐标、字符串句柄、前景色及背景色。
 * @return 无返回值接口的占位零。
 */
Value LcdText(Context &context, const Args &args)
{
    Memory::String text;
    if (!context.readString(args[2].asInt(), text) || text.find('\0') != Memory::String::npos)
    {
        context.fail("lcdText requires a valid text string without NUL");
        return Value::fromInt(0);
    }
    const auto x = args[0].asInt(), y = args[1].asInt();
    if (!InRange(context, x, FkLcdWidth() - 1) || !InRange(context, y, FkLcdHeight() - 1) || !InRange(context, args[3].asInt(), 65535) || !InRange(context, args[4].asInt(), 65535)) return Value::fromInt(0);
    /* 固定 16 像素字体；逐字绘制避免 BSP 自动换行越过屏幕底边。 */
    if (y + 16 > FkLcdHeight())
    {
        context.fail("lcdText exceeds display height");
        return Value::fromInt(0);
    }
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const unsigned char c = text[i];
        if (c < 32 || c > 126)
        {
            context.fail("lcdText currently accepts printable ASCII");
            return Value::fromInt(0);
        }
        if (x + static_cast<std::int64_t>((i + 1) * 8) > FkLcdWidth()) break;
        FkLcdChar(x + i * 8, y, c, args[3].asInt(), args[4].asInt());
    }
    return Value::fromInt(0);
}

/**
 * @brief 取得已初始化屏幕的宽度。
 * @return 水平像素数。
 */
Value LcdWidth(Context &, const Args &) { return Value::fromInt(FkLcdWidth()); }
/**
 * @brief 取得已初始化屏幕的高度。
 * @return 垂直像素数。
 */
Value LcdHeight(Context &, const Args &) { return Value::fromInt(FkLcdHeight()); }

/**
 * @brief 查询物理按键当前是否按下；电平极性由 BSP 处理。
 * @param context 当前原生调用。
 * @param args 按键编号，0 为 MODE，1 为 ENTER，2 为 UP，3 为 DOWN。
 * @return 按下返回一，否则为零。
 */
Value KeyPressed(Context &context, const Args &args)
{
    if (!InRange(context, args[0].asInt(), 3)) return Value::fromInt(0);
    return Value::fromInt(FkKeyPressed(args[0].asInt()));
}

/**
 * @brief 消费按键状态机生成的单击事件。
 * @param context 当前原生调用。
 * @param args 按键编号，范围为零至三。
 * @return 检测到单击为一，否则为零。
 */
Value KeyClicked(Context &context, const Args &args)
{
    if (!InRange(context, args[0].asInt(), 3)) return Value::fromInt(0);
    return Value::fromInt(FkKeyClicked(args[0].asInt()));
}

/**
 * @brief 查询并消费板端 MODE 或 ENTER 的退出请求。
 * @return 实体或虚拟按键请求退出为一，否则为零。
 */
Value BoardExitRequested(Context &, const Args &) { return Value::fromInt(FkExitRequested()); }

/**
 * @brief 使用 BSP 延时等待指定毫秒，硬件中断继续运行。
 * @param context 当前原生调用。
 * @param args 延时毫秒数，范围为零至一千。
 * @return 无返回值接口的占位零。
 */
Value BoardDelay(Context &context, const Args &args)
{
    if (InRange(context, args[0].asInt(), 1000)) FkDelay(args[0].asInt());
    return Value::fromInt(0);
}
/**
 * @brief 读取 DWT 周期计数，调用方按无符号 32 位差值处理一次回绕。
 * @return 当前 32 位计数值。
 */
Value BoardCycles(Context &, const Args &) { return Value::fromInt(FkCycles()); }
/**
 * @brief 读取 TIM6 提供的毫秒时基。
 * @return 启动以来的毫秒数，32 位计数会回绕。
 */
Value BoardMillis(Context &, const Args &) { return Value::fromInt(FkMillis()); }
/**
 * @brief 返回实际配置的内核时钟频率。
 * @return 时钟频率，单位 Hz。
 */
Value BoardClock(Context &, const Args &) { return Value::fromInt(FkClockHz()); }

/**
 * @brief 执行与 Magel 相同的整数递推，作为原生 C++ 性能对照。
 * @param context 当前原生调用。
 * @param args 迭代次数与非负 31 位种子。
 * @return 递推结束后的校验值。
 */
Value NativeInteger(Context &context, const Args &args)
{
    if (!InRange(context, args[0].asInt(), 1000000) || !InRange(context, args[1].asInt(), 2147483647)) return Value::fromInt(0);
    std::int64_t value = args[1].asInt();
    for (std::int64_t i = 0; i < args[0].asInt(); ++i)
        value = (value * 1664525 + 1013904223) & 2147483647;
    return Value::fromInt(value);
}

/**
 * @brief 保存一轮计时与校验值，供串口输出之外的 SWD 离线采样复核。
 * @param context 当前原生调用。
 * @param args 样本序号、迭代数、VM 周期、原生周期及校验值。
 * @return 无返回值接口的占位零。
 */
Value BenchRecord(Context &context, const Args &args)
{
    if (!InRange(context, args[0].asInt(), 4)) return Value::fromInt(0);
    const auto index = static_cast<unsigned>(args[0].asInt());
    auto &sample = g_MagelBenchmark.samples[index];
    sample.iterations = args[1].asInt();
    sample.vmCycles = args[2].asInt();
    sample.nativeCycles = args[3].asInt();
    sample.checksum = args[4].asInt();

    g_MagelBenchmark.count = index + 1;
    FkPublish(&g_MagelBenchmark, sizeof(g_MagelBenchmark));
    return Value::fromInt(0);
}

/**
 * @brief 注册与 programs/board.mgl 对应的外设和计时函数。
 * @param registry 生命周期覆盖整个执行过程的原生函数表。
 */
void RegisterBoard(stackvm::NativeRegistry &registry)
{
    registry.add("lcdClear", 1, LcdClear);
    registry.add("lcdFill", 5, LcdFill);
    registry.add("lcdText", 5, LcdText);
    registry.add("lcdWidth", 0, LcdWidth);
    registry.add("lcdHeight", 0, LcdHeight);
    registry.add("keyPressed", 1, KeyPressed);
    registry.add("keyClicked", 1, KeyClicked);
    registry.add("boardExitRequested", 0, BoardExitRequested);
    registry.add("boardDelay", 1, BoardDelay);
    registry.add("boardCycles", 0, BoardCycles);
    registry.add("boardMillis", 0, BoardMillis);
    registry.add("boardClockHz", 0, BoardClock);
    registry.add("nativeIntegerLoop", 2, NativeInteger);
    registry.add("benchRecord", 5, BenchRecord);
}

/**
 * @brief 从已挂载 FatFS 读取有大小上限的部署文件。
 * @param path 绝对卷路径。
 * @param bytes 接收文件内容的 SDRAM 容器。
 * @return 文件完整读入并成功关闭时返回 true。
 */
bool ReadFile(const char *path, Memory::Vector<std::uint8_t> &bytes)
{
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) return false;
    const auto length = f_size(&file);
    bool ok = length <= 1024u * 1024u;
    if (ok)
    {
        bytes.resize(length);
        UINT got = 0;
        ok = f_read(&file, bytes.data(), length, &got) == FR_OK && got == length;
    }
    const bool closed = f_close(&file) == FR_OK;
    return ok && closed;
}

/**
 * @brief 使用最高默认 VM 优化运行部署程序；FatFS 启动文件存在但损坏时不回退。
 * @param selected 要运行的内置程序或 FatFS 文件项。
 * @return 成功为零，映像、数据区或执行出错为非零。
 */
int Execute(const ProgramEntry &selected)
{
    Memory::Vector<std::uint8_t> image, dataImage, layoutImage;
    const void *programBytes = builtin_program;
    std::size_t programSize = builtin_program_size;
    const void *dataBytes = builtin_data;
    std::size_t dataSize = builtin_data_size;
    Memory::String layout(reinterpret_cast<const char *>(builtin_layout), builtin_layout_size);
    if (selected.builtin == 2)
    {
        programBytes = builtin_hello_program;
        programSize = builtin_hello_program_size;
        dataBytes = builtin_hello_data;
        dataSize = builtin_hello_data_size;
        layout.assign(reinterpret_cast<const char *>(builtin_hello_layout), builtin_hello_layout_size);
    }
    if (selected.builtin == 3)
    {
        programBytes = builtin_panic_program;
        programSize = builtin_panic_program_size;
        dataBytes = builtin_panic_data;
        dataSize = builtin_panic_data_size;
        layout.assign(reinterpret_cast<const char *>(builtin_panic_layout), builtin_panic_layout_size);
    }
    if (!selected.builtin)
    {
        char path[192];
        std::snprintf(path, sizeof(path), "0:/magel/%s.svmb", selected.name);
        bool loaded = ReadFile(path, image);
        std::snprintf(path, sizeof(path), "0:/magel/%s.data.bin", selected.name);
        loaded = ReadFile(path, dataImage) && loaded;
        std::snprintf(path, sizeof(path), "0:/magel/%s.layout", selected.name);
        loaded = ReadFile(path, layoutImage) && loaded;
        if (!loaded)
        {
            std::printf("MAGEL file read failed\r\n");
            return 2;
        }
        programBytes = image.data();
        programSize = image.size();
        dataBytes = dataImage.data();
        dataSize = dataImage.size();
        layout.assign(reinterpret_cast<const char *>(layoutImage.data()), layoutImage.size());
        std::printf("MAGEL image source=FatFS\r\n");
    }
    else
        std::printf("MAGEL image source=Flash\r\n");
    stackvm::Program program;
    Memory::Vector<Memory::String> diagnostics;
    if (!stackvm::loadProgramBinary(programBytes, programSize, program, &diagnostics))
    {
        for (const auto &error : diagnostics) std::printf("MAGEL load: %s\r\n", error.c_str());
        return 3;
    }
    stackvm::DataArea data;
    if (!data.loadFromBytes(dataBytes, dataSize, layout, &diagnostics))
    {
        for (const auto &error : diagnostics) std::printf("MAGEL data: %s\r\n", error.c_str());
        return 4;
    }
    stackvm::NativeRegistry registry;
    stackvm::Machine machine;
    machine.setHeapAllocator(&mcuArenas::CurrentBytePool());
    machine.setData(&data);
    machine.setFileApi(&mcuFilesystem::CurrentApi());
    mcuGlue::SetOutput(SerialWrite, nullptr, false);
    mcuGlue::Register(machine, &registry);
    RegisterBoard(registry);
    mcuConsole::SetRead(SerialRead, nullptr);
    const auto result = machine.run(program, {}, 10000000u);
    if (!result.ok) std::printf("MAGEL run: %s\r\n", result.error.c_str());
    return result.ok ? 0 : 5;
}
}  // namespace

/**
 * @brief 从 BSP 外部 RAM 堆申请三块独立池并持续处理 LCD 程序选择。
 * @retval 1 表示 SDRAM 池初始化失败；成功时持续运行选择循环，不返回。
 * @note 每次程序退出或报错后输出内存峰值并重新绘制选择页。
 */
int Magel_BoardRun(void)
{
    g_MagelBenchmark.magic = 0x4d47454cu;
    g_MagelBenchmark.clockHz = FkClockHz();
    g_MagelBenchmark.status = 0xffffffffu;
    g_MagelUiTrace.magic = 0x55494d47u;
    void *metadata = FkSdramAllocate(metadataBytes);
    void *runtime = FkSdramAllocate(runtimeBytes);
    void *bytes = FkSdramAllocate(bytePoolBytes);
    if (!metadata || !runtime || !bytes || !mcuArenas::SetMetadata(metadata, metadataBytes) || !mcuArenas::SetRuntimeArena(runtime, runtimeBytes))
    {
        std::printf("MAGEL SDRAM pool initialization failed\r\n");
        return 1;
    }
    mcuArenas::InstallRuntimeArena();
    // 字节池的身份索引也使用 Memory 容器，须在运行时分配器安装后构造。
    if (!mcuArenas::SetBytePool(bytes, bytePoolBytes))
    {
        std::printf("MAGEL byte pool initialization failed\r\n");
        return 1;
    }
    std::printf("MAGEL pools metadata=%p/%lu runtime=%p/%lu bytes=%p/%lu\r\n",
                metadata, static_cast<unsigned long>(metadataBytes), runtime, static_cast<unsigned long>(runtimeBytes),
                bytes, static_cast<unsigned long>(bytePoolBytes));
    const auto mounted = f_mount(&filesystem, "0:", 1);
    std::printf("MAGEL FatFS mount=%d (never formats media)\r\n", static_cast<int>(mounted));
    ScanPrograms();
    unsigned selected = 0;
    int lastStatus = -1;
    bool run = false;
#if (defined(FK_AUTORUN_BENCHMARK) && FK_AUTORUN_BENCHMARK) || (defined(FK_BOARD_SELFTEST) && FK_BOARD_SELFTEST)
    run = true;
#endif
    DrawMenu(selected, lastStatus);
    FkResetKeys();
    for (;;)
    {
        if (run)
        {
            /* Execute 的程序、数据区、Machine、结果和宿主注册表均在返回前析构。
             * 不重置承载 C++ 静态对象的元数据池，避免制造失效指针。 */
            Memory::restartSignal().clear();
            lastStatus = Execute(entries[selected]);
            g_MagelBenchmark.status = lastStatus;
            FkPublish(&g_MagelBenchmark, sizeof(g_MagelBenchmark));
            std::printf("MAGEL returned status=%d metadata_peak=%lu runtime_peak=%lu byte_peak=%lu\r\n", lastStatus,
                        static_cast<unsigned long>(mcuMetadataPeakBytes()),
                        static_cast<unsigned long>(mcuArenas::CurrentRuntimeArena().stats().peakBytes),
                        static_cast<unsigned long>(mcuArenas::CurrentBytePool().peakBytes()));
            run = false;
            FkResetKeys();
            DrawMenu(selected, lastStatus);
            const unsigned trace = g_MagelUiTrace.completed % 3;
            g_MagelUiTrace.statuses[trace] = lastStatus;
            g_MagelUiTrace.programs[trace] = selected;
            ++g_MagelUiTrace.completed;
            FkPublish(&g_MagelUiTrace, sizeof(g_MagelUiTrace));
#if defined(FK_BOARD_SELFTEST) && FK_BOARD_SELFTEST
            if (g_MagelUiTrace.completed < 3)
            {
                selected = g_MagelUiTrace.completed == 1 ? 2 : 1;
                run = true;
            }
#endif
        }
        if (!FkMenuReady())
        {
            FkDelay(10);
            continue;
        }
        if (FkKeyClicked(0))
        {
            ScanPrograms();
            selected = 0;
            DrawMenu(selected, lastStatus);
        }
        if (FkKeyClicked(2))
        {
            selected = selected ? selected - 1 : entryCount - 1;
            DrawMenu(selected, lastStatus);
        }
        if (FkKeyClicked(3))
        {
            selected = (selected + 1) % entryCount;
            DrawMenu(selected, lastStatus);
        }
        if (FkKeyClicked(1)) run = true;
        FkDelay(10);
    }
}
