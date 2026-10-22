#define CHALI_NO_MAIN
#include CHALI_EXPORTED_SOURCE
#include <cassert>
static unsigned char buffer[32 * 1024 * 1024];
/**
 * @brief 运行本文件的回归测试并报告执行结果。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main()
{
    assert(Memory::defaultRuntimeAllocatorIfCreated() == nullptr);
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        std::memset(buffer, 0xa5, sizeof(buffer));
        std::size_t peak = 0;
        int status = runChaLi(0, nullptr, false, iteration == 2 ? 1 : 1000000, buffer, sizeof(buffer), &peak);
        assert(status == (iteration == 2 ? 1 : 0) && peak > 0);
        for (unsigned char b : buffer) assert(b == 0);
        assert(Memory::defaultRuntimeAllocatorIfCreated() == nullptr);
    }
    std::memset(buffer, 0xa5, 4096);
    std::size_t peak = 0;
    assert(runChaLi(0, nullptr, false, 1000000, buffer, 4096, &peak) == 1);
    for (std::size_t i = 0; i < 4096; ++i) assert(buffer[i] == 0);
    assert(Memory::defaultRuntimeAllocatorIfCreated() == nullptr);
}
