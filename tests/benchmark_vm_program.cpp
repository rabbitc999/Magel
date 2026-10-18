/**
 * @file benchmark_vm_program.cpp
 * @brief 加载真实 Magel 字节码后测量执行时间，不含前端与文件加载。
 */
#include "stackvm.hpp"
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <fstream>
/**
 * @brief 重复运行已加载程序并输出时间、步骤及校验值。
 * @param argc 参数数。
 * @param argv 依次为产物前缀、次数、优化开关、输入次数或以 @ 开头的输入文件、核心、标签分派、跳转折叠、专用码、基本块、运行时服务开关，末六项可省略。
 * @return 失败为一。
 */
int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    Memory::String prefix(argv[1]);
    stackvm::Program program;
    stackvm::DataArea data;
    Memory::Vector<Memory::String> errors;
    if (!stackvm::loadProgramBinary(prefix + ".svmb", program, &errors) || !data.load(prefix + ".data.bin", prefix + ".layout", &errors))
    {
        for (const auto &error : errors) std::cerr << error << '\n';
        return 1;
    }
    stackvm::Machine machine;
    machine.setData(&data);
#ifndef VM_BASELINE_ABI
    machine.setExecutionOptimization(argc < 4 || std::atoi(argv[3]) != 0);
    machine.setRegisterExecution(argc < 6 || std::atoi(argv[5]) != 0);
    if (argc > 6) machine.setRegisterThreadedDispatch(std::atoi(argv[6]) != 0);
    if (argc > 7) machine.setRegisterJumpFolding(std::atoi(argv[7]) != 0);
    if (argc > 8) machine.setRegisterSpecialization(std::atoi(argv[8]) != 0);
    if (argc > 9) machine.setRegisterBlockExecution(std::atoi(argv[9]) != 0);
    if (argc > 10) machine.setRegisterServices(std::atoi(argv[10]) != 0);
#endif
    const int repeats = argc > 2 ? std::atoi(argv[2]) : 7;
    Memory::Vector<Memory::String> input;
    if (argc > 4)
    {
        if (argv[4][0] == '@')
        {
            std::ifstream file(argv[4] + 1);
            if (!file)
            {
                std::cerr << "无法读取基准输入文件\n";
                return 2;
            }
            std::string token;
            while (file >> token) input.emplace_back(token.data(), token.size());
        }
        else
            input.emplace_back(argv[4]);
    }
    for (int i = -1; i < repeats; ++i)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto result = machine.run(program, input, 0);
        const auto end = std::chrono::steady_clock::now();
        if (!result.ok)
        {
            std::cerr << result.error << '\n';
            return 1;
        }
        auto checksum = result.output + result.text;
        while (!checksum.empty() && (checksum.back() == '\n' || checksum.back() == '\r')) checksum.pop_back();
        if (i >= 0) std::cout << std::chrono::duration<double, std::milli>(end - start).count() << '\t' << result.steps << '\t' << checksum << '\n';
#ifndef VM_BASELINE_ABI
        if (i == 0) std::cerr << "register dispatches=" << result.registerDispatches << " entries=" << result.registerEntries << '\n';
#endif
    }
}
