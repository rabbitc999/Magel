#include "../source/magel_borrow_checker.hpp"
#include <cassert>

/**
 * @brief 验证借用检查器可独立链接并在一个批次中返回多个源码错误。
 * @return 成功返回零。
 */
int main()
{
    using namespace magel_borrow_checker;
    const Memory::Vector<Fact> facts = {
        { Fact::Store, 2, 3, 1, {} },
        { Fact::Return, 0, 0, 2, {} },
        { Fact::Return, 1, 0, 3, {} },
        { Fact::ObjectArgument, 0, 0, 4, {} },
        { Fact::Store, 3, 2, 5, {} },
        { Fact::Return, 2, 0, 6, "：局部变量" },
        { Fact::ObjectArgument, 1, 0, 7, "：参数" },
    };
    const auto diagnostics = check(facts);
    assert(diagnostics.size() == 3);
    for (std::size_t i = 0; i < diagnostics.size(); ++i)
    {
        assert(diagnostics[i].line == i + 5);
        assert(diagnostics[i].message.find("MGL1204") != Memory::String::npos);
    }
    assert(check({}).empty());
    assert(facts.size() == 7);
    const Place object{ "object", {} }, alias{ "alias", {} }, dereference{ "alias", { "*" } };
    // 两条分支中仅一条释放，合流后的别名访问仍必须拒绝。
    FlowGraph branch{ {
        { FlowNode::Bind, alias, object, { 1 } },
        { FlowNode::Nop, {}, {}, { 2, 3 } },
        { FlowNode::Destroy, object, {}, { 3 } },
        { FlowNode::Read, dereference, {}, {}, 9, "alias" },
    } };
    assert(checkFlow(branch).size() == 1);
    assert(checkFlow(branch)[0].line == 9);
    // 回边让下一次迭代看到释放；检查不能只按源码顺序扫描一次。
    branch.nodes[3].next = { 1 };
    assert(checkFlow(branch).size() == 1);
    FlowGraph restore{ {
        { FlowNode::Move, object, {}, { 1 } },
        { FlowNode::Write, object, {}, { 2 } },
        { FlowNode::Read, object, {}, {} },
    } };
    assert(checkFlow(restore).size() == 2);
    restore.nodes[1].kind = FlowNode::Initialize;
    assert(checkFlow(restore).empty());
    restore.nodes[0].kind = FlowNode::Drop;
    restore.nodes[1].kind = FlowNode::Write;
    assert(checkFlow(restore).empty());
    restore.nodes[0].kind = FlowNode::Destroy;
    assert(checkFlow(restore).size() == 2);
    // 同一分配点反复分配时，新实例有效，旧实例上的别名保持失效。
    FlowGraph allocation{ {
        { FlowNode::Allocate, alias, object, { 1 } },
        { FlowNode::Bind, { "saved", {} }, dereference, { 2 } },
        { FlowNode::Destroy, dereference, {}, { 3 } },
        { FlowNode::Allocate, alias, object, { 4 } },
        { FlowNode::Read, dereference, {}, { 5 } },
        { FlowNode::Read, { "saved", { "*" } }, {}, {}, 17, "saved" },
    } };
    assert(checkFlow(allocation).size() == 1);
    assert(checkFlow(allocation)[0].line == 17);
    assert(!overlaps({ "x", { ".left" } }, { "x", { ".right" } }));
    assert(overlaps({ "x", { "[*]" } }, { "x", { "[3]" } }));
    // 重新定义要求所有到达路径都完整移动；字段移动和重新写入不满足条件。
    FlowGraph redeclare{ {
        { FlowNode::Write, object, {}, { 1 } },
        { FlowNode::Nop, {}, {}, { 2, 3 } },
        { FlowNode::Move, object, {}, { 4 } },
        { FlowNode::Nop, {}, {}, { 4 } },
        { FlowNode::Redeclare, object, {}, {}, 23, "object" },
    } };
    assert(checkFlow(redeclare).size() == 1);
    assert(checkFlow(redeclare)[0].line == 23);
    redeclare.nodes[3].kind = FlowNode::Move;
    redeclare.nodes[3].target = object;
    assert(checkFlow(redeclare).empty());
    redeclare.nodes[2].target.path = { ".field" };
    assert(checkFlow(redeclare).size() == 1);
    redeclare.nodes[2].target = object;
    redeclare.nodes[3].kind = FlowNode::Write;
    assert(checkFlow(redeclare).size() == 1);
    redeclare.nodes[1].next = { 2 };
    redeclare.nodes[2].kind = FlowNode::MoveScalar;
    assert(checkFlow(redeclare).empty());
    // 零次循环路径不能继承循环体里的移动事实。
    redeclare.nodes[1].next = { 2, 4 };
    redeclare.nodes[2].next = { 1 };
    const auto loopDiagnostics = checkFlow(redeclare);
    assert(loopDiagnostics.size() == 2);  // 重复移动与零次循环后的非法重新定义。
    assert(std::any_of(loopDiagnostics.begin(), loopDiagnostics.end(), [](const FlowDiagnostic &item) { return item.line == 23; }));
    // 引用槽被移走后不可读取或重绑，只有新声明建立新的可用绑定。
    FlowGraph movedReference{ {
        { FlowNode::Bind, alias, object, { 1 } },
        { FlowNode::Move, alias, {}, { 2 } },
        { FlowNode::Read, alias, {}, {}, 31, "alias" },
    } };
    assert(checkFlow(movedReference).size() == 1);
    movedReference.nodes[2] = { FlowNode::Bind, alias, object, { 3 } };
    movedReference.nodes.push_back({ FlowNode::Read, alias, {}, { 4 } });
    movedReference.nodes.push_back({ FlowNode::Read, dereference, {}, {} });
    assert(checkFlow(movedReference).size() == 2);
    movedReference.nodes.insert(movedReference.nodes.begin(), { FlowNode::Destroy, object, {}, { 1 } });
    for (std::size_t i = 1; i < movedReference.nodes.size(); ++i)
        for (auto &next : movedReference.nodes[i].next) ++next;
    assert(checkFlow(movedReference).size() == 3);
    assert(checkFlow({}).empty());
    return 0;
}
