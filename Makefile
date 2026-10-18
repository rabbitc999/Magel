# Makefile —— Magel 工具链（手写、纳入版本管理，不再由 ParseEngine 覆盖）
#
# 与同目录的 `gen.sh` 分工：
#   * `gen.sh` 只重新生成解析器 / 驱动源码（`magel_parser.*` / `magel_main.cpp`），
#     它给 ParseEngine 传 `--no-build-files`，所以不会覆盖这份 Makefile；
#   * 这份 Makefile 负责「生成代码 → 构建」两段：
#       make gen   用 ParseEngine 从 `magel.gram` 重新生成源码
#       make       编译静态库 libparser.a 与统一工具链 magel
#     生成与构建也可以分开：先 `make gen`，再 `make`。
#
# 目标：
#   make        默认编译静态库 libparser.a 与统一工具链 magel
#   make gen    跑 gen.sh：从 magel.gram 重新生成解析器 / 驱动源码
#   make lib    只编译静态库 libparser.a
#   make demo   只编译统一工具链 magel
#   make run    编译并运行（ARGS 会当 argv 传给 magel）
#   make check  跑 example/samples/*.mgl 与 example/expected/*.txt 的自检
#   make clean  删除中间文件与产物
#
# 覆盖变量示例：
#   make CXX=clang++ CXXFLAGS="-std=c++17 -O0 -g -Wall -Wextra"

# 注意：这份工程用 Magel 自己的 C 分配器（MAGEL_CUSTOM_ALLOCATOR=1，
#       见 local.mk），不是生成模板里那份「标准库内存」的默认值。
# 注意：make 内置定义了 CXX/AR/ARFLAGS，用 ?= 不会生效，这里显式赋值；
# 命令行传参（make CXX=clang++ ...）优先级最高，仍可覆盖。
CXX      = g++
AR       = ar
# Release 口径（默认）：关断言 + 按函数/数据分节，配合链接期裁剪与 strip 减小产物体积。
# 需要调试时覆盖：make CXXFLAGS="-std=c++17 -Wall -Wextra -O0 -g" LDFLAGS=""
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -DNDEBUG -ffunction-sections -fdata-sections
# 链接期：--gc-sections 丢弃未引用的节，-s 去掉符号表（体积主要靠这两项下降）。
LDFLAGS  = -Wl,--gc-sections -s
LDLIBS   = -lm
ARFLAGS  = rcs

# lztext：内嵌标准库 / 运行时资产的压缩存储与运行时解压（见 lztext/README.md）。
# dap：VS Code 断点调试用的 DAP 服务端（见 dap/dap_server.hpp）。
FRONTEND_BUILD := build/magel-frontend
LIB_SRCS  := source/magel_parser.cpp library/lztext/lztext.cpp source/dap/dap_server.cpp
DEMO_SRCS := source/magel_main.cpp
HDRS      := source/magel_parser.hpp source/magel_parser_user.h
LIB_OBJS  := $(FRONTEND_BUILD)/magel_parser.o $(FRONTEND_BUILD)/lztext.o $(FRONTEND_BUILD)/dap_server.o
DEMO_OBJS := $(FRONTEND_BUILD)/magel_main.o

LIB    := $(FRONTEND_BUILD)/libparser.a
TARGET := magel

.PHONY: all lib demo run clean gen check samples reflection

all: $(LIB) $(TARGET)

lib: $(LIB)

demo: $(TARGET)

# 重新生成解析器 / 驱动源码（产物位于 source/ 的 magel_parser.* / magel_main.cpp）。
gen:
	$(SH) gen.sh

$(LIB): $(LIB_OBJS) | $(FRONTEND_BUILD)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET): $(DEMO_OBJS) $(LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(DEMO_OBJS) $(LIB) $(LDLIBS)

$(FRONTEND_BUILD):
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$@' | Out-Null"
else
	mkdir -p $@
endif

$(FRONTEND_BUILD)/%.o: source/%.cpp $(HDRS) | $(FRONTEND_BUILD)
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

$(FRONTEND_BUILD)/lztext.o: library/lztext/lztext.cpp $(HDRS) | $(FRONTEND_BUILD)
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

$(FRONTEND_BUILD)/dap_server.o: source/dap/dap_server.cpp $(HDRS) | $(FRONTEND_BUILD)
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

# 编入注解与反射：前端（MAGEL_WITH_REFLECTION）与运行时（STACKVM_WITH_REFLECTION）
# 默认同时开启，缺一个 `--reflection` 就会报缺宏；本目标保留为显式启用入口。
# 单次源码编译默认启用反射，可用 --no-reflection 关闭；独立板端 VM 按其 Makefile 裁剪。
reflection:
	$(MAKE) MAGEL_WITH_REFLECTION=1 STACKVM_WITH_REFLECTION=1

run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -f $(LIB_OBJS) $(DEMO_OBJS) $(LIB) $(TARGET)

# 挂载点：生成目录里如果有一个 local.mk（可以用 `%copy local.mk` 一起带出来），
# 就在所有默认变量与规则之后被包含进来——工程可以在这里补自己的源文件、
# 头文件搜索路径、链接库与额外目标（例如 `check`）。
include features.mk
-include local.mk

override CXXFLAGS += $(MAGEL_TRIM_FLAGS)
