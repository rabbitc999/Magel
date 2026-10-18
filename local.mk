ifeq ($(origin CC),default)
CC := gcc
endif

# local.mk —— 生成工程自己的挂载点（生成的 Makefile 末尾有 `-include local.mk`）。
#
# magel 自带 ast / vm / bignum 三份源码副本，只有前处理仍用仓库里的库：
#   * library/simple_ast：C 子集语法树 → 四元式 + .data（降级目标）
#   * library/stackvm ：四元式 → 栈指令 → 执行（跑出结果；大整数用 library/bignum）
#   * library/preprocess：`%include` / `%if` / `%define` 那一步（源码进解析器之前）
# 三份副本各跑 `make -C library/<名字>`（产物仍落在仓库根的 `build/<名字>/`），
# 所有依赖都使用 MAGEL 内部副本。
#
# 注意：`library/{simple_ast,stackvm,bignum}` 是从 ParseEngine 的 `library/` 复制进来的**副本**，
# 改了这边不会自动同步到 ParseEngine 那份（反之亦然）——动其中一个时想清楚要不要一起改。
#
# 生成源码与报告位于 source/，对象文件与静态库位于 build/magel-frontend/。
# 根目录只保留工具链入口与手写构建文件，重跑 gen.sh 不覆盖 local.mk。

AST_DIR := library/simple_ast
VM_DIR  := library/stackvm
PP_DIR  := library/preprocess
AST_LIB := build/magel-runtime/libsimple_ast.a
VM_BUILD := build/magel-stackvm
VM_LIB  := $(VM_BUILD)/libstackvm.a
PP_LIB  := build/magel-runtime/libpreprocess.a

ifeq ($(OS),Windows_NT)
MAGEL_RUNTIME_MKDIR = powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$@' | Out-Null"
else
MAGEL_RUNTIME_MKDIR = mkdir -p $@
endif

MAGEL_SOURCE_DIR  := source
MAGEL_LIBRARY_DIR := library
# 搬进 source/ 与 library/ 后，代码里的裸文件名 include 靠这组搜索路径解析。
MAGEL_INCLUDES := -I$(MAGEL_SOURCE_DIR) -I$(MAGEL_LIBRARY_DIR) \
    -I$(MAGEL_SOURCE_DIR)/allocator -I$(MAGEL_SOURCE_DIR)/lsp -I$(MAGEL_SOURCE_DIR)/stackvm_mcu \
    -I$(MAGEL_LIBRARY_DIR)/bignum -I$(MAGEL_LIBRARY_DIR)/json -I$(MAGEL_LIBRARY_DIR)/toml \
    -I$(MAGEL_LIBRARY_DIR)/sqlite -I$(MAGEL_LIBRARY_DIR)/console -I$(MAGEL_LIBRARY_DIR)/lztext
override CXXFLAGS += -DMAGEL_CUSTOM_ALLOCATOR=1 -pthread $(MAGEL_INCLUDES) -I$(AST_DIR) -I$(VM_DIR) -I$(PP_DIR)

# ---- 统一工具链：magel 同时是前端 / 导出器 / 工程生成器 ----
# `$(FRONTEND_BUILD)/magel_main.o` 只提供 `magel_main_entry`，真正的 main 在 magel_tool.cpp；
# magel-emit / magel-generator 各自去掉自己的 main，改成进程内 `runCli`。
override CXXFLAGS += -DMAGEL_FRONT_END_NO_MAIN=1

MAGEL_TOOL_OBJS := $(FRONTEND_BUILD)/magel_tool.o $(FRONTEND_BUILD)/emit-main.o $(FRONTEND_BUILD)/emit-aot.o $(FRONTEND_BUILD)/generator-main.o
DEMO_OBJS += $(MAGEL_TOOL_OBJS)
$(TARGET): $(MAGEL_TOOL_OBJS)

$(MAGEL_TOOL_OBJS): | $(FRONTEND_BUILD)

$(FRONTEND_BUILD)/magel_tool.o: source/magel_tool.cpp source/magel_package.hpp tools/abi_import.hpp source/magel-emit/emit_cli.hpp source/magel-generator/generator_cli.hpp \
        source/runtime_allocator.hpp source/allocator/magel_allocator.hpp
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

$(FRONTEND_BUILD)/emit-main.o: source/magel-emit/main.cpp source/magel-emit/module_build.hpp source/magel-emit/emit_cli.hpp source/magel-emit/aot.hpp source/magel_stdlib_embedded.cpp \
        library/template/engine.hpp source/runtime_allocator.hpp $(VM_DIR)/stackvm.hpp $(VM_DIR)/value_text.hpp $(VM_DIR)/reflection.hpp
	$(CXX) $(CXXFLAGS) -DMAGEL_EMIT_NO_MAIN=1 -I. -I$(VM_DIR) -c $< -o $@

$(FRONTEND_BUILD)/emit-aot.o: source/magel-emit/aot.cpp source/magel-emit/aot.hpp source/runtime_allocator.hpp $(VM_DIR)/stackvm.hpp $(VM_DIR)/value_text.hpp $(VM_DIR)/reflection.hpp
	$(CXX) $(CXXFLAGS) -I. -I$(VM_DIR) -c $< -o $@

$(FRONTEND_BUILD)/generator-main.o: source/magel-generator/main.cpp source/magel-generator/generator_cli.hpp \
        library/template/engine.hpp source/runtime_allocator.hpp
	$(CXX) $(CXXFLAGS) -DMAGEL_GENERATOR_NO_MAIN=1 -I. -c $< -o $@

# 注意顺序：libparser.a 引用了 simple_ast / stackvm 的符号，必须排在它们前面。
LDLIBS   += $(AST_LIB) $(VM_LIB) $(PP_LIB)
ifeq ($(shell uname -s),Linux)
LDLIBS   += -ldl
endif

# `%main` / `%code` / `%epilogue` 的 include 会被生成器展开进 .cpp；显式列出原文件，
# 避免只改泛型 / 宏展开头或降级实现时 make 误判为无需重编。
$(FRONTEND_BUILD)/magel_tool.o $(FRONTEND_BUILD)/magel_main.o: source/magel_project.hpp source/magel_cli.hpp

$(FRONTEND_BUILD)/magel_main.o: source/magel_analysis.hpp source/magel_diagnostics.hpp source/magel_stdlib_embedded.cpp source/magel_package.hpp source/magel_driver.cpp source/magel_incremental.hpp source/magel_module.hpp source/magel_macro.hpp source/magel_macro_controls.hpp source/magel_lambda.hpp source/magel_generic.hpp source/magel_glue.hpp $(wildcard source/glue/*.hpp) library/sqlite/sqlite3.h $(VM_DIR)/stackvm.hpp $(VM_DIR)/value_text.hpp $(VM_DIR)/reflection.hpp
$(FRONTEND_BUILD)/magel_parser.o: source/magel_types.h source/magel_epilogue.cpp source/magel_generic.hpp $(wildcard source/epilogue/*.cpp) library/sqlite/sqlite3.h source/magel_glue.hpp $(VM_DIR)/stackvm.hpp $(VM_DIR)/value_text.hpp $(VM_DIR)/reflection.hpp

# 外部静态库更新后必须重链；只把它们写进 LDLIBS 不会让 make 追踪时间戳。
$(TARGET): $(AST_LIB) $(VM_LIB) $(PP_LIB)

# Magel's VM copy includes the std bindings that are intentionally absent from
# the general-purpose library/stackvm build. Keep its archive separate.
$(VM_BUILD):
	$(MAGEL_RUNTIME_MKDIR)

$(VM_BUILD)/stackvm.o: $(VM_DIR)/stackvm.cpp $(wildcard $(VM_DIR)/parts/*) $(VM_DIR)/register_core.hpp $(VM_DIR)/execution_plan.hpp $(VM_DIR)/allocation_index.hpp $(VM_DIR)/data_capacity_index.hpp $(VM_DIR)/stackvm.hpp $(VM_DIR)/value_text.hpp $(VM_DIR)/reflection.hpp $(VM_DIR)/stackvm_glue.hpp | $(VM_BUILD)
	$(CXX) $(CXXFLAGS) -DSTACKVM_WITH_BIGINT=1 -DSTACKVM_WITH_BIGFLOAT=1 -I$(VM_DIR) -I$(MAGEL_LIBRARY_DIR)/bignum -c $< -o $@

$(VM_BUILD)/stackvm_glue.o: $(VM_DIR)/stackvm_glue.cpp $(VM_DIR)/stackvm_glue.hpp | $(VM_BUILD)
	$(CXX) $(CXXFLAGS) -I$(VM_DIR) -c $< -o $@

$(VM_BUILD)/bigint.o: library/bignum/bigint.cpp library/bignum/bigint.hpp | $(VM_BUILD)
	$(CXX) $(CXXFLAGS) -I$(MAGEL_LIBRARY_DIR)/bignum -c $< -o $@

$(VM_BUILD)/bigfloat.o: library/bignum/bigfloat.cpp library/bignum/bigfloat.hpp library/bignum/bigint.hpp | $(VM_BUILD)
	$(CXX) $(CXXFLAGS) -I$(MAGEL_LIBRARY_DIR)/bignum -c $< -o $@

$(VM_BUILD)/sqlite3.o: library/sqlite/sqlite3.c library/sqlite/sqlite3.h | $(VM_BUILD)
	$(CC) -std=c11 -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION=1 -c $< -o $@

$(VM_LIB): $(VM_BUILD)/sqlite3.o $(VM_BUILD)/stackvm.o $(VM_BUILD)/stackvm_glue.o $(VM_BUILD)/bigint.o $(VM_BUILD)/bigfloat.o
	$(AR) $(ARFLAGS) $@ $^

.PHONY: check samples

# Windows 上 make 找不到 sh：优先用 Git Bash / MSYS2 的 bash 跑 check.sh（与 project/mini-basic 的写法一致）。
ifeq ($(OS),Windows_NT)
  SH ?= $(firstword $(wildcard C:/PROGRA~1/Git/bin/bash.exe) \
                    $(wildcard C:/PROGRA~2/Git/bin/bash.exe) \
                    $(wildcard C:/msys64/usr/bin/bash.exe) sh)
else
  SH ?= sh
endif

# 自检：跑 example/samples/*.mgl，逐个比对 example/expected/*.txt。
check: $(TARGET)
	$(SH) check.sh

# 只跑样例、不重新编译（改样例时用）。
samples: $(TARGET)
	$(SH) check.sh

# Keep allocator-specialized dependencies separate from the generic libraries.
build/magel-runtime:
	$(MAGEL_RUNTIME_MKDIR)
build/magel-runtime/simple_ast.o: $(AST_DIR)/simple_ast.cpp $(AST_DIR)/simple_ast.hpp source/allocator/magel_allocator.hpp source/allocator/magel_pool.c | build/magel-runtime
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/magel-runtime/preprocess.o: $(PP_DIR)/preprocess.cpp $(PP_DIR)/preprocess.hpp source/allocator/magel_allocator.hpp source/allocator/magel_pool.c | build/magel-runtime
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(AST_LIB): build/magel-runtime/simple_ast.o
	$(AR) $(ARFLAGS) $@ $^
$(PP_LIB): build/magel-runtime/preprocess.o
	$(AR) $(ARFLAGS) $@ $^
$(FRONTEND_BUILD)/magel_main.o $(FRONTEND_BUILD)/magel_parser.o $(VM_BUILD)/stackvm.o $(VM_BUILD)/stackvm_glue.o $(VM_BUILD)/bigint.o $(VM_BUILD)/bigfloat.o: source/runtime_allocator.hpp source/allocator/magel_allocator.hpp source/allocator/magel_pool.c source/allocator/magel_pool.h

.PHONY: config-force
build/magel-runtime/buildConfig.json: config-force
	$(PYTHON) tools/write_build_config.py $@ "$(CXX)" "$(CXXFLAGS)" "$(LDLIBS)"
$(LIB_OBJS) $(DEMO_OBJS) $(VM_BUILD)/stackvm.o $(VM_BUILD)/stackvm_glue.o $(VM_BUILD)/bigint.o $(VM_BUILD)/bigfloat.o build/magel-runtime/simple_ast.o build/magel-runtime/preprocess.o: build/magel-runtime/buildConfig.json

# 结构化数据解析器直接编入胶水翻译单元。
$(FRONTEND_BUILD)/magel_main.o: $(wildcard library/json/*.hpp library/json/*.cpp library/toml/*.hpp library/toml/*.cpp)

# ---- Magel 宏/块 LSP（source/lsp）：编进编译器本体，不再另发可执行文件 --------------
# `magel lsp --macros` 在**本进程内**跑它（见 magel_tool.cpp 的分派）；共用件
# （最小 JSON 与头文件级共用类型）本仓库自带：`library/json/` 与 `source/lsp/types.hpp`，不再引用外面。
LSP_DIR := source/lsp
LSP_LIB_DIR := source/lsp
EMBEDDED_LSP_BUILD := build/magel-embedded-lsp

$(EMBEDDED_LSP_BUILD):
	$(MAGEL_RUNTIME_MKDIR)
$(EMBEDDED_LSP_BUILD)/mc.o: $(LSP_DIR)/mc.cpp $(LSP_DIR)/mc.hpp $(LSP_LIB_DIR)/types.hpp source/magel_macro.hpp source/magel_macro_controls.hpp | $(EMBEDDED_LSP_BUILD)
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@
$(EMBEDDED_LSP_BUILD)/server.o: $(LSP_DIR)/server.cpp $(LSP_DIR)/server.hpp $(LSP_DIR)/mc.hpp library/json/json.hpp | $(EMBEDDED_LSP_BUILD)
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@
MAGEL_LSP_OBJS := $(EMBEDDED_LSP_BUILD)/mc.o $(EMBEDDED_LSP_BUILD)/server.o
DEMO_OBJS += $(MAGEL_LSP_OBJS)
$(TARGET): $(MAGEL_LSP_OBJS)

# 静态消息表变更后所有相关翻译单元必须一致，避免旧消息表混入链接。
$(LIB_OBJS) $(DEMO_OBJS) $(VM_BUILD)/stackvm.o $(VM_BUILD)/stackvm_glue.o: source/magel_i18n.hpp library/console/utf8.hpp

# 单态化符号由前端与宿主胶水共享；更新编码时必须重编相关目标。
$(FRONTEND_BUILD)/magel_main.o $(FRONTEND_BUILD)/magel_parser.o build/magel-embedded-lsp/mc.o build/magel-embedded-lsp/server.o: source/magel_generic_name.hpp
$(EMBEDDED_LSP_BUILD)/mc.o $(EMBEDDED_LSP_BUILD)/server.o: source/magel_generic.hpp
$(FRONTEND_BUILD)/magel_main.o $(FRONTEND_BUILD)/magel_parser.o $(EMBEDDED_LSP_BUILD)/mc.o $(EMBEDDED_LSP_BUILD)/server.o: source/magel_lambda.hpp source/magel_report.hpp

$(FRONTEND_BUILD)/magel_parser.o $(FRONTEND_BUILD)/magel_main.o: source/magel_borrow_checker.hpp

$(FRONTEND_BUILD)/magel_parser.o $(FRONTEND_BUILD)/magel_main.o: source/magel_borrow_flow.hpp

# 原生语言服务和开发命令。
MAGEL_NATIVE_DEVELOPMENT := build/magel-native-development.o
DEMO_OBJS += $(MAGEL_NATIVE_DEVELOPMENT)
$(TARGET): $(MAGEL_NATIVE_DEVELOPMENT)
build/magel-native:
	$(MAGEL_RUNTIME_MKDIR)
$(MAGEL_NATIVE_DEVELOPMENT): tools/native_development.cpp tools/native_development.hpp source/magel_macro.hpp source/magel_module.hpp source/magel_stdlib_embedded.cpp | build/magel-native
	$(CXX) $(CXXFLAGS) -MMD -MP -MF build/magel-native-development.d -I. -c $< -o $@
$(FRONTEND_BUILD)/magel_tool.o: tools/native_development.hpp

# 内置 ZIP/Gzip 的 Deflate 解压器；仅构建解压部分。
MAGEL_MINIZ := build/magel-miniz-tinfl.o
DEMO_OBJS += $(MAGEL_MINIZ)
$(TARGET): $(MAGEL_MINIZ)
$(MAGEL_MINIZ): library/miniz/miniz_tinfl.c $(wildcard library/miniz/*.h) | build/magel-native
	$(CC) -O2 -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_DEFLATE_APIS -c $< -o $@
ifeq ($(OS),Windows_NT)
LDLIBS += -lwinhttp
endif

-include build/magel-native-development.d
