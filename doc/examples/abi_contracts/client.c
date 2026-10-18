#include "counter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 从纯 C 客户端验证独立上下文、输出参数和错误隔离。
 * @return 验证通过为 0。
 */
int main(void)
{
    MagelContext *a = magel_create(8 * 1024 * 1024);
    MagelContext *b = magel_create(8 * 1024 * 1024);
    assert(a && b);
    int32_t output = -1;
    assert(magel_add(a, 1, 2, &output) == 0 && output == 11);
    assert(magel_add(b, 1, 2, &output) == 0 && output == 11);
    assert(magel_reset(a, 10) == 0);
    assert(magel_add(a, 0, 0, &output) == 0 && output == 11);
    assert(magel_labelSize(a, &output) == 0 && output == 7);
    double real = 0;
    assert(magel_scale(a, 1.25, &real) == 0 && real == 2.5);
    assert(magel_add(a, 0, 0, NULL) != 0 && strstr(magel_last_error(a), "output"));
    for (int i = 0; i < 100; ++i) assert(magel_labelSize(a, &output) == 0 && output == 7);
    assert(magel_fail(a, &output) != 0 && strstr(magel_last_error(a), "export failure"));
    assert(magel_labelSize(b, &output) == 0 && output == 7);
    magel_destroy(a);
    magel_destroy(b);
    puts("C exports OK");
    return 0;
}
