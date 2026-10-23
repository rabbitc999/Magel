# ChaLi 宏/块的编辑器语言服务器（`chali-lsp`）：构建 + 自检
#
# 产物在仓库根的 build/chali-lsp：chali-lsp（服务器）与 chali-lsp-tests（单元测试）。
#   make            # 构建
#   make check      # 跑单元测试
#   make clean
#
# 与外面那份文法 LSP（ParseEngine 的 library/lsp）的关系：类型定义与最小 JSON 都是
# **本项目自带的副本**（`lsp/protocol_types.hpp`、`../../source/json/`），不跨仓库引用——
#   * `lsp/protocol_types.hpp`（位置 / 区间 / 诊断 / 悬停 … 的头文件级共用类型）；
#   * `../../source/json/`（JSON 库的 ChaLi 内置副本，与 `std::json` 同一份实现）。
# 语言模型各管各的：文法在 ParseEngine 那份，ChaLi 宏/块在 `macro_analyzer.hpp` / `macro_analyzer.cpp`。
# 源码是 UTF-8（中文注释与中文诊断）。GCC 下钉住输入/执行字符集，MSVC 用 /utf-8。

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
PYTHON   ?= python

ifeq ($(findstring g++,$(CXX)),g++)
UTF8FLAGS := -finput-charset=UTF-8 -fexec-charset=UTF-8
endif

ROOT_DIR    := ../..
LSP_LIB_DIR := .
BUILD_DIR   ?= $(ROOT_DIR)/build/chali-lsp
ifeq ($(OS),Windows_NT)
EXE_SUFFIX := .exe
endif
SERVER      := $(BUILD_DIR)/chali-lsp$(EXE_SUFFIX)
TESTS       := $(BUILD_DIR)/chali-lsp-tests$(EXE_SUFFIX)

SOURCES := macro_analyzer.cpp language_server.cpp ../../source/json/json.cpp
OBJECTS := $(BUILD_DIR)/macro_signature.o $(BUILD_DIR)/source_text.o  $(BUILD_DIR)/macro_analyzer.o $(BUILD_DIR)/language_server.o $(BUILD_DIR)/json.o
INCLUDES := -I.. -I. -I$(LSP_LIB_DIR) -I$(ROOT_DIR)/source -I$(ROOT_DIR)/source/preprocess

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

$(SERVER): $(OBJECTS) lsp_main.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) lsp_main.cpp $(OBJECTS) -o $@

$(TESTS): $(OBJECTS) macro_analyzer_test.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) macro_analyzer_test.cpp $(OBJECTS) -o $@

$(BUILD_DIR)/macro_analyzer.o: macro_analyzer.cpp macro_analyzer.hpp $(LSP_LIB_DIR)/protocol_types.hpp ../macro/signature.hpp ../syntax/source_text.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) -MMD -MP -c macro_analyzer.cpp -o $@

$(BUILD_DIR)/language_server.o: language_server.cpp language_server.hpp macro_analyzer.hpp ../../source/json/json.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) -MMD -MP -c language_server.cpp -o $@

$(BUILD_DIR)/json.o: ../../source/json/json.cpp ../../source/json/json.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) -MMD -MP -c ../../source/json/json.cpp -o $@

check: all
	$(TESTS)

$(BUILD_DIR):
	$(MKDIR)

clean:
	$(CLEAN)

$(BUILD_DIR)/macro_signature.o: ../macro/signature.cpp ../macro/signature.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) -MMD -MP -c $< -o $@
$(BUILD_DIR)/source_text.o: ../syntax/source_text.cpp ../syntax/source_text.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) $(INCLUDES) -MMD -MP -c $< -o $@
-include $(wildcard $(BUILD_DIR)/*.d)
