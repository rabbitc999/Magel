# magel/source/magel-emit/Makefile —— AOT 导出器
#
#   make            编译 build/magel-emit/magel-emit
#   make run FILE=<前缀>   拿一份 .quad（+ .data.bin / .layout）生成 + 编成可执行文件
#   make clean      删掉产物
#
# 生成器链接 stackvm + big_number，在生成期解析、化简和组装四元式，
# 再把 MAGEL 函数翻译成原生 C++ 基本块并编译。

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
include ../../features.mk
override CXXFLAGS += $(MAGEL_TRIM_FLAGS)
ROOT_DIR := ../..
BUILD_DIR ?= $(ROOT_DIR)/build/magel-emit
TARGET   := $(BUILD_DIR)/magel-emit

ifeq ($(findstring g++,$(CXX)),g++)
UTF8FLAGS := -finput-charset=UTF-8 -fexec-charset=UTF-8
endif

ifeq ($(OS),Windows_NT)
BUILD_DIR_WIN := $(subst /,\,$(BUILD_DIR))
MKDIR := powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD_DIR_WIN)' | Out-Null"
CLEAN := cmd /c if exist "$(BUILD_DIR_WIN)" rmdir /s /q "$(BUILD_DIR_WIN)"
else
MKDIR := mkdir -p "$(BUILD_DIR)"
CLEAN := rm -rf "$(BUILD_DIR)"
endif

.PHONY: all run clean

all: $(TARGET)

VM_DIR := ../../library/stackvm
BIGINT_DIR := $(ROOT_DIR)/library/bignum

$(TARGET): ../magel_i18n.hpp ../../library/console/utf8.hpp main.cpp module_build.hpp aot.cpp aot.hpp ../magel_stdlib_embedded.cpp ../allocator/magel_allocator.hpp ../allocator/magel_pool.c ../allocator/magel_pool.h \
        ../magel_glue.hpp $(wildcard ../../library/json/*.hpp ../../library/json/*.cpp ../../library/toml/*.hpp ../../library/toml/*.cpp) ../glue/structured_data.hpp ../glue/std_json.hpp ../glue/std_toml.hpp ../glue/std_math.hpp ../glue/std_random.hpp ../glue/std_regex.hpp ../glue/lang_numeric.hpp ../glue/std_binary.hpp ../glue/std_parse.hpp ../glue/std_process.hpp ../glue/std_glue.cpp ../glue/cpp_glue.cpp ../runtime_allocator.hpp \
        $(VM_DIR)/stackvm.cpp $(wildcard $(VM_DIR)/parts/*) $(VM_DIR)/register_core.hpp $(VM_DIR)/execution_plan.hpp $(VM_DIR)/allocation_index.hpp $(VM_DIR)/data_capacity_index.hpp $(VM_DIR)/stackvm.hpp $(VM_DIR)/reflection.hpp \
		$(VM_DIR)/stackvm_glue.cpp $(VM_DIR)/stackvm_glue.hpp \
		$(BIGINT_DIR)/bigint.cpp $(BIGINT_DIR)/bigint.hpp \
		$(BIGINT_DIR)/bigfloat.cpp $(BIGINT_DIR)/bigfloat.hpp \
		../../library/lztext/lztext.cpp ../../library/lztext/lztext.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(UTF8FLAGS) -DMAGEL_CUSTOM_ALLOCATOR=1 -pthread -I$(ROOT_DIR)/source -I$(ROOT_DIR)/library -I$(VM_DIR) -I$(BIGINT_DIR) \
		main.cpp aot.cpp $(VM_DIR)/stackvm.cpp $(VM_DIR)/stackvm_glue.cpp \
		$(BIGINT_DIR)/bigint.cpp $(BIGINT_DIR)/bigfloat.cpp \
		../../library/lztext/lztext.cpp -o $@

# `make run FILE=tmp/x`：生成 + 编译（用 build-target.sh 一条龙）
run: $(TARGET)
	$(MAKE) $(MAKEFLAGS) -s build-target FILE=$(FILE)

clean:
	$(CLEAN)

$(BUILD_DIR):
	$(MKDIR)

.PHONY: config-force
$(BUILD_DIR)/buildConfig.json: config-force
	$(PYTHON) ../../tools/write_build_config.py $@ "$(CXX)" "$(CXXFLAGS)"
$(TARGET): $(BUILD_DIR)/buildConfig.json
