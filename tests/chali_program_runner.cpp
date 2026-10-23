/**
 * @file chali_program_runner.cpp
 * @brief 使用共享 VM 和标准胶水执行字节码，供持久化应用回归传入真实程序参数。
 */
#include "../source/stackvm/stackvm.hpp"
#include "../source/chali_glue.hpp"
#include "../source/glue/std_registry.hpp"
#include "../source/glue/std_math.hpp"
#include <iostream>
#include "../source/chali_i18n.hpp"

/**
 * @brief 加载同前缀字节码与数据映像，执行程序并检查停机资源回收。
 * @param argc 参数数量，至少包含产物前缀。
 * @param argv 第一项用户参数为产物前缀，其余参数原样传给 ChaLi main。
 * @return 用法错误为 2，VM 或资源回收错误为 1，正常时为程序退出码。
 */
int main(int argc, char **argv)
{
    tool_console::Session console;
    const auto values = chali_i18n::processArguments(argc, argv);
    if (values.size() < 2) return 2;
    const Memory::String prefix(values[1].c_str());
    stackvm::Program program;
    stackvm::DataArea data;
    Memory::Vector<Memory::String> errors;
    if (!stackvm::loadProgramBinary(prefix + ".svmb", program, &errors) || !data.load(prefix + ".data.bin", prefix + ".layout", &errors))
    {
        for (const auto &error : errors) std::cerr << error << '\n';
        return 1;
    }
    chali_glue::Registry registry;
    Memory::String error;
    if (!std_registry::bindStandardGlue(registry, &error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    stackvm::Machine machine;
    machine.setData(&data);
    machine.setNativeRegistry(&registry.native());
    Memory::Vector<Memory::String> arguments;
    for (std::size_t index = 2; index < values.size(); ++index) arguments.emplace_back(values[index].c_str());
    machine.setProgramArguments(arguments);
    const auto result = machine.run(program, {}, 20000000);
    std::cout << result.output << result.text;
    if (!result.ok)
    {
        std::cerr << result.error << '\n';
        return 1;
    }
    if (result.heapLiveBytes || result.liveObjects)
    {
        std::cerr << "程序停机仍持有资源：" << result.heapLiveBytes << " 字节，" << result.liveObjects << " 个对象\n";
        return 1;
    }
    return result.exitCode;
}
