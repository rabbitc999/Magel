cmake_minimum_required(VERSION 3.15)
project(ParseEngineMcuVm LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    message(FATAL_ERROR "mcu_vm 的构建口径是 gcc/clang 风格（-fno-exceptions 等）；MSVC 需要另写一套开关")
endif()

# 板端口径：异常自由 + 无默认堆 + 无 OS 线程 / 互斥 / 条件变量 + 无文件 I/O + 无宿主异常兜底。
set(MCU_DEFS
    STACKVM_EMBEDDED=1
    STACKVM_WITH_AOT=0
    STACKVM_WITH_DEFAULT_HEAP=0
    STACKVM_WITH_THREADS=0
    STACKVM_WITH_HOST_EXCEPTION_GUARD=0
    STACKVM_WITH_HOST_IO=0
    STACKVM_WITH_BIGINT=0
    STACKVM_WITH_BIGFLOAT=0)

set(MCU_BUDGET "")
set(EMBED_POOL_MAX_BLOCKS 4096 CACHE STRING "静态字节池的最大物理块数")
if(DEFINED MCU_VM_ARENA_BYTES)
    list(APPEND MCU_BUDGET MCU_VM_ARENA_BYTES=${MCU_VM_ARENA_BYTES})
endif()
if(DEFINED MCU_VM_FILE_BACKEND)
    list(APPEND MCU_BUDGET MCU_VM_FILE_BACKEND=${MCU_VM_FILE_BACKEND})
endif()
if(DEFINED MCU_VM_MAX_OPEN_FILES)
    list(APPEND MCU_BUDGET MCU_VM_MAX_OPEN_FILES=${MCU_VM_MAX_OPEN_FILES})
endif()
if(DEFINED MCU_METADATA_BYTES)
    list(APPEND MCU_BUDGET MCU_METADATA_BYTES=${MCU_METADATA_BYTES})
endif()

add_executable(mcu-host
    mcu_host.cpp
    mcu_metadata_heap.cpp
    mcu_file_api.cpp
    mcu_glue.cpp
    mcu_arenas.cpp
    ../../library/stackvm/stackvm.cpp
    embed_pool.c)

target_include_directories(mcu-host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../library
    ${CMAKE_CURRENT_SOURCE_DIR}/../../library/stackvm)

target_compile_definitions(mcu-host PRIVATE
    ${MCU_DEFS}
    ${MCU_BUDGET}
    EMBED_POOL_MAX_BLOCKS=${EMBED_POOL_MAX_BLOCKS})

# 这条开关就是"让 try/catch 编译不过"：MCU 目标必须一辈子带着它。
target_compile_options(mcu-host PRIVATE
    -fno-exceptions -fno-rtti -fno-threadsafe-statics -Wall -Wextra)
