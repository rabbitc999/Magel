/**
 * @file chali_sql_pg.c
 * @brief libpq ABI 测试夹具；用于验证连接、结果类型和错误恢复，不模拟真实 PostgreSQL 服务。
 */
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define API __declspec(dllexport)
#else
#define API
#endif
/**
 * @brief 夹具连接状态。
 */
typedef struct Connection
{
    int bad;
    int setup_bad;
} Connection;
/**
 * @brief 夹具查询状态。
 */
typedef struct Result
{
    int status;
    int rows;
    int cols;
    const char *affected;
} Result;
static int live_connections, live_results;
/**
 * @brief 创建夹具连接。
 * @param options 夹具控制串。
 * @return 新连接。
 */
API void *PQconnectdb(const char *options)
{
    Connection *c = (Connection *)calloc(1, sizeof(*c));
    c->bad = strcmp(options, "fixture-fail") == 0;
    c->setup_bad = strcmp(options, "fixture-setup-fail") == 0;
    ++live_connections;
    return c;
}
/**
 * @brief 返回连接状态。
 * @param c 连接。
 * @return libpq 状态码。
 */
API int PQstatus(const void *c) { return ((const Connection *)c)->bad; }
/**
 * @brief 释放连接。
 * @param c 连接。
 */
API void PQfinish(void *c)
{
    free(c);
    --live_connections;
}
/**
 * @brief 返回连接错误。
 * @param c 连接。
 * @return 错误文本。
 */
API char *PQerrorMessage(const void *c)
{
    (void)c;
    return "fixture connection failed";
}
/**
 * @brief 验证 UTF-8 编码设置。
 * @param c 连接。
 * @param encoding 编码。
 * @return 成功为零。
 */
API int PQsetClientEncoding(void *c, const char *encoding)
{
    (void)c;
    return strcmp(encoding, "UTF8") != 0;
}
/**
 * @brief 执行受控查询。
 * @param c 连接。
 * @param sql 查询。
 * @return 结果对象。
 */
API void *PQexec(void *c, const char *sql)
{
    Result *r = (Result *)calloc(1, sizeof(*r));
    ++live_results;
    r->status = 1;
    r->affected = "";
    if (strstr(sql, "SET standard_conforming_strings"))
        r->status = ((Connection *)c)->setup_bad ? 7 : 1;
    else if (strcmp(sql, "SELECT fixtures") == 0)
    {
        r->status = 2;
        r->rows = 1;
        r->cols = 6;
        r->affected = "1";
    }
    else if (strcmp(sql, "UPDATE fixtures") == 0)
        r->affected = "7";
    else if (strcmp(sql, "UPDATE huge") == 0)
        r->affected = "5000000000";
    else if (strcmp(sql, "SELECT fail") == 0)
        r->status = 7;
    else if (strcmp(sql, "COPY fixtures") == 0)
        r->status = 3;
    return r;
}
/**
 * @brief 返回查询状态。
 * @param r 结果。
 * @return libpq 状态码。
 */
API int PQresultStatus(const void *r) { return ((const Result *)r)->status; }
/**
 * @brief 返回查询错误。
 * @param r 结果。
 * @return 错误文本。
 */
API char *PQresultErrorMessage(const void *r)
{
    (void)r;
    return "fixture query failed";
}
/**
 * @brief 释放结果。
 * @param r 结果。
 */
API void PQclear(void *r)
{
    free(r);
    --live_results;
}
/**
 * @brief 返回行数。
 * @param r 结果。
 * @return 行数。
 */
API int PQntuples(const void *r) { return ((const Result *)r)->rows; }
/**
 * @brief 返回列数。
 * @param r 结果。
 * @return 列数。
 */
API int PQnfields(const void *r) { return ((const Result *)r)->cols; }
/**
 * @brief 返回列名。
 * @param r 结果。
 * @param col 列号。
 * @return 列名。
 */
API char *PQfname(const void *r, int col)
{
    static char *names[] = { "big", "real", "text", "unicode", "nothing", "boolean" };
    (void)r;
    return names[col];
}
/**
 * @brief 返回列类型 OID。
 * @param r 结果。
 * @param col 列号。
 * @return 类型 OID。
 */
API unsigned int PQftype(const void *r, int col)
{
    static const unsigned int types[] = { 20, 701, 25, 25, 25, 16 };
    (void)r;
    return types[col];
}
/**
 * @brief 判定空值。
 * @param r 结果。
 * @param row 行号。
 * @param col 列号。
 * @return 是否为空。
 */
API int PQgetisnull(const void *r, int row, int col)
{
    (void)r;
    (void)row;
    return col == 4;
}
/**
 * @brief 返回文本值。
 * @param r 结果。
 * @param row 行号。
 * @param col 列号。
 * @return 数据。
 */
API char *PQgetvalue(const void *r, int row, int col)
{
    static char *values[] = { "9223372036854775807", "1.25", "123", "中文", "", "t" };
    (void)r;
    (void)row;
    return values[col];
}
/**
 * @brief 返回文本字节数。
 * @param r 结果。
 * @param row 行号。
 * @param col 列号。
 * @return 字节数。
 */
API int PQgetlength(const void *r, int row, int col) { return (int)strlen(PQgetvalue(r, row, col)); }
/**
 * @brief 返回影响行数。
 * @param r 结果。
 * @return 十进制文本。
 */
API char *PQcmdTuples(void *r) { return (char *)((Result *)r)->affected; }
/**
 * @brief 返回尚未释放的连接数。
 * @return 连接数。
 */
API int fixture_live_connections(void) { return live_connections; }
/**
 * @brief 返回尚未释放的结果数。
 * @return 结果数。
 */
API int fixture_live_results(void) { return live_results; }

/**
 * @brief 返回查询命令标签。
 * @param r 结果。
 * @return 命令标签。
 */
API char *PQcmdStatus(void *r) { return ((Result *)r)->status == 2 ? "SELECT" : *(((Result *)r)->affected) ? "UPDATE" :
                                                                                                             "SET"; }
