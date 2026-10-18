/* ============================================================
 * 入口：magel <源文件.mgl> [--out 前缀] [--emit] [--svmb] [--debug] [--input "1 2 3"]
 *                           [-I 目录] [-D 符号[=值]] [--raw] [--deps]
 *
 *   ① 读源文件（不给文件就读标准输入）；
 *   ② **前处理**：`%include` / `%import` / `%if` / `%define` 等指令（library/preprocess，
 *      语言无关的那一份），默认用空行占位，所以行号与源文件一一对应；
 *   ③ 解析：文法动作在归约的那一刻就把自己的 C 语法树建好；
 *   ④ M_Compile：降级到 simple_ast → 出 .quad / .data → stackvm 组装执行。
 *
 * %main 替换内建 demo main，外壳（stdio / string / parser.hpp）仍是模板给的，
 * 正文前面自动补了 `using namespace magel;`。
 * ============================================================ */
#include <fstream>
#include <iostream>

/* 前处理器（仓库里的通用库，只依赖 C++17 标准库）。 */
#include "preprocess.hpp"
#include "magel_diagnostics.hpp"
#include "magel_macro.hpp"
#include "magel_interpolation.hpp"
#include "magel_module.hpp"
#include "magel_incremental.hpp"
#include "magel_analysis.hpp"
#include "magel_lambda.hpp"
#include "magel_generic.hpp"
#include "magel_project.hpp"
#include "magel_cli.hpp"
#include "magel_glue.hpp"
#include "magel_report.hpp"
#include "dap/dap_server.hpp"
#include "glue/std_math.hpp"
#include "glue/std_random.hpp"
#include "glue/std_regex.hpp"
#include "glue/std_registry.hpp"

namespace
{

struct DemoCounterTag;
struct DemoSignalTag;
struct GlueDemoPointTag;
struct GlueDemoSignalTag;
struct GlueDemoRectTag;
struct GlueDemoCubeTag;
struct GlueDemoBufferTag;
struct GluePointerBoxTag;
using DemoCounter = magel_glue::Handle<DemoCounterTag>;
using DemoSignal = magel_glue::Enum<DemoSignalTag>;

/**
 * @brief 演示驱动自带的两项宿主实现；应用可用同一 Registry 接入自己的 C++ 代码。
 * @param registry 接收函数绑定的宿主注册表。
 */
void InstallDemoGlue(magel_glue::Registry &registry)
{
    /* 标准胶水（math / random / regex）统一走 std_registry，GLUE_NO_XXX 编译期裁剪。 */
    Memory::String stdError;
    if (!std_registry::bindStandardGlue(registry, &stdError))
        std::fprintf(stderr, "magel：注册标准胶水失败：%s\n", stdError.c_str());
    registry.bindTypedImplementation<DemoCounter, std::uint64_t>(
        "demo_counter_from_uint", [](std::uint64_t value) { return DemoCounter{ value }; });
    registry.bindTypedImplementation<std::uint64_t, DemoCounter>(
        "demo_counter_to_uint", [](DemoCounter value) { return value.value; });
    registry.bindTyped<DemoCounter, DemoCounter, DemoCounter>(
        "demo_cpp_add", [](DemoCounter left, DemoCounter right) {
            return DemoCounter{ left.value + right.value };
        });
    registry.bindTypedGeneric<std::int32_t, std::int32_t>(
        "demo_cpp_identity", { "int32" }, [](std::int32_t value) { return value; });
    registry.bindTyped<std::int32_t, Memory::String>(
        "demo_cpp_text_size",
        [](const Memory::String &text) { return static_cast<std::int32_t>(text.size()); });
    registry.bindTyped<std::int64_t, std::int64_t, magel_glue::Variadic<std::int64_t>>(
        "demo_cpp_variadic_sum",
        [](std::int64_t base, const magel_glue::Variadic<std::int64_t> &values) {
            for (const std::int64_t value : values) base += value;
            return base;
        });
    registry.bindTyped<std::int64_t, magel_glue::Function<std::int64_t(std::int64_t)>,
                       std::int64_t>(
        "demo_cpp_apply",
        [](const magel_glue::Function<std::int64_t(std::int64_t)> &function,
           std::int64_t value) { return function(value); });
    registry.bindTypedGeneric<DemoSignal, DemoSignal>(
        "demo_cpp_identity", { "Signal" }, [](DemoSignal value) { return value; });
    registry.bindTypedGeneric<std::int32_t, DemoSignal>(
        "demo_cpp_enum_tag", { "Signal" }, [](DemoSignal value) { return value.tag(); });
    registry.bindTypedGeneric<std::int64_t, DemoSignal>(
        "demo_cpp_enum_payload", { "Signal" },
        [](DemoSignal value) { return value.payload<std::int64_t>(1u); });

    /* ---- 胶水层声明的 MAGEL 类型（结构体 / 枚举 / 接口 + 宿主实现）----
     *
     * 这些定义不写在 `.mgl` 里：`registry.declarations()` 生成声明文本，编译前追加到
     * 合并源码末尾（见本文件 `main` 里的 `declarationSource()`），样例直接使用即可。
     * 多方法接口 `GlueDemoInspect` 的方法按 `<impl 符号>.<方法名>` 查宿主符号。 */
    magel_glue::Declarations &declarations = registry.declarations();
    declarations.structType("GlueDemoPoint", { { "int64", "x" }, { "int64", "y" } });
    declarations.enumType("GlueDemoSignal", { { "Idle", {} }, { "Tone", { { "int64", "hz" } } } });
    declarations.interfaceType("GlueDemoInspect",
                               { { "int64", "area", {} },
                                 { "int64", "scaled", { { "int64", "factor" } } } });
    declarations.implementation("GlueDemoPoint", "GlueDemoInspect", "glue_demo_inspect");
    declarations.interfaceType("GlueDemoCoded", { { "int64", "code", {} } });
    declarations.implementation("GlueDemoSignal", "GlueDemoCoded", "glue_demo_signal");
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoPointTag>>(
        "glue_demo_inspect", "area",
        [](magel_glue::Record<GlueDemoPointTag> self) {
            return self.field<std::int64_t>(0u) * self.field<std::int64_t>(1u);
        });
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoPointTag>, std::int64_t>(
        "glue_demo_inspect", "scaled",
        [](magel_glue::Record<GlueDemoPointTag> self, std::int64_t factor) {
            return self.field<std::int64_t>(0u) * self.field<std::int64_t>(1u) * factor;
        });
    registry.bindTyped<std::int64_t, magel_glue::Enum<GlueDemoSignalTag>>(
        "glue_demo_signal",
        [](magel_glue::Enum<GlueDemoSignalTag> self) {
            return static_cast<std::int64_t>(self.tag());
        });

    /* ---- 胶水层声明**接口继承**：`GlueDemoMeasured : GlueDemoInspect`（外加 unit()），
     * `GlueDemoTagged` 用 `interfaceInherit` 只继承、不加方法。结构体实现派生接口时，
     * 继承来的 area / scaled 与自己的 unit 都要有宿主符号（多方法 impl 仍按
     * `<impl 符号>.<方法名>` 查）。样例见 `example/samples/glue_interface_inherit.mgl`。 ---- */
    declarations.interfaceType("GlueDemoMeasured", { { "int64", "unit", {} } },
                               { "GlueDemoInspect" });
    declarations.interfaceInherit("GlueDemoTagged", { "GlueDemoMeasured" });
    declarations.structType("GlueDemoCube", { { "int64", "edge" }, { "int64", "unitScale" } });
    declarations.implementation("GlueDemoCube", "GlueDemoTagged", "glue_demo_cube");
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoCubeTag>>(
        "glue_demo_cube", "area",
        [](magel_glue::Record<GlueDemoCubeTag> self) {
            const std::int64_t edge = self.field<std::int64_t>(0u);
            return edge * edge;
        });
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoCubeTag>, std::int64_t>(
        "glue_demo_cube", "scaled",
        [](magel_glue::Record<GlueDemoCubeTag> self, std::int64_t factor) {
            const std::int64_t edge = self.field<std::int64_t>(0u);
            return edge * edge * factor;
        });
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoCubeTag>>(
        "glue_demo_cube", "unit",
        [](magel_glue::Record<GlueDemoCubeTag> self) {
            return self.field<std::int64_t>(1u);
        });

    /* ---- 胶水层声明**系统接口继承**：`GlueDemoBuffered` 继承 `Index<int64,int64>` /
     * `Multiply<int64,int64>` / `ExplicitConvert<int64>`，再带一个自己的 `size()`。
     * 一个宿主 impl 要绑 4 条方法（多方法接口按 `<impl 符号>.<方法名>` 查），于是 MAGEL 侧
     * `buffer[3]` / `buffer * 4` / `cast<int64>(buffer)` 都落到宿主符号上。
     * 样例见 `example/samples/glue_system_interfaces.mgl`。 ---- */
    declarations.structType("GlueDemoBuffer", { { "int64", "base" } });
    declarations.interfaceType("GlueDemoBuffered",
                               { { "int64", "size", {} } },
                               { "Index<int64, int64>", "Multiply<int64, int64>",
                                 "ExplicitConvert<int64>" });
    declarations.implementation("GlueDemoBuffer", "GlueDemoBuffered", "glue_demo_buffer");
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoBufferTag>>(
        "glue_demo_buffer", "size",
        [](magel_glue::Record<GlueDemoBufferTag> self) {
            return self.field<std::int64_t>(0u);
        });
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoBufferTag>, std::int64_t>(
        "glue_demo_buffer", "index",
        [](magel_glue::Record<GlueDemoBufferTag> self, std::int64_t key) {
            return self.field<std::int64_t>(0u) + key;
        });
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoBufferTag>, std::int64_t>(
        "glue_demo_buffer", "leftMul",
        [](magel_glue::Record<GlueDemoBufferTag> self, std::int64_t value) {
            return self.field<std::int64_t>(0u) * value;
        });
    registry.bindTypedMethod<std::int64_t, magel_glue::Record<GlueDemoBufferTag>>(
        "glue_demo_buffer", "explicitConvert",
        [](magel_glue::Record<GlueDemoBufferTag> self) {
            return self.field<std::int64_t>(0u);
        });
    /* `interfaceInherit`（只继承、不加新方法）+ 系统接口：单方法接口沿用原符号名。 */
    declarations.interfaceInherit("GlueDemoAddable", { "Add<int64, int64>" });
    declarations.implementation("GlueDemoBuffer", "GlueDemoAddable", "glue_demo_buffer_add");
    registry.bindTypedImplementation<std::int64_t, magel_glue::Record<GlueDemoBufferTag>,
                                     std::int64_t>(
        "glue_demo_buffer_add",
        [](magel_glue::Record<GlueDemoBufferTag> self, std::int64_t value) {
            return self.field<std::int64_t>(0u) + value;
        });

    /* 方式二：`memberFunctions` —— 每条成员自带宿主符号，编译器自动合成单方法接口 +
     * impl，胶水按符号名逐个绑定即可（重载靠各自的符号区分）。最后一条 `missing`
     * **故意只声明不绑定**：`example/samples-error/glue_member_unbound.mgl` 用它锁定"宿主符号
     * 没注册"的运行期报错。 */
    declarations.structType("GlueDemoRect", { { "int64", "width" }, { "int64", "height" } });
    declarations.memberFunctions(
        "GlueDemoRect",
        { { "int64", "area", "glue_demo_rect_area", {} },
          { "int64", "area", "glue_demo_rect_area_scaled", { { "int64", "factor" } } },
          { "int64", "perimeter", "glue_demo_rect_perimeter", {} },
          { "int64", "missing", "glue_demo_rect_missing", {} } });
    registry.bindTypedImplementation<std::int64_t, magel_glue::Record<GlueDemoRectTag>>(
        "glue_demo_rect_area", [](magel_glue::Record<GlueDemoRectTag> self) {
            return self.field<std::int64_t>(0u) * self.field<std::int64_t>(1u);
        });
    registry.bindTypedImplementation<std::int64_t, magel_glue::Record<GlueDemoRectTag>,
                                     std::int64_t>(
        "glue_demo_rect_area_scaled",
        [](magel_glue::Record<GlueDemoRectTag> self, std::int64_t factor) {
            return self.field<std::int64_t>(0u) * self.field<std::int64_t>(1u) * factor;
        });
    registry.bindTypedImplementation<std::int64_t, magel_glue::Record<GlueDemoRectTag>>(
        "glue_demo_rect_perimeter", [](magel_glue::Record<GlueDemoRectTag> self) {
            return 2 * (self.field<std::int64_t>(0u) + self.field<std::int64_t>(1u));
        });

    /* ---- 类型化指针：`T mut&` / `T const&` 过边界是 `Reference<T>`，宿主侧直接按 T
     * 读 / 写，不再手算 VM 地址。标量用 `Reference<int64>` 的 read / write / at；结构体
     * 指针用 `Reference<Record<Tag>>`，read 出视图后 `field` / `set` 按字段读写。
     * 样例 `example/samples/pointer_glue.mgl`，语言侧数组直接退化成 `T mut&`。 ---- */
    registry.bindTypedWithContext<std::int64_t, magel_glue::Reference<std::int64_t>>(
        "demo_pointer_bump",
        [](magel_glue::Context &context, magel_glue::Reference<std::int64_t> cell) {
            const std::int64_t old = cell.read(context);
            cell.write(context, old + 1);
            return old;
        });
    registry.bindTypedWithContext<
        std::int64_t,
        magel_glue::Reference<magel_glue::Record<GluePointerBoxTag>>>(
        "demo_pointer_sum",
        [](magel_glue::Context &context,
           magel_glue::Reference<magel_glue::Record<GluePointerBoxTag>> box) {
            const auto view = box.read(context);
            return view.template field<std::int64_t>(0u) + view.template field<std::int64_t>(1u);
        });
    registry.bindTypedWithContext<
        void,
        magel_glue::Reference<magel_glue::Record<GluePointerBoxTag>>,
        std::int64_t, std::int64_t>(
        "demo_pointer_offset",
        [](magel_glue::Context &context,
           magel_glue::Reference<magel_glue::Record<GluePointerBoxTag>> box,
           std::int64_t dx, std::int64_t dy) {
            const auto view = box.read(context);
            view.template set<std::int64_t>(0u, view.template field<std::int64_t>(0u) + dx);
            view.template set<std::int64_t>(1u, view.template field<std::int64_t>(1u) + dy);
        });
    registry.bindTypedWithContext<std::int64_t, magel_glue::Reference<std::int64_t>,
                                  std::int64_t>(
        "demo_pointer_at",
        [](magel_glue::Context &context, magel_glue::Reference<std::int64_t> data,
           std::int64_t index) { return data.at(context, index); });
    using Reference = magel_glue::Reference<std::int64_t>;
    using OptionalReference = std::optional<Reference>;
    registry.bindTyped<OptionalReference, bool, Reference>("demo_reference_optional",
                                                           [](bool present, Reference value) -> OptionalReference {
                                                               return present ? OptionalReference(value) : std::nullopt;
                                                           });
    registry.bindTypedWithContext<std::int64_t, OptionalReference>("demo_reference_read",
                                                                   [](magel_glue::Context &context, const OptionalReference &value) {
                                                                       return value ? value->read(context) : std::int64_t(-1);
                                                                   });
    registry.bindTypedWithContext<void, OptionalReference>("demo_reference_write",
                                                           [](magel_glue::Context &context, const OptionalReference &value) {
                                                               if (value) value->write(context, value->read(context) + 1);
                                                           });
}

/**
 * @brief 选项要一个值：`-I dir` / `--include-dir dir` 两种写法都行。
 * @param index 待访问的元素索引。
 * @param argc 命令行参数数量。
 * @param what 测试条件的说明文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NeedValue(int index, int argc, const char *what)
{
    if (index + 1 < argc) return true;
    std::fprintf(stderr, MAGEL_TEXT("选项 %s 后面要跟一个值\n"), what);
    return false;
}

/**
 * @brief 路径的目录部分（`%include` 相对源文件目录解析，所以要先抽出目录）。
 * @param path 目标文件或目录的路径。
 * @retval 生成或读取的结果文本。
 */
Memory::String DirNameOf(const Memory::String &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    return (slash == Memory::String::npos) ? Memory::String(".") : path.substr(0u, slash);
}

/**
 * @brief `-h` / `--help`：把全部命令行选项列一遍（不回显源文件，直接退出 0）。
 * @param out 接收处理结果的输出对象。
 */
void PrintUsage(std::FILE *out)
{
    std::fputs(
        MAGEL_TEXT("用法：magel 文件.mgl [程序参数...]\n"
                   "      magel 场景 [文件.mgl] [配置词...] [-- 程序参数...]\n"
                   "\n"
                   "统一工具链：解释、导出 .svmb、导出 AOT 可执行文件都在这一个程序里。\n"
                   "场景指令（运行方式也可用 --mode 指定）：\n"
                   "  run|interpret  内部解释运行（缺省）；直接传入文件时，其后全为程序参数\n"
                   "  check FILE [json]    仅检查，可输出 JSON 诊断\n"
                   "  help                 显示帮助\n"
                   "  emit           只出 .quad / .data.bin / .layout\n"
                   "  svmb           只导出 <前缀>.svmb，不执行\n"
                   "  aot FILE [out EXE] [opt LEVEL] [checked|unchecked]  导出 AOT（默认 checked）\n"
                   "  new DIR [name NAME]  新建 Magel 应用工程（目标目录必须不存在）\n"
                   "  gen            生成独立编译器工程（转发 magel-generator compiler）\n"
                   "  clear out DIR  清理登记过的中间产物（不重新编译）\n"
                   "  lang zh|en|auto     选择显示语言；默认自动检测系统语言\n"
                   "常用配置词：out 路径、opt 级别、reflect、debug、release；new 使用 name 工程名。\n"
                   "开发场景：test tests backend all、fmt check src、deps lock project . offline。\n"
                   "直接文件调用始终运行；文件后的所有内容传给程序。显式场景的高级选项：\n"
                   "不给源文件时从标准输入读。选项：\n"
                   "  -h, --help             显示本帮助并退出\n"
                   "  --out 前缀             生成产物的文件名前缀（缺省 build/magel_out）\n"
                   "  --separate-modules     AOT 按模块缓存本地目标文件并统一链接\n"
                   "  --shared / --object    AOT 库输出，可省略 main；.dll/.so/.o 自动选择类型\n"
                   "  --module-cache DIR     指定 AOT 模块目标文件缓存目录\n"
                   "  --check                仅检查，不执行、不生成产物\n"
                   "  --hard-realtime        禁止整个编译单元使用反射\n"
                   "  --reflection           启用注解与反射（要求前端和运行时均编入支持）\n"
                   "  --no-reflection        关闭注解与反射（注解与反射缺省已开启）\n"
                   "  --diagnostics=json     检查并输出稳定 JSON 诊断\n"
                   "  test|fmt|lsp|deps      测试、格式化、语言服务与依赖锁定（原生内建）\n"
                   "  --emit                 只生成 .quad / .data.bin / .layout，不执行\n"
                   "  --svmb                 额外写出可由 stackvm 直接加载的 <前缀>.svmb\n"
                   "  --debug                写 .svmb 时带上调试节，并在解释执行时打印位置轨迹\n"
                   "  --dap                  作为调试适配器运行（VS Code 断点调试用，stdio 收发 DAP）\n"
                   "  --compress-symbols     写无名变长符号表，生成 PC 映射文件\n"
                   "  --no-compress-symbols  覆盖 magel 配置，关闭符号压缩\n"
                   "  --symbol-map FILE      映射路径（默认 <前缀>.symbol_map.json）\n"
                   "  --input \"文本\"        `input(变量);` 的输入项（空白分隔）；不给就从标准读，调试时可直接敲\n"
                   "  --raw                  跳过前处理：源文件原样喂给解析器\n"
                   "  --deps                 只列出 `%include` 拉进来的文件（构建脚本用）\n"
                   "  --incremental          跨次复用模块预处理与完整编译产物\n"
                   "  --threads              启用隔离的宿主线程执行\n"
                   "  --no-debug             （兼容别名）显式不带调试信息；现在也是缺省\n"
                   "  --stats                打印运行时及增量文件校验统计\n"
                   "  --memory-size 字节     运行时堆容量（缺省 512MiB，最小 64）\n"
                   "  --watchdog             打开可选软重启看门狗（分配失败 / 显式请求 / 超水位重跑）\n"
                   "  --no-watchdog          关闭软重启看门狗（缺省）\n"
                   "  --max-restarts N       最多软重启几次（缺省 3；隐含 --watchdog）\n"
                   "  --watchdog-watermark N 内存水位（字节；隐含 --watchdog）\n"
                   "  --step-limit N         本次执行的指令条数上限（缺省不设上限；0 同缺省）\n"
                   "  -I, --include-dir 目录 追加 include 搜索目录（也可写 -I目录）\n"
                   "  --include-path 目录    追加 include 搜索目录\n"
                   "  --import-dir 目录      追加 import 搜索目录\n"
                   "  -D, --define 符号[=值] 定义预处理符号（也可写 -D符号=值）\n"
                   "  --                     后面全部作为程序的命令行参数\n"),
        out);
    fputs(MAGEL_TEXT("  man [目标]            查询内嵌标准库手册、参数、边界与示例\n"), out);
}

}  // namespace

/**
 * @brief 获取内嵌开发工具资产。
 * @param name 仓库相对路径。
 * @return UTF-8 文本；进程内持续有效。
 */
const char *magel_development_tool(const char *name)
{
    static std::map<std::string, Memory::String> sources;
    auto &source = sources[name];
    if (source.empty()) magel_module::embedded::findGeneratorAssetByPath(name, &source);
    return source.c_str();
}

/**
 * @brief 解析前端命令行参数并执行编译或运行任务。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int magel_main_entry(int argc, char **argv)
{
#ifndef MAGEL_FRONT_END_NO_MAIN
    auto originalArguments = magel_i18n::processArguments(argc, argv);
    std::vector<char *> originalValues;
    for (auto &argument : originalArguments) originalValues.push_back(argument.data());
    auto normalized = magel_cli::normalize(static_cast<int>(originalArguments.size()), originalValues.data());
    std::string localeError;
    if (!magel_i18n::configure(normalized, localeError))
    {
        std::fprintf(stderr, "magel: %s\n", localeError.c_str());
        return 2;
    }
    std::vector<char *> normalizedArgv;
    for (auto &argument : normalized) normalizedArgv.push_back(argument.data());
    normalizedArgv.push_back(nullptr);
    argc = static_cast<int>(normalized.size());
    argv = normalizedArgv.data();
#endif
    if (argc > 1 && std::strcmp(argv[1], "new") == 0) return magel_project::runCli(argc, argv);
    magel_glue::Registry glue;
    InstallDemoGlue(glue);
    glue.install();
    const char *path = nullptr;
    /* 默认产物落在 build/ 下，不和源码混在一起（`--out` 显式给路径时按用户给的来）。 */
    Memory::String prefix = "build/magel_out";
    Memory::String inputText;
    int run = 1;
    bool checkOnly = false, jsonDiagnostics = false;
    bool abiPrepareOnly = false, abiNoCache = false;
    Memory::String abiCache, abiBridge, abiWrite;
    bool hardRealtime = false;
    /* 注解与反射默认开启：宿主（本 exe）没编入支持时静默降级为关闭，不打扰用户；
     * 只有显式写了 `--reflection` 才按"要求必须有支持"处理（不支持就报错）。 */
    bool reflectionEnabled = true;
    bool reflectionExplicit = false; /* 用户是否显式给了 --reflection / --no-reflection。 */
    bool emitBinary = false;         /* --svmb：额外输出可由 stackvm 直接加载的程序 */
    bool raw = false;                /* --raw：不跑前处理 */
    bool depsOnly = false;           /* --deps：只列出依赖文件 */
    bool threadsEnabled = false;     /* --threads：启用隔离的宿主线程 */
    /* 可选软重启看门狗：默认关闭；数值选项同时隐含打开（与 `--watchdog` 同义）。 */
    bool watchdogEnabled = false;
    int watchdogMaxRestarts = 3;
    std::size_t watchdogWatermarkBytes = 0u;
    bool debugEnabled = false; /* --debug：执行时打印源码位置轨迹（stderr） */
    bool dapEnabled = false;   /* --dap：作为 VS Code 的调试适配器跑（stdio 上收发 DAP） */
    bool stripDebug = true;    /* 缺省不带调试信息；--debug 才写调试节 */
    int compressSymbolsOption = -1;
    Memory::String symbolMap;
    bool symbolMapGiven = false;
    std::size_t memoryBytes = Memory::defaultRuntimeCapacity;
    /**
     * `--memory-size` 是否被命令行显式给过：显式给过就优先于 `[embedded] max_memory`。
     */
    bool memoryBytesGiven = false;
    std::size_t stepLimit = 0u; /* --step-limit：0 表示不设上限（缺省） */
    bool runtimeStats = false;
    bool incremental = false;  /* --incremental：跨次复用模块预处理及完整编译产物 */
    bool sourceStdin = false;  ///< 编辑器用真实文件身份读取标准输入中的未保存内容。
    Memory::String cacheDirectory, overlayPath, analysisDependencies;
    Memory::Vector<Memory::String> includeDirs;
    Memory::Vector<Memory::String> importDirs;
    Memory::Vector<Memory::String> moduleRuntimeDirs;
    {
        std::error_code ec;
        const std::filesystem::path executable =
            std::filesystem::weakly_canonical(std::filesystem::u8path(argv[0]), ec);
        if (!ec && executable.has_parent_path())
            moduleRuntimeDirs.push_back(executable.parent_path().u8string());
    }
    Memory::Vector<Memory::String> defineItems;
    Memory::Vector<Memory::String> programArguments;
    /* 前置子命令（与统一入口一致）：run|interpret / emit / svmb。aot 需要 magel-emit，
     * 独立编译器工程没有链进来，给明确指引而不是"不认识的选项"。 */
    int firstArgument = 1;
    if (argc > 1)
    {
        const Memory::String first = argv[1];
        if (first == "run" || first == "interpret") { firstArgument = 2; }
        else if (first == "emit")
        {
            run = 0;
            firstArgument = 2;
        }
        else if (first == "svmb" || first == "smvb")
        {
            run = 0;
            emitBinary = true;
            firstArgument = 2;
        }
        else if (first == "aot")
        {
            std::fprintf(stderr, MAGEL_TEXT("magel：这个构建不含 aot 导出；请用仓库的统一 magel 或 magel-emit\n"));
            return 2;
        }
    }
    for (int index = firstArgument; index < argc; ++index)
    {
        const Memory::String argument = argv[index];
        if (argument == "-h" || argument == "--help")
        {
            PrintUsage(stdout);
            return 0;
        }
        /* `--mode`：与统一入口一致的运行方式；独立构建只认 interpret / emit / svmb。 */
        if (argument == "--mode" || argument == "--run-mode")
        {
            if (index + 1 >= argc)
            {
                std::fprintf(stderr, MAGEL_TEXT("magel：%s 缺少取值\n"), argument.c_str());
                return 2;
            }
            const Memory::String mode = argv[++index];
            if (mode == "interpret") { /* 缺省 */ }
            else if (mode == "emit")
                run = 0;
            else if (mode == "svmb" || mode == "smvb")
            {
                run = 0;
                emitBinary = true;
            }
            else if (mode == "aot")
            {
                std::fprintf(stderr, MAGEL_TEXT("magel：这个构建不含 %s 导出；请用仓库的统一 magel 或 magel-emit\n"), mode.c_str());
                return 2;
            }
            else
            {
                std::fprintf(stderr, MAGEL_TEXT("magel：不认识的运行方式：%s\n"), mode.c_str());
                return 2;
            }
            continue;
        }
        if (argument == "--")
        {
            for (++index; index < argc; ++index) programArguments.emplace_back(argv[index]);
            break;
        }
        if (argument == "--out")
        {
            if (!NeedValue(index, argc, "--out")) return 2;
            prefix = argv[++index];
            continue;
        }
        if (argument == "--memory-size")
        {
            if (index + 1 >= argc)
            {
                std::fprintf(stderr, MAGEL_TEXT("--memory-size 缺少容量\n"));
                return 2;
            }
            const Memory::String value = argv[++index];
            if (value.empty() || value.find_first_not_of("0123456789") != Memory::String::npos) return 2;
            try
            {
                memoryBytes = static_cast<std::size_t>(std::stoull(value));
            } catch (...) { return 2; }
            if (memoryBytes < 64) return 2;
            memoryBytesGiven = true;
            continue;
        }
        if (argument == "--hard-realtime")
        {
            hardRealtime = true;
            continue;
        }
        /* 可选软重启看门狗（默认关闭）：`--watchdog` 打开，数值选项同时隐含打开。 */
        if (argument == "--watchdog")
        {
            watchdogEnabled = true;
            continue;
        }
        if (argument == "--no-watchdog")
        {
            watchdogEnabled = false;
            continue;
        }
        if (argument == "--max-restarts" || argument == "--watchdog-watermark")
        {
            if (!NeedValue(index, argc, argument.c_str())) return 2;
            const Memory::String value = argv[++index];
            if (value.empty() || value.find_first_not_of("0123456789") != Memory::String::npos)
                return 2;
            try
            {
                const unsigned long long parsed = std::stoull(value);
                if (argument == "--max-restarts")
                {
                    watchdogMaxRestarts = parsed > 1000ull ? 1000 : static_cast<int>(parsed);
                }
                else
                {
                    watchdogWatermarkBytes = parsed > 0xffffffffull ? static_cast<std::size_t>(0xffffffffu) : static_cast<std::size_t>(parsed);
                }
            } catch (...) { return 2; }
            watchdogEnabled = true;
            continue;
        }
        if (argument == "--dap")
        {
            dapEnabled = true;
            continue;
        }
        if (argument == "--reflection")
        {
            reflectionEnabled = true;
            reflectionExplicit = true;
            continue;
        }
        if (argument == "--no-reflection")
        {
            reflectionEnabled = false;
            reflectionExplicit = true;
            continue;
        }
        if (argument == "--abi-cache" || argument == "--abi-bridge" || argument == "--abi-write-bridge")
        {
            if (index + 1 >= argc)
            {
                std::fputs("ABI 选项缺少路径\n", stderr);
                return 2;
            }
            const Memory::String value = argv[++index];
            if (value.empty())
            {
                std::fputs("ABI 路径不能为空\n", stderr);
                return 2;
            }
            if (argument == "--abi-cache")
                abiCache = value;
            else if (argument == "--abi-bridge")
                abiBridge = value;
            else
                abiWrite = value;
            continue;
        }
        if (argument == "--abi-no-cache")
        {
            abiNoCache = true;
            continue;
        }
        if (argument == "--abi-prepare")
        {
            abiPrepareOnly = true;
            continue;
        }
        if (argument == "--check")
        {
            checkOnly = true;
            run = 0;
            continue;
        }
        if (argument == "--diagnostics=json")
        {
            checkOnly = true;
            jsonDiagnostics = true;
            run = 0;
            continue;
        }
        if (argument == "--source-stdin")
        {
            sourceStdin = true;
            continue;
        }
        if (argument == "--cache-dir" || argument == "--source-overlays" || argument == "--analysis-deps")
        {
            if (!NeedValue(index, argc, argument.c_str())) return 2;
            const Memory::String value = argv[++index];
            if (argument == "--cache-dir")
                cacheDirectory = value;
            else if (argument == "--source-overlays")
                overlayPath = value;
            else
                analysisDependencies = value;
            continue;
        }
        if (argument == "--stats")
        {
            runtimeStats = true;
            continue;
        }
        if (argument == "--step-limit") /* 0 表示不设上限（缺省），正整数用于防死循环 */
        {
            if (!NeedValue(index, argc, "--step-limit")) return 2;
            const Memory::String value = argv[++index];
            if (value.empty() || value.find_first_not_of("0123456789") != Memory::String::npos)
            {
                std::fprintf(stderr, MAGEL_TEXT("--step-limit 需要非负整数（0 表示不设上限）\n"));
                return 2;
            }
            try
            {
                stepLimit = static_cast<std::size_t>(std::stoull(value));
            } catch (...)
            {
                std::fprintf(stderr, MAGEL_TEXT("--step-limit 需要非负整数（0 表示不设上限）\n"));
                return 2;
            }
            continue;
        }
        if (argument == "--emit") /* 只生成 .quad / .data.bin / .layout，不执行 */
        {
            run = 0;
            continue;
        }
        if (argument == "--threads")
        {
            threadsEnabled = true;
            continue;
        }
        if (argument == "--debug") /* 带调试信息：写调试节，并解释执行时按位置打印（stderr） */
        {
            debugEnabled = true;
            stripDebug = false;
            continue;
        }
        if (argument == "--incremental")
        {
            incremental = true;
            continue;
        }
        if (argument == "--svmb") /* 生成 <前缀>.svmb；是否执行仍由 --emit 决定 */
        {
            emitBinary = true;
            continue;
        }
        if (argument == "--no-debug") /* 兼容别名：显式不带调试节（现在也是缺省） */
        {
            stripDebug = true;
            debugEnabled = false;
            continue;
        }
        if (argument == "--compress-symbols")
        {
            compressSymbolsOption = 1;
            continue;
        }
        if (argument == "--no-compress-symbols")
        {
            compressSymbolsOption = 0;
            continue;
        }
        if (argument == "--symbol-map")
        {
            if (!NeedValue(index, argc, "--symbol-map")) return 2;
            symbolMap = argv[++index];
            symbolMapGiven = true;
            continue;
        }
        if (argument == "--input") /* `input(变量);` 的输入项（空白分隔） */
        {
            if (!NeedValue(index, argc, "--input")) return 2;
            inputText = argv[++index];
            continue;
        }
        if (argument == "--raw") /* 跳过前处理：把源文件原样喂给解析器 */
        {
            raw = true;
            continue;
        }
        if (argument == "--deps") /* 只列出 `%include` 扯进来的文件（构建脚本用） */
        {
            depsOnly = true;
            continue;
        }
        if (argument == "-I" || argument == "--include-dir")
        {
            if (!NeedValue(index, argc, argument.c_str())) return 2;
            includeDirs.push_back(argv[++index]);
            importDirs.push_back(includeDirs.back());
            continue;
        }
        if (argument.compare(0u, 2u, "-I") == 0 && argument.size() > 2u) /* -I目录 */
        {
            includeDirs.push_back(argument.substr(2u));
            importDirs.push_back(includeDirs.back());
            continue;
        }
        if (argument == "--include-path" || argument == "--import-dir")
        {
            if (!NeedValue(index, argc, argument.c_str())) return 2;
            auto &directories = argument == "--include-path" ? includeDirs : importDirs;
            directories.push_back(argv[++index]);
            continue;
        }
        if (argument == "-D" || argument == "--define")
        {
            if (!NeedValue(index, argc, argument.c_str())) return 2;
            defineItems.push_back(argv[++index]);
            continue;
        }
        if (argument.compare(0u, 2u, "-D") == 0 && argument.size() > 2u) /* -D符号=值 */
        {
            defineItems.push_back(argument.substr(2u));
            continue;
        }
        if (!argument.empty() && argument[0] == '-')
        {
            std::fprintf(stderr, MAGEL_TEXT("不认识的选项：%s\n"), argument.c_str());
            return 2;
        }
        if (path != nullptr)
        {
            std::fprintf(stderr, MAGEL_TEXT("一次只能编译一个源文件\n"));
            return 2;
        }
        path = argv[index];
    }

    /* 硬实时禁止反射：默认开启的那份自动让位，只有用户显式点名要反射才算冲突。 */
    if (hardRealtime && reflectionEnabled)
    {
        if (reflectionExplicit)
        {
            const char *message = MAGEL_TEXT("硬实时编译禁止启用反射");
            if (jsonDiagnostics)
                magel_diagnostics::report("MGL1102", message, path ? path : "<命令行>");
            else
                std::fprintf(stderr, "magel：%s\n", message);
            return 2;
        }
        reflectionEnabled = false;
    }
    if (!M_SetReflectionEnabled(reflectionEnabled) && reflectionExplicit)
    {
        /* 默认开启只是"能用就用"：宿主没编入支持时静默关掉，不能让每个程序都失败。
         * 显式 `--reflection` 才按硬要求处理，报出缺哪个编译宏。 */
        const char *message = MAGEL_TEXT("--reflection 需要 MAGEL_WITH_REFLECTION=1 且运行时 STACKVM_WITH_REFLECTION=1");
        if (jsonDiagnostics)
            magel_diagnostics::report("MGL1102", message, path ? path : "<命令行>");
        else
            std::fprintf(stderr, "magel：%s\n", message);
        return 2;
    }
    if (incremental && (path == nullptr || raw))
    {
        std::fprintf(stderr, MAGEL_TEXT("magel：--incremental 需要源文件，且不能与 --raw 合用\n"));
        return 2;
    }
    if (sourceStdin && (!path || !checkOnly))
    {
        std::fputs("magel：--source-stdin 需要 --check 和源文件路径\n", stderr);
        return 2;
    }
    Memory::Map<Memory::String, Memory::String> sourceOverlays;
    try
    {
        sourceOverlays = magel_analysis::overlays(overlayPath);
    } catch (const std::exception &error)
    {
        if (jsonDiagnostics)
            magel_diagnostics::report("MGL1102", error.what(), "<命令行>");
        else
            std::fprintf(stderr, "magel：%s\n", error.what());
        return 2;
    }
    magel_package::Project project;
    Memory::String packageError;
    if (!raw && !magel_package::discover(path ? path : "", project, packageError))
    {
        if (jsonDiagnostics)
            magel_diagnostics::report("MGL1101", packageError, project.file);
        else
            std::fprintf(stderr, "magel：magel：%s\n", packageError.c_str());
        return 1;
    }
    includeDirs.insert(includeDirs.end(), project.includePaths.begin(), project.includePaths.end());
    importDirs.insert(importDirs.end(), project.importPaths.begin(), project.importPaths.end());
    includeDirs.insert(includeDirs.end(), project.searchPaths.begin(), project.searchPaths.end());
    importDirs.insert(importDirs.end(), project.searchPaths.begin(), project.searchPaths.end());
    importDirs.insert(importDirs.end(), moduleRuntimeDirs.begin(), moduleRuntimeDirs.end());
    /* `[magel] debug = true` 是项目级的“带调试节”，等价于命令行写 `--debug`。
     * 缺省 false（不带调试信息）；配置内容进缓存签名，改它会让缓存失效。 */
    if (project.debug) stripDebug = false;
    const bool compressSymbols = compressSymbolsOption < 0 ? project.compressSymbols : compressSymbolsOption != 0;
    if (!symbolMapGiven) symbolMap = project.symbolMap;
    /* 构建**项目**时入口是 `[magel] main` 指定的主文件（命令行没给源文件才用它）；
     * 单文件编译就是命令行显式给的那个文件，不看主文件。
     * `projectMainPath` 只是为了让 `const char *path` 在整个编译期都有效。 */
    Memory::String projectMainPath;
    if (path == nullptr && !project.main.empty())
    {
        projectMainPath = project.main;
        path = projectMainPath.c_str();
    }
    /* `[embedded] max_memory`：嵌入式模式的目标最大内存 —— 嵌入式**用正常解释器**执行，
     * 只是把运行时内存上限压到这个值；命令行 `--memory-size` 优先。 */
    if (!memoryBytesGiven && project.maxMemory > 0)
        memoryBytes = static_cast<std::size_t>(project.maxMemory);
    if (!compressSymbols && !symbolMap.empty())
    {
        if (symbolMapGiven)
        {
            std::fputs(MAGEL_TEXT("--symbol-map 需要启用 --compress-symbols\n"), stderr);
            return 2;
        }
        symbolMap.clear();
    }
    if (compressSymbols && emitBinary)
    {
        namespace fs = std::filesystem;
        const auto mapPath = fs::absolute(Memory::u8path(symbolMap.empty() ? prefix + ".symbol_map.json" : symbolMap)).lexically_normal();
        if (mapPath == fs::absolute(Memory::u8path(prefix + ".svmb")).lexically_normal() || (path && mapPath == fs::absolute(Memory::u8path(path)).lexically_normal()))
        {
            std::fputs(MAGEL_TEXT("映射路径不能覆盖输入或字节码输出\n"), stderr);
            return 2;
        }
    }
    Memory::UniquePtr<magel_incremental::Cache> cache;
    if (incremental)
    {
        Memory::Vector<Memory::String> searchDirs = includeDirs;
        searchDirs.insert(searchDirs.end(), importDirs.begin(), importDirs.end());
        Memory::String pathSignature = project.signature() + "\ninclude paths\n";
        for (const auto &directory : includeDirs) pathSignature += magel_incremental::canonical(directory) + "\n";
        pathSignature += "import paths\n";
        for (const auto &directory : importDirs) pathSignature += magel_incremental::canonical(directory) + "\n";
        cache = Memory::makeUnique<magel_incremental::Cache>(cacheDirectory.empty() ? prefix + ".magel-cache" : cacheDirectory, argv[0],
                                                             searchDirs, defineItems,
                                                             pathSignature,
                                                             Memory::String(reflectionEnabled ? "reflection\n" : "no reflection\n") + (stripDebug ? "strip debug\n" : "keep debug\n") + (compressSymbols ? "compress symbols\n" : "plain symbols\n") + symbolMap + "\n");
        cache->setOverlays(sourceOverlays);
    }

    const Memory::String sourceLabel = (path != nullptr) ? Memory::String(path) : Memory::String("<标准输入>");
    Memory::String text;
    if (path != nullptr && !sourceStdin)
    {
        std::ifstream stream(std::filesystem::u8path(path), std::ios::binary);
        if (!stream)
        {
            std::fprintf(stderr, MAGEL_TEXT("打不开文件：%s\n"), path);
            return 2;
        }
        text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
    else
    {
        text.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    }

    /* ---- 前处理：`%include` / `%if` / `%define` / `%error` … ----
     *
     * 用 `forMetro()` 这套：只认 `%` 前缀，`%define` / `%undefine` 也算符号定义。
     * `preserveLines` 默认开着 —— 指令行与"条件为假"的行都输出成空行，所以
     * 预处理之后的**行号与源文件一一对应**（`%include` 那几行靠 lineMap 翻回去）。
     * `%include` 先查"当前文件所在目录"，再按 `-I` 的顺序找。 */
    preprocess::Result processed;
    std::uint64_t incrementalInput = 0;
    if (!raw)
    {
        preprocess::Options options = preprocess::Preprocessor::forMetro();
        options.includeDirs = includeDirs;
        if (!sourceOverlays.empty()) options.includeLoader = [&](const preprocess::IncludeRequest &request) {
            const auto found = sourceOverlays.find(magel_incremental::canonical(request.resolvedPath));
            return found == sourceOverlays.end() ? preprocess::IncludeResult::useDefault() :
                                                   preprocess::IncludeResult::inlineText(found->second, request.resolvedPath);
        };
        for (const Memory::String &item : defineItems)
        {
            preprocess::Define define;
            if (!preprocess::Preprocessor::parseDefine(item, define))
            {
                std::fprintf(stderr, "magel：-D 的写法不对（要 NAME 或 NAME=值）：%s\n",
                             item.c_str());
                return 2;
            }
            options.defines.push_back(define);
        }
        Memory::Vector<Memory::String> moduleDirs = importDirs;
        magel_module::Loader loader(options, moduleDirs, cache.get(), &project);
        Memory::String moduleError;
        const bool ok = (path != nullptr && !sourceStdin) ? loader.rootFile(path, processed, moduleError) : loader.rootText(text, sourceLabel, path ? Memory::String(std::filesystem::path(path).parent_path().string()) : Memory::String("."), processed, moduleError);
        auto analysisFiles = processed.dependencies;
        if (path) analysisFiles.push_back(path);
        if (!project.file.empty())
        {
            analysisFiles.push_back(project.file);
            const auto lock = std::filesystem::path(project.file).parent_path() / "magel.lock";
            if (std::filesystem::exists(lock)) analysisFiles.push_back(lock.string());
        }
        magel_analysis::dependencies(analysisDependencies, analysisFiles, ok);
        for (const preprocess::Diagnostic &item : processed.diagnostics)
            if (!jsonDiagnostics) std::fprintf(stderr, MAGEL_TEXT("magel：前处理：%s\n"), item.toString().c_str());
        if (!ok)
        {
            if (jsonDiagnostics)
            {
                if (!processed.diagnostics.empty())
                {
                    const auto &d = processed.diagnostics.front();
                    magel_diagnostics::report("MGL1100", d.toString(), d.file, d.line);
                }
                else
                    magel_diagnostics::report("MGL1100", moduleError, sourceLabel);
                return 1;
            }
            std::fprintf(stderr, "magel：%s：%s\n",
                         loader.preprocessError() ? "前处理" : "模块",
                         moduleError.c_str());
            return 1;
        }

        /* 胶水层声明的 MAGEL 类型 / 接口（`Registry::declarations()`）追加在**合并源码末尾**：
         * 于是 `.mgl` 里不必重复写 `struct` / `enum` / `interface` / `impl`。
         *
         * 只有程序里真的出现了这些名字才注入：类型号按出现顺序分配，凭空多出的声明会让
         * `__typeTag(...)` 的既有数值漂移（`example/samples/obj.mgl` 就盯着这些数字）。没用到就一个
         * 字节都不加；用到时追加在末尾，也尽量少动编号。合成行只进 lineMap，报错时指向
         * `<胶水声明>`。 */
        Memory::String glueDeclarations;
        if (glue.declarations().usedBy(processed.text))
            glueDeclarations = glue.declarationSource();
        if (!glueDeclarations.empty())
        {
            if (!processed.text.empty() && processed.text.back() != '\n')
            {
                processed.text += '\n';
                preprocess::LineOrigin pad;
                if (!processed.lineMap.empty()) pad = processed.lineMap.back();
                processed.lineMap.push_back(pad);
            }
            processed.text += glueDeclarations;
            std::size_t declarationLines = 0u;
            for (const char value : glueDeclarations)
                if (value == '\n') ++declarationLines;
            if (glueDeclarations.back() != '\n') ++declarationLines;
            for (std::size_t line = 1u; line <= declarationLines; ++line)
                processed.lineMap.push_back(preprocess::LineOrigin{ "<胶水声明>", line });
        }

        if (cache && run == 0 && !depsOnly && !checkOnly)
        {
            incrementalInput = magel_incremental::inputHash(processed, cache->signature());
            if (cache->outputsCurrent(prefix, incrementalInput, emitBinary,
                                      compressSymbols ? (symbolMap.empty() ? prefix + ".symbol_map.json" : symbolMap) : Memory::String()))
            {
                std::fprintf(stderr, "magel：增量缓存命中：%zu 个模块复用，编译产物未变\n",
                             cache->reused());
                if (runtimeStats) std::fprintf(stderr, "magel：增量文件校验：%zu 次读取，%zu 次复用\n", cache->hashReads(), cache->hashReuses());
                return 0;
            }
        }
        Memory::String macroError;
        if (!magel_interpolation::run(processed, macroError))
        {
            std::fprintf(stderr, "magel：插值：%s\n", macroError.c_str());
            return 1;
        }
        if (!magel_macro::run(processed, macroError, &loader.moduleByFile()))
        {
            if (jsonDiagnostics)
            {
                magel_diagnostics::report("MGL1100", macroError, sourceLabel);
                return 1;
            }
            std::fprintf(stderr, MAGEL_TEXT("magel：前处理：%s\n"), macroError.c_str());
            return 1;
        }
        if (!magel_lambda::run(processed, macroError))
        {
            if (jsonDiagnostics)
            {
                magel_diagnostics::report("MGL1100", macroError, sourceLabel);
                return 1;
            }
            std::fprintf(stderr, "magel：lambda：%s\n", macroError.c_str());
            return 1;
        }
        if (!magel_generic::run(processed, macroError))
        {
            if (jsonDiagnostics)
            {
                magel_diagnostics::report("MGL1500", macroError, sourceLabel);
                return 1;
            }
            std::fprintf(stderr, MAGEL_TEXT("magel：泛型：%s\n"), macroError.c_str());
            return 1;
        }
        text = processed.text;
        if (depsOnly)
        {
            for (const Memory::String &file : processed.dependencies) std::printf("%s\n", file.c_str());
            return 0;
        }
    }

    Memory::Vector<Memory::String> sourceFiles;
    sourceFiles.reserve(processed.lineMap.size());
    for (const auto &origin : processed.lineMap) sourceFiles.push_back(origin.file);
    M_SetSourceFiles(sourceFiles, sourceLabel);
    Memory::Vector<std::size_t> sourceLines;
    for (const auto &origin : processed.lineMap) sourceLines.push_back(origin.line);
    M_SetSourceLines(sourceLines);
    Engine engine;
    ParserValue_T value{};
    magel_generic::markContainerTypeArguments(text);
    const Status status = engine.runText(text, &value);
    if (status != Status::Ok)
    {
        const Error *error = engine.lastError();
        if (jsonDiagnostics)
        {
            const auto line = error ? error->line() : 1;
            const auto original = line && line <= processed.lineMap.size() ? processed.lineMap[line - 1] : preprocess::LineOrigin{ sourceLabel, line };
            /* JSON 诊断（编辑器里划波浪线）也走一遍呈现层：否则编辑器显示的是
             * `SEMI` / `IMPL` 这种内部名，跟终端里看到的两套说法。 */
            magel_diagnostics::report("MGL1001",
                                      error ? magel_report::localizeSyntaxMessage(error->message()) : Memory::String(MAGEL_TEXT("解析失败")),
                                      original.file, original.line, error ? error->column() : 1);
            return 1;
        }
        if (error != nullptr)
        {
            /* 行号是**预处理之后**的行号：有映射就翻回源文件（`%include` 展开的地方
             * 尤其需要 —— 报错要指向真正写错的那个文件）。 */
            const std::size_t line = error->line();
            /* 消息里写的是 Token 内部名（`SEMI` / `IMPL`…），换成「分号(;)」这种
             * 用户看得懂的说法；外层已经写了「语法错误：」，消息里那句前缀要剥掉。
             * 整句骨架也按当前显示语言重建（英文是 `expected … , found …` 的语序）。 */
            const Memory::String message =
                magel_report::localizeSyntaxMessage(error->message());
            if (line >= 1u && line <= processed.lineMap.size() && !processed.lineMap[line - 1u].file.empty())
            {
                const preprocess::LineOrigin &origin = processed.lineMap[line - 1u];
                std::fprintf(stderr, MAGEL_TEXT("语法错误：%s 第 %zu 行第 %u 列：%s\n"), origin.file.c_str(),
                             origin.line, error->column(), message.c_str());
                /* 回显出错那一行 + 波浪号（长度取出错 Token 的词素长度）。 */
                const Memory::String snippet = magel_report::renderSnippet(
                    origin.file, origin.line, error->column(), error->tokenText().size());
                if (!snippet.empty()) std::fputs(snippet.c_str(), stderr);
            }
            else
            {
                std::fprintf(stderr, MAGEL_TEXT("语法错误：第 %u 行第 %u 列：%s\n"), error->line(),
                             error->column(), message.c_str());
            }
        }
        else
        {
            std::fprintf(stderr, MAGEL_TEXT("解析失败：%s\n"), statusName(status));
        }
        return 1;
    }

    Memory::Vector<const char *> argumentPointers;
    argumentPointers.reserve(programArguments.size());
    for (const Memory::String &argument : programArguments)
        argumentPointers.push_back(argument.c_str());
    M_SetProgramArguments(static_cast<int>(argumentPointers.size()), argumentPointers.data());
    M_SetThreadsEnabled(threadsEnabled ? 1 : 0);
    M_SetDebugEnabled(debugEnabled ? 1 : 0);
    M_SetStepLimit(stepLimit);
    M_SetStripDebug(stripDebug ? 1 : 0);
    M_SetSymbolCompression(compressSymbols ? 1 : 0, symbolMap.c_str(), project.name.c_str(), project.version.c_str());
    M_SetRuntimeMemory(memoryBytes);
    M_SetWatchdog(watchdogEnabled ? 1 : 0, watchdogMaxRestarts,
                  static_cast<int>(watchdogWatermarkBytes));
    M_SetCheckOnly(checkOnly);
    M_SetAbiPrepareOnly(abiPrepareOnly);
    M_SetAbiOptions(abiCache, abiBridge, abiWrite, abiNoCache);
    if (dapEnabled)
    {
        /* DAP 模式：不在这里直接跑，把「编译 + 执行」交给调试适配器——它要先等 VS Code
         * 下断点、发完 configurationDone，再在 `launch` 里回调执行，程序才有机会停在
         * 用户下的断点上。编译错误也走 `output` 事件，否则用户只能看到"什么都没发生"。 */
        M_SetDapEnabled(true);
        return dap::serve([&]() {
            const int inner = M_Compile("magel", prefix.c_str(), 1, emitBinary ? 1 : 0,
                                        inputText.c_str());
            if (inner == 0) return 0;
            Memory::String text = "magel：";
            const char *message = M_LastError();
            if (message != nullptr && message[0] != '\0')
            {
                const auto line = M_LastErrorLine();
                const auto original = line && line <= processed.lineMap.size() ? processed.lineMap[line - 1] : preprocess::LineOrigin{ sourceLabel, line };
                text += original.file + " 第 " + std::to_string(original.line) + " 行";
                if (M_LastErrorColumn())
                    text += "第 " + std::to_string(M_LastErrorColumn()) + " 列";
                text += "：";
                text += message;
            }
            else
                text += "编译失败";
            dap::emitOutput(std::string(text.data(), text.size()) + "\n", "stderr");
            return inner;
        });
    }
    const int code = M_Compile("magel", prefix.c_str(), run, emitBinary ? 1 : 0,
                               inputText.c_str());
    Memory::Vector<magel_diagnostics::Diagnostic> warnings;
    for (const auto &item : M_Warnings())
    {
        const auto original = item.line && item.line <= processed.lineMap.size() ? processed.lineMap[item.line - 1] : preprocess::LineOrigin{ sourceLabel, item.line };
        warnings.push_back({ item.message, original.file, original.line, item.column ? item.column : 1u, true });
        if (!jsonDiagnostics)
        {
            std::fprintf(stderr, "magel：警告：%s:%zu:%zu：%s\n", original.file.c_str(), original.line, item.column ? item.column : 1u, item.message.c_str());
            const auto snippet = magel_report::renderSnippet(original.file, original.line, item.column ? item.column : 1u);
            if (!snippet.empty()) std::fputs(snippet.c_str(), stderr);
        }
    }
    if (runtimeStats && run && !Memory::runtimeMemoryStatsAvailable) std::fputs("运行时分配器：malloc/free，峰值内存不可用\n", stderr);
    if (runtimeStats && run && Memory::runtimeMemoryStatsAvailable) std::fprintf(stderr, "运行时分配器：峰值 %zu 字节（容量 %zu 字节；运行后已清零）\n", M_RuntimePeakBytes(), memoryBytes);
    if (code != 0)
    {
        Memory::Vector<magel_diagnostics::Diagnostic> diagnostics;
        for (const auto &item : M_Diagnostics())
        {
            const auto original = item.line && item.line <= processed.lineMap.size() ? processed.lineMap[item.line - 1] : preprocess::LineOrigin{ sourceLabel, item.line };
            diagnostics.push_back({ item.message, original.file, original.line, item.column ? item.column : 1u });
            if (!jsonDiagnostics)
            {
                /* `<内建 lang>` 这类合成文件是编译器自己拼出来的源码（前处理的行映射会翻
                 * 到它），行号对用户没有意义；真正读不到原文（renderSnippet 返回空）也一样，
                 * 那就维持原来"只有一句话"的写法 —— 宁可不给位置，也不给一个错的位置。 */
                const bool positioned = item.line && !original.file.empty() && original.file.front() != '<' && magel_report::hasSourceLine(original.file, original.line);
                if (positioned)
                {
                    /* 语义 / 类型 / 借用诊断也要给"报错源提醒"：先写 `文件 第 N 行[第 M 列]`，
                     * 再回显那一行原文 + 波浪号。列只有注解那种贴着 token 的检查才给得出，
                     * 拿不到就只写行、整行铺波浪号（不能什么都不写 —— 用户 copy 不到出错位置）。 */
                    /* 自动落点只会给到语句最后那个 token，所以要往前挪到语句开头；
                     * 这笔账算在这里 —— 文案里的"第 N 列"与波浪号必须是同一个位置。 */
                    const std::size_t column = item.column ? magel_report::anchoredColumn(original.file, original.line, item.column) : 0u;
                    if (column)
                        std::fprintf(stderr, MAGEL_TEXT("magel：%s 第 %zu 行第 %zu 列：%s\n"),
                                     original.file.c_str(), original.line, column, item.message.c_str());
                    else
                        std::fprintf(stderr, MAGEL_TEXT("magel：%s 第 %zu 行：%s\n"),
                                     original.file.c_str(), original.line, item.message.c_str());
                    const Memory::String snippet = magel_report::renderSnippet(
                        original.file, original.line, column,
                        column ? 0u : magel_report::kSnippetWholeLine);
                    if (!snippet.empty()) std::fputs(snippet.c_str(), stderr);
                }
                else
                    std::fprintf(stderr, "magel：%s\n", item.message.c_str());
            }
        }
        if (diagnostics.empty() && M_LastError() && M_LastError()[0])
        {
            const auto line = M_LastErrorLine();
            const auto column = M_LastErrorColumn();
            const auto original = line && line <= processed.lineMap.size() ? processed.lineMap[line - 1] : preprocess::LineOrigin{ sourceLabel, line };
            diagnostics.push_back({ M_LastError(), original.file, original.line, column ? column : 1u });
            if (!jsonDiagnostics)
            {
                /* 行号为 0 = 这条错误没有可指的源码位置（比如运行期错误：位置得跑起来才知道，
                 * 它的 `出错位置` / `调用栈` 自己就写在下面几行）。这时保持单行原文不加前缀。 */
                if (!line || original.file.empty() || original.file.front() == '<' || !magel_report::hasSourceLine(original.file, original.line))
                    std::fprintf(stderr, "magel：%s\n", M_LastError());
                else
                {
                    const std::size_t anchored = column ? magel_report::anchoredColumn(original.file, original.line, column) : 0u;
                    if (anchored)
                        std::fprintf(stderr, MAGEL_TEXT("magel：%s 第 %zu 行第 %zu 列：%s\n"),
                                     original.file.c_str(), original.line, anchored, M_LastError());
                    else
                        std::fprintf(stderr, MAGEL_TEXT("magel：%s 第 %zu 行：%s\n"),
                                     original.file.c_str(), original.line, M_LastError());
                    const Memory::String snippet = magel_report::renderSnippet(
                        original.file, original.line, anchored,
                        anchored ? 0u : magel_report::kSnippetWholeLine);
                    if (!snippet.empty()) std::fputs(snippet.c_str(), stderr);
                }
            }
        }
        diagnostics.insert(diagnostics.end(), warnings.begin(), warnings.end());
        if (jsonDiagnostics) magel_diagnostics::reportMany(diagnostics);
        return code;
    }
    if (jsonDiagnostics) magel_diagnostics::reportMany(warnings);
    if (cache && run == 0 && !checkOnly)
    {
        cache->saveOutputs(prefix, incrementalInput, emitBinary,
                           compressSymbols ? (symbolMap.empty() ? prefix + ".symbol_map.json" : symbolMap) : Memory::String());
        std::fprintf(stderr, "magel：增量编译：%zu 个模块复用，%zu 个模块重新预处理\n",
                     cache->reused(), cache->rebuilt());
        if (runtimeStats) std::fprintf(stderr, "magel：增量文件校验：%zu 次读取，%zu 次复用\n", cache->hashReads(), cache->hashReuses());
    }
    return 0;
}

/* 独立编译器工程（非统一构建）自带 main；统一构建由 `magel_tool.cpp` 提供 main 并
 * 直接调用上面的 `magel_main_entry`，所以在 magel 那套构建里定义
 * `MAGEL_FRONT_END_NO_MAIN` 把这里的 main 关掉。 */
#ifndef MAGEL_FRONT_END_NO_MAIN
/**
 * @brief 调用 Magel 前端入口并返回其退出状态。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main(int argc, char **argv) { return magel_main_entry(argc, argv); }
#endif
