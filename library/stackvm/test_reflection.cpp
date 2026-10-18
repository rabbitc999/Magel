/**
 * @file test_reflection.cpp
 * @brief 元数据完整性、查询、容器往返与关闭配置拒绝回归。
 */
#include "stackvm.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>
#define CHECK(expr)                                   \
    do                                                \
    {                                                 \
        ++checks;                                     \
        if (!(expr))                                  \
        {                                             \
            std::cerr << __LINE__ << ": " #expr "\n"; \
            return 1;                                 \
        }                                             \
    } while (0)
namespace r = stackvm::reflection;
/**
 * @brief 生成包含所有记录种类的编译器模拟夹具。
 * @return 类型化元数据。
 */
static r::Table fixture()
{
    r::Table table;
    auto id = [&](const char *key) { auto value = r::stableId(key); table.identities.push_back({value, key}); return value; };
    const auto i32 = id("type:int32"), str = id("type:string"), boolean = id("type:bool"), voidId = id("type:void");
    const auto service = id("struct:app::Service"), state = id("enum:app::State"), iface = id("interface:app::Read");
    const auto route = id("annotation:http::Route");
    table.annotations.push_back({ "http::Route", route, { { "path", str, r::Constant{ str, 0, "/default" } }, { "level", i32, r::Constant{ i32, 60, {} } } } });
    r::Annotation annotation{ route, { { str, 0, Memory::String("/a\0b", 4) }, { i32, 2, {} } } };
    auto function = [&](const char *name, const char *key) { return r::FuncInfo{ name, id(key), { { "value", i32 } }, i32, { annotation } }; };
    r::StructInfo info{ "app::Service", service };
    info.annotations = { annotation };
    info.fields = { { "value", 16, i32, false, { annotation } }, { "name", 24, str, true, {} } };
    info.staticMembers = { { "enabled", boolean, false, { annotation } } };
    info.implementedInterfaces = { iface };
    info.methods.push_back({ function("read", "method:app::Service::read(int32)"), false, false, true, false });
    info.methods.push_back({ function("reset", "method:app::Service::reset(int32)"), true, false, false, false });
    auto ctor = function("new", "method:app::Service::new(int32)");
    ctor.returnTypeId = service;
    info.methods.push_back({ ctor, false, true, false, false });
    table.structs.push_back(info);
    r::EnumInfo enumeration{ "app::State", state };
    enumeration.variants = { { "Idle", 0, {} }, { "Pair", 3, { { "first", i32 }, { "second", str } } } };
    enumeration.methods = { { function("read", "method:app::State::read(int32)"), false, false, true, false },
                            { function("reset", "method:app::State::reset(int32)"), true, false, false, false } };
    enumeration.implementedInterfaces = { iface };
    table.enums.push_back(enumeration);
    r::InterfaceInfo interface{ "app::Read", iface };
    interface.implementedBy = { service, state };
    auto method = function("read", "method:app::Read::read(int32)");
    method.returnTypeId = voidId;
    interface.methods.push_back({ method, false, false, true, true });
    table.interfaces.push_back(interface);
    table.functions.push_back(function("app::increment", "function:app::increment(int32)"));
    table.moduleAnnotations.push_back(annotation);
    return table;
}
/**
 * @brief 读测试二进制。
 * @param path 路径。
 * @return 原始内容。
 */
static Memory::Vector<std::uint8_t> bytes(const Memory::String &path)
{
    std::ifstream input(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}
/**
 * @brief 写测试二进制。
 * @param path 路径。
 * @param data 内容。
 */
static void write(const Memory::String &path, const Memory::Vector<std::uint8_t> &data)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
}
/**
 * @brief 执行有意义的安全边界测试并留下导出夹具。
 * @param argc 参数数量。
 * @param argv 首参数是夹具目录。
 * @return 成功为零。
 */
int main(int argc, char **argv)
{
    int checks = 0;
    const Memory::String directory = argc > 1 ? argv[1] : "reflection-fixtures";
    std::filesystem::create_directories(std::filesystem::path(directory.c_str()));
    auto table = fixture();
    CHECK(r::stableId("type:int32") == 0x8c2d4d7646cafecfull);  // 跨进程、跨平台固定向量。
    CHECK(r::validate(table));
    const auto serviceId = r::stableId("struct:app::Service");
    const auto record = r::getStructInfo(table, serviceId);
    CHECK(record && record->get().fields[0].offset == 16 && record->get().fields[1].isOwned);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(record->get())>>);
    CHECK(!r::getStructInfo(table, 0));
    CHECK(r::getEnumInfo(table, r::stableId("enum:app::State"))->get().variants[1].payloads.size() == 2);
    CHECK(r::getInterfaceInfo(table, r::stableId("interface:app::Read"))->get().methods[0].hasDefaultBody);
    CHECK(r::getFuncInfo(table, "app::increment"));
    CHECK(!r::getFuncInfo(table, "increment"));
    CHECK(r::getAnnotationInfo(table, "http::Route")->get().params[1].defaultValue->bits == 60);
    CHECK(record->get().annotations[0].args[0].text.size() == 4);
    CHECK(record->get().methods[0].isConst && record->get().methods[1].isStatic && record->get().methods[2].isConstructor);
    auto overloaded = table;
    auto second = overloaded.functions[0];
    second.typeId = r::stableId("function:app::increment(string)");
    second.params[0].typeId = r::stableId("type:string");
    overloaded.identities.push_back({ second.typeId, "function:app::increment(string)" });
    overloaded.functions.push_back(second);
    CHECK(r::validate(overloaded) && !r::getFuncInfo(overloaded, "app::increment"));
    CHECK(r::findById(overloaded.functions, second.typeId));
    Memory::Vector<std::uint8_t> encoded, reencoded;
    CHECK(r::encode(table, encoded));
    r::Table decoded;
    CHECK(r::decode(encoded.data(), encoded.size(), decoded));
    CHECK(r::encode(decoded, reencoded) && reencoded == encoded);
    auto staticConstructor = table;
    staticConstructor.structs[0].methods[2].isStatic = true;
    CHECK(r::encode(staticConstructor, reencoded));
    CHECK(r::decode(reencoded.data(), reencoded.size(), decoded));
    CHECK(decoded.structs[0].methods[2].isStatic && decoded.structs[0].methods[2].isConstructor);
    staticConstructor.structs[0].methods[2].isConst = true;
    CHECK(!r::validate(staticConstructor));
    CHECK(r::encode(table, reencoded) && reencoded == encoded);
    for (std::size_t size = 0; size < encoded.size(); ++size)
    {
        decoded = table;
        CHECK(!r::decode(encoded.data(), size, decoded));
        CHECK(decoded.structs[0].name == "app::Service");
    }
    auto invalid = encoded;
    invalid[4] = 2;
    CHECK(!r::decode(invalid.data(), invalid.size(), decoded));
    r::detail::Writer signatureBytes;
    CHECK(signatureBytes(table.structs[0].methods[0].function));
    invalid = encoded;
    const auto signatureAt = std::search(invalid.begin(), invalid.end(), signatureBytes.bytes.begin(), signatureBytes.bytes.end());
    CHECK(signatureAt != invalid.end());
    *(signatureAt + signatureBytes.bytes.size()) = 2;  // isStatic 只接受规范布尔值。
    CHECK(!r::decode(invalid.data(), invalid.size(), decoded));
    invalid = encoded;
    invalid.push_back(0);
    CHECK(!r::decode(invalid.data(), invalid.size(), decoded));
    invalid = encoded;
    invalid[8] = invalid[9] = invalid[10] = invalid[11] = 255;
    CHECK(!r::decode(invalid.data(), invalid.size(), decoded));
    CHECK(!r::decode(nullptr, r::kMaximumBytes + 1, decoded));
    auto bad = table;
    bad.identities.push_back(bad.identities[0]);
    CHECK(!r::validate(bad));
    bad = table;
    bad.identities[0].key += "changed";
    CHECK(!r::validate(bad));
    bad = table;
    bad.structs[0].fields[0].typeId = 42;
    CHECK(!r::validate(bad));
    bad = table;
    bad.structs[0].fields.push_back(bad.structs[0].fields[0]);
    CHECK(!r::validate(bad));
    bad = table;
    bad.enums[0].variants[1].tag = 0;
    CHECK(!r::validate(bad));
    bad = table;
    bad.annotations[0].params[0].defaultValue->typeId = r::stableId("type:int32");
    CHECK(!r::validate(bad));
    bad = table;
    bad.moduleAnnotations[0].args.pop_back();
    CHECK(!r::validate(bad));
    bad = table;
    bad.moduleAnnotations[0].annotationId = 42;
    CHECK(!r::validate(bad));
    bad = table;
    bad.moduleAnnotations[0].args[0].typeId = r::stableId("type:int32");
    CHECK(!r::validate(bad));
    bad = table;
    bad.structs[0].methods[0].isStatic = true;
    CHECK(!r::validate(bad));
    reencoded = { 42 };
    CHECK(!r::encode(bad, reencoded) && reencoded == Memory::Vector<std::uint8_t>{ 42 });
    const auto hex = r::toHex(table);
    CHECK(hex && r::fromHex(*hex, decoded));
    CHECK(!r::fromHex("x0", decoded) && !r::fromHex("f", decoded));
    auto quads = stackvm::parseQuads("# magel-features: reflection\n# magel-reflection: " + *hex + "\n(write, 42, _, _)\n(halt, _, _, _)\n", "metadata.quad");
    CHECK(quads.ok() && !quads.reflection.empty());
    const auto optimization = stackvm::optimize(quads);
    (void)optimization;
    CHECK(r::toHex(quads.reflection) == hex);
    CHECK(!stackvm::parseQuads("# magel-reflection: " + *hex, "missing-flag").ok());
    CHECK(!stackvm::parseQuads("# magel-features: reflection\n# magel-reflection: " + *hex + "\n# magel-reflection: " + *hex, "duplicate").ok());
    stackvm::Program program;
    CHECK(stackvm::assemble(quads, program) == bool(STACKVM_WITH_REFLECTION));
    if (!STACKVM_WITH_REFLECTION)
    {
        for (int format = 0; format < 2; ++format)
        {
            const auto path = directory + "/metadata-" + Memory::toString(format) + ".svmb";
            if (std::filesystem::exists(path.c_str()))
            {
                program.name = "unchanged";
                CHECK(!stackvm::loadProgramBinary(path, program) && program.name == "unchanged");
            }
        }
    }
    else
    {
        CHECK(stackvm::Machine().run(program).ok);
        auto unflagged = program;
        unflagged.requiredFeatures = 0;
        CHECK(!stackvm::Machine().run(unflagged).ok);
        CHECK(!stackvm::saveProgramBinary(unflagged, directory + "/unflagged.svmb"));
        bad = table;
        bad.structs[0].fields[0].typeId = 42;
        auto broken = program;
        broken.reflection = bad;
        CHECK(!stackvm::Machine().run(broken).ok);
        const auto protectedPath = directory + "/protected.svmb";
        write(protectedPath, { 1, 2, 3 });
        CHECK(!stackvm::saveProgramBinary(broken, protectedPath) && bytes(protectedPath) == Memory::Vector<std::uint8_t>({ 1, 2, 3 }));
        for (int format = 0; format < 2; ++format)
        {
            const auto path = directory + "/metadata-" + Memory::toString(format) + ".svmb";
            CHECK(stackvm::saveProgramBinary(program, path, nullptr, format == 1));
            stackvm::Program loaded;
            CHECK(stackvm::loadProgramBinary(path, loaded));
            CHECK(r::encode(loaded.reflection, reencoded) && reencoded == encoded);
            CHECK(stackvm::Machine().run(loaded).ok);
        }
        // 修复外层校验和后，仍须由元数据校验拒绝未知 schema。
        auto file = bytes(directory + "/metadata-0.svmb");
        const auto metadataStart = file.size() - encoded.size();
        file[metadataStart + 4] = 2;
        std::uint32_t crc = 2166136261u;
        for (std::size_t i = 24; i < file.size(); ++i)
        {
            crc ^= file[i];
            crc *= 16777619u;
        }
        for (unsigned i = 0; i < 4; ++i) file[20 + i] = std::uint8_t(crc >> (8 * i));
        write(directory + "/bad-schema.svmb", file);
        stackvm::Program loaded;
        loaded.name = "unchanged";
        CHECK(!stackvm::loadProgramBinary(directory + "/bad-schema.svmb", loaded) && loaded.name == "unchanged");
        // 1.5/1.6 的反射能力契约产物没有段，仍按空表兼容加载。
        file = bytes(directory + "/metadata-0.svmb");
        file.resize(file.size() - encoded.size());
        const std::uint64_t oldPayloadLength = file.size() - 24;
        for (unsigned i = 0; i < 8; ++i) file[12 + i] = std::uint8_t(oldPayloadLength >> (8 * i));
        crc = 2166136261u;
        for (std::size_t i = 24; i < file.size(); ++i)
        {
            crc ^= file[i];
            crc *= 16777619u;
        }
        for (unsigned i = 0; i < 4; ++i) file[20 + i] = std::uint8_t(crc >> (8 * i));
        write(directory + "/legacy-capability.svmb", file);
        CHECK(stackvm::loadProgramBinary(directory + "/legacy-capability.svmb", loaded) && loaded.reflection.empty());
        // 为真实 magel-emit VM/AOT 查询测试保留输入和宿主断言。
        std::ofstream quad(directory + "/metadata.quad", std::ios::binary);
        quad << "# magel-features: reflection\n# magel-reflection: " << *hex << "\n(write, 42, _, _)\n(halt, _, _, _)\n";
        std::ofstream header(directory + "/query.inc", std::ios::binary);
        header << "int main() { const auto &table = magel_reflection_metadata(); auto s = stackvm::reflection::getStructInfo(table, " << serviceId << "ull);\n"
                                                                                                                                                      "if (!s || s->get().methods.size() != 3 || s->get().fields[1].isOwned != true) return 1;\n"
                                                                                                                                                      "auto a = stackvm::reflection::getAnnotationInfo(table, \"http::Route\");\n"
                                                                                                                                                      "if (!a || a->get().params[1].defaultValue->bits != 60 || table.enums[0].variants[1].payloads.size() != 2) return 2; return 0; }\n";
    }
    std::cout << "reflection metadata checks=" << checks << " runtime=" << STACKVM_WITH_REFLECTION << '\n';
    return 0;
}
