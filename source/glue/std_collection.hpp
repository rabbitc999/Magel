#pragma once
#include "stackvm.hpp"

namespace collection_glue
{
using NativeValue = stackvm::NativeValue;

/**
 * @brief 创建原生调用可用的运行时列表。
 * @retval 创建原生调用可用的运行时列表。
 * @param host 当前调用所属的公开运行时上下文。
 */
std::int64_t makeList(stackvm::NativeCallContext &host);

/**
 * @brief 将一个原生值追加到运行时列表。
 * @param listHandle 目标运行时列表的句柄。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool listPush(stackvm::NativeCallContext &host, std::int64_t listHandle, const NativeValue &value);

/**
 * @brief 将结构体数据追加到运行时列表。
 * @param listHandle 目标运行时列表的句柄。
 * @param structAddress 结构体数据的运行时地址。
 * @param slotCount 结构体数据包含的值槽数量。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool listPushStruct(stackvm::NativeCallContext &host, std::int64_t listHandle, std::int64_t structAddress,
                    std::size_t slotCount);

/**
 * @brief 从列表项读取结构体数据。
 * @param handle 待操作的运行时句柄。
 * @param slotCount 结构体数据包含的值槽数量。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool readListStruct(stackvm::NativeCallContext &host, std::int64_t handle, std::size_t slotCount,
                    Memory::Vector<double> &out);

/**
 * @brief 将运行时列表转换为原生值数组。
 * @param handle 待操作的运行时句柄。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool readList(stackvm::NativeCallContext &host, std::int64_t handle,
              Memory::Vector<NativeValue> &out);

/**
 * @brief 创建原生调用可用的运行时映射。
 * @retval 创建原生调用可用的运行时映射。
 * @param host 当前调用所属的公开运行时上下文。
 */
std::int64_t makeMap(stackvm::NativeCallContext &host);

/**
 * @brief 创建原生调用可用的运行时集合。
 * @retval 创建原生调用可用的运行时集合。
 * @param host 当前调用所属的公开运行时上下文。
 */
std::int64_t makeSet(stackvm::NativeCallContext &host);

/**
 * @brief 向运行时映射写入一个键值对。
 * @param mapHandle 目标运行时映射的句柄。
 * @param key 查找或写入使用的键。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool mapSet(stackvm::NativeCallContext &host, std::int64_t mapHandle, const NativeValue &key,
            const NativeValue &value);

/**
 * @brief 将原生值插入运行时集合。
 * @param setHandle 目标运行时集合的句柄。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool setInsert(stackvm::NativeCallContext &host, std::int64_t setHandle, const NativeValue &value);

/**
 * @brief 读取运行时映射的全部键值。
 * @param handle 待操作的运行时句柄。
 * @param keys 接收映射键的输出列表。
 * @param values 输入值列表或接收读取值的输出列表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool readMap(stackvm::NativeCallContext &host, std::int64_t handle, Memory::Vector<NativeValue> &keys,
             Memory::Vector<NativeValue> &values);

/**
 * @brief 读取运行时集合中的原生值。
 * @param handle 待操作的运行时句柄。
 * @param elements 元素类型或元素值列表。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 * @param host 当前调用所属的公开运行时上下文。
 */
bool readSet(stackvm::NativeCallContext &host, std::int64_t handle, Memory::Vector<NativeValue> &elements);

/**
 * @brief 将集合胶水的内部槽函数绑定到公开的原生注册表。
 * @param registry 目标注册表。
 * @param error 接收注册失败原因。
 * @return 全部绑定成功时返回 true。
 */
bool bind(stackvm::NativeRegistry &registry, Memory::String *error = nullptr);
}  // namespace collection_glue
