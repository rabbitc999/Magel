# mcu_vm —— 板端（MCU）用的一份 **异常自由** 的 Magel 字节码 VM。
#
# 与 `library/stackvm` 的关系：同一份 VM 源码（`../../library/stackvm/stackvm.cpp`），
# 这里只是把它**按板端口径**编译：
#   * `-fno-exceptions`：`try/catch/throw` 在这个目标里直接编译不过（这就是"锁死"）；
#   * `-STACKVM_WITH_DEFAULT_HEAP=0`：不编"靠 STL 扩容的默认字节堆"，必须注入静态池；
#   * `-STACKVM_WITH_HOST_EXCEPTION_GUARD=0`：契约改成"宿主回调不许抛异常"；
#   * `-STACKVM_WITH_HOST_IO=0`：整段不碰 `<fstream>`，只能用内存映像加载；
#   * 关掉大整数/大浮点（要的话再加回来，见 README 的预算表）。
#
# 目标：`make` 出 build/stackvm_mcu/mcu-host（桌面可跑的板端宿主，用于等价性回归）。

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1

ROOT_DIR := ../..
BUILD_DIR ?= $(ROOT_DIR)/build/stackvm_mcu
# 桌面与板端编译同一份核心，板端仅保留宿主和静态池适配。
STACKVM_DIR := ../../library/stackvm
EMBED_DIR := .
HERE := .

MCU_DEFS := -fno-exceptions -fno-rtti -fno-threadsafe-statics \
	-DSTACKVM_WITH_DEFAULT_HEAP=0 -DSTACKVM_WITH_HOST_EXCEPTION_GUARD=0 \
	-DSTACKVM_WITH_HOST_IO=0 -DSTACKVM_WITH_BIGINT=0 -DSTACKVM_WITH_BIGFLOAT=0 \
	-DSTACKVM_WITH_THREADS=0 -DSTACKVM_WITH_AOT=0 -DSTACKVM_EMBEDDED=1

# 预算：按"H743 + 16 MB 外部 SDRAM"给（片内只有 ~1 MB，静态数组放不下这么多，
# 板端要用 mcuArenas::SetBytePool / SetMetadata 把 SDRAM 注入进来；见 README）。
#   字节池 4 MB + 元数据 8 MB = 12 MB，给 16 MB 留 4 MB 余量。
MCU_BUDGET ?= -DMCU_VM_ARENA_BYTES=4194304 -DMCU_METADATA_BYTES=8388608 -DEMBED_POOL_MAX_BLOCKS=4096

# 文件系统后端：0 关闭（缺省）/ 1 C stdio（桌面回归）/ 2 FatFS（板端，需 -I 指向 ff.h）。
# 板端还可以不选后端、直接用 mcuFilesystem::SetApi() 注入自己的实现。
MCU_VM_FILE_BACKEND ?= 0
MCU_VM_MAX_OPEN_FILES ?= 4
MCU_FS_DEFS := -DMCU_VM_FILE_BACKEND=$(MCU_VM_FILE_BACKEND) -DMCU_VM_MAX_OPEN_FILES=$(MCU_VM_MAX_OPEN_FILES)

SOURCES := $(STACKVM_DIR)/stackvm.cpp $(EMBED_DIR)/embed_pool.c \
	$(HERE)/mcu_host.cpp $(HERE)/mcu_metadata_heap.cpp $(HERE)/mcu_file_api.cpp \
	$(HERE)/mcu_glue.cpp $(HERE)/mcu_arenas.cpp
INCLUDES := -I../../library -I$(STACKVM_DIR) -I../../library/bignum -I$(EMBED_DIR) -I$(HERE)

# 目标名后缀：门禁用它编第二份宿主（`MCU_TARGET_SUFFIX=-fs` → `mcu-host-fs`），
# 换后端时不会覆盖主目标；缺省空 = `mcu-host`。
MCU_TARGET_SUFFIX ?=
TARGET := $(BUILD_DIR)/mcu-host$(MCU_TARGET_SUFFIX)

ifeq ($(OS),Windows_NT)
BUILD_DIR_WIN := $(subst /,\,$(BUILD_DIR))
MKDIR := powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD_DIR_WIN)' | Out-Null"
CLEAN := powershell -NoProfile -Command "Remove-Item -LiteralPath '$(BUILD_DIR_WIN)' -Recurse -Force -ErrorAction SilentlyContinue"
else
MKDIR := mkdir -p "$(BUILD_DIR)"
CLEAN := rm -rf "$(BUILD_DIR)"
endif

.PHONY: all clean show-flags

all: $(TARGET)

$(TARGET): $(SOURCES) $(wildcard $(STACKVM_DIR)/*.hpp $(STACKVM_DIR)/parts/* *.hpp) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(MCU_DEFS) $(MCU_BUDGET) $(MCU_FS_DEFS) $(INCLUDES) $(SOURCES) -o $@

$(BUILD_DIR):
	$(MKDIR)

# 打印这次实际用的口径（写进 footprint 报告 / 排查"怎么和桌面不一样"）。
show-flags:
	@echo "$(CXX) $(CXXFLAGS) $(MCU_DEFS) $(MCU_BUDGET) $(MCU_FS_DEFS) $(INCLUDES)"

clean:
	$(CLEAN)
