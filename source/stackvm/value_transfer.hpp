/**
 * @file value_transfer.hpp
 * @brief VM 内部 value transfer 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 脱离发送方堆和对象表的消息快照；对象句柄仅作为快照内部的索引。
 */
struct TransferPacket
{
    Memory::String type;
    ObjectValue value;
    Memory::Map<std::int64_t, RuntimeObject> objects;
};
}  // namespace stackvm::runtimeDetail
