/**
 * @file ast_node.cpp
 * @brief AST 节点构造与参数装配。
 */

#include "simple_ast.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace simple_ast
{
// ---- 节点 -----------------------------------------------------------------

/**
 * @brief 构造整数常量对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::i(std::int64_t value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::IntLit;
    node->intValue = value;
    return node;
}

/**
 * @brief 构造浮点常量对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::f(double value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::FloatLit;
    node->floatValue = value;
    return node;
}

/**
 * @brief 构造字符串常量对应的低层语法节点。
 * @param text 待处理的文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::str(Memory::String text)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::StringLit;
    node->text = std::move(text);
    return node;
}

/**
 * @brief 构造保存原始 UTF-8 字节及精确长度的数组常量。
 * @param bytes 原始字节序列，零字节也计入长度。
 * @return 使用 u8 数组布局的低层节点。
 */
NodePtr Node::byteArray(Memory::String bytes)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::ByteArrayLit;
    node->declared = Type::array(Type::u8(), "count=" + Memory::toString(bytes.size()));
    node->text = std::move(bytes);
    return node;
}

/**
 * @brief 构造布尔常量对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::boolean(bool value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::BoolLit;
    node->intValue = value ? 1 : 0;
    return node;
}

/**
 * @brief 构造空值对应的低层语法节点。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::null()
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::NullLit;
    return node;
}

/**
 * @brief 构造变量引用对应的低层语法节点。
 * @param name 名称文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::var(Memory::String name)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Var;
    node->text = std::move(name);
    return node;
}

/**
 * @brief 构造二元运算对应的低层语法节点。
 * @param op 运算符文本或类别。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::binary(Memory::String op, NodePtr left, NodePtr right)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Binary;
    node->text = std::move(op);
    node->kids = { std::move(left), std::move(right) };
    return node;
}

/**
 * @brief 构造顺序求值对应的低层语法节点。
 * @param statements 待处理的语句列表。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::sequence(Memory::Vector<NodePtr> statements, NodePtr value)
{
    NodePtr node = Memory::makeShared<Node>();
    node->kind = Kind::Sequence;
    node->kids = std::move(statements);
    node->kids.push_back(std::move(value));
    return node;
}

/**
 * @brief 构造一元运算对应的低层语法节点。
 * @param op 运算符文本或类别。
 * @param operand 一元运算的操作数。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::unary(Memory::String op, NodePtr operand)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Unary;
    node->text = std::move(op);
    node->kids = { std::move(operand) };
    return node;
}

/**
 * @brief 构造索引访问对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @param position 目标位置或源码位置。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::index(NodePtr target, NodePtr position)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Index;
    node->kids = { std::move(target), std::move(position) };
    return node;
}

/**
 * @brief 按**自定义步长**算元素地址（不取值）：`base + position × stride`。
 *
 * `index` 固定按 8 字节槽位取值，适合标量数组；元素是结构体时每个元素占
 * `字段数 × 8` 字节、而表达式要的是**句柄**（地址）而不是取出来的值，
 * 这时用 `offset`（结构体数组 `arr[i].f` 就靠它）。
 * @param base 起始地址、基值或目标表达式。
 * @param position 目标位置或源码位置。
 * @param stride 相邻元素之间的字节步长。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::offset(NodePtr base, NodePtr position, std::int64_t stride)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Offset;
    node->text = Memory::toString(stride);
    node->kids = { std::move(base), std::move(position) };
    return node;
}

/**
 * @brief 构造成员访问对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @param field 待访问的字段名称或描述。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::member(NodePtr target, Memory::String field)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Member;
    node->text = std::move(field);
    node->kids = { std::move(target) };
    return node;
}

/**
 * @brief 构造函数调用对应的低层语法节点。
 * @param callee 被调用函数的名称或表达式。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::call(Memory::String callee, Memory::Vector<NodePtr> args)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Call;
    node->text = std::move(callee);
    node->kids = std::move(args);
    return node;
}

/**
 * @brief 带形参名的调用：第 i 个实参绑定到 `parameterNames[i]`。
 *
 * 四元式里 `(param, 实参, _, 形参名)` 是**按名字**绑定的（见 stackvm 的 `Op::Call`），
 * 所以多参数调用必须把 callee 的形参名逐个写出来；只写 `arg` 会让所有实参挤在同一个槽。
 * 方法调用也可以走这里：形参名写 `self` + 各实参名，callee 写 `Type.方法名`。
 * @param callee 被调用函数的名称或表达式。
 * @param parameterNames 形参名称列表。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::call(Memory::String callee, Memory::Vector<Memory::String> parameterNames,
                   Memory::Vector<NodePtr> args)
{
    NodePtr node = call(std::move(callee), std::move(args));
    node->names = std::move(parameterNames);
    return node;
}

/**
 * @brief 构造方法调用对应的低层语法节点。
 * @param object 待访问的对象。
 * @param method 压缩方法编号或目标方法名称。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::methodCall(NodePtr object, Memory::String method, Memory::Vector<NodePtr> args)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::MethodCall;
    node->text = std::move(method);
    node->kids.push_back(std::move(object));
    for (NodePtr &argument : args) node->kids.push_back(std::move(argument));
    return node;
}

/**
 * @brief 构造闭包函数对应的低层语法节点。
 * @param body 待处理的函数体或循环体。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::lambda(NodePtr body)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Lambda;
    if (body) node->kids = { std::move(body) };
    return node;
}

/**
 * @brief 构造数组构造对应的低层语法节点。
 * @param type 类型描述。
 * @param items 待保存的元素集合。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::arrayNew(TypePtr type, Memory::Vector<NodePtr> items)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::ArrayNew;
    node->text = type ? type->meta : Memory::String();
    node->declared = type;  // 元素类型进 .data 的描述（浮点数组也要写对）
    node->kids = std::move(items);
    return node;
}

/**
 * @brief 构造结构体构造对应的低层语法节点。
 * @param typeName 目标类型的名称。
 * @param fields 结构体字段定义或字段初始化值列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::structNew(Memory::String typeName, Memory::Vector<NodePtr> fields)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::StructNew;
    node->text = std::move(typeName);
    node->kids = std::move(fields);
    return node;
}

/**
 * @brief 构造枚举变体构造对应的低层语法节点。
 * @param typeName 目标类型的名称。
 * @param variant 枚举变体名称或定义。
 * @param payload 枚举载荷或协议消息正文。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::enumNew(Memory::String typeName, Memory::String variant, Memory::Vector<NodePtr> payload)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::EnumNew;
    node->text = std::move(variant);
    node->typeName = std::move(typeName);  // 保留类型名（二进制 tag / 类型推断都要用）
    node->kids = std::move(payload);
    return node;
}

/**
 * @brief 堆分配：`(alloc, 字节数, _, t)`，表达式的值是拿到的地址（`i64`）。
 *
 * 与 `arrayNew` / `structNew` 的区别：那两个把对象放进**数据区**（每个声明点一块、
 * 静态布局），这里要的是**运行期才知道大小**的对象，走虚拟机的堆；什么时候还回去
 * 由程序自己 `freeMemory`（栈机那边是 `(free, 地址, _, _)`）。返回 0 表示没分配到。
 * @param bytes 内存区域或数据的字节数。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::alloc(NodePtr bytes)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Alloc;
    if (bytes) node->kids = { std::move(bytes) };
    return node;
}

/**
 * @brief 按地址读一个**指定类型**的值：`(load:T, 地址, _, t)`。
 *
 * 与 `loadInt` 的区别是"这 8 个字节怎么解释"由 `type` 决定（`f64` 按 IEEE-754
 * 读，`i64` 按整数读）。字段访问用它：字段地址是自己按槽位算出来的，不能走
 * `member()` 那种"按名字在基类型里查下标"的路（基类型是内层结构体时会查错）。
 * @param address 地址（i64 表达式）。
 * @param type 值的类型。
 * @return 表达式节点。
 */
NodePtr Node::loadFrom(NodePtr address, TypePtr type)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::LoadInt;
    node->declared = type ? std::move(type) : Type::i64();
    if (address) node->kids = { std::move(address) };
    return node;
}

/**
 * @brief 往一个**算好的地址**写值：`(store:T, 地址, 值, _)`，当语句用。
 *
 * `assign(左值, 值)` 只认"变量 / 下标 / 成员"三种左值；地址已经自己算好（比如
 * 结构体字段的槽位偏移）时用这一条。
 * @param address 地址。
 * @param value 值。
 * @param type 值的类型（决定浮点按位写还是整数写）。
 * @return 语句节点。
 */
NodePtr Node::storeAt(NodePtr address, NodePtr value, TypePtr type)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::StoreAt;
    node->text = (type ? type : Type::i64())->quadType();  // 写的时候按这个类型写
    if (address) node->kids.push_back(std::move(address));
    if (value) node->kids.push_back(std::move(value));
    return node;
}

/**
 * @brief **软释放**：`(drop, 地址, _, _)`，当语句用。
 *
 * 与 `freeMemory` 的区别是"还不了就当没这回事"：地址是 0（空指针）、指向数据区
 * （静态字面量）、或者那块堆内存**已经还过**了，都不算错。编译器在语句块出口插的
 * 自动释放走这一条（见 chali 的"生命周期"），显式 `delete` 仍用 `freeMemory`。
 * @param pointer 待释放或校验的指针。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::release(NodePtr pointer)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Drop;
    node->kids.push_back(std::move(pointer));
    return node;
}

/**
 * @brief 构造内存释放对应的低层语法节点。
 * @param pointer 待释放或校验的指针。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::freeMemory(NodePtr pointer)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Free;
    if (pointer) node->kids = { std::move(pointer) };
    return node;
}

/**
 * @brief 间接调用（函数指针）：`(param …)` + `(callptr, 函数指针, _, 目标)`。
 *
 * `target` 的值是"数据区里存着函数标签的那串字节"的地址（0 = 空指针）。
 * 栈机侧实参**按位置**绑定：位置表由组装时从"函数标签前面那段 decl"扫出来，
 * 所以这里的 `parameterNames` 只是占位（走 `(param, 值, _, 名字)` 的格式而已）。
 * @param target 待访问或修改的目标。
 * @param parameterNames 形参名称列表。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::callPointer(NodePtr target, Memory::Vector<Memory::String> parameterNames,
                          Memory::Vector<NodePtr> args)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::CallPointer;
    if (target) node->kids.push_back(std::move(target));
    for (NodePtr &argument : args) node->kids.push_back(std::move(argument));
    node->names = std::move(parameterNames);
    return node;
}

/**
 * @brief **具名**字符串数据项：同名只登记一次，表达式的值是它的地址。
 *
 * `str()` 每次发射都新开一个数据项（同一个内容出现两次就两份）；要"同一个东西
 * 地址始终一致"就得用这个（编译器给函数指针发的"函数标签"就是它 —— 于是
 * `p == 函数名` 能按地址比）。
 * @param name 数据项名字（要唯一地对应一份内容）。
 * @param text 内容（不含引号）。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::namedStr(Memory::String name, Memory::String text)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::StringLit;
    node->text = std::move(text);
    node->typeName = std::move(name);  // 数据项名字（普通字符串字面量这里为空）
    return node;
}

/**
 * @brief 按地址读 8 个字节，当**整数**用（`(load:i64, 地址, _, t)`）。
 *
 * `index()` 的取值宽度跟着**基址的元素类型**走（浮点元素就按 IEEE-754 解释），
 * 而"运行时数组的长度"这类值必须按整数读 —— 所以单独给一个原语。
 * @param address 运行时内存地址。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::loadInt(NodePtr address)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::LoadInt;
    if (address) node->kids = { std::move(address) };
    return node;
}

/**
 * @brief 下标陷阱：`(check, 下标, 上限, 标签)`，越界就终止运行。
 *
 * 表达式的值就是**下标本身**，所以可以直接塞进 `index()` / `offset()` 的位置参数。
 * @param index 待访问的元素索引。
 * @param limit 索引、步数或资源使用的上限。
 * @param label 诊断或跳转使用的标签。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::checkIndex(NodePtr index, NodePtr limit, Memory::String label)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::CheckIndex;
    node->text = std::move(label);
    if (index) node->kids.push_back(std::move(index));
    node->kids.push_back(std::move(limit));
    return node;
}

/**
 * @brief 按区块容量查下标：`(checkptr, 下标, 基址, 步长, 标签)`。
 *
 * 校验"第 index 个元素（`基址 + index × 步长`，共 `步长` 字节）"落在基址所在的
 * **那一块**里（堆块按分配大小、数据区按符号 `size=`）；基址不在任何一块里也报错。
 * @param index 待访问的元素索引。
 * @param base 起始地址、基值或目标表达式。
 * @param stride 相邻元素之间的字节步长。
 * @param label 诊断或跳转使用的标签。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::checkPointer(NodePtr index, NodePtr base, std::int64_t stride, Memory::String label)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::CheckPointer;
    node->text = std::move(label);
    node->intValue = stride;
    if (index) node->kids.push_back(std::move(index));
    node->kids.push_back(std::move(base));
    return node;
}

/**
 * @brief 构造赋值对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::assign(Memory::String target, NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Assign;
    node->text = std::move(target);
    node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 构造赋值对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::assign(NodePtr target, NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Assign;
    node->kids = { std::move(target), std::move(value) };
    return node;
}

/**
 * @brief 构造变量声明对应的低层语法节点。
 * @param name 名称文本。
 * @param type 类型描述。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::declare(Memory::String name, TypePtr type)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Declare;
    node->text = std::move(name);
    node->declared = std::move(type);
    return node;
}

/**
 * @brief 构造类型转换对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @param type 类型描述。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::cast(NodePtr value, TypePtr type)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Cast;
    node->declared = std::move(type);
    node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 构造条件分支对应的低层语法节点。
 * @param condition 控制分支或循环的条件。
 * @param thenBranch 条件成立时执行的分支。
 * @param elseBranch 条件不成立时执行的分支。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::ifThenElse(NodePtr condition, NodePtr thenBranch, NodePtr elseBranch)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::If;
    node->kids = { std::move(condition), std::move(thenBranch), std::move(elseBranch) };
    return node;
}

/**
 * @brief 构造前置条件循环对应的低层语法节点。
 * @param condition 控制分支或循环的条件。
 * @param body 待处理的函数体或循环体。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::whileLoop(NodePtr condition, NodePtr body)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::While;
    node->kids = { std::move(condition), std::move(body) };
    return node;
}

/**
 * @brief 构造后置条件循环对应的低层语法节点。
 * @param body 待处理的函数体或循环体。
 * @param condition 控制分支或循环的条件。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::doWhileLoop(NodePtr body, NodePtr condition)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::DoWhile;
    node->kids = { std::move(body), std::move(condition) };
    return node;
}

/**
 * @brief 构造跳转标签对应的低层语法节点。
 * @param name 名称文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::label(Memory::String name)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Label;
    node->text = std::move(name);
    return node;
}

/**
 * @brief 构造无条件跳转对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::jump(Memory::String target)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Jump;
    node->text = std::move(target);
    return node;
}

/**
 * @brief 构造函数返回对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::ret(NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Return;
    if (value) node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 构造表达式语句对应的低层语法节点。
 * @param expression 待解析或求值的表达式文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::exprStmt(NodePtr expression)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::ExprStmt;
    node->kids = { std::move(expression) };
    return node;
}

/**
 * @brief 构造值输出对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::write(NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Write;
    if (value) node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 构造语句块对应的低层语法节点。
 * @param statements 待处理的语句列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::block(Memory::Vector<NodePtr> statements)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Block;
    node->kids = std::move(statements);
    return node;
}

/**
 * @brief 构造文本输出对应的低层语法节点。
 * @param text 待处理的文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::print(Memory::String text)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Print;
    node->text = std::move(text);
    return node;
}

/**
 * @brief 构造字符输出对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::putc(NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Putc;
    node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 从输入读一个值写进变量（生成 `(read, _, _, 变量)`）。
 *
 * 输入项由调用方通过 `Machine::run(program, input)` 按顺序提供（每项一个
 * 数字字面量），类型按变量声明的类型收（整数 / 浮点都行）。
 * @param variable 变量名（要先 `Node::declare` 或当形参声明过）。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::read(Memory::String variable)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Read;
    node->text = std::move(variable);
    return node;
}

}  // namespace simple_ast
