/**
 * @file chali_generic_monomorphization_test.cpp
 * @brief 泛型文本替换、类型身份和递归展开的回归。
 */
#include "chali_generic.hpp"
#include "chali_glue.hpp"
#include <cstdio>
#include <cstdlib>

/**
 * @brief 检查回归条件。
 * @param value 条件。
 * @param message 失败原因。
 * @return 无。
 */
void require(bool value, const char *message)
{
    if (!value)
    {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

/**
 * @brief 展开独立测试源码。
 * @param text 源码。
 * @param result 结果。
 * @param error 诊断。
 * @return 展开成功返回 true。
 */
bool expand(const Memory::String &text, preprocess::Result &result, Memory::String &error)
{
    result = preprocess::Result{};
    result.text = text;
    error.clear();
    return chali_generic::run(result, error);
}

/**
 * @brief 验证不能由运行结果直接观察的实例复用和文本边界。
 * @return 成功返回零。
 */
int main()
{
    using namespace chali_generic;
    const Memory::String literal = R"(T "T \\" /* T */ // T
'T' TT T)";
    const auto replaced = substituteIdentifiers(literal, { { "T", "int64" } });
    require(replaced == R"(int64 "T \\" /* T */ // T
'T' TT int64)",
            "字符串、字符、注释或标识符边界发生误替换");
    require(mangle("G", { "(int64,std::lang::String)" }) != mangle("G", { "_int64_lang__String_" }), "标点不同的类型发生实例名称碰撞");
    require(mangle("G", { Memory::String(4000u, 'a') }).size() < 100u, "实例名称随嵌套类型无限增长");
    preprocess::Result result;
    Memory::String error;
    require(expand("struct AuditUnit<T,V=T,W=AuditUnitInner<T,V>>{W value;};"
                   "struct AuditUnitInner<T,V>{T a;V b;};AuditUnit<String> a;AuditUnit<std::lang::String> b;",
                   result, error),
            error.c_str());
    std::size_t outer = 0, inner = 0;
    for (const auto &entry : session.made)
    {
        if (entry.second.generic->name == "AuditUnit")
        {
            ++outer;
            require(entry.second.args[0] == "std::lang::String" && entry.second.args[1] == "std::lang::String", "依赖默认类型未绑定或归一");
        }
        if (entry.second.generic->name == "AuditUnitInner") ++inner;
    }
    require(outer == 1u && inner == 1u, "重复使用或类型别名生成了重复实例");
    require(!expand("struct AuditForward<T=V,V=int64>{T value;};", result, error) && error.find("只能引用前面的类型参数") != Memory::String::npos, "前向默认类型缺少明确诊断");
    require(!expand("struct AuditCycle<T=AuditCycle<>>{T value;};AuditCycle<> a;", result, error) && error.find("128") != Memory::String::npos, "递归默认类型没有及时终止");
    require(!expand("struct AuditGrow<T>{AuditGrow<List<T>> mut& next;};AuditGrow<int64> a;", result, error) && (error.find("1024") != Memory::String::npos || error.find("128") != Memory::String::npos), "不断扩大的递归类型没有及时终止");
    Memory::String deepDefault = "int64";
    for (std::size_t i = 0; i < 128u; ++i) deepDefault = "List<" + deepDefault + ">";
    require(!expand("struct AuditDeepDefault<T=" + deepDefault + ">{T value;};AuditDeepDefault<> a;", result, error) && error.find("128") != Memory::String::npos, "深层容器默认类型没有及时终止");
    Memory::String many = "struct AuditMany<T>{T value;};";
    for (std::size_t i = 0; i < 1025u; ++i)
        many += "AuditMany<N" + Memory::toString(i) + "> a" + Memory::toString(i) + ";";
    require(!expand(many, result, error) && error.find("1024") != Memory::String::npos, "实例总数上限没有及时终止");
    collectCoreAliasShadows("module demo;struct Concrete{};struct Generic<T>{T value;};");
    require(canonicalType("Concrete") == canonicalType("demo::Concrete"), "导入裸名称与限定名称的类型身份不一致");
    require(canonicalType("demo::Generic__int64") == "Generic__int64", "泛型实例限定名称未归一");
    require(chali_glue::specialize("native", { "int64 fn(int64)", "(int64,std::lang::String)" }) == mangle("native", { "int64 fn(int64)", "(int64,std::lang::String)" }), "宿主胶水与单态化器的复杂类型符号不一致");
    require(mangle("native", { "int32" }) == "native__int32", "简单类型的既有宿主符号发生变化");
    Memory::String marked = "List<List<int64>> xs; List<int64 fn(int64)> fs; List</* < > */int64> cs; (a<b); a<b; a>>1;";
    markContainerTypeArguments(marked);
    require(marked == "List@<List@<int64@>@> xs; List@<int64 fn(int64)@> fs; List@</* < > */int64@> cs; (a<b); a<b; a>>1;", "容器标点破坏了比较、右移、注释或函数类型");
    auto once = marked;
    markContainerTypeArguments(marked);
    require(marked == once, "容器标点归一化不幂等");
    require(expand("#derive(Equal,Hash,Clone)\nstruct DerivedBox<T>{T value;};DerivedBox<int64> value;", result, error), error.c_str());
    require(result.text.find("#derive(Equal,Hash,Clone)") != Memory::String::npos, "泛型实例丢失字段派生指令");
    require(!expand("#derive(Equal,Equal) struct Bad<T>{T value;};", result, error) && error.find("重复请求") != Memory::String::npos, "未实例化泛型的重复派生没有报错");
    std::puts("generic monomorphization unit tests OK");
}
