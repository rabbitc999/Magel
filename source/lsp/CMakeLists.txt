cmake_minimum_required(VERSION 3.15)
project(MagelLsp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 共用件是本项目自带的副本：`lsp/protocol_types.hpp` 与 `library/json/`。
set(LSP_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../library/json)

add_library(magel_lsp STATIC macro_analyzer.cpp language_server.cpp ../magel_macro.cpp ../magel_macro_controls.cpp ${JSON_DIR}/json.cpp)
target_include_directories(magel_lsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${LSP_LIB_DIR}
    ../../library ../../library/preprocess)
if(MSVC)
  target_compile_options(magel_lsp PRIVATE /W4 /utf-8)
else()
  target_compile_options(magel_lsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(magel-lsp lsp_main.cpp)
target_link_libraries(magel-lsp PRIVATE magel_lsp)
if(MSVC)
  target_compile_options(magel-lsp PRIVATE /utf-8)
endif()

enable_testing()
add_executable(magel-lsp-tests macro_analyzer_test.cpp)
target_link_libraries(magel-lsp-tests PRIVATE magel_lsp)
if(MSVC)
  target_compile_options(magel-lsp-tests PRIVATE /utf-8)
endif()
add_test(NAME magel-lsp-tests COMMAND magel-lsp-tests)
