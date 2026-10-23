/**
 * @file runtime_internal.hpp
 * @brief 执行实现的内部模块依赖入口。
 */
#pragma once
#include "stackvm.hpp"
#include "event_service.hpp"
#include "value_text.hpp"
#include "allocation_index.hpp"
#include "execution_plan.hpp"
#include "machine_optimize.hpp"
#include "data_capacity_index.hpp"
#include "register_core.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <deque>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "runtime_slot.hpp"
#include "runtime_heap.hpp"
#include "runtime_context.hpp"
#include "runtime_objects.hpp"
#include "value_transfer.hpp"
#include "native_functions.hpp"
#include "native_context_impl.hpp"
#include "native_registry.hpp"
#include "binary_stream.hpp"
#include "quad_optimizer_internal.hpp"
