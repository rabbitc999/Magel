#include "expression.h"
#include "expression_var.h"

void Expr_InitParser(Parser_T *parser, const char *input);
Node_T *Expr_ParseExpression(Parser_T *parser);
void Expr_ResetNodePool(void);

/**
 * @brief 打印缩进到字符串缓冲区
 * @param depth 缩进深度
 * @param buffer 输出缓冲区
 * @param length 缓冲区长度
 * @param offset 当前偏移量指针
 */
static void Expr_PrintIndent(uint32_t depth, char *buffer, uint32_t length, uint32_t *offset)
{
    for (uint32_t i = 0; i < depth; i++)
    {
        if (*offset + 2 < length)
        {
            buffer[(*offset)++] = ' ';
            buffer[(*offset)++] = ' ';
        }
        else
        {
            break;
        }
    }
    buffer[*offset] = '\0';
}

/**
 * @brief
 */
uint32_t Expr_PrintUnaryOp(Node_T *currentNode, char *buffer, uint32_t length, uint32_t offset)
{
    if (offset + 20 > length) { return offset; }

    offset += snprintf(buffer + offset, length - offset, "运算符: ");

    switch (currentNode->op)
    {
    case EXPR_TOKEN_PLUS:
        offset += snprintf(buffer + offset, length - offset, "+");
        break;
    case EXPR_TOKEN_MINUS:
        offset += snprintf(buffer + offset, length - offset, "-");
        break;
    case EXPR_TOKEN_NOT:
        offset += snprintf(buffer + offset, length - offset, "!");
        break;
    default:
        offset += snprintf(buffer + offset, length - offset, "未知");
        break;
    }

    if (currentNode->value.type == EXPR_VALUE_TYPE_BOOL)
    {
        offset += snprintf(buffer + offset, length - offset, " (%s)\n", currentNode->value.boolValue ? "TRUE" : "FALSE");
    }
    else
    {
        offset += snprintf(buffer + offset, length - offset, " (%.3f)\n", currentNode->value.numberValue);
    }

    return offset;
}

/**
 * @brief
 */
uint32_t Expr_PrintBinaryOp(Node_T *currentNode, char *buffer, uint32_t length, uint32_t offset)
{
    if (offset + 20 > length) { return offset; }

    offset += snprintf(buffer + offset, length - offset, "运算符: ");

    switch (currentNode->op)
    {
    case EXPR_TOKEN_PLUS:
        offset += snprintf(buffer + offset, length - offset, "+");
        break;
    case EXPR_TOKEN_MINUS:
        offset += snprintf(buffer + offset, length - offset, "-");
        break;
    case EXPR_TOKEN_MUL:
        offset += snprintf(buffer + offset, length - offset, "*");
        break;
    case EXPR_TOKEN_DIV:
        offset += snprintf(buffer + offset, length - offset, "/");
        break;
    case EXPR_TOKEN_MOD:
        offset += snprintf(buffer + offset, length - offset, "%%");
        break;
    case EXPR_TOKEN_GT:
        offset += snprintf(buffer + offset, length - offset, ">");
        break;
    case EXPR_TOKEN_LT:
        offset += snprintf(buffer + offset, length - offset, "<");
        break;
    case EXPR_TOKEN_EQ:
        offset += snprintf(buffer + offset, length - offset, "=");
        break;
    case EXPR_TOKEN_GE:
        offset += snprintf(buffer + offset, length - offset, ">=");
        break;
    case EXPR_TOKEN_LE:
        offset += snprintf(buffer + offset, length - offset, "<=");
        break;
    case EXPR_TOKEN_NE:
        offset += snprintf(buffer + offset, length - offset, "!=");
        break;
    case EXPR_TOKEN_AND:
        offset += snprintf(buffer + offset, length - offset, "&");
        break;
    case EXPR_TOKEN_OR:
        offset += snprintf(buffer + offset, length - offset, "|");
        break;
    case EXPR_TOKEN_POW:
        offset += snprintf(buffer + offset, length - offset, "^");
        break;
    case EXPR_TOKEN_COMMA:
        offset += snprintf(buffer + offset, length - offset, ",");
        break;
    default:
        offset += snprintf(buffer + offset, length - offset, "未知");
        break;
    }

    if (currentNode->value.type == EXPR_VALUE_TYPE_BOOL)
    {
        offset += snprintf(buffer + offset, length - offset, " (%s)\n", currentNode->value.boolValue ? "TRUE" : "FALSE");
    }
    else
    {
        offset += snprintf(buffer + offset, length - offset, " (%.3f)\n", currentNode->value.numberValue);
    }

    return offset;
}

/**
 * @brief
 */
uint32_t Expr_PrintFunction(Node_T *currentNode, char *buffer, uint32_t length, uint32_t offset)
{
    if (offset + 20 > length) { return offset; }

    offset += snprintf(buffer + offset, length - offset, "函数: ");

    // 打印函数名
    const char *funcName = "未知";
    for (uint32_t i = 0; i < FUNCTION_TABLE_SIZE; i++)
    {
        if (g_FunctionTable[i].type == currentNode->funcType)
        {
            funcName = g_FunctionTable[i].Name;
            break;
        }
    }

    // 打印参数个数
    offset += snprintf(buffer + offset, length - offset, "%s (参数个数: %d)", funcName, currentNode->data.function.argCount);

    if (currentNode->value.type == EXPR_VALUE_TYPE_BOOL)
    {
        offset += snprintf(buffer + offset, length - offset, " (%s)\n", currentNode->value.boolValue ? "TRUE" : "FALSE");
    }
    else
    {
        offset += snprintf(buffer + offset, length - offset, " (%.3f)\n", currentNode->value.numberValue);
    }

    return offset;
}

/**
 * @brief
 */
uint32_t Expr_PrintVariable(Node_T *currentNode, char *buffer, uint32_t length, uint32_t offset)
{
    if (offset + 20 > length) { return offset; }

    const char *variableName = "未知";
    if (currentNode->data.variableIndex < VARIABLE_TABLE_SIZE)
    {
        variableName = g_VariableTable[currentNode->data.variableIndex].Name;
    }

    offset += snprintf(buffer + offset, length - offset, "变量: %s (%.3f)\n", variableName, currentNode->value.numberValue);

    return offset;
}

/**
 * @brief
 */
uint32_t Expr_PrintContast(Node_T *currentNode, char *buffer, uint32_t length, uint32_t offset)
{
    if (offset + 20 > length) { return offset; }

    const char *constantName = "未知";
    if (currentNode->data.constantIndex < CONSTANT_TABLE_SIZE)
    {
        constantName = g_ConstantTable[currentNode->data.constantIndex].Name;
    }

    offset += snprintf(buffer + offset, length - offset, "常量: %s (%.3f)\n", constantName, currentNode->value.numberValue);

    return offset;
}

/**
 * @brief
 */
uint32_t Expr_PrintAssignment(Node_T *currentNode, char *buffer, uint32_t length, uint32_t offset)
{
    if (offset + 20 > length) { return offset; }

    offset += snprintf(buffer + offset, length - offset, "赋值运算: ");

    const char *variableName = "未知变量";

    if (currentNode->data.children.left != NULL && currentNode->data.children.left->type == EXPR_NODE_VARIABLE
        && currentNode->data.children.left->data.variableIndex < VARIABLE_TABLE_SIZE)
    {
        variableName = g_VariableTable[currentNode->data.children.left->data.variableIndex].Name;
    }

    if (currentNode->value.type == EXPR_VALUE_TYPE_BOOL)
    {
        offset += snprintf(buffer + offset, length - offset, " (变量%s := %s)\n", variableName, currentNode->value.boolValue ? "TRUE" : "FALSE");
    }
    else
    {
        offset += snprintf(buffer + offset, length - offset, " (变量%s := %.3f)\n", variableName, currentNode->value.numberValue);
    }

    return offset;
}

/**
 * @brief
 */
uint32_t Expr_PrintNumber(Node_T *currentNode, char *buffer, uint32_t length, uint32_t offset)
{
    if (offset + 20 > length) { return offset; }

    offset += snprintf(buffer + offset, length - offset, "数字: %.3f (%.3f)\n", currentNode->data.numberValue, currentNode->value.numberValue);

    return offset;
}

/**
 * @brief
 */
uint32_t Expr_PrintNULL(Node_T *currentNode, char *buffer, uint32_t length, uint32_t offset)
{
    if (offset + 20 > length) { return offset; }

    offset += snprintf(buffer + offset, length - offset, "空节点\n");

    return offset;
}

/**
 * @brief 打印语法树到字符串缓冲区（迭代版本）
 * @param node 语法树节点
 * @param depth 当前深度
 * @param buffer 输出缓冲区
 * @param length 缓冲区长度
 */
void Expr_PrintTree(Node_T *node, int depth, char *buffer, uint32_t length)
{
    if (node == NULL || buffer == NULL || length == 0)
    {
        buffer[0] = '\0';
        return;
    }

    uint32_t offset = 0;
    buffer[0] = '\0';

    /* 定义栈结构用于迭代遍历 */
    typedef struct __StackItem_T
    {
        Node_T *node;
        int depth;
        uint8_t visited;
    } StackItem_T;

    StackItem_T stack[EXPR_MAX_SYNTAX_NODES];
    int stackTop = 0;

    /* 根节点入栈 */
    if (node != NULL)
    {
        stack[stackTop].node = node;
        stack[stackTop].depth = depth;
        stack[stackTop].visited = FALSE;
        stackTop++;
    }

    while (stackTop > 0 && offset < length - 1)
    {
        /* 弹出栈顶元素 */
        stackTop--;
        Node_T *currentNode = stack[stackTop].node;
        int currentDepth = stack[stackTop].depth;
        uint8_t visited = stack[stackTop].visited;

        /* 忽略访问的节点 */
        if (visited) { continue; }

        // 检查是否是逗号运算符且需要跳过
        if (currentNode != NULL && currentNode->type == EXPR_NODE_BINARY_OP && currentNode->op == EXPR_TOKEN_COMMA)
        {
            // 跳过逗号运算符，直接处理其子节点
            if (currentNode->data.children.right != NULL)
            {
                if (stackTop < EXPR_MAX_SYNTAX_NODES - 1)
                {
                    stack[stackTop].node = currentNode->data.children.right;
                    stack[stackTop].depth = currentDepth;  // 保持相同深度
                    stack[stackTop].visited = FALSE;
                    stackTop++;
                }
            }
            if (currentNode->data.children.left != NULL)
            {
                if (stackTop < EXPR_MAX_SYNTAX_NODES - 1)
                {
                    stack[stackTop].node = currentNode->data.children.left;
                    stack[stackTop].depth = currentDepth;  // 保持相同深度
                    stack[stackTop].visited = FALSE;
                    stackTop++;
                }
            }
            continue;
        }

        /* 第一次访问该节点，打印节点缩进 */
        Expr_PrintIndent(currentDepth, buffer, length, &offset);

        /* 打印NULL节点 */
        if (currentNode == NULL)
        {
            offset = Expr_PrintNULL(currentNode, buffer, length, offset);
            continue;
        }

        switch (currentNode->type)
        {
        case EXPR_NODE_ASSIGNMENT:
        {
            offset = Expr_PrintAssignment(currentNode, buffer, length, offset);
            break;
        }
        case EXPR_NODE_NUMBER:
        {
            offset = Expr_PrintNumber(currentNode, buffer, length, offset);
            break;
        }
        case EXPR_NODE_VARIABLE:
        {
            offset = Expr_PrintVariable(currentNode, buffer, length, offset);
            break;
        }
        case EXPR_NODE_CONSTANT:
        {
            offset = Expr_PrintContast(currentNode, buffer, length, offset);
            break;
        }
        case EXPR_NODE_UNARY_OP:
        {
            offset = Expr_PrintUnaryOp(currentNode, buffer, length, offset);
            break;
        }
        case EXPR_NODE_BINARY_OP:
        {
            offset = Expr_PrintBinaryOp(currentNode, buffer, length, offset);
            break;
        }
        case EXPR_NODE_FUNCTION:
        {
            offset = Expr_PrintFunction(currentNode, buffer, length, offset);
            break;
        }
        }

        /* 标记为已访问，并将子节点按逆序入栈（保证先处理左子树） */
        stack[stackTop].node = currentNode;
        stack[stackTop].depth = currentDepth;
        stack[stackTop].visited = TRUE;
        stackTop++;

        /* 将子节点入栈 */
        if (currentNode->type == EXPR_NODE_BINARY_OP)
        {
            /* 先右后左，保证出栈时先左后右 */
            if (stackTop < EXPR_MAX_SYNTAX_NODES - 1 && currentNode->data.children.right != NULL)
            {
                stack[stackTop].node = currentNode->data.children.right;
                stack[stackTop].depth = currentNode->op == EXPR_TOKEN_COMMA ? currentDepth : currentDepth + 1;
                stack[stackTop].visited = FALSE;
                stackTop++;
            }
            if (stackTop < EXPR_MAX_SYNTAX_NODES - 1 && currentNode->data.children.left != NULL)
            {
                stack[stackTop].node = currentNode->data.children.left;
                stack[stackTop].depth = currentNode->op == EXPR_TOKEN_COMMA ? currentDepth : currentDepth + 1;
                stack[stackTop].visited = FALSE;
                stackTop++;
            }
        }
        else if (currentNode->type == EXPR_NODE_UNARY_OP && currentNode->data.children.left != NULL)
        {
            if (stackTop < EXPR_MAX_SYNTAX_NODES - 1)
            {
                stack[stackTop].node = currentNode->data.children.left;
                stack[stackTop].depth = currentDepth + 1;
                stack[stackTop].visited = FALSE;
                stackTop++;
            }
        }
        else if (currentNode->type == EXPR_NODE_ASSIGNMENT && currentNode->data.children.right != NULL)
        {
            if (stackTop < EXPR_MAX_SYNTAX_NODES - 1)
            {
                stack[stackTop].node = currentNode->data.children.right;
                stack[stackTop].depth = currentDepth + 1;
                stack[stackTop].visited = FALSE;
                stackTop++;
            }
        }
        else if (currentNode->type == EXPR_NODE_FUNCTION)
        {
            // 将参数链表按逆序入栈（保证先处理第一个参数）
            if (currentNode->data.function.argRoot != NULL)
            {
                if (stackTop < EXPR_MAX_SYNTAX_NODES - 1)
                {
                    stack[stackTop].node = currentNode->data.function.argRoot;
                    stack[stackTop].depth = currentDepth + 1;
                    stack[stackTop].visited = FALSE;
                    stackTop++;
                }
            }
        }
    }

    /* 确保字符串以空字符结尾 */
    if (offset >= length)
    {
        buffer[length - 1] = '\0';
    }
    else
    {
        buffer[offset] = '\0';
    }
}

/**
 * @brief 打印当前变量值到字符串缓冲区
 * @param buffer 输出缓冲区
 * @param length 缓冲区长度
 */
/**
 * @brief 打印当前变量值到字符串缓冲区
 * @param buffer 输出缓冲区
 * @param length 缓冲区长度
 */
void Expr_PrintVariables(char *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0)
    {
        return;
    }

    uint32_t offset = 0;
    offset += snprintf(buffer + offset, length - offset, "当前变量值:\n");

    for (uint32_t i = 0; i < VARIABLE_TABLE_SIZE && offset < length - 1; i++)
    {
        // 每5个变量换一行（除了第一个变量）
        if (i > 0 && i % 5 == 0)
        {
            offset += snprintf(buffer + offset, length - offset, "\n");
        }

        // 格式化输出变量
        offset += snprintf(buffer + offset, length - offset, "  %s=%.3f", g_VariableTable[i].Name, g_VariableTable[i].value);

        // 如果不是每行的最后一个变量，添加空格分隔
        if ((i + 1) % 5 != 0 && i < VARIABLE_TABLE_SIZE - 1)
        {
            offset += snprintf(buffer + offset, length - offset, " ");
        }
    }

    // 确保最后有换行
    if (offset < length - 1)
    {
        offset += snprintf(buffer + offset, length - offset, "\n");
    }

    if (offset >= length)
    {
        buffer[length - 1] = '\0';
    }
}

/**
 * @brief 获取函数的详细描述
 * @param funcType 函数类型
 * @return 函数描述字符串
 */
static const char *Expr_GetFunctionDescription(FunctionType_T funcType)
{
    switch (funcType)
    {
    case EXPR_FUNC_SIN: return "正弦函数 sin(x)";
    case EXPR_FUNC_COS: return "余弦函数 cos(x)";
    case EXPR_FUNC_TAN: return "正切函数 tan(x)";
    case EXPR_FUNC_ASIN: return "反正弦函数 asin(x), x∈[-1,1]";
    case EXPR_FUNC_ACOS: return "反余弦函数 acos(x), x∈[-1,1]";
    case EXPR_FUNC_ATAN: return "反正切函数 atan(x)";
    case EXPR_FUNC_SINH: return "双曲正弦函数 sinh(x)";
    case EXPR_FUNC_COSH: return "双曲余弦函数 cosh(x)";
    case EXPR_FUNC_TANH: return "双曲正切函数 tanh(x)";
    case EXPR_FUNC_LN: return "自然对数 ln(x), x>0";
    case EXPR_FUNC_LOG10: return "常用对数 log10(x), x>0";
    case EXPR_FUNC_EXP: return "指数函数 exp(x)";
    case EXPR_FUNC_POW: return "幂函数 pow(x,y) = x^y";
    case EXPR_FUNC_SQRT: return "平方根 sqrt(x), x≥0";
    case EXPR_FUNC_ABS: return "绝对值 abs(x)";
    case EXPR_FUNC_CEIL: return "向上取整 ceil(x)";
    case EXPR_FUNC_FLOOR: return "向下取整 floor(x)";
    case EXPR_FUNC_ROUND: return "四舍五入 round(x)";
    case EXPR_FUNC_MAX: return "最大值 Max(x1,x2,...)";
    case EXPR_FUNC_MIN: return "最小值 Min(x1,x2,...)";
    case EXPR_FUNC_IF: return "条件判断 if(cond,true_val,false_val)";
    case EXPR_FUNC_SUM: return "求和 sum(x1,x2,...)";
    default: return "未知函数";
    }
}

/**
 * @brief 打印支持的函数列表到字符串缓冲区
 * @param buffer 输出缓冲区
 * @param length 缓冲区长度
 */
void Expr_PrintFunctions(char *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0)
    {
        return;
    }

    uint32_t offset = 0;
    offset += snprintf(buffer + offset, length - offset, "==============================\n");
    offset += snprintf(buffer + offset, length - offset, "========支持的函数列表========\n");
    offset += snprintf(buffer + offset, length - offset, "==============================\n");

    // 按函数类型分组显示（避免重复显示别名）
    uint8_t displayed[36] = { 0 };  // 标记已显示的函数类型

#define MIN(x, y) ((x) < (y) ? (x) : (y))
    for (uint8_t i = 0; i < MIN(FUNCTION_TABLE_SIZE, 36) && offset < length - 1; i++)
    {
        // 如果这个函数类型已经显示过，跳过（避免显示别名）
        if (displayed[g_FunctionTable[i].type])
            continue;

        displayed[g_FunctionTable[i].type] = 1;

        // 收集这个函数类型的所有别名
        char aliases[256] = "";
        int alias_count = 0;

        for (uint8_t j = i; j < FUNCTION_TABLE_SIZE; j++)
        {
            if (g_FunctionTable[j].type == g_FunctionTable[i].type)
            {
                if (alias_count > 0)
                {
                    strcat(aliases, ", ");
                }
                strcat(aliases, g_FunctionTable[j].Name);
                alias_count++;
            }
        }

        // 格式化函数信息
        if (g_FunctionTable[i].minArgs == g_FunctionTable[i].maxArgs)
        {
            if (g_FunctionTable[i].minArgs == 1)
            {
                offset += snprintf(buffer + offset, length - offset, "%s - 参数: 1个\n", aliases);
            }
            else
            {
                offset += snprintf(buffer + offset, length - offset, "%s - 参数: %d个\n", aliases, g_FunctionTable[i].minArgs);
            }
        }
        else
        {
            offset += snprintf(buffer + offset, length - offset, "%s - 参数: %d-%d个\n", aliases, g_FunctionTable[i].minArgs, g_FunctionTable[i].maxArgs);
        }

        // 添加函数描述
        const char *description = Expr_GetFunctionDescription(g_FunctionTable[i].type);
        if (description)
        {
            offset += snprintf(buffer + offset, length - offset, "  %s\n", description);
        }

        offset += snprintf(buffer + offset, length - offset, "\n");
    }
#undef MIN

    if (offset >= length)
    {
        buffer[length - 1] = '\0';
    }
}

/**
 * @brief 获取变量值
 * @param Name 变量名
 * @param value 用于存储变量值的指针
 * @return 是否成功找到变量 (TRUE: 成功, FALSE: 失败)
 */
uint8_t Expr_GetVariable(const char *Name, float *value)
{
    if (Name == NULL || value == NULL)
    {
        return FALSE;
    }

    for (uint32_t i = 0; i < VARIABLE_TABLE_SIZE; i++)
    {
        if (String_Compare(g_VariableTable[i].Name, Name, EXPR_MAX_VARIABLE_NAME_LENGTH) == 0)
        {
            *value = g_VariableTable[i].value;
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief 获取所有变量的名称列表
 * @param names 用于存储变量名称的指针数组
 * @param maxCount 最大变量数量
 * @return 实际获取的变量数量
 */
uint16_t Expr_GetVariableNames(const char **names, uint16_t maxCount)
{
    if (names == NULL || maxCount == 0)
    {
        return 0;
    }

    uint16_t count = 0;
    for (uint32_t i = 0; i < VARIABLE_TABLE_SIZE && count < maxCount; i++)
    {
        names[count++] = g_VariableTable[i].Name;
    }

    return count;
}

/**
 * @brief 获取变量数量
 * @return 变量表中的变量总数
 */
uint16_t Expr_GetVariableCount(void)
{
    return VARIABLE_TABLE_SIZE;
}

/**
 * @brief 获取常量值
 * @param Name 常量名
 * @param value 用于存储常量值的指针
 * @return 是否成功找到常量 (TRUE: 成功, FALSE: 失败)
 */
uint8_t Expr_GetConstant(const char *Name, float *value)
{
    if (Name == NULL || value == NULL)
    {
        return FALSE;
    }

    for (uint32_t i = 0; i < CONSTANT_TABLE_SIZE; i++)
    {
        if (String_Compare(g_ConstantTable[i].Name, Name, EXPR_MAX_VARIABLE_NAME_LENGTH) == 0)
        {
            *value = g_ConstantTable[i].value;
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief 获取所有常量的名称列表
 * @param names 用于存储常量名称的指针数组
 * @param maxCount 最大常量数量
 * @return 实际获取的常量数量
 */
uint16_t Expr_GetConstantNames(const char **names, uint16_t maxCount)
{
    if (names == NULL || maxCount == 0)
    {
        return 0;
    }

    uint16_t count = 0;
    for (uint32_t i = 0; i < CONSTANT_TABLE_SIZE && count < maxCount; i++)
    {
        names[count++] = g_ConstantTable[i].Name;
    }

    return count;
}

/**
 * @brief 获取常量数量
 * @return 常量表中的常量总数
 */
uint16_t Expr_GetConstantCount(void)
{
    return CONSTANT_TABLE_SIZE;
}

/**
 * @brief 评估表达式字符串到输出缓冲区（同时构建语法树和求值）
 * @param expr 要评估的表达式字符串
 * @param hasError 指向布尔变量的指针，用于指示是否发生错误
 * @param buffer 输出缓冲区
 * @param length 缓冲区长度
 * @return 语法树根节点
 */
Node_T *Expr_EvaluateExpression(const char *expr, uint8_t *hasError, char *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0)
    {
        *hasError = TRUE;
        return NULL;
    }

    Parser_T parser;
    Expr_InitParser(&parser, expr);
    Expr_ResetNodePool();

    Node_T *root = Expr_ParseExpression(&parser);

    uint32_t offset = 0;
    buffer[0] = '\0';

    if (parser.error != EXPR_ERROR_NONE)
    {
        offset += snprintf(buffer + offset, length - offset, "%s\n", parser.errorMsg);
        *hasError = TRUE;
    }
    else if (parser.currentToken.type != EXPR_TOKEN_EOF)
    {
        offset += snprintf(buffer + offset, length - offset, "错误: 意外的标记在表达式之后\n");
        *hasError = TRUE;
    }
    else if (root == NULL)
    {
        offset += snprintf(buffer + offset, length - offset, "错误: 无法构建语法树\n");
        *hasError = TRUE;
    }
    else
    {
        *hasError = FALSE;
        /* 结果已经在节点中计算好了 */
        if (root->value.type == EXPR_VALUE_TYPE_BOOL)
        {
            offset += snprintf(buffer + offset, length - offset,
                               "结果(布尔值): %s \n",
                               root->value.boolValue ? "TRUE" : "FALSE");
        }
        else
        {
            offset += snprintf(buffer + offset, length - offset,
                               "结果(数值): %.3f\n",
                               root->value.numberValue);
        }
    }

    if (offset >= length)
    {
        buffer[length - 1] = '\0';
    }
    else
    {
        buffer[offset] = '\0';
    }

    return root;
}

/**
 * @brief 计算表达式并返回结果
 * @param expr 要计算的表达式字符串
 * @param result 指向结果结构体的指针，用于存储计算结果
 * @param error 错误信息缓冲区
 * @param errorLength 错误信息缓冲区长度
 * @return 如果出错返回TRUE，否则返回FALSE
 */
uint8_t Expr_Calculate(const char *expr, Value_T *result, char *error, uint32_t errorLength)
{
    if (expr == NULL || result == NULL || error == NULL || errorLength == 0)
    {
        if (error && errorLength > 0)
        {
            strncpy(error, "无效的参数", errorLength - 1);
            error[errorLength - 1] = '\0';
        }
        return TRUE;
    }

    if (String_GetLength(expr, EXPR_MAX_EXPRESSION_LENGTH) == 0)
    {
        if (errorLength > 0)
        {
            strncpy(error, "表达式为空", errorLength - 1);
            error[errorLength - 1] = '\0';
        }
        return TRUE;
    }

    Parser_T parser;
    Expr_InitParser(&parser, expr);
    Expr_ResetNodePool();

    Node_T *root = Expr_ParseExpression(&parser);

    if (parser.error != EXPR_ERROR_NONE)
    {
        if (errorLength > 0)
        {
            strncpy(error, parser.errorMsg, errorLength - 1);
            error[errorLength - 1] = '\0';
        }
        return TRUE;
    }
    else if (parser.currentToken.type != EXPR_TOKEN_EOF)
    {
        if (errorLength > 0)
        {
            strncpy(error, "表达式解析不完整", errorLength - 1);
            error[errorLength - 1] = '\0';
        }
        return TRUE;
    }
    else if (root == NULL)
    {
        if (errorLength > 0)
        {
            strncpy(error, "无法构建语法树", errorLength - 1);
            error[errorLength - 1] = '\0';
        }
        return TRUE;
    }

    /* 复制结果 */
    *result = root->value;

    /* 添加结尾符号 */
    if (errorLength > 0)
    {
        error[0] = '\0';
    }

    return FALSE;
}
