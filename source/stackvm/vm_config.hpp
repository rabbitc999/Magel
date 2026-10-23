/**
 * @file vm_config.hpp
 * @brief VM 功能裁剪、分配器和宿主基础类型配置。
 */
#pragma once
#include "../../source/chali_i18n.hpp"
#include "../../source/allocator/chali_allocator.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/**
 * @def STACKVM_WITH_REFLECTION
 * @brief 是否编入注解与反射运行时支持；默认关闭，只允许 0 或 1。
 */
#ifndef STACKVM_WITH_REFLECTION
#define STACKVM_WITH_REFLECTION 0
#endif
#if STACKVM_WITH_REFLECTION != 0 && STACKVM_WITH_REFLECTION != 1
#error STACKVM_WITH_REFLECTION must be 0 or 1
#endif

/**
 * @def STACKVM_WITH_ALLOCATION_GUARDS
 * @brief 默认校验完整访问范围属于活跃分配。
 * 分配器自身的底层边界检查不受此宏影响。
 */
#ifndef STACKVM_WITH_ALLOCATION_GUARDS
#define STACKVM_WITH_ALLOCATION_GUARDS 1
#endif
#if STACKVM_WITH_ALLOCATION_GUARDS != 0 && STACKVM_WITH_ALLOCATION_GUARDS != 1
#error STACKVM_WITH_ALLOCATION_GUARDS must be 0 or 1
#endif

/**
 * @def STACKVM_WITH_PANIC_TRACE
 * @brief 默认记录 panic/运行错误调用栈；设为 0 或定义 NO_CHALI_PANIC_TRACE 可裁剪。
 * 关闭后不自动保留错误栈，显式调试回调仍可查看调试帧。
 */
#ifdef NO_CHALI_PANIC_TRACE
#undef STACKVM_WITH_PANIC_TRACE
#define STACKVM_WITH_PANIC_TRACE 0
#elif !defined(STACKVM_WITH_PANIC_TRACE)
#define STACKVM_WITH_PANIC_TRACE 1
#endif

/**
 * @def STACKVM_WITH_THREADS
 * @brief 是否编入宿主线程、共享槽、有界通道和显式事件服务，默认开启。
 * @note 关闭后保留单执行上下文、同步宿主调用、执行预算、停止与看门狗检查。
 */
#ifndef STACKVM_WITH_THREADS
#define STACKVM_WITH_THREADS 1
#endif
#if STACKVM_WITH_THREADS != 0 && STACKVM_WITH_THREADS != 1
#error STACKVM_WITH_THREADS must be 0 or 1
#endif

/**
 * @def STACKVM_WITH_DEFAULT_HEAP
 * @brief 是否编入**默认字节堆**（`runtime_heap.cpp` 的 `Heap`：STL 容器实现）。
 *
 * `1`（缺省，桌面行为）= 宿主没 `setHeapAllocator` 时用这个堆，分配失败靠 STL 扩容报错。
 * `0`（目标宿主）= 整个类裁掉（连带它的互斥锁与 STL 容器），此时**必须**由宿主注入
 * `HeapAllocator`（`SetBytePool`）；没注入时所有堆分配明确失败，不会退化成宿主堆。
 */
#ifndef STACKVM_WITH_DEFAULT_HEAP
#define STACKVM_WITH_DEFAULT_HEAP 1
#endif
#if STACKVM_WITH_DEFAULT_HEAP != 0 && STACKVM_WITH_DEFAULT_HEAP != 1
#error STACKVM_WITH_DEFAULT_HEAP must be 0 or 1
#endif

/**
 * @def STACKVM_WITH_HOST_EXCEPTION_GUARD
 * @brief 宿主回调异常保护；无异常构建设为零。
 */
#ifndef STACKVM_WITH_HOST_EXCEPTION_GUARD
#define STACKVM_WITH_HOST_EXCEPTION_GUARD 1
#endif
/**
 * @def STACKVM_WITH_HOST_IO
 * @brief 是否编入按文件路径加载和保存的接口实现。
 */
#ifndef STACKVM_WITH_HOST_IO
#define STACKVM_WITH_HOST_IO 1
#endif
