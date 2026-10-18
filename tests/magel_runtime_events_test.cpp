/**
 * @file magel_runtime_events_test.cpp
 * @brief 验证宿主调用结束后的事件投递、AOT 泵钩子与 VM 共用接口。
 */
#include "stackvm.hpp"
#include <iostream>
#include <stdexcept>

/**
 * @brief 在发布构建中仍执行断言。
 * @param condition 条件。
 * @param expression 表达式。
 * @param line 行号。
 */
static void check(bool condition, const char *expression, int line)
{
    if (!condition)
    {
        std::cerr << "runtime event check failed at " << line << ": " << expression << '\n';
        throw std::runtime_error("runtime event regression");
    }
}
#define CHECK(condition) check(static_cast<bool>(condition), #condition, __LINE__)

/**
 * @brief 验证运行时事件的延迟交付、注销与执行边界钩子。
 * @return 成功返回零。
 */
int main()
{
    using namespace stackvm;
    NativeRegistry registry;
    Memory::Function<void(std::int64_t, std::int64_t)> post;
    int hooks = 0, delivered = 0;
    CHECK(registry.add("host.setup", 0u, [&](NativeCallContext &context, const Memory::Vector<NativeValue> &) {
        post = context.interruptPoster();
        CHECK(post);
        CHECK(context.setInstructionHook(2u, [&](const InstructionHookInfo &) { ++hooks; }));
        return NativeValue::fromInt(1);
    }));
    CHECK(registry.add("host.stop", 0u, [](NativeCallContext &context, const Memory::Vector<NativeValue> &) {
        CHECK(context.setInstructionHook(0u, {}));
        return NativeValue::fromInt(1);
    }));
    CHECK(registry.add("host.throw", 0u, [](NativeCallContext &context, const Memory::Vector<NativeValue> &) {
        CHECK(context.setInstructionHook(1u, [](const InstructionHookInfo &) { throw std::runtime_error("hook failure"); }));
        return NativeValue::fromInt(1);
    }));
    DataArea data;
    data.binary.resize(8u, 0u);
    RuntimeBridge bridge;
    bridge.setData(&data);
    bridge.setNativeRegistry(&registry);
    bridge.setFunctionRunner([&](RuntimeBridge &, std::int64_t target, const Memory::Vector<RuntimeValue> &args, RuntimeValue &) {
        CHECK(target == 123 && args.size() == 1 && args[0].integer == 42);
        ++delivered;
        return true;
    });
    CHECK(bridge.begin());
    RuntimeValue result;
    CHECK(bridge.callNative("host.setup", {}, result));
    CHECK(bridge.boundary() && hooks == 0);
    CHECK(bridge.boundary() && hooks == 1);  // 没有活动定时器也必须运行泵。
    CHECK(bridge.callNative("std::event::__timerCreate", { RuntimeValue::ofInt(0) }, result));
    const auto timer = result.integer;
    CHECK(bridge.callNative("std::event::__timerBind", { RuntimeValue::ofInt(timer), RuntimeValue::ofInt(123) }, result));
    post(timer, 42);  // 原 NativeCallContext 已销毁，投递器仍有效。
    CHECK(delivered == 0);
    CHECK(bridge.boundary() && delivered == 1);
    CHECK(bridge.callNative("std::event::__timerDestroy", { RuntimeValue::ofInt(timer) }, result));
    post(timer, 42);
    CHECK(bridge.boundary() && delivered == 1);
    CHECK(bridge.callNative("host.stop", {}, result));
    const int stopped = hooks;
    CHECK(bridge.boundary() && bridge.boundary() && hooks == stopped);
    CHECK(bridge.callNative("host.throw", {}, result));
    CHECK(!bridge.boundary() && bridge.error().find("hook failure") != Memory::String::npos);
    post = {};
    bridge.end();
    CHECK(bridge.begin());
    CHECK(bridge.boundary() && hooks == stopped);  // begin 不继承旧钩子。
    bridge.end();

    QuadFile file = parseQuads("0: (call, host.setup, _, result)\n1: (:=, 1, _, value)\n2: (call, host.stop, _, ignored)\n3: (halt, _, _, _)\n", "<host-events>");
    Program program;
    CHECK(assemble(file, program));
    Machine machine;
    machine.setData(&data);
    machine.setNativeRegistry(&registry);
    CHECK(machine.run(program).ok);
    CHECK(post && hooks > stopped);
    post = {};
    std::cout << "VM/AOT host events and boundary hooks passed\n";
}
