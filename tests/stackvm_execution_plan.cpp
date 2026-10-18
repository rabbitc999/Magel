/**
 * @file stackvm_execution_plan.cpp
 * @brief 优化执行与逐条执行的差分回归，覆盖融合安全边界。
 */
#include "stackvm.hpp"
#include "allocation_index.hpp"
#include "execution_plan.hpp"
#include "register_core.hpp"
#include "data_capacity_index.hpp"
#include <iostream>
#include <fstream>
#include <iterator>
#include <cstdint>
namespace
{
int failures = 0, checks = 0;
/**
 * @brief 记录断言。
 * @param condition 条件。
 * @param context 上下文。
 * @return 无。
 */
void check(bool condition, const char *context)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cerr << "失败：" << context << '\n';
    }
}
/**
 * @brief 对照运行，检查输出、错误位置、步数和栈峰值。
 * @param source 四元式。
 * @param limit 步数上限。
 * @param mode 观察者类型。
 * @return 优化执行结果。
 */
stackvm::RunResult compare(const Memory::String &source, std::size_t limit = 10000, int mode = 0)
{
    auto file = stackvm::parseQuads(source, "<execution-differential>");
    stackvm::Program program;
    check(stackvm::assemble(file, program), "组装");
    stackvm::Machine slow(mode == 2), fast(mode == 2);
    slow.setExecutionOptimization(false);
    stackvm::Machine legacy;
    legacy.setRegisterExecution(false);
    Memory::Vector<std::size_t> slowHooks, fastHooks;
    if (mode == 1)
    {
        slow.setStackLimit(1);
        fast.setStackLimit(1);
    }
    if (mode == 3)
    {
        slow.setInstructionHook(1, [&](const stackvm::InstructionHookInfo &i) { slowHooks.push_back(i.steps); });
        fast.setInstructionHook(1, [&](const stackvm::InstructionHookInfo &i) { fastHooks.push_back(i.steps); });
    }
    if (mode == 4)
    {
        slow.setTimeCallback([] { return std::uint64_t(1); });
        fast.setTimeCallback([] { return std::uint64_t(1); });
    }
    const auto reference = slow.run(program, {}, limit), result = fast.run(program, {}, limit);
    if (mode == 0)
    {
        const auto previous = legacy.run(program, {}, limit);
        check(previous.ok == result.ok && previous.error == result.error && previous.output == result.output && previous.text == result.text && previous.steps == result.steps && previous.maxStackDepth == result.maxStackDepth, "新旧核心差分");
        for (unsigned options = 0; options < 16; ++options)
        {
            stackvm::Machine variant;
            variant.setRegisterJumpFolding(options & 1);
            variant.setRegisterSpecialization(options & 2);
            variant.setRegisterBlockExecution(options & 4);
            variant.setRegisterThreadedDispatch(options & 8);
            const auto actual = variant.run(program, {}, limit);
            check(actual.ok == reference.ok && actual.error == reference.error && actual.output == reference.output && actual.text == reference.text && actual.steps == reference.steps && actual.maxStackDepth == reference.maxStackDepth,
                  "跳转、专用码、基本块及两种分派组合差分");
        }
    }
    if (mode == 0)
    {
        stackvm::Machine withoutServices;
        withoutServices.setRegisterServices(false);
        const auto previous = withoutServices.run(program, {}, limit);
        check(previous.ok == reference.ok && previous.error == reference.error && previous.output == reference.output && previous.text == reference.text && previous.steps == reference.steps && previous.maxStackDepth == reference.maxStackDepth,
              "运行时服务和调用帧复用开关差分");
    }
    check(result.ok == reference.ok, "成功状态");
    check(result.output == reference.output && result.text == reference.text, "输出");
    check(result.error == reference.error, "错误及位置");
    check(result.steps == reference.steps, "逻辑步数");
    check(result.maxStackDepth == reference.maxStackDepth, "栈峰值");
    check(result.trace == reference.trace, "逐条轨迹");
    check(slowHooks == fastHooks, "指令钩子");
    if (mode >= 2) check(result.fusedGroups == 0, "受观察执行逐条运行");
    return result;
}
}  // namespace
/**
 * @brief 运行整数差分、观察边界和分配身份回归。
 * @return 失败为一。
 */
int main(int argc, char **argv)
{
    const Memory::String floatCode = "(decl, f32, global, x)\n(:=, 16777216.0, _, x)\n(+:f32, x, 1.0, t1)\n(:=, t1, _, x)\n(*:f32, x, 0.5, t2)\n(<:f32, t2, 9000000.0, t3)\n(jz, t3, _, done)\n(write, t2, _, _)\n(label, _, _, done)\n(halt, _, _, _)\n";
    check(compare(floatCode).fusedGroups > 0, "浮点立即数及比较进入融合路径");
    for (std::size_t limit = 1; limit < 35; ++limit) compare(floatCode, limit);
    for (int mode = 1; mode <= 4; ++mode) compare(floatCode, 10000, mode);
    compare("(decl, f64, global, x)\n(:=, 2.5, _, x)\n(+:f64, x, 1, t1)\n(*:f64, x, t1, t2)\n(write, t2, _, _)\n(halt, _, _, _)\n");
    stackvm::DataArea indexedData;
    for (std::size_t i = 0; i < 1000; ++i)
    {
        const auto name = Memory::toString(i);
        indexedData.symbols[name] = i * 16;
        indexedData.sizes[name] = 8;
    }
    stackvm::execution_detail::DataCapacityIndex dataIndex(&indexedData);
    std::size_t capacity = 0;
    for (std::size_t i = 0; i < 16000; ++i)
    {
        const auto found = dataIndex.capacityAt(i, capacity);
        check(found == (i % 16 < 8), "静态区间与间隙");
        if (found) check(capacity == 8 - i % 16, "静态区间剩余容量");
    }
    indexedData.symbols["overlap"] = 4;
    indexedData.sizes["overlap"] = 16;
    stackvm::execution_detail::DataCapacityIndex overlapping(&indexedData);
    check(overlapping.capacityAt(5, capacity) && capacity == 3, "重叠符号保持原名字顺序");
    const Memory::String heapServices =
        "0: (decl, i64, global, p)\n1: (decl, i8, global, value)\n2: (alloc, 16, _, p)\n"
        "3: (:=, 0, _, index)\n4: (checkptr, index, p, 8)\n5: (store:i64, p, 255, _)\n"
        "6: (load:i64, p, _, value)\n7: (write, value, _, _)\n8: (free, p, _, _)\n"
        "9: (load:i64, p, _, value)\n10: (halt, _, _, _)\n";
    const Memory::String calls =
        "0: (:=, 3, _, arg)\n1: (param, arg, _, x)\n2: (call, twice, _, answer)\n"
        "3: (param, answer, _, x)\n4: (call, twice, _, answer)\n5: (write, answer, _, _)\n6: (halt, _, _, _)\n"
        "7: (label, _, _, twice)\n8: (decl, i64, local, x)\n9: (decl, i64, local, y)\n"
        "10: (+:i64, x, x, y)\n11: (ret:i64, y, _, _)\n";
    for (const auto &position : { Memory::String("-1"), Memory::String("2"), Memory::String("9223372036854775807") })
    {
        const auto badIndex = Memory::String("0: (alloc, 16, _, p)\n1: (:=, ") + position + ", _, index)\n2: (checkptr, index, p, 8)\n3: (halt, _, _, _)\n";
        check(!compare(badIndex).ok, "指针负数、边界及溢出下标均失败");
        for (std::size_t limit = 1; limit < 14; ++limit) compare(badIndex, limit);
    }
    const Memory::String arrayCheck = "0: (:=, 1, _, index)\n1: (check, index, 2, array)\n"
                                      "2: (:=, 2, _, index)\n3: (check, index, 2, array)\n4: (halt, _, _, _)\n";
    check(!compare(arrayCheck).ok, "固定数组合法检查后越界仍报错");
    const Memory::String staleWrite = "0: (alloc, 8, _, p)\n1: (store:i64, p, 42, _)\n"
                                      "2: (free, p, _, _)\n3: (store:i64, p, 17, _)\n4: (halt, _, _, _)\n";
    check(!compare(staleWrite).ok, "释放后写入回到精确错误路径");
    const Memory::String aliasRead = "0: (alloc, 8, _, p)\n1: (store:i64, p, 42, _)\n"
                                     "2: (load:i64, p, _, p)\n3: (write, p, _, _)\n4: (halt, _, _, _)\n";
    check(compare(aliasRead).output == "42", "堆读目标与地址槽相同仍先取地址再写值");
    const Memory::String resetFrame =
        "0: (:=, 7, _, y)\n1: (:=, 1, _, arg)\n2: (param, arg, _, flag)\n3: (call, choose, _, answer)\n"
        "4: (write, answer, _, _)\n5: (:=, 0, _, arg)\n6: (param, arg, _, flag)\n7: (call, choose, _, answer)\n"
        "8: (write, answer, _, _)\n9: (halt, _, _, _)\n10: (label, _, _, choose)\n11: (decl, i64, local, flag)\n"
        "12: (jz, flag, _, done)\n13: (decl, i64, local, y)\n14: (:=, 99, _, y)\n"
        "15: (label, _, _, done)\n16: (ret:i64, y, _, _)\n";
    check(compare(resetFrame).output == "99 7", "复用帧不残留上次声明状态，未执行局部声明仍回退全局");
    for (std::size_t limit = 1; limit < 65; ++limit) compare(resetFrame, limit);
    check(compare(calls).output == "12", "同一小帧多次复用仍正确传参与返回");
    check(compare(heapServices).output == "-1", "整数堆读写保留目标窄化，释放后读报告错误");
    for (const auto &source : { heapServices, calls })
        for (std::size_t limit = 1; limit < 65; ++limit)
            for (int mode = 0; mode < 5; ++mode) compare(source, limit, mode);
    Memory::String loop = "0: (decl, i64, global, a)\n1: (:=, 1, _, a)\n2: (+:i64, a, 2, a)\n3: (*:i64, a, a, a)\n4: (write, a, _, _)\n5: (halt, _, _, _)\n";
    check(compare(loop).fusedGroups > 0, "普通整数计算发生融合");
    check(sizeof(stackvm::register_core::Instruction) <= 32, "热指令结构限制为 32 字节");
    for (const auto &source : {
             Memory::String("0: (label, _, _, self)\n1: (j, _, _, self)\n"),
             Memory::String("0: (label, _, _, first)\n1: (j, _, _, second)\n2: (label, _, _, second)\n3: (j, _, _, first)\n"),
             Memory::String("0: (:=, 2, _, a)\n1: (+:i64, a, 3, temp)\n2: (:=, temp, _, a)\n3: (j, _, _, one)\n4: (label, _, _, one)\n5: (j, _, _, two)\n6: (label, _, _, two)\n7: (write, a, _, _)\n8: (halt, _, _, _)\n"),
             Memory::String("0: (:=, 1, _, a)\n1: (+:i64, a, 2, a)\n2: (*:i64, a, 3, a)\n3: (/:i64, a, 0, a)\n4: (write, a, _, _)\n5: (halt, _, _, _)\n") })
        for (std::size_t limit = 1; limit <= 64; ++limit) compare(source, limit);
    Memory::OutputStringStream chain;
    chain << "0: (j, _, _, chain0)\n";
    for (int i = 0; i < 48; ++i) chain << i * 2 + 1 << ": (label, _, _, chain" << i << ")\n"
                                       << i * 2 + 2 << ": (j, _, _, chain" << i + 1 << ")\n";
    chain << "97: (label, _, _, chain48)\n98: (:=, 42, _, a)\n99: (write, a, _, _)\n100: (halt, _, _, _)\n";
    for (std::size_t limit = 1; limit <= 64; ++limit) compare(chain.str(), limit);
    Memory::OutputStringStream straight;
    straight << "0: (decl, i64, global, a)\n1: (:=, 0, _, a)\n";
    for (int i = 0; i < 80; ++i) straight << i + 2 << ": (+:i64, a, 1, a)\n";
    straight << "82: (write, a, _, _)\n83: (halt, _, _, _)\n";
    for (std::size_t limit = 1; limit <= 350; ++limit) compare(straight.str(), limit);
    auto straightFile = stackvm::parseQuads(straight.str(), "<block-boundary>");
    stackvm::Program straightProgram;
    check(stackvm::assemble(straightFile, straightProgram), "直线块边界组装");
    stackvm::execution_detail::ExecutionPlan straightPlan(straightProgram);
    stackvm::register_core::Engine straightEngine(straightProgram, straightPlan);
    bool fullBlock = false;
    for (const auto &block : straightEngine.region(1).blocks)
    {
        check(block.count <= 16, "基本块长度上限");
        fullBlock |= block.count == 16;
    }
    check(fullBlock, "长直线区域生成批量执行块");
    const Memory::String copyAfter = "0: (decl, i8, global, temp)\n1: (decl, u8, global, target)\n2: (:=, 127, _, a)\n3: (+:i64, a, 2, temp)\n4: (:=, temp, _, target)\n5: (write, temp, _, _)\n6: (write, target, _, _)\n7: (halt, _, _, _)\n";
    check(compare(copyAfter).output == "-127 129", "合并保留临时槽及目标槽分别回绕");
    auto copyFile = stackvm::parseQuads(copyAfter, "<copy-after-arithmetic>");
    stackvm::Program copyProgram;
    check(stackvm::assemble(copyFile, copyProgram), "运算后复制组装");
    stackvm::execution_detail::ExecutionPlan copyPlan(copyProgram);
    stackvm::register_core::Engine copyEngine(copyProgram, copyPlan);
    bool combinedCopy = false;
    for (std::size_t pc = 0; pc < copyProgram.code.size(); ++pc)
        for (const auto &node : copyEngine.region(pc).code)
            if (node.flags & stackvm::register_core::CopyResult) combinedCopy = true;
    check(combinedCopy, "六条运算与复制指令合为一次核心分派");
    const Memory::String aliasCopy = "0: (:=, 7, _, a)\n1: (+:i64, a, 3, temp)\n2: (:=, temp, _, a)\n3: (write, temp, _, _)\n4: (write, a, _, _)\n5: (halt, _, _, _)\n";
    const Memory::String labeledCopy = "0: (:=, 7, _, a)\n1: (+:i64, a, 3, temp)\n2: (label, _, _, copy)\n3: (:=, temp, _, a)\n4: (write, temp, _, _)\n5: (write, a, _, _)\n6: (halt, _, _, _)\n";
    check(compare(aliasCopy).output == "10 10", "目标与运算输入别名仍保留临时值");
    auto labeledFile = stackvm::parseQuads(labeledCopy, "<copy-label>");
    stackvm::Program labeledProgram;
    check(stackvm::assemble(labeledFile, labeledProgram), "标签边界组装");
    stackvm::execution_detail::ExecutionPlan labeledPlan(labeledProgram);
    stackvm::register_core::Engine labeledEngine(labeledProgram, labeledPlan);
    bool crossedLabel = false;
    for (std::size_t pc = 0; pc < labeledProgram.code.size(); ++pc)
        for (const auto &node : labeledEngine.region(pc).code) crossedLabel |= bool(node.flags & stackvm::register_core::CopyResult);
    check(!crossedLabel, "复制入口标签阻止合并");
    for (const auto *source : { &copyAfter, &aliasCopy, &labeledCopy })
        for (int mode = 0; mode <= 4; ++mode)
            for (std::size_t limit = 1; limit <= 48; ++limit) compare(*source, limit, mode);
    for (int mode = 0; mode <= 4; ++mode)
        for (std::size_t limit = 1; limit <= 24; ++limit) compare(loop, limit, mode);
    const Memory::String branch = "0: (:=, 0, _, i)\n1: (label, _, _, loop)\n2: (<:i64, i, 12, condition)\n3: (jz, condition, _, done)\n4: (+:i64, i, 1, i)\n5: (j, _, _, loop)\n6: (label, _, _, done)\n7: (write, i, _, _)\n8: (write, condition, _, _)\n9: (halt, _, _, _)\n";
    auto branchFile = stackvm::parseQuads(branch, "<register-branch>");
    stackvm::Program branchProgram;
    check(stackvm::assemble(branchFile, branchProgram), "比较循环组装");
    stackvm::execution_detail::ExecutionPlan branchPlan(branchProgram);
    bool preparedBranch = false;
    for (const auto &instruction : branchPlan.registers)
        if (instruction.kind == stackvm::execution_detail::RegisterOp::IntegerBranch && instruction.span == 6) preparedBranch = true;
    check(preparedBranch, "准备阶段生成六指令比较跳转");
    const auto branchResult = compare(branch);
    check(branchResult.ok && branchResult.output == "12 0" && branchResult.fusedGroups > 0, "比较跳转及布尔临时值保存");
    check(branchResult.registerDispatches > 0, "默认进入独立寄存器核心");
    auto longBranch = branch;
    longBranch.replace(longBranch.find("12,"), 3, "10000,");
    const auto longResult = compare(longBranch, 1000000);
    check(longResult.ok && longResult.output == "10000 0" && longResult.registerDispatches >= 20000 && longResult.registerEntries < 100, "长循环在新核心内连续执行并定期返回安全点");
    for (std::size_t limit = 22520; limit <= 22550; ++limit) compare(longBranch, limit);
    for (int format = 0; format < 2; ++format)
    {
        const Memory::String path = "build/register-roundtrip-" + Memory::toString(format) + ".svmb";
        check(stackvm::saveProgramBinary(branchProgram, path, nullptr, format == 1), "内部指令保存");
        stackvm::Program loaded;
        check(stackvm::loadProgramBinary(path, loaded), "内部指令加载");
        check(loaded.preparedInternal && loaded.internalCode.size() == branchPlan.registers.size(), "产物携带内部序列");
        const auto result = stackvm::Machine().run(loaded);
        check(result.ok && result.output == branchResult.output && result.steps == branchResult.steps && result.fusedGroups == branchResult.fusedGroups, "产物直接执行一致");
    }
    {
        std::ifstream input("build/register-roundtrip-0.svmb", std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
        const auto write = [&](const char *path) {
            std::ofstream output(path, std::ios::binary);
            output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        };
        bytes[8] = 4;
        write("build/register-legacy.svmb");
        stackvm::Program rejected;
        rejected.name = "unchanged";
        check(!stackvm::loadProgramBinary("build/register-legacy.svmb", rejected) && rejected.name == "unchanged", "拒绝旧版本且不更改输出");
        bytes[8] = 5;
        bytes.back() ^= 1;
        std::uint32_t checksum = 2166136261u;
        for (std::size_t i = 24; i < bytes.size(); ++i)
        {
            checksum ^= bytes[i];
            checksum *= 16777619u;
        }
        for (unsigned i = 0; i < 4; ++i) bytes[20 + i] = static_cast<unsigned char>(checksum >> (i * 8));
        write("build/register-invalid.svmb");
        check(!stackvm::loadProgramBinary("build/register-invalid.svmb", rejected) && rejected.name == "unchanged", "校验和正确但内部指令损坏仍拒绝");
    }
    for (int mode = 0; mode <= 4; ++mode)
        for (std::size_t limit = 1; limit <= 240; ++limit) compare(branch, limit, mode);
    auto nonzeroBranch = branch;
    nonzeroBranch.replace(nonzeroBranch.find("<:i64"), 5, ">=:u64");
    nonzeroBranch.replace(nonzeroBranch.find("jz,"), 3, "jnz,");
    check(compare(nonzeroBranch).output == "12 1", "无符号比较非零跳转");
    for (std::size_t limit = 1; limit <= 240; ++limit) compare(nonzeroBranch, limit);
    std::uint64_t state = 1234567;
    for (const char *op : { "+:i8", "-:i32", "*:u32", "-:i64", "*:i64", "+:u64", "-:u64", "*:u64", "<:i64", ">=:u64", "==:i64", "!=:i64", "+:f64",
                            "/:i32", "%:u64", "&:i64", "|:i64", "^:i64", "<<:i64", ">>:i64", ">>:u64" })
    {
        for (int i = 0; i < 48; ++i)
        {
            state = state * 6364136223846793005ULL + 1;
            const auto a = static_cast<std::int64_t>(state);
            state = state * 6364136223846793005ULL + 1;
            const auto b = static_cast<std::int64_t>(state);
            Memory::OutputStringStream text;
            text << "0: (:=, " << a << ", _, a)\n1: (:=, " << b << ", _, b)\n2: (" << op << ", a, b, c)\n3: (:=, c, _, a)\n4: (write, c, _, _)\n5: (write, a, _, _)\n6: (halt, _, _, _)\n";
            compare(text.str());
        }
    }
    compare("0: (+:i64, missing, 1, value)\n1: (halt, _, _, _)\n");
    compare("0: (:=, 12, _, a)\n1: (:=, 0, _, b)\n2: (/:i64, a, b, a)\n3: (halt, _, _, _)\n");
    compare("0: (:=, -9223372036854775808, _, a)\n1: (:=, -1, _, b)\n2: (/:i64, a, b, a)\n3: (%:i64, a, b, b)\n4: (write, a, _, _)\n5: (write, b, _, _)\n6: (halt, _, _, _)\n");
    for (int i = 1; i < argc; ++i)
    {
        std::ifstream file(argv[i]);
        std::string source((std::istreambuf_iterator<char>(file)), {});
        compare(Memory::String(source.data(), source.size()), 1000000);
    }
    // 已缓存宿主入口在移除、重新注册和切换注册表后都必须失效。
    stackvm::NativeRegistry registry, other;
    stackvm::Machine machine;
    other.add("value", 0, [](stackvm::NativeCallContext &, const Memory::Vector<stackvm::NativeValue> &) { return stackvm::NativeValue::fromInt(30); });
    registry.add("value", 0, [](stackvm::NativeCallContext &, const Memory::Vector<stackvm::NativeValue> &) { return stackvm::NativeValue::fromInt(10); });
    registry.add("change", 0, [&](stackvm::NativeCallContext &, const Memory::Vector<stackvm::NativeValue> &) {
        registry.remove("value");
        registry.add("value", 0, [](stackvm::NativeCallContext &, const Memory::Vector<stackvm::NativeValue> &) { return stackvm::NativeValue::fromInt(20); });
        return stackvm::NativeValue::fromInt(0);
    });
    registry.add("switch", 0, [&](stackvm::NativeCallContext &, const Memory::Vector<stackvm::NativeValue> &) {machine.setNativeRegistry(&other);return stackvm::NativeValue::fromInt(0); });
    auto dynamic = stackvm::parseQuads("0: (call, value, _, a)\n1: (write, a, _, _)\n2: (call, change, _, _)\n3: (call, value, _, a)\n4: (write, a, _, _)\n5: (call, switch, _, _)\n6: (call, value, _, a)\n7: (write, a, _, _)\n8: (halt, _, _, _)\n", "<registry-cache>");
    stackvm::Program dynamicProgram;
    check(stackvm::assemble(dynamic, dynamicProgram), "注册表测试组装");
    machine.setNativeRegistry(&registry);
    const auto dynamicResult = machine.run(dynamicProgram);
    check(dynamicResult.ok && dynamicResult.output == "10 20 30", "宿主注册表版本及对象切换");
    stackvm::execution_detail::AllocationIndex index;
    index.reset(8);
    const auto first = index.add(0, 16);
    check(index.find(first + 15) != nullptr, "区间末尾有效");
    check(!index.find(first + 16), "区间末尾越界");
    check(index.release(first), "首次释放");
    const auto second = index.add(0, 16);
    check(first != second, "物理复用逻辑身份不同");
    check(index.release(first), "重复释放幂等");
    check(index.find(second) != nullptr && !index.find(first), "旧身份不影响新分配及缓存");
    check(!index.release(second + 1), "内部地址不能释放");
    check(index.sweep() == 1, "清扫旧身份并保留活跃分配");
    check(!index.release(first) && index.find(second), "清扫后旧引用失去幂等保证");
    check(index.release(second) && index.release(second), "跨清扫存活对象仍可幂等释放");
    check(index.sweep() == 1 && !index.release(second), "再次清扫撤销后续墓碑");
    for (int i = 0; i < 100000; ++i)
    {
        const auto address = index.add(0, 16);
        check(address && index.release(address), "长期复用");
    }
    check(index.slotCount() == 1, "十万次分配只保留一个物理槽");
    index.reset(static_cast<std::size_t>(-2));
    check(!index.available(16), "逻辑空间溢出保护");
    std::cout << "执行优化差分：" << checks << " 项断言，" << failures << " 项失败\n";
    return failures ? 1 : 0;
}
