#ifndef MAGEL_MACRO_AST_HPP
#define MAGEL_MACRO_AST_HPP

/**
 * @brief 构造宏形参组，保留有无圆括号的区别。
 * @param names 形参节点列表。
 * @return 参数组节点编号。
 */
int M_MacroParameters(int names);
/**
 * @brief 构造一个宏形参，不创建运行时变量。
 * @param name 名称池编号。
 * @return 形参节点编号。
 */
int M_MacroParameter(int name);
/**
 * @brief 构造已解析的宏模板，表达式尾项不加括号。
 * @param statements 语句节点列表。
 * @param expression 可选末尾表达式，负值表示仅语句。
 * @return 模板节点编号。
 */
int M_MacroTemplate(int statements, int expression);
/**
 * @brief 构造多行宏定义，不执行文本替换。
 * @param name 宏名称池编号。
 * @param parameters 参数组节点，负值表示没有参数组。
 * @param body 宏模板节点。
 * @param generics 宏泛型参数列表。
 * @return 宏定义节点编号。
 */
int M_MacroDefinition(int name, int parameters, int body, int generics);
/**
 * @brief 构造宏调用，保留完整实参 AST。
 * @param name 含美元前缀的名称池编号。
 * @param arguments 实参节点列表，负值表示没有参数组。
 * @return 宏调用节点编号。
 */
int M_MacroCall(int name, int arguments);
/**
 * @brief 保存宏泛型名称及可选的实参类型推导来源。
 * @param name 泛型名称池编号。
 * @param source 来源形参名称，负值表示结果上下文推导。
 * @return 泛型参数节点。
 */
int M_MacroGeneric(int name, int source);
/**
 * @brief 保存显式泛型调用，不在解析期选择具体类型。
 * @param name 带美元前缀的宏名。
 * @param types 显式类型参数列表。
 * @param arguments 实参列表，负值表示无参数组。
 * @return 调用节点。
 */
int M_MacroTypedCall(int name, int types, int arguments);
/**
 * @brief 创建语法类别匹配分支，空类别表示兜底。
 * @param category 类别名称池编号，负值表示兜底。
 * @param body 分支模板。
 * @return 分支节点。
 */
int M_MacroBranch(int category, int body);
/**
 * @brief 将一个候选分支放到后续分支之前，保持源代码顺序。
 * @param branch 当前分支节点。
 * @param rest 后续分支列表。
 * @return 有序分支列表。
 */
int M_MacroBranches(int branch, int rest);
/**
 * @brief 建立只在编译期执行的语法类别匹配。
 * @param parameter 待匹配参数名。
 * @param branches 有序候选分支。
 * @return 匹配节点。
 */
int M_MacroMatch(int parameter, int branches);
/**
 * @brief 建立正则匹配节点并保留尚未解码的语言字符串。
 * @param parameter 待匹配参数名。
 * @param pattern 字符串名称池编号。
 * @param body 成功分支模板。
 * @param otherwise 可选兜底模板，负值表示不存在。
 * @return 正则节点。
 */
int M_MacroRegex(int parameter, int pattern, int body, int otherwise);
#endif
