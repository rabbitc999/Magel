/**
 * @file magel_sql_mysql.c
 * @brief MySQL/MariaDB 连接 ABI 夹具，不连接真实服务器。
 */
#include <string.h>
#if defined(_WIN32)
#define API __declspec(dllexport)
#else
#define API
#endif
static int connection;
/**
 * @brief 创建夹具连接。
 * @param p 忽略。
 * @return 夹具句柄。
 */
API void *mysql_init(void *p)
{
    (void)p;
    return &connection;
}
/**
 * @brief 检查连接参数与默认端口。
 * @param p 句柄。
 * @param h 主机。
 * @param u 用户。
 * @param pw 密码。
 * @param db 库名。
 * @param port 端口。
 * @param sock 套接字。
 * @param flags 标记。
 * @return 参数正确则返回句柄。
 */
API void *mysql_real_connect(void *p, const char *h, const char *u, const char *pw, const char *db, unsigned int port, const char *sock, unsigned long flags)
{
    (void)sock;
    (void)flags;
    if (!h || !u || !pw || !db || strcmp(u, "user") || strcmp(pw, "password") || strcmp(db, "database")) return 0;
    if ((!strcmp(h, "mysql-default") || !strcmp(h, "maria-default")) && port == 3306) return p;
    if (!strcmp(h, "mysql-explicit") && port == 3307) return p;
    if (!strcmp(h, "maria-explicit") && port == 3308) return p;
    return 0;
}
/**
 * @brief 关闭夹具。
 * @param p 句柄。
 * @return 无。
 */
API void mysql_close(void *p) { (void)p; }
/**
 * @brief 拒绝夹具中的 SQL 执行。
 * @param p 句柄。
 * @param sql SQL。
 * @return 失败。
 */
API int mysql_query(void *p, const char *sql)
{
    (void)p;
    (void)sql;
    return 1;
}
/**
 * @brief 无结果集。
 * @param p 句柄。
 * @return 空。
 */
API void *mysql_store_result(void *p)
{
    (void)p;
    return 0;
}
/**
 * @brief 无结果行。
 * @param p 结果。
 * @return 空。
 */
API char **mysql_fetch_row(void *p)
{
    (void)p;
    return 0;
}
/**
 * @brief 无结果列。
 * @param p 结果。
 * @return 零。
 */
API unsigned int mysql_num_fields(void *p)
{
    (void)p;
    return 0;
}
/**
 * @brief 返回夹具错误。
 * @param p 句柄。
 * @return 错误文本。
 */
API const char *mysql_error(void *p)
{
    (void)p;
    return "fixture connection parameters rejected";
}
/**
 * @brief 接受字符集设置。
 * @param p 句柄。
 * @param name 字符集。
 * @return 成功。
 */
API int mysql_set_character_set(void *p, const char *name)
{
    (void)p;
    (void)name;
    return 0;
}
