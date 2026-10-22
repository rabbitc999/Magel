#ifndef CHALI_MACRO_AST_HPP
#define CHALI_MACRO_AST_HPP

/**
 * @brief 构造宏形参组，保留有无圆括号的区别。
 * @param names 形参节点列表。
 * @return 参数组节点编号。
 */
int M_MacroParameters(int names, int opening = '(');
/**
 * @brief 追加显式参数组，保留组的类型与顺序。
 * @param groups 已有组结构。
 * @param group 待追加组。
 * @return 合并的参数组结构。
 */
int M_MacroParameterGroups(int groups, int group);
/**
 * @brief 构造一个宏形参，不创建运行时变量。
 * @param name 名称池编号。
 * @return 形参节点编号。
 */
int M_MacroParameter(int name, bool variadic = false);
/**
 * @brief 构造编译期片段列表循环。
 * @param variable 循环参数名称。
 * @param source 变长参数名称。
 * @param body 循环模板。
 * @return 循环节点。
 */
int M_MacroFor(int variable, int source, int body);
/**
 * @brief 保存通用类型应用，保持语法分类独立于模板是否声明。
 * @param name 类型名称池编号。
 * @param arguments 类型项节点列表。
 * @return 类型池编号。
 */
int M_AstGenericType(int name, int arguments);
/**
 * @brief 从限定名称表达式保存通用类型应用。
 * @param name 限定名称表达式。
 * @param arguments 类型项节点列表。
 * @return 类型池编号。
 */
int M_QualifiedAstGenericType(int name, int arguments);
/**
 * @brief 保存类型应用限定的静态成员，不在语法分类时实例化该类型。
 * @param type 类型池编号。
 * @param member 成员名称池编号。
 * @return 限定成员表达式编号。
 */
int M_AstTypeQualifiedRef(int type, int member);
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
 * @brief 建立实际参数组，括号类别由文法明确给出。
 * @param arguments 已解析表达式或原文片段引用的列表。
 * @param opening 左括号字符。
 * @return 参数组编号。
 */
int M_MacroCallGroup(int arguments, int opening);
/**
 * @brief 将文法读取的全部实参组关联到宏调用。
 * @param name 宏名称池编号。
 * @param groups 按源顺序保存的参数组列表。
 * @param types 类型参数列表，负值表示未显式提供。
 * @return 调用节点编号。
 */
int M_MacroGroupedCall(int name, int groups, int types);
/**
 * @brief 保存词法边界层捕获的原文引用，不将片段误当运行时标识符。
 * @param marker 内部片段引用在名称池中的编号。
 * @return 片段引用节点编号。
 */
int M_MacroFragmentReference(int marker);
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
