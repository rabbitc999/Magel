# 09 工具链与命令行

Magel 只通过共享 VM 执行程序。源码经过前处理、语法与语义检查，再降级为四元式和 VM 指令；支持导出四元式与 `.svmb` 字节码。

## 执行与检查

```sh
magel run app.mgl
magel check app.mgl
magel check app.mgl json
magel emit app.mgl out build/app
magel svmb app.mgl out build/app
```

直接传入 `.mgl` 文件也会运行，文件后的参数传给程序；显式 `run` 可配置产物前缀。
`emit` 输出 `.quad`、`.data.bin` 和 `.layout`；`svmb` 还输出 VM 字节码。运行时资产与标准库内嵌在编译器中。

## 开发工具

```sh
magel new demo name demo
magel test tests backend interpret
magel fmt check src
magel deps lock project . offline
magel man List
magel lsp
magel gen compiler out build/compiler
magel clear out build/compiler
```

生成器用于独立编译器工程；宿主集成通过共享 VM、宿主注册表和字节码加载接口完成。
可恢复的运行失败、调用栈及宿主错误按 VM 的统一协议传播。

## 默认构建与优化

前端及运行时默认同时编入反射，单次编译以 `--no-reflection` 关闭。四元式默认执行完整优化流程；VM 默认启用最高可用执行优化。
`--memory-size` 与 `--step-limit` 设置 VM 资源预算；`--threads` 启用隔离线程。编译器帮助列出当前支持的选项。

项目配置的运行方式为 `interpret` 与 `svmb`。
配置见[工程与包管理](13-projects.md)，字节码约定见[SVMB 格式](15-svmb-format.md)。

## 源码模块与增量构建

`source/` 的 C/C++ 文件按模块职责使用下划线名称；类、成员和函数保持既有命名。
标准库胶水统一位于 `source/glue/std_xxx.{hpp,cpp}`：公共头暴露绑定入口，平台适配、记录布局和
算法实现在对应 `.cpp` 独立编译。`std_binding.hpp` 只提供注册表前置声明；模板需要的
结构化数据与类型转换定义保留在头文件中。VM 核心继续使用通用宿主协议。

泛型、闭包、工程清单和诊断分别由 `magel_generic.cpp`、`magel_lambda.cpp`、
`magel_generic_closure.cpp`、`magel_package.cpp` 和 `magel_report.cpp` 编译。
`macro/` 独立编译卫生绑定、结构化替换、宏注册、正则匹配、原始片段和编辑器宏头模型；
`syntax/` 提供普通源码分组及常量求值，`editor/` 输出真实编译分析。
AST 声明位于 `magel_ast.hpp`；`semantics/` 中的符号登记、卫生展开、泛型闭包和程序降级
由权威文法装配进生成解析器。语言服务位于 `lsp/`，工程生成器位于 `generator/`。

修改文法后执行 `sh gen.sh`，然后执行 `make -j4`。普通 C++ 实现变化由 `.d` 文件追踪实际包含关系。
`make` 自动检查标准库、胶水及手册的内嵌输入，内容由 `tools/embed_stdlib.py` 生成并单独编译为
`magel_stdlib_embedded.o`；压缩数据不再被公共头包含。生成输入使用时间戳记录，内容未变化时不重复写入
生成源码。独立导出的编译器工程也将这些实现作为单独的 CMake 源文件链接。

宿主接入胶水时同时包含 `magel_glue.hpp` 与所需 `std_xxx.hpp`，并链接对应 `.cpp` 或宿主记录库。
只有绑定接口的头文件不会隐式带入全部实现；完整构建命令见[同步回调示例](../example/native-callbacks/README.md)。
