# Standard records belong to the host glue, independently of the VM archive.
if(STACKVM_EMBEDDED)
    add_library(magel_standard_records INTERFACE)
    return()
endif()
add_library(magel_standard_records STATIC ../../library/bignum/bigint.cpp ../../library/bignum/bigfloat.cpp)
target_include_directories(magel_standard_records PUBLIC ../../library/bignum)
target_compile_definitions(magel_standard_records PUBLIC MAGEL_CUSTOM_ALLOCATOR=1)
