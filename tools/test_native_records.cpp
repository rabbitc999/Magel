/**
 * @file test_native_records.cpp
 * @brief 验证通用胶水记录的独立复制、类型隔离及一次销毁协议。
 */
#include "stackvm.hpp"
#include <cassert>
#include <iostream>

namespace
{
int copies = 0;
int destroys = 0;
/**
 * @brief 复制测试记录并统计真正执行的宿主复制次数。
 * @param source 原始整数记录。
 * @param error 复制错误输出，本测试不会失败。
 * @return 新分配的独立整数记录。
 */
void *cloneRecord(const void *source, Memory::String &error)
{
    (void)error;
    ++copies;
    return new int(*static_cast<const int *>(source));
}
/**
 * @brief 销毁一条测试记录并统计销毁次数。
 * @param record 当前运行时独占的整数记录。
 */
void destroyRecord(void *record)
{
    ++destroys;
    delete static_cast<int *>(record);
}
const stackvm::NativeRecordType copyable{ "copyable", cloneRecord, destroyRecord };
const stackvm::NativeRecordType unique{ "unique", nullptr, destroyRecord };
}  // namespace

/**
 * @brief 通过 AOT 公共桥验证与 VM 共用的对象存储和胶水上下文。
 * @return 所有生命周期断言通过时返回零。
 */
int main()
{
    using stackvm::NativeValue;
    using stackvm::RuntimeValue;
    stackvm::NativeRegistry registry;
    assert(registry.add("create", 0, [](stackvm::NativeCallContext &context, const Memory::Vector<NativeValue> &) {
        return NativeValue::fromInt(context.makeNativeRecord(copyable, new int(17)));
    }));
    assert(registry.add("copy", 1, [](stackvm::NativeCallContext &context, const Memory::Vector<NativeValue> &args) {
        return NativeValue::fromInt(context.cloneNativeRecord(args[0].asInt()));
    }));
    assert(registry.add("mutate", 1, [](stackvm::NativeCallContext &context, const Memory::Vector<NativeValue> &args) {
        auto *value = static_cast<int *>(context.nativeRecord(args[0].asInt(), copyable));
        assert(value && !context.nativeRecord(args[0].asInt(), unique));
        return NativeValue::fromInt(++*value);
    }));
    assert(registry.add("drop", 1, [](stackvm::NativeCallContext &context, const Memory::Vector<NativeValue> &args) {
        context.releaseNativeRecord(args[0].asInt());
        assert(!context.nativeRecord(args[0].asInt(), copyable));
        return NativeValue::fromInt(0);
    }));
    assert(registry.add("unique", 0, [](stackvm::NativeCallContext &context, const Memory::Vector<NativeValue> &) {
        return NativeValue::fromInt(context.makeNativeRecord(unique, new int(23)));
    }));
    stackvm::RuntimeBridge runtime;
    runtime.setNativeRegistry(&registry);
    assert(runtime.begin());
    RuntimeValue result;
    assert(runtime.callNative("create", {}, result));
    const auto original = result;
    assert(runtime.callNative("copy", { original }, result));
    const auto copied = result;
    assert(copies == 1 && copied.integer != original.integer);
    assert(runtime.callNative("mutate", { copied }, result) && result.integer == 18);
    assert(runtime.callNative("mutate", { original }, result) && result.integer == 18);
    assert(runtime.callNative("drop", { copied }, result));
    assert(runtime.callNative("drop", { copied }, result) && destroys == 1);
    assert(runtime.callNative("unique", {}, result));
    assert(!runtime.callNative("copy", { result }, result));
    assert(copies == 1);
    runtime.end();
    assert(destroys == 3);
    std::cout << "native records: independent copy, type identity, noncopyable and destruction passed\n";
}
