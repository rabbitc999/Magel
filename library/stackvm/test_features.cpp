/**
 * @file test_features.cpp
 * @brief 注解与反射能力契约、二进制及执行入口回归。
 */
#include "stackvm.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#define CHECK(condition)                                   \
    do                                                     \
    {                                                      \
        if (!(condition))                                  \
        {                                                  \
            std::cerr << __LINE__ << ": " #condition "\n"; \
            return 1;                                      \
        }                                                  \
    } while (0)

/**
 * @brief 读取测试二进制。
 * @param path 文件路径。
 * @return 文件内容。
 */
static std::vector<unsigned char> bytes(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}
/**
 * @brief 写入用于拒绝加载的测试文件。
 * @param path 路径。
 * @param data 内容。
 */
static void writeBytes(const std::string &path, const std::vector<unsigned char> &data)
{
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
}
/**
 * @brief 验证当前配置，并在共享目录中验证另一种配置生成的产物。
 * @param argc 参数数量。
 * @param argv 首参数为测试目录。
 * @return 成功返回零。
 */
int main(int argc, char **argv)
{
    const std::string directory = argc > 1 ? argv[1] : "feature-fixtures";
    std::filesystem::create_directories(directory);
    constexpr bool enabled = STACKVM_WITH_REFLECTION != 0;
    CHECK(stackvm::supportedProgramFeatures() == (stackvm::kFeatureByteMemory | (enabled ? stackvm::kFeatureReflection : 0)));
    CHECK(stackvm::validateProgramFeatures(0));
    CHECK(stackvm::validateProgramFeatures(stackvm::kFeatureReflection) == enabled);
    CHECK(!stackvm::validateProgramFeatures(0x80000000u));
    auto quads = stackvm::parseQuads("(write, 42, _, _)\n(halt, _, _, _)\n", "ordinary.quad");
    stackvm::Program program;
    CHECK(stackvm::assemble(quads, program));
    CHECK(program.requiredFeatures == 0);
    CHECK(stackvm::Machine().run(program).ok);
    auto reflection = stackvm::parseQuads("# chali-features: reflection\n(write, 42, _, _)\n(halt, _, _, _)\n", "reflection.quad");
    CHECK(reflection.ok() && reflection.requiredFeatures == stackvm::kFeatureReflection);
    const auto optimization = stackvm::optimize(reflection);
    (void)optimization;
    CHECK(reflection.requiredFeatures == stackvm::kFeatureReflection);
    stackvm::Program reflected;
    CHECK(stackvm::assemble(reflection, reflected) == enabled);
    auto unknown = stackvm::parseQuads("# chali-features: unknown\n(halt, _, _, _)\n", "unknown.quad");
    CHECK(!unknown.ok());
    stackvm::Program rejected;
    CHECK(!stackvm::assemble(unknown, rejected));
    program.requiredFeatures = stackvm::kFeatureReflection;
    const auto execution = stackvm::Machine().run(program);
    CHECK(execution.ok == enabled);
    if (!enabled) CHECK(execution.steps == 0 && execution.error.find("STACKVM_WITH_REFLECTION") != Memory::String::npos);
    program.requiredFeatures = 0x80000000u;
    CHECK(!stackvm::Machine().run(program).ok);
    const auto protectedPath = directory + "/protected.svmb";
    writeBytes(protectedPath, { 'k', 'e', 'e', 'p' });
    CHECK(!stackvm::saveProgramBinary(program, protectedPath));
    CHECK(bytes(protectedPath) == std::vector<unsigned char>({ 'k', 'e', 'e', 'p' }));
    for (int format = 0; format < 2; ++format)
    {
        const bool compact = format == 1;
        const auto plainPath = directory + "/plain-" + std::to_string(format) + ".svmb";
        const auto reflectedPath = directory + "/reflection-" + std::to_string(format) + ".svmb";
        program.requiredFeatures = 0;
        CHECK(stackvm::saveProgramBinary(program, plainPath, nullptr, compact));
        const auto ordinary = bytes(plainPath);
        const std::size_t flagOffset = compact ? 12 : 10;
        CHECK(ordinary[flagOffset] == 0);
        stackvm::Program loaded;
        CHECK(stackvm::loadProgramBinary(plainPath, loaded));
        CHECK(loaded.requiredFeatures == 0 && stackvm::Machine().run(loaded).ok);
        auto bad = ordinary;
        bad[flagOffset] |= 8;
        const auto badPath = directory + "/unknown-" + std::to_string(format) + ".svmb";
        writeBytes(badPath, bad);
        loaded.name = "unchanged";
        loaded.requiredFeatures = 0x40000000u;
        CHECK(!stackvm::loadProgramBinary(badPath, loaded));
        CHECK(loaded.name == "unchanged" && loaded.requiredFeatures == 0x40000000u);
        if (!compact)
        {
            // 历史载荷加密位已删除，不能再接受或执行这类产物。
            auto legacyPayload = ordinary;
            legacyPayload[10] |= 1;
            writeBytes(badPath, legacyPayload);
            Memory::Vector<Memory::String> errors;
            CHECK(!stackvm::loadProgramBinary(badPath, loaded, &errors));
            CHECK(!errors.empty() && errors.front().find("版本或标志") != Memory::String::npos);
            CHECK(loaded.name == "unchanged" && loaded.requiredFeatures == 0x40000000u);
            for (unsigned char version : { 1, 2, 3 })
            {
                auto legacy = ordinary;
                legacy[8] = version;
                legacy[9] = 0;
                legacy[10] |= 2;
                writeBytes(badPath, legacy);
                Memory::Vector<Memory::String> diagnostics;
                CHECK(!stackvm::loadProgramBinary(badPath, loaded, &diagnostics));
                CHECK(!diagnostics.empty() && diagnostics.front().find("版本或标志") != Memory::String::npos);
                CHECK(loaded.name == "unchanged" && loaded.requiredFeatures == 0x40000000u);
            }
        }
        program.requiredFeatures = stackvm::kFeatureReflection;
        if (enabled)
        {
            CHECK(stackvm::saveProgramBinary(program, reflectedPath, nullptr, compact));
            const auto encoded = bytes(reflectedPath);
            CHECK(encoded[flagOffset] == 2);
        }
        else
        {
            CHECK(!stackvm::saveProgramBinary(program, plainPath, nullptr, compact));
            CHECK(bytes(plainPath) == ordinary);
        }
        if (std::filesystem::exists(reflectedPath))
        {
            Memory::Vector<Memory::String> diagnostics;
            CHECK(stackvm::loadProgramBinary(reflectedPath, loaded, &diagnostics) == enabled);
            if (enabled)
            {
                CHECK(loaded.requiredFeatures == stackvm::kFeatureReflection);
                CHECK(stackvm::Machine().run(loaded).ok);
            }
            else
            {
                CHECK(!diagnostics.empty() && diagnostics.front().find("STACKVM_WITH_REFLECTION") != Memory::String::npos);
                CHECK(loaded.name == "unchanged" && loaded.requiredFeatures == 0x40000000u);
            }
        }
    }
    std::cout << "feature contract passed: reflection=" << enabled << '\n';
}
