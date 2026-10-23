cmake_minimum_required(VERSION 3.15)
project(ChaLiLsp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 共用件是本项目自带的副本：`lsp/protocol_types.hpp` 与 `source/json/`。
set(LSP_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../source/json)

add_library(chali_lsp STATIC macro_analyzer.cpp language_server.cpp ../macro/signature.cpp ../syntax/source_text.cpp ${JSON_DIR}/json.cpp)
target_include_directories(chali_lsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${LSP_LIB_DIR}
    ../../source ../../source/preprocess)
if(MSVC)
  target_compile_options(chali_lsp PRIVATE /W4 /utf-8)
else()
  target_compile_options(chali_lsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(chali-lsp lsp_main.cpp)
target_link_libraries(chali-lsp PRIVATE chali_lsp)
if(MSVC)
  target_compile_options(chali-lsp PRIVATE /utf-8)
endif()

enable_testing()
add_executable(chali-lsp-tests macro_analyzer_test.cpp)
target_link_libraries(chali-lsp-tests PRIVATE chali_lsp)
if(MSVC)
  target_compile_options(chali-lsp-tests PRIVATE /utf-8)
endif()
add_test(NAME chali-lsp-tests COMMAND chali-lsp-tests)
