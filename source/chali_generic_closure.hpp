#pragma once
#include "allocator/chali_allocator.hpp"
#include "preprocess.hpp"
#include <set>

namespace chali_lambda
{
/**
 * @brief 泛型闭包的一个参数模式及其单态化函数名称。
 */
struct GenericClosureBranch
{
    Memory::String function;
    Memory::String matcher;
    Memory::Vector<Memory::String> patterns;
};

/**
 * @brief 闭包声明的共享捕获环境与按类型选择的实现。
 */
struct GenericClosure
{
    Memory::String seed;
    Memory::String sourceSeed;
    Memory::String primary;
    Memory::String module;
    Memory::String parameters;
    Memory::String outerParameters;
    Memory::Vector<Memory::String> outerNames;
    Memory::Vector<Memory::String> outerArguments;
    int captureMode = 0;
    bool member = false;
    Memory::Map<Memory::String, int> captures;
    Memory::Vector<Memory::String> names;
    Memory::Vector<GenericClosureBranch> branches;
};

extern Memory::Map<Memory::String, GenericClosure> genericClosures;
extern Memory::Map<Memory::String, Memory::String> genericClosureBodies;
extern Memory::Set<Memory::String> genericClosureNames;

/**
 * @brief 跳过源码掩码中的空白，字符串和注释由调用方预先遮蔽。
 * @param mask 与源码等长的代码掩码。
 * @param at 读写当前扫描位置。
 */
void closureSkipSpace(const Memory::String &mask, std::size_t &at);
/**
 * @brief 将带类型参数的闭包提升为模板实现，原位置保留唯一的环境构造入口。
 * @param result 待归一化源码及原始行位置。
 * @param error 接收无效声明和重复特化的说明。
 * @return 完成所有声明替换时返回 true。
 * @note 函数签名、函数体和块语法仍由生成解析器检查。
 */
bool normalizeGenericClosures(preprocess::Result &result, Memory::String &error);
/**
 * @brief 为尚未被普通模板替换的显式闭包调用标记类型参数边界。
 * @param source 已完成普通泛型单态化的源码。
 * @note 只处理紧随调用参数表的角括号，不改变普通比较表达式。
 */
void markClosureTypeArguments(Memory::String &source);
}  // namespace chali_lambda
