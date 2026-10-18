# lztext（magel/library/lztext）：构建 + 自检
#
# 与 ParseEngine 的 library/lztext 是同一份库，只是目录层级不同（本仓库放在 `library/` 下），
# 所以构建目录指向仓库根的 build/lztext（相对本目录要回一级）。算法与容器格式两边完全一致。
#
#   make            构建 build/lztext 下的 lztext-tests / lztext-pack，并跑一遍自检
#   make pack       只构建命令行打包工具（tools/embed_stdlib.py 要用它）
#   make check      自检（往返一致性 + 边界用例 + 随机语料模糊测试）
#   make clean
#
# 只有一个编译单元（lztext.cpp），不依赖任何第三方库。
# 源码是 UTF-8（含中文注释）。GCC 下钉住输入/执行字符集。

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

ifeq ($(findstring g++,$(CXX)),g++)
UTF8FLAGS := -finput-charset=UTF-8 -fexec-charset=UTF-8
endif

BUILD_DIR ?= ../../build/lztext

ifeq ($(OS),Windows_NT)
EXE           := .exe
BUILD_DIR_WIN := $(subst /,\,$(BUILD_DIR))
MKDIR         := powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD_DIR_WIN)' | Out-Null"
CLEAN         := cmd /c if exist "$(BUILD_DIR_WIN)" rmdir /s /q "$(BUILD_DIR_WIN)"
else
EXE           :=
MKDIR         := mkdir -p "$(BUILD_DIR)"
CLEAN         := rm -rf "$(BUILD_DIR)"
endif

TESTS_BIN := $(BUILD_DIR)/lztext-tests$(EXE)
PACK_BIN  := $(BUILD_DIR)/lztext-pack$(EXE)

.PHONY: all tests pack check clean

all: tests pack

tests: $(TESTS_BIN)
	$(TESTS_BIN)

pack: $(PACK_BIN)

check: tests

$(TESTS_BIN): test_lztext.cpp lztext.cpp lztext.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) -I. test_lztext.cpp lztext.cpp -o $@

$(PACK_BIN): lztext_pack.cpp lztext.cpp lztext.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) -I. lztext_pack.cpp lztext.cpp -o $@

$(BUILD_DIR):
	$(MKDIR)

clean:
	$(CLEAN)
