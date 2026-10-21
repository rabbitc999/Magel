# ---------------------------------------------------------------------------
# caspp —— C++17 代数符号计算器
#
#   make              构建 build/caspp（可执行）与全部测试
#   make test         运行全部测试（等价于 make check）
#   make run          启动 REPL
#   make bench        跑大整数与表达式性能基准
#   make clean        清理 build/
#
# 目录约定：头文件 include/cas/**，实现 src/**（对应目录结构），测试 tests/**。
# 产物全部落在 build/，源码树保持干净。
# ---------------------------------------------------------------------------

CXX      ?= g++
.DEFAULT_GOAL := all
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude -I../bignum
LDFLAGS  ?=
LDLIBS   ?=

BUILD_DIR := build
ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

# 库源文件（不含 main.cpp）
LIB_SOURCES := $(shell python -c "from pathlib import Path; print(' '.join(Path('core_sources.txt').read_text().splitlines()))")
LIB_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(LIB_SOURCES))
SHARED_BIGINT_SOURCES := $(wildcard ../bignum/bigint*.cpp)
SHARED_BIGINT_OBJECTS := $(patsubst ../bignum/%.cpp,$(BUILD_DIR)/shared_%.o,$(SHARED_BIGINT_SOURCES))
LIB_OBJECTS += $(SHARED_BIGINT_OBJECTS)

$(SHARED_BIGINT_OBJECTS): $(BUILD_DIR)/shared_%.o: ../bignum/%.cpp ../bignum/bigint.hpp
	@python -c "from pathlib import Path; Path('$(dir $@)').mkdir(parents=True, exist_ok=True)"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

# 可执行与测试
MAIN_OBJECT := $(BUILD_DIR)/src/main.o
TARGET      := $(BUILD_DIR)/caspp$(EXE)

# 测试只链接库对象 + 各自的 main，不能带上 src/main.o
# 注意用递归赋值：LIB_OBJECTS 在其后才定义
TEST_OBJECTS = $(LIB_OBJECTS)

TEST_SOURCES := $(wildcard tests/*.cpp)
TEST_TARGETS := $(patsubst tests/%.cpp,$(BUILD_DIR)/tests/%$(EXE),$(TEST_SOURCES))

.PHONY: all test check run bench clean rebuild format

all: $(TARGET) $(TEST_TARGETS)

$(BUILD_DIR)/%.o: %.cpp
	@python -c "from pathlib import Path; Path('$(dir $@)').mkdir(parents=True, exist_ok=True)"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(LIB_OBJECTS) $(MAIN_OBJECT)
	@python -c "from pathlib import Path; Path('$(dir $@)').mkdir(parents=True, exist_ok=True)"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "  -> $@"

$(BUILD_DIR)/tests/%$(EXE): tests/%.cpp $(TEST_OBJECTS)
	@python -c "from pathlib import Path; Path('$(dir $@)').mkdir(parents=True, exist_ok=True)"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(TEST_OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

-include $(wildcard $(BUILD_DIR)/src/*.d $(BUILD_DIR)/src/detail/*.d $(BUILD_DIR)/*.d)

test check: all
	@echo "=== run all tests ==="
	@status=0; \
	for t in $(TEST_TARGETS); do \
		echo "--- $$t"; \
		$$t || status=1; \
	done; \
	if [ $$status -eq 0 ]; then echo "=== all passed ==="; else echo "=== FAILURES ==="; fi; \
	exit $$status

run: $(TARGET)
	@./$(TARGET)

bench: $(TARGET)
	@./$(BUILD_DIR)/tests/big_int_fuzz$(EXE) >/dev/null 2>&1 || true
	@echo "=== 基准 ==="
	@./$(TARGET) -q -c "factor(x^12-1)" -c "expand((x+y+z)^8)" -c "integrate(x^3*sin(x), x)" \
		-c "groebner([x^2+y^2-1, x-y], [x, y])" -c "solve(x^4-5x^2+4=0, x)"

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all
