/**
 * @file chali_runtime_events_test.cpp
 * @brief 验证有界事件、弱引用宿主投递、定时合并及执行停止。
 */
#include "stackvm.hpp"
#include "event_service.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
/**
 * @brief 在发布构建中保留回归断言。
 * @param condition 必须满足的条件。
 * @param expression 条件文字。
 * @param line 失败位置。
 */
static void check(bool condition, const char *expression, int line)
{
    if (!condition)
    {
        std::cerr << line << ": " << expression << '\n';
        throw std::runtime_error("event regression");
    }
}
#define CHECK(value) check(static_cast<bool>(value), #value, __LINE__)
#if STACKVM_WITH_THREADS
/**
 * @brief 验证队列背压、并发投递、超时、周期积压合并和销毁。
 */
static void testQueue()
{
    using namespace stackvm;
    const auto queue = std::make_shared<EventQueue>();
    queue->capacity = 8;
    std::atomic<int> accepted{ 0 };
    const auto producer = [&]() { for (int i = 0; i < 100; ++i) if (postEvent(queue, RuntimeEvent{0, i, 17, 0}) == TransferStatus::Ready) ++accepted; };
    std::thread first(producer), second(producer);
    first.join();
    second.join();
    CHECK(accepted == 8);
    CHECK(postEvent(queue, {}) == TransferStatus::Full);
    RuntimeEvent event;
    destroyEventTimer(queue, 0);
    CHECK(queue->pending.size() == 8);
    for (int i = 0; i < 8; ++i) CHECK(receiveEvent(queue, event, 0, [] { return false; }) == TransferStatus::Ready);
    CHECK(receiveEvent(queue, event, 0, [] { return false; }) == TransferStatus::Timeout);
    const auto id = createEventTimer(queue, 1000000, true);
    CHECK(id > 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    CHECK(receiveEvent(queue, event, 0, [] { return false; }) == TransferStatus::Ready);
    CHECK(event.timer == id && event.missed >= 3);
    destroyEventTimer(queue, id);
    CHECK(receiveEvent(queue, event, 0, [] { return false; }) == TransferStatus::Timeout);
    CHECK(receiveEvent(queue, event, -1, [] { return true; }) == TransferStatus::Cancelled);
    closeEventQueue(queue);
    CHECK(postEvent(queue, {}) == TransferStatus::Closed);
    CHECK(receiveEvent(queue, event, 0, [] { return false; }) == TransferStatus::Closed);
}
/**
 * @brief 验证宿主投递不会调用用户回调，结束后的投递器安全失效。
 */
static void testHost()
{
    using namespace stackvm;
    NativeRegistry registry;
    EventPoster post;
    int hooks = 0;
    CHECK(registry.add("host.setup", 0, [&](NativeCallContext &context, const Memory::Vector<NativeValue> &) {
        post = context.eventPoster();
        CHECK(post && post(RuntimeEvent{ 0, 12, 19, 0 }) == TransferStatus::Ready);
        CHECK(context.setInstructionHook(2, [&](const InstructionHookInfo &) { ++hooks; }));
        return NativeValue::fromInt(0);
    }));
    Program program;
    CHECK(assemble(parseQuads("0: (call, host.setup, _, _)\n1: (param, 0, _, millis)\n2: (call, std::event::__receive, _, status)\n3: (call, std::event::__source, _, value)\n4: (write, value, _, _)\n5: (halt, _, _, _)\n", "host-events"), program));
    Machine machine;
    machine.setNativeRegistry(&registry);
    const auto result = machine.run(program);
    CHECK(result.ok && result.output == "19" && hooks > 0);
    CHECK(post(RuntimeEvent{}) == TransferStatus::Closed);
    Memory::restartSignal().recordFailure(Memory::WatchdogReason::PoolExhausted, 1, 0, 0, 0);
    const auto early = machine.run(program);
    Memory::restartSignal().clear();
    CHECK(early.recoverable && !early.ok);
    CHECK(machine.eventPoster()(RuntimeEvent{}) == TransferStatus::Closed);
}
/**
 * @brief 验证宿主投递器不延长 VM 内存池生命周期，池销毁后仍安全返回关闭。
 */
static void testHostPool()
{
    using namespace stackvm;
    EventPoster saved;
    std::atomic<bool> ready{ false }, posted{ false }, done{ false };
    NativeRegistry registry;
    CHECK(registry.add("host.save", 0, [&](NativeCallContext &context, const Memory::Vector<NativeValue> &) {
        saved = context.eventPoster();
        ready.store(true, std::memory_order_release);
        while (!posted.load(std::memory_order_acquire)) std::this_thread::yield();
        return NativeValue::fromInt(0);
    }));
    Program program;
    CHECK(assemble(parseQuads("0: (call, host.save, _, _)\n1: (halt, _, _, _)\n", "host-pool"), program));
    std::thread producer([&] {
        while (!ready.load(std::memory_order_acquire)) std::this_thread::yield();
        while (!done.load(std::memory_order_acquire))
        {
            saved(RuntimeEvent{});
            posted.store(true, std::memory_order_release);
        }
    });
    bool retainedPool = false;
    {
        Memory::DynamicRuntimeArena arena(1024u * 1024u);
        {
            Memory::AllocationScope scope(&arena);
            {
                Machine machine;
                machine.setNativeRegistry(&registry);
                const auto result = machine.run(program);
                CHECK(result.ok);
            }
            retainedPool = arena.stats().liveBlocks != 0;
            if (retainedPool)
            {
                done.store(true, std::memory_order_release);
                producer.join();
                saved = {};
            }
            CHECK(arena.stats().liveBlocks == 0);
        }
    }
    done.store(true, std::memory_order_release);
    if (producer.joinable()) producer.join();
    CHECK(!retainedPool);
    CHECK(saved(RuntimeEvent{}) == TransferStatus::Closed);
}
/**
 * @brief 记录不透明资源的销毁，并释放宿主持有的计数器包装。
 * @param record 指向外部原子计数器的独占包装指针。
 */
static void destroyCountedResource(void *record)
{
    auto *counter = static_cast<std::atomic<int> **>(record);
    ++**counter;
    delete counter;
}
/**
 * @brief 验证计算循环和无限事件等待均响应宿主停止请求，并销毁原生资源。
 */
static void testStop()
{
    using namespace stackvm;
    for (const bool wait : { false, true })
    {
        std::atomic<int> destroyed{ 0 };
        const NativeRecordType resource{ "event-test-resource", nullptr, destroyCountedResource };
        NativeRegistry registry;
        CHECK(registry.add("host.allocate", 0, [&](NativeCallContext &context, const Memory::Vector<NativeValue> &) {
            auto *record = new std::atomic<int> *(&destroyed);
            const auto handle = context.makeNativeRecord(resource, record);
            if (!handle) delete record;
            CHECK(handle != 0);
            return NativeValue::fromInt(handle);
        }));
        Program program;
        const auto quads = wait ? "0: (param, -1, _, millis)\n1: (call, std::event::__receive, _, value)\n2: (halt, _, _, _)\n" : "0: (label, _, _, loop)\n1: (j, _, _, loop)\n";
        const Memory::String source = Memory::String("0: (call, host.allocate, _, _)\n") + quads;
        CHECK(assemble(parseQuads(source, "host-stop"), program));
        Machine machine;
        machine.setNativeRegistry(&registry);
        std::thread stop([&] { std::this_thread::sleep_for(std::chrono::milliseconds(20)); machine.requestStop(); });
        const auto result = machine.run(program);
        stop.join();
        CHECK(result.cancelled && !result.ok);
        CHECK(destroyed == 1);
    }
}
#endif
/**
 * @brief 执行独立事件服务与宿主 VM 协议回归。
 * @return 全部通过返回零。
 */
int main()
{
#if STACKVM_WITH_THREADS
    testQueue();
    testHost();
    testStop();
    testHostPool();
#endif
    std::cout << "VM bounded events, timers and cancellation passed\n";
}
