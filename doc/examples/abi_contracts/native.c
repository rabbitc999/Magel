#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API
#endif
typedef struct Packet
{
    int32_t value;
    double real;
} Packet;
static int32_t releases = 0;

/**
 * @brief 返回需要 free_owned 释放的字符串。
 * @return 新分配的字符串。
 */
API char *owned_text(void)
{
    char *text = (char *)malloc(6);
    if (text) memcpy(text, "owned", 6);
    return text;
}
/**
 * @brief 返回需要 free_owned 释放的整数缓冲区。
 * @param count 元素数量。
 * @return 新分配的缓冲区，空列表返回空指针。
 */
API int32_t *owned_numbers(int32_t count)
{
    int32_t *values = count > 0 ? (int32_t *)malloc((size_t)count * sizeof(int32_t)) : NULL;
    if (values)
        for (int32_t i = 0; i < count; ++i) values[i] = i + 10;
    return values;
}
/**
 * @brief 返回需要释放的结构体。
 * @return 新分配的结构体。
 */
API Packet *owned_packet(void)
{
    Packet *packet = (Packet *)malloc(sizeof(Packet));
    if (packet)
    {
        packet->value = 42;
        packet->real = 2.5;
    }
    return packet;
}
/**
 * @brief 归还返回值内存并记录释放次数。
 * @param pointer 宿主分配的内存。
 */
API void free_owned(void *pointer)
{
    const char *marker = getenv("MAGEL_ABI_FREE_MARKER");
    ++releases;
    if (marker)
    {
        FILE *file = fopen(marker, "a");
        if (file)
        {
            fputs("free\n", file);
            fclose(file);
        }
    }
    free(pointer);
}
/**
 * @brief 读取已释放的返回值数量。
 * @return 释放次数。
 */
API int32_t released_count(void) { return releases; }
/**
 * @brief 同步调用两个连续输入，验证回调状态。
 * @param callback 当前调用期间有效的回调。
 * @param value 第一个输入。
 * @return 两次回调结果之和。
 */
API int32_t apply(int32_t (*callback)(int32_t), int32_t value)
{
    int32_t first = callback(value);
    return first + callback(value + 1);
}
/**
 * @brief 同时使用两个相同签名的回调。
 * @param left 左回调。
 * @param right 右回调。
 * @param value 输入值。
 * @return 两个结果之和。
 */
API int32_t apply_two(int32_t (*left)(int32_t), int32_t (*right)(int32_t), int32_t value)
{ return left(value) + right(value); }
/**
 * @brief 验证浮点回调调用约定。
 * @param callback 浮点回调。
 * @param value 输入值。
 * @return 回调结果。
 */
API double apply_double(double (*callback)(double), double value) { return callback(value); }
/**
 * @brief 回调执行后仍返回需释放的内存，用于验证错误路径。
 * @param callback 同步回调。
 * @return 需释放的字符串。
 */
API char *callback_owned(int32_t (*callback)(int32_t))
{
    callback(0);
    return owned_text();
}
/**
 * @brief 原样返回整数，供头文件导入示例调用。
 * @param value 输入值。
 * @return 原输入值。
 */
API int32_t identity(int32_t value) { return value; }
/**
 * @brief 原样返回值结构体。
 * @param value 输入结构体。
 * @return 输入副本。
 */
API Packet roundtrip(Packet value) { return value; }
