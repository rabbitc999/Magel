#include <stdint.h>

#if defined(_WIN32)
#define MAGEL_ABI_EXPORT __declspec(dllexport)
#else
#define MAGEL_ABI_EXPORT
#endif

MAGEL_ABI_EXPORT int32_t abi_add(int32_t left, int32_t right)
{
    return left + right;
}

MAGEL_ABI_EXPORT const char *abi_name(void)
{
    return "native";
}
