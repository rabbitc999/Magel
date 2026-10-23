/**
 * @file value_literals.cpp
 * @brief 字面量解析、名称规范化与标量格式化。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 去掉首尾空白。
 * @param text 原文。
 * @return 去空白后的文本。
 */
Memory::String trim(const Memory::String &text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == Memory::String::npos) return Memory::String();
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1u);
}

/**
 * @brief 去掉首尾引号。
 * @param text 原文。
 * @return 去引号后的文本。
 */
Memory::String unquote(const Memory::String &text)
{
    if (text.size() >= 2u && (text.front() == '"' || text.front() == '\'') && text.back() == text.front())
    {
        return text.substr(1u, text.size() - 2u);
    }
    return text;
}

/**
 * @brief 按逗号切分四元式参数：引号里的逗号不算分隔符。
 * @param body 括号里的内容。
 * @return 参数列表（已去空白）。
 */
Memory::Vector<Memory::String> splitArguments(const Memory::String &body)
{
    Memory::Vector<Memory::String> parts;
    Memory::String current;
    char quote = '\0';
    for (const char ch : body)
    {
        if (quote != '\0')
        {
            current += ch;
            if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'')
        {
            quote = ch;
            current += ch;
            continue;
        }
        if (ch == ',')
        {
            parts.push_back(trim(current));
            current.clear();
            continue;
        }
        current += ch;
    }
    if (!current.empty() || !parts.empty()) parts.push_back(trim(current));
    return parts;
}

/**
 * @brief 把文本解析成整数 / 浮点。
 * @param text 文本。
 * @param isFloat 输出：是不是浮点。
 * @param intValue 输出：整数。
 * @param doubleValue 输出：浮点。
 * @return 是数字返回 true。
 */
bool parseNumber(const Memory::String &text, bool &isFloat, std::int64_t &intValue,
                 double &doubleValue)
{
    if (text.empty()) return false;

    // 1. 先按整数试（含负号）。
    std::size_t index = 0;
    if (text[index] == '+' || text[index] == '-') ++index;
    bool digits = false;
    bool isPlainInt = index < text.size();
    for (; index < text.size(); ++index)
    {
        if (text[index] >= '0' && text[index] <= '9')
        {
            digits = true;
            continue;
        }
        isPlainInt = false;
        break;
    }
    if (isPlainInt && digits)
    {
        char *end = nullptr;
        const long long value = std::strtoll(text.c_str(), &end, 10);
        if (end != nullptr && *end == '\0')
        {
            isFloat = false;
            intValue = static_cast<std::int64_t>(value);
            return true;
        }
    }

    // 2. 再按浮点试。
    char *end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if ((end != nullptr) && (*end == '\0') && (end != text.c_str()))
    {
        isFloat = true;
        doubleValue = value;
        intValue = static_cast<std::int64_t>(value);
        return true;
    }
    return false;
}

/**
 * @brief 最短往返的浮点文本（与 source/json 同一套路）。
 * @param value 浮点。
 * @return 文本。
 */
Memory::String formatDouble(double value)
{
    char buffer[64];
    Memory::String text;
    for (int precision = 17; precision >= 1; --precision)
    {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        text = buffer;
        char *end = nullptr;
        if (std::strtod(text.c_str(), &end) == value && end != nullptr && *end == '\0') break;
    }
    return text;
}

/**
 * @brief 浮点**字面量**的四元式文本：一定带小数点（`2` → `2.0`）。
 *
 * 四元式是文本形态，后端按"有没有小数点"区分整数槽位与浮点槽位；折叠出来的
 * 浮点常量必须写成 `2.0`，否则 `(:=, 2, _, t)` 之后按浮点用时会被当成整数。
 * @param value 浮点。
 * @return 文本。
 */
Memory::String formatFloatLiteral(double value)
{
    Memory::String text = formatDouble(value);
    if (text.find_first_of(".eE") == Memory::String::npos) text += ".0";
    return text;
}

/**
 * @brief 按位宽回绕并做符号扩展。
 * @param value 原值。
 * @param type 类型。
 * @return 归一化后的值。
 */
std::int64_t normalizeInt(std::int64_t value, const TypeInfo &type)
{
    if (type.bits <= 0 || type.bits >= 64) return value;
    const std::int64_t mask = (static_cast<std::int64_t>(1) << type.bits) - 1;
    value &= mask;
    if (!type.isUnsigned && (value & (static_cast<std::int64_t>(1) << (type.bits - 1))))
    {
        value -= (static_cast<std::int64_t>(1) << type.bits);
    }
    return value;
}

/**
 * @brief 判断浮点值是否落在目标整数类型的可表示范围内。
 * @param value 待转换的浮点值。
 * @param type 目标整数类型（位宽与是否有符号）。
 * @retval 可以安全转换时返回 true；非有限值或超出范围时返回 false。
 * @note 浮点转整数越界在宿主上是未定义行为，调用方应在转换前先过这一关。
 */
bool floatFitsInteger(double value, const TypeInfo &type)
{
    if (!std::isfinite(value)) return false;
    if (type.bits <= 0 || type.bits > 64) return true;
    const double magnitude = std::ldexp(1.0, type.isUnsigned ? type.bits : type.bits - 1);
    if (type.isUnsigned) return value >= 0.0 && value < magnitude;
    return value >= -magnitude && value < magnitude;
}

/**
 * @brief f32 在每次带类型的运算或转换后按 IEEE-754 单精度舍入，f64 保持 double。
 * @param value 待转换、保存或参与计算的值。
 * @param bits 数值类型的位宽。
 * @retval 保存f32 在每次带类型的运算或转换后按 IEEE-754 单精度舍入，f64 保持 double结果的对象。
 */
double normalizeFloat(double value, int bits)
{
    return bits == 32 ? static_cast<double>(static_cast<float>(value)) : value;
}

/**
 * @brief 虚拟机自己的堆：一块连续的字节区 + 空闲块表。
 *
 * **为什么不放在数据区里**：数据区（`.data.bin`）是编译期定好符号与槽位的静态内存，
 * 程序跑起来只有那一片；而 `new` / `malloc` 要多少字节是运行期才知道的，也不该跟
 * 静态数据抢地址。所以堆单独长，只和数据区**共用一套地址空间**：地址从 `base()`
 * 起算（紧跟在数据区后面），`(load, 地址, …)` / `(store, 地址, …)` 两边通用 ——
 * 对象既不在数据区里，也不在求值栈 / 调用帧里，什么时候还回来由程序自己 `free`。
 *
 * 分配策略：**按 1 字节单元** + 首次适配 + 相邻空闲块
 * 合并——**释放过的块会被后续分配复用**，块表也不会随"反复申请 / 释放"越滚越大，
 * 物理块继续复用，逻辑地址则单调分配；旧引用不会命中新对象。
 * 释放立即撤销活跃身份；重复释放、内部地址及未知地址均返回失败。
 * 读写通过逻辑地址映射到物理偏移，仅允许访问当前活跃分配。
 *
 * 线程安全：`allocate` / `release` 由一把互斥锁串行化（宿主回调 / 多线程场景下
 * 同一个堆可能被并发触发分配与释放）。只读查询不加锁——堆按 `run()` / 线程独占，
 * 契约是"读的时候没有并发写"。
 */
#if STACKVM_WITH_DEFAULT_HEAP

#else /* !STACKVM_WITH_DEFAULT_HEAP */

#endif /* STACKVM_WITH_DEFAULT_HEAP */

/* ---------------------------------------------------------------------------
 *  内部 C++ 运行时：字符串 / 格式化
 * ---------------------------------------------------------------------------
 *
 * 运行时字符串是“8 字节长度 + UTF-8 字节数据”，不附加结尾 NUL；数据区字面量只是
 * 复制源，仍用生成期哨兵界定。**读字节 / 拼接 / 切片 / 格式化这些活儿全在这层 C++ 里干**，宿主语言
 * （chali）只发一次调用 —— 不用在四元式里手写逐字节的循环，也就能顺手用上
 * `Memory::String` / `snprintf`。
 *
 * 与数学原生函数（`kNatives`）的两点不同：
 * 1. 参数与结果都是 **Slot**：地址 / 长度必须落在**整数槽**里（`(load, 地址, …)`
 *    认的是 `Slot.integer`，浮点槽的 `integer` 是 0，塞进去就成空指针了）；
 * 2. 要能用堆（造新串）与数据区（读字面量），还要能报错 —— 所以带一个 NativeContext。
 */

/**
 * 扫数据区字面量时最多看多少字节（运行时堆字符串直接读取长度）。
 */
/**
 * @brief 把 `op:type` 拆成基础操作符与类型。
 * @param op 原文。
 * @param typeName 输出类型名。
 * @return 基础操作符。
 */
Memory::String splitType(const Memory::String &op, Memory::String &typeName)
{
    // `:=` 自己就带冒号，不能按类型后缀切。
    if (op == ":=")
    {
        typeName.clear();
        return op;
    }
    const auto colon = op.find(':');
    if ((colon == Memory::String::npos) || (colon + 1u >= op.size()))
    {
        typeName.clear();
        return op;
    }
    typeName = op.substr(colon + 1u);
    return op.substr(0u, colon);
}
}  // namespace runtimeDetail
}  // namespace stackvm
