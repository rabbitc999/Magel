#include "chali_i18n.hpp"
#ifndef PARSE_ENGINE_CHALI_RUNTIME_ALLOCATOR_HPP
#define PARSE_ENGINE_CHALI_RUNTIME_ALLOCATOR_HPP
/* Explicit C-backed allocators. No global operator new/delete replacement. */
#ifndef CHALI_CUSTOM_ALLOCATOR
#define CHALI_CUSTOM_ALLOCATOR 1
#endif
#ifndef PARSER_MEMORY_NAMESPACE
#define PARSER_MEMORY_NAMESPACE Memory
#endif
#include "allocator/chali_allocator.hpp"
#endif
