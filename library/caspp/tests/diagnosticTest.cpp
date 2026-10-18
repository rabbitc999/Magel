// SPDX-License-Identifier: MIT
// Copyright (c) 2026 小兔糖糖OvO

// 求解与诊断的收尾自检：
//  - 三角方程解里的反函数值要被折叠（不能漏出 asin(0) 这种半成品）
//  - 除以零要报确切原因，不能笼统说"无法解析表达式"
#include "cas/allocator.h"
#include "cas/engine.h"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void expectText(Cas::Engine *engine, const char *input, const char *expected) {
    const Cas::Result result = engine->evaluate(input);
    if (!result.ok) {
        std::printf("FAIL %-38s ← 错误：%s\n", input, result.message.c_str());
        ++failures;
        return;
    }
    if (result.text != expected) {
        std::printf("FAIL %-38s ← 得到 %s，期望 %s\n", input, result.text.c_str(), expected);
        ++failures;
        return;
    }
    std::printf("  %-36s = %s\n", input, result.text.c_str());
}

/** @brief 断言失败，且错误信息里含某个关键字。 */
void expectErrorWith(Cas::Engine *engine, const char *input, const char *keyword) {
    const Cas::Result result = engine->evaluate(input);
    if (result.ok) {
        std::printf("FAIL %-38s ← 本应报错，却得到 %s\n", input, result.text.c_str());
        ++failures;
        return;
    }
    if (result.message.find(keyword) == std::string::npos) {
        std::printf("FAIL %-38s ← 错误信息是 \"%s\"，不含 \"%s\"\n", input, result.message.c_str(), keyword);
        ++failures;
        return;
    }
    std::printf("  %-36s ↯ %s\n", input, result.message.c_str());
}

/* ------------------------------------------------ 反三角函数特殊值折叠 */

void testInverseTrigValues() {
    Cas::Engine engine;

    expectText(&engine, "trigsimp(asin(0))", "0");
    expectText(&engine, "trigsimp(asin(1))", "1/2*pi");
    expectText(&engine, "trigsimp(asin(-1))", "-1/2*pi");
    expectText(&engine, "trigsimp(asin(1/2))", "1/6*pi");
    expectText(&engine, "trigsimp(asin(-1/2))", "-1/6*pi");

    expectText(&engine, "trigsimp(acos(1))", "0");
    expectText(&engine, "trigsimp(acos(0))", "1/2*pi");
    expectText(&engine, "trigsimp(acos(-1))", "pi");
    expectText(&engine, "trigsimp(acos(1/2))", "1/3*pi");
    expectText(&engine, "trigsimp(acos(-1/2))", "2/3*pi");

    expectText(&engine, "trigsimp(atan(1))", "1/4*pi");
    expectText(&engine, "trigsimp(atan(-1))", "-1/4*pi");
    expectText(&engine, "trigsimp(atan(0))", "0");

    // 表外取值原样保留，不猜
    expectText(&engine, "trigsimp(asin(1/3))", "asin(1/3)");
}

/* ------------------------------------------ 三角方程的解不能是半成品 */

void testTrigEquations() {
    Cas::Engine engine;

    expectText(&engine, "solve(sin(x)=0, x)", "x = 0");
    expectText(&engine, "solve(sin(x)=1/2, x)", "x = 1/6*pi");
    expectText(&engine, "solve(cos(x)=1/2, x)", "x = 1/3*pi");
    expectText(&engine, "solve(cos(x)=0, x)", "x = 1/2*pi");
    expectText(&engine, "solve(exp(x)=1, x)", "x = 0");
}

/* ---------------------------------------------------------- 除以零诊断 */

void testDivisionByZero() {
    Cas::Engine engine;

    expectErrorWith(&engine, "1/0", "除以零");
    expectErrorWith(&engine, "x/0", "除以零");
    expectErrorWith(&engine, "1/(2-2)", "除以零");

    // 正常除法不受影响
    expectText(&engine, "1/2", "1/2");
    expectText(&engine, "x/2", "1/2*x");
}

} // namespace

int main() {
    std::printf("求解与诊断自检\n");
    Cas::Allocator::initialize();

    testInverseTrigValues();  std::printf("inverse trig values ok\n");
    testTrigEquations();      std::printf("trig equations ok\n");
    testDivisionByZero();     std::printf("division by zero ok\n");

    if (failures == 0) {
        std::printf("全部通过\n");
        return 0;
    }
    std::printf("%d 项失败\n", failures);
    return 1;
}
