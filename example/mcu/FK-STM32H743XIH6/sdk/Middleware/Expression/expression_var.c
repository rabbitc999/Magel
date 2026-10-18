#include "expression.h"

// 常量表
const Variable_T g_ConstantTable[] = {
    { "PI", 3.14159265358979323846 },
    { "E", 2.71828182845904523536 },
};

// 支持的函数表
const FunctionInfo_T g_FunctionTable[] = {
    { "sin", EXPR_FUNC_SIN, 1, 1 },      // 正弦函数
    { "cos", EXPR_FUNC_COS, 1, 1 },      // 余弦函数
    { "tan", EXPR_FUNC_TAN, 1, 1 },      // 正切函数
    { "asin", EXPR_FUNC_ASIN, 1, 1 },    // 反正弦函数
    { "acos", EXPR_FUNC_ACOS, 1, 1 },    // 反余弦函数
    { "atan", EXPR_FUNC_ATAN, 1, 1 },    // 反正切函数
    { "sinh", EXPR_FUNC_SINH, 1, 1 },    // 双曲正弦函数
    { "cosh", EXPR_FUNC_COSH, 1, 1 },    // 双曲余弦函数
    { "tanh", EXPR_FUNC_TANH, 1, 1 },    // 双曲正切函数
    { "ln", EXPR_FUNC_LN, 1, 1 },        // 自然对数函数
    { "log", EXPR_FUNC_LN, 1, 1 },       // 自然对数函数别名
    { "log10", EXPR_FUNC_LOG10, 1, 1 },  // 以10为底的对数函数
    { "exp", EXPR_FUNC_EXP, 1, 1 },      // 指数函数
    { "pow", EXPR_FUNC_POW, 2, 2 },      // 幂函数
    { "sqrt", EXPR_FUNC_SQRT, 1, 1 },    // 平方根函数
    { "abs", EXPR_FUNC_ABS, 1, 1 },      // 绝对值函数
    { "ceil", EXPR_FUNC_CEIL, 1, 1 },    // 向上取整函数
    { "floor", EXPR_FUNC_FLOOR, 1, 1 },  // 向下取整函数
    { "round", EXPR_FUNC_ROUND, 1, 1 },  // 四舍五入函数
    { "max", EXPR_FUNC_MAX, 1, 255 },    // 最大值函数
    { "min", EXPR_FUNC_MIN, 1, 255 },    // 最小值函数
    { "if", EXPR_FUNC_IF, 3, 3 },        // 条件函数
    { "sum", EXPR_FUNC_SUM, 1, 255 },    // 求和函数
};

// 变量表
Variable_T g_VariableTable[] = {
    { "a", 0.0 },
    { "b", 0.0 },
    { "c", 0.0 },
    { "d", 0.0 },
    { "t", 0.0 },
    { "u", 0.0 },
    { "v", 0.0 },
    { "x", 0.0 },
    { "y", 0.0 },
    { "z", 0.0 },
    { "dx", 0.0 },
    { "dy", 0.0 },
    { "dz", 0.0 },
    { "dw", 0.0 },
    { "dt", 0.0 },
    { "sx", 0.0 },
    { "sy", 0.0 },
    { "sz", 0.0 },
    { "sw", 0.0 },
    { "wx", 0.0 },
    { "wy", 0.0 },
    { "wz", 0.0 },
    { "pitch", 0.0 },
    { "roll", 0.0 },
    { "yaw", 0.0 },
    { "max_x", 0.0 },
    { "max_w", 0.0 },
    { "min_x", 0.0 },
    { "min_w", 0.0 },
    { "t_dyaw", 0.0 },
    { "stop_flag", 0.0 },
    { "beep_flag", 0.0 },
};

/* 变量表、常量表和函数表的大小 */
const uint32_t VARIABLE_TABLE_SIZE = sizeof(g_VariableTable) / sizeof(g_VariableTable[0]);
const uint32_t CONSTANT_TABLE_SIZE = sizeof(g_ConstantTable) / sizeof(g_ConstantTable[0]);
const uint32_t FUNCTION_TABLE_SIZE = sizeof(g_FunctionTable) / sizeof(g_FunctionTable[0]);
