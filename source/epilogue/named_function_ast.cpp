/**
 * @brief 将语句级具名函数转换为有确定签名的局部闭包绑定。
 * @param resultType 明确声明的返回类型。
 * @param name 绑定名称。
 * @param params 参数列表。
 * @param body 函数体；负一时仅建立可后续赋值的函数变量。
 * @param capture 默认捕获方式，默认只读借用。
 * @retval 局部函数值声明。
 */
int M_NamedFunction(int resultType, int name, int params, int body, int capture)
{
    const int signature = M_FunctionType(params, resultType);
    const int value = body < 0 ? -1 : M_Lambda(resultType, params, body, capture);
    const int declaration = M_Decl(name, -1, value);
    g_nodes[declaration].type = signature;
    g_nodes[declaration].type2 = 0;
    return M_LocalDecl(signature, M_List1(declaration), 0);
}
