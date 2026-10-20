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
