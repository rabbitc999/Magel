/**
 * @file vm_features.cpp
 * @brief VM 功能位与程序元数据契约检查。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 查询实际编译进运行时的能力。
 * @return 能力位集合。
 */
std::uint32_t supportedProgramFeatures()
{
#if STACKVM_WITH_REFLECTION
    return kFeatureReflection | kFeatureByteMemory;
#else
    return kFeatureByteMemory;
#endif
}

/**
 * @brief 验证二进制、组装及直接执行共用的能力契约。
 * @param required 程序能力位。
 * @param diagnostics 可选诊断输出。
 * @return 校验成功返回 true。
 */
bool validateProgramFeatures(std::uint32_t required,
                             Memory::Vector<Memory::String> *diagnostics)
{
    const char *error = nullptr;
    if ((required & ~kKnownProgramFeatures) != 0)
        error = "不支持的程序特征标志";
    else if ((required & ~supportedProgramFeatures()) != 0)
        error = "程序要求反射支持，但运行时未启用 STACKVM_WITH_REFLECTION=1";
    if (error == nullptr) return true;
    if (diagnostics != nullptr) diagnostics->emplace_back(error);
    return false;
}
/**
 * @brief 检查指令宽度、能力位与反射表是否满足统一执行契约。
 * @param program 待校验程序。
 * @param diagnostics 接收失败说明。
 * @return 所有元数据合法且当前构建支持时返回真。
 */
bool validateProgramMetadata(const Program &program, Memory::Vector<Memory::String> *diagnostics)
{
    if (!validateProgramFeatures(program.requiredFeatures, diagnostics)) return false;
    for (const auto &instruction : program.code)
    {
        if (instruction.op != Op::LoadAt && instruction.op != Op::StoreAt) continue;
        if ((instruction.intValue != 0 && instruction.intValue != 1) || (instruction.intValue == 1 && (instruction.isFloat || !(program.requiredFeatures & kFeatureByteMemory))))
        {
            if (diagnostics) diagnostics->push_back("内存指令宽度无效或缺少 byte-memory 特征位");
            return false;
        }
    }
    Memory::String error;
    if (!(program.requiredFeatures & kFeatureReflection))
    {
        if (program.reflection.empty()) return true;
        error = "反射元数据缺少 reflection 特征位";
    }
    else if (reflection::validate(program.reflection, &error))
        return true;
    if (diagnostics) diagnostics->push_back(error);
    return false;
}
}  // namespace stackvm
