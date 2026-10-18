Memory::String CxxStringLiteral(const Memory::String &value);

/**
 * @brief 将可导出的标量类型映射为 C 头文件可用的类型名。
 * @param type 已校验的导出类型。
 * @return C 类型名称。
 */
Memory::String CExportType(const std::shared_ptr<MType> &type)
{
    Memory::String result = CAbiType(type);
    const auto at = result.find("std::");
    if (at != Memory::String::npos) result.erase(at, 5);
    return result;
}

/**
 * @brief 生成上下文式 C 导出声明和 AOT 包装；没有导出时返回空内容。
 * @param header 接收可由 C 编译器包含的公开头文件。
 * @return 追加到单文件 AOT 翻译单元中的包装源码。
 */
Memory::String CExportSource(Memory::String *header)
{
    bool any = false;
    for (const auto &function : g_functionDecls) any = any || function.cExport;
    if (!any) return {};
    *header = "#pragma once\n#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n#ifdef __cplusplus\nextern \"C\" {\n#endif\ntypedef struct MagelContext MagelContext;\nMagelContext *magel_create(size_t memory_bytes);\nvoid magel_destroy(MagelContext *context);\nconst char *magel_last_error(const MagelContext *context);\n";
    Memory::String source = R"MGLC(
#include <stdint.h>
#include <atomic>
#include <memory>
#if defined(_WIN32) && !defined(MAGEL_C_OBJECT)
#define MAGEL_C_API extern "C" __declspec(dllexport)
#elif defined(_WIN32)
#define MAGEL_C_API extern "C"
#else
#define MAGEL_C_API extern "C" __attribute__((visibility("default")))
#endif
static thread_local char magel_c_error[2048]{};
static void magel_c_set_error(const char *message) noexcept {
    std::snprintf(magel_c_error, sizeof(magel_c_error), "%s", message ? message : "C export failure");
}
struct MagelContext {
    std::unique_ptr<Memory::DynamicRuntimeArena> arena;
    std::unique_ptr<stackvm::DataArea> data;
    std::unique_ptr<magel_glue::Registry> glue;
    std::unique_ptr<stackvm::RuntimeBridge> runtime;
    std::atomic<bool> busy{false};
    bool started = false;
    ~MagelContext() {
        if (!arena) return;
        Memory::AllocationScope session(arena.get());
        if (runtime && started) runtime->end();
        runtime.reset(); glue.reset(); data.reset();
    }
};
static bool magel_c_invoke(MagelContext *context, const char *label,
                           const Memory::Vector<stackvm::RuntimeValue> &args,
                           stackvm::RuntimeValue &result) {
    Memory::Vector<std::int64_t> storage;
    for (const auto &argument : args)
        storage.push_back(argument.isFloat ? magel_aot::magel_d2b(argument.real) : argument.integer);
    for (const auto &entry : kMcDispatchTable)
        if (std::strcmp(label, entry.name) == 0) {
            bool floating = false;
            const auto value = entry.fn(*context->runtime, storage.empty() ? nullptr : storage.data(), floating);
            result = floating ? stackvm::RuntimeValue::ofFloat(magel_aot::magel_b2d(value)) : stackvm::RuntimeValue::ofInt(value);
            return !context->runtime->failed();
        }
    context->runtime->fail("unknown C export target");
    return false;
}
MAGEL_C_API const char *magel_last_error(const MagelContext *) { return magel_c_error; }
MAGEL_C_API MagelContext *magel_create(std::size_t bytes) {
    magel_c_error[0] = 0;
    try {
        auto context = std::make_unique<MagelContext>();
        context->arena = std::make_unique<Memory::DynamicRuntimeArena>(bytes ? bytes : MAGEL_C_ARENA_DEFAULT);
        Memory::AllocationScope session(context->arena.get());
        context->data = std::make_unique<stackvm::DataArea>(); loadData(context->data.get());
        context->glue = std::make_unique<magel_glue::Registry>();
        Memory::String error;
        if (!bindMagelStandardGlue(*context->glue, &error)) throw std::runtime_error(error.c_str());
#ifdef MAGEL_C_HAS_ABI
        if (!bindMagelAbi(*context->glue, &error)) throw std::runtime_error(error.c_str());
#endif
#ifdef MAGEL_C_HAS_USER
        if (!bindMagelGlue(*context->glue, &error)) throw std::runtime_error(error.c_str());
#endif
        context->runtime = std::make_unique<stackvm::RuntimeBridge>();
        auto &runtime = *context->runtime;
        runtime.setData(context->data.get()); runtime.setNativeRegistry(&context->glue->native());
        runtime.setGlobalsSize(sizeof(McGlobals)); runtime.setFunctionRunner(magel_aot_run_on);
        runtime.setInvoker([pointer=context.get()](std::int64_t function,
                const Memory::Vector<stackvm::RuntimeValue> &args, stackvm::RuntimeValue &result) {
            return magel_aot_run_on(*pointer->runtime, function, args, result);
        });
        runtime.begin(0u); context->started = true;
        stackvm::RuntimeValue result;
        if (!magel_c_invoke(context.get(), "__magel_c_init", {}, result)) throw std::runtime_error(runtime.error().c_str());
        return context.release();
    } catch (const std::exception &error) { magel_c_set_error(error.what()); }
    catch (...) { magel_c_set_error("C export initialization failed"); }
    return nullptr;
}
MAGEL_C_API void magel_destroy(MagelContext *context) {
    if (!context) return;
    if (context->busy.exchange(true)) { magel_c_set_error("C export context is busy"); return; }
    try {
        Memory::AllocationScope session(context->arena.get());
        stackvm::RuntimeValue result;
        if (!context->runtime->failed()) magel_c_invoke(context, "__magel_c_destroy", {}, result);
    } catch (const std::exception &error) { magel_c_set_error(error.what()); }
    catch (...) { magel_c_set_error("C export destruction failed"); }
    delete context;
}
static int magel_c_call(MagelContext *context, const char *label,
                        const Memory::Vector<stackvm::RuntimeValue> &args,
                        stackvm::RuntimeValue &result) noexcept {
    magel_c_error[0] = 0;
    if (!context) { magel_c_set_error("C export context is null"); return 1; }
    if (context->busy.exchange(true)) { magel_c_set_error("C export context is busy"); return 2; }
    struct Guard { MagelContext *context; ~Guard(){context->busy.store(false);} } guard{context};
    try {
        Memory::AllocationScope session(context->arena.get());
        if (context->runtime->failed()) { magel_c_set_error(context->runtime->error().c_str()); return 1; }
        if (magel_c_invoke(context, label, args, result)) return 0;
        magel_c_set_error(context->runtime->error().c_str());
    } catch (const std::exception &error) { magel_c_set_error(error.what()); }
    catch (...) { magel_c_set_error("C export call failed"); }
    return 1;
}
)MGLC";
    source = "// magel-c-entry __magel_c_init\n// magel-c-entry __magel_c_destroy\n" + source;
    for (const auto &function : g_functionDecls)
    {
        if (!function.cExport || function.superseded) continue;
        source = "// magel-c-entry " + function.label + "\n" + source;
        Memory::String signature = "int magel_" + function.source + "(MagelContext *context";
        Memory::String arguments;
        for (std::size_t i = 0; i < function.signature.params.size(); ++i)
        {
            const auto &type = function.signature.params[i].type;
            const auto name = "a" + Memory::toString(i);
            signature += ", " + CExportType(type) + " " + name;
            if (i) arguments += ",";
            arguments += "stackvm::RuntimeValue::" + Memory::String(type->kind == MType::Double ? "ofFloat" : "ofInt") + "(" + name + ")";
        }
        const auto result = function.signature.result;
        if (result->kind != MType::Void) signature += ", " + CExportType(result) + " *output";
        signature += ")";
        *header += "/** Returns 0 on success; inspect magel_last_error on failure. */\n" + signature + ";\n";
        source += "MAGEL_C_API " + signature + " { try {\n";
        if (result->kind != MType::Void) source += "if (!output) { magel_c_set_error(\"C export output is null\"); return 1; }\n";
        source += "stackvm::RuntimeValue result; const int status = magel_c_call(context, " + CxxStringLiteral(function.label) + ", {" + arguments + "}, result);\n";
        if (result->kind != MType::Void)
            source += "if (!status) *output = static_cast<" + CExportType(result) + ">(result." + Memory::String(result->kind == MType::Double ? "real" : "integer") + ");\n";
        source += "return status;\n} catch (const std::exception &error) { magel_c_set_error(error.what()); return 1; } catch (...) { magel_c_set_error(\"C export call failed\"); return 1; }\n}\n";
    }
    *header += "#ifdef __cplusplus\n}\n#endif\n";
    return source;
}
