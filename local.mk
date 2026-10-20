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
VM_BUILD := build/magel-stackvm-core
HOST_BUILD := build/magel-host-records
HOST_LIB := $(HOST_BUILD)/librecords.a
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
    -I$(MAGEL_SOURCE_DIR)/allocator -I$(MAGEL_SOURCE_DIR)/lsp \
    -I$(MAGEL_LIBRARY_DIR)/bignum -I$(MAGEL_LIBRARY_DIR)/json -I$(MAGEL_LIBRARY_DIR)/toml \
    -I$(MAGEL_LIBRARY_DIR)/sqlite -I$(MAGEL_LIBRARY_DIR)/console -I$(MAGEL_LIBRARY_DIR)/lztext
override CXXFLAGS += -DMAGEL_CUSTOM_ALLOCATOR=1 -pthread $(MAGEL_INCLUDES) -I$(AST_DIR) -I$(VM_DIR) -I$(PP_DIR)

# ---- 统一工具链：magel 同时是前端 / VM / 工程生成器 ----
# `$(FRONTEND_BUILD)/magel_main.o` 只提供 `magel_main_entry`，真正的 main 在 magel_tool.cpp；
# magel-generator 去掉自己的 main，改成进程内 `runCli`。
override CXXFLAGS += -DMAGEL_FRONT_END_NO_MAIN=1

MAGEL_TOOL_OBJS := $(FRONTEND_BUILD)/magel_tool.o $(FRONTEND_BUILD)/generator_cli.o
DEMO_OBJS += $(MAGEL_TOOL_OBJS)
$(TARGET): $(MAGEL_TOOL_OBJS)

$(MAGEL_TOOL_OBJS): | $(FRONTEND_BUILD)

# 宿主只读取内嵌资产声明，压缩数据与解压缓存在独立编译单元中。
$(FRONTEND_BUILD)/magel_main.o $(FRONTEND_BUILD)/magel_tool.o $(FRONTEND_BUILD)/generator_cli.o: source/magel_module.hpp source/magel_stdlib.hpp

$(FRONTEND_BUILD)/magel_tool.o: source/magel_tool.cpp source/magel_package.hpp tools/abi_import.hpp source/generator/generator_cli.hpp \
        source/magel_runtime_allocator.hpp source/allocator/magel_allocator.hpp
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@

$(FRONTEND_BUILD)/generator_cli.o: source/generator/generator_cli.cpp source/generator/generator_cli.hpp \
        library/template/engine.hpp source/magel_runtime_allocator.hpp
	$(CXX) $(CXXFLAGS) -MMD -MP -DMAGEL_GENERATOR_NO_MAIN=1 -I. -c $< -o $@

# 注意顺序：libparser.a 引用了 simple_ast / stackvm 的符号，必须排在它们前面。
LDLIBS   += $(AST_LIB) $(VM_LIB) $(HOST_LIB) $(PP_LIB)
ifeq ($(shell uname -s),Linux)
LDLIBS   += -ldl
endif

# `%main` / `%code` / `%epilogue` 的 include 会被生成器展开进 .cpp；显式列出原文件，
# 避免只改泛型 / 宏展开头或降级实现时 make 误判为无需重编。
$(FRONTEND_BUILD)/magel_tool.o $(FRONTEND_BUILD)/magel_main.o: source/magel_project.hpp source/magel_cli.hpp

$(FRONTEND_BUILD)/magel_main.o: source/magel_analysis.hpp source/magel_diagnostics.hpp source/magel_stdlib.hpp source/magel_package.hpp source/magel_driver.cpp source/magel_incremental.hpp source/magel_module.hpp source/magel_macro.hpp source/magel_macro_controls.hpp source/magel_lambda.hpp source/magel_generic_closure.hpp source/magel_generic.hpp source/magel_glue.hpp library/sqlite/sqlite3.h $(VM_DIR)/stackvm.hpp $(VM_DIR)/value_text.hpp $(VM_DIR)/reflection.hpp
$(FRONTEND_BUILD)/magel_parser.o: source/magel_ast.hpp source/magel_semantics.cpp source/magel_generic.hpp $(wildcard source/semantics/*.cpp) library/sqlite/sqlite3.h source/magel_glue.hpp $(VM_DIR)/stackvm.hpp $(VM_DIR)/value_text.hpp $(VM_DIR)/reflection.hpp

# 外部静态库更新后必须重链；只把它们写进 LDLIBS 不会让 make 追踪时间戳。
$(TARGET): $(AST_LIB) $(VM_LIB) $(HOST_LIB) $(PP_LIB)

# Magel's VM copy includes the std bindings that are intentionally absent from
# the general-purpose library/stackvm build. Keep its archive separate.
$(VM_BUILD) $(HOST_BUILD):
	$(MAGEL_RUNTIME_MKDIR)

$(VM_BUILD)/stackvm.o: $(VM_DIR)/stackvm.cpp $(wildcard $(VM_DIR)/parts/*) $(VM_DIR)/register_core.hpp $(VM_DIR)/execution_plan.hpp $(VM_DIR)/allocation_index.hpp $(VM_DIR)/data_capacity_index.hpp $(VM_DIR)/stackvm.hpp $(VM_DIR)/value_text.hpp $(VM_DIR)/reflection.hpp | $(VM_BUILD)
	$(CXX) $(CXXFLAGS) -I$(VM_DIR) -c $< -o $@

$(HOST_BUILD)/bigint.o: library/bignum/bigint.cpp library/bignum/bigint.hpp | $(HOST_BUILD)
	$(CXX) $(CXXFLAGS) -I$(MAGEL_LIBRARY_DIR)/bignum -c $< -o $@

$(HOST_BUILD)/bigfloat.o: library/bignum/bigfloat.cpp library/bignum/bigfloat.hpp library/bignum/bigint.hpp | $(HOST_BUILD)
	$(CXX) $(CXXFLAGS) -I$(MAGEL_LIBRARY_DIR)/bignum -c $< -o $@

$(HOST_BUILD)/sqlite3.o: library/sqlite/sqlite3.c library/sqlite/sqlite3.h | $(HOST_BUILD)
	$(CC) -std=c11 -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION=1 -c $< -o $@

$(VM_LIB): $(VM_BUILD)/stackvm.o
	$(AR) $(ARFLAGS) $@ $^

# 每个标准库模块独立编译；实际包含关系由编译器生成 .d 文件追踪。
MAGEL_STANDARD_SRCS := $(filter-out source/glue/std_demo.cpp,$(wildcard source/glue/std_*.cpp))
MAGEL_STANDARD_OBJS := $(patsubst source/glue/%.cpp,$(HOST_BUILD)/%.o,$(MAGEL_STANDARD_SRCS))
$(HOST_BUILD)/std_%.o: source/glue/std_%.cpp | $(HOST_BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@
$(HOST_BUILD)/json.o: library/json/json.cpp | $(HOST_BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@
$(HOST_BUILD)/toml.o: library/toml/toml.cpp | $(HOST_BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@
$(MAGEL_STANDARD_OBJS) $(HOST_BUILD)/json.o $(HOST_BUILD)/toml.o: build/magel-runtime/buildConfig.json
-include $(wildcard $(HOST_BUILD)/*.d)

$(HOST_LIB): $(HOST_BUILD)/sqlite3.o $(HOST_BUILD)/bigint.o $(HOST_BUILD)/bigfloat.o $(MAGEL_STANDARD_OBJS) $(HOST_BUILD)/json.o $(HOST_BUILD)/toml.o
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
$(FRONTEND_BUILD)/magel_main.o $(FRONTEND_BUILD)/magel_parser.o $(VM_BUILD)/stackvm.o $(HOST_BUILD)/bigint.o $(HOST_BUILD)/bigfloat.o: source/magel_runtime_allocator.hpp source/allocator/magel_allocator.hpp source/allocator/magel_pool.c source/allocator/magel_pool.h

.PHONY: config-force
build/magel-runtime/buildConfig.json: config-force
	$(PYTHON) tools/write_build_config.py $@ "$(CXX)" "$(CXXFLAGS)" "$(LDLIBS)" "$(LDFLAGS)"
$(LIB_OBJS) $(DEMO_OBJS) $(VM_BUILD)/stackvm.o $(HOST_BUILD)/bigint.o $(HOST_BUILD)/bigfloat.o build/magel-runtime/simple_ast.o build/magel-runtime/preprocess.o: build/magel-runtime/buildConfig.json

# JSON 与 TOML 实现由宿主库单独编译，包含依赖由 .d 文件维护。

# ---- Magel 宏/块 LSP（source/lsp）：编进编译器本体，不再另发可执行文件 --------------
# `magel lsp --macros` 在**本进程内**跑它（见 magel_tool.cpp 的分派）；共用件
# （最小 JSON 与头文件级共用类型）本仓库自带：`library/json/` 与 `source/lsp/protocol_types.hpp`，不再引用外面。
LSP_DIR := source/lsp
LSP_LIB_DIR := source/lsp
EMBEDDED_LSP_BUILD := build/magel-embedded-lsp

$(EMBEDDED_LSP_BUILD):
	$(MAGEL_RUNTIME_MKDIR)
$(EMBEDDED_LSP_BUILD)/macro_analyzer.o: $(LSP_DIR)/macro_analyzer.cpp $(LSP_DIR)/macro_analyzer.hpp $(LSP_LIB_DIR)/protocol_types.hpp source/magel_macro.hpp source/magel_macro_controls.hpp | $(EMBEDDED_LSP_BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@
$(EMBEDDED_LSP_BUILD)/language_server.o: $(LSP_DIR)/language_server.cpp $(LSP_DIR)/language_server.hpp $(LSP_DIR)/macro_analyzer.hpp library/json/json.hpp | $(EMBEDDED_LSP_BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@
MAGEL_LSP_OBJS := $(EMBEDDED_LSP_BUILD)/macro_analyzer.o $(EMBEDDED_LSP_BUILD)/language_server.o
DEMO_OBJS += $(MAGEL_LSP_OBJS)
$(TARGET): $(MAGEL_LSP_OBJS)

# 静态消息表变更后所有相关翻译单元必须一致，避免旧消息表混入链接。
$(LIB_OBJS) $(DEMO_OBJS) $(VM_BUILD)/stackvm.o: source/magel_i18n.hpp library/console/utf8.hpp

# 单态化符号由前端与宿主胶水共享；更新编码时必须重编相关目标。
$(FRONTEND_BUILD)/magel_main.o $(FRONTEND_BUILD)/magel_parser.o: source/magel_generic_name.hpp
$(FRONTEND_BUILD)/magel_main.o $(FRONTEND_BUILD)/magel_parser.o: source/magel_lambda.hpp source/magel_report.hpp

$(FRONTEND_BUILD)/magel_parser.o $(FRONTEND_BUILD)/magel_main.o: source/magel_borrow_checker.hpp

$(FRONTEND_BUILD)/magel_parser.o $(FRONTEND_BUILD)/magel_main.o: source/magel_borrow_flow.hpp

# 原生语言服务和开发命令。
MAGEL_NATIVE_DEVELOPMENT := build/magel-native-development.o
DEMO_OBJS += $(MAGEL_NATIVE_DEVELOPMENT)
$(TARGET): $(MAGEL_NATIVE_DEVELOPMENT)
build/magel-native:
	$(MAGEL_RUNTIME_MKDIR)
$(MAGEL_NATIVE_DEVELOPMENT): tools/native_development.cpp tools/native_development.hpp source/magel_macro.hpp source/magel_module.hpp source/magel_stdlib.hpp | build/magel-native
	$(CXX) $(CXXFLAGS) -MMD -MP -MF build/magel-native-development.d -I. -c $< -o $@
$(MAGEL_NATIVE_DEVELOPMENT): source/magel_i18n.hpp library/console/utf8.hpp
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

# 泛型与闭包文本处理实现独立编译，解析器和语言服务只依赖声明。
MAGEL_FRONTEND_MODULES := magel_resource_cleanup magel_generic magel_lambda magel_generic_closure magel_macro magel_macro_controls magel_package magel_report
MAGEL_FRONTEND_MODULE_OBJS := $(addprefix $(FRONTEND_BUILD)/,$(addsuffix .o,$(MAGEL_FRONTEND_MODULES)))
LIB_OBJS += $(MAGEL_FRONTEND_MODULE_OBJS)
$(LIB): $(MAGEL_FRONTEND_MODULE_OBJS)
$(MAGEL_FRONTEND_MODULE_OBJS): $(FRONTEND_BUILD)/%.o: source/%.cpp | $(FRONTEND_BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@
$(MAGEL_FRONTEND_MODULE_OBJS): build/magel-runtime/buildConfig.json
-include $(wildcard $(FRONTEND_BUILD)/*.d)

MAGEL_STDLIB_OBJ := $(FRONTEND_BUILD)/magel_stdlib_embedded.o
LIB_OBJS += $(MAGEL_STDLIB_OBJ)
$(LIB): $(MAGEL_STDLIB_OBJ)
$(MAGEL_STDLIB_OBJ): source/magel_stdlib_embedded.cpp source/magel_stdlib.hpp | $(FRONTEND_BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@
$(MAGEL_STDLIB_OBJ): build/magel-runtime/buildConfig.json

-include $(wildcard $(EMBEDDED_LSP_BUILD)/*.d)

# 更新标准库或胶水实现时只刷新单个内嵌资产对象，不重编宿主入口。
MAGEL_EMBED_INPUTS := $(shell $(PYTHON) tools/embed_stdlib.py --dependencies)
MAGEL_EMBED_STAMP := $(FRONTEND_BUILD)/stdlib_assets.stamp
$(MAGEL_EMBED_STAMP): $(MAGEL_EMBED_INPUTS) | $(FRONTEND_BUILD)
	$(PYTHON) tools/embed_stdlib.py --stamp $@
source/magel_stdlib_embedded.cpp: | $(MAGEL_EMBED_STAMP)
	$(PYTHON) tools/embed_stdlib.py
$(MAGEL_STDLIB_OBJ): $(MAGEL_EMBED_STAMP)
