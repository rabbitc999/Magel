#ifndef __EXPRESSION_H
#define __EXPRESSION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>

#include "bsp_system.h"

#define EXPR_MAX_EXPRESSION_LENGTH    512
#define EXPR_MAX_ERROR_MSG_LENGTH     80
#define EXPR_MAX_VARIABLE_NAME_LENGTH 32
#define EXPR_MAX_OPERATOR_LENGTH      4
#define EXPR_MAX_FUNCTION_LENGTH      8
#define EXPR_FLOAT_EPSILON            1e-3
#define EXPR_MAX_SYNTAX_NODES         128  // 语法树节点池大小

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __ErrorType_T
{
    EXPR_ERROR_NONE = 0,
    EXPR_ERROR_VAR_NOT_FOUND,
    EXPR_ERROR_SYNTAX_ERROR,
    EXPR_ERROR_DIV_BY_ZERO,
    EXPR_ERROR_INVALID_EXPR,
    EXPR_ERROR_EXPR_NODE_POOL_EXHAUSTED
} ErrorType_T;

typedef enum __ValueType_T
{
    EXPR_VALUE_TYPE_BOOL = 0,
    EXPR_VALUE_TYPE_NUMBER,  // 浮点数类型
    EXPR_VALUE_TYPE_UNKNOWN
} ValueType_T;

typedef enum __TokenType_T
{
    EXPR_TOKEN_EOF = 0,   // 结束符号
    EXPR_TOKEN_NUMBER,    // 数字字面量
    EXPR_TOKEN_VARIABLE,  // 变量标识符
    EXPR_TOKEN_FUNCTION,  // 函数标识符
    EXPR_TOKEN_COMMA,     // 参数分隔符
    EXPR_TOKEN_CONSTANT,  // 常量标记
    EXPR_TOKEN_ASSIGN,    /* := */
    EXPR_TOKEN_PLUS,      /* + */
    EXPR_TOKEN_MINUS,     /* - */
    EXPR_TOKEN_MUL,       /* * */
    EXPR_TOKEN_DIV,       /* / */
    EXPR_TOKEN_MOD,       /* % */
    EXPR_TOKEN_POW,       /* ^ */
    EXPR_TOKEN_GT,        /* > */
    EXPR_TOKEN_LT,        /* < */
    EXPR_TOKEN_EQ,        /* = */
    EXPR_TOKEN_GE,        /* >= */
    EXPR_TOKEN_LE,        /* <= */
    EXPR_TOKEN_NE,        /* != */
    EXPR_TOKEN_AND,       /* & */
    EXPR_TOKEN_OR,        /* | */
    EXPR_TOKEN_NOT,       /* ! */
    EXPR_TOKEN_LPAREN,    /* ( */
    EXPR_TOKEN_RPAREN,    /* ) */
} TokenType_T;

// 语法树节点类型
typedef enum __NodeType_T
{
    EXPR_NODE_NUMBER = 0,  // 数字节点
    EXPR_NODE_VARIABLE,    // 变量节点
    EXPR_NODE_UNARY_OP,    // 一元操作符节点
    EXPR_NODE_BINARY_OP,   // 二元操作符节点
    EXPR_NODE_FUNCTION,    // 函数调用节点
    EXPR_NODE_CONSTANT,    // 常量节点
    EXPR_NODE_ASSIGNMENT,  // 赋值节点
} NodeType_T;

// 函数类型枚举
typedef enum __FunctionType_T
{
    EXPR_FUNC_UNKNOW,
    EXPR_FUNC_SIN,
    EXPR_FUNC_COS,
    EXPR_FUNC_TAN,
    EXPR_FUNC_ASIN,
    EXPR_FUNC_ACOS,
    EXPR_FUNC_ATAN,
    EXPR_FUNC_SINH,
    EXPR_FUNC_COSH,
    EXPR_FUNC_TANH,
    EXPR_FUNC_LN,
    EXPR_FUNC_LOG10,
    EXPR_FUNC_EXP,
    EXPR_FUNC_POW,
    EXPR_FUNC_SQRT,
    EXPR_FUNC_ABS,
    EXPR_FUNC_CEIL,
    EXPR_FUNC_FLOOR,
    EXPR_FUNC_ROUND,
    EXPR_FUNC_MAX,
    EXPR_FUNC_MIN,
    EXPR_FUNC_IF,
    EXPR_FUNC_SUM
} FunctionType_T;

typedef struct __Token_T
{
    TokenType_T type;
    float numberValue;
    uint16_t variableIndex;
    int position;
    FunctionType_T funcType;
    int funcNameStart;
    int funcNameLength;
} Token_T;

typedef struct __Value_T
{
    ValueType_T type;
    union
    {
        float numberValue;  // 浮点数值
        uint8_t boolValue;  // 布尔值
    };
} Value_T;

typedef struct __Variable_T
{
    const char *Name;
    float value;
} Variable_T;

typedef struct __Parser_T
{
    const char *input;
    uint16_t position;
    Token_T currentToken;
    ErrorType_T error;
    Variable_T *variables;
    uint16_t variableCount;
    const Variable_T *constants;
    uint16_t constantCount;
    char errorMsg[EXPR_MAX_ERROR_MSG_LENGTH];
} Parser_T;

// 函数信息结构
typedef struct __FunctionInfo_T
{
    const char *Name;
    FunctionType_T type;
    uint8_t minArgs;
    uint8_t maxArgs;
} FunctionInfo_T;

// 语法树节点结构
typedef struct __Node_T
{
    NodeType_T type;
    TokenType_T op;           // 用于运算符节点
    FunctionType_T funcType;  // 用于函数节点
    Value_T value;            // 节点值
    union
    {
        float numberValue;
        uint16_t variableIndex;
        uint16_t constantIndex;
        struct
        {
            struct __Node_T *left;
            struct __Node_T *right;
        } children;
        struct
        {
            struct __Node_T *argRoot;  // 参数链表头节点
            uint8_t argCount;          // 参数个数
        } function;
    } data;
} Node_T;

void Expr_PrintFunctions(char *buffer, uint32_t length);
uint8_t Expr_SetVariable(const char *Name, float value);
void Expr_PrintTree(Node_T *node, int depth, char *buffer, uint32_t length);
void Expr_PrintVariables(char *buffer, uint32_t length);
Node_T *Expr_EvaluateExpression(const char *expr, uint8_t *hasError, char *buffer, uint32_t length);
uint8_t Expr_Calculate(const char *expr, Value_T *result, char *error, uint32_t errorLength);
uint8_t Expr_GetVariable(const char *Name, float *value);
uint16_t Expr_GetVariableNames(const char **names, uint16_t maxCount);
uint16_t Expr_GetVariableCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __FF_MATH_H */
