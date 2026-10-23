/**
 * @file chali_aot.cpp
 * @brief `chali aot` 的实现：四元式 → 自包含 C 源码 → 调 cc 编成原生产物。
 *
 * 生成思路与当年那份被删掉的 `magel-emit/aot.cpp` 一样：**基本块 + 局部变量 + `goto`**。
 * 不一样的是运行时——那版把整个 VM 运行时（`RuntimeBridge` + 胶水 + 大数源码）内嵌进产物，
 * 这一版只带一份几百行的 C 运行时，产物里没有字节码、没有分派循环、不链接 stackvm。
 */
#include "chali_aot.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace chali_aot
{
namespace
{

/* ------------------------------------------------------------------ 小工具 */

/** @brief 把任意四元式名字净化成合法的 C 标识符片段。 */
Memory::String sanitize(const Memory::String &name)
{
    Memory::String out;
    for (const char c : name)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        out += ok ? c : '_';
    }
    return out;
}

/** @brief 十进制整数文本。 */
Memory::String decimal(long long value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%lld", value);
    return Memory::String(buffer);
}

/** @brief 立即数是不是浮点（带 `.` 或指数）。 */
bool literalIsFloat(const Memory::String &token)
{
    if (token.empty()) return false;
    bool digit = false;
    for (std::size_t index = 0; index < token.size(); ++index)
    {
        const char c = token[index];
        if (c == '.') return true;
        if ((c == 'e' || c == 'E') && digit && index + 1u < token.size()) return true;
        if (c >= '0' && c <= '9') digit = true;
    }
    return false;
}

/** @brief 这一格是不是类型名（`cvt:i64 42 i32 t` 的第二格就是）——类型名不是变量。 */
bool isTypeName(const Memory::String &token)
{
    static const std::set<std::string> names = {
        "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64",
        "bool", "char", "string", "void", "local", "global", "param", "field", "const",
    };
    return names.count(std::string(token.c_str())) != 0u;
}

/** @brief 立即数文本是否"看起来是个名字"（不是数字常量）。 */
bool literalIsName(const Memory::String &token)
{
    if (token.empty()) return false;
    const char first = token.front();
    return !(first == '-' || first == '+' || (first >= '0' && first <= '9'));
}

/* -------------------------------------------------------------- 运行时源码 */

/**
 * @brief 产物自带的 C 运行时。
 *
 * 只有内存（数据区 + bump 堆）、字符串、格式化输出与 panic 四件事。
 * 地址一律是**从内存块起点算起的字节偏移**，与四元式里 `addr` / `+` 出来的整数一致。
 */
const char *const kRuntime = R"CHALI_RUNTIME(
/* ---- ChaLi AOT 运行时（产物自带，不依赖虚拟机） ---- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef int64_t chali_i64;
typedef uint64_t chali_u64;

static unsigned char *chali_mem = NULL;
static size_t chali_mem_size = 0u;
static size_t chali_bump = 0u;

static void chali_panic(const char *message)
{
    fflush(stdout);
    fprintf(stderr, "chali aot: %s\n", message);
    exit(1);
}

/* `--shared` 时这两个要能被外面的 C 调到，所以走 CHALI_AOT_API 宏（见 generateSource）。 */
CHALI_AOT_API void chali_init(void)
{
    chali_mem_size = (size_t)CHALI_DATA_BYTES + (size_t)CHALI_HEAP_BYTES;
    chali_mem = (unsigned char *)calloc(1u, chali_mem_size);
    if (chali_mem == NULL) chali_panic("运行时内存不足");
    memcpy(chali_mem, chali_data_image, (size_t)CHALI_DATA_BYTES);
    chali_bump = (size_t)CHALI_DATA_BYTES;
}

CHALI_AOT_API void chali_finish(void)
{
    fflush(stdout);
    free(chali_mem);
    chali_mem = NULL;
}

/* 堆：只增不减的 bump 分配器。drop / __stringDrop 在这里是空操作——
 * 程序退出时整块一起释放，所以不会出现悬垂指针。 */
static chali_i64 chali_alloc(size_t bytes)
{
    const size_t aligned = (bytes + 15u) & ~(size_t)15u;
    if (chali_bump + aligned > chali_mem_size) chali_panic("堆内存不足");
    const size_t at = chali_bump;
    chali_bump += aligned;
    return (chali_i64)at;
}

static void chali_drop(chali_i64 address) { (void)address; }

/* 读写：地址是内存块内的偏移 */
static chali_i64 chali_load_i64(chali_i64 address) { chali_i64 v = 0; memcpy(&v, chali_mem + address, 8); return v; }
static void chali_store_i64(chali_i64 address, chali_i64 value) { memcpy(chali_mem + address, &value, 8); }
static double chali_load_f64(chali_i64 address) { chali_i64 v = chali_load_i64(address); double d = 0.0; memcpy(&d, &v, 8); return d; }
static void chali_store_f64(chali_i64 address, double value) { chali_i64 v = 0; memcpy(&v, &value, 8); chali_store_i64(address, v); }
static chali_i64 chali_load_u8(chali_i64 address) { return (chali_i64)chali_mem[address]; }
static void chali_store_u8(chali_i64 address, chali_i64 value) { chali_mem[address] = (unsigned char)(value & 0xFF); }
static chali_i64 chali_load_i32(chali_i64 address) { int32_t v = 0; memcpy(&v, chali_mem + address, 4); return (chali_i64)v; }
static void chali_store_i32(chali_i64 address, chali_i64 value) { int32_t v = (int32_t)value; memcpy(chali_mem + address, &v, 4); }
static chali_i64 chali_load_i16(chali_i64 address) { int16_t v = 0; memcpy(&v, chali_mem + address, 2); return (chali_i64)v; }
static void chali_store_i16(chali_i64 address, chali_i64 value) { int16_t v = (int16_t)value; memcpy(chali_mem + address, &v, 2); }

/* 字符串：[8 字节长度][字节…]，值就是块起点（与 VM 的堆串布局一致） */
static chali_i64 chali_string_new(const char *bytes, size_t length)
{
    const chali_i64 at = chali_alloc(length + 8u);
    const chali_i64 n = (chali_i64)length;
    memcpy(chali_mem + at, &n, 8);
    if (length != 0u) memcpy(chali_mem + at + 8, bytes, length);
    return at;
}
/* 两种串共存：数据映像里的字面量是 NUL 结尾的裸字节（地址 < CHALI_DATA_BYTES），
 * 堆上的串是「8 字节长度 + 字节」。VM 那侧靠 `heap->contains` 区分，这里靠地址分界线。 */
static int chali_on_heap(chali_i64 address) { return address >= (chali_i64)CHALI_DATA_BYTES; }
static chali_i64 chali_string_length(chali_i64 s)
{
    if (s == 0) return 0;
    return chali_on_heap(s) ? chali_load_i64(s) : (chali_i64)strlen((const char *)(chali_mem + s));
}
static const char *chali_string_bytes(chali_i64 s)
{
    return (const char *)(chali_mem + s + (chali_on_heap(s) ? 8 : 0));
}
static chali_i64 chali_string_literal(chali_i64 address)
{
    const char *text = (const char *)(chali_mem + address);
    return chali_string_new(text, strlen(text));
}
static chali_i64 chali_string_copy(chali_i64 s)
{
    return s == 0 ? 0 : chali_string_new(chali_string_bytes(s), (size_t)chali_string_length(s));
}
static void chali_string_drop(chali_i64 s) { (void)s; }

/* 数组：块头 8 字节是元素个数，返回值指向元素区，块起点 = 返回值 - 8
 * （四元式里释放数组写的是 drop(a - 8)） */
static chali_i64 chali_new_array(chali_i64 length, chali_i64 stride)
{
    const chali_i64 count = length < 0 ? 0 : length;
    const chali_i64 base = chali_alloc((size_t)count * (size_t)stride + 8u);
    memcpy(chali_mem + base, &count, 8);
    return base + 8;
}

static void chali_bounds(chali_i64 index, chali_i64 limit, const char *what)
{
    if (index < 0 || index >= limit)
    {
        char message[128];
        snprintf(message, sizeof(message), "%s越界：下标 %lld，长度 %lld", what, (long long)index, (long long)limit);
        chali_panic(message);
    }
}
static void chali_null(chali_i64 address, const char *what)
{
    if (address == 0)
    {
        char message[128];
        snprintf(message, sizeof(message), "%s：空指针", what);
        chali_panic(message);
    }
}

/* 输出：格式串一个个替换。println 的换行已经由前端写进格式串字面量，
 * 所以这里不需要区分 print 与 println（VM 那一侧也是这么做的）。
 * 前端既可能留 `{}`，也可能已经翻成 printf 的 `%d` / `%g` / `%s`，两种都认。 */
static chali_i64 chali_fmt_at = 0;
static size_t chali_fmt_pos = 0u;
static void chali_fmt_begin(chali_i64 format) { chali_fmt_at = format; chali_fmt_pos = 0u; }

/* 吐出到下一个占位符之前的普通字符，返回占位符的转换字符：
 * `'?'` = `{}`（按值的种类自己挑格式）、`'%'` = 转义的百分号、0 = 后面没有了。
 * 宽度 / 精度 / 标志留在 prefix 里（`%` 开头，不含长度修饰符与转换字符）。 */
static int chali_fmt_scan(char *prefix, size_t prefixSize)
{
    const char *text = (const char *)(chali_mem + chali_fmt_at);
    size_t at = chali_fmt_pos;
    while (text[at] != '\0' && text[at] != '%' && !(text[at] == '{' && text[at + 1u] == '}')) at++;
    if (at > chali_fmt_pos) { fwrite(text + chali_fmt_pos, 1u, at - chali_fmt_pos, stdout); chali_fmt_pos = at; }
    if (text[at] == '\0') { prefix[0] = '\0'; return 0; }
    if (text[at] == '{')
    {
        size_t close = at + 1u;
        while (text[close] != '\0' && text[close] != '}') close++;
        chali_fmt_pos = (text[close] == '}') ? close + 1u : close;
        prefix[0] = '%'; prefix[1] = '\0';
        size_t inner = at + 1u;
        if (text[inner] == ':') inner++;
        const size_t last = close;
        if (last > inner)   /* `{:3F}` / `{:x}`：冒号后面就是 printf 的规格，末字符是转换字符 */
        {
            size_t n = 1u;
            for (size_t scan = inner; scan + 1u < last && n + 2u < prefixSize; ++scan) prefix[n++] = text[scan];
            prefix[n] = '\0';
            return (int)(unsigned char)text[last - 1u];
        }
        return '?';   /* 光秃秃的 `{}`：按值的种类自己挑 */
    }
    size_t scan = at + 1u;
    if (text[scan] == '%')
    {
        fputc('%', stdout);
        chali_fmt_pos = scan + 1u;
        prefix[0] = '\0';
        return '%';
    }
    size_t n = 0u;
    prefix[n++] = '%';
    while (text[scan] != '\0' && strchr("-+ #0123456789.", text[scan]) != NULL && n + 2u < prefixSize) prefix[n++] = text[scan++];
    while (text[scan] != '\0' && strchr("hlLqjzt", text[scan]) != NULL) scan++; /* 长度修饰符丢掉，自己按 64 位补 */
    prefix[n] = '\0';
    if (text[scan] == '\0') { chali_fmt_pos = scan; return 0; }
    chali_fmt_pos = scan + 1u;
    return (int)(unsigned char)text[scan];
}

/* 怎么打印由**格式串里的转换字符**说了算，不看翻译器猜的种类：
 * `%s` 就把这个 64 位当地址去读串，`%f` / `%g` 就把它按 double 的位模式解释。
 * 猜错种类（`float64 fromInt = 2` 那条路上的老坑）不会再把 `%.3f` 喂给整数打印。 */
static void chali_fmt_emit(int conv, const char *prefix, chali_i64 bits, int asFloat)
{
    char spec[64], padded[40];
    if (conv == 's')
    {
        if (bits == 0) return;
        snprintf(spec, sizeof spec, "%s.*s", prefix);
        printf(spec, (int)chali_string_length(bits), chali_string_bytes(bits));
        return;
    }
    if (conv == 'c') { fputc((int)(bits & 0xFF), stdout); return; }
    if (conv == '?') conv = asFloat ? 'g' : 'd';
    if (strchr("gGeEfFaA", conv) != NULL)
    {
        double value = 0.0;
        memcpy(&value, &bits, 8);
        /* 语言的默认精度是三位小数（`native_format.cpp` 里写死的 `.3`）：
         * `%g` 打 3.625 出来的是 `3.62`，不是 C 默认的 `3.625`。 */
        snprintf(padded, sizeof padded, "%s", prefix);
        if (strchr(padded, '.') == NULL) strncat(padded, ".3", sizeof padded - strlen(padded) - 1u);
        snprintf(spec, sizeof spec, "%s%c", padded, (char)conv);
        printf(spec, value);
        return;
    }
    if (conv == 'u' || conv == 'x' || conv == 'X' || conv == 'o')
    {
        snprintf(spec, sizeof spec, "%sll%c", prefix, (char)conv);
        printf(spec, (unsigned long long)(chali_u64)bits);
        return;
    }
    snprintf(spec, sizeof spec, "%slld", prefix);
    printf(spec, (long long)bits);
}
static void chali_fmt_int(chali_i64 value)
{
    char prefix[32];
    const int conv = chali_fmt_scan(prefix, sizeof prefix);
    if (conv == 0 || conv == '%') return;
    chali_fmt_emit(conv, prefix, value, 0);
}
static void chali_fmt_uint(chali_i64 value) { chali_fmt_int(value); }
static void chali_fmt_char(chali_i64 value)
{
    char prefix[32];
    const int conv = chali_fmt_scan(prefix, sizeof prefix);
    if (conv == 0 || conv == '%') return;
    chali_fmt_emit(conv, prefix, value, 0);
}
static void chali_fmt_double(double value)
{
    char prefix[32];
    chali_i64 bits = 0;
    const int conv = chali_fmt_scan(prefix, sizeof prefix);
    if (conv == 0 || conv == '%') return;
    memcpy(&bits, &value, 8);
    chali_fmt_emit(conv, prefix, bits, 1);
}
static void chali_fmt_string(chali_i64 value)
{
    char prefix[32], spec[48];
    const int conv = chali_fmt_scan(prefix, sizeof prefix);
    if (conv == 0 || conv == '%' || value == 0) return;
    const char *bytes = chali_string_bytes(value);
    const int length = (int)chali_string_length(value);
    /* 堆串是「8 字节长度 + 字节」，不一定带结尾 0，所以一律按长度打印。 */
    snprintf(spec, sizeof spec, "%s.*s", prefix);
    printf(spec, length, bytes);
}
static void chali_fmt_finish(void)
{
    const char *text = (const char *)(chali_mem + chali_fmt_at);
    size_t at = chali_fmt_pos;
    /* 没有占位符的格式串也可能带 `%%`，照样要还原成一个 `%`（VM 那侧也是这个语义）。 */
    while (text[at] != '\0')
    {
        if (text[at] == '%' && text[at + 1u] == '%') { fputc('%', stdout); at += 2u; continue; }
        fputc(text[at], stdout);
        at++;
    }
    chali_fmt_pos = at;
}

/* 字符串比较 / 子串 / 查找：与 VM 那侧一样按字节做，串是「8 字节长度 + 字节」。 */
static chali_i64 chali_string_cmp(chali_i64 left, chali_i64 right)
{
    const size_t a = (size_t)(left == 0 ? 0 : chali_string_length(left));
    const size_t b = (size_t)(right == 0 ? 0 : chali_string_length(right));
    const size_t n = a < b ? a : b;
    const int order = n == 0u ? 0 : memcmp(chali_string_bytes(left), chali_string_bytes(right), n);
    if (order != 0) return order < 0 ? -1 : 1;
    return a == b ? 0 : (a < b ? -1 : 1);
}
static chali_i64 chali_string_sub(chali_i64 value, chali_i64 from, chali_i64 count)
{
    if (value == 0) return chali_string_new("", 0u);
    const chali_i64 length = chali_string_length(value);
    chali_i64 start = from < 0 ? 0 : from;
    chali_i64 take = count < 0 ? 0 : count;
    if (start > length) start = length;
    if (take > length - start) take = length - start;
    return chali_string_new(chali_string_bytes(value) + start, (size_t)take);
}
static chali_i64 chali_string_pos(chali_i64 value, chali_i64 needle)
{
    if (value == 0 || needle == 0) return -1;
    const chali_i64 length = chali_string_length(value);
    const chali_i64 wanted = chali_string_length(needle);
    if (wanted == 0) return 0;
    for (chali_i64 at = 0; at + wanted <= length; ++at)
        if (memcmp(chali_string_bytes(value) + at, chali_string_bytes(needle), (size_t)wanted) == 0) return at;
    return -1;
}
/* 输入：一行 / 一个数。这一版只做最简单的版本，够样例跑起来。 */
static chali_i64 chali_read_line(void)
{
    char buffer[4096];
    if (fgets(buffer, (int)sizeof buffer, stdin) == NULL) return chali_string_new("", 0u);
    size_t length = strlen(buffer);
    while (length > 0u && (buffer[length - 1u] == '\n' || buffer[length - 1u] == '\r')) buffer[--length] = '\0';
    return chali_string_new(buffer, length);
}
static chali_i64 chali_read_number(int wantFloat)
{
    char buffer[256];
    if (fgets(buffer, (int)sizeof buffer, stdin) == NULL) return 0;
    if (wantFloat != 0) { double value = 0.0; if (sscanf(buffer, "%lf", &value) != 1) value = 0.0; chali_i64 bits = 0; double copy = value; memcpy(&bits, &copy, 8); return bits; }
    long long value = 0;
    if (sscanf(buffer, "%lld", &value) != 1) value = 0;
    return (chali_i64)value;
}

static chali_i64 chali_string_of_int(chali_i64 value) { char buffer[32]; snprintf(buffer, sizeof(buffer), "%lld", (long long)value); return chali_string_new(buffer, strlen(buffer)); }
static chali_i64 chali_string_of_double(double value) { char buffer[64]; snprintf(buffer, sizeof(buffer), "%g", value); return chali_string_new(buffer, strlen(buffer)); }

/* 位宽收口：与精确执行器一致，按四元式声明的位宽截断 */
#define CHALI_I8(v)  ((chali_i64)(int8_t)(v))
#define CHALI_I16(v) ((chali_i64)(int16_t)(v))
#define CHALI_I32(v) ((chali_i64)(int32_t)(v))
#define CHALI_U8(v)  ((chali_i64)(uint8_t)(v))
#define CHALI_U16(v) ((chali_i64)(uint16_t)(v))
#define CHALI_U32(v) ((chali_i64)(uint32_t)(v))
#define CHALI_F32(v) ((double)(float)(v))
)CHALI_RUNTIME";

/* ------------------------------------------------------------ 类型与形态 */

/**
 * @brief 一个值的种类（决定 C 类型与打印方式）。
 */
enum class Kind
{
    Int,     ///< 整数与地址（都装进 `chali_i64`）。
    Float,   ///< `double`。
    String,  ///< 字符串：值仍是地址，但打印与释放按串处理。
};

/** @brief 类型后缀 → 位宽与有无符号、是否浮点。 */
struct Width
{
    int bits = 64;
    bool unsigned_ = false;
    bool float_ = false;
};

/** @brief 拆 `+:i8` / `cvt:f64` 这种带类型后缀的操作符。 */
void splitOperation(const Memory::String &op, Memory::String &base, Memory::String &suffix)
{
    /* `:=` 不是"带类型后缀的运算"——按 find(':') 切会切出 base="" / suffix="="，
     * 于是赋值目标被当成整数重新标一遍，`float64 fromInt = 2` 这类就丢了浮点种类。 */
    const std::size_t colon = op.find(':');
    if (colon == Memory::String::npos || colon == 0u)
    {
        base = op;
        suffix.clear();
        return;
    }
    base = op.substr(0, colon);
    suffix = op.substr(colon + 1u);
}

/** @brief 这是不是"结果只能是 0/1"的运算（比较、逻辑、按位取反）。
 *
 * 关键在带后缀的比较：`(bool)0.5` 翻出来是 `ne:f64`，后缀是浮点，但结果是整数 0/1。
 * 只按后缀推种类会把 `bool fraction` 变成 `double 1.0`，打印时按 `%d` 读就是
 * 一串位模式（4607182418800017408），VM 那边是整数 1。 */
bool isLogicalOperator(const Memory::String &base)
{
    return base == "eq" || base == "ne" || base == "lt" || base == "le" || base == "gt" || base == "ge" ||
           base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=" ||
           base == "and" || base == "or" || base == "not";
}

/** @brief 类型后缀文本 → 位宽信息。 */
Width widthOf(const Memory::String &suffix)
{
    Width width;
    if (suffix.empty()) return width;
    if (suffix == "f32") { width.bits = 32; width.float_ = true; return width; }
    if (suffix == "f64") { width.bits = 64; width.float_ = true; return width; }
    if (!suffix.empty() && suffix.front() == 'u') width.unsigned_ = true;
    Memory::String digits = suffix;
    if (!digits.empty() && (digits.front() == 'u' || digits.front() == 'i')) digits = digits.substr(1u);
    int value = 64;
    if (!digits.empty())
    {
        value = 0;
        for (const char c : digits)
            if (c >= '0' && c <= '9') value = value * 10 + (c - '0');
    }
    width.bits = value == 0 ? 64 : value;
    return width;
}

/** @brief 位宽收口的 C 表达式（整数）。 */
Memory::String truncateInteger(const Memory::String &expression, const Width &width)
{
    if (width.float_) return "CHALI_F32(" + expression + ")";
    if (width.unsigned_)
    {
        if (width.bits <= 8) return "CHALI_U8(" + expression + ")";
        if (width.bits <= 16) return "CHALI_U16(" + expression + ")";
        if (width.bits <= 32) return "CHALI_U32(" + expression + ")";
        return expression;
    }
    if (width.bits <= 8) return "CHALI_I8(" + expression + ")";
    if (width.bits <= 16) return "CHALI_I16(" + expression + ")";
    if (width.bits <= 32) return "CHALI_I32(" + expression + ")";
    return expression;
}

/** @brief 声明类型名（`decl` 的第一个参数）→ C 类型与种类。 */
void typeOfDeclaration(const Memory::String &name, Memory::String &cType, Kind &kind)
{
    cType = "chali_i64";
    kind = Kind::Int;
    if (name == "f32" || name == "f64") { cType = "double"; kind = Kind::Float; return; }
    if (name == "bool" || name == "i8" || name == "u8" || name == "char") { cType = "chali_i64"; return; }
    if (name == "i16" || name == "u16" || name == "i32" || name == "u32") { cType = "chali_i64"; return; }
}

/* ------------------------------------------------------------ 生成器主体 */

/**
 * @brief 一个 ChaLi 函数（或一段清理块）在 C 里的形态。
 */
struct Function
{
    Memory::String label;                       ///< 四元式标签。
    Memory::String cName;                       ///< 生成的 C 函数名。
    std::size_t begin = 0;                      ///< 起始四元式下标（含标签之前的形参 decl）。
    std::size_t labelIndex = 0;                 ///< 入口 `label` 这条四元式的下标。
    std::size_t end = 0;                        ///< 结束下标（不含）。
    Memory::Vector<Memory::String> parameters;  ///< 形参名（按序）。
    bool exported = false;                      ///< 是否导出给 C。
    bool returnsFloat = false;                  ///< `ret` 出来的是不是浮点。
    bool returnsString = false;                 ///< `ret` 出来的是不是字符串。
    bool defined = false;                       ///< 有没有函数体（abi 外部函数没有）。
};

/**
 * @brief 一次 AOT 翻译的全部中间状态。
 */
struct Translator
{
    const Request &request;
    Memory::Vector<Memory::String> *diagnostics;

    std::vector<Function> functions;
    std::unordered_map<std::string, std::size_t> functionByLabel;  ///< 标签 → functions 下标。
    std::unordered_map<std::string, std::string> variableName;     ///< 四元式名字 → C 变量名。
    std::unordered_map<std::string, std::string> labelName;        ///< 四元式标签 → C 标签。
    std::unordered_map<std::string, Kind> kinds;                   ///< 名字 → 种类。
    std::unordered_map<std::string, std::string> declarations;     ///< 名字 → C 声明（`chali_i64 v_x`）。
    std::unordered_map<std::string, std::string> globals;          ///< 全局量 → C 名字（`g_x`）。
    std::unordered_map<std::string, std::size_t> dataOffsets;      ///< 数据区符号 → 偏移。
    std::set<std::string> externLabels;                            ///< abi 外部函数的标签。
    std::unordered_map<std::string, const ExternFunction *> externsByLabel;
    /* 从 `main`（与 `--export` 点名的函数）出发沿 `call` 边能走到的标签。
     * 标准库里绝大多数段落这份程序根本不会跑，走不到的就不翻译——
     * 生成空壳保留签名，链接完整、语义不变，翻译量却降一个数量级。 */
    std::set<std::string> reachable;

    explicit Translator(const Request &in, Memory::Vector<Memory::String> *out) : request(in), diagnostics(out) {}

    void fail(const Memory::String &message)
    {
        if (diagnostics != nullptr && diagnostics->size() < 32u) diagnostics->push_back(message);
    }

    /** @brief 为四元式名字分配（或复用）一个 C 标识符。 */
    Memory::String identifier(const Memory::String &name, const Memory::String &prefix)
    {
        auto found = variableName.find(std::string(name.c_str()));
        if (found != variableName.end()) return Memory::String(found->second.c_str());
        const Memory::String made = prefix + decimal(static_cast<long long>(variableName.size())) + "_" + sanitize(name);
        variableName.emplace(std::string(name.c_str()), std::string(made.c_str()));
        return made;
    }

    /** @brief 读一个已经登记过的 C 标识符（没有就现分配）。 */
    Memory::String use(const Memory::String &name)
    {
        /* 全局量（decl 的第二个格子写 `global`）是全程序一份，不能当局部形参见谁谁传。 */
        const auto found = globals.find(std::string(name.c_str()));
        if (found != globals.end()) return Memory::String(found->second.c_str());
        return identifier(name, "v");
    }

    /** @brief 一个操作数的 C 表达式：立即数原样写，名字用 C 标识符。 */
    Memory::String operand(const Memory::String &token)
    {
        if (!literalIsName(token)) return token;
        return use(token);
    }

    /** @brief 种类：用于选打印函数与浮点运算。 */
    Kind kindOf(const Memory::String &name)
    {
        const auto found = kinds.find(std::string(name.c_str()));
        return found == kinds.end() ? Kind::Int : found->second;
    }

    void markKind(const Memory::String &name, Kind kind)
    {
        if (name.empty() || !literalIsName(name)) return;
        const auto found = kinds.find(std::string(name.c_str()));
        if (found != kinds.end() && found->second == Kind::String) return;  /* 一旦是串就不再改 */
        kinds[std::string(name.c_str())] = kind;
    }

    /** @brief 标记"这个值是字符串"：串会沿着 `:=` 与形参传递扩散。 */
    void markString(const Memory::String &name)
    {
        if (name.empty() || !literalIsName(name)) return;
        kinds[std::string(name.c_str())] = Kind::String;
    }

    /** @brief 数据区符号的字节偏移（找不到返回 -1）。 */
    std::int64_t offsetOf(const Memory::String &symbol) const
    {
        const auto found = dataOffsets.find(std::string(symbol.c_str()));
        return found == dataOffsets.end() ? -1 : static_cast<std::int64_t>(found->second);
    }
};

/** @brief 解析数据布局文本，填出「符号 → 偏移」表。 */
void loadLayout(Translator &state)
{
    Memory::Vector<stackvm::LayoutRow> rows;
    if (!stackvm::decodeDataLayout(state.request.dataLayout, rows))
    {
        state.fail("AOT：读不懂数据区布局");
        return;
    }
    for (const auto &row : rows) state.dataOffsets.emplace(std::string(row.name.c_str()), row.offset);
}

/** @brief 分函数、登记标签、推断种类。 */
void analyze(Translator &state)
{
    const stackvm::QuadFile *file = state.request.quads;
    if (file == nullptr)
    {
        state.fail("AOT：没有四元式输入");
        return;
    }
    const Memory::Vector<stackvm::Quad> &quads = file->quads;
    for (const auto &function : state.request.externs)
    {
        state.externLabels.insert(std::string(function.label.c_str()));
        state.externsByLabel.emplace(std::string(function.label.c_str()), &function);
    }

    /* 1) 哪些标签是函数入口：被 `call` 点名的，加上 `main`。
     * 同时把所有 `label` 的名字收下来——跳转标签不是变量，别混进形参表。 */
    std::set<std::string> entryLabels{ "main" };
    std::set<std::string> labelNames;
    for (const auto &quad : quads)
    {
        if (quad.op == "call" && !quad.args.empty()) entryLabels.insert(std::string(quad.arg(0).c_str()));
        if (quad.op == "label" && quad.args.size() >= 3u) labelNames.insert(std::string(quad.arg(2).c_str()));
    }
    if (state.request.program != nullptr)
        for (const auto &entry : state.request.program->parameters) entryLabels.insert(std::string(entry.first.c_str()));

    /* 2) 切段：每个函数入口到下一个入口之间是一段。
     * ChaLi 把形参的 `decl` 排在函数标签**之前**（紧跟着上一段的 `ret`），所以新段的起点
     * 要往前吞掉这一串 `decl`；上一段的终点同步回退，别把别人的形参声明抢走。 */
    std::size_t index = 0;
    while (index < quads.size())
    {
        const auto &quad = quads[index];
        if (quad.op != "label" || quad.args.size() < 3u || entryLabels.count(std::string(quad.arg(2).c_str())) == 0u)
        {
            ++index;
            continue;
        }
        std::size_t begin = index;
        while (begin > 0u && quads[begin - 1u].op == "decl") --begin;
        Function function;
        function.label = quad.arg(2);
        function.cName = "f_" + sanitize(function.label);
        function.begin = begin;
        function.labelIndex = index;
        if (!state.functions.empty()) state.functions.back().end = begin;  /* 交出尾巴上的形参 decl */
        std::size_t end = index + 1u;
        while (end < quads.size())
        {
            const auto &next = quads[end];
            if (next.op == "label" && next.args.size() >= 3u && entryLabels.count(std::string(next.arg(2).c_str())) != 0u) break;
            if (next.op == "halt") break;
            ++end;
        }
        function.end = end;
        function.defined = true;
        if (state.request.program != nullptr)
        {
            const auto found = state.request.program->parameters.find(function.label);
            if (found != state.request.program->parameters.end()) function.parameters = found->second;
        }
        state.functionByLabel.emplace(std::string(function.label.c_str()), state.functions.size());
        state.functions.push_back(function);
        index = end;
    }
    /* 2.1 全局量：`decl(<类型>, global, 名字)`。它们全程序一份，
     * 既不进形参表也不在函数里声明局部——`const int64 N = 4000000` 就是这一类。 */
    for (const auto &quad : quads)
    {
        if (quad.op != "decl" || quad.args.size() < 3u) continue;
        if (quad.arg(1) != "global") continue;
        const Memory::String &name = quad.arg(2);
        if (name.empty() || name == "_") continue;
        state.globals.emplace(std::string(name.c_str()), "g_" + sanitize(name));
    }
    /* 2.2 形参表：入口标签之前那串 `decl` 就是形参声明，按出现顺序。
     * 这比"只读不写"的推断准得多——`String.length` 的 `self`、`half` 的 `x$193` 都靠它认出来。 */
    for (auto &function : state.functions)
        for (std::size_t at = function.begin; at < function.labelIndex; ++at)
        {
            const auto &quad = quads[at];
            if (quad.op != "decl" || quad.args.size() < 3u) continue;
            const Memory::String &name = quad.arg(2);
            if (name.empty() || name == "_") continue;
            if (std::find(function.parameters.begin(), function.parameters.end(), name) != function.parameters.end()) continue;
            function.parameters.push_back(name);
        }
    /* 2.5 形参推断：段内只被读、从没被写过的名字就是形参。
     * `program` 给不出参数表时（AOT 直接读 `.quad` 就是这种情况）靠这一遍补齐——
     * 少了它，`__chali_len` 这类形参在产物里就是没声明的标识符。 */
    for (auto &function : state.functions)
    {
        std::set<std::string> written;
        for (std::size_t at = function.begin; at < function.end; ++at)
        {
            const auto &quad = quads[at];
            if (quad.args.size() < 3u) continue;
            const Memory::String &target = quad.arg(2);
            if (!literalIsName(target)) continue;
            if (quad.op == "decl" || quad.op == ":=" || quad.op == "call" || quad.op == "addr" ||
                quad.op == "alloc" || quad.op.rfind("load", 0) == 0u || quad.op.find(':') != Memory::String::npos)
                written.insert(std::string(target.c_str()));
        }
        for (std::size_t at = function.begin; at < function.end; ++at)
        {
            const auto &quad = quads[at];
            /* `decl(i64, local, v)` 的前两格是类型与存储类，`call(f, ...)` 的第一格是被调者，
             * 跳转指令的第一格之外还有标签——它们都不是变量，别混进形参表里。 */
            if (quad.op == "decl" || quad.op == "label") continue;
            for (std::size_t slot = 0u; slot + 1u < quad.args.size() && slot < 2u; ++slot)
            {
                if (slot == 0u && quad.op == "call") continue;
                const Memory::String &name = quad.args[slot];
                if (name.empty() || name == "_" || !literalIsName(name)) continue;
                if (isTypeName(name)) continue;                                   /* 类型名不是变量 */
                if (name.rfind("__temp_", 0) == 0u) continue;                     /* 临时量必然会被写 */
                if (state.dataOffsets.count(std::string(name.c_str())) != 0u) continue;  /* 数据区符号不是变量 */
                if (state.globals.count(std::string(name.c_str())) != 0u) continue;      /* 全局量不是形参 */
                if (entryLabels.count(std::string(name.c_str())) != 0u) continue;  /* 函数标签 */
                if (labelNames.count(std::string(name.c_str())) != 0u) continue;   /* 跳转标签 */
                if (written.count(std::string(name.c_str())) != 0u) continue;
                if (std::find(function.parameters.begin(), function.parameters.end(), name) != function.parameters.end()) continue;
                function.parameters.push_back(name);
            }
        }
    }
    /* 函数入口可能重复出现（同名标签只留第一段）。 */
    for (const auto &name : state.request.options.exports)
    {
        const auto found = state.functionByLabel.find(std::string(name.c_str()));
        if (found != state.functionByLabel.end()) state.functions[found->second].exported = true;
    }
    if (state.functions.empty()) state.fail("AOT：四元式里没有可翻译的函数");

    /* 3) 跳转标签：段内的 label 是本函数的 goto 目标。 */
    for (const auto &function : state.functions)
        for (std::size_t at = function.begin; at < function.end; ++at)
        {
            const auto &quad = quads[at];
            if (quad.op != "label" || quad.args.size() < 3u) continue;
            const Memory::String name = quad.arg(2);
            const Memory::String made = "L_" + sanitize(name);
            state.labelName.emplace(std::string(name.c_str()), std::string(made.c_str()));
        }

    /* 4) 种类推断：先按 decl 与运算目标打底，再沿 `:=` 传播浮点 / 字符串。 */
    for (std::size_t at = 0; at < quads.size(); ++at)
    {
        const auto &quad = quads[at];
        if (quad.op == "decl" && quad.args.size() >= 3u)
        {
            Memory::String cType;
            Kind kind = Kind::Int;
            typeOfDeclaration(quad.arg(0), cType, kind);
            state.markKind(quad.arg(2), kind);
            state.declarations.emplace(std::string(quad.arg(2).c_str()), std::string((cType + " ").c_str()));
        }
        Memory::String base, suffix;
        splitOperation(quad.op, base, suffix);
        if (base == "cvt")
        {
            const Width width = widthOf(suffix);
            if (quad.args.size() >= 3u) state.markKind(quad.arg(2), width.float_ ? Kind::Float : Kind::Int);
            continue;
        }
        if (!suffix.empty() && quad.args.size() >= 3u)
        {
            const Width width = widthOf(suffix);
            state.markKind(quad.arg(2), (width.float_ && !isLogicalOperator(base)) ? Kind::Float : Kind::Int);
        }
        /* 比较 / 逻辑运算即使不带后缀，结果也得是整数（`ne`、`and` 这种）。 */
        if (isLogicalOperator(base) && quad.args.size() >= 3u && state.kindOf(quad.arg(2)) != Kind::String)
            state.markKind(quad.arg(2), Kind::Int);
    }
    /* 4.5 字符串来源：只认**确实返回字符串**的那几个内建。
     * 不能按名字里含 `String` 来猜——`std::lang::String.length` 返回的是整数，
     * `__std_str_len` 也是，猜错就会把长度当字符串打印。 */
    for (std::size_t at = 0; at < quads.size(); ++at)
    {
        const auto &quad = quads[at];
        if (quad.op != "call" || quad.args.size() < 3u) continue;
        const Memory::String &callee = quad.arg(0);
        const Memory::String &target = quad.arg(2);
        if (target.empty() || target == "_" || !literalIsName(target)) continue;
        if (callee == "__std_str_lit" || callee == "__std_str_join" || callee == "__std_str_of_int" ||
            callee == "__std_str_of_flt" || callee == "__std_str_sub" || callee == "__std_str_cut" ||
            callee == "std::lang::__stringCopy" || callee == "std::lang::__stringNew")
            state.markString(target);
    }
    /* 浮点与字符串沿着 `:=`、`load`、函数返回值扩散；顺带重算每个函数的返回种类。
     * 多跑几轮直到稳定——返回种类和调用点的种类是互相喂出来的。 */
    for (int round = 0; round < 6; ++round)
    {
        /* 5) 返回类型：函数体里出现过浮点 / 字符串 `ret` 就按那个种类返回。 */
        for (auto &function : state.functions)
            for (std::size_t at = function.begin; at < function.end; ++at)
            {
                const auto &quad = quads[at];
                if (quad.op != "ret" || quad.args.empty()) continue;
                const Kind kind = state.kindOf(quad.arg(0));
                if (kind == Kind::Float) function.returnsFloat = true;
                if (kind == Kind::String) function.returnsString = true;
            }
        bool changed = false;
        for (std::size_t at = 0; at < quads.size(); ++at)
        {
            const auto &quad = quads[at];
            const Memory::String op = quad.op;
            if (quad.args.size() < 3u) continue;
            const Memory::String &target = quad.arg(2);
            if (target.empty() || target == "_" || !literalIsName(target)) continue;
            /* 调用返回值：被调函数返回什么，接收方就是什么。 */
            if (op == "call")
            {
                const auto found = state.functionByLabel.find(std::string(quad.arg(0).c_str()));
                if (found == state.functionByLabel.end()) continue;
                const Function &callee = state.functions[found->second];
                const Kind kind = callee.returnsString ? Kind::String : (callee.returnsFloat ? Kind::Float : Kind::Int);
                if (kind != Kind::Int && state.kindOf(target) != kind)
                {
                    state.markKind(target, kind);
                    changed = true;
                }
                continue;
            }
            if (op != ":=" && op.rfind("load", 0) != 0u) continue;
            const Memory::String &source = quad.arg(0);
            if (!literalIsName(source)) continue;
            const Kind kind = state.kindOf(source);
            if (kind == Kind::Int) continue;
            if (state.kindOf(target) != kind)
            {
                state.markKind(target, kind);
                changed = true;
            }
        }
        if (!changed) break;
    }

    /* 6) 可达性：从 `main` 与被导出的函数出发沿 `call` 边走一遍。
     * 走不到的段（标准库里绝大多数）不翻译，只留空壳。 */
    Memory::Vector<Memory::String> pending;
    pending.push_back("main");
    for (const auto &function : state.functions)
        if (function.exported) pending.push_back(function.label);
    while (!pending.empty())
    {
        const Memory::String label = pending.back();
        pending.pop_back();
        if (!state.reachable.emplace(std::string(label.c_str())).second) continue;
        const auto found = state.functionByLabel.find(std::string(label.c_str()));
        if (found == state.functionByLabel.end()) continue;
        const Function &function = state.functions[found->second];
        for (std::size_t at = function.begin; at < function.end; ++at)
        {
            const auto &quad = quads[at];
            if (quad.op != "call" || quad.args.empty()) continue;
            const Memory::String target = quad.arg(0);
            if (state.functionByLabel.count(std::string(target.c_str())) == 0u) continue;
            if (state.reachable.count(std::string(target.c_str())) != 0u) continue;
            pending.push_back(target);
        }
    }
}

/**
 * @brief 这一段函数体这份程序跑得到吗（跑得到才值得翻译）。
 */
bool isLive(const Translator &state, const Function &function)
{
    return function.exported || state.reachable.count(std::string(function.label.c_str())) != 0u;
}

/**
 * @brief 内部函数（没有函数体的调用目标）：返回空表示不认识。
 */
struct Builtin
{
    const char *label;
    bool variadic;
};

/** @brief 认得的运行时内建。 */
bool isBuiltin(const Memory::String &label)
{
    static const std::set<std::string> known = {
        "__std_print", "__std_str_lit", "__std_str_join", "__std_str_of_int", "__std_str_of_flt",
        "__std_str_cmp", "__std_str_sub", "__std_str_pos", "__chali_new_array",
        "std::lang::__stringDrop", "std::lang::__stringCopy", "std::lang::__stringNew",
        "__std_read_line", "__std_read_int", "__std_read_float", "__chali_memcpy", "__chali_memset",
    };
    return known.count(std::string(label.c_str())) != 0u;
}

/**
 * @brief 一个已经攒下的实参：四元式里的原样 Token、翻好的 C 表达式、形参名。
 * @note 两个都要留着——C 表达式用来生成调用，原样 Token 用来查种类（`kinds` 按四元式名字索引）。
 */
struct Argument
{
    Memory::String token;
    Memory::String expression;
    Memory::String formal;
};

/**
 * @brief 为一个函数调用生成 C 语句。
 * @param state 翻译状态。
 * @param function 调用所在的（或正在生成的）函数，用于报错。
 * @param callee 被调用标签。
 * @param arguments 已经攒下的实参与形参名（按出现顺序）。
 * @param destination 返回值目的地（可空）。
 * @return C 语句文本；空串表示不支持（已经报错）。
 */
Memory::String emitCall(Translator &state, const Function &function, const Memory::String &callee,
                        const Memory::Vector<Argument> &arguments,
                        const Memory::String &destination)
{
    const bool wantValue = !destination.empty() && destination != "_";
    /* 报错带上所在函数：翻译器报"不支持"时，一眼能看出是主流程还是哪段标准库。 */
    const Memory::String where = "（在 " + function.label + " 里）";
    auto pick = [&](const Memory::String &name) -> Memory::String {
        for (const auto &argument : arguments)
            if (argument.formal == name) return argument.expression;
        return Memory::String();
    };
    /** @brief 取实参在四元式里的原样 Token（查种类用）。 */
    auto tokenOf = [&](const Memory::String &name) -> Memory::String {
        for (const auto &argument : arguments)
            if (argument.formal == name) return argument.token;
        return Memory::String();
    };
    /** @brief 按位置取实参：先认 `__chali_pN`，认不到就按出现顺序取第 N 个。 */
    auto positional = [&](std::size_t index) -> Memory::String {
        const Memory::String named = pick("__chali_p" + decimal(static_cast<long long>(index)));
        if (!named.empty()) return named;
        return index < arguments.size() ? arguments[index].expression : Memory::String();
    };

    /* ---- abi 外部 C 函数：直接生成 extern 调用 ---- */
    if (state.externLabels.count(std::string(callee.c_str())) != 0u)
    {
        const ExternFunction *external = state.externsByLabel.at(std::string(callee.c_str()));
        /* abi 调用点的形参名是 ChaLi 侧写死的（`left` / `times` 这种），不是 `__chali_pN`，
         * 所以按位置兜底——不然实参全填 0，调出来的结果永远是 0。 */
        Memory::String call = external->symbol + "(";
        for (std::size_t index = 0; index < external->parameterTypes.size(); ++index)
        {
            if (index != 0u) call += ", ";
            const Memory::String value = positional(index);
            call += value.empty() ? Memory::String("0") : value;
        }
        call += ")";
        if (!wantValue) return call + ";";
        const bool floating = external->resultType.find("double") != Memory::String::npos ||
                              external->resultType.find("float") != Memory::String::npos;
        if (floating) state.markKind(destination, Kind::Float);
        return state.use(destination) + " = " + (floating ? Memory::String("(double)(") : Memory::String("(chali_i64)(")) + call + ");";
    }

    /* ---- 打印：fmt + fa1..faN ---- */
    if (callee == "__std_print")
    {
        const Memory::String format = pick("__chali_fmt");
        if (format.empty())
        {
            state.fail("AOT：`__std_print` 缺格式串参数");
            return Memory::String();
        }
        Memory::String out = "chali_fmt_begin(" + format + ");";
        for (int index = 1; index < 32; ++index)
        {
            const Memory::String formal = "__chali_fa" + decimal(index);
            const Memory::String value = pick(formal);
            if (value.empty()) break;
            const Memory::String token = tokenOf(formal);
            /* 实参是变量时按它的种类选打印函数；立即数一律按整数。 */
            if (literalIsName(token) && state.kindOf(token) == Kind::Float)
                out += " chali_fmt_double(" + value + ");";
            else if (literalIsName(token) && state.kindOf(token) == Kind::String)
                out += " chali_fmt_string(" + value + ");";
            else if (literalIsFloat(token))
                out += " chali_fmt_double(" + value + ");";
            else
                out += " chali_fmt_int(" + value + ");";
        }
        out += " chali_fmt_finish();";
        return out;
    }
    if (callee == "__std_str_lit")
    {
        const Memory::String value = pick("__chali_addr");
        if (value.empty()) { state.fail("AOT：`__std_str_lit` 缺地址参数"); return Memory::String(); }
        state.markString(destination);
        return state.use(destination) + " = chali_string_literal(" + value + ");";
    }
    if (callee == "__std_str_join")
    {
        if (!wantValue) return Memory::String(";");
        state.markString(destination);
        Memory::String out = "{ char *chali_parts[32]; chali_i64 chali_n = 0; chali_i64 chali_total = 0;";
        Memory::Vector<Memory::String> pieces;
        for (const auto &argument : arguments)
        {
            if (argument.formal.rfind("__chali_p", 0) != 0u) continue;
            pieces.push_back(argument.expression);
        }
        /* 逐个量长度、拷进缓冲区，最后一次成形 */
        out += " char *chali_buffer = NULL;";
        out += " { size_t need = 1; ";
        for (const auto &piece : pieces)
            out += "need += (size_t)chali_string_length(" + piece + "); ";
        out += "chali_buffer = (char *)malloc(need); chali_buffer[0] = '\\0'; }";
        for (const auto &piece : pieces)
            out += " { const char *p = chali_string_bytes(" + piece + "); strcat(chali_buffer, p); }";
        out += " " + state.use(destination) + " = chali_string_new(chali_buffer, strlen(chali_buffer)); free(chali_buffer); }";
        return out;
    }
    if (callee == "__std_str_of_int")
    {
        const Memory::String value = pick("__chali_p0");
        state.markString(destination);
        return state.use(destination) + " = chali_string_of_int(" + (value.empty() ? Memory::String("0") : value) + ");";
    }
    if (callee == "__std_str_of_flt")
    {
        const Memory::String value = pick("__chali_p0");
        state.markString(destination);
        return state.use(destination) + " = chali_string_of_double((double)" + (value.empty() ? Memory::String("0") : value) + ");";
    }
    if (callee == "__chali_new_array")
    {
        if (!wantValue) return Memory::String(";");
        const Memory::String length = pick("__chali_len");
        const Memory::String stride = pick("__chali_stride");
        return state.use(destination) + " = chali_new_array(" + (length.empty() ? Memory::String("0") : length) + ", " +
               (stride.empty() ? Memory::String("8") : stride) + ");";
    }
    if (callee == "std::lang::__stringCopy")
    {
        const Memory::String value = pick("__chali_v");
        if (!wantValue) return Memory::String(";");
        state.markString(destination);
        return state.use(destination) + " = chali_string_copy(" + (value.empty() ? Memory::String("0") : value) + ");";
    }
    if (callee == "std::lang::__stringDrop" || callee == "std::lang::__stringNew")
    {
        const Memory::String value = pick("__chali_v");
        if (!wantValue) return value.empty() ? Memory::String(";") : "chali_string_drop(" + value + ");";
        state.markString(destination);
        return state.use(destination) + " = 0;";
    }
    /* ---- panic：VM 会打消息再退出，这里一样，只是消息走自己的格式化 ---- */
    if (callee == "__std_panic")
    {
        const Memory::String message = positional(0);
        if (message.empty()) return "chali_panic(\"panic\");";
        return "{ chali_fmt_begin(" + message + "); chali_fmt_finish(); chali_panic(\"panic\"); }";
    }
    if (callee == "__std_assert")
    {
        const Memory::String condition = positional(0);
        const Memory::String message = positional(1);
        Memory::String out = "if (!(" + (condition.empty() ? Memory::String("1") : condition) + ")) { ";
        if (!message.empty()) out += "chali_fmt_begin(" + message + "); chali_fmt_finish(); ";
        return out + "chali_panic(\"assert\"); }";
    }
    /* ---- 所有权 / 引用记账：AOT 的整块内存在退出时一起释放，这些记账没有对应动作 ---- */
    if (callee == "__std_move_begin" || callee == "__std_move_end" || callee == "__std_move_require" ||
        callee == "__std_move_live" || callee == "__std_heap_live" || callee == "__std_heap_drop_live" ||
        callee == "__std_function_copy" || callee == "__std_object_copy" || callee == "__std_reference" ||
        callee == "__std_pause" || callee == "__std_program_arguments" || callee == "__std_obj_addr" ||
        callee == "__std_object_free" || callee == "__chali_copy_struct" || callee == "__chali_memcpy" ||
        callee == "__chali_memset" || callee == "__std_memcpy" || callee == "__std_memset")
    {
        const Memory::String value = positional(0);
        if (!wantValue) return value.empty() ? Memory::String(";") : "/* 记账 */ " + value + ";";
        if (callee == "__chali_memcpy" || callee == "__std_memcpy")
        {
            const Memory::String length = positional(2);
            return "memcpy(chali_mem + (" + (positional(0).empty() ? Memory::String("0") : positional(0)) + "), chali_mem + (" +
                   (positional(1).empty() ? Memory::String("0") : positional(1)) + "), (size_t)(" +
                   (length.empty() ? Memory::String("0") : length) + ")); " + state.use(destination) + " = " +
                   (positional(0).empty() ? Memory::String("0") : positional(0)) + ";";
        }
        if (callee == "__chali_memset" || callee == "__std_memset")
        {
            const Memory::String length = positional(2);
            return "memset(chali_mem + (" + (positional(0).empty() ? Memory::String("0") : positional(0)) + "), (int)(" +
                   (positional(1).empty() ? Memory::String("0") : positional(1)) + "), (size_t)(" +
                   (length.empty() ? Memory::String("0") : length) + ")); " + state.use(destination) + " = " +
                   (positional(0).empty() ? Memory::String("0") : positional(0)) + ";";
        }
        return state.use(destination) + " = " + value + ";";
    }
    /* ---- 字符串：长度、比较、取字符、子串、查找 ---- */
    if (callee == "__std_str_len")
    {
        const Memory::String value = positional(0);
        if (!wantValue) return Memory::String(";");
        return state.use(destination) + " = chali_string_length(" + (value.empty() ? Memory::String("0") : value) + ");";
    }
    if (callee == "__std_str_cmp")
    {
        const Memory::String left = positional(0), right = positional(1);
        if (!wantValue) return Memory::String(";");
        return state.use(destination) + " = chali_string_cmp(" + (left.empty() ? Memory::String("0") : left) + ", " +
               (right.empty() ? Memory::String("0") : right) + ");";
    }
    if (callee == "__std_str_at")
    {
        const Memory::String value = positional(0), index = positional(1);
        if (!wantValue) return Memory::String(";");
        return state.use(destination) + " = ((chali_i64)(unsigned char)chali_string_bytes(" +
               (value.empty() ? Memory::String("0") : value) + ")[(size_t)(" + (index.empty() ? Memory::String("0") : index) + ")]);";
    }
    if (callee == "__std_str_set")
    {
        const Memory::String value = positional(0), index = positional(1), character = positional(2);
        return "chali_store_u8(" + (value.empty() ? Memory::String("0") : value) + " + 8 + (" +
               (index.empty() ? Memory::String("0") : index) + "), " + (character.empty() ? Memory::String("0") : character) + ");";
    }
    if (callee == "__std_str_sub")
    {
        const Memory::String value = positional(0), from = positional(1), count = positional(2);
        if (!wantValue) return Memory::String(";");
        state.markString(destination);
        return state.use(destination) + " = chali_string_sub(" + (value.empty() ? Memory::String("0") : value) + ", " +
               (from.empty() ? Memory::String("0") : from) + ", " + (count.empty() ? Memory::String("0") : count) + ");";
    }
    if (callee == "__std_str_pos")
    {
        const Memory::String value = positional(0), needle = positional(1);
        if (!wantValue) return Memory::String(";");
        return state.use(destination) + " = chali_string_pos(" + (value.empty() ? Memory::String("0") : value) + ", " +
               (needle.empty() ? Memory::String("0") : needle) + ");";
    }
    /* ---- 输入：这一版只做最简单的行 / 整数 / 浮点 ---- */
    if (callee == "__std_read_line" || callee == "__std_input_line")
    {
        if (!wantValue) return Memory::String(";");
        state.markString(destination);
        return state.use(destination) + " = chali_read_line();";
    }
    if (callee == "__std_read_int" || callee == "__std_input_int" || callee == "__std_read_float" || callee == "__std_input_float")
    {
        if (!wantValue) return Memory::String(";");
        return state.use(destination) + " = chali_read_number(" +
               Memory::String(callee.find("float") != Memory::String::npos ? "1" : "0") + ");";
    }
    if (callee == "__std_put_char")
    {
        const Memory::String value = positional(0);
        return "fputc((int)(" + (value.empty() ? Memory::String("0") : value) + ") & 0xFF, stdout);";
    }
    if (isBuiltin(callee))
    {
        state.fail("AOT：暂不支持的内部函数 " + callee);
        return Memory::String();
    }

    /* ---- 普通 ChaLi 函数 ---- */
    const auto found = state.functionByLabel.find(std::string(callee.c_str()));
    if (found == state.functionByLabel.end())
    {
        state.fail("AOT：调用目标既不是 ChaLi 函数也不是 abi 声明的外部函数：" + callee + where);
        return Memory::String();
    }
    const Function &target = state.functions[found->second];
    Memory::String call = target.cName + "(";
    for (std::size_t index = 0; index < target.parameters.size(); ++index)
    {
        if (index != 0u) call += ", ";
        /* 先按形参名匹配；对不上就按位置——调用点的形参名（`__chali_s`）与
         * 被调函数体里的名字（`self`）常常不是同一个，按位置才接得上。 */
        Memory::String value = pick(target.parameters[index]);
        if (value.empty() && index < arguments.size()) value = arguments[index].expression;
        call += value.empty() ? Memory::String("0") : value;
    }
    call += ")";
    if (!wantValue) return call + ";";
    /* 被调函数返回浮点时，接收方也得是 double —— 写成 `(chali_i64)` 会把 3.625 截成 3。 */
    if (target.returnsFloat) state.markKind(destination, Kind::Float);
    const bool wantFloat = state.kindOf(destination) == Kind::Float;
    if (wantFloat) state.markKind(destination, Kind::Float);
    return state.use(destination) + " = " + (wantFloat ? Memory::String("(double)(") : Memory::String("(chali_i64)(")) + call + ");";
}

/**
 * @brief 生成一段函数体的 C 语句。
 */
Memory::String emitBody(Translator &state, const Function &function)
{
    const Memory::Vector<stackvm::Quad> &quads = state.request.quads->quads;
    Memory::String out;
    Memory::Vector<Argument> arguments;  /* 攒下来的实参（原样 Token / C 表达式 / 形参名） */
    std::set<std::string> locals;
    /* 段内所有跳转指令的目标。段是按"下一个被调用的入口"切的，标准库里没人调用的函数
     * 会整段跟在后面；`ret` 之后碰到的 label 只有被本段跳到过才算本函数的分支，
     * 没被跳到就说明已经走到下一段了，停手。 */
    std::set<std::string> jumpTargets;
    for (std::size_t at = function.begin; at < function.end; ++at)
    {
        const auto &quad = quads.at(at);
        if ((quad.op == "j" || quad.op == "jz" || quad.op == "jnz") && quad.args.size() >= 3u)
            jumpTargets.insert(std::string(quad.arg(2).c_str()));
    }
    bool closed = false;  ///< 已经翻过一条 `ret`。

    auto flush = [&]() { arguments.clear(); };
    auto declare = [&](const Memory::String &name) {
        if (locals.count(std::string(name.c_str())) != 0u) return;
        /* 形参在函数签名里已经声明过了，这里再声明一次就成了 C 的重复定义 */
        if (std::find(function.parameters.begin(), function.parameters.end(), name) != function.parameters.end()) return;
        if (state.globals.count(std::string(name.c_str())) != 0u) return;  /* 全局量在文件作用域声明 */
        locals.insert(std::string(name.c_str()));
        const auto found = state.declarations.find(std::string(name.c_str()));
        const Memory::String type = found == state.declarations.end() ? Memory::String("chali_i64 ") : Memory::String(found->second.c_str());
        const Kind kind = state.kindOf(name);
        out += "    " + (kind == Kind::Float ? Memory::String("double ") : type) + state.use(name) + " = 0;\n";
    };

    for (std::size_t at = function.begin; at < function.end; ++at)
    {
        const auto &quad = quads.at(at);
        const Memory::String &op = quad.op;
        Memory::String base, suffix;
        splitOperation(op, base, suffix);
        const Memory::String a = quad.arg(0), b = quad.arg(1), c = quad.arg(2);

        if (op == "label" && closed && jumpTargets.count(std::string(c.c_str())) == 0u) break;  /* 已经翻到下一段了 */
        if (op == "ret" || op == "halt") closed = true;

        if (op == "label")
        {
            const auto found = state.labelName.find(std::string(c.c_str()));
            const Memory::String name = found == state.labelName.end() ? Memory::String("L_" + sanitize(c)) : Memory::String(found->second.c_str());
            out += name + ": ;\n";
            continue;
        }
        if (op == "decl")
        {
            declare(c);
            continue;
        }
        if (op == ":=")
        {
            const Memory::String value = literalIsName(a) ? state.use(a) : a;
            if (literalIsName(c)) declare(c);
            out += "    " + state.use(c) + " = " + value + ";\n";
            continue;
        }
        if (op == "param")
        {
            arguments.push_back(Argument{ a, literalIsName(a) ? state.use(a) : a, c });
            continue;
        }
        if (op == "call")
        {
            const Memory::String statement = emitCall(state, function, a, arguments, c);
            flush();
            if (statement.empty()) return Memory::String();
            if (literalIsName(c) && c != "_") declare(c);
            out += "    " + statement + "\n";
            continue;
        }
        if (op == "addr")
        {
            const std::int64_t offset = state.offsetOf(a);
            if (offset < 0)
            {
                state.fail("AOT：数据区里没有符号 " + a);
                return Memory::String();
            }
            if (literalIsName(c)) declare(c);
            out += "    " + state.use(c) + " = " + decimal(offset) + ";\n";
            continue;
        }
        if (op == "j")
        {
            const auto found = state.labelName.find(std::string(c.c_str()));
            if (found == state.labelName.end())
            {
                state.fail("AOT：跳转目标不在本函数内：" + c);
                return Memory::String();
            }
            out += "    goto " + Memory::String(found->second.c_str()) + ";\n";
            continue;
        }
        if (op == "jz" || op == "jnz")
        {
            const auto found = state.labelName.find(std::string(c.c_str()));
            if (found == state.labelName.end())
            {
                state.fail("AOT：跳转目标不在本函数内：" + c);
                return Memory::String();
            }
            const Memory::String condition = literalIsName(a) ? state.use(a) : a;
            out += "    if (" + (op == "jz" ? Memory::String("!") : Memory::String("")) + condition + ") goto " +
                   Memory::String(found->second.c_str()) + ";\n";
            continue;
        }
        if (op == "ret")
        {
            const Memory::String value = a.empty() ? Memory::String("0") : (literalIsName(a) ? state.use(a) : a);
            out += "    return " + value + ";\n";
            continue;
        }
        if (op == "halt")
        {
            out += "    return 0;\n";
            continue;
        }
        if (op == "drop")
        {
            out += "    /* drop：bump 分配器不回收 */\n";
            continue;
        }
        if (op == "check" || op == "checkptr")
        {
            if (!state.request.options.checked) continue;
            out += "    chali_bounds(" + (literalIsName(a) ? state.use(a) : a) + ", " + (literalIsName(b) ? state.use(b) : b) + ", \"下标\");\n";
            continue;
        }
        if (op.rfind("load", 0) == 0u)
        {
            const Width width = widthOf(suffix.empty() ? Memory::String("i64") : suffix);
            const Memory::String address = literalIsName(a) ? state.use(a) : a;
            if (literalIsName(c)) declare(c);
            Memory::String load = "chali_load_i64";
            if (width.float_) load = width.bits <= 32 ? "chali_load_i32" : "chali_load_f64";
            else if (width.bits <= 8) load = "chali_load_u8";
            else if (width.bits <= 16) load = "chali_load_i16";
            else if (width.bits <= 32) load = "chali_load_i32";
            out += "    " + state.use(c) + " = (" + (width.float_ && width.bits > 32 ? Memory::String("double") : Memory::String("chali_i64")) + ")" + load + "(" + address + ");\n";
            continue;
        }
        if (op.rfind("store", 0) == 0u)
        {
            const Width width = widthOf(suffix.empty() ? Memory::String("i64") : suffix);
            const Memory::String address = literalIsName(a) ? state.use(a) : a;
            const Memory::String value = literalIsName(b) ? state.use(b) : b;
            Memory::String store = "chali_store_i64";
            if (width.float_) store = width.bits <= 32 ? "chali_store_i32" : "chali_store_f64";
            else if (width.bits <= 8) store = "chali_store_u8";
            else if (width.bits <= 16) store = "chali_store_i16";
            else if (width.bits <= 32) store = "chali_store_i32";
            out += "    " + store + "(" + address + ", " + value + ");\n";
            continue;
        }
        if (base == "cvt")
        {
            const Width width = widthOf(suffix);
            const Memory::String value = literalIsName(a) ? state.use(a) : a;
            if (literalIsName(c)) declare(c);
            /* 窄整数要真截断 / 符号扩展：`(int8)258` 在 VM 里是 2，写成裸 cast 会漏。
             * 浮点之间只换类型，不动位模式。 */
            const Memory::String converted = width.float_ ? truncateInteger(value, width)
                                                          : truncateInteger(value, width);
            out += "    " + state.use(c) + " = " + (width.float_ ? Memory::String("(double)") : Memory::String("")) +
                   converted + ";\n";
            continue;
        }
        if (base == "neg" || base == "not")
        {
            const Memory::String value = literalIsName(a) ? state.use(a) : a;
            if (literalIsName(c)) declare(c);
            out += "    " + state.use(c) + " = " + (base == "neg" ? Memory::String("-(") : Memory::String("~(")) + value + ");\n";
            continue;
        }
        if (op == "alloc")
        {
            /* 直接在运行时内存里划一块（数组 / 结构体的裸分配）。 */
            const Memory::String size = literalIsName(a) ? state.use(a) : a;
            if (literalIsName(c)) declare(c);
            out += "    " + state.use(c) + " = chali_alloc((size_t)(" + size + "));\n";
            continue;
        }
        /* 二元运算与比较 */
        struct Mapping { const char *name; const char *text; };
        static const Mapping mappings[] = {
            { "add", "+" }, { "+", "+" }, { "sub", "-" }, { "-", "-" }, { "mul", "*" }, { "*", "*" },
            { "div", "/" }, { "/", "/" }, { "mod", "%" }, { "%", "%" },
            { "shl", "<<" }, { "shr", ">>" }, { "shru", ">>" },
            { "band", "&" }, { "bor", "|" }, { "bxor", "^" },
            { "eq", "==" }, { "ne", "!=" }, { "lt", "<" }, { "le", "<=" }, { "gt", ">" }, { "ge", ">=" },
            { "==", "==" }, { "!=", "!=" }, { "<", "<" }, { "<=", "<=" }, { ">", ">" }, { ">=", ">=" },
            { "and", "&&" }, { "or", "||" },
            { "<<", "<<" }, { ">>", ">>" }, { ">>>", ">>" },
            { "&", "&" }, { "|", "|" }, { "^", "^" },
        };
        const char *symbol = nullptr;
        for (const auto &mapping : mappings)
            if (base == mapping.name) { symbol = mapping.text; break; }
        if (symbol != nullptr)
        {
            const Width width = widthOf(suffix.empty() ? Memory::String("i64") : suffix);
            Memory::String left = literalIsName(a) ? state.use(a) : a;
            Memory::String right = literalIsName(b) ? state.use(b) : b;
            const bool floating = width.float_ || state.kindOf(a) == Kind::Float || state.kindOf(b) == Kind::Float ||
                                  (!literalIsName(a) && literalIsFloat(a)) || (!literalIsName(b) && literalIsFloat(b));
            if (literalIsName(c)) declare(c);
            Memory::String expression;
            if (base == "shl" || base == "shr" || base == "<<" || base == ">>")
                expression = "(" + left + ") " + symbol + " ((int)(" + right + ") & 63)";
            else if (base == "shru" || base == ">>>")
                expression = "((chali_u64)(" + left + ") >> ((int)(" + right + ") & 63))";
            else if (width.unsigned_ && !floating && std::string(symbol) != "+" && std::string(symbol) != "-" &&
                     std::string(symbol) != "*" && std::string(symbol) != "/" && std::string(symbol) != "%")
                expression = "((chali_u64)(" + left + ") " + symbol + " (chali_u64)(" + right + "))";
            else
                expression = "(" + left + ") " + symbol + " (" + right + ")";
            if (floating && width.bits <= 32) expression = "CHALI_F32(" + expression + ")";
            else if (!floating && (std::string(symbol) == "+" || std::string(symbol) == "-" || std::string(symbol) == "*" ||
                                   std::string(symbol) == "/" || std::string(symbol) == "%" || std::string(symbol) == "<<" ||
                                   std::string(symbol) == ">>"))
                expression = truncateInteger(expression, width);
            const bool comparison = std::string(symbol) == "==" || std::string(symbol) == "!=" ||
                                    std::string(symbol) == "<" || std::string(symbol) == "<=" ||
                                    std::string(symbol) == ">" || std::string(symbol) == ">=";
            out += "    " + state.use(c) + " = " + (comparison ? Memory::String("(chali_i64)(") : Memory::String("")) +
                   expression + (comparison ? Memory::String(" ? 1 : 0)") : Memory::String("")) + ";\n";
            continue;
        }
        state.fail("AOT：暂不支持的四元式操作符 " + op + "（这一版只翻译算术、控制流、数组与格式化输出）");
        return Memory::String();
    }
    return out;
}

/** @brief 数据区字节数组的 C 文本。 */
Memory::String dataImage(const Memory::String &bytes)
{
    Memory::String out = "static const unsigned char chali_data_image[] = {";
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        if (index % 16u == 0u) out += "\n    ";
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "%d,", static_cast<int>(static_cast<unsigned char>(bytes[index])));
        out += buffer;
    }
    out += "\n};\n";
    return out;
}

/** @brief 前端注入的取 abi 声明的办法（见 `setAbiExternProvider`）。 */
Memory::Vector<ExternFunction> (*g_abiExternProvider)() = nullptr;

}  // namespace

void setAbiExternProvider(Memory::Vector<ExternFunction> (*provider)())
{
    g_abiExternProvider = provider;
}

Memory::Vector<ExternFunction> collectAbiExterns()
{
    return g_abiExternProvider != nullptr ? g_abiExternProvider() : Memory::Vector<ExternFunction>();
}

Memory::String generateSource(const Request &request, Memory::Vector<Memory::String> *diagnostics, Memory::String *header)
{
    Translator state(request, diagnostics);
    if (header != nullptr) header->clear();
    if (request.quads == nullptr)
    {
        state.fail("AOT：没有四元式输入");
        return Memory::String();
    }
    loadLayout(state);
    analyze(state);
    if (diagnostics != nullptr && !diagnostics->empty()) return Memory::String();

    Memory::String out;
    out += "/* 自动生成：ChaLi AOT 产物，请勿手改。\n";
    out += " * 程序：" + request.options.programName + "\n";
    out += " * 运行期检查：" + Memory::String(request.options.checked ? "开（--checked）" : "关（AOT 默认 UNCHECKED）") + "\n";
    out += " * 这份源码自带运行时，不依赖 ChaLi 编译器与虚拟机。\n */\n";
    out += "#define CHALI_DATA_BYTES " + decimal(static_cast<long long>(request.dataBytes.size())) + "\n";
    out += "#define CHALI_HEAP_BYTES " + decimal(static_cast<long long>(request.options.memoryBytes)) + "\n";
    /* 出动态库时 `chali_init` / `chali_finish` 和导出的函数都要真导出，外面的 C 才链接得上。 */
    if (request.options.shared)
        out += "#if defined(_WIN32)\n#define CHALI_AOT_API __declspec(dllexport)\n#else\n#define CHALI_AOT_API __attribute__((visibility(\"default\")))\n#endif\n";
    else
        out += "#define CHALI_AOT_API\n";
    /* 数据映像要写在运行时之前：`chali_init` 里就要 memcpy 它。 */
    out += dataImage(request.dataBytes);
    out += "\n";
    out += kRuntime;
    out += "\n";

    /* 全局量：全程序一份，放在文件作用域。 */
    if (!state.globals.empty())
    {
        out += "/* 全局量（decl 里写 global 的那些） */\n";
        for (const auto &entry : state.globals)
        {
            const Memory::String name(entry.first.c_str());
            out += "static " + (state.kindOf(name) == Kind::Float ? Memory::String("double ") : Memory::String("chali_i64 ")) +
                   Memory::String(entry.second.c_str()) + " = 0;\n";
        }
        out += "\n";
    }

    /* abi 外部函数声明 */
    for (const auto &function : request.externs)
    {
        out += "/* abi " + function.library + " */\n";
        out += "extern " + function.resultType + " " + function.symbol + "(";
        for (std::size_t index = 0; index < function.parameterTypes.size(); ++index)
        {
            if (index != 0u) out += ", ";
            out += function.parameterTypes[index];
        }
        if (function.parameterTypes.empty()) out += "void";
        out += ");\n";
    }
    out += "\n";

    /* 前向声明：被调函数可能写在调用点之后 */
    for (const auto &function : state.functions)
    {
        Memory::String prefix = function.exported ? Memory::String("CHALI_AOT_API ") : Memory::String("static ");
        out += prefix + (function.returnsFloat ? Memory::String("double ") : Memory::String("chali_i64 ")) + function.cName + "(";
        for (std::size_t index = 0; index < function.parameters.size(); ++index)
        {
            if (index != 0u) out += ", ";
            out += (state.kindOf(function.parameters[index]) == Kind::Float ? Memory::String("double ") : Memory::String("chali_i64 ")) +
                   state.use(function.parameters[index]);
        }
        if (function.parameters.empty()) out += "void";
        out += ")" + (function.exported ? Memory::String("") : Memory::String(" __attribute__((unused))")) + ";\n";
    }
    out += "\n";

    /* 函数体：跑不到的段只留空壳（签名照旧，链接完整，但不翻译它里面的东西）。 */
    for (const auto &function : state.functions)
    {
        Memory::String body;
        if (isLive(state, function))
        {
            body = emitBody(state, function);
            if (body.empty() && diagnostics != nullptr && !diagnostics->empty()) return Memory::String();
        }
        else
            body = "    /* 这一段这份程序跑不到，不翻译 */\n";
        Memory::String prefix = function.exported ? Memory::String("CHALI_AOT_API ") : Memory::String("static ");
        out += prefix + (function.returnsFloat ? Memory::String("double ") : Memory::String("chali_i64 ")) + function.cName + "(";
        for (std::size_t index = 0; index < function.parameters.size(); ++index)
        {
            if (index != 0u) out += ", ";
            out += (state.kindOf(function.parameters[index]) == Kind::Float ? Memory::String("double ") : Memory::String("chali_i64 ")) +
                   state.use(function.parameters[index]);
        }
        if (function.parameters.empty()) out += "void";
        out += ")\n{\n" + body + "    return 0;\n}\n\n";
    }

    /* 入口：可执行文件才有 main */
    if (!request.options.shared)
    {
        out += "int main(int argc, char **argv)\n{\n";
        out += "    (void)argc; (void)argv;\n";
        out += "    chali_init();\n";
        out += "    f_" + sanitize(Memory::String("main")) + "();\n";
        out += "    chali_finish();\n";
        out += "    return 0;\n}\n";
    }
    /* 导出头文件：外部 C 靠它调用产物。签名必须和函数体一致——参数丢了就调不进来。
     * 参数在 `state` 里，只有这里拿得到，所以头文件和源码一起出。 */
    if (header != nullptr && (request.options.shared || !request.options.exports.empty()))
    {
        Memory::String text = "/* 自动生成：ChaLi AOT 产物的 C 入口。 */\n#ifndef CHALI_AOT_H\n#define CHALI_AOT_H\n";
        text += "#include <stdint.h>\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
        text += "/* 初始化运行时（数据区 + 堆）；调用任何入口之前先跑一次。 */\nvoid chali_init(void);\n";
        text += "/* 收尾：刷新输出并释放运行时内存。 */\nvoid chali_finish(void);\n";
        for (const auto &function : state.functions)
        {
            if (!function.exported) continue;
            text += (function.returnsFloat ? Memory::String("double ") : Memory::String("int64_t ")) + function.cName + "(";
            for (std::size_t index = 0; index < function.parameters.size(); ++index)
            {
                if (index != 0u) text += ", ";
                text += (state.kindOf(function.parameters[index]) == Kind::Float ? Memory::String("double ") : Memory::String("int64_t ")) +
                        state.use(function.parameters[index]);
            }
            if (function.parameters.empty()) text += "void";
            text += ");\n";
        }
        text += "#ifdef __cplusplus\n}\n#endif\n#endif\n";
        *header = text;
    }
    return out;
}

namespace
{

/** @brief 跑一条命令，返回退出码并收集输出。 */
int runCommand(const Memory::String &command, Memory::String &output)
{
    output.clear();
    FILE *pipe = nullptr;
#ifdef _WIN32
    pipe = _popen(command.c_str(), "r");
#else
    pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) return 127;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
    int status = 0;
#ifdef _WIN32
    status = _pclose(pipe);
#else
    status = pclose(pipe);
#endif
    return status;
}

/** @brief 挑一个能用的 C 编译器。 */
Memory::String pickCompiler(const Memory::String &wanted)
{
    if (!wanted.empty()) return wanted;
    if (const char *fromEnvironment = std::getenv("CHALI_AOT_CC"))
        if (*fromEnvironment != '\0') return Memory::String(fromEnvironment);
    const char *candidates[] = { "gcc", "cc", "clang", "cl" };
    for (const char *candidate : candidates)
    {
        Memory::String probe = Memory::String(candidate) + " --version";
        Memory::String output;
        if (runCommand(probe, output) == 0) return Memory::String(candidate);
    }
    return Memory::String("cc");
}

}  // namespace

int run(const Request &request, Memory::Vector<Memory::String> *diagnostics)
{
    Memory::String header;
    const Memory::String source = generateSource(request, diagnostics, &header);
    if (source.empty()) return 1;
    const Memory::String prefix = request.options.outputPrefix.empty() ? Memory::String("chali_aot") : request.options.outputPrefix;
    const Memory::String cPath = prefix + ".c";
    {
        std::FILE *file = std::fopen(cPath.c_str(), "wb");
        if (file == nullptr)
        {
            if (diagnostics != nullptr) diagnostics->push_back("AOT：写不出 C 源码 " + cPath);
            return 1;
        }
        std::fwrite(source.data(), 1u, source.size(), file);
        std::fclose(file);
    }
    /* 导出头文件：外部 C 靠它调用产物。 */
    if (!header.empty())
    {
        const Memory::String hPath = prefix + ".h";
        std::FILE *file = std::fopen(hPath.c_str(), "wb");
        if (file != nullptr)
        {
            std::fwrite(header.data(), 1u, header.size(), file);
            std::fclose(file);
        }
    }
    if (request.options.emitSourceOnly) return 0;

    const Memory::String compiler = pickCompiler(request.options.compiler);
    Memory::String command = compiler + " -O2 -std=c99 -o \"" + prefix + (request.options.shared ? ".dll\" -shared " : ".exe\" ");
    command += "\"" + cPath + "\"";
    for (const auto &library : request.options.linkLibraries)
        command += " \"" + library + "\"";
    Memory::String output;
    const int status = runCommand(command, output);
    if (status != 0)
    {
        if (diagnostics != nullptr)
        {
            diagnostics->push_back("AOT：C 编译器返回 " + decimal(status));
            Memory::String tail = output;
            if (tail.size() > 2000u) tail = tail.substr(tail.size() - 2000u);
            diagnostics->push_back(tail);
        }
        return 1;
    }
    if (!request.options.keepSource)
    {
        /* 源码默认留着（出问题时能看），只有显式要求才删。 */
    }
    return 0;
}

}  // namespace chali_aot
