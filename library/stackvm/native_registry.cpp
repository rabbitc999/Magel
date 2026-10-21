/**
 * @file native_registry.cpp
 * @brief VM 内建服务的名称、实参数量和入口注册。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
const SlotNativeEntry kSlotNativeEntries[] = {
    { "__std_value_to_string", { -1, valueToString } },
    { "__std_exit", { 1, programExit } },
    { "__std_assert", { 1, programAssert } },
    { "__std_panic", { 1, programPanic } },
    { "__std_str_len", { 1, strLen } },
    { "__std_str_at", { 2, strAt } },
    { "__std_str_set", { 3, strSet } },
    { "__std_read_text", { 0, readText } },
    { "__std_input_line", { 0, readText } },
    { "__std_pause", { 0, pauseForDebug } },
    { "__std_input_int", { 0, inputInt } },
    { "__std_input_uint", { 0, inputUInt } },
    { "__std_input_float", { 0, inputFloat } },
    { "__std_input_bool", { 0, inputBool } },
    { "__std_input_char", { 0, inputChar } },
    { "__std_memcpy", { 3, memCopy } },
    { "__std_memset", { 3, memSet } },
    /* 容器：vec / set / map */
    { "__std_str_ref", { 2, strRef } },
    { "__std_char_load", { 1, charLoad } },
    { "__std_char_store", { 2, charStore } },
    { "__std_iter_box", { 4, iterBox } },
    { "__std_heap_live", { 1, heapLive } },
    { "__std_heap_drop_live", { 1, heapDropLive } },
    { "__std_move_live", { 3, moveLifetime<0> } },
    { "__std_move_require", { 3, moveLifetime<1> } },
    { "__std_move_end", { 3, moveLifetime<2> } },
    { "__std_move_begin", { 2, moveLifetime<3> } },
    { "__std_object_copy", { 1, objectCopy } },
    { "__std_closure_new", { 2, closureNew } },
    { "__std_closure_capture", { 4, closureCapture } },
    { "__std_closure_capture_copy", { 4, closureCaptureCopy } },
    { "__std_closure_capture_move", { 6, closureCaptureMove } },
    { "__std_closure_destructor", { 3, closureDestructor } },
    { "__std_closure_ref", { 5, closureRef } },
    { "__std_closure_get", { 2, closureGet } },
    { "__std_function_copy", { 1, functionCopy } },
    { "__std_object_free", { 1, objectFree } },
    { "std::memory::__memoryLiveBytes", { 0, memoryLiveBytes } },
    /* std::watchdog（软重启看门狗）：模块级原生函数按「模块限定名」登记；裸名一并留着，
     * 便于别的宿主 / 导出代码直接按裸名调用。 */
    { "std::watchdog::__watchdogTimeout", { 1, watchdogTimeout } },
    { "std::watchdog::__watchdogKick", { 0, watchdogKick } },
    { "std::watchdog::__watchdogEnable", { 1, watchdogEnable } },
    { "std::watchdog::__watchdogRequestRestart", { 0, requestRestart } },
    { "__watchdogEnable", { 1, watchdogEnable } },
    { "__watchdogRequestRestart", { 0, requestRestart } },
    { "std::memory::__memoryLiveBlocks", { 0, memoryLiveBlocks } },
    { "std::memory::__memoryPeakBytes", { 0, memoryPeakBytes } },
    { "std::memory::__arenaCreate", { 0, arenaCreate } },
    { "std::memory::__arenaAllocate", { 2, arenaAllocate } },
    { "std::memory::__arenaReset", { 1, arenaReset } },
    { "std::memory::__arenaDrop", { 1, arenaDrop } },
    /* 装箱格子（obj）：值 + 类型号；也是接口指针（胖指针）的运行时表示 */
    { "__std_obj_new", { 0, objNew } },
    { "__std_obj_set", { -1, objSet } },
    { "__std_obj_get", { -1, objGet } }, /* 变参：后面跟一串"允许的类型号" */
    { "__std_obj_has_tag", { -1, objHasTag } },
    { "__std_obj_get_ptr", { -1, objGetPointer } },
    { "__std_obj_name", { 1, objName } },
    { "__std_obj_addr", { 1, objAddress } },
    { "__std_reference", { 1, requireReference } },
    { "__std_iface_fail", { 2, ifaceFail } },
    { "__std_str_fmt", { -1, strFormat } },
    { "__std_print", { -1, strPrint } },
    { "__std_put_char", { 1, putChar } },
    { "std::lang::__readLine", { 0, inputReadLine } },
    { "std::lang::__readLines", { 1, inputReadLines } },
    { "__readLine", { 0, inputReadLine } },
    { "__readLines", { 1, inputReadLines } },
};
const SlotNativeTable kSlotNatives{ kSlotNativeEntries, sizeof(kSlotNativeEntries) / sizeof(kSlotNativeEntries[0]) };
}  // namespace runtimeDetail
}  // namespace stackvm
