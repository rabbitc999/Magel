#ifndef __EXPRESSION_PARSER_H
#define __EXPRESSION_PARSER_H

#include "expression.h"
#include "expression_var.h"

static void Expr_NextToken(Parser_T *parser);
static Node_T *Expr_ParseOr(Parser_T *parser);
static Node_T *Expr_ParseAnd(Parser_T *parser);
static Node_T *Expr_ParseRelation(Parser_T *parser);
static Node_T *Expr_ParseAdd(Parser_T *parser);
static Node_T *Expr_ParseMul(Parser_T *parser);
static Node_T *Expr_ParseUnary(Parser_T *parser);
static Node_T *Expr_ParsePrimary(Parser_T *parser);
static Node_T *Expr_CreateNode(NodeType_T type);
static Node_T *Expr_ParseAssignment(Parser_T *parser);
static ErrorType_T Expr_CalculateFunctionValue(Node_T *node, Parser_T *parser);
static Node_T *Expr_ParseCommaExpression(Parser_T *parser);
static Node_T *Expr_ParseExp(Parser_T *parser);
static void Expr_SetError(Parser_T *parser, ErrorType_T error, const char *msg);
static FunctionType_T Expr_FindFunction(const char *Name, uint32_t length);

#endif /* __EXPRESSION_VAR_H */