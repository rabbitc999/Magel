# magel-generator：独立编译器工程生成器

`magel gen compiler out build/compiler` 从权威 magel.gram 生成独立编译器工程，复制运行时、胶水、模板引擎和必要源码。`magel clear out build/compiler` 按清单清理，保留用户文件。

生成器优先使用 `ENGINE` 环境变量指定的 ParseEngine；未指定时查找仓库及相邻目录中的可执行文件。调用前展开文法的 `%include`，生成时使用 IELR 与 `std::regex` 词法模式。导出的工程带齐前端语义分片、VM 分片与宿主胶水，默认以 Release 构建，同时编入前端与运行时反射。VM 核心和宿主记录后端使用独立的 CMake 目标。

程序导出仅支持 AOT 与 `.svmb` 编译产物。程序导出使用 `magel aot FILE out EXE` 或 `magel svmb FILE out PREFIX`；生成器不再转发内嵌 VM 导出。

```sh
magel/magel gen compiler out build/compiler
magel/magel aot magel/example/samples/cpp_glue.mgl out build/app --glue magel/source/glue/cpp_glue.cpp
magel/magel svmb magel/example/samples/hello.mgl out build/hello
```

独立工程提供 Magel 前端、解释运行和字节码导出。AOT 使用统一 magel 或独立 magel-emit 工具。标准库源码、宿主运行时资产在构建时内嵌，编译器运行时无需联网。反射能力、非空引用和值语义遵循主工具链配置。
