# 标准库的实现只编入宿主胶水库；VM 核心继续使用通用原生记录协议。
file(GLOB MAGEL_STANDARD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/std_*.cpp")
list(REMOVE_ITEM MAGEL_STANDARD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/std_demo.cpp")
file(GLOB MAGEL_BIGNUM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../library/bignum/bigint*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../library/bignum/bigfloat*.cpp")
list(APPEND MAGEL_STANDARD_SOURCES ${MAGEL_BIGNUM_SOURCES}
    ../../library/json/json.cpp ../../library/toml/toml.cpp ../../library/toml/toml_source.cpp)
add_library(magel_standard_records STATIC ${MAGEL_STANDARD_SOURCES})
target_include_directories(magel_standard_records PUBLIC
    .. ../../library ../../library/stackvm ../../library/bignum)
target_compile_definitions(magel_standard_records PUBLIC MAGEL_CUSTOM_ALLOCATOR=1
    STACKVM_WITH_REFLECTION=$<BOOL:${STACKVM_WITH_REFLECTION}>)

# 与使用这些记录的 VM 保持相同的预算与功能宏。
target_compile_definitions(magel_standard_records PUBLIC
    $<TARGET_PROPERTY:magel_stackvm,INTERFACE_COMPILE_DEFINITIONS>)
