# 标准库的实现只编入宿主胶水库；VM 核心继续使用通用原生记录协议。
file(GLOB CHALI_STANDARD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/std_*.cpp")
list(REMOVE_ITEM CHALI_STANDARD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/std_demo.cpp")
file(GLOB CHALI_BIGNUM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../source/bignum/bigint*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../source/bignum/bigfloat*.cpp")
list(APPEND CHALI_STANDARD_SOURCES ${CHALI_BIGNUM_SOURCES}
    ../../source/json/json.cpp ../../source/toml/toml.cpp ../../source/toml/toml_source.cpp)
enable_language(C)
find_package(Threads REQUIRED)
set(CHALI_CAS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../library/caspp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CHALI_CAS_ROOT}/core_sources.txt")
file(STRINGS "${CHALI_CAS_ROOT}/core_sources.txt" CHALI_CAS_SOURCES)
list(TRANSFORM CHALI_CAS_SOURCES PREPEND "${CHALI_CAS_ROOT}/")
if(NO_CHALI_STD OR NO_CHALI_STD_CAS OR NO_CHALI_BIG_INT)
    set(CHALI_CAS_SOURCES "")
endif()
list(APPEND CHALI_STANDARD_SOURCES ${CHALI_CAS_SOURCES}
    ../../library/ipage/source/ipage/sql.cpp ../../library/ipage/source/ipage/dialect.cpp
    ../../library/sqlite/sqlite3.c ../../source/lztext/lztext.cpp)
set_source_files_properties(../../library/sqlite/sqlite3.c PROPERTIES
    COMPILE_DEFINITIONS "SQLITE_THREADSAFE=1;SQLITE_OMIT_LOAD_EXTENSION=1")
add_library(chali_standard_records STATIC ${CHALI_STANDARD_SOURCES})
target_link_libraries(chali_standard_records PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
target_include_directories(chali_standard_records PUBLIC
    .. ../../library ../../source/stackvm ../../source/bignum ../../library/caspp/include)
target_compile_definitions(chali_standard_records PUBLIC CHALI_CUSTOM_ALLOCATOR=1
    STACKVM_WITH_REFLECTION=$<BOOL:${STACKVM_WITH_REFLECTION}>)

# 与使用这些记录的 VM 保持相同的预算与功能宏。
target_compile_definitions(chali_standard_records PUBLIC
    $<TARGET_PROPERTY:chali_stackvm,INTERFACE_COMPILE_DEFINITIONS>)
