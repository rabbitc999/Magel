/**
 * @file runtime_slot.hpp
 * @brief VM 内部 runtime slot 模块的类型与接口。
 */
#pragma once
#include "stackvm.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 栈里的一个值。
 */
struct Slot
{
    bool isFloat = false;  ///< 是否浮点。
    bool isText = false;   ///< 是否字符串。
    std::int64_t integer = 0;
    double real = 0.0;
    Memory::String text;

    /**
     * @brief 造一个整数。
     * @param value 值。
     * @return 槽位。
     */
    static Slot fromInt(std::int64_t value)
    {
        Slot slot;
        slot.integer = value;
        return slot;
    }

    /**
     * @brief 造一个浮点。
     * @param value 值。
     * @return 槽位。
     */
    static Slot fromDouble(double value)
    {
        Slot slot;
        slot.isFloat = true;
        slot.real = value;
        return slot;
    }

    /**
     * @brief 造一个字符串。
     * @param value 文本。
     * @return 槽位。
     */
    static Slot fromText(Memory::String value)
    {
        Slot slot;
        slot.isText = true;
        slot.text = std::move(value);
        return slot;
    }

    /**
     * @brief 将当前值槽中的整数或浮点数读取为 double。
     *
     * 取出数值（整数或浮点都行）。
     * @retval 值槽中的整数经 double 转换后的值，或原有浮点值。
     */
    [[nodiscard]] double number() const { return isFloat ? real : static_cast<double>(integer); }
};
}  // namespace stackvm::runtimeDetail
