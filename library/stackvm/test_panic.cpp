#include "stackvm.hpp"
#include <iostream>
#include <cstdio>
/**
 * @brief 验证 panic 调用栈开关与压缩产物不泄露错误路径。
 * @param argc 数量。
 * @param argv 输出路径。
 * @return 零表示通过。
 */
int main(int argc, char **argv)
{
    stackvm::DataArea data;
    const Memory::String message = "fatal";
    data.binary.resize(9 + message.size(), 0);
    for (size_t i = 0; i < message.size(); ++i) data.binary[8 + i] = static_cast<uint8_t>(message[i]);
    const auto quads = stackvm::parseQuads(
        "0: (label, _, _, main)\n1: (call, outer, _, x)\n2: (halt, _, _, _)\n"
        "3: (label, _, _, outer)\n4: (call, inner, _, y)\n5: (ret, 0, _, _)\n"
        "6: (label, _, _, inner)\n7: (param, 8, _, message)\n8: (call, __vm_panic, _, z)\n9: (ret, 0, _, _)\n",
        "private-path.quad");
    stackvm::Program program;
    if (!stackvm::assemble(quads, program)) return 1;
    stackvm::Machine vm;
    vm.setData(&data);
    auto check = [&](const stackvm::Program &p, bool hidden) {
        auto result = vm.run(p);
        if (result.ok || result.error != "panic：fatal")
        {
            std::cerr << result.error << "\n";
            return false;
        }
        if (hidden || !STACKVM_WITH_PANIC_TRACE) return result.sourceTrace.empty() && vm.callStack().empty() && (!hidden || !result.failureLocation.known());
        return result.sourceTrace.size() == 3 && result.sourceTrace[1].function == "outer" && result.sourceTrace.back().function == "inner" && vm.callStack().size() == 3;
    };
    if (!check(program, false)) return 2;
    program.debug = stackvm::DebugInfo{};
    if (!check(program, false)) return 3;
    const Memory::String path = argc > 1 ? argv[1] : "panic-compact.svmb";
    if (!stackvm::saveProgramBinary(program, path, nullptr, true)) return 4;
    stackvm::Program compact;
    if (!stackvm::loadProgramBinary(path, compact)) return 5;
    stackvm::DebugInfo external;
    stackvm::SourceLocation secret;
    secret.file = "secret.mgl";
    secret.function = "secret_function";
    secret.line = 7;
    external.add(0, secret);
    vm.attachDebugInfo(&external);
    if (!check(compact, true)) return 6;
    vm.attachDebugInfo(nullptr);
    vm.setPanicDetailsEnabled(false);
    if (!check(program, true)) return 7;
    vm.setPanicDetailsEnabled(true);
    if (!check(program, false)) return 8;
    std::remove(path.c_str());
    stackvm::RuntimeBridge runtime;
    runtime.setData(&data);
    if (!runtime.begin()) return 9;
    {
        stackvm::RuntimeCallFrame root(runtime, "main");
        stackvm::RuntimeCallFrame outer(runtime, "outer");
        stackvm::RuntimeCallFrame inner(runtime, "inner");
        stackvm::RuntimeValue out;
        if (runtime.callNative("__vm_panic", { stackvm::RuntimeValue::ofInt(8) }, out)) return 10;
    }
    auto trace = runtime.callStack();
    if (trace.size() != (STACKVM_WITH_PANIC_TRACE ? 3u : 0u)) return 11;
    if (STACKVM_WITH_PANIC_TRACE && trace.back().function != "inner") return 12;
    runtime.end();
    if (runtime.callStack().size() != trace.size()) return 13;
    if (!runtime.begin() || !runtime.callStack().empty()) return 14;
    runtime.end();
    // 两个真实线程：忙循环兄弟线程必须被另一个线程的 panic 取消。
    data.binary.resize(128, 0);
    data.writeBytes(32, "spin", 5);
    data.writeBytes(64, "crash", 6);
    const auto threadedQuads = stackvm::parseQuads(
        "0: (call, main, _, entry)\n"
        "1: (halt, _, _, _)\n"
        "2: (label, _, _, main)\n"
        "3: (param, 32, _, task)\n"
        "4: (call, std::thread::create, _, a)\n"
        "5: (param, 64, _, task)\n"
        "6: (call, std::thread::create, _, b)\n"
        "7: (label, _, _, main_loop)\n"
        "8: (j, _, _, main_loop)\n"
        "9: (label, _, _, spin)\n"
        "10: (label, _, _, spin_loop)\n"
        "11: (j, _, _, spin_loop)\n"
        "12: (label, _, _, crash)\n"
        "13: (call, inner, _, x)\n"
        "14: (ret, 0, _, _)\n"
        "15: (label, _, _, inner)\n"
        "16: (param, 8, _, message)\n"
        "17: (call, __vm_panic, _, z)\n"
        "18: (ret, 0, _, _)\n",
        "thread-panic.quad");
    stackvm::Program threadedProgram;
    if (!stackvm::assemble(threadedQuads, threadedProgram)) return 15;
    vm.setThreadsEnabled(true);
    const auto stopped = vm.run(threadedProgram, {}, 10000000);
    if (stopped.ok || stopped.error.find("线程 2 失败：panic：fatal") == Memory::String::npos)
    {
        std::cerr << stopped.error << "\n";
        for (const auto &frame : stopped.sourceTrace) std::cerr << frame.function << " thread=" << frame.threadId << "\n";
        for (const auto &c : stopped.coroutines) std::cerr << c.id << " " << c.state << "\n";
        return 16;
    }
    if (STACKVM_WITH_PANIC_TRACE && (stopped.sourceTrace.empty() || stopped.sourceTrace.back().threadId != 2 || stopped.sourceTrace.back().function != "inner")) return 17;
    if (!stackvm::saveProgramBinary(threadedProgram, path, nullptr, true) || !stackvm::loadProgramBinary(path, compact)) return 18;
    const auto hiddenStopped = vm.run(compact, {}, 10000000);
    if (hiddenStopped.ok || hiddenStopped.error.find("panic：fatal") == Memory::String::npos || !hiddenStopped.sourceTrace.empty() || hiddenStopped.failureLocation.known()) return 19;
    std::remove(path.c_str());
    // AOT 桥同样取消真实忙循环线程；不显式 wait，由 end() 回收。
    runtime.setThreadsEnabled(true);
    runtime.setFunctionRunner([](stackvm::RuntimeBridge &child, int64_t target, const Memory::Vector<stackvm::RuntimeValue> &, stackvm::RuntimeValue &) {
        stackvm::RuntimeCallFrame frame(child, target == 32 ? "spin" : "crash");
        if (target == 32)
        {
            while (child.boundary()) {}
            return false;
        }
        stackvm::RuntimeCallFrame inner(child, "inner");
        stackvm::RuntimeValue out;
        return child.callNative("__vm_panic", { stackvm::RuntimeValue::ofInt(8) }, out);
    });
    if (!runtime.begin()) return 20;
    stackvm::RuntimeValue thread;
    if (!runtime.callNative("std::thread::create", { stackvm::RuntimeValue::ofInt(32) }, thread)) return 21;
    runtime.callNative("std::thread::create", { stackvm::RuntimeValue::ofInt(64) }, thread);
    runtime.end();  // 即使第一位兄弟忙循环，第二位 panic 也必须令 join 返回。
    if (runtime.error().find("线程 2 失败：panic：fatal") == Memory::String::npos)
    {
        std::cerr << runtime.error() << "\n";
        return 22;
    }
    const auto childTrace = runtime.callStack();
    if (STACKVM_WITH_PANIC_TRACE && (childTrace.empty() || childTrace.back().threadId != 2 || childTrace.back().function != "inner")) return 23;
    if (!runtime.begin() || !runtime.boundary() || !runtime.callStack().empty()) return 24;
    runtime.end();
    std::cout << "panic trace, stripped debug, compression privacy, AOT, reset: PASS\n";
    return 0;
}
