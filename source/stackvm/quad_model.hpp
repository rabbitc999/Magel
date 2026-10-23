/**
 * @file quad_model.hpp
 * @brief 四元式、布局与标量类型模型。
 */
#pragma once
#include "execution_types.hpp"
#include "reflection.hpp"

namespace stackvm
{
/**
 * @brief 一条四元式：`(op, arg1, arg2, result[, extra])`。
 */
struct Quad
{
    Memory::String sourceFile;            ///< 四元式对应的 CHALI 源文件。
    std::int32_t sourceLine = 0;          ///< CHALI 原始行号，零为未知。
    std::size_t line = 0;                 ///< 源码行号（1 基，诊断用）。
    Memory::String op;                    ///< 操作符，可能带类型后缀（`+:i8`）。
    Memory::Vector<Memory::String> args;  ///< 参数（`_` 表示空位）。

    /**
     * @brief 取下第 index 个参数；`_` 与不存在都算空。
     * @param index 下标。
     * @return 参数文本。
     */
    [[nodiscard]] Memory::String arg(std::size_t index) const;
};

/**
 * @brief 一个 `.quad` 文件解析出来的内容。
 */
struct QuadFile
{
    reflection::Table reflection;                ///< 可选反射元数据，优化不得裁剪。
    std::uint32_t requiredFeatures = 0;          ///< 程序要求的运行时能力位，不随优化裁剪。
    Memory::String path;                         ///< 文件路径。
    Memory::String name;                         ///< `# 名称:`。
    Memory::String expected;                     ///< `# 期望:`（示例套件用来比对）。
    Memory::String expectedText;                 ///< `# 期望文本:`。
    Memory::String input;                        ///< `# 输入:`（`read` / `getc` 的数据）。
    Memory::Vector<Quad> quads;                  ///< 四元式。
    Memory::Vector<Memory::String> diagnostics;  ///< 解析诊断。

    /**
     * @brief 是否解析成功。
     * @return 没有诊断返回 true。
     */
    [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

/**
 * @brief 解析 `.quad` 文本。
 * @param text 文本（UTF-8）。
 * @param label 诊断里显示的文件名。
 * @return 解析结果。
 */
[[nodiscard]] QuadFile parseQuads(std::string_view text, Memory::String label);

/**
 * @brief 读一个 `.quad` 文件。
 * @param path 路径。
 * @return 解析结果（打不开时诊断里给消息）。
 */
[[nodiscard]] QuadFile loadQuadFile(const Memory::String &path);

/**
 * @brief 编码带字符串表的二进制四元式，不写入宿主路径。
 * @param file 已解析的四元式。
 * @return 编码字节；输入含诊断时返回空。
 */
[[nodiscard]] Memory::String encodeQuadBinary(const QuadFile &file);
/**
 * @brief 解码二进制四元式，严格校验版本、引用和边界。
 * @param bytes 编码字节。
 * @param label 诊断路径。
 * @return 四元式或诊断。
 */
[[nodiscard]] QuadFile decodeQuadBinary(std::string_view bytes, Memory::String label);
/**
 * @brief 数据符号的二进制布局记录。
 */
struct LayoutRow
{
    Memory::String name;                           ///< 符号名称。
    std::size_t offset = 0, size = 0, stride = 0;  ///< 字节偏移、容量和步长。
    bool hasSize = false, hasStride = false;       ///< 可选字段是否存在。
};
/**
 * @brief 解析文本或二进制布局，失败不修改输出。
 * @param bytes 布局数据。
 * @param rows 输出记录。
 * @return 是否成功。
 */
bool decodeDataLayout(std::string_view bytes, Memory::Vector<LayoutRow> &rows);
/**
 * @brief 将布局编码为变长整数与长度前缀字符串。
 * @param rows 布局记录。
 * @return 二进制字节。
 */
[[nodiscard]] Memory::String encodeDataLayout(const Memory::Vector<LayoutRow> &rows);

// ---------------------------------------------------------------------------
//  类型
// ---------------------------------------------------------------------------

/**
 * @brief 变量类型。
 */
struct TypeInfo
{
    Memory::String name;      ///< `i32` / `u8` / `f64` …
    int bits = 16;            ///< 位宽。
    bool isUnsigned = false;  ///< 有无符号。
    bool isFloat = false;     ///< 是否浮点。
    std::size_t size = 2;     ///< 字节数（本库只用于展示）。
};

/**
 * @brief 名字 → 类型；没写 `decl` 时按 `i16`（与 transfer 的旧语义一致）。
 * @param name 类型名；空串表示"没声明"。
 * @return 类型信息。
 */
[[nodiscard]] TypeInfo typeOf(std::string_view name);

// ---------------------------------------------------------------------------
//  构建能力
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  栈指令
// ---------------------------------------------------------------------------

/**
 * @brief 打印格式（由 `write` 的类型后缀或变量声明决定）。
 */
enum class PrintFormat
{
    Signed,    ///< 有符号整数。
    Unsigned,  ///< 无符号整数。
    Float,     ///< 浮点。
    Text,      ///< 字符串（`print`）。
};
}  // namespace stackvm
