cmake_minimum_required(VERSION 3.20)
project(magel_fk_h743 LANGUAGES C CXX ASM)

get_filename_component(MAGEL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(FK_BOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}/sdk" CACHE PATH "FK-STM32H743XIH6 BSP source directory")
set(ARM_RUNTIME_ROOT "${CMAKE_CURRENT_LIST_DIR}/toolchain/arm-runtime" CACHE PATH "Arm GNU extracted runtime root")
if(NOT EXISTS "${FK_BOARD_ROOT}/Project/CMake/sources.cmake")
  message(FATAL_ERROR "Board SDK missing: ${FK_BOARD_ROOT}/Project/CMake/sources.cmake")
endif()
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
  message(FATAL_ERROR "Use -DCMAKE_TOOLCHAIN_FILE=<board-example>/sdk/Project/CMake/arm-clang.cmake")
endif()
set(CAR_BRINGUP_NO_ACTUATION ON CACHE BOOL "Disable motor and servo outputs" FORCE)
set(CAR_SERVO_ZERO OFF CACHE BOOL "No servo movement in VM firmware" FORCE)
add_subdirectory("${FK_BOARD_ROOT}" board-sdk)
get_target_property(BOARD_SOURCES stm32_lua_car SOURCES)
list(FILTER BOARD_SOURCES EXCLUDE REGEX "(^|/)User/main\\.c$")
set_property(TARGET stm32_lua_car PROPERTY SOURCES ${BOARD_SOURCES})
set_target_properties(stm32_lua_car PROPERTIES OUTPUT_NAME magel-fk-h743)

set(MCU "${MAGEL_ROOT}/source/stackvm_mcu")
option(FK_AUTORUN_BENCHMARK "Run built-in benchmark once before opening the selector" ON)
option(FK_BOARD_SELFTEST "Automatically run benchmark, panic and exit programs through the selector" OFF)
if(FK_BOARD_SELFTEST)
  target_compile_definitions(stm32_lua_car PRIVATE FK_BOARD_SELFTEST=1)
endif()
if(FK_AUTORUN_BENCHMARK)
  target_compile_definitions(stm32_lua_car PRIVATE FK_AUTORUN_BENCHMARK=1)
endif()
target_sources(stm32_lua_car PRIVATE
  main.c board_io.c magel_board.cpp
  ${MCU}/mcu_glue.cpp ${MCU}/mcu_arenas.cpp ${MCU}/mcu_metadata_heap.cpp
  ${MCU}/mcu_file_api.cpp ${MCU}/embed_pool.c
  ${MAGEL_ROOT}/library/stackvm/stackvm.cpp)
target_include_directories(stm32_lua_car PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/assets"
  "${MCU}" "${MAGEL_ROOT}/library" "${MAGEL_ROOT}/library/stackvm")
file(GLOB CPP_VERSION_DIR "${ARM_RUNTIME_ROOT}/arm-none-eabi/include/c++/*")
if(NOT CPP_VERSION_DIR)
  message(FATAL_ERROR "C++ headers missing; run tools/fetch_cpp_runtime.py first")
endif()
list(GET CPP_VERSION_DIR 0 CPP_HEADERS)
target_include_directories(stm32_lua_car SYSTEM BEFORE PRIVATE "${CPP_HEADERS}"
  "${CPP_HEADERS}/arm-none-eabi/thumb/v7e-m+dp/hard" "${CPP_HEADERS}/arm-none-eabi"
  "${CPP_HEADERS}/backward")
target_compile_options(stm32_lua_car PRIVATE -Os -g3)
target_compile_definitions(stm32_lua_car PRIVATE
  STACKVM_EMBEDDED=1 STACKVM_WITH_AOT=0 STACKVM_WITH_DEFAULT_HEAP=0
  STACKVM_WITH_THREADS=0 STACKVM_WITH_HOST_EXCEPTION_GUARD=0 STACKVM_WITH_HOST_IO=0
  STACKVM_WITH_BIGINT=0 STACKVM_WITH_BIGFLOAT=0 STACKVM_WITH_REFLECTION=0
  MCU_VM_FILE_BACKEND=2 MCU_VM_MAX_OPEN_FILES=4 MCU_METADATA_BOOTSTRAP_MALLOC=1
  MCU_VM_ARENA_BYTES=64 MCU_RUNTIME_ARENA_BYTES=64 MCU_METADATA_BYTES=64
  EMBED_POOL_MAX_BLOCKS=4096)
target_compile_definitions(stm32_lua_car PRIVATE FK_VM_DEBUG_SYNC_IO=1)
target_link_libraries(stm32_lua_car PRIVATE -Wl,--start-group stdc++ supc++ c m gcc -Wl,--end-group)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(MAGEL_COMPILER "${MAGEL_ROOT}/magel.exe" CACHE FILEPATH "Host Magel compiler")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets/builtin_program.hpp"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/build_assets.py"
    --magel "${MAGEL_COMPILER}" --out "${CMAKE_CURRENT_BINARY_DIR}/assets"
  DEPENDS programs/benchmark.mgl programs/board.mgl programs/hello.mgl programs/panic.mgl tools/build_assets.py "${MAGEL_COMPILER}"
  VERBATIM)
target_sources(stm32_lua_car PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/assets/builtin_program.hpp")
add_custom_target(magel_board_assets DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/assets/builtin_program.hpp")
add_dependencies(stm32_lua_car magel_board_assets)
