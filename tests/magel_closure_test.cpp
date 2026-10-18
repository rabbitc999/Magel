/**
 * @file magel_closure_test.cpp
 * @brief 捕获环境的独立复制、释放及失效句柄单元测试。
 */
#include "stackvm.hpp"
#include <cstdlib>
#include <iostream>

/**
 * @brief 验证条件。
 * @param condition 断言。
 * @param message 失败说明。
 */
void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

/**
 * @brief 验证环境独占所有权，不依靠虚拟机退出时兜底清理。
 * @return 成功返回零。
 */
int main()
{
    using Value = stackvm::RuntimeValue;
    stackvm::DataArea data;
    data.binary.assign(32, 0);
    data.binary[8] = 'f';
    data.binary[16] = '@';
    data.binary[17] = 's';
    data.binary[18] = 'i';
    data.binary[19] = 'o';
    stackvm::RuntimeBridge runtime;
    runtime.setData(&data);
    require(runtime.begin(), "runtime begin");
    auto call = [&](const char *name, Memory::Vector<Value> args) {
        Value result;
        require(runtime.callNative(name, args, result), name);
        return result;
    };
    const auto original = call("__vm_closure_new", { Value::ofInt(8), Value::ofInt(1) });
    call("__vm_closure_capture", { original, Value::ofInt(7), Value::ofInt(0), Value::ofInt(0) });
    const auto list = call("__vm_vec_new", {});
    call("__vm_vec_push", { list, Value::ofInt(11), Value::ofInt(0), Value::ofInt(0) });
    call("__vm_closure_capture", { original, list, Value::ofInt(0), Value::ofInt(1) });
    const auto copy = call("__vm_function_copy", { original });
    require(copy.integer != original.integer, "closure copies must be independent");
    require(runtime.release(original.integer, true), "release original");
    require(runtime.release(list.integer, true), "release source list");
    require(!runtime.alive(original.integer) && runtime.alive(copy.integer), "copy outlives original");
    std::int64_t environment = 0;
    require(runtime.resolveFunction(copy.integer, environment) == 8 && environment == copy.integer, "resolve environment");
    require(call("__vm_closure_get", { copy, Value::ofInt(0) }).integer == 7, "captured scalar");
    const auto first = call("__vm_closure_get", { copy, Value::ofInt(1) });
    const auto second = call("__vm_closure_get", { copy, Value::ofInt(1) });
    require(first.integer != second.integer, "each invocation receives a separate owned list");
    require(call("__vm_vec_at", { first, Value::ofInt(0), Value::ofInt(0), Value::ofInt(0) }).integer == 11, "deep copy");
    require(runtime.release(first.integer, true) && runtime.release(second.integer, true), "release capture copies");
    require(runtime.release(copy.integer, true) && !runtime.alive(copy.integer), "release copied environment");
    require(call("__vm_function_copy", { Value::ofInt(8) }).integer == 8, "static function address remains stable");
    require(runtime.resolveFunction(8, environment) == 8 && environment == 0, "legacy function pointer");
    // 模拟枚举：未激活字符串槽、标量标签与拥有型载荷；源释放后反复实体化独立副本。
    std::int64_t source = 0;
    require(runtime.allocate(24, source), "allocate aggregate");
    const auto payload = call("__vm_vec_new", {});
    call("__vm_vec_push", { payload, Value::ofInt(19), Value::ofInt(0), Value::ofInt(0) });
    require(runtime.storeMemory(source, false, Value::ofInt(0)) && runtime.storeMemory(source + 8, false, Value::ofInt(7)) && runtime.storeMemory(source + 16, false, payload), "initialize aggregate");
    const auto aggregate = call("__vm_closure_new", { Value::ofInt(8), Value::ofInt(1) });
    call("__vm_closure_capture", { aggregate, Value::ofInt(source), Value::ofInt(0), Value::ofInt(16) });
    require(runtime.release(source, true) && runtime.release(payload.integer, true), "release aggregate source");
    for (int i = 0; i < 50; ++i)
    {
        const auto value = call("__vm_closure_get", { aggregate, Value::ofInt(0) });
        Value slot;
        require(runtime.loadMemory(value.integer, false, slot) && slot.integer == 0, "inactive payload remains zero");
        require(runtime.loadMemory(value.integer + 8, false, slot) && slot.integer == 7, "aggregate scalar");
        require(runtime.loadMemory(value.integer + 16, false, slot), "aggregate owned payload");
        require(call("__vm_vec_at", { slot, Value::ofInt(0), Value::ofInt(0), Value::ofInt(0) }).integer == 19,
                "aggregate deep copy");
        require(runtime.release(slot.integer, true) && runtime.release(value.integer, true), "release aggregate invocation");
    }
    require(runtime.release(aggregate.integer, true), "release aggregate environment");
    require(runtime.resolveFunction(copy.integer, environment) == 0 && runtime.failed(), "reject released environment");
    runtime.end();
    std::cout << "closure unit tests OK\n";
}
