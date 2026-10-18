#include "expression.h"
#include "expression_parse.h"

// 静态节点池
static Node_T g_NodePool[EXPR_MAX_SYNTAX_NODES];
static uint32_t g_NodeCount = 0;

/**
 * @brief 初始化解析器
 * @param parser 指向解析器结构体的指针
 * @param input 要解析的表达式字符串
 */
void Expr_InitParser(Parser_T *parser, const char *input)
{
    parser->input = input;
    parser->position = 0;
    parser->error = EXPR_ERROR_NONE;
    parser->errorMsg[0] = '\0';
    parser->variables = (Variable_T *)g_VariableTable;
    parser->variableCount = VARIABLE_TABLE_SIZE;
    parser->constants = g_ConstantTable;  // 初始化常量表
    parser->constantCount = CONSTANT_TABLE_SIZE;
    Expr_NextToken(parser);
}

/**
 * @brief 获取下一个词法单元
 * @param parser 指向解析器结构体的指针
 */
static void Expr_NextToken(Parser_T *parser)
{
    const char *input = parser->input;
    uint32_t pos = parser->position;

    /* 跳过空白字符 */
    while (input[pos] != '\0' && isspace((unsigned char)input[pos]))
    {
        pos++;
    }

    if (input[pos] == '\0')
    {
        parser->currentToken.type = EXPR_TOKEN_EOF;
        parser->currentToken.position = pos;
        return;
    }

    parser->currentToken.position = pos;
    char ch = input[pos];

    /* 识别双字符运算符 */
    if (ch == '>' && input[pos + 1] == '=')
    {
        parser->currentToken.type = EXPR_TOKEN_GE;
        parser->position = pos + 2;
        return;
    }
    if (ch == '<' && input[pos + 1] == '=')
    {
        parser->currentToken.type = EXPR_TOKEN_LE;
        parser->position = pos + 2;
        return;
    }
    if (ch == '!' && input[pos + 1] == '=')
    {
        parser->currentToken.type = EXPR_TOKEN_NE;
        parser->position = pos + 2;
        return;
    }
    if (ch == '=' && input[pos + 1] == '=')
    {
        /* 支持双等号，但规范中用单等号 */
        parser->currentToken.type = EXPR_TOKEN_EQ;
        parser->position = pos + 2;
        return;
    }
    if (ch == ':' && input[pos + 1] == '=')
    {
        parser->currentToken.type = EXPR_TOKEN_ASSIGN;
        parser->position = pos + 2;
        return;
    }

    /* 识别单字符运算符 */
    switch (ch)
    {
    case '+':
        parser->currentToken.type = EXPR_TOKEN_PLUS;
        parser->position = pos + 1;
        break;
    case '-':
        parser->currentToken.type = EXPR_TOKEN_MINUS;
        parser->position = pos + 1;
        break;
    case '*':
        parser->currentToken.type = EXPR_TOKEN_MUL;
        parser->position = pos + 1;
        break;
    case '/':
        parser->currentToken.type = EXPR_TOKEN_DIV;
        parser->position = pos + 1;
        break;
    case '>':
        parser->currentToken.type = EXPR_TOKEN_GT;
        parser->position = pos + 1;
        break;
    case '<':
        parser->currentToken.type = EXPR_TOKEN_LT;
        parser->position = pos + 1;
        break;
    case '=':
        parser->currentToken.type = EXPR_TOKEN_EQ;
        parser->position = pos + 1;
        break;
    case '&':
        parser->currentToken.type = EXPR_TOKEN_AND;
        parser->position = pos + 1;
        break;
    case '|':
        parser->currentToken.type = EXPR_TOKEN_OR;
        parser->position = pos + 1;
        break;
    case '!':
        parser->currentToken.type = EXPR_TOKEN_NOT;
        parser->position = pos + 1;
        break;
    case '(':
        parser->currentToken.type = EXPR_TOKEN_LPAREN;
        parser->position = pos + 1;
        break;
    case ')':
        parser->currentToken.type = EXPR_TOKEN_RPAREN;
        parser->position = pos + 1;
        break;
    case '%':
        parser->currentToken.type = EXPR_TOKEN_MOD;
        parser->position = pos + 1;
        break;
    case '^':
        parser->currentToken.type = EXPR_TOKEN_POW;
        parser->position = pos + 1;
        break;
    case ';':
    case ',':
        parser->currentToken.type = EXPR_TOKEN_COMMA;
        parser->position = pos + 1;
        break;
    default:
        /* 识别数字 */
        if (isdigit((unsigned char)ch) || ch == '.')
        {
            char *endptr;
            parser->currentToken.numberValue = strtod(&input[pos], &endptr);
            parser->currentToken.type = EXPR_TOKEN_NUMBER;
            parser->position = endptr - input;
            return;
        }
        /* 识别函数名 */
        if (isalpha((unsigned char)ch))
        {
            uint32_t start = pos;
            while (input[pos] && (isalnum((unsigned char)input[pos]) || input[pos] == '_'))
            {
                pos++;
            }
            uint32_t len = pos - start;

            // 检查后面是否是左括号，如果是则识别为函数
            uint32_t tempPos = pos;
            while (input[tempPos] && isspace((unsigned char)input[tempPos]))
            {
                tempPos++;
            }

            if (input[tempPos] == '(')
            {
                // 是函数调用
                FunctionType_T funcType = Expr_FindFunction(&input[start], len);
                if (funcType != EXPR_FUNC_UNKNOW)  // 这里需要更好的错误处理
                {
                    parser->currentToken.type = EXPR_TOKEN_FUNCTION;
                    parser->currentToken.funcType = funcType;
                    parser->currentToken.funcNameLength = len;
                    parser->currentToken.funcNameStart = start;
                    parser->position = pos;
                    return;
                }
            }

            // 否则识别为变量或常量
            parser->currentToken.variableIndex = VARIABLE_TABLE_SIZE;

            // 首先检查是否是常量
            for (uint32_t i = 0; i < parser->constantCount; i++)
            {
                if (String_GetLength(parser->constants[i].Name, EXPR_MAX_VARIABLE_NAME_LENGTH) == len && String_Compare(parser->constants[i].Name, &input[start], len) == 0)
                {
                    parser->currentToken.type = EXPR_TOKEN_CONSTANT;  // 新增常量标记
                    parser->currentToken.variableIndex = i;
                    parser->currentToken.numberValue = parser->constants[i].value;
                    parser->position = pos;
                    return;
                }
            }

            // 否则识别为变量
            for (uint32_t i = 0; i < VARIABLE_TABLE_SIZE; i++)
            {
                if (String_GetLength(g_VariableTable[i].Name, EXPR_MAX_VARIABLE_NAME_LENGTH) == len && String_Compare(g_VariableTable[i].Name, &input[start], len) == 0)
                {
                    parser->currentToken.variableIndex = i;
                    break;
                }
            }
            parser->currentToken.type = EXPR_TOKEN_VARIABLE;
            parser->position = pos;
            return;
        }
        Expr_SetError(parser, EXPR_ERROR_SYNTAX_ERROR, "无效字符");
        parser->currentToken.type = EXPR_TOKEN_EOF;
        return;
    }
}

/**
 * @brief 设置变量的值
 * @param Name 变量名
 * @param value 变量的值
 * @return 是否设置成功，如果返回TRUE则设置成功，否则设置失败
 */
uint8_t Expr_SetVariable(const char *Name, float value)
{
    for (uint32_t i = 0; i < VARIABLE_TABLE_SIZE; i++)
    {
        if (String_Compare(g_VariableTable[i].Name, Name, EXPR_MAX_VARIABLE_NAME_LENGTH) == 0)
        {
            g_VariableTable[i].value = value;
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief 重置节点池
 */
void Expr_ResetNodePool(void)
{
    g_NodeCount = 0;
}

/**
 * @brief 创建新节点
 * @param type 节点类型
 * @return 新节点指针，失败返回NULL
 */
static Node_T *Expr_CreateNode(NodeType_T type)
{
    if (g_NodeCount >= EXPR_MAX_SYNTAX_NODES)
    {
        return NULL;  // 节点池耗尽
    }
    Node_T *node = &g_NodePool[g_NodeCount++];
    memset(node, 0, sizeof(Node_T));
    node->type = type;
    node->value.type = EXPR_VALUE_TYPE_NUMBER;
    node->value.numberValue = 0.0;
    return node;
}

/**
 * @brief 解析表达式
 * @param parser 指向解析器结构体的指针
 * @return 语法树根节点
 */
Node_T *Expr_ParseExpression(Parser_T *parser)
{
    return Expr_ParseCommaExpression(parser);
}

/**
 * @brief 解析逗号表达式（参数列表）
 * @param parser 解析器指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseCommaExpression(Parser_T *parser)
{
    Node_T *left = Expr_ParseAssignment(parser);

    if (parser->error != EXPR_ERROR_NONE || left == NULL)
    {
        return left;
    }

    // 处理逗号分隔的多个表达式
    while (parser->currentToken.type == EXPR_TOKEN_COMMA)
    {
        Expr_NextToken(parser);  // 跳过逗号

        Node_T *right = Expr_ParseCommaExpression(parser);
        if (parser->error != EXPR_ERROR_NONE || right == NULL)
        {
            return left;
        }

        // 创建逗号表达式节点
        Node_T *commaNode = Expr_CreateNode(EXPR_NODE_BINARY_OP);
        if (commaNode == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }

        commaNode->op = EXPR_TOKEN_COMMA;
        commaNode->data.children.left = left;
        commaNode->data.children.right = right;
        commaNode->value = right->value;

        left = commaNode;
    }

    return left;
}

/**
 * @brief 解析赋值语句
 * @param parser 解析器结构体指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseAssignment(Parser_T *parser)
{
    // 检查是否是赋值操作
    Node_T *left = Expr_ParseOr(parser);

    if (parser->error != EXPR_ERROR_NONE || left == NULL)
    {
        return left;
    }

    while (parser->currentToken.type == EXPR_TOKEN_ASSIGN)
    {
        Expr_NextToken(parser);

        // 解析右侧表达式（右结合）
        Node_T *right = Expr_ParseAssignment(parser);
        if (parser->error != EXPR_ERROR_NONE || right == NULL)
        {
            return NULL;
        }

        // 创建赋值节点
        Node_T *node = Expr_CreateNode(EXPR_NODE_ASSIGNMENT);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }

        // 查找变量赋值
        uint8_t varIndex = VARIABLE_TABLE_SIZE;

        if (left->type == EXPR_NODE_VARIABLE)
        {
            varIndex = left->data.variableIndex;
        }
        else
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "赋值语句左边内容必须为变量");
            return NULL;
        }

        if (varIndex >= VARIABLE_TABLE_SIZE)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "变量不存在");
            return NULL;
        }

        // 创建左右子树
        node->data.children.left = left;
        node->data.children.right = right;

        // 同时计算值
        if (right->value.type == EXPR_VALUE_TYPE_NUMBER)
        {
            parser->variables[varIndex].value = right->value.numberValue;
        }
        else if (right->value.type == EXPR_VALUE_TYPE_BOOL)
        {
            parser->variables[varIndex].value = right->value.boolValue;
        }
        else
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "逻辑或运算需要布尔类型的操作数");
            return node;
        }

        node->value.type = EXPR_VALUE_TYPE_NUMBER;
        node->value.numberValue = parser->variables[varIndex].value;

        // 赋值节点的值就是被赋的值
        node->value = right->value;

        return node;
    }

    return left;
}

/**
 * @brief 解析逻辑或表达式
 * @param parser 指向解析器结构体的指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseOr(Parser_T *parser)
{
    Node_T *left = Expr_ParseAnd(parser);

    if (parser->error != EXPR_ERROR_NONE || left == NULL)
    {
        return left;
    }

    while (parser->currentToken.type == EXPR_TOKEN_OR)
    {
        TokenType_T op = parser->currentToken.type;
        Expr_NextToken(parser);

        Node_T *right = Expr_ParseAnd(parser);
        if (parser->error != EXPR_ERROR_NONE || right == NULL)
        {
            return left;
        }

        Node_T *node = Expr_CreateNode(EXPR_NODE_BINARY_OP);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->op = op;
        node->data.children.left = left;
        node->data.children.right = right;

        // 同时计算值
        if (left->value.type == EXPR_VALUE_TYPE_NUMBER && right->value.type == EXPR_VALUE_TYPE_NUMBER)
        {
            uint8_t leftBool = (left->value.numberValue != 0);
            uint8_t rightBool = (right->value.numberValue != 0);
            node->value.type = EXPR_VALUE_TYPE_BOOL;
            node->value.boolValue = leftBool || rightBool;
        }
        else if (left->value.type == EXPR_VALUE_TYPE_BOOL && right->value.type == EXPR_VALUE_TYPE_BOOL)
        {
            node->value.type = EXPR_VALUE_TYPE_BOOL;
            node->value.boolValue = left->value.boolValue || right->value.boolValue;
        }
        else
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "逻辑或运算需要布尔类型的操作数");
            return node;
        }

        left = node;
    }

    return left;
}

/**
 * @brief 解析逻辑与表达式
 * @param parser 指向解析器结构体的指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseAnd(Parser_T *parser)
{
    Node_T *left = Expr_ParseRelation(parser);

    if (parser->error != EXPR_ERROR_NONE || left == NULL)
    {
        return left;
    }

    while (parser->currentToken.type == EXPR_TOKEN_AND)
    {
        TokenType_T op = parser->currentToken.type;
        Expr_NextToken(parser);

        Node_T *right = Expr_ParseRelation(parser);
        if (parser->error != EXPR_ERROR_NONE || right == NULL)
        {
            return left;
        }

        Node_T *node = Expr_CreateNode(EXPR_NODE_BINARY_OP);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->op = op;
        node->data.children.left = left;
        node->data.children.right = right;

        // 同时计算值
        if (left->value.type == EXPR_VALUE_TYPE_NUMBER && right->value.type == EXPR_VALUE_TYPE_NUMBER)
        {
            uint8_t leftBool = (left->value.numberValue != 0);
            uint8_t rightBool = (right->value.numberValue != 0);
            node->value.type = EXPR_VALUE_TYPE_BOOL;
            node->value.boolValue = leftBool && rightBool;
        }
        else if (left->value.type == EXPR_VALUE_TYPE_BOOL && right->value.type == EXPR_VALUE_TYPE_BOOL)
        {
            node->value.type = EXPR_VALUE_TYPE_BOOL;
            node->value.boolValue = left->value.boolValue && right->value.boolValue;
        }
        else
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "逻辑与运算需要布尔类型的操作数");
            return node;
        }

        left = node;
    }

    return left;
}

/**
 * @brief 解析关系表达式
 * @param parser 指向解析器结构体的指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseRelation(Parser_T *parser)
{
    Node_T *left = Expr_ParseAdd(parser);

    if (parser->error != EXPR_ERROR_NONE || left == NULL)
    {
        return left;
    }

    TokenType_T op = parser->currentToken.type;
    if (op == EXPR_TOKEN_GT || op == EXPR_TOKEN_LT || op == EXPR_TOKEN_EQ || op == EXPR_TOKEN_GE || op == EXPR_TOKEN_LE || op == EXPR_TOKEN_NE)
    {
        Expr_NextToken(parser);

        Node_T *right = Expr_ParseAdd(parser);
        if (parser->error != EXPR_ERROR_NONE || right == NULL)
        {
            return left;
        }

        Node_T *node = Expr_CreateNode(EXPR_NODE_BINARY_OP);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->op = op;
        node->data.children.left = left;
        node->data.children.right = right;

        // 同时计算值
        if (left->value.type != EXPR_VALUE_TYPE_NUMBER || right->value.type != EXPR_VALUE_TYPE_NUMBER)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "关系运算需要数值类型的操作数");
            return node;
        }

        float leftVal = left->value.numberValue;
        float rightVal = right->value.numberValue;

        node->value.type = EXPR_VALUE_TYPE_BOOL;

        switch (op)
        {
        case EXPR_TOKEN_GT:
            node->value.boolValue = leftVal > rightVal;
            break;
        case EXPR_TOKEN_LT:
            node->value.boolValue = leftVal < rightVal;
            break;
        case EXPR_TOKEN_EQ:
            node->value.boolValue = fabs(leftVal - rightVal) < EXPR_FLOAT_EPSILON;
            break;
        case EXPR_TOKEN_GE:
            node->value.boolValue = leftVal >= rightVal;
            break;
        case EXPR_TOKEN_LE:
            node->value.boolValue = leftVal <= rightVal;
            break;
        case EXPR_TOKEN_NE:
            node->value.boolValue = fabs(leftVal - rightVal) >= EXPR_FLOAT_EPSILON;
            break;
        default:
            node->value.boolValue = FALSE;
            break;
        }

        return node;
    }

    return left;
}

/**
 * @brief 解析加法/减法表达式
 * @param parser 指向解析器结构体的指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseAdd(Parser_T *parser)
{
    Node_T *left = Expr_ParseMul(parser);

    if (parser->error != EXPR_ERROR_NONE || left == NULL)
    {
        return left;
    }

    while (parser->currentToken.type == EXPR_TOKEN_PLUS || parser->currentToken.type == EXPR_TOKEN_MINUS)
    {
        TokenType_T op = parser->currentToken.type;
        Expr_NextToken(parser);

        Node_T *right = Expr_ParseMul(parser);
        if (parser->error != EXPR_ERROR_NONE || right == NULL)
        {
            return left;
        }

        Node_T *node = Expr_CreateNode(EXPR_NODE_BINARY_OP);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->op = op;
        node->data.children.left = left;
        node->data.children.right = right;

        // 同时计算值
        if (left->value.type != EXPR_VALUE_TYPE_NUMBER || right->value.type != EXPR_VALUE_TYPE_NUMBER)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "算术运算需要数值类型的操作数");
            return node;
        }

        node->value.type = EXPR_VALUE_TYPE_NUMBER;

        if (op == EXPR_TOKEN_PLUS)
        {
            node->value.numberValue = left->value.numberValue + right->value.numberValue;
        }
        else
        {
            node->value.numberValue = left->value.numberValue - right->value.numberValue;
        }

        left = node;
    }

    return left;
}

/**
 * @brief 解析乘法/除法表达式
 * @param parser 指向解析器结构体的指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseMul(Parser_T *parser)
{
    Node_T *left = Expr_ParseExp(parser);

    if (parser->error != EXPR_ERROR_NONE || left == NULL)
    {
        return left;
    }

    while (parser->currentToken.type == EXPR_TOKEN_MUL || parser->currentToken.type == EXPR_TOKEN_DIV || parser->currentToken.type == EXPR_TOKEN_MOD)
    {
        TokenType_T op = parser->currentToken.type;
        Expr_NextToken(parser);

        Node_T *right = Expr_ParseExp(parser);
        if (parser->error != EXPR_ERROR_NONE || right == NULL)
        {
            return left;
        }

        Node_T *node = Expr_CreateNode(EXPR_NODE_BINARY_OP);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->op = op;
        node->data.children.left = left;
        node->data.children.right = right;

        // 同时计算值
        if (left->value.type != EXPR_VALUE_TYPE_NUMBER || right->value.type != EXPR_VALUE_TYPE_NUMBER)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "算术运算需要数值类型的操作数");
            return node;
        }

        node->value.type = EXPR_VALUE_TYPE_NUMBER;

        if (op == EXPR_TOKEN_MUL)
        {
            node->value.numberValue = left->value.numberValue * right->value.numberValue;
        }
        else if (op == EXPR_TOKEN_DIV)
        {
            if (fabs(right->value.numberValue) < EXPR_FLOAT_EPSILON)
            {
                Expr_SetError(parser, EXPR_ERROR_DIV_BY_ZERO, "除以零错误");
                return node;
            }
            node->value.numberValue = left->value.numberValue / right->value.numberValue;
        }
        else
        {
            if (fabs(right->value.numberValue) < EXPR_FLOAT_EPSILON)
            {
                Expr_SetError(parser, EXPR_ERROR_DIV_BY_ZERO, "除以零错误");
                return node;
            }
            node->value.numberValue = fmodf(left->value.numberValue, right->value.numberValue);
        }

        left = node;
    }

    return left;
}

/**
 * @brief 解析乘法/除法表达式
 * @param parser 指向解析器结构体的指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseExp(Parser_T *parser)
{
    Node_T *left = Expr_ParseUnary(parser);

    if (parser->error != EXPR_ERROR_NONE || left == NULL)
    {
        return left;
    }

    while (parser->currentToken.type == EXPR_TOKEN_POW)
    {
        TokenType_T op = parser->currentToken.type;
        Expr_NextToken(parser);

        Node_T *right = Expr_ParseExp(parser);
        if (parser->error != EXPR_ERROR_NONE || right == NULL)
        {
            return left;
        }

        Node_T *node = Expr_CreateNode(EXPR_NODE_BINARY_OP);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->op = op;
        node->data.children.left = left;
        node->data.children.right = right;

        // 同时计算值
        if (left->value.type != EXPR_VALUE_TYPE_NUMBER || right->value.type != EXPR_VALUE_TYPE_NUMBER)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "算术运算需要数值类型的操作数");
            return node;
        }

        node->value.type = EXPR_VALUE_TYPE_NUMBER;
        node->value.numberValue = pow(left->value.numberValue, right->value.numberValue);

        left = node;
    }

    return left;
}

/**
 * @brief 解析一元表达式
 * @param parser 指向解析器结构体的指针
 * @return 语法树节点
 */
static Node_T *Expr_ParseUnary(Parser_T *parser)
{
    if (parser->currentToken.type == EXPR_TOKEN_PLUS || parser->currentToken.type == EXPR_TOKEN_MINUS)
    {
        TokenType_T op = parser->currentToken.type;
        Expr_NextToken(parser);

        Node_T *operand = Expr_ParseUnary(parser);
        if (parser->error != EXPR_ERROR_NONE || operand == NULL)
        {
            return operand;
        }

        Node_T *node = Expr_CreateNode(EXPR_NODE_UNARY_OP);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->op = op;
        node->data.children.left = operand;
        node->data.children.right = NULL;

        // 同时计算值
        if (operand->value.type != EXPR_VALUE_TYPE_NUMBER)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "一元表达式需要数值类型的操作数");
            return node;
        }

        node->value.type = EXPR_VALUE_TYPE_NUMBER;
        node->value.numberValue = (op == EXPR_TOKEN_MINUS) ? -operand->value.numberValue : operand->value.numberValue;

        return node;
    }
    if (parser->currentToken.type == EXPR_TOKEN_NOT)
    {
        Expr_NextToken(parser);

        Node_T *operand = Expr_ParseUnary(parser);
        if (parser->error != EXPR_ERROR_NONE || operand == NULL)
        {
            return operand;
        }

        Node_T *node = Expr_CreateNode(EXPR_NODE_UNARY_OP);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->op = EXPR_TOKEN_NOT;
        node->data.children.left = operand;
        node->data.children.right = NULL;

        // 同时计算值
        uint8_t operandBool;
        if (operand->value.type == EXPR_VALUE_TYPE_BOOL)
        {
            operandBool = operand->value.boolValue;
        }
        else if (operand->value.type == EXPR_VALUE_TYPE_NUMBER)
        {
            operandBool = (operand->value.numberValue != 0);
        }
        else
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "逻辑非运算需要布尔或数值类型的操作数");
            return node;
        }

        node->value.type = EXPR_VALUE_TYPE_BOOL;
        node->value.boolValue = !operandBool;

        return node;
    }

    return Expr_ParsePrimary(parser);
}

/**
 * @brief 计算逗号表达式中的参数个数
 * @param argRoot 逗号表达式根节点
 * @return 参数个数
 */
static uint8_t Expr_CountCommaArguments(Node_T *argRoot)
{
    if (argRoot == NULL)
    {
        return 0;
    }

    uint8_t count = 0;
    Node_T *current = argRoot;

    // 迭代遍历逗号表达式树
    while (current != NULL)
    {
        count++;

        // 如果是逗号表达式节点，继续遍历右子树
        if (current->type == EXPR_NODE_BINARY_OP && current->op == EXPR_TOKEN_COMMA)
        {
            current = current->data.children.right;
        }
        else
        {
            // 不是逗号节点，到达参数链表的末尾
            break;
        }
    }

    return count;
}

/**
 * @brief 获取逗号表达式中的第index个参数
 * @param argRoot 逗号表达式根节点
 * @param index 参数索引（从0开始）
 * @return 参数节点
 */
static Node_T *Expr_GetArgument(Node_T *argRoot, uint8_t index)
{
    if (argRoot == NULL || index >= Expr_CountCommaArguments(argRoot))
    {
        return NULL;
    }

    Node_T *current = argRoot;
    uint8_t currentIndex = 0;

    while (current != NULL)
    {
        if (currentIndex == index)
        {
            // 找到目标参数
            if (current->type == EXPR_NODE_BINARY_OP && current->op == EXPR_TOKEN_COMMA)
            {
                return current->data.children.left;
            }
            return current;
        }

        // 移动到下一个参数
        if (current->type == EXPR_NODE_BINARY_OP && current->op == EXPR_TOKEN_COMMA)
        {
            current = current->data.children.right;
            currentIndex++;
        }
        else
        {
            break;
        }
    }

    return NULL;
}

/**
 * @brief 解析基本表达式（数字、变量、括号）
 * @param parser 指向解析器结构体的指针
 * @return 语法树节点
 */
static Node_T *Expr_ParsePrimary(Parser_T *parser)
{
    Node_T *node = NULL;

    if (parser->currentToken.type == EXPR_TOKEN_NUMBER)
    {
        node = Expr_CreateNode(EXPR_NODE_NUMBER);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->data.numberValue = parser->currentToken.numberValue;
        node->value.type = EXPR_VALUE_TYPE_NUMBER;
        node->value.numberValue = parser->currentToken.numberValue;
        Expr_NextToken(parser);
    }
    else if (parser->currentToken.type == EXPR_TOKEN_VARIABLE)
    {
        if (parser->currentToken.variableIndex >= VARIABLE_TABLE_SIZE)
        {
            Expr_SetError(parser, EXPR_ERROR_VAR_NOT_FOUND, "变量未定义");
            return NULL;
        }
        node = Expr_CreateNode(EXPR_NODE_VARIABLE);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->data.variableIndex = parser->currentToken.variableIndex;
        node->value.type = EXPR_VALUE_TYPE_NUMBER;
        node->value.numberValue = parser->variables[parser->currentToken.variableIndex].value;
        Expr_NextToken(parser);
    }
    else if (parser->currentToken.type == EXPR_TOKEN_CONSTANT)  // 新增：常量解析
    {
        Node_T *node = Expr_CreateNode(EXPR_NODE_CONSTANT);
        if (node != NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }
        node->data.constantIndex = parser->currentToken.variableIndex;
        node->value.type = EXPR_VALUE_TYPE_NUMBER;
        node->value.numberValue = parser->currentToken.numberValue;
        Expr_NextToken(parser);
        return node;
    }
    else if (parser->currentToken.type == EXPR_TOKEN_FUNCTION)
    {
        // 函数调用
        FunctionType_T funcType = parser->currentToken.funcType;
        Expr_NextToken(parser);  // 跳过函数名

        if (parser->currentToken.type != EXPR_TOKEN_LPAREN)
        {
            Expr_SetError(parser, EXPR_ERROR_SYNTAX_ERROR, "函数调用后缺少左括号");
            return NULL;
        }
        Expr_NextToken(parser);  // 跳过左括号

        node = Expr_CreateNode(EXPR_NODE_FUNCTION);
        if (node == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED, "语法树节点池耗尽");
            return NULL;
        }

        // 初始化节点
        node->funcType = funcType;
        node->data.function.argRoot = NULL;
        node->data.function.argCount = 0;

        // 检查是否为空参数列表
        if (parser->currentToken.type == EXPR_TOKEN_RPAREN)
        {
            // 空参数列表
            Expr_NextToken(parser);
        }
        else
        {
            // 解析参数列表（逗号表达式）
            Node_T *argTree = Expr_ParseCommaExpression(parser);
            if (parser->error != EXPR_ERROR_NONE || argTree == NULL)
            {
                return node;
            }

            node->data.function.argRoot = argTree;

            // 计算参数个数
            node->data.function.argCount = Expr_CountCommaArguments(argTree);
        }

        if (parser->currentToken.type != EXPR_TOKEN_RPAREN)
        {
            Expr_SetError(parser, EXPR_ERROR_SYNTAX_ERROR, "函数调用后缺少右括号");
            return node;
        }
        Expr_NextToken(parser);  // 跳过右括号

        // 参数个数检查
        for (uint32_t i = 0; i < FUNCTION_TABLE_SIZE; i++)
        {
            if (g_FunctionTable[i].type == funcType)
            {
                if (node->data.function.argCount < g_FunctionTable[i].minArgs || node->data.function.argCount > g_FunctionTable[i].maxArgs)
                {
                    char errorMsg[64];
                    snprintf(errorMsg, sizeof(errorMsg),
                             "函数%s需要%d-%d个参数，但提供了%d个",
                             g_FunctionTable[i].Name,
                             g_FunctionTable[i].minArgs,
                             g_FunctionTable[i].maxArgs,
                             node->data.function.argCount);
                    Expr_SetError(parser, EXPR_ERROR_SYNTAX_ERROR, errorMsg);
                    return node;
                }
                break;
            }
        }

        // 计算函数值
        if (Expr_CalculateFunctionValue(node, parser) != EXPR_ERROR_NONE)
        {
            return node;
        }

        return node;
    }
    else if (parser->currentToken.type == EXPR_TOKEN_LPAREN)
    {
        Expr_NextToken(parser);
        node = Expr_ParseExpression(parser);
        if (parser->currentToken.type != EXPR_TOKEN_RPAREN)
        {
            Expr_SetError(parser, EXPR_ERROR_SYNTAX_ERROR, "缺少右括号");
            return NULL;
        }
        Expr_NextToken(parser);
    }
    else
    {
        Expr_SetError(parser, EXPR_ERROR_SYNTAX_ERROR, "意外的标记");
        return NULL;
    }

    return node;
}

/**
 * @brief 计算函数节点的值
 * @param node 指向函数节点的指针
 * @param parser 指向解析器结构体的指针
 * @return 错误类型
 */
static ErrorType_T Expr_CalculateFunctionValue(Node_T *node, Parser_T *parser)
{
    if (node->type != EXPR_NODE_FUNCTION)
    {
        return EXPR_ERROR_INVALID_EXPR;
    }

    // 验证参数类型（除了IF函数）
    for (uint32_t i = 0; i < node->data.function.argCount && node->funcType != EXPR_FUNC_IF; i++)
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, i);
        if (arg && arg->value.type != EXPR_VALUE_TYPE_NUMBER)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "函数参数需要数值类型");
            return EXPR_ERROR_INVALID_EXPR;
        }
    }

    node->value.type = EXPR_VALUE_TYPE_NUMBER;

    // 计算函数值
    switch (node->funcType)
    {
    case EXPR_FUNC_SIN:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "sin函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = sin(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_COS:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "cos函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = cos(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_TAN:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "tan函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = tan(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_LN:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "ln函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        if (arg->value.numberValue <= 0)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "ln函数参数必须大于零");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = log(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_EXP:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "exp函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = exp(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_POW:
    {
        Node_T *arg1 = Expr_GetArgument(node->data.function.argRoot, 0);
        Node_T *arg2 = Expr_GetArgument(node->data.function.argRoot, 1);
        if (arg1 == NULL || arg2 == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "pow函数需要两个参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        float base = arg1->value.numberValue;
        float exponent = arg2->value.numberValue;
        node->value.numberValue = pow(base, exponent);
        break;
    }
    case EXPR_FUNC_SQRT:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "sqrt函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        if (arg->value.numberValue < 0)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "sqrt函数参数必须非负");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = sqrt(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_ABS:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "abs函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = fabs(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_MAX:
    {
        if (node->data.function.argCount == 0)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "Max函数至少需要一个参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        else
        {
            Node_T *firstArg = Expr_GetArgument(node->data.function.argRoot, 0);
            node->value.numberValue = firstArg->value.numberValue;

            for (uint32_t i = 1; i < node->data.function.argCount; i++)
            {
                Node_T *arg = Expr_GetArgument(node->data.function.argRoot, i);
                float argValue = arg->value.numberValue;
                if (argValue > node->value.numberValue)
                {
                    node->value.numberValue = argValue;
                }
            }
        }
        break;
    }
    case EXPR_FUNC_MIN:
    {
        if (node->data.function.argCount == 0)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "Min函数至少需要一个参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        else
        {
            Node_T *firstArg = Expr_GetArgument(node->data.function.argRoot, 0);
            node->value.numberValue = firstArg->value.numberValue;

            for (uint32_t i = 1; i < node->data.function.argCount; i++)
            {
                Node_T *arg = Expr_GetArgument(node->data.function.argRoot, i);
                float argValue = arg->value.numberValue;
                if (argValue < node->value.numberValue)
                {
                    node->value.numberValue = argValue;
                }
            }
        }
        break;
    }
    case EXPR_FUNC_ASIN:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "asin函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        float argValue = arg->value.numberValue;
        if (argValue < -1.0 || argValue > 1.0)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "asin函数参数必须在[-1,1]范围内");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = asin(argValue);
        break;
    }
    case EXPR_FUNC_ACOS:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "acos函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        float argValue = arg->value.numberValue;
        if (argValue < -1.0 || argValue > 1.0)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "acos函数参数必须在[-1,1]范围内");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = acos(argValue);
        break;
    }
    case EXPR_FUNC_ATAN:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "atan函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = atan(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_SINH:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "sinh函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = sinh(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_COSH:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "cosh函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = cosh(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_TANH:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "tanh函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = tanh(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_LOG10:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "log10函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        float argValue = arg->value.numberValue;
        if (argValue <= 0)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "log10函数参数必须大于0");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = log10(argValue);
        break;
    }
    case EXPR_FUNC_CEIL:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "ceil函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = ceil(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_FLOOR:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "floor函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = floor(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_ROUND:
    {
        Node_T *arg = Expr_GetArgument(node->data.function.argRoot, 0);
        if (arg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "round函数缺少参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        node->value.numberValue = round(arg->value.numberValue);
        break;
    }
    case EXPR_FUNC_IF:
    {
        // 第一个参数是条件
        Node_T *condition = Expr_GetArgument(node->data.function.argRoot, 0);
        if (condition == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "if函数缺少条件参数");
            return EXPR_ERROR_INVALID_EXPR;
        }

        uint8_t conditionValue;
        if (condition->value.type == EXPR_VALUE_TYPE_BOOL)
        {
            conditionValue = condition->value.boolValue;
        }
        else if (condition->value.type == EXPR_VALUE_TYPE_NUMBER)
        {
            conditionValue = (fabs(condition->value.numberValue) > EXPR_FLOAT_EPSILON);
        }
        else
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "if函数条件参数需要布尔或数值类型");
            return EXPR_ERROR_INVALID_EXPR;
        }

        // 第二个参数（true分支）
        Node_T *trueArg = Expr_GetArgument(node->data.function.argRoot, 1);
        // 第三个参数（false分支）
        Node_T *falseArg = Expr_GetArgument(node->data.function.argRoot, 2);

        if (trueArg == NULL || falseArg == NULL)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "if函数需要三个参数");
            return EXPR_ERROR_INVALID_EXPR;
        }

        if (conditionValue)
        {
            node->value = trueArg->value;
        }
        else
        {
            node->value = falseArg->value;
        }
        break;
    }
    case EXPR_FUNC_SUM:
    {
        if (node->data.function.argCount == 0)
        {
            Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "sum函数需要至少一个参数");
            return EXPR_ERROR_INVALID_EXPR;
        }
        else
        {
            float sum = 0.0;
            for (uint32_t i = 0; i < node->data.function.argCount; i++)
            {
                Node_T *arg = Expr_GetArgument(node->data.function.argRoot, i);
                if (arg->value.type != EXPR_VALUE_TYPE_NUMBER)
                {
                    Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "sum函数参数必须是数值类型");
                    return EXPR_ERROR_INVALID_EXPR;
                }
                sum += arg->value.numberValue;
            }
            node->value.numberValue = sum;
        }
        break;
    }
    default:
    {
        Expr_SetError(parser, EXPR_ERROR_INVALID_EXPR, "未知函数类型");
        return EXPR_ERROR_INVALID_EXPR;
    }
    }

    return EXPR_ERROR_NONE;
}

/**
 * @brief 查找函数类型
 * @param Name 函数名
 * @param length 函数名长度
 * @return 函数类型枚举
 */
static FunctionType_T Expr_FindFunction(const char *Name, uint32_t length)
{
    for (uint32_t i = 0; i < FUNCTION_TABLE_SIZE; i++)
    {
        if (String_GetLength(g_FunctionTable[i].Name, EXPR_MAX_FUNCTION_LENGTH) == length
            && String_Compare(g_FunctionTable[i].Name, Name, length) == 0)
        {
            return g_FunctionTable[i].type;
        }
    }
    return EXPR_FUNC_UNKNOW;  // 默认返回，实际应该处理未找到情况
}

/**
 * @brief 设置解析器错误信息
 * @param parser 指向解析器结构体的指针
 * @param error 错误类型
 * @param msg 错误消息
 */
static void Expr_SetError(Parser_T *parser, ErrorType_T error, const char *msg)
{
    if (parser->error == EXPR_ERROR_NONE)
    {
        parser->error = error;
        snprintf(parser->errorMsg, EXPR_MAX_ERROR_MSG_LENGTH, "错误（位置%d）: %s", parser->currentToken.position, msg);
    }
}
