#include <stdint.h>

#if defined(_WIN32)
#define CHALI_ABI_EXPORT __declspec(dllexport)
#else
#define CHALI_ABI_EXPORT
#endif

CHALI_ABI_EXPORT int32_t abi_add(int32_t left, int32_t right)
{
    return left + right;
}

CHALI_ABI_EXPORT const char *abi_name(void)
{
    return "native";
}
