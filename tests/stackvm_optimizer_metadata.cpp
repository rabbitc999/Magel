#include "../source/stackvm/stackvm.hpp"
#include <cassert>

/**
 * @brief 验证常量传播保留参数声明元数据，不把名为 local 的变量代入存储类别。
 * @return 成功返回零。
 */
int main()
{
    auto file = stackvm::parseQuads(
        "(:=, 0, _, local)\n(ret, 0, _, _)\n"
        "(decl, i64, local, parameter)\n(label, _, _, callee)\n"
        "(ret, parameter, _, _)\n",
        "metadata-test");
    assert(file.ok());
    const auto report = stackvm::optimize(file);
    assert(report.rounds > 0);
    bool found = false;
    for (const auto &quad : file.quads)
        if (quad.op == "decl")
        {
            assert(quad.arg(0) == "i64");
            assert(quad.arg(1) == "local");
            assert(quad.arg(2) == "parameter");
            found = true;
        }
    assert(found);
    return 0;
}
