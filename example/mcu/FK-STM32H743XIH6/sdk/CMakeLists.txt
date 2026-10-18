cmake_minimum_required(VERSION 3.20)
project(stm32_lua_car LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
  message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=Project/CMake/arm-clang.cmake")
endif()
set(ARM_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../toolchain/arm-runtime" CACHE PATH "Arm GNU extracted runtime root")
if(NOT EXISTS "${ARM_RUNTIME_ROOT}/arm-none-eabi/include/stdio.h")
  message(FATAL_ERROR "Set ARM_RUNTIME_ROOT to an Arm GNU Toolchain root, or run tools/fetch_cpp_runtime.py from the board example")
endif()
include(Project/CMake/sources.cmake)
add_executable(stm32_lua_car ${CAR_SOURCES}
  Middleware/GUI/gui_runtime.cpp Middleware/GUI/gui_capture.c User/storage.c User/logger.cpp User/logger_port.c
  Middleware/Command/cmd_runtime.cpp Middleware/Command/cmd_file_transfer.cpp
  Middleware/Command/cmd_hex_parser.cpp Middleware/Command/cmd_string_parser.cpp
  Libraries/CMSIS/Device/ST/STM32H7xx/Source/Templates/gcc/startup_stm32h743xx.s
  Project/CMake/newlib_syscalls.c Project/CMake/hardfault.S)
set(MCU_FLAGS -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard)
target_compile_options(stm32_lua_car PRIVATE ${MCU_FLAGS}
  $<$<COMPILE_LANGUAGE:C>:-Og;-g3;-ffunction-sections;-fdata-sections;-fno-common;-Wno-invalid-source-encoding;-Wno-typedef-redefinition;-Wno-unknown-pragmas>)
target_compile_options(stm32_lua_car PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Og;-g3;-ffunction-sections;-fdata-sections;-fno-exceptions;-fno-rtti;-fno-threadsafe-statics>)
target_compile_definitions(stm32_lua_car PRIVATE USE_HAL_DRIVER USE_FULL_LL_DRIVER STM32H743xx ARM_MATH_CM7)
option(CAR_SERVO_ZERO "Build dedicated servo centering firmware with Servo Zero display" OFF)
if(CAR_SERVO_ZERO)
  if(CAR_BRINGUP_NO_ACTUATION)
    message(FATAL_ERROR "CAR_SERVO_ZERO requires actuator outputs")
  endif()
  target_compile_definitions(stm32_lua_car PRIVATE CAR_SERVO_ZERO=1)
endif()
option(CAR_BRINGUP_NO_ACTUATION "Disable actuator outputs for board startup verification" OFF)
if(CAR_BRINGUP_NO_ACTUATION)
  target_compile_definitions(stm32_lua_car PRIVATE CAR_BRINGUP_NO_ACTUATION=1)
endif()

target_include_directories(stm32_lua_car PRIVATE ${CAR_INCLUDES})
target_include_directories(stm32_lua_car SYSTEM PRIVATE "${ARM_RUNTIME_ROOT}/arm-none-eabi/include")
file(GLOB GCC_RUNTIME_DIR "${ARM_RUNTIME_ROOT}/lib/gcc/arm-none-eabi/*/thumb/v7e-m+dp/hard")
target_link_directories(stm32_lua_car PRIVATE
  "${ARM_RUNTIME_ROOT}/arm-none-eabi/lib/thumb/v7e-m+dp/hard" ${GCC_RUNTIME_DIR})
target_link_options(stm32_lua_car PRIVATE ${MCU_FLAGS} -nostdlib -fuse-ld=lld
  "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/Project/CMake/stm32h743xi.ld"
  -Wl,--gc-sections "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/stm32_lua_car.map")
target_link_libraries(stm32_lua_car PRIVATE -Wl,--start-group c m gcc -Wl,--end-group)
set_target_properties(stm32_lua_car PROPERTIES SUFFIX .elf)
set_property(TARGET stm32_lua_car APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Project/CMake/stm32h743xi.ld")
add_custom_command(TARGET stm32_lua_car POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O ihex $<TARGET_FILE:stm32_lua_car> "${CMAKE_CURRENT_BINARY_DIR}/stm32_lua_car.hex"
  COMMAND "${CMAKE_OBJCOPY}" --gap-fill=0xff -O binary $<TARGET_FILE:stm32_lua_car> "${CMAKE_CURRENT_BINARY_DIR}/stm32_lua_car.bin"
  COMMAND "${CMAKE_SIZE}" $<TARGET_FILE:stm32_lua_car>)
