/**
 * @file native_registry.hpp
 * @brief VM 内部 native registry 模块的类型与接口。
 */
#pragma once
#include "runtime_context.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 一个"内部 C++"原生函数（字符串 / 格式化那一套）。
 */
struct SlotNative
{
    /**
     * 参数个数；**负数 = 变参**（吃光这次攒下的所有实参：`printf` / `format` 用它，
     * 第一个实参是格式串，其余按顺序对应占位符）。
     */
    int arity = 0;
    /// 实现。
    Slot (*apply)(NativeContext &, const Memory::Vector<Slot> &) = nullptr;
};
/**
 * @brief 名字 → 内部 C++ 原生函数。
 */
struct SlotNativeEntry
{
    const char *first;
    SlotNative second;
};
struct SlotNativeTable
{
    const SlotNativeEntry *begin_;
    std::size_t count_;
    /**
     * @brief 将名字解析为稳定整数身份，动态回调临时名称不自动驻留。
     * @param name 名字。
     * @return 已登记身份或 absent。
     */
    const SlotNativeEntry *find(const Memory::String &name) const
    {
        for (std::size_t i = 0; i < count_; ++i)
            if (name == begin_[i].first) return begin_ + i;
        return nullptr;
    }
    /**
     * @brief 取得容器末尾迭代器。
     * @retval 对应对象的指针。
     */
    const SlotNativeEntry *end() const { return nullptr; }
};
extern const SlotNativeEntry kSlotNativeEntries[66];
extern const SlotNativeTable kSlotNatives;
}  // namespace stackvm::runtimeDetail
