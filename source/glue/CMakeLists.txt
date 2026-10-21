# 标准库的实现只编入宿主胶水库；VM 核心继续使用通用原生记录协议。
file(GLOB MAGEL_STANDARD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/std_*.cpp")
list(REMOVE_ITEM MAGEL_STANDARD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/std_demo.cpp")
file(GLOB MAGEL_BIGNUM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../library/bignum/bigint*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../library/bignum/bigfloat*.cpp")
list(APPEND MAGEL_STANDARD_SOURCES ${MAGEL_BIGNUM_SOURCES}
    ../../library/json/json.cpp ../../library/toml/toml.cpp ../../library/toml/toml_source.cpp)
enable_language(C)
find_package(Threads REQUIRED)
set(MAGEL_CAS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../library/caspp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MAGEL_CAS_ROOT}/core_sources.txt")
file(STRINGS "${MAGEL_CAS_ROOT}/core_sources.txt" MAGEL_CAS_SOURCES)
list(TRANSFORM MAGEL_CAS_SOURCES PREPEND "${MAGEL_CAS_ROOT}/")
if(NO_MAGEL_STD OR NO_MAGEL_STD_CAS OR NO_MAGEL_BIG_INT)
    set(MAGEL_CAS_SOURCES "")
endif()
list(APPEND MAGEL_STANDARD_SOURCES ${MAGEL_CAS_SOURCES}
    ../../library/ipage/source/ipage/sql.cpp ../../library/ipage/source/ipage/dialect.cpp
    ../../library/sqlite/sqlite3.c ../../library/lztext/lztext.cpp)
set_source_files_properties(../../library/sqlite/sqlite3.c PROPERTIES
    COMPILE_DEFINITIONS "SQLITE_THREADSAFE=1;SQLITE_OMIT_LOAD_EXTENSION=1")
add_library(magel_standard_records STATIC ${MAGEL_STANDARD_SOURCES})
target_link_libraries(magel_standard_records PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
target_include_directories(magel_standard_records PUBLIC
    .. ../../library ../../library/stackvm ../../library/bignum ../../library/caspp/include)
target_compile_definitions(magel_standard_records PUBLIC MAGEL_CUSTOM_ALLOCATOR=1
    STACKVM_WITH_REFLECTION=$<BOOL:${STACKVM_WITH_REFLECTION}>)

# 与使用这些记录的 VM 保持相同的预算与功能宏。
target_compile_definitions(magel_standard_records PUBLIC
    $<TARGET_PROPERTY:magel_stackvm,INTERFACE_COMPILE_DEFINITIONS>)
