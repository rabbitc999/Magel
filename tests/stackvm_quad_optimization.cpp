/**
 * @file stackvm_quad_optimization.cpp
 * @brief 四元式优化前后差分，覆盖类型、复制与控制流边界。
 */
#include "../source/stackvm/stackvm.hpp"
#include <iostream>
#include <cstdint>

namespace
{
int checks = 0, failures = 0;
/**
 * @brief 记录断言。
 * @param ok 条件。
 * @param context 上下文。
 * @return 无。
 */
void check(bool ok, const char *context)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::cerr << context << '\n';
    }
}
/**
 * @brief 对照原始和优化后四元式的执行结果，不比较优化后有意变化的步数。
 * @param source 四元式。
 * @param input 输入。
 * @return 优化统计。
 */
stackvm::OptimizationReport compare(const Memory::String &source, const Memory::Vector<Memory::String> &input = {})
{
    auto plain = stackvm::parseQuads(source, "quad-optimization");
    auto optimized = plain;
    check(plain.ok(), "解析");
    const auto report = stackvm::optimize(optimized);
    stackvm::Program a, b;
    check(stackvm::assemble(plain, a), "原始组装");
    check(stackvm::assemble(optimized, b), "优化组装");
    stackvm::Machine first, second;
    first.setExecutionOptimization(false);
    second.setExecutionOptimization(false);
    const auto x = first.run(a, input, 10000), y = second.run(b, input, 10000);
    check(x.ok == y.ok && x.output == y.output && x.text == y.text, "优化前后输出或状态不同");
    if (x.ok != y.ok || x.output != y.output || x.text != y.text)
    {
        std::cerr << source << "原始：" << x.output << ' ' << x.error << "\n优化：" << y.output << ' ' << y.error << '\n';
    }
    return report;
}
}  // namespace

/**
 * @brief 运行确定性及随机优化差分。
 * @return 失败返回一。
 */
int main()
{
    compare("(decl, i64, global, x)\n(read, _, _, x)\n(<<:i64, x, 1, t1)\n(write, t1, _, _)\n(halt, _, _, _)\n", { "7" });
    compare("(decl, i64, global, x)\n(read, _, _, x)\n(+:i8, x, 0, t1)\n(write, t1, _, _)\n(halt, _, _, _)\n", { "129" });
    compare("(decl, i8, global, x)\n(:=, 255, _, x)\n(+:i64, x, 1, t1)\n(write, t1, _, _)\n(halt, _, _, _)\n");
    compare("(<<:i8, 127, 1, t1)\n(write, t1, _, _)\n(~:i8, 128, _, t2)\n(write, t2, _, _)\n(halt, _, _, _)\n");
    const auto copies = compare("(decl, i64, global, x)\n(read, _, _, x)\n(:=, x, _, t1)\n"
                                "(cvt:i64, t1, _, t2)\n(+:i64, t2, 2, t3)\n(:=, t3, _, result)\n(write, result, _, _)\n(halt, _, _, _)\n",
                                { "40" });
    check(copies.copiesPropagated > 0 && copies.conversionsRemoved > 0, "复制和同类型转换确实被优化");
    compare("(decl, i64, global, x)\n(read, _, _, x)\n(:=, x, _, t1)\n(:=, 99, _, x)\n(write, t1, _, _)\n(halt, _, _, _)\n", { "7" });
    compare("(decl, i64, global, x)\n(decl, i8, global, narrow)\n(read, _, _, x)\n"
            "(:=, x, _, narrow)\n(:=, narrow, _, t1)\n(write, t1, _, _)\n(halt, _, _, _)\n",
            { "255" });
    compare("(:=, missing, _, t1)\n(halt, _, _, _)\n");
    compare("(alloc, 8, _, p)\n(store:i64, p, 42, _)\n(:=, 99, _, t1)\n(load:i64, p, _, t1)\n(write, t1, _, _)\n(free, p, _, _)\n(halt, _, _, _)\n");
    compare("(decl, i64, global, x)\n(:=, 1, _, x)\n(param, 0, _, flag)\n(call, change, _, result)\n(write, x, _, _)\n(halt, _, _, _)\n"
            "(label, _, _, change)\n(:=, 7, _, x)\n(ret, 0, _, _)\n");
    const Memory::String branch = "(decl, i64, global, x)\n(read, _, _, x)\n(jz, x, _, zero)\n(:=, 7, _, t1)\n(j, _, _, done)\n"
                                  "(label, _, _, zero)\n(:=, 9, _, t1)\n(label, _, _, done)\n(write, t1, _, _)\n(halt, _, _, _)\n";
    compare(branch, { "0" });
    compare(branch, { "1" });
    compare("(decl, i64, global, x)\n(read, _, _, x)\n(:=, x, _, t1)\n(call, replace, _, x)\n(write, t1, _, _)\n(halt, _, _, _)\n"
            "(label, _, _, replace)\n(ret, 99, _, _)\n",
            { "7" });
    compare("(decl, f64, global, x)\n(read, _, _, x)\n(*:f64, x, 0, t1)\n(write, t1, _, _)\n(halt, _, _, _)\n", { "1.5" });
    compare("(:=, 99, _, hi)\n(mul:u32, 4294967295, 2, lo, hi)\n(write, hi, _, _)\n(halt, _, _, _)\n");
    compare("(:=, 99, _, remainder)\n(div:u32, 17, 5, quotient, remainder)\n(write, remainder, _, _)\n(halt, _, _, _)\n");
    compare("(decl, i64, global, x)\n(decl, i64, global, y)\n(read, _, _, x)\n(read, _, _, y)\n(:=, x, _, t1)\n(:=, y, _, x)\n(:=, t1, _, y)\n(write, x, _, _)\n(write, y, _, _)\n(halt, _, _, _)\n", { "7", "42" });
    const auto identity = compare("(decl, i64, global, x)\n(read, _, _, x)\n(+:i64, x, 0, t1)\n(*:i64, x, 1, t2)\n(write, t1, _, _)\n(write, t2, _, _)\n(halt, _, _, _)\n", { "42" });
    check(identity.algebraicSimplified >= 2, "整数加零与乘一确实化简");
    compare("(decl, i64, global, x)\n(read, _, _, x)\n(+:i8, x, 0, t1)\n(write, t1, _, _)\n(halt, _, _, _)\n", { "255" });
    const auto cse = compare("(decl, i64, global, x)\n(read, _, _, x)\n(*:i64, x, 3, t1)\n(*:i64, x, 3, t2)\n(write, t1, _, _)\n(write, t2, _, _)\n(halt, _, _, _)\n", { "19" });
    check(cse.commonExpressionsRemoved > 0, "公共子表达式确实消除");
    compare("(decl, i64, global, x)\n(read, _, _, x)\n(*:i64, x, 3, t1)\n(:=, 7, _, x)\n(*:i64, x, 3, t2)\n(write, t1, _, _)\n(write, t2, _, _)\n(halt, _, _, _)\n", { "19" });
    const auto loop = compare("(decl, i64, global, x)\n(decl, i64, global, i)\n(:=, 7, _, x)\n(:=, 0, _, i)\n(label, _, _, loop)\n(+:i64, i, x, t1)\n(:=, t1, _, i)\n(<:i64, i, 28, t2)\n(jnz, t2, _, loop)\n(write, i, _, _)\n(halt, _, _, _)\n");
    check(loop.crossBlockConstants > 0, "循环回边保留不变常量");
    for (const auto &value : { "0", "1" })
    {
        compare("(decl, i64, global, x)\n(read, _, _, x)\n(jz, x, _, zero)\n(:=, 7, _, t1)\n(j, _, _, done)\n(label, _, _, zero)\n(:=, 7, _, t1)\n(label, _, _, done)\n(write, t1, _, _)\n(halt, _, _, _)\n", { value });
        compare("(decl, i64, global, x)\n(read, _, _, x)\n(jz, x, _, zero)\n(:=, 7, _, t1)\n(j, _, _, done)\n(label, _, _, zero)\n(:=, 9, _, t1)\n(label, _, _, done)\n(write, t1, _, _)\n(halt, _, _, _)\n", { value });
    }
    compare("(:=, 7, _, x)\n(j, _, _, loop)\n(label, _, _, loop)\n(+:i64, x, 1, t1)\n(:=, t1, _, x)\n(<:i64, x, 10, t2)\n(jnz, t2, _, loop)\n(write, x, _, _)\n(halt, _, _, _)\n");
    auto traced = stackvm::parseQuads("(+:i64, 2, 3, t1)\n(write, t1, _, _)\n(halt, _, _, _)\n", "trace");
    auto silent = traced;
    check(stackvm::optimize(silent).trace.empty(), "默认不分配逐项日志");
    check(!stackvm::optimize(traced, 4, true).trace.empty(), "显式请求优化日志");
    std::uint64_t random = 0x123456789abcdefULL;
    const char *types[] = { "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64" };
    const char *ops[] = { "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "==", "!=", "<", "<=", ">", ">=" };
    for (int i = 0; i < 1500; ++i)
    {
        random = random * 6364136223846793005ULL + 1;
        const auto value = static_cast<std::int64_t>(random);
        const auto right = static_cast<std::int64_t>((random >> 48) % 17 + 1);
        const Memory::String type = types[(random >> 32) % 8], op = ops[(random >> 40) % 16];
        const auto prefix = Memory::String("(decl, ") + type + ", global, x)\n";
        const auto tail = Memory::String("(:=, x, _, t1)\n(") + op + ":" + type + ", t1, " + Memory::toString(right) + ", t2)\n"
                                                                                                                       "(:=, t2, _, t3)\n(cvt:"
                          + type + ", t3, _, t4)\n(write, t4, _, _)\n(halt, _, _, _)\n";
        compare(prefix + "(read, _, _, x)\n" + tail, { Memory::toString(value) });
        compare(prefix + "(:=, " + Memory::toString(value) + ", _, x)\n" + tail);
    }
    std::cout << "四元式优化差分：" << checks << " 项断言，" << failures << " 项失败\n";
    return failures ? 1 : 0;
}
