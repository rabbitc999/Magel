# magel-emit：Magel AOT 导出器

从 4.0.0 起，编译产物只有 AOT 可执行文件与 `.svmb` 字节码。`magel-emit` 只负责 AOT；`.svmb` 由 Magel 前端生成。解释运行继续由 `magel run` 提供。

## 统一入口

在仓库根目录执行，Windows 可使用 magel.exe：

```sh
magel/magel aot magel/example/samples/aot_basic.mgl out build/app opt 0
./build/app
magel/magel svmb magel/example/samples/aot_basic.mgl out build/app-bytecode
```

`smvb` 与 `svmb` 是等价命令，产物扩展名始终为 `.svmb`。字节码需要兼容的 StackVM 和程序实际使用的宿主绑定；AOT 程序需要对应操作系统的运行库和外部动态库。

## C/C++ AOT 导出（`--target cpp-aot`）

每个 Magel 函数在编译期转换成 C++ 函数，控制流使用基本块、局部变量与 goto。原生编译器构建最终程序，执行时不逐条解释程序字节码。复用 Magel 的值语义、引用、接口分派、资源管理与宿主运行时。

```sh
make -C magel/source/magel-emit
magel/magel emit magel/example/samples/aot_basic.mgl out build/app
build/magel-emit/magel-emit --quad build/app.quad --data build/app.data.bin \
  --layout build/app.layout --out build/app-native --opt-level=0
```

独立导出器默认目标为 cpp-aot，可显式写 `--target cpp-aot`。统一入口仅提供 aot 与 svmb 导出，不提供内嵌 VM 导出或对应模板。

## 源码、路径与构建

- `--out FILE` / `--execute-output FILE` 指定最终可执行文件；`--out-dir DIR` 使用输出目录。
- `--cpp-output` 保留 `<可执行文件名>.aot.cpp`；`--cpp-output-dir DIR` 指定源码目录并隐含保留。
- `--opt-level 0|1|2|3|s|fast` 或 `-O2` 传给宿主编译器，默认 O0。
- `--cxx PATH` 指定 C++ 编译器，优先于环境变量 CXX；缺省 g++。
- `--cc PATH` 指定构建配置中的 C 编译器。支持 --g++ / --gxx / --gcc 别名。
- `--make PATH` 生成 `.aot.mk` 再调用 make；默认直接调用宿主编译器。
- `--keep-intermediate` 保留源码、脚本和日志；默认成功后清理。
- `--debug` 保留调试位置和 -g；默认不带调试信息，`--release` 额外剥符号。
- `clear --out-dir DIR` 或 `--clean --out-dir DIR` 按 `.magel-aot-manifest` 清理登记的中间文件，保留最终程序和用户文件。

Windows 的 AOT 仍使用 MinGW 大目标文件选项。大型项目也可按模块拆分编译：

```sh
magel/magel aot magel/example/module-compilation/main.mgl out build/modules \
  modules cache build/module-cache opt 0
```

`--separate-modules [--module-cache DIR]` 生成独立本地目标文件并链接唯一运行时。前端仍统一解析、类型检查和布局，不是 Magel 对象库式独立前端编译。缓存校验源码、编译配置、头依赖及对象内容；首版不支持与 --make 混用。详见[部署说明](../doc/06-deployment.md)。

## 宿主胶水与 ABI

标准库宿主胶水按程序模块裁剪。Magel `abi` 声明生成的绑定自动包含；外部动态库仍须部署。

```sh
magel/magel aot magel/example/samples/cpp_glue.mgl out build/glue-demo \
  --glue magel/source/glue/cpp_glue.cpp
```

应用胶水 `.cpp` 定义 `bindMagelGlue`。签名、非空引用与可空 ABI 映射见[宿主集成与 C ABI](../doc/08-host-integration.md)。

## 运行内存与外部缓冲区

默认运行内存池为 512 MiB；`--runtime-arena BYTES` 修改导出默认值，运行时 `--memory-size BYTES` 可覆盖。使用 C 内存池与显式 STL allocator，不替换全局 new/delete，不引入 GC。

定义 `MAGEL_NO_MAIN` 后可调用 `runMagel`，将调用方缓冲区交给生成程序。调用完成后先销毁本次对象，再清理分配器。运行时返回与错误路径均须遵守所有权；具体宿主接口见[分配器指南](../doc/20-memory-allocator.md)。

## 注解与反射

反射由编译器和 StackVM 能力宏及单次编译开关共同控制；AOT 同步配置。元数据来自已校验声明，导出保留 `magel_reflection_metadata()` 宿主查询和现有 Magel 调用桥接。`.svmb` 仍携带能力位和反射段，加载时校验能力匹配。详见[反射说明](../doc/19-reflection.md)。

## 验证

```sh
python tests/magel_emit.py --magel magel/magel.exe
python tests/magel_web_students.py --magel magel/magel.exe --backends run aot
python tests/magel_web_students.py --magel magel/magel.exe --backends aot --make make
```

导出目标专项验证已删除入口的拒绝、AOT 默认目标、源码保留、脱离仓库的内嵌资产与 `.svmb` 输出。Web 专项使用真实 HTTP/SQLite，验证 CRUD、错误恢复、持久化与退出资源清理。
