/**
 * @file magel_reflection_declarations_test.cpp
 * @brief 真实 MAGEL 声明生成表的结构和往返单元测试。
 */
#include "stackvm/stackvm.hpp"
#include <cassert>
namespace r = stackvm::reflection;
/**
 * @brief 验证真实布局、签名、注解与显式暴露边界。
 * @param table 待验证元数据。
 */
void verifyDeclarations(const r::Table &table)
{
    auto s = r::findByName(table.structs, "Service");
    assert(s);
    const auto &service = s->get();
    assert(service.fields.size() == 2 && service.fields[0].name == "value" && service.fields[0].offset == 0);
    assert(service.fields[1].offset == 8 && !service.fields[0].isOwned && service.fields[1].isOwned);
    assert(service.fields[0].typeId == r::stableId("type:int64"));
    assert(service.staticMembers.size() == 1 && service.staticMembers[0].name == "shared");
    assert(service.implementedInterfaces.size() == 1 && service.implementedInterfaces[0] == r::stableId("interface:Value"));
    auto method = [&](const char *name) -> const r::MethodInfo & { for(const auto &m:service.methods) if(m.function.name==name) return m; std::abort(); };
    assert(service.methods.size() == 5);
    assert(method("new").isConstructor && method("new").function.returnTypeId == service.typeId);
    assert(method("new").function.params.size() == 1 && method("new").function.annotations.size() == 1);
    assert(method("read").isConst && !method("read").isStatic && method("read").function.params.empty());
    assert(!method("update").isConst && method("update").function.params.size() == 1);
    assert(method("answer").isStatic);
    assert(method("fallback").isConst && method("fallback").function.annotations.size() == 1);
    assert(method("read").function.annotations.size() == 1);
    auto e = r::findByName(table.enums, "State");
    assert(e && e->get().variants.size() == 2 && e->get().methods.size() == 2);
    assert(e->get().variants[1].payloads.size() == 1 && e->get().variants[1].payloads[0].name == "value");
    auto i = r::findByName(table.interfaces, "Value");
    assert(i && i->get().methods.size() == 2);
    assert(!i->get().methods[0].hasDefaultBody && i->get().methods[0].function.annotations.size() == 1);
    assert(i->get().implementedBy.size() == 1 && i->get().implementedBy[0] == service.typeId);
    assert(!r::findByName(table.structs, "Hidden"));
    auto f = r::findByName(table.functions, "plus");
    assert(f && f->get().params.size() == 1);
    assert(f->get().annotations.size() == 2 && f->get().annotations[1].args[0].text == "default");
    auto a = r::findByName(table.annotations, "Note");
    assert(a && a->get().params[0].defaultValue->text == "default");
    assert(r::findByName(table.annotations, "std::reflect::Reflect"));
    Memory::Vector<std::uint8_t> bytes;
    assert(r::encode(table, bytes));
    r::Table copy;
    assert(r::decode(bytes.data(), bytes.size(), copy));
    Memory::Vector<std::uint8_t> again;
    assert(r::encode(copy, again) && bytes == again);
}
#ifndef MAGEL_REFLECTION_QUERY_ONLY
/**
 * @brief 加载真实编译产物并验证，错误时输出诊断。
 * @param argc 参数数。
 * @param argv 路径。
 * @return 状态码。
 */
int main(int argc, char **argv)
{
    assert(argc == 2);
    stackvm::Program program;
    Memory::Vector<Memory::String> diagnostics;
    if (!stackvm::loadProgramBinary(argv[1], program, &diagnostics))
    {
        for (const auto &d : diagnostics) std::fprintf(stderr, "%s\n", d.c_str());
        return 1;
    }
    verifyDeclarations(program.reflection);
    std::puts("MAGEL 声明元数据单元测试通过");
}
#endif
