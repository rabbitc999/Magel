cmake_minimum_required(VERSION 3.15)
project(MagelStackVM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(STACKVM_WITH_REFLECTION "启用注解与反射运行时支持" ON)
option(STACKVM_WITH_PANIC_TRACE "Retain panic call stacks" ON)
option(STACKVM_WITH_THREADS "编入宿主线程和共享值" ON)
option(STACKVM_WITH_DEFAULT_HEAP "编入默认动态字节堆" ON)
option(STACKVM_WITH_HOST_EXCEPTION_GUARD "保护宿主回调异常" ON)
option(STACKVM_WITH_HOST_IO "编入按路径加载与保存" ON)
add_library(magel_stackvm STATIC stackvm.cpp)
target_include_directories(magel_stackvm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
foreach(feature THREADS DEFAULT_HEAP HOST_EXCEPTION_GUARD HOST_IO)
    target_compile_definitions(magel_stackvm PUBLIC STACKVM_WITH_${feature}=$<BOOL:${STACKVM_WITH_${feature}}>)
endforeach()
target_compile_definitions(magel_stackvm PUBLIC STACKVM_WITH_REFLECTION=$<BOOL:${STACKVM_WITH_REFLECTION}>)
target_compile_definitions(magel_stackvm PUBLIC STACKVM_WITH_PANIC_TRACE=$<BOOL:${STACKVM_WITH_PANIC_TRACE}>)
add_subdirectory(../../source/glue ${CMAKE_CURRENT_BINARY_DIR}/standard-records)
add_executable(magel-stackvm main.cpp)
target_link_libraries(magel-stackvm PRIVATE magel_stackvm magel_standard_records)

add_executable(magel-stackvm-benchmark benchmark_stackvm.cpp)
target_link_libraries(magel-stackvm-benchmark PRIVATE magel_stackvm)

add_executable(magel-stackvm-magel-benchmark benchmark_magel.cpp)
target_link_libraries(magel-stackvm-magel-benchmark PRIVATE magel_stackvm)

add_executable(magel-stackvm-tests test_stackvm.cpp)
target_link_libraries(magel-stackvm-tests PRIVATE magel_stackvm magel_standard_records)

target_compile_definitions(magel_stackvm PUBLIC MAGEL_CUSTOM_ALLOCATOR=1)

add_executable(svmb_symtool svmb_symtool.cpp ../json/json.cpp)
target_link_libraries(svmb_symtool PRIVATE magel_stackvm)

add_executable(magel-symbol-tests test_symbols.cpp)
target_link_libraries(magel-symbol-tests PRIVATE magel_stackvm)
enable_testing()
add_executable(magel-artifact-tests test_artifacts.cpp)
target_link_libraries(magel-artifact-tests PRIVATE magel_stackvm)
add_test(NAME binary-artifacts COMMAND magel-artifact-tests)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/magel_runtime_events_test.cpp")
    add_executable(magel-runtime-event-tests ../../tests/magel_runtime_events_test.cpp)
    target_link_libraries(magel-runtime-event-tests PRIVATE magel_stackvm)
    add_test(NAME runtime-host-events COMMAND magel-runtime-event-tests)
endif()
add_test(NAME symbols COMMAND magel-symbol-tests ${CMAKE_CURRENT_BINARY_DIR}/symbol-fixtures)
add_executable(magel-panic-tests test_panic.cpp)
target_link_libraries(magel-panic-tests PRIVATE magel_stackvm)
add_test(NAME panic-trace COMMAND magel-panic-tests ${CMAKE_CURRENT_BINARY_DIR}/panic-compact.svmb)

set_tests_properties(panic-trace PROPERTIES TIMEOUT 20)

add_executable(magel-feature-tests test_features.cpp)
target_link_libraries(magel-feature-tests PRIVATE magel_stackvm)
add_test(NAME feature-contract COMMAND magel-feature-tests ${CMAKE_CURRENT_BINARY_DIR}/feature-fixtures)

add_executable(magel-reflection-tests test_reflection.cpp)
target_link_libraries(magel-reflection-tests PRIVATE magel_stackvm)
add_test(NAME reflection-metadata COMMAND magel-reflection-tests ${CMAKE_CURRENT_BINARY_DIR}/reflection-fixtures)

# 回归中的原始诊断断言采用中文；语言切换另由专项测试验证。
set_tests_properties(symbols panic-trace feature-contract reflection-metadata PROPERTIES ENVIRONMENT "MAGEL_LANG=zh")

# IR 与执行器的差分门禁同时用于桌面和裁剪构建。
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build")
foreach(case quad_optimization optimizer_metadata execution_plan)
    add_executable(magel-${case}-tests ../../tests/stackvm_${case}.cpp)
    target_link_libraries(magel-${case}-tests PRIVATE magel_stackvm)
    add_test(NAME ${case} COMMAND magel-${case}-tests)
    set_tests_properties(${case} PROPERTIES ENVIRONMENT "MAGEL_LANG=zh")
endforeach()
