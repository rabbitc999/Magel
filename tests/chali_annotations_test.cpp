/**
 * @file chali_annotations_test.cpp
 * @brief 链接真实编译器，验证 AST 挂载、参数归一与编译隔离。
 */
#include "chali_parser.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

/**
 * @brief 检查断言，失败立即终止测试。
 * @param value 条件。
 * @param message 说明。
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
 * @brief 使用生成解析器构造真实语法树。
 * @param source 源码。
 * @return 顶层列表下标。
 */
int parse(const char *source)
{
    chali::Engine parser;
    int root = -1;
    require(parser.runText(source, &root) == chali::Status::Ok, "解析失败");
    return root;
}
/**
 * @brief 本模块单元测试入口。
 * @return 全部通过返回零。
 */
int main()
{
    require(M_SetReflectionEnabled(true), "测试必须链接已开启能力的编译器与 VM");
    M_SetCheckOnly(true);
    int root = parse(R"CHALI(
@annotation http::Route(path:String, level:int32=60, enabled:bool=true)
@annotation Flag
@http::Route(enabled:false, path:"/service")
struct Service {
    @Flag int64 value;
    @Flag static int64 shared;
    @Flag Service() { self.value=0; }
    @Flag int64 read() { return self.value; }
    @Flag static int64 answer() { return 42; }
};
@Flag enum State { Ready; @Flag int64 code() { return 7; } };
@Flag interface Value { @Flag int64 read(); };
@http::Route("/main")
void main(){}
@Flag module tail;
)CHALI");
    require(M_Compile("unit", "unused", 0, 0, "") == 0, M_LastError());
    const auto tops = M_ParsedNode(root).kids;
    require(tops.size() == 7, "顶层注解声明没有保留");
    const auto &definition = M_ParsedNode(tops[0]);
    require(definition.kind == MNode::AnnotationDecl && definition.s == "http::Route", "限定声明丢失");
    const auto params = M_ParsedNode(definition.kids[0]).kids;
    require(params.size() == 3, "参数表错误");
    require(M_ParsedNode(params[1]).annotationValues[0].integer == 60, "默认值未归一");
    const auto &service = M_ParsedNode(tops[2]);
    require(service.annotations.size() == 1, "类型注解丢失");
    const auto &use = M_ParsedNode(service.annotations[0]);
    require(use.type2 == tops[0], "使用未绑定到声明身份");
    require(use.annotationValues.size() == 3 && use.annotationValues[0].text == "/service" && use.annotationValues[1].integer == 60 && use.annotationValues[2].integer == 0, "命名参数没有按声明顺序补默认值");
    require(use.annotationValues[1].type == M_ParsedNode(params[1]).type, "常量丢失精确类型身份");
    require(use.sourceLine == 4 && use.sourceColumn == 1, "起始位置错误");
    const auto members = M_ParsedNode(service.kids[0]).kids;
    require(members.size() == 5, "结构体成员数错误");
    for (int member : members) require(M_ParsedNode(member).annotations.size() == 1, "字段或方法注解丢失");
    require(M_ParsedNode(members[1]).type2 == 1 && M_ParsedNode(members[2]).type2 == 2 && M_ParsedNode(members[4]).type2 == 1, "注解破坏静态成员或构造函数标记");
    const auto &function = M_ParsedNode(tops[5]);
    const auto &functionUse = M_ParsedNode(function.annotations[0]);
    require(functionUse.annotationValues[0].text == "/main" && functionUse.annotationValues[1].integer == 60 && functionUse.annotationValues[2].integer == 1, "位置参数与默认值错误");
    require(M_ParsedNode(tops[3]).annotations.size() == 1 && M_ParsedNode(tops[4]).annotations.size() == 1 && M_ParsedNode(tops[6]).annotations.size() == 1, "枚举、接口或模块注解丢失");
    require(M_LastErrorColumn() == 0, "成功常量解析污染错误列号");
    require(M_SetReflectionEnabled(false), "关闭能力失败");
    parse("module __root; void main(){}");
    require(M_Compile("unit", "unused", 0, 0, "") == 0, "历史注解污染关闭开关的新编译");
    require(M_SetReflectionEnabled(true), "重新开启失败");
    parse("module __root; @annotation Flag @Flag void main(){}");
    require(M_Compile("unit", "unused", 0, 0, "") == 0, "历史符号污染新编译");
    bool threw = false;
    try
    {
        M_ParsedNode(-1);
    } catch (const std::out_of_range &) { threw = true; }
    require(threw, "非法 AST 下标没有拒绝");
    std::puts("注解 AST 单元测试通过");
}
