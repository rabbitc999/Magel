# C++17 SQL 核心；只依赖标准库，在 POSIX shell 中运行。
CXX ?= g++
SH ?= sh
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
BUILD ?= build
LIB_OBJ = $(BUILD)/dialect.o $(BUILD)/sql.o
.PHONY: all test clean
all: $(BUILD)/example $(BUILD)/test_ipage
	$(BUILD)/test_ipage
$(BUILD):
	$(SH) -c "mkdir -p '$(BUILD)'"
$(BUILD)/%.o: ipage/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -I. -c $< -o $@
$(BUILD)/test_ipage: tests/test_ipage.cpp $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -I. $^ -o $@
$(BUILD)/example: examples/main.cpp $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -I. $^ -o $@
test: $(BUILD)/test_ipage
	$(BUILD)/test_ipage
clean:
	$(SH) -c "rm -f '$(BUILD)'/*.o '$(BUILD)'/*.d '$(BUILD)/example' '$(BUILD)/example.exe' '$(BUILD)/test_ipage' '$(BUILD)/test_ipage.exe'"
-include $(LIB_OBJ:.o=.d)
