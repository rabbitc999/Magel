#include "std_sql.hpp"
#include "std_native_record.hpp"
#include "std_sql_ipage.hpp"
#include "../../source/json/json.hpp"

#include "../chali_glue.hpp"
#include "../../library/sqlite/sqlite3.h"

#include <cerrno>
#include <cmath>
#include <limits>
#include <memory>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/*
 * `std::sql` 的宿主胶水：让 CHALI 语言连接**外部 SQL 数据库**（MySQL / SQLite / PostgreSQL / SQL Server ODBC）。
 *
 * SQL 的解析、执行、事务、索引全部在外部数据库里完成，CHALI 只做两件事：把语句发过去、
 * 把结果搬回来。MySQL 的 C API 全是指针（`MYSQL *`、`MYSQL_RES *`、`MYSQL_ROW`、
 * `MYSQL_FIELD *`），CHALI 一侧不能持有这类指针，所以这里把指针收进句柄表，对外只暴露
 * 小整数句柄 + 标量：
 *
 *   __sqlConnect / __sqlClose / __sqlExec / __sqlAffected / __sqlError
 *   __sqlRowCount / __sqlColCount / __sqlColName
 *   __sqlCellText / __sqlCellInt / __sqlCellReal / __sqlCellKind / __sqlFree
 *
 * SQLite 静态内置，不读取 SQLITE_LIBRARY；libpq / ODBC 可分别通过
 * POSTGRES_LIBRARY / ODBC_LIBRARY 指定动态库。MySQL 查找顺序：
 *   1. 环境变量 `MYSQL_LIBRARY`（指向 libmysqlclient/libmysql 的完整路径）；
 *   2. Windows：`C:\Program Files\MySQL\MySQL Server <版本>\lib\libmysql.dll`
 *      （同时把同目录树的 `bin` 加进 DLL 搜索路径，OpenSSL 依赖在那里）；
 *   3. 系统搜索路径里的 `libmysql.dll` / `libmysqlclient.dll` / `libmariadb.dll`；
 *   4. 其它平台：`libmysqlclient.so.21` / `.so` / `libmariadb.so` / `.dylib`。
 */
namespace std_sql_glue
{

using chali_glue::Registry;
using Memory::String;

/* ── MySQL 运行期函数表 ───────────────────────────────────────────────── */

using InitFn = void *(*)(void *);
using RealConnectFn = void *(*)(void *, const char *, const char *, const char *, const char *,
                                unsigned int, const char *, unsigned long);
using CloseFn = void (*)(void *);
using QueryFn = int (*)(void *, const char *);
using StoreResultFn = void *(*)(void *);
using FreeResultFn = void (*)(void *);
using NumFieldsFn = unsigned int (*)(void *);
using NumRowsFn = unsigned long long (*)(void *);
using FieldCountFn = unsigned int (*)(void *);
using FetchRowFn = char **(*)(void *);
using FetchFieldFn = void *(*)(void *);
using ErrorFn = const char *(*)(void *);
using AffectedRowsFn = unsigned long long (*)(void *);
using SetCharsetFn = int (*)(void *, const char *);
using MoreResultsFn = int (*)(void *);
using NextResultFn = int (*)(void *);

/**
 * @brief MySQL 客户端公开 MYSQL_FIELD 的固定 ABI 前缀，用于读取列名与服务器类型。
 */
struct MysqlFieldPrefix
{
    char *name, *originalName, *table, *originalTable, *database, *catalog, *defaultValue;
    unsigned long length, maximumLength;
    unsigned int nameLength, originalNameLength, tableLength, originalTableLength;
    unsigned int databaseLength, catalogLength, defaultLength, flags, decimals, charset;
    int type;
};

struct MysqlApi
{
    InitFn init = nullptr;
    RealConnectFn real_connect = nullptr;
    CloseFn close = nullptr;
    QueryFn query = nullptr;
    StoreResultFn store_result = nullptr;
    FreeResultFn free_result = nullptr;
    NumFieldsFn num_fields = nullptr;
    NumRowsFn num_rows = nullptr;
    FieldCountFn field_count = nullptr;
    FetchRowFn fetch_row = nullptr;
    FetchFieldFn fetch_field = nullptr;
    ErrorFn error = nullptr;
    AffectedRowsFn affected_rows = nullptr;
    SetCharsetFn set_charset = nullptr;
    MoreResultsFn more_results = nullptr;
    NextResultFn next_result = nullptr;
};

/**
 * @brief 取得 MySQL 驱动函数入口表。
 * @return 进程内持久保存的驱动函数表。
 */
inline MysqlApi &api()
{
    static MysqlApi instance;
    return instance;
}

/**
 * @brief 取得 MySQL 驱动初始化标志。
 * @return 可读写的初始化状态引用。
 */
inline bool &apiReady()
{
    static bool instance = false;
    return instance;
}

/*
 * 常驻的错误文本一律放在**定长 C 缓冲**里，不用 Memory::String：宿主侧的内存
 * 分配器在进程退出时会核对活动块，长期挂着的 Memory::String 会被算成泄漏。
 * 只有返回值（VM 会立刻复制走）才临时构造 Memory::String。
 */
constexpr std::size_t kErrorTextSize = 512;

/**
 * @brief 取得数据库驱动加载失败的诊断缓冲。
 * @return 进程内持久保存的可写诊断缓冲。
 */
inline char *loadErrorText()
{
    static char buffer[kErrorTextSize] = { 0 };
    return buffer;
}

/**
 * @brief 取得最近一次数据库操作的诊断缓冲。
 * @return 进程内持久保存的可写诊断缓冲。
 */
inline char *lastErrorText()
{
    static char buffer[kErrorTextSize] = { 0 };
    return buffer;
}

/**
 * @brief 在固定诊断缓冲内保存并终止错误文本。
 * @param buffer 接收字节或诊断文本的输出缓冲。
 * @param message 要保存到错误缓冲的诊断文本。
 */
inline void setErrorText(char *buffer, const char *message)
{
    std::snprintf(buffer, kErrorTextSize, "%s", message != nullptr ? message : "");
}

/**
 * @brief 按当前平台的动态加载接口打开数据库驱动。
 * @param name 需要绑定或解析的入口名称。
 * @return 库句柄；加载失败返回空。
 */
inline void *openLibrary(const char *name)
{
#if defined(_WIN32)
    // 绝对路径的客户端依赖库通常与主 DLL 同目录（例如 PostgreSQL 的 OpenSSL）。
    const DWORD flags = std::filesystem::path(name).is_absolute() ? LOAD_WITH_ALTERED_SEARCH_PATH : 0;
    return reinterpret_cast<void *>(::LoadLibraryExA(name, nullptr, flags));
#else
    return ::dlopen(name, RTLD_NOW | RTLD_LOCAL);
#endif
}

/**
 * @brief 从动态数据库驱动中解析一个入口地址。
 * @param library 已经成功加载的数据库驱动句柄。
 * @param name 需要绑定或解析的入口名称。
 * @return 入口地址；符号不存在时返回空。
 */
inline void *librarySymbol(void *library, const char *name)
{
#if defined(_WIN32)
    return reinterpret_cast<void *>(::GetProcAddress(reinterpret_cast<HMODULE>(library), name));
#else
    return ::dlsym(library, name);
#endif
}

/* 加载一个候选库；Windows 上先把客户端库旁边的 bin 目录加进搜索路径（OpenSSL 依赖在那）。 */
inline void *tryLoad(const std::string &path)
{
    std::error_code code;
    if (!path.empty() && std::filesystem::exists(path, code))
    {
#if defined(_WIN32)
        const std::filesystem::path parent = std::filesystem::path(path).parent_path().parent_path();
        const std::string binary = (parent / "bin").string();
        if (std::filesystem::exists(binary, code)) ::SetDllDirectoryA(binary.c_str());
#endif
        if (void *handle = openLibrary(path.c_str())) return handle;
#if defined(_WIN32)
        ::SetDllDirectoryA(nullptr);
#endif
        return nullptr;
    }
    return openLibrary(path.c_str());
}

/**
 * @brief 检查 MySQL 驱动表是否缺少必需入口。
 * @param table 需要检查的数据库驱动函数表。
 * @return 至少一个必需入口缺失时为 true。
 */
inline bool missing(const MysqlApi &table)
{
    return table.init == nullptr || table.real_connect == nullptr || table.query == nullptr || table.store_result == nullptr || table.fetch_row == nullptr || table.num_fields == nullptr || table.error == nullptr;
}

/**
 * @brief 从已打开的 MySQL 驱动解析并保存函数入口。
 * @param library 已经成功加载的数据库驱动句柄。
 */
inline void loadSymbols(void *library)
{
    MysqlApi &table = api();
    table.init = reinterpret_cast<InitFn>(librarySymbol(library, "mysql_init"));
    table.real_connect = reinterpret_cast<RealConnectFn>(librarySymbol(library, "mysql_real_connect"));
    table.close = reinterpret_cast<CloseFn>(librarySymbol(library, "mysql_close"));
    table.query = reinterpret_cast<QueryFn>(librarySymbol(library, "mysql_query"));
    table.store_result = reinterpret_cast<StoreResultFn>(librarySymbol(library, "mysql_store_result"));
    table.free_result = reinterpret_cast<FreeResultFn>(librarySymbol(library, "mysql_free_result"));
    table.num_fields = reinterpret_cast<NumFieldsFn>(librarySymbol(library, "mysql_num_fields"));
    table.num_rows = reinterpret_cast<NumRowsFn>(librarySymbol(library, "mysql_num_rows"));
    table.field_count = reinterpret_cast<FieldCountFn>(librarySymbol(library, "mysql_field_count"));
    table.fetch_row = reinterpret_cast<FetchRowFn>(librarySymbol(library, "mysql_fetch_row"));
    table.fetch_field = reinterpret_cast<FetchFieldFn>(librarySymbol(library, "mysql_fetch_field"));
    table.error = reinterpret_cast<ErrorFn>(librarySymbol(library, "mysql_error"));
    table.affected_rows = reinterpret_cast<AffectedRowsFn>(librarySymbol(library, "mysql_affected_rows"));
    table.set_charset = reinterpret_cast<SetCharsetFn>(librarySymbol(library, "mysql_set_character_set"));
    table.more_results = reinterpret_cast<MoreResultsFn>(librarySymbol(library, "mysql_more_results"));
    table.next_result = reinterpret_cast<NextResultFn>(librarySymbol(library, "mysql_next_result"));
}

/* ── 客户端库定位 ─────────────────────────────────────────────────────── */

inline void appendCandidates(std::vector<std::string> &out)
{
#if defined(_WIN32)
    static const char *versions[] = { "9.4", "9.3", "9.2", "9.1", "9.0", "8.4", "8.3", "8.2",
                                      "8.1", "8.0", "5.7" };
    static const char *roots[] = { "C:/Program Files/MySQL/MySQL Server ",
                                   "C:/Program Files (x86)/MySQL/MySQL Server " };
    for (const char *root : roots)
        for (const char *version : versions)
            out.push_back(std::string(root) + version + "/lib/libmysql.dll");
    out.push_back("libmysql.dll");
    out.push_back("libmysqlclient.dll");
    out.push_back("libmariadb.dll");
#else
    out.push_back("libmysqlclient.so.21");
    out.push_back("libmysqlclient.so");
    out.push_back("libmysqlclient.dylib");
    out.push_back("libmariadb.so");
    out.push_back("libmariadb.so.3");
#endif
}

/**
 * @brief 按候选路径加载并校验 MySQL 驱动入口。
 * @return 驱动可用时为 true；失败时保留加载诊断。
 */
inline bool initMysql()
{
    if (apiReady()) return true;
    std::vector<std::string> candidates;
    const char *overridePath = std::getenv("MYSQL_LIBRARY");
    if (overridePath != nullptr && overridePath[0] != '\0') candidates.push_back(overridePath);
    appendCandidates(candidates);
    void *library = nullptr;
    for (std::size_t i = 0; library == nullptr && i < candidates.size(); ++i)
        library = tryLoad(candidates[i]);
    if (library == nullptr)
    {
        setErrorText(loadErrorText(), "找不到 MySQL 客户端库（可用环境变量 MYSQL_LIBRARY 指定路径）");
        return false;
    }
    loadSymbols(library);
    if (missing(api()))
    {
        setErrorText(loadErrorText(), "MySQL 客户端库缺少需要的符号");
        return false;
    }
    apiReady() = true;
    return true;
}

/**
 * @brief SQLite 句柄适配函数表，实际 API 静态链接到内置引擎。
 */
struct SqliteApi
{
    int (*initialize)() = nullptr;
    int (*open)(const char *, void **) = nullptr;
    int (*close)(void *) = nullptr;
    const char *(*error)(void *) = nullptr;
    int (*prepare)(void *, const char *, int, void **, const char **) = nullptr;
    int (*step)(void *) = nullptr;
    int (*finalize)(void *) = nullptr;
    int (*columns)(void *) = nullptr;
    const char *(*columnName)(void *, int) = nullptr;
    int (*columnType)(void *, int) = nullptr;
    long long (*columnInt)(void *, int) = nullptr;
    double (*columnReal)(void *, int) = nullptr;
    const unsigned char *(*columnText)(void *, int) = nullptr;
    int (*columnBytes)(void *, int) = nullptr;
    int (*changes)(void *) = nullptr;
    int (*readonly)(void *) = nullptr;
    int (*totalChanges)(void *) = nullptr;
};

/**
 * @brief 返回进程共用的 SQLite 函数表。
 * @return 函数表引用。
 */
inline SqliteApi &sqliteApi()
{
    static SqliteApi instance;
    return instance;
}

/**
 * @brief 初始化静态内置的 SQLite 引擎并绑定类型安全适配函数。
 * @return 初始化是否成功。
 */
inline bool initSqlite()
{
    static const bool ready = [] {
        SqliteApi table;
        table.initialize = sqlite3_initialize;
        table.open = [](const char *path, void **out) { sqlite3 *db=nullptr; int rc=sqlite3_open(path,&db); *out=db; return rc; };
        table.close = [](void *db) { return sqlite3_close(static_cast<sqlite3 *>(db)); };
        table.error = [](void *db) { return sqlite3_errmsg(static_cast<sqlite3 *>(db)); };
        table.prepare = [](void *db, const char *sql, int size, void **out, const char **tail) {
            sqlite3_stmt *statement = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3 *>(db), sql, size, &statement, tail);
            *out = statement;
            return rc;
        };
        table.step = [](void *p) { return sqlite3_step(static_cast<sqlite3_stmt *>(p)); };
        table.finalize = [](void *p) { return sqlite3_finalize(static_cast<sqlite3_stmt *>(p)); };
        table.columns = [](void *p) { return sqlite3_column_count(static_cast<sqlite3_stmt *>(p)); };
        table.columnName = [](void *p, int i) { return sqlite3_column_name(static_cast<sqlite3_stmt *>(p), i); };
        table.columnType = [](void *p, int i) { return sqlite3_column_type(static_cast<sqlite3_stmt *>(p), i); };
        table.columnInt = [](void *p, int i) -> long long { return sqlite3_column_int64(static_cast<sqlite3_stmt *>(p), i); };
        table.columnReal = [](void *p, int i) { return sqlite3_column_double(static_cast<sqlite3_stmt *>(p), i); };
        table.columnText = [](void *p, int i) { return sqlite3_column_text(static_cast<sqlite3_stmt *>(p), i); };
        table.columnBytes = [](void *p, int i) { return sqlite3_column_bytes(static_cast<sqlite3_stmt *>(p), i); };
        table.changes = [](void *db) { return sqlite3_changes(static_cast<sqlite3 *>(db)); };
        table.readonly = [](void *p) { return sqlite3_stmt_readonly(static_cast<sqlite3_stmt *>(p)); };
        table.totalChanges = [](void *db) { return sqlite3_total_changes(static_cast<sqlite3 *>(db)); };
        sqliteApi() = table;
        return table.initialize() == SQLITE_OK;
    }();
    if (!ready) setErrorText(lastErrorText(), "SQLite 引擎初始化失败");
    return ready;
}

/**
 * @brief PostgreSQL libpq 运行期函数表，使用文本结果并保留服务端列类型。
 */
struct PostgresApi
{
    void *(*connect)(const char *) = nullptr;
    int (*status)(const void *) = nullptr;
    void (*finish)(void *) = nullptr;
    char *(*error)(const void *) = nullptr;
    int (*encoding)(void *, const char *) = nullptr;
    void *(*exec)(void *, const char *) = nullptr;
    int (*resultStatus)(const void *) = nullptr;
    char *(*resultError)(const void *) = nullptr;
    void (*clear)(void *) = nullptr;
    int (*rows)(const void *) = nullptr;
    int (*columns)(const void *) = nullptr;
    char *(*columnName)(const void *, int) = nullptr;
    unsigned int (*columnType)(const void *, int) = nullptr;
    int (*isNull)(const void *, int, int) = nullptr;
    char *(*value)(const void *, int, int) = nullptr;
    int (*length)(const void *, int, int) = nullptr;
    char *(*affected)(void *) = nullptr;
    char *(*command)(void *) = nullptr;
};

/**
 * @brief 取得 PostgreSQL 函数表。
 * @return 进程共用的表。
 */
inline PostgresApi &postgresApi()
{
    static PostgresApi instance;
    return instance;
}

/**
 * @brief 关闭未使用的候选动态库。
 * @param library 动态库句柄。
 */
inline void closeLibrary(void *library)
{
#if defined(_WIN32)
    ::FreeLibrary(reinterpret_cast<HMODULE>(library));
#else
    ::dlclose(library);
#endif
}

/**
 * @brief 加载 libpq。
 * @return 成功为真，失败诊断写入全局错误文本。
 */
inline bool initPostgres()
{
    static bool ready = false;
    if (ready) return true;
    std::vector<std::string> candidates;
    if (const char *path = std::getenv("POSTGRES_LIBRARY"))
        if (*path) candidates.emplace_back(path);
#if defined(_WIN32)
    candidates.emplace_back("libpq.dll");
#else
    candidates.emplace_back("libpq.so.5");
    candidates.emplace_back("libpq.so");
    candidates.emplace_back("libpq.dylib");
#endif
    for (const auto &path : candidates)
    {
        void *library = openLibrary(path.c_str());
        if (!library) continue;
        PostgresApi table;
#define CHALI_PQ_LOAD(member, symbol) table.member = reinterpret_cast<decltype(table.member)>(librarySymbol(library, symbol))
        CHALI_PQ_LOAD(connect, "PQconnectdb");
        CHALI_PQ_LOAD(status, "PQstatus");
        CHALI_PQ_LOAD(finish, "PQfinish");
        CHALI_PQ_LOAD(error, "PQerrorMessage");
        CHALI_PQ_LOAD(encoding, "PQsetClientEncoding");
        CHALI_PQ_LOAD(exec, "PQexec");
        CHALI_PQ_LOAD(resultStatus, "PQresultStatus");
        CHALI_PQ_LOAD(resultError, "PQresultErrorMessage");
        CHALI_PQ_LOAD(clear, "PQclear");
        CHALI_PQ_LOAD(rows, "PQntuples");
        CHALI_PQ_LOAD(columns, "PQnfields");
        CHALI_PQ_LOAD(columnName, "PQfname");
        CHALI_PQ_LOAD(columnType, "PQftype");
        CHALI_PQ_LOAD(isNull, "PQgetisnull");
        CHALI_PQ_LOAD(value, "PQgetvalue");
        CHALI_PQ_LOAD(length, "PQgetlength");
        CHALI_PQ_LOAD(affected, "PQcmdTuples");
        CHALI_PQ_LOAD(command, "PQcmdStatus");
#undef CHALI_PQ_LOAD
        if (table.connect && table.status && table.finish && table.error && table.encoding && table.exec && table.resultStatus && table.resultError && table.clear && table.rows && table.columns && table.columnName && table.columnType && table.isNull && table.value && table.length && table.affected && table.command)
        {
            postgresApi() = table;
            ready = true;
            return true;
        }
        closeLibrary(library);
    }
    setErrorText(lastErrorText(), "找不到可用的 PostgreSQL libpq（可用 POSTGRES_LIBRARY 指定路径）");
    return false;
}

#if defined(_WIN32)
#define CHALI_SQL_ODBC_CALL __stdcall
#else
#define CHALI_SQL_ODBC_CALL
#endif

/**
 * @brief ODBC 3 Unicode 函数表；长度类型使用指针宽度以匹配 32/64 位 ABI。
 */
struct OdbcApi
{
    std::int16_t(CHALI_SQL_ODBC_CALL *allocate)(std::int16_t, void *, void **) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *freeHandle)(std::int16_t, void *) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *environment)(void *, std::int32_t, void *, std::int32_t) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *connectAttribute)(void *, std::int32_t, void *, std::int32_t) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *connect)(void *, void *, std::uint16_t *, std::int16_t,
                                               std::uint16_t *, std::int16_t, std::int16_t *, std::uint16_t) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *disconnect)(void *) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *exec)(void *, std::uint16_t *, std::int32_t) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *columns)(void *, std::int16_t *) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *describe)(void *, std::uint16_t, std::uint16_t *, std::int16_t,
                                                std::int16_t *, std::int16_t *, std::uintptr_t *, std::int16_t *, std::int16_t *) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *fetch)(void *) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *get)(void *, std::uint16_t, std::int16_t, void *, std::intptr_t, std::intptr_t *) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *rowCount)(void *, std::intptr_t *) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *more)(void *) = nullptr;
    std::int16_t(CHALI_SQL_ODBC_CALL *diagnostic)(std::int16_t, void *, std::int16_t,
                                                  std::uint16_t *, std::int32_t *, std::uint16_t *, std::int16_t, std::int16_t *) = nullptr;
};
#undef CHALI_SQL_ODBC_CALL

/**
 * @brief 返回 ODBC 函数表。
 * @return 进程共用的函数表。
 */
inline OdbcApi &odbcApi()
{
    static OdbcApi instance;
    return instance;
}

/**
 * @brief 判断 ODBC 返回码。
 * @param status 返回码。
 * @return 成功或成功并附带信息时为真。
 */
inline bool odbcSuccess(std::int16_t status) { return status == 0 || status == 1; }

/**
 * @brief 将 UTF-8 文本转换为 ODBC UTF-16。
 * @param text UTF-8 文本。
 * @return UTF-16 编码。
 */
inline std::u16string sqlUtf16(const std::string &text)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.from_bytes(text);
}

/**
 * @brief 将 ODBC UTF-16 转为 UTF-8。
 * @param text UTF-16 文本。
 * @return UTF-8 编码。
 */
inline std::string sqlUtf8(const std::u16string &text)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.to_bytes(text);
}

/**
 * @brief 复制 ODBC 首条诊断。
 * @param type 句柄类型。
 * @param handle 出错句柄。
 * @param output 错误缓冲区。
 */
inline void odbcError(std::int16_t type, void *handle, char *output)
{
    std::uint16_t state[6] = {}, message[512] = {};
    std::int32_t native = 0;
    std::int16_t length = 0;
    if (odbcSuccess(odbcApi().diagnostic(type, handle, 1, state, &native, message, 512, &length)))
    {
        const std::size_t size = static_cast<std::size_t>(std::max<std::int16_t>(0, std::min<std::int16_t>(length, 511)));
        std::u16string text;
        for (std::size_t i = 0; i < size; ++i) text.push_back(static_cast<char16_t>(message[i]));
        setErrorText(output, sqlUtf8(text).c_str());
    }
    else
        setErrorText(output, "ODBC 操作失败，驱动未提供诊断");
}

/**
 * @brief 加载系统 ODBC 驱动管理器。
 * @return 是否成功。
 */
inline bool initOdbc()
{
    static bool ready = false;
    if (ready) return true;
    std::vector<std::string> candidates;
    if (const char *path = std::getenv("ODBC_LIBRARY"))
        if (*path) candidates.emplace_back(path);
#if defined(_WIN32)
    candidates.emplace_back("odbc32.dll");
#else
    candidates.emplace_back("libodbc.so.2");
    candidates.emplace_back("libodbc.so");
    candidates.emplace_back("libodbc.dylib");
#endif
    for (const auto &path : candidates)
    {
        void *library = openLibrary(path.c_str());
        if (!library) continue;
        OdbcApi table;
#define CHALI_ODBC_LOAD(member, symbol) table.member = reinterpret_cast<decltype(table.member)>(librarySymbol(library, symbol))
        CHALI_ODBC_LOAD(allocate, "SQLAllocHandle");
        CHALI_ODBC_LOAD(freeHandle, "SQLFreeHandle");
        CHALI_ODBC_LOAD(environment, "SQLSetEnvAttr");
        CHALI_ODBC_LOAD(connectAttribute, "SQLSetConnectAttrW");
        CHALI_ODBC_LOAD(connect, "SQLDriverConnectW");
        CHALI_ODBC_LOAD(disconnect, "SQLDisconnect");
        CHALI_ODBC_LOAD(exec, "SQLExecDirectW");
        CHALI_ODBC_LOAD(columns, "SQLNumResultCols");
        CHALI_ODBC_LOAD(describe, "SQLDescribeColW");
        CHALI_ODBC_LOAD(fetch, "SQLFetch");
        CHALI_ODBC_LOAD(get, "SQLGetData");
        CHALI_ODBC_LOAD(rowCount, "SQLRowCount");
        CHALI_ODBC_LOAD(more, "SQLMoreResults");
        CHALI_ODBC_LOAD(diagnostic, "SQLGetDiagRecW");
#undef CHALI_ODBC_LOAD
        if (table.allocate && table.freeHandle && table.environment && table.connect && table.disconnect && table.exec && table.columns && table.describe && table.fetch && table.get && table.rowCount && table.more && table.diagnostic)
        {
            odbcApi() = table;
            ready = true;
            return true;
        }
        closeLibrary(library);
    }
    setErrorText(lastErrorText(), "找不到可用的 ODBC 驱动管理器（可用 ODBC_LIBRARY 指定路径）");
    return false;
}

/* ── 句柄表 ───────────────────────────────────────────────────────────── */

constexpr int kMaxDatabases = 32;
constexpr int kMaxResults = 256;
constexpr unsigned long kClientMultiStatements = 65536UL; /* CLIENT_MULTI_STATEMENTS */

/**
 * @brief SQL 连接的数据库后端，集中区分各引擎的执行与关闭规则。
 */
enum class SqlBackend
{
    MySQL,
    SQLite,
    PostgreSQL,
    Odbc
};

struct DatabaseSlot
{
    bool used = false;
    void *handle = nullptr;
    void *environment = nullptr;
    SqlBackend backend = SqlBackend::MySQL;
    std::int64_t affected = 0;
    char error[kErrorTextSize] = { 0 };
};

struct Cell
{
    int kind = 0; /* 0 空值、1 整数、2 实数、3 文本 */
    long long intValue = 0;
    double realValue = 0.0;
    std::string text;
};

struct ResultSlot
{
    bool used = false;
    int rows = 0;
    int cols = 0;
    unsigned long long affected = 0;
    std::vector<std::string> colNames;
    std::vector<Cell> cells;
};

/**
 * @brief 取得 SQL 胶水拥有的数据库连接槽表。
 * @return 进程内持久保存的连接槽数组。
 */
inline std::vector<DatabaseSlot> &databases()
{
    static std::vector<DatabaseSlot> instance(kMaxDatabases);
    return instance;
}

/**
 * @brief 取得 SQL 胶水拥有的结果集槽表。
 * @return 进程内持久保存的结果槽数组。
 */
inline std::vector<ResultSlot> &results()
{
    static std::vector<ResultSlot> instance(kMaxResults);
    return instance;
}

/**
 * @brief 校验数据库句柄并读取已占用的连接槽。
 * @param index 从零开始的数据库资源槽编号。
 * @return 有效连接槽；句柄越界或未占用时返回空。
 */
inline DatabaseSlot *databaseAt(std::int32_t index)
{
    if (index < 0 || index >= kMaxDatabases) return nullptr;
    DatabaseSlot &slot = databases()[static_cast<std::size_t>(index)];
    return slot.used ? &slot : nullptr;
}

/**
 * @brief 校验结果集句柄并读取已占用的结果槽。
 * @param index 从零开始的数据库资源槽编号。
 * @return 有效结果槽；句柄越界或未占用时返回空。
 */
inline ResultSlot *resultAt(std::int32_t index)
{
    if (index < 0 || index >= kMaxResults) return nullptr;
    ResultSlot &slot = results()[static_cast<std::size_t>(index)];
    return slot.used ? &slot : nullptr;
}

/**
 * @brief 按文本形状判断格子类型：整数 / 实数 / 其它当文本。
 */
inline Cell classify(const char *text, unsigned long long length)
{
    Cell cell;
    if (text == nullptr)
    {
        cell.kind = 0;
        return cell;
    }
    cell.text.assign(text, static_cast<std::size_t>(length));
    const std::string &value = cell.text;
    bool integer = !value.empty();
    std::size_t start = (value[0] == '-' || value[0] == '+') ? 1u : 0u;
    if (start >= value.size()) integer = false;
    for (std::size_t i = start; integer && i < value.size(); ++i)
        if (value[i] < '0' || value[i] > '9') integer = false;
    if (integer)
    {
        cell.kind = 1;
        errno = 0;
        cell.intValue = std::strtoll(value.c_str(), nullptr, 10);
        if (errno == ERANGE)
        {
            cell.kind = 3;
            cell.intValue = 0;
            return cell;
        }
        cell.realValue = static_cast<double>(cell.intValue);
        return cell;
    }
    char *stop = nullptr;
    const double real = std::strtod(value.c_str(), &stop);
    if (stop != nullptr && *stop == '\0' && stop != value.c_str() && std::isfinite(real))
    {
        cell.kind = 2;
        cell.realValue = real;
        cell.intValue = real >= -9223372036854775808.0 && real < 9223372036854775808.0 ? static_cast<long long>(real) : 0;
        return cell;
    }
    cell.kind = 3;
    return cell;
}

/**
 * @brief `mysql_affected_rows` 在没有影响行信息时返回 (my_ulonglong)-1，统一折成 0。
 */
inline std::int64_t normalizeAffected(unsigned long long value)
{
    return value == ~0ULL ? 0 : static_cast<std::int64_t>(value);
}

/* ── 对外函数 ─────────────────────────────────────────────────────────── */

inline std::int32_t sqlConnect(String host, String user, String password, String database, std::int32_t port)
{
    if (!initMysql())
    {
        setErrorText(lastErrorText(), loadErrorText());
        return -1;
    }
    int slot = -1;
    for (int i = 0; i < kMaxDatabases; ++i)
    {
        if (!databases()[static_cast<std::size_t>(i)].used)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        setErrorText(lastErrorText(), "数据库句柄用尽");
        return -1;
    }
    void *handle = api().init(nullptr);
    if (handle == nullptr)
    {
        setErrorText(lastErrorText(), "mysql_init 失败（内存不足）");
        return -1;
    }
    const char *hostText = host.empty() ? nullptr : host.c_str();
    const char *userText = user.empty() ? nullptr : user.c_str();
    const char *passwordText = password.empty() ? nullptr : password.c_str();
    const char *databaseText = database.empty() ? nullptr : database.c_str();
    void *connected = api().real_connect(handle, hostText, userText, passwordText, databaseText,
                                         static_cast<unsigned int>(port), nullptr,
                                         kClientMultiStatements);
    if (connected == nullptr)
    {
        setErrorText(lastErrorText(), api().error(handle));
        if (api().close != nullptr) api().close(handle);
        return -1;
    }
    if (api().set_charset != nullptr) api().set_charset(handle, "utf8mb4");
    DatabaseSlot &target = databases()[static_cast<std::size_t>(slot)];
    target.used = true;
    target.handle = handle;
    target.backend = SqlBackend::MySQL;
    target.affected = 0;
    target.error[0] = '\0';
    return slot;
}

/**
 * @brief 打开 SQLite 文件或内存数据库。
 * @param path UTF-8 文件路径或 :memory:。
 * @return 连接句柄，失败返回 -1。
 */
inline std::int32_t sqlOpenSqlite(String path)
{
    if (!initSqlite()) return -1;
    for (int i = 0; i < kMaxDatabases; ++i)
    {
        DatabaseSlot &slot = databases()[static_cast<std::size_t>(i)];
        if (slot.used) continue;
        void *handle = nullptr;
        if (sqliteApi().open(path.c_str(), &handle) != 0)
        {
            setErrorText(lastErrorText(), handle ? sqliteApi().error(handle) : "SQLite 打开失败");
            if (handle) sqliteApi().close(handle);
            return -1;
        }
        slot.handle = handle;
        slot.used = true;
        slot.backend = SqlBackend::SQLite;
        slot.affected = 0;
        slot.error[0] = '\0';
        return i;
    }
    setErrorText(lastErrorText(), "数据库句柄用尽");
    return -1;
}

/**
 * @brief 查找空连接槽。
 * @return 空槽下标；用尽返回 -1 并记录错误。
 */
inline int freeDatabaseSlot()
{
    for (int i = 0; i < kMaxDatabases; ++i)
        if (!databases()[static_cast<std::size_t>(i)].used) return i;
    setErrorText(lastErrorText(), "数据库句柄用尽");
    return -1;
}

/**
 * @brief 使用 libpq 连接串连接 PostgreSQL，固定 UTF-8 与标准字符串转义。
 * @param options libpq 连接串或 URI。
 * @return 连接句柄或 -1。
 */
inline std::int32_t sqlOpenPostgres(String options)
{
    if (!initPostgres()) return -1;
    const int index = freeDatabaseSlot();
    if (index < 0) return -1;
    auto &table = postgresApi();
    void *handle = table.connect(options.c_str());
    if (!handle || table.status(handle) != 0 || table.encoding(handle, "UTF8") != 0)
    {
        setErrorText(lastErrorText(), handle ? table.error(handle) : "PostgreSQL 连接分配失败");
        if (handle) table.finish(handle);
        return -1;
    }
    void *setup = table.exec(handle, "SET standard_conforming_strings = on");
    if (!setup || table.resultStatus(setup) != 1)
    {
        setErrorText(lastErrorText(), setup ? table.resultError(setup) : table.error(handle));
        if (setup) table.clear(setup);
        table.finish(handle);
        return -1;
    }
    table.clear(setup);
    DatabaseSlot &slot = databases()[static_cast<std::size_t>(index)];
    slot.used = true;
    slot.handle = handle;
    slot.environment = nullptr;
    slot.backend = SqlBackend::PostgreSQL;
    slot.affected = 0;
    slot.error[0] = '\0';
    return index;
}

/**
 * @brief 使用 Unicode ODBC 连接串打开 SQL Server 或其他 ODBC 数据源。
 * @param options 驱动连接串。
 * @return 连接句柄或 -1。
 */
inline std::int32_t sqlOpenOdbc(String options)
{
    if (!initOdbc()) return -1;
    const int index = freeDatabaseSlot();
    if (index < 0) return -1;
    std::u16string text;
    try
    {
        text = sqlUtf16(std::string(options.data(), options.size()));
    } catch (const std::range_error &)
    {
        setErrorText(lastErrorText(), "ODBC 连接串不是有效 UTF-8");
        return -1;
    }
    auto &table = odbcApi();
    void *environment = nullptr, *handle = nullptr;
    if (!odbcSuccess(table.allocate(1, nullptr, &environment)))
    {
        setErrorText(lastErrorText(), "ODBC 环境句柄分配失败");
        return -1;
    }
    if (!odbcSuccess(table.environment(environment, 200, reinterpret_cast<void *>(std::uintptr_t(3)), 0)))
    {
        odbcError(1, environment, lastErrorText());
        table.freeHandle(1, environment);
        return -1;
    }
    if (!odbcSuccess(table.allocate(2, environment, &handle)))
    {
        odbcError(1, environment, lastErrorText());
        table.freeHandle(1, environment);
        return -1;
    }
    if (table.connectAttribute) table.connectAttribute(handle, 103, reinterpret_cast<void *>(std::uintptr_t(5)), 0);
    const auto status = table.connect(handle, nullptr, reinterpret_cast<std::uint16_t *>(text.data()), -3,
                                      nullptr, 0, nullptr, 0);  // SQL_NTS / SQL_DRIVER_NOPROMPT
    if (!odbcSuccess(status))
    {
        odbcError(2, handle, lastErrorText());
        table.freeHandle(2, handle);
        table.freeHandle(1, environment);
        return -1;
    }
    DatabaseSlot &slot = databases()[static_cast<std::size_t>(index)];
    slot.used = true;
    slot.handle = handle;
    slot.environment = environment;
    slot.backend = SqlBackend::Odbc;
    slot.affected = 0;
    slot.error[0] = '\0';
    return index;
}

/**
 * @brief 按连接后端关闭数据库资源并回收槽位。
 * @param index 从零开始的数据库资源槽编号。
 * @return 关闭操作的语言侧状态码。
 */
inline std::int32_t sqlClose(std::int32_t index)
{
    DatabaseSlot *slot = databaseAt(index);
    if (slot == nullptr) return -1;
    if (slot->handle != nullptr)
    {
        if (slot->backend == SqlBackend::SQLite)
            sqliteApi().close(slot->handle);
        else if (slot->backend == SqlBackend::PostgreSQL)
            postgresApi().finish(slot->handle);
        else if (slot->backend == SqlBackend::Odbc)
        {
            odbcApi().disconnect(slot->handle);
            odbcApi().freeHandle(2, slot->handle);
            odbcApi().freeHandle(1, slot->environment);
        }
        else if (api().close != nullptr)
            api().close(slot->handle);
    }
    slot->used = false;
    slot->handle = nullptr;
    slot->environment = nullptr;
    slot->error[0] = '\0';
    return 0;
}

/**
 * @brief 读取指定数据库连接或最近一次全局 SQL 错误。
 * @param index 从零开始的数据库资源槽编号。
 * @return 连接可见的原始错误文本。
 */
inline String sqlError(std::int32_t index)
{
    DatabaseSlot *slot = databaseAt(index);
    if (slot == nullptr) return String(lastErrorText());
    return String(slot->error);
}

/**
 * @brief 清空结果集内容并设置其占用状态。
 * @param result 需要清空内容并更新占用状态的结果槽。
 * @param used 是否将结果槽标记为已占用。
 */
inline void resetResult(ResultSlot &result, bool used)
{
    result.used = used;
    result.rows = 0;
    result.cols = 0;
    result.affected = 0;
    result.colNames.clear();
    result.cells.clear();
}

/**
 * @brief 按服务端列类型复制文本格子，避免将数字形状的 TEXT 误判为数值。
 * @param text 数据字节。
 * @param length 字节数。
 * @param kind 目标格子类型。
 * @return 格子副本。
 */
inline Cell typedCell(const char *text, std::size_t length, int kind)
{
    if (kind == 3)
    {
        Cell cell;
        cell.kind = 3;
        cell.text.assign(text, length);
        return cell;
    }
    Cell cell = classify(text, length);
    if (kind == 2 && (cell.kind == 1 || cell.kind == 2)) cell.kind = 2;
    return cell;
}

/**
 * @brief 执行 PostgreSQL 同步查询，复制最终结果并释放 libpq 结果对象。
 * @param slot 连接槽。
 * @param result 输出结果。
 * @param sql SQL 文本。
 * @return 是否成功。
 */
inline bool executePostgres(DatabaseSlot &slot, ResultSlot &result, const String &sql)
{
    auto &table = postgresApi();
    void *native = table.exec(slot.handle, sql.c_str());
    if (!native)
    {
        setErrorText(slot.error, table.error(slot.handle));
        return false;
    }
    std::unique_ptr<void, void (*)(void *)> nativeOwner(native, table.clear);
    const int status = table.resultStatus(native);
    if (status != 0 && status != 1 && status != 2)
    {
        setErrorText(slot.error, table.resultError(native));
        nativeOwner.reset();
        if (status == 3 || status == 4 || status == 8)
        {
            setErrorText(slot.error, "PostgreSQL COPY 流不属于同步结果接口，连接已关闭");
            table.finish(slot.handle);
            slot.handle = nullptr;
        }
        return false;
    }
    result.rows = table.rows(native);
    result.cols = table.columns(native);
    const char *command = table.command(native);
    const bool writes = std::strncmp(command, "INSERT", 6) == 0 || std::strncmp(command, "UPDATE", 6) == 0 || std::strncmp(command, "DELETE", 6) == 0 || std::strncmp(command, "MERGE", 5) == 0;
    result.affected = writes ? std::strtoull(table.affected(native), nullptr, 10) : 0;
    for (int c = 0; c < result.cols; ++c) result.colNames.emplace_back(table.columnName(native, c));
    for (int r = 0; r < result.rows; ++r)
        for (int c = 0; c < result.cols; ++c)
        {
            Cell cell;
            if (!table.isNull(native, r, c))
            {
                const char *text = table.value(native, r, c);
                const auto length = static_cast<std::size_t>(table.length(native, r, c));
                const unsigned int type = table.columnType(native, c);
                if (type == 16)  // PostgreSQL bool 文本为 t / f。
                {
                    cell.kind = 1;
                    cell.intValue = length && text[0] == 't' ? 1 : 0;
                    cell.realValue = static_cast<double>(cell.intValue);
                    cell.text.assign(text, length);
                }
                else
                {
                    const int kind = type == 20 || type == 21 || type == 23 ? 1 : type == 700 || type == 701 || type == 1700 ? 2 :
                                                                                                                               3;
                    cell = typedCell(text, length, kind);
                }
            }
            result.cells.push_back(std::move(cell));
        }
    nativeOwner.reset();
    slot.affected = static_cast<std::int64_t>(result.affected);
    return true;
}

/**
 * @brief 从 ODBC 结果中分块读取 Unicode 文本格子。
 * @param statement 语句句柄。
 * @param column 一起始列号。
 * @param kind 列类型。
 * @param cell 输出格子。
 * @param error 错误缓冲区。
 * @return 读取是否成功。
 */
inline bool odbcCell(void *statement, std::uint16_t column, int kind, Cell &cell, char *error)
{
    std::u16string text;
    while (true)
    {
        std::uint16_t buffer[1024] = {};
        std::intptr_t length = 0;
        const auto status = odbcApi().get(statement, column, -8, buffer, sizeof(buffer), &length);  // SQL_C_WCHAR
        if (status == 100) break;
        if (!odbcSuccess(status))
        {
            odbcError(3, statement, error);
            return false;
        }
        if (length == -1)
        {
            cell = Cell{};
            return true;
        }  // SQL_NULL_DATA
        std::size_t units = 0;
        if (length >= 0)
            units = static_cast<std::size_t>(std::min<std::intptr_t>(length / 2, 1023));
        else
            while (units < 1023 && buffer[units]) ++units;  // SQL_NO_TOTAL
        for (std::size_t i = 0; i < units; ++i) text.push_back(static_cast<char16_t>(buffer[i]));
        if (status == 0 || (length >= 0 && length < static_cast<std::intptr_t>(sizeof(buffer)))) break;
    }
    const std::string bytes = sqlUtf8(text);
    cell = typedCell(bytes.data(), bytes.size(), kind);
    return true;
}

/**
 * @brief 执行 ODBC SQL，遍历多结果集并复制最终结果；Unicode 与长文本走宽字符接口。
 * @param slot 连接槽。
 * @param result 输出结果。
 * @param sql SQL 文本。
 * @return 是否成功。
 */
inline bool executeOdbc(DatabaseSlot &slot, ResultSlot &result, const String &sql)
{
    auto &table = odbcApi();
    std::u16string query;
    try
    {
        query = sqlUtf16(std::string(sql.data(), sql.size()));
    } catch (const std::range_error &)
    {
        setErrorText(slot.error, "ODBC SQL 不是有效 UTF-8");
        return false;
    }
    void *statement = nullptr;
    if (!odbcSuccess(table.allocate(3, slot.handle, &statement)))
    {
        odbcError(2, slot.handle, slot.error);
        return false;
    }
    const auto fail = [&]() { table.freeHandle(3, statement); return false; };
    const auto executed = table.exec(statement, reinterpret_cast<std::uint16_t *>(query.data()), -3);
    // SQL_NO_DATA 是无行写入或部分驱动 DDL 的正常结果，不是连接错误。
    if (!odbcSuccess(executed) && executed != 100)
    {
        odbcError(3, statement, slot.error);
        return fail();
    }
    while (true)
    {
        resetResult(result, true);
        std::int16_t columns = 0;
        if (!odbcSuccess(table.columns(statement, &columns)))
        {
            odbcError(3, statement, slot.error);
            return fail();
        }
        result.cols = columns;
        std::vector<int> kinds;
        for (std::int16_t c = 1; c <= columns; ++c)
        {
            std::uint16_t name[1024] = {};
            std::int16_t length = 0, type = 0, scale = 0, nullable = 0;
            std::uintptr_t capacity = 0;
            if (!odbcSuccess(table.describe(statement, static_cast<std::uint16_t>(c), name, 1024, &length, &type, &capacity, &scale, &nullable)))
            {
                odbcError(3, statement, slot.error);
                return fail();
            }
            if (length < 0 || length >= 1024)
            {
                setErrorText(slot.error, "ODBC 列名超出支持长度");
                return fail();
            }
            std::u16string wide;
            for (int i = 0; i < length; ++i) wide.push_back(static_cast<char16_t>(name[i]));
            result.colNames.push_back(sqlUtf8(wide));
            if (type == -2 || type == -3 || type == -4)
            {
                setErrorText(slot.error, "ODBC 二进制列暂不支持，请在 SQL 中转换为文本");
                return fail();
            }
            const int kind = type == -7 || type == -6 || type == -5 || type == 4 || type == 5 ? 1 : type == 2 || type == 3 || type == 6 || type == 7 || type == 8 ? 2 :
                                                                                                                                                                    3;
            kinds.push_back(kind);
        }
        if (columns > 0)
        {
            while (true)
            {
                const auto fetched = table.fetch(statement);
                if (fetched == 100) break;
                if (!odbcSuccess(fetched))
                {
                    odbcError(3, statement, slot.error);
                    return fail();
                }
                for (std::int16_t c = 1; c <= columns; ++c)
                {
                    Cell cell;
                    if (!odbcCell(statement, static_cast<std::uint16_t>(c), kinds[static_cast<std::size_t>(c - 1)], cell, slot.error)) return fail();
                    result.cells.push_back(std::move(cell));
                }
                ++result.rows;
            }
        }
        std::intptr_t affected = 0;
        if (!odbcSuccess(table.rowCount(statement, &affected)))
        {
            odbcError(3, statement, slot.error);
            return fail();
        }
        result.affected = affected > 0 && columns == 0 ? static_cast<unsigned long long>(affected) : 0;
        const auto more = table.more(statement);
        if (more == 100) break;
        if (!odbcSuccess(more))
        {
            odbcError(3, statement, slot.error);
            return fail();
        }
    }
    table.freeHandle(3, statement);
    slot.affected = static_cast<std::int64_t>(result.affected);
    return true;
}

/* 把一段结果集搬进槽（覆盖上一次的内容）。 */
inline bool collectResult(DatabaseSlot &slot, ResultSlot &result, void *res)
{
    const unsigned int columns = api().num_fields(res);
    result.cols = static_cast<int>(columns);
    result.rows = 0;
    result.colNames.clear();
    result.cells.clear();
    result.colNames.resize(columns);
    std::vector<int> kinds(columns, 0);
    for (unsigned int c = 0; c < columns; ++c)
    {
        void *field = api().fetch_field != nullptr ? api().fetch_field(res) : nullptr;
        MysqlFieldPrefix metadata{};
        if (field != nullptr) std::memcpy(&metadata, field, sizeof(metadata));
        result.colNames[c] = metadata.name != nullptr ? metadata.name : "";
        if (field != nullptr)
        {
            const int type = metadata.type;
            kinds[c] = type == 1 || type == 2 || type == 3 || type == 8 || type == 9 || type == 13 ? 1 : type == 0 || type == 4 || type == 5 || type == 246 ? 2 :
                                                                                                                                                              3;
        }
    }
    while (true)
    {
        char **row = api().fetch_row(res);
        if (row == nullptr) break;
        for (unsigned int c = 0; c < columns; ++c)
        {
            const char *text = row[c];
            const std::size_t length = text != nullptr ? std::strlen(text) : 0u;
            result.cells.push_back(text != nullptr && kinds[c] != 0 ? typedCell(text, length, kinds[c]) : classify(text, length));
        }
        result.rows += 1;
    }
    (void)slot;
    return true;
}

/**
 * @brief 执行 SQLite 多条语句并复制最后一条语句的结果；每条已准备语句都保证释放。
 * @param slot 已打开的 SQLite 连接。
 * @param result 接收结果的槽。
 * @param sql SQL 文本。
 * @param parameters 可选的绑定参数数组；存在时限制为一条语句。
 * @return 是否成功；失败时连接保留诊断，之前已执行的语句不自动回滚。
 */
inline bool executeSqlite(DatabaseSlot &slot, ResultSlot &result, const String &sql, const json::Array *parameters = nullptr)
{
    auto &table = sqliteApi();
    const char *cursor = sql.c_str();
    bool executed = false;
    while (*cursor)
    {
        void *statement = nullptr;
        const char *tail = nullptr;
        if (table.prepare(slot.handle, cursor, -1, &statement, &tail) != 0)
        {
            setErrorText(slot.error, table.error(slot.handle));
            if (statement) table.finalize(statement);
            return false;
        }
        cursor = tail;
        if (!statement) continue;  // 空白和注释不会产生语句。
        executed = true;
        std::unique_ptr<void, int (*)(void *)> statementOwner(statement, table.finalize);
        if (parameters)
        {
            void *extra = nullptr;
            const char *remaining = cursor;
            while (remaining && *remaining)
            {
                const char *next = nullptr;
                if (table.prepare(slot.handle, remaining, -1, &extra, &next) != 0 || extra)
                {
                    if (extra) table.finalize(extra);
                    setErrorText(slot.error, "参数化 SQL 只允许一条完整语句");
                    return false;
                }
                remaining = next;
            }
            auto *prepared = static_cast<sqlite3_stmt *>(statement);
            if (sqlite3_bind_parameter_count(prepared) != static_cast<int>(parameters->size()))
            {
                setErrorText(slot.error, "SQL 占位符与参数数量不一致");
                return false;
            }
            int position = 1;
            for (const auto &value : *parameters)
            {
                int status = SQLITE_MISMATCH;
                if (value.isNull())
                    status = sqlite3_bind_null(prepared, position);
                else if (value.isBool())
                    status = sqlite3_bind_int64(prepared, position, *value.asBool() ? 1 : 0);
                else if (value.isInt())
                    status = sqlite3_bind_int64(prepared, position, *value.asInt());
                else if (value.isDouble() && std::isfinite(*value.asDouble()))
                    status = sqlite3_bind_double(prepared, position, *value.asDouble());
                else if (value.isString())
                {
                    const auto bytes = *value.asString();
                    status = sqlite3_bind_text64(prepared, position, bytes.data(), bytes.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
                }
                if (status != SQLITE_OK)
                {
                    setErrorText(slot.error, "SQL 参数类型无效或绑定失败");
                    return false;
                }
                ++position;
            }
            cursor = nullptr;
        }
        resetResult(result, true);
        result.cols = table.columns(statement);
        for (int c = 0; c < result.cols; ++c) result.colNames.emplace_back(table.columnName(statement, c));
        const bool readonly = table.readonly(statement) != 0;
        const int totalBefore = table.totalChanges(slot.handle);
        int status;
        while ((status = table.step(statement)) == 100)  // SQLITE_ROW
        {
            for (int c = 0; c < result.cols; ++c)
            {
                Cell cell;
                const int type = table.columnType(statement, c);
                if (type == 4)  // BLOB 需要专用二进制接口，禁止静默按 C 字符串截断。
                {
                    setErrorText(slot.error, "SQLite BLOB 暂不支持，请使用 hex(...) 查询");
                    statementOwner.reset();
                    return false;
                }
                if (type != 5)  // SQLITE_NULL
                {
                    cell.kind = type == 1 ? 1 : type == 2 ? 2 :
                                                            3;
                    if (cell.kind == 1 || cell.kind == 2)
                    {
                        cell.intValue = table.columnInt(statement, c);
                        cell.realValue = table.columnReal(statement, c);
                    }
                    const auto *text = table.columnText(statement, c);
                    const int length = table.columnBytes(statement, c);
                    if (text) cell.text.assign(reinterpret_cast<const char *>(text), static_cast<std::size_t>(length));
                }
                result.cells.push_back(std::move(cell));
            }
            ++result.rows;
        }
        if (status != 101)  // SQLITE_DONE
        {
            setErrorText(slot.error, table.error(slot.handle));
            statementOwner.reset();
            return false;
        }
        result.affected = readonly || table.totalChanges(slot.handle) == totalBefore ? 0 : static_cast<unsigned long long>(table.changes(slot.handle));
        statementOwner.reset();
        slot.affected = static_cast<std::int64_t>(result.affected);
        if (parameters) break;
    }
    if (parameters && !executed)
    {
        setErrorText(slot.error, "参数化 SQL 必须包含一条语句");
        return false;
    }
    return true;
}

/**
 * @brief 按连接后端执行 SQL 并保存结果集或受影响行数。
 * @param index 从零开始的数据库资源槽编号。
 * @param sql 需要执行的完整 SQL 文本。
 * @param parameters SQLite 的可选绑定参数数组，空指针表示普通文本执行。
 * @return 结果集句柄；失败时返回无效句柄并保存诊断。
 */
inline std::int32_t sqlExecPrepared(std::int32_t index, String sql, const json::Array *parameters)
{
    DatabaseSlot *slot = databaseAt(index);
    if (slot == nullptr)
    {
        setErrorText(lastErrorText(), "数据库句柄无效");
        return -1;
    }
    if (slot->handle == nullptr)
    {
        if (slot->error[0] == '\0') setErrorText(slot->error, "数据库未连接");
        return -1;
    }
    if (parameters && slot->backend != SqlBackend::SQLite)
    {
        setErrorText(slot->error, "此后端尚未实现参数化执行，请使用 SQLite 或导出 SQL 与参数交给原生驱动");
        return -1;
    }
    int handle = -1;
    for (int i = 0; i < kMaxResults; ++i)
    {
        if (!results()[static_cast<std::size_t>(i)].used)
        {
            handle = i;
            break;
        }
    }
    if (handle < 0)
    {
        setErrorText(slot->error, "结果集句柄用尽");
        return -1;
    }
    ResultSlot &result = results()[static_cast<std::size_t>(handle)];
    resetResult(result, true);
    slot->error[0] = '\0';

    if (slot->backend == SqlBackend::SQLite)
    {
        if (executeSqlite(*slot, result, sql, parameters)) return handle;
        resetResult(result, false);
        return -1;
    }
    if (slot->backend == SqlBackend::PostgreSQL || slot->backend == SqlBackend::Odbc)
    {
        const bool ok = slot->backend == SqlBackend::PostgreSQL ? executePostgres(*slot, result, sql) : executeOdbc(*slot, result, sql);
        if (ok) return handle;
        resetResult(result, false);
        return -1;
    }
    if (api().query(slot->handle, sql.c_str()) != 0)
    {
        setErrorText(slot->error, api().error(slot->handle));
        resetResult(result, false);
        return -1;
    }
    /*
     * 多结果集按 MySQL 官方写法推进：store_result 取结果，next_result 的
     * 0 = 还有下一段、-1 = 没有更多、>0 = 出错。`USE` 这类没有结果集的语句
     * 拿到的就是 -1，不能当成失败。
     */
    while (true)
    {
        void *res = api().store_result(slot->handle);
        if (res != nullptr)
        {
            collectResult(*slot, result, res);
            if (api().free_result != nullptr) api().free_result(res);
        }
        else if (api().field_count != nullptr && api().field_count(slot->handle) != 0u)
        {
            setErrorText(slot->error, api().error(slot->handle));
            resetResult(result, false);
            return -1;
        }
        else if (api().affected_rows != nullptr)
        {
            result.affected = normalizeAffected(api().affected_rows(slot->handle));
        }
        if (api().next_result == nullptr) break;
        const int status = api().next_result(slot->handle);
        if (status > 0)
        {
            setErrorText(slot->error, api().error(slot->handle));
            resetResult(result, false);
            return -1;
        }
        if (status != 0) break;
    }
    return handle;
}

/**
 * @brief 执行不带参数的 SQL，保留多语句兼容行为。
 * @param index 数据库槽位。
 * @param sql SQL 文本。
 * @return 结果句柄，失败时返回负数。
 */
inline std::int32_t sqlExec(std::int32_t index, String sql)
{
    return sqlExecPrepared(index, std::move(sql), nullptr);
}
/**
 * @brief 校验参数数组并执行单条参数化 SQLite 语句。
 * @param index 数据库槽位。
 * @param sql 包含占位符的 SQL。
 * @param parameters 参数数组 JSON。
 * @return 结果句柄，失败时连接保留诊断。
 */
inline std::int32_t sqlExecBound(std::int32_t index, String sql, String parameters)
{
    const auto parsed = json::parse(parameters.c_str());
    if (!parsed.ok() || !parsed.value->isArray())
    {
        if (auto *slot = databaseAt(index))
            setErrorText(slot->error, "SQL 参数必须为 JSON 标量数组");
        else
            setErrorText(lastErrorText(), "数据库句柄无效");
        return -1;
    }
    return sqlExecPrepared(index, std::move(sql), parsed.value->asArray());
}

/**
 * @brief 读取连接最近一次执行影响的行数。
 * @param index 从零开始的数据库资源槽编号。
 * @return 受影响行数；连接无效时返回约定的失败值。
 */
inline std::int64_t sqlAffected(std::int32_t index)
{
    DatabaseSlot *slot = databaseAt(index);
    if (slot == nullptr || slot->handle == nullptr) return 0;
    if (slot->backend != SqlBackend::MySQL) return slot->affected;
    if (api().affected_rows == nullptr) return 0;
    return normalizeAffected(api().affected_rows(slot->handle));
}

/**
 * @brief 释放结果集内容并回收其槽位。
 * @param handle 当前模块中的资源句柄。
 * @return 释放操作的语言侧状态码。
 */
inline std::int32_t sqlFree(std::int32_t handle)
{
    ResultSlot *slot = resultAt(handle);
    if (slot == nullptr) return -1;
    resetResult(*slot, false);
    return 0;
}

/**
 * @brief 读取结果集的行数。
 * @param handle 当前模块中的资源句柄。
 * @return 有效结果的行数；句柄无效时返回约定的失败值。
 */
inline std::int32_t sqlRowCount(std::int32_t handle)
{
    ResultSlot *slot = resultAt(handle);
    return slot == nullptr ? 0 : slot->rows;
}

/**
 * @brief 读取结果集的列数。
 * @param handle 当前模块中的资源句柄。
 * @return 有效结果的列数；句柄无效时返回约定的失败值。
 */
inline std::int32_t sqlColCount(std::int32_t handle)
{
    ResultSlot *slot = resultAt(handle);
    return slot == nullptr ? 0 : slot->cols;
}

/*
 * 影响行数从**结果句柄**里取：`mysql_affected_rows` 在 `mysql_next_result`
 * 返回「没有更多结果」之后会被重置成 0，语句跑完再问连接就晚了。
 */
inline std::int64_t sqlResultAffected(std::int32_t handle)
{
    ResultSlot *slot = resultAt(handle);
    if (slot == nullptr) return 0;
    return static_cast<std::int64_t>(slot->affected);
}

/**
 * @brief 读取结果集指定列的名称。
 * @param handle 当前模块中的资源句柄。
 * @param column 从零开始的结果列下标。
 * @return 列名称；句柄或列下标无效时返回空串。
 */
inline String sqlColName(std::int32_t handle, std::int32_t column)
{
    ResultSlot *slot = resultAt(handle);
    if (slot == nullptr || column < 0 || column >= slot->cols) return String();
    return slot->colNames[static_cast<std::size_t>(column)];
}

/**
 * @brief 校验结果句柄及行列下标并借用单元格。
 * @param handle 当前模块中的资源句柄。
 * @param row 从零开始的结果行下标。
 * @param column 从零开始的结果列下标。
 * @return 有效单元格地址；越界或无效句柄返回空。
 */
inline Cell *cellAt(std::int32_t handle, std::int32_t row, std::int32_t column)
{
    ResultSlot *slot = resultAt(handle);
    if (slot == nullptr || row < 0 || row >= slot->rows || column < 0 || column >= slot->cols) return nullptr;
    return &slot->cells[static_cast<std::size_t>(row) * static_cast<std::size_t>(slot->cols) + static_cast<std::size_t>(column)];
}

/**
 * @brief 读取 SQL 单元格保存的文本值。
 * @param handle 当前模块中的资源句柄。
 * @param row 从零开始的结果行下标。
 * @param column 从零开始的结果列下标。
 * @return 单元格文本；位置无效时返回空串。
 */
inline String sqlCellText(std::int32_t handle, std::int32_t row, std::int32_t column)
{
    Cell *cell = cellAt(handle, row, column);
    if (cell == nullptr) return String();
    return String(cell->text.data(), cell->text.size());
}

/**
 * @brief 读取 SQL 单元格的整数值。
 * @param handle 当前模块中的资源句柄。
 * @param row 从零开始的结果行下标。
 * @param column 从零开始的结果列下标。
 * @return 单元格整数；位置或类型无效时返回约定的零值。
 */
inline std::int64_t sqlCellInt(std::int32_t handle, std::int32_t row, std::int32_t column)
{
    Cell *cell = cellAt(handle, row, column);
    if (cell == nullptr) return 0;
    return static_cast<std::int64_t>(cell->intValue);
}

/**
 * @brief 读取 SQL 单元格的浮点值。
 * @param handle 当前模块中的资源句柄。
 * @param row 从零开始的结果行下标。
 * @param column 从零开始的结果列下标。
 * @return 单元格实数；位置或类型无效时返回约定的零值。
 */
inline double sqlCellReal(std::int32_t handle, std::int32_t row, std::int32_t column)
{
    Cell *cell = cellAt(handle, row, column);
    if (cell == nullptr) return 0.0;
    if (cell->kind == 1 || cell->kind == 2) return cell->realValue;
    return std::strtod(cell->text.c_str(), nullptr);
}

/**
 * @brief 读取 SQL 单元格的语言侧值种类。
 * @param handle 当前模块中的资源句柄。
 * @param row 从零开始的结果行下标。
 * @param column 从零开始的结果列下标。
 * @return 单元格种类编码；无效位置返回约定的失败编码。
 */
inline std::int32_t sqlCellKind(std::int32_t handle, std::int32_t row, std::int32_t column)
{
    Cell *cell = cellAt(handle, row, column);
    return cell == nullptr ? 0 : cell->kind;
}

/**
 * @brief 独占一个 SQL 连接槽及其公开状态，销毁时自动关闭连接。
 */
struct DatabaseOwner
{
    std::int32_t connection = -1;
    std::int64_t driver = 0;
    std::int64_t affected = 0;
    String error;
    /**
     * @brief 关闭仍由本记录拥有的连接，包括记录分配失败时的临时所有者。
     */
    ~DatabaseOwner()
    {
        if (connection >= 0) sqlClose(connection);
    }
};
/**
 * @brief 释放独占连接，并清理所属状态。
 * @param value 待销毁的数据库所有者。
 */
void destroyDatabaseOwner(void *value)
{
    delete static_cast<DatabaseOwner *>(value);
}
const stackvm::NativeRecordType databaseOwnerType{ "std::sql::Database", nullptr, destroyDatabaseOwner, nullptr };
/**
 * @brief 校验并借用仍存活的连接所有者。
 * @param context 当前胶水上下文。
 * @param handle 所有者记录句柄。
 * @return 对应状态；类型错误或已释放时报告错误并返回空。
 */
DatabaseOwner *databaseOwner(chali_glue::Context &context, std::int64_t handle)
{
    auto *owner = static_cast<DatabaseOwner *>(context.nativeRecord(handle, databaseOwnerType));
    if (!owner) context.fail("Database 连接所有者已经释放或类型无效");
    return owner;
}
/**
 * @brief 接管连接槽，使其生命周期随不可复制的 opcode 管理。
 * @param context 当前胶水上下文。
 * @param connection 连接槽编号，失败时为负数。
 * @param driver 公开数据库驱动编号。
 * @return 新建所有者的原生记录句柄。
 */
std::int64_t ownDatabase(chali_glue::Context &context, std::int32_t connection, std::int64_t driver)
{
    auto owner = std::make_unique<DatabaseOwner>();
    owner->connection = connection;
    owner->driver = driver;
    if (connection < 0) owner->error = sqlError(connection);
    const auto handle = context.makeNativeRecord(databaseOwnerType, owner.get());
    if (handle)
        owner.release();
    else if (connection >= 0)
        sqlClose(connection);
    return handle;
}
/**
 * @brief 默认构造未连接的独占数据库值。
 * @param context 当前胶水上下文。
 * @return 拥有空连接状态的记录句柄。
 */
std::int64_t newDatabaseOwner(chali_glue::Context &context)
{
    auto owner = std::make_unique<DatabaseOwner>();
    const auto handle = context.makeNativeRecord(databaseOwnerType, owner.get());
    if (handle) owner.release();
    return handle;
}
/**
 * @brief 注册连接所有者的访问、状态更新和关闭协议。
 * @param registry 当前宿主函数注册表。
 * @param error 接收注册失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bindDatabaseOwner(Registry &registry, String *error)
{
    return registry.bindTypedWithContext<std::int64_t>("std::sql::__sqlOwnerNew", newDatabaseOwner, error)
           && registry.bindTypedWithContext<std::int64_t, std::int32_t, std::int64_t>("std::sql::__sqlOwn", ownDatabase, error)
           && native_record_glue::bind(registry.native(), "std::sql::__sqlOwnerDrop", 1, native_record_glue::drop, error)
           && registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int64_t>("std::sql::__sqlOwnerState", [](chali_glue::Context &context, std::int64_t handle, std::int64_t operation) {
                auto *owner = databaseOwner(context, handle);
                if (!owner) return std::int64_t(0);
                if (operation == 0) return std::int64_t(owner->connection);
                if (operation == 1) return owner->driver;
                if (operation == 2) return owner->affected;
                if (operation == 3) {
                    auto *slot = databaseAt(owner->connection);
                    return std::int64_t(slot && slot->handle);
                }
                if (operation == 4) { if (owner->connection >= 0) sqlClose(owner->connection); owner->connection = -1; return std::int64_t(0); }
                context.fail("Database 状态操作编号无效");
                return std::int64_t(0); }, error) && registry.bindTypedWithContext<String, std::int64_t>("std::sql::__sqlOwnerError", [](chali_glue::Context &context, std::int64_t handle) { auto *owner = databaseOwner(context, handle); return owner ? owner->error : String(); }, error) && registry.bindTypedWithContext<void, std::int64_t, String, std::int64_t>("std::sql::__sqlOwnerUpdate", [](chali_glue::Context &context, std::int64_t handle, String text, std::int64_t affected) { auto *owner = databaseOwner(context, handle); if (owner) { owner->error = std::move(text); if (affected >= 0) owner->affected = affected; } }, error);
}

/**
 * @brief 为宿主注册 std_sql 模块的标准库接口。
 * @param registry 接收类型化函数绑定的宿主注册表。
 * @param error 绑定失败时接收原始诊断，可为空。
 * @return 全部入口绑定成功时返回 true。
 */
bool bind(Registry &registry, String *error)
{
    return std_sql_ipage_glue::bind(registry, error) && registry.bindTyped<std::int32_t, std::int32_t, String, String>("std::sql::__sqlExecBound", sqlExecBound, error) && bindDatabaseOwner(registry, error) && registry.bindTyped<std::int32_t, String, String, String, String, std::int32_t>("std::sql::__sqlConnect", sqlConnect, error)
           && registry.bindTyped<std::int32_t, String>("std::sql::__sqlOpenSqlite", sqlOpenSqlite, error) && registry.bindTyped<std::int32_t, String>("std::sql::__sqlOpenPostgres", sqlOpenPostgres, error) && registry.bindTyped<std::int32_t, String>("std::sql::__sqlOpenOdbc", sqlOpenOdbc, error) && registry.bindTyped<std::int32_t, std::int32_t>("std::sql::__sqlClose", sqlClose, error) && registry.bindTyped<std::int32_t, std::int32_t, String>("std::sql::__sqlExec", sqlExec, error) && registry.bindTyped<std::int64_t, std::int32_t>("std::sql::__sqlAffected", sqlAffected, error) && registry.bindTyped<String, std::int32_t>("std::sql::__sqlError", sqlError, error) && registry.bindTyped<std::int32_t, std::int32_t>("std::sql::__sqlRowCount", sqlRowCount, error) && registry.bindTyped<std::int32_t, std::int32_t>("std::sql::__sqlColCount", sqlColCount, error) && registry.bindTyped<std::int64_t, std::int32_t>("std::sql::__sqlResultAffected", sqlResultAffected, error) && registry.bindTyped<String, std::int32_t, std::int32_t>("std::sql::__sqlColName", sqlColName, error) && registry.bindTyped<String, std::int32_t, std::int32_t, std::int32_t>("std::sql::__sqlCellText", sqlCellText, error) && registry.bindTyped<std::int64_t, std::int32_t, std::int32_t, std::int32_t>("std::sql::__sqlCellInt", sqlCellInt, error) && registry.bindTyped<double, std::int32_t, std::int32_t, std::int32_t>("std::sql::__sqlCellReal", sqlCellReal, error) && registry.bindTyped<std::int32_t, std::int32_t, std::int32_t, std::int32_t>("std::sql::__sqlCellKind", sqlCellKind, error) && registry.bindTyped<std::int32_t, std::int32_t>("std::sql::__sqlFree", sqlFree, error);
}

}  // namespace std_sql_glue
