#include "chali_glue.hpp"
#include "glue/std_collection.hpp"
#include "glue/std_native_record.hpp"
#include "glue.cpp"

#include <cstdio>

/**
 * @brief 加载 ChaLi 字节码，注册宿主胶水并在默认最高优化 VM 中运行。
 * @param argc 参数个数，至少为四个。
 * @param argv 依次为宿主路径、字节码、数据区和布局文件路径，其余参数传给 ChaLi main。
 * @return 正常完成返回程序退出码；参数或加载失败返回 2，运行失败返回 1。
 */
int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    stackvm::Program program;
    stackvm::DataArea data;
    Memory::Vector<Memory::String> diagnostics;
    chali_glue::Registry registry;
    Memory::String error;
    if (!stackvm::loadProgramBinary(argv[1], program, &diagnostics)
        || !data.load(argv[2], argv[3], &diagnostics)
        || !collection_glue::bind(registry.native(), &error)
        || !native_record_glue::bindString(registry.native(), &error)
        || !bindChaLiGlue(registry, &error))
    {
        for (const auto &message : diagnostics) std::fprintf(stderr, "%s\n", message.c_str());
        if (!error.empty()) std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    stackvm::Machine machine;
    machine.setData(&data);
    machine.setNativeRegistry(&registry.native());
    Memory::Vector<Memory::String> arguments;
    for (int index = 4; index < argc; ++index) arguments.emplace_back(argv[index]);
    machine.setProgramArguments(std::move(arguments));
    const auto result = machine.run(program, {}, 1000000u);
    std::fwrite(result.text.data(), 1, result.text.size(), stdout);
    if (!result.ok)
    {
        std::fprintf(stderr, "%s\n", result.error.c_str());
        return 1;
    }
    if (result.heapLiveBlocks || result.liveObjects)
    {
        std::fprintf(stderr, "回调结束后仍有活动资源\n");
        return 1;
    }
    return result.exitCode;
}
