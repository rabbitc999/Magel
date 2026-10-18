# Magel 宏/块的编辑器语言服务器（`magel-lsp`）：构建 + 自检
#
# 产物在仓库根的 build/magel-lsp：magel-lsp（服务器）与 magel-lsp-tests（单元测试）。
#   make            # 构建
#   make check      # 跑单元测试
#   make clean
#
# 与外面那份文法 LSP（ParseEngine 的 library/lsp）的关系：类型定义与最小 JSON 都是
# **本项目自带的副本**（`lsp/types.hpp`、`../../library/json/`），不跨仓库引用——
#   * `lsp/types.hpp`（位置 / 区间 / 诊断 / 悬停 … 的头文件级共用类型）；
#   * `../../library/json/`（JSON 库的 Magel 内置副本，与 `std::json` 同一份实现）。
# 语言模型各管各的：文法在 ParseEngine 那份，Magel 宏/块在 `mc.hpp` / `mc.cpp`。
# 源码是 UTF-8（中文注释与中文诊断）。GCC 下钉住输入/执行字符集，MSVC 用 /utf-8。

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic
PYTHON   ?= python

ifeq ($(findstring g++,$(CXX)),g++)
UTF8FLAGS := -finput-charset=UTF-8 -fexec-charset=UTF-8
endif

ROOT_DIR    := ../..
LSP_LIB_DIR := .
BUILD_DIR   ?= $(ROOT_DIR)/build/magel-lsp
SERVER      := $(BUILD_DIR)/magel-lsp
TESTS       := $(BUILD_DIR)/magel-lsp-tests

SOURCES := mc.cpp server.cpp ../../library/json/json.cpp
OBJECTS := $(BUILD_DIR)/mc.o $(BUILD_DIR)/server.o $(BUILD_DIR)/json.o
INCLUDES := -I. -I$(LSP_LIB_DIR) -I$(ROOT_DIR)/library -I$(ROOT_DIR)/library/preprocess

# Windows 用 PowerShell 建目录、cmd 清理；其余平台用 POSIX 写法。
ifeq ($(OS),Windows_NT)
BUILD_DIR_WIN := $(subst /,\,$(BUILD_DIR))
MKDIR := powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD_DIR_WIN)' | Out-Null"
CLEAN := cmd /c if exist "$(BUILD_DIR_WIN)" rmdir /s /q "$(BUILD_DIR_WIN)"
else
MKDIR := mkdir -p "$(BUILD_DIR)"
CLEAN := rm -rf "$(BUILD_DIR)"
endif

.PHONY: all server tests check clean

all: server tests

server: $(SERVER)

tests: $(TESTS)

$(SERVER): $(OBJECTS) main.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) main.cpp $(OBJECTS) -o $@

$(TESTS): $(OBJECTS) test_mc.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) test_mc.cpp $(OBJECTS) -o $@

$(BUILD_DIR)/mc.o: mc.cpp mc.hpp $(LSP_LIB_DIR)/types.hpp ../magel_macro.hpp ../magel_macro_controls.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) -c mc.cpp -o $@

$(BUILD_DIR)/server.o: server.cpp server.hpp mc.hpp ../../library/json/json.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) -c server.cpp -o $@

$(BUILD_DIR)/json.o: ../../library/json/json.cpp ../../library/json/json.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) -c ../../library/json/json.cpp -o $@

check: all
	$(TESTS)

$(BUILD_DIR):
	$(MKDIR)

clean:
	$(CLEAN)
