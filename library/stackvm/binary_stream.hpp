/**
 * @file binary_stream.hpp
 * @brief VM 内部 binary stream 模块的类型与接口。
 */
#pragma once
#include "stackvm.hpp"

namespace stackvm::runtimeDetail
{
constexpr std::uint8_t kProgramMagic[] = { 'P', 'E', 'S', 'V', 'M', 'B', '\r', '\n' };
/**
 * @brief 当前写出的版本。
 */
constexpr std::uint16_t kProgramFormatVersion = 6u;
constexpr std::uint8_t kSymbolMagic[] = { 'M', 'C', 'V', 'M', '0', '0', '0', '5' };
constexpr std::size_t kSymbolHeaderSize = 44u;
/**
 * @brief 从这一版起 payload 末尾有"导出标签"节。
 */
constexpr std::uint16_t kProgramExportsFormatVersion = 3u;
/**
 * @brief 从这一版起 payload 末尾（导出标签之后）还有"调试信息"节。
 */
constexpr std::uint16_t kProgramDebugFormatVersion = 4u;
constexpr std::uint16_t kProgramCompactFormatVersion = 2u;
constexpr std::uint16_t kProgramLegacyFormatVersion = 1u;
constexpr std::size_t kProgramHeaderSize = 24u;
constexpr std::size_t kMaximumBinarySize = 512u * 1024u * 1024u;
constexpr std::uint32_t kMaximumItemCount = 10u * 1000u * 1000u;
constexpr std::uint32_t kMaximumStringSize = 64u * 1024u * 1024u;

struct BinaryWriter
{
    Memory::Vector<std::uint8_t> bytes;
    Memory::String error;
    const Memory::Map<Memory::String, std::uint64_t> *symbolIndices = nullptr;
    /**
     * @brief 读写二进制中间产物的符号字段。
     * @param value 待转换、保存或参与计算的值。
     */
    void symbol(const Memory::String &value);
    /**
     * @brief 读写二进制中间产物的数值字段。
     * @param value 待转换、保存或参与计算的值。
     */
    void number(std::uint64_t value);

    /**
     * @brief 追加未经编码的原始字节。
     * @param data 输入或待写入的字节数据。
     * @param size 输入数据或目标区域的大小。
     */
    void raw(const std::uint8_t *data, std::size_t size);

    /**
     * @brief 读写一个 8 位无符号整数。
     * @param value 待转换、保存或参与计算的值。
     */
    void u8(std::uint8_t value);

    /**
     * @brief 读写一个 16 位无符号整数。
     * @param value 待转换、保存或参与计算的值。
     */
    void u16(std::uint16_t value);

    /**
     * @brief 读写一个 32 位无符号整数。
     * @param value 待转换、保存或参与计算的值。
     */
    void u32(std::uint32_t value);

    /**
     * @brief 读写一个 64 位无符号整数。
     * @param value 待转换、保存或参与计算的值。
     */
    void u64(std::uint64_t value);

    /**
     * @brief 读取变长编码的无符号整数。
     *
     * ULEB128：小整数只占一个字节。
     * @param value 待转换、保存或参与计算的值。
     */
    void varUInt(std::uint64_t value);

    /**
     * @brief 读取变长编码的有符号整数。
     *
     * ZigZag + ULEB128：正负小整数都紧凑。
     * @param value 待转换、保存或参与计算的值。
     */
    void varInt(std::int64_t value);

    /**
     * @brief 读写带变长长度前缀的文本。
     * @param value 待转换、保存或参与计算的值。
     */
    void varText(const Memory::String &value);

    /**
     * @brief 取得文本字段的字符串内容。
     * @param value 待转换、保存或参与计算的值。
     */
    void text(const Memory::String &value);

    /**
     * @brief 读写二进制记录的条目数量。
     * @param value 待转换、保存或参与计算的值。
     * @param what 测试条件的说明文本。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool count(std::size_t value, const char *what);
};

struct BinaryReader
{
    const Memory::Vector<std::uint8_t> &bytes;
    std::size_t position = 0u;
    Memory::String error;

    const Memory::Vector<SymbolRecord> *symbolRecords = nullptr;
    /**
     * @brief 读写二进制中间产物的符号字段。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool symbol(Memory::String *out);
    /**
     * @brief 读写二进制中间产物的数值字段。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool number(std::uint64_t *out);
    /**
     * @brief 初始化二进制中间产物读取器。
     * @param input 程序输入或待解码的字节流。
     */
    explicit BinaryReader(const Memory::Vector<std::uint8_t> &input);

    /**
     * @brief 检查并消耗指定长度的二进制数据。
     * @param size 输入数据或目标区域的大小。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool take(std::size_t size, const std::uint8_t **out);

    /**
     * @brief 读写一个 8 位无符号整数。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool u8(std::uint8_t *out);

    /**
     * @brief 读写一个 16 位无符号整数。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool u16(std::uint16_t *out);

    /**
     * @brief 读写一个 32 位无符号整数。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool u32(std::uint32_t *out);

    /**
     * @brief 读写一个 64 位无符号整数。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool u64(std::uint64_t *out);

    /**
     * @brief 读取变长编码的无符号整数。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool varUInt(std::uint64_t *out);

    /**
     * @brief 读取变长编码的有符号整数。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool varInt(std::int64_t *out);

    /**
     * @brief 读写带变长长度前缀的文本。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool varText(Memory::String *out);

    /**
     * @brief 取得文本字段的字符串内容。
     * @param out 接收处理结果的输出对象。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool text(Memory::String *out);

    /**
     * @brief 读写二进制记录的条目数量。
     * @param out 接收处理结果的输出对象。
     * @param what 测试条件的说明文本。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool count(std::uint32_t *out, const char *what);
};
/**
 * @brief 将二进制解析错误加入诊断列表。
 * @param diagnostics 接收处理诊断的列表。
 * @param message 待报告或输出的信息文本。
 */
void binaryDiagnostic(Memory::Vector<Memory::String> *diagnostics, const Memory::String &message);

/**
 * @brief 计算二进制中间产物的完整性校验值。
 * @param data 输入或待写入的字节数据。
 * @param size 输入数据或目标区域的大小。
 * @retval 二进制数据的完整性校验值。
 */
std::uint32_t binaryChecksum(const std::uint8_t *data, std::size_t size);

/**
 * @brief 将二进制文件内容读取到字节数组。
 * @param path 目标文件或目录的路径。
 * @param bytes 内存区域或数据的字节数。
 * @param diagnostics 接收处理诊断的列表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool readBinaryFile(const Memory::String &path, Memory::Vector<std::uint8_t> *bytes,
                    Memory::Vector<Memory::String> *diagnostics);
/**
 * @brief 收集全部语义名字，检查哈希碰撞并按引用次数降序排序。
 * @param program 原始程序。
 * @param records 条目。
 * @param indices 名字到物理索引。
 * @param diagnostics 诊断。
 * @return 成功与否。
 */
bool buildSymbols(const Program &program, Memory::Vector<SymbolRecord> &records,
                  Memory::Map<Memory::String, std::uint64_t> &indices,
                  Memory::Vector<Memory::String> *diagnostics);

/**
 * @brief 按 JSON 标准转义字符串，不依赖运行时文件。
 * @param value 字符串。
 * @return 带双引号的 JSON 字面量。
 */
Memory::String symbolJsonString(const Memory::String &value);

/**
 * @brief 写 MCVM0005 容器。
 * @param program 程序。
 * @param payload 指令及元数据。
 * @param records 符号。
 * @param path 路径。
 * @param diagnostics 诊断。
 * @return 成功与否。
 */
bool writeSymbolContainer(const Program &program, const Memory::Vector<std::uint8_t> &payload,
                          const Memory::Vector<SymbolRecord> &records, const Memory::String &path,
                          Memory::Vector<Memory::String> *diagnostics);

/**
 * @brief 解码新头部及符号表；按物理编号生成运行时内部键。
 * @param bytes 文件。
 * @param decoded 程序。
 * @param payload 指令及元数据。
 * @param diagnostics 诊断。
 * @return 成功与否。
 */
bool readSymbolContainer(const Memory::Vector<std::uint8_t> &bytes, Program &decoded,
                         Memory::Vector<std::uint8_t> &payload,
                         Memory::Vector<Memory::String> *diagnostics);

/**
 * @brief 验证条目与程序元数据一致并统计真实指令引用。
 * @param bytes 文件。
 * @param program 程序。
 * @param diagnostics 诊断。
 * @return 成功与否。
 */
bool validateSymbolProgram(const Memory::Vector<std::uint8_t> &bytes, Program &program,
                           Memory::Vector<Memory::String> *diagnostics);
}  // namespace stackvm::runtimeDetail
