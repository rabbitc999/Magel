/**
 * @file mcu_host.cpp
 * @brief `mcu_vm` 的宿主骨架（桌面可跑，用来验证板端配置）。
 *
 * 构成就是板端那一套：**静态字节池 + 元数据 arena + 内存映像加载**。桌面上它读文件
 * 只是为了让等价性回归能跑；板端把 `ReadImage()` 换成"flash 里的指针 + 长度"即可，
 * 其余一行都不用改（VM 侧已经没有任何文件系统依赖）。
 *
 * 用法（桌面验证）：
 *   ./mcu-host program.svmb [data.data.bin] [data.layout]
 */

#include "stackvm.hpp"
#include "embed_pool_allocator.hpp"
#include "mcu_metadata_heap.hpp"
#include "mcu_glue.hpp"
#include "mcu_console.hpp"
#include "mcu_filesystem.hpp"
#include "mcu_arenas.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#ifdef _WIN32
#include "console/utf8.hpp"
#endif

#ifndef MCU_VM_ARENA_BYTES
#define MCU_VM_ARENA_BYTES (256u * 1024u) /**< VM 字节池（H743 起步口径）。 */
#endif

namespace
{
/**
 * @brief 输出下沉的目标：默认 stdout，`--out FILE` 时改写到文件。
 */
std::FILE *g_sinkTarget = nullptr;

/**
 * @brief 默认输出下沉：写 stdout（桌面验证用）。
 *
 * 板端把它换成串口 / OLED / RTT，例如 `mcuGlue::SetOutput(&UartWrite, &uart1, false)`。
 * `keepText=false` 时不累积 `RunResult::text`，省 RAM，也不用等 `run()` 结束才看到输出。
 */
void HostOutputSink(void *, const char *bytes, std::size_t count)
{
    std::FILE *target = g_sinkTarget != nullptr ? g_sinkTarget : stdout;
    std::fwrite(bytes, 1u, count, target);
    std::fflush(target);
}

/* ---- 板端替换点 0：程序输入与 `main` 参数（板端来自串口 / 配置 / 出厂参数） ---- */

/**
 * @brief 桌面回归用的"串口"读函数：从 stdin 取字节。
 * @return 实际读到的字节数；0 表示暂时没数据（stdin 到结尾）。
 */
unsigned ReadStdin(void *, unsigned char *buffer, unsigned length)
{
    return static_cast<unsigned>(std::fread(buffer, 1u, length, stdin));
}

/**
 * @brief `MAGEL_NO_STDIN_INPUT=1` 时不接 stdin（门禁要确定性：缺输入必须报错）。
 * @return 关掉兜底返回 true。
 */
bool StdinInputDisabled()
{
    const char *flag = std::getenv("MAGEL_NO_STDIN_INPUT");
    return flag != nullptr && flag[0] != '\0' && flag[0] != '0';
}

/**
 * @brief 按空白切分 `input(变量);` 的输入项（与桌面 `--input` 同义）。
 */
Memory::Vector<Memory::String> SplitInput(const Memory::String &text)
{
    Memory::Vector<Memory::String> items;
    std::size_t at = 0u;
    while (at < text.size())
    {
        while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at;
        const std::size_t start = at;
        while (at < text.size() && !std::isspace(static_cast<unsigned char>(text[at]))) ++at;
        if (at > start) items.push_back(text.substr(start, at - start));
    }
    return items;
}

/* ---- 板端替换点 1/2：程序映像（现在是文件，板端是 flash 指针 + 长度） ---- */
/**
 * @brief 从宿主文件读取程序映像的二进制字节。
 * @param path 目标文件或目录的路径。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ReadImage(const char *path, Memory::Vector<std::uint8_t> *out)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    out->assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

/* ---- 板端替换点 2/2：布局文本（现在是文件，板端是一个 `const char[]`） ---- */
/**
 * @brief 从宿主文件读取数据布局文本。
 * @param path 目标文件或目录的路径。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ReadText(const char *path, Memory::String *out)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    out->assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

/**
 * @brief 将虚拟机诊断写入宿主错误输出。
 * @param what 测试条件的说明文本。
 * @param diagnostics 接收处理诊断的列表。
 */
void ReportDiagnostics(const char *what, const Memory::Vector<Memory::String> &diagnostics)
{
    std::fprintf(stderr, "mcu-vm：%s失败\n", what);
    for (const Memory::String &line : diagnostics) std::fprintf(stderr, "  %s\n", line.c_str());
}
}  // namespace

/**
 * @brief 运行桌面验证宿主，Windows 参数统一为 UTF-8。
 * @param argc 实参数量。
 * @param argv 原始实参。
 * @return 执行成功为零。
 */
int main(int argc, char **argv)
{
#ifdef _WIN32
    tool_console::Session consoleSession;
    auto utf8Arguments = tool_console::arguments(argc, argv);
    std::vector<char *> values;
    for (auto &argument : utf8Arguments) values.push_back(argument.data());
    values.push_back(nullptr);
    argc = static_cast<int>(utf8Arguments.size());
    argv = values.data();
#endif
    /* 用法：mcu-host <程序.svmb> [数据区.data.bin] [布局.layout]
     *                [--input "项 项 …"] [-- 程序参数 …] */
    Memory::String inputText;
    Memory::Vector<Memory::String> programArguments;
    Memory::Vector<const char *> positional;
    /* 软重启（触发之一：内存水位）：跑完一轮后 `liveBytes` 达到它就重置 arena 重跑。 */
    std::size_t watermarkBytes = 0u;
    /* 软重启（触发之二：分配失败）：本进程给它一个**定容**运行时 arena 才好复现，
     * 0 = 不装（用缺省的宿主堆式 arena，分配失败基本碰不到；水位触发仍然可用）。 */
    std::size_t arenaBytes = 0u;
    int maxRestarts = 3;
    for (int index = 1; index < argc; ++index)
    {
        const Memory::String argument = argv[index];
        if (argument == "--input" && index + 1 < argc)
        {
            inputText = argv[++index];
            continue;
        }
        if (argument == "--watermark" && index + 1 < argc)
        {
            /* 触发之一：跑完一轮后池内活跃字节达到这个水位就软重启（重置 arena 重跑）。 */
            watermarkBytes = static_cast<std::size_t>(std::strtoull(argv[++index], nullptr, 10));
            continue;
        }
        if (argument == "--max-restarts" && index + 1 < argc)
        {
            maxRestarts = static_cast<int>(std::strtol(argv[++index], nullptr, 10));
            continue;
        }
        if (argument == "--arena-bytes" && index + 1 < argc)
        {
            /* 装一个定容运行时 arena（`Memory::BufferRuntimeArena`）：跑满即分配失败 →
             * 可恢复终止 → 重置 arena 重跑。板端这里换成 SDRAM / 静态数组即可。 */
            arenaBytes = static_cast<std::size_t>(std::strtoull(argv[++index], nullptr, 10));
            continue;
        }
        if (argument == "--out" && index + 1 < argc)
        {
            /* 把输出下沉写到文件：门禁用它独立断言"流式输出与期望一致"。 */
            g_sinkTarget = std::fopen(argv[++index], "wb");
            if (g_sinkTarget == nullptr)
            {
                std::fprintf(stderr, "mcu-vm：打不开输出文件：%s\n", argv[index]);
                return 2;
            }
            continue;
        }
        if (argument == "--")
        {
            for (++index; index < argc; ++index) programArguments.emplace_back(argv[index]);
            break;
        }
        positional.push_back(argv[index]);
    }
    if (positional.empty())
    {
        std::fprintf(stderr,
                     "用法：mcu-host <程序.svmb> [数据区.data.bin] [布局.layout] "
                     "[--input \"项 项 …\"] [-- 程序参数 …]\n");
        return 2;
    }

    Memory::Vector<std::uint8_t> programImage;
    if (!ReadImage(positional[0], &programImage))
    {
        std::fprintf(stderr, "mcu-vm：读不到程序映像：%s\n", positional[0]);
        return 2;
    }

    /* 字节池：编译期静态数组，或（带 SDRAM 的板子）mcuArenas::SetBytePool 注入。
     * 两种情况下运行期都不向宿主堆要内存。 */
    embed_stackvm::EmbedPoolAllocatorView &bytePool = mcuArenas::CurrentBytePool();
    stackvm::Machine machine;
    /* 这个构建没有默认堆（STACKVM_WITH_DEFAULT_HEAP=0），必须注入。 */
    machine.setHeapAllocator(&bytePool);
    machine.setProgramArguments(programArguments);
    /* HAL 胶水：板端把自己那套接口挂上来（默认输出接到 stdout）。 */
    mcuGlue::SetOutput(&HostOutputSink, nullptr, /*keepText=*/false);
    mcuGlue::Register(machine);
    /* 文件系统：编译期后端或板端注入（关掉时 __vm_file_* 会明确报错）。 */
    machine.setFileApi(&mcuFilesystem::CurrentApi());
    /* 串口输入：桌面回归把 mcuConsole 接到 stdin；板端换成自己的 UART 读函数即可。 */
    if (!StdinInputDisabled())
    {
        mcuConsole::SetRead(&ReadStdin, nullptr);
    }

    stackvm::Program program;
    Memory::Vector<Memory::String> diagnostics;
    if (!stackvm::loadProgramBinary(programImage.data(), programImage.size(), program,
                                    &diagnostics))
    {
        ReportDiagnostics("加载程序", diagnostics);
        return 1;
    }
    std::fprintf(stderr, "mcu-vm：加载后 元数据峰值 %lu 字节（映像 %lu 字节）\n",
                 static_cast<unsigned long>(mcuMetadataPeakBytes()),
                 static_cast<unsigned long>(programImage.size()));

    stackvm::DataArea data;
    if (positional.size() > 1)
    {
        Memory::Vector<std::uint8_t> dataImage;
        Memory::String layoutText;
        if (!ReadImage(positional[1], &dataImage))
        {
            std::fprintf(stderr, "mcu-vm：读不到数据区：%s\n", positional[1]);
            return 2;
        }
        if (positional.size() > 2 && !ReadText(positional[2], &layoutText))
        {
            std::fprintf(stderr, "mcu-vm：读不到布局表：%s\n", positional[2]);
            return 2;
        }
        if (!data.loadFromBytes(dataImage.data(), dataImage.size(), layoutText, &diagnostics))
        {
            ReportDiagnostics("加载数据区", diagnostics);
            return 1;
        }
        machine.setData(&data);
    }

    /* 软重启（两个触发，见 `std::watchdog`）：① 分配失败（`result.recoverable`）；
     * ② 跑完一轮后池内活跃字节达到水位（`--watermark`）。被弃轮次的 `output` 不打印；
     * 文本已经经输出下沉实时写出去了。
     *
     * 定容运行时 arena（`--arena-bytes`）在**加载完程序 / 数据区之后**才装上：宿主自己的
     * 字符串（argv、`inputText`、诊断）在缺省 arena 里，软重启重置定容 arena 时不受影响。
     * 跨轮必须清干净引自定容 arena 的对象（下面先 `result = {}` 再重置）。 */
    Memory::BufferRuntimeArena *runtimeArena = nullptr;
    if (arenaBytes != 0u)
    {
        void *buffer = std::malloc(arenaBytes);
        if (buffer == nullptr)
        {
            std::fprintf(stderr, "mcu-vm：要不到运行时 arena（%lu 字节）\n",
                         static_cast<unsigned long>(arenaBytes));
            return 2;
        }
        /* 走 `mcuArenas`：桌面这里塞一块 malloc 来的缓冲，板端换成 SDRAM 指针即可。 */
        if (!mcuArenas::SetRuntimeArena(buffer, static_cast<std::uint32_t>(arenaBytes)))
        {
            std::fprintf(stderr, "mcu-vm：运行时 arena 已经装上，不能再换缓冲区\n");
            return 2;
        }
        mcuArenas::InstallRuntimeArena();
        runtimeArena = &mcuArenas::CurrentRuntimeArena();
        std::fprintf(stderr, "mcu-vm：运行时 arena %lu 字节（其中应急预留 %lu 字节）\n",
                     static_cast<unsigned long>(runtimeArena->capacity()),
                     static_cast<unsigned long>(runtimeArena->rescueBytes()));
    }
    else
    {
        /* 没显式给尺寸也用**定容**运行时 arena（编译期静态数组）：板端口径就是"一块固定
         * 预算、一个字节都不向宿主要"，桌面回归也用同一套，免得两边内存行为不一样。 */
        mcuArenas::InstallRuntimeArena();
        runtimeArena = &mcuArenas::CurrentRuntimeArena();
        std::fprintf(stderr, "mcu-vm：运行时 arena %lu 字节（编译期静态；应急预留 %lu 字节）\n",
                     static_cast<unsigned long>(runtimeArena->capacity()),
                     static_cast<unsigned long>(runtimeArena->rescueBytes()));
    }

    stackvm::RunResult result;
    for (int attempt = 0;; ++attempt)
    {
        Memory::restartSignal().clear(); /* 每轮开始前清标志（序号跨轮保留） */
        {
            const Memory::Vector<Memory::String> inputItems = SplitInput(inputText);
            result = machine.run(program, inputItems); /* run() 内部会重置字节池 */
        }
        /* 本轮触发了什么？先报原因（这些都是标量，读起来不碰 arena）。 */
        if (result.recoverable)
        {
            const Memory::AllocationFailure failure = Memory::restartSignal().peek();
            if (failure.reason == Memory::WatchdogReason::ExplicitRequest)
                std::fprintf(stderr, "mcu-vm：本轮可恢复终止（程序显式重启请求；第 %llu 次）\n",
                             static_cast<unsigned long long>(failure.sequence));
            else
                std::fprintf(stderr,
                             "mcu-vm：本轮可恢复终止（分配失败第 %llu 次：申请 %lu 字节，"
                             "池内 %lu/%lu 字节，块 %lu）\n",
                             static_cast<unsigned long long>(failure.sequence),
                             static_cast<unsigned long>(failure.requestedBytes),
                             static_cast<unsigned long>(failure.liveBytes),
                             static_cast<unsigned long>(failure.capacity),
                             static_cast<unsigned long>(failure.liveBlocks));
        }
        const bool restartOnOom = result.recoverable;
        /* 水位触发：跑完一轮后看**三块 arena 里最高的那个水位**（峰值＝这一轮最高用到多少；
         * 活跃＝结束时还没还回去的），任一个达到水位就软重启。（只看程序字节池的话，
         * 大量样例的字节池峰值只有几十字节、内存其实压在运行时 arena 上，撞不到水位。） */
        const std::size_t runtimePeak =
            runtimeArena != nullptr ? runtimeArena->stats().peakBytes : 0u;
        const std::size_t runtimeLive =
            runtimeArena != nullptr ? runtimeArena->stats().liveBytes : 0u;
        const std::size_t waterPeak =
            std::max(std::max(bytePool.peakBytes(), runtimePeak),
                     static_cast<std::size_t>(mcuMetadataPeakBytes()));
        const std::size_t waterLive =
            std::max(std::max(bytePool.liveBytes(), runtimeLive),
                     static_cast<std::size_t>(mcuMetadataLiveBytes()));
        /* 水位：宿主的 `--watermark` 与程序 `std::watchdog::enable(N)` 取较大者（0 = 不按水位判）。
         * 程序的 enable() 在 run() 里发生，所以这里读到的就是本轮它要的水位。 */
        const std::size_t programWatermark =
            Memory::restartSignal().enabled() ? Memory::restartSignal().watermarkBytes() : 0u;
        const std::size_t effectiveWatermark = std::max(watermarkBytes, programWatermark);
        const bool restartOnWater =
            effectiveWatermark != 0u && result.ok && (waterPeak >= effectiveWatermark || waterLive >= effectiveWatermark);
        if ((!restartOnOom && !restartOnWater) || attempt >= maxRestarts) break;
        if (restartOnWater)
            std::fprintf(stderr,
                         "mcu-vm：软重启 %d/%d（内存水位触发：峰值 %lu / 活跃 %lu ≥ %lu 字节；"
                         "字节池 %lu/%lu，运行时 %lu/%lu，元数据 %lu/%lu）\n",
                         attempt + 1, maxRestarts,
                         static_cast<unsigned long>(waterPeak),
                         static_cast<unsigned long>(waterLive),
                         static_cast<unsigned long>(effectiveWatermark),
                         static_cast<unsigned long>(bytePool.peakBytes()),
                         static_cast<unsigned long>(bytePool.liveBytes()),
                         static_cast<unsigned long>(runtimePeak),
                         static_cast<unsigned long>(runtimeLive),
                         static_cast<unsigned long>(mcuMetadataPeakBytes()),
                         static_cast<unsigned long>(mcuMetadataLiveBytes()));
        else
            std::fprintf(stderr, "mcu-vm：软重启 %d/%d（%s）\n", attempt + 1, maxRestarts,
                         Memory::restartSignal().reason() == Memory::WatchdogReason::ExplicitRequest ? "程序显式请求触发" : "分配失败触发");
        /* 准备下一轮：先把上一轮引自定容 arena 的对象清掉，**再**重置两块 arena
         * （顺序反了就是"释放已经重置过的池"→ 直接终止）。 */
        result = stackvm::RunResult{};
        // 水位重启也走完整清扫；随后重置物理池和身份槽，再重置承载执行状态的 arena。
        bytePool.sweep();
        bytePool.reset(bytePool.base());
        if (runtimeArena != nullptr) mcuArenas::CurrentRuntimeArena().resetForRestart();
        mcuMetadataResetForRestart();
    }
    std::fprintf(stderr, "mcu-vm：运行后 元数据峰值 %lu 字节\n",
                 static_cast<unsigned long>(mcuMetadataPeakBytes()));
    /* 文本已经在下沉里实时写出去了（keepText=false）；`output` 仍由宿主决定怎么送。 */
    if (!result.output.empty()) std::printf("%s\n", result.output.c_str());
    std::fflush(stdout);
    if (!result.ok)
    {
        if (result.recoverable)
            std::fprintf(stderr,
                         "mcu-vm：重启次数用尽，本轮仍是可恢复终止（原因码 %d，见上面每轮报告）\n",
                         result.recoverableReason);
        else if (!result.error.empty())
            std::fprintf(stderr, "mcu-vm：运行失败：%s\n", result.error.c_str());
        else
            std::fprintf(stderr, "mcu-vm：运行失败（没有错误文案）\n");
        return 1;
    }

    /* footprint 观测：三块 arena 的峰值 / 活跃 / 块数（板端调预算靠这几个数）。 */
    if (runtimeArena != nullptr)
    {
        const Memory::RuntimeAllocator::Stats arenaStats = runtimeArena->stats();
        std::fprintf(stderr,
                     "mcu-vm：运行时 arena 峰值 %lu / 容量 %lu 字节，活跃 %lu 字节 %lu 块，"
                     "应急预留余 %lu\n",
                     static_cast<unsigned long>(arenaStats.peakBytes),
                     static_cast<unsigned long>(runtimeArena->capacity()),
                     static_cast<unsigned long>(arenaStats.liveBytes),
                     static_cast<unsigned long>(arenaStats.liveBlocks),
                     static_cast<unsigned long>(runtimeArena->rescueRemainingBytes()));
    }
    std::fprintf(stderr, "mcu-vm：元数据 峰值 %lu / 容量 %lu 字节，块 %u\n",
                 static_cast<unsigned long>(mcuMetadataPeakBytes()),
                 static_cast<unsigned long>(mcuMetadataCapacityBytes()),
                 static_cast<unsigned>(mcuMetadataBlockCount()));
    std::fprintf(stderr, "mcu-vm：字节池 峰值 %lu / 容量 %lu 字节，活跃 %lu，块 %u\n",
                 static_cast<unsigned long>(bytePool.peakBytes()),
                 static_cast<unsigned long>(bytePool.capacity()),
                 static_cast<unsigned long>(bytePool.liveBytes()),
                 static_cast<unsigned>(bytePool.blockCount()));
    std::fprintf(stderr, "mcu-vm：文件系统后端 %s（可开可用=%s）\n",
                 mcuFilesystem::BackendName(),
                 mcuFilesystem::Available() ? "yes" : "no");
    return 0;
}
