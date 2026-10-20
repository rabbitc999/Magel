/** @file magel_i18n.hpp
 * @brief Magel 工具链与运行时的中、英静态消息及系统语言检测。
 */
#ifndef MAGEL_I18N_HPP
#define MAGEL_I18N_HPP
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include "console/utf8.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#if defined(_WIN32) && !defined(_MSC_VER)
extern "C" int __cdecl __wgetmainargs(int*, wchar_t***, wchar_t***, int, void*);
#endif
#ifdef _MSC_VER
#include <corecrt_startup.h>
#endif
namespace magel_i18n {
/** @brief 获取系统命令行的 UTF-8 实参；Windows 使用 CRT 的宽字符解析结果。
 * @param argc 原始参数数量。 @param argv 原始参数数组。 @return 拥有型 UTF-8 参数。
 */
inline std::vector<std::string> processArguments(int argc, char **argv){
    static tool_console::Session console;
    return tool_console::arguments(argc,argv);
}

/** @brief 支持的显示语言；Auto 仅用于选择，不作为消息表下标。 */
enum class Language { Zh, En, Auto };
/** @brief 识别语言名称和系统区域串。 @param value UTF-8 区域名称。 @return 可识别语言或 Auto。 */
inline Language parse(const char *value) {
    if (!value || !*value) return Language::Auto;
    const auto lower=[](char c){ return c>='A' && c<='Z' ? char(c-'A'+'a') : c; };
    if (std::strlen(value)>=2) {
        const char a=lower(value[0]),b=lower(value[1]);
        if (value[2]==0 || value[2]=='_' || value[2]=='-' || value[2]=='.' || value[2]=='@') {
            if(a=='z' && b=='h')return Language::Zh;
            if(a=='e' && b=='n')return Language::En;
        }
    }
    return Language::Auto;
}
/** @brief 自动选择：MAGEL_LANG、POSIX 区域环境、Windows UI 语言、英文。 @return 具体显示语言。 */
inline Language detect() {
    const char *setting=std::getenv("MAGEL_LANG");
    if (setting && std::strcmp(setting,"auto")!=0) {
        const auto found=parse(setting); return found==Language::Auto?Language::En:found;
    }
    for(const char *name:{"LC_ALL","LC_MESSAGES","LANG"}) {
        const char *value=std::getenv(name);
        if(value && *value) { const auto found=parse(value); return found==Language::Auto?Language::En:found; }
    }
#ifdef _WIN32
    const WORD primary=PRIMARYLANGID(GetUserDefaultUILanguage());
    if(primary==LANG_CHINESE)return Language::Zh;
#endif
    return Language::En;
}
/** @brief 显式语言状态；静态存储不占用语言对象的 Arena。 @return 原子状态。 */
inline std::atomic<int>& selection(){static std::atomic<int> value{-1};return value;}
/** @brief 获取当前语言。 @return 具体显示语言。 */
inline Language language(){const int selected=selection().load(std::memory_order_relaxed);static const Language automatic=detect();return selected<0?automatic:static_cast<Language>(selected);}
/** @brief 覆盖当前进程语言。 @param value 语言，Auto 重新读取环境。 @return 无。 */
inline void select(Language value){selection().store(static_cast<int>(value==Language::Auto?detect():value),std::memory_order_relaxed);}
/** @brief 静态消息记录；标识符、路径及用户程序输出不进入翻译表。 */
struct Message {const char *zh,*en;};
// MAGEL_CATALOG_BEGIN
inline constexpr Message messages[] = {
    {"\n\nMagel 应用工程。以下命令在工程根目录执行：\n\n", "\n\nMagel application. Run these commands from the project root:\n\n"},
    {"\n调用栈（外 → 内）：", "\nCall stack (outer to inner):"},
    {"      magel 场景 [文件.mgl] [配置词...] [-- 程序参数...]\n", "       magel COMMAND [FILE.mgl] [settings...] [-- program arguments...]\n"},
    {"  --                     后面全部作为程序的命令行参数\n", "  --                     Pass all remaining arguments to the program\n"},
    {"  --check                仅检查，不执行、不生成产物\n", "  --check                Check only; do not execute or write artifacts\n"},
    {"  --compress-symbols     写无名变长符号表，生成 PC 映射文件\n", "  --compress-symbols     Write compact symbols and a PC mapping file\n"},
    {"  --dap                  作为调试适配器运行（VS Code 断点调试用，stdio 收发 DAP）\n", "  --dap                  Run the debug adapter over standard input/output\n"},
    {"  --debug                写 .svmb 时带上调试节，并在解释执行时打印位置轨迹\n", "  --debug                Include .svmb debug data and interpreter location traces\n"},
    {"  --deps                 只列出 `%include` 拉进来的文件（构建脚本用）\n", "  --deps                 List files loaded by %include (for build scripts)\n"},
    {"  --deps                 只列出 import 模块与工程依赖文件（构建脚本用）\n", "  --deps                 List imported modules and project dependencies for build scripts\n"},
    {"  --diagnostics=json     检查并输出稳定 JSON 诊断\n", "  --diagnostics=json     Check and emit structured JSON diagnostics\n"},
    {"  --emit                 只生成 .quad / .data.bin / .layout，不执行\n", "  --emit                 Export .quad / .data.bin / .layout without executing\n"},
    {"  --emit                 只生成 .quad / .data，不执行\n", "  --emit                 Export .quad / .data without executing\n"},
    {"  --hard-realtime        禁止整个编译单元使用反射\n", "  --hard-realtime        Forbid reflection throughout the compilation unit\n"},
    {"  --import-dir 目录      追加 import 搜索目录\n", "  --import-dir DIR       Add an import search directory\n"},
    {"  --include-path 目录    追加 include 搜索目录\n", "  --include-path DIR     Add an include search directory\n"},
    {"  --include-path 目录    追加模块搜索目录\n", "  --include-path DIR    Add a module search directory\n"},
    {"  --incremental          跨次复用模块预处理与完整编译产物\n", "  --incremental          Reuse module preprocessing and compiled artifacts\n"},
    {"  --input \"文本\"        `input(变量);` 的输入项（空白分隔）\n", "  --input \"TEXT\"        Whitespace-separated input for input(variable);\n"},
    {"  --input \"文本\"        `input(变量);` 的输入项（空白分隔）；不给就从标准读，调试时可直接敲\n", "  --input \"TEXT\"        Whitespace-separated input values; otherwise read standard input\n"},
    {"  --max-restarts N       最多软重启几次（缺省 3；隐含 --watchdog）\n", "  --max-restarts N       Maximum soft restarts (default 3; enables --watchdog)\n"},
    {"  --memory-size 字节     运行时堆容量（缺省 512MiB，最小 64）\n", "  --memory-size BYTES    Runtime memory capacity (default: 512MiB; minimum: 64)\n"},
    {"  --no-compress-symbols  覆盖 magel 配置，关闭符号压缩\n", "  --no-compress-symbols  Override configuration and disable symbol compression\n"},
    {"  --no-debug             （兼容别名）显式不带调试信息；现在也是缺省\n", "  --no-debug             Omit debug information (also the default)\n"},
    {"  --no-panic-details 隐藏 panic 路径及调用记录（上游符号压缩时自动启用）。\n", "  --no-panic-details Hide panic paths and call traces (automatic with compressed symbols).\n"},
    {"  --no-reflection        关闭注解与反射（注解与反射缺省已开启）\n", "  --no-reflection        Disable annotations/reflection (enabled by default)\n"},
    {"  --no-watchdog          关闭软重启看门狗（缺省）\n", "  --no-watchdog          Disable soft restarts (default)\n"},
    {"  --out 前缀             生成产物的文件名前缀（缺省 build/magel_out）\n", "  --out PREFIX           Output file prefix (default: build/magel_out)\n"},
    {"  --raw                  跳过前处理：源文件原样喂给解析器\n", "  --raw                  Skip preprocessing and parse the source directly\n"},
    {"  --reflection           启用注解与反射（要求前端和运行时均编入支持）\n", "  --reflection           Enable annotations/reflection (compiler and VM support required)\n"},
    {"  --stats                打印运行时及增量文件校验统计\n", "  --stats                Show runtime and incremental validation statistics\n"},
    {"  --step-limit N         本次执行的指令条数上限（缺省不设上限；0 同缺省）\n", "  --step-limit N         Instruction limit (default: unlimited; 0 means unlimited)\n"},
    {"  --svmb                 额外写出可由 stackvm 直接加载的 <前缀>.svmb\n", "  --svmb                 Also write <prefix>.svmb for stackvm\n"},
    {"  --symbol-map FILE      映射路径（默认 <前缀>.symbol_map.json）\n", "  --symbol-map FILE      Mapping path (default: <prefix>.symbol_map.json)\n"},
    {"  --threads              启用隔离的宿主线程执行\n", "  --threads              Enable isolated host-thread execution\n"},
    {"  --watchdog             打开可选软重启看门狗（分配失败 / 显式请求 / 超水位重跑）\n", "  --watchdog             Enable optional soft restarts on allocation failure, requests or watermarks\n"},
    {"  --watchdog-watermark N 内存水位（字节；隐含 --watchdog）\n", "  --watchdog-watermark N Memory watermark in bytes; enables --watchdog\n"},
    {"  -D, --define 符号[=值] 定义预处理符号（也可写 -D符号=值）\n", "  -D, --define NAME[=VALUE] Define a preprocessing symbol (also -DNAME=VALUE)\n"},
    {"  -I, --include-dir 目录 追加 include 搜索目录（也可写 -I目录）\n", "  -I, --include-dir DIR  Add an include search directory (also -IDIR)\n"},
    {"  -I, --include-dir 目录 追加模块搜索目录（也可写 -I目录）\n", "  -I, --include-dir DIR  Add a module search directory; -IDIR is also accepted\n"},
    {"  -h, --help             显示本帮助并退出\n", "  -h, --help             Show this help and exit\n"},
    {"  check FILE [json]    仅检查，可输出 JSON 诊断\n", "  check FILE [json]    Check only; optionally emit JSON diagnostics\n"},
    {"  clear out DIR  清理登记过的中间产物（不重新编译）\n", "  clear out DIR  Remove registered intermediate files without recompiling\n"},
    {"  emit           只出 .quad / .data / .layout\n", "  emit           Export .quad / .data / .layout only\n"},
    {"  emit           只出 .quad / .data.bin / .layout\n", "  emit           Export .quad / .data.bin / .layout only\n"},
    {"  gen            生成独立编译器工程（转发 magel-generator compiler）\n", "  gen            Generate a standalone compiler project (magel-generator compiler)\n"},
    {"  help                 显示帮助\n", "  help                 Show help\n"},
    {"  lang zh|en|auto     选择显示语言；默认自动检测系统语言\n", "  lang zh|en|auto     Select display language (default: system detection)\n"},
    {"  man [目标]            查询内嵌标准库手册、参数、边界与示例\n", "  man [TARGET]          Query embedded library docs, parameters, boundaries and examples\n"},
    {"  new DIR [name NAME]  新建 Magel 应用工程（目标目录必须不存在）\n", "  new DIR [name NAME]  Create a Magel project (destination must not exist)\n"},
    {"  run|interpret  内部解释运行（缺省）；直接传入文件时，其后全为程序参数\n", "  run|interpret  Interpret (default); arguments after a direct file go to the program\n"},
    {"  svmb           只导出 <前缀>.svmb，不执行\n", "  svmb           Export <prefix>.svmb without executing\n"},
    {"  test|fmt|lsp|deps      测试、格式化、语言服务与依赖锁定（原生内建）\n", "  test|fmt|lsp|deps      Tests, formatting, language service, dependency locks (native)\n"},
    {" 个文件，lang.mgl 来自引用模板）\n", " files; lang.mgl generated from the template)\n"},
    {" 个编译器文件\n", " compiler files\n"},
    {" 缺少取值", " requires a value"},
    {" 需要非空来源字符串", " requires a nonempty source string"},
    {"%s：运行失败：%s\n", "%s: execution failed: %s\n"},
    {"--memory-size 缺少容量\n", "--memory-size requires a capacity\n"},
    {"--reflection 需要 MAGEL_WITH_REFLECTION=1 且运行时 STACKVM_WITH_REFLECTION=1", "--reflection requires MAGEL_WITH_REFLECTION=1 and STACKVM_WITH_REFLECTION=1"},
    {"--search 后需要关键词", "--search requires a keyword"},
    {"--step-limit 需要非负整数（0 表示不设上限）\n", "--step-limit requires a nonnegative integer (0 means unlimited)\n"},
    {"--symbol-map 需要启用 --compress-symbols\n", "--symbol-map requires --compress-symbols\n"},
    {"--update 需要 Magel 源码仓库和 Python 构建环境", "--update requires the Magel source repository and Python build environment"},
    {"Git 依赖必须使用 git+URL#完整40位提交号", "Git dependencies require git+URL# followed by a full 40-digit commit hash"},
    {"LSP 消息过大", "LSP message exceeds the size limit"},
    {"Magel 开发工具：编译器驱动的测试与诊断、保守格式化、LSP 和可复现依赖锁定。", "Magel development tools: compiler-driven tests, formatting, LSP and reproducible dependency locks."},
    {"Magel 标准库手册", "Magel standard library manual"},
    {"[MGL1204] 保存的引用不能超过来源的生命周期", "[MGL1204] A stored reference cannot outlive its source"},
    {"`magel.toml` 配置项目包；`src/` 放应用源码，`tests/` 放普通 main 测试。\n", "Configure the package in `magel.toml`; `src/` contains source files and `tests/` contains ordinary main tests.\n"},
    {"lambda 不能按值捕获引用或不可安全复制的资源：", "lambda cannot capture references or resources without safe value copying: "},
    {"lambda 不能隐式捕获 self 或接收者字段：", "lambda cannot implicitly capture self or receiver fields: "},
    {"lang 后需要 zh、en 或 auto", "lang requires zh, en or auto"},
    {"magel：%s 第 %zu 行第 %zu 列：%s\n", "magel: %s:%zu:%zu: %s\n"},
    {"magel：%s 缺少取值\n", "magel: %s requires a value\n"},
    {"magel：--incremental 需要源文件，且不能与 --raw 合用\n", "magel: --incremental requires a source file and cannot be combined with --raw\n"},
    {"magel：clear 需要 --out-dir DIR\n", "magel: clear requires --out-dir DIR\n"},
    {"magel：name 必须且只能指定一个工程名\n", "magel: name requires exactly one project name\n"},
    {"magel：不认识的运行方式：%s\n", "magel: unknown execution mode: %s\n"},
    {"magel：创建工程失败：%s\n", "magel: project creation failed: %s\n"},
    {"magel：前处理：%s\n", "magel: preprocessing: %s\n"},
    {"magel：子命令与 --mode=%s 冲突\n", "magel: command conflicts with --mode=%s\n"},
    {"magel：无效工程名；请用 name 指定字母开头的字母、数字、- 或 _ 组合\n", "magel: invalid project name; use name with a leading letter and letters, digits, - or _\n"},
    {"magel：泛型：%s\n", "magel: generics: %s\n"},
    {"options", "Options"},
    {"package.compiler 必须为 >=X.Y.Z", "package.compiler must use >=X.Y.Z"},
    {"panic：访问未分配内存（写）：地址 ", "panic: write to unallocated memory at address "},
    {"panic：访问未分配内存（读）：地址 ", "panic: read from unallocated memory at address "},
    {"positional arguments", "Positional arguments"},
    {"show this help message and exit", "Show this help message and exit"},
    {"usage: ", "Usage: "},
    {"void 函数不能返回值", "A void function cannot return a value"},
    {"一次只能编译一个源文件", "Only one source file can be compiled at a time"},
    {"一次只能编译一个源文件\n", "Only one source file can be compiled at a time\n"},
    {"不安全路径：", "Unsafe path: "},
    {"不支持的显示语言：", "Unsupported display language: "},
    {"不给源文件时从标准输入读。选项：\n", "Without a source file, read standard input. Options:\n"},
    {"不认识的文件：%s\n", "Unknown file: %s\n"},
    {"不认识的选项：", "Unknown option: "},
    {"不认识的选项：%s\n", "Unknown option: %s\n"},
    {"依赖 ", "Dependency "},
    {"依赖文件不支持符号链接：", "Symbolic links are forbidden in dependency files: "},
    {"依赖目录不支持符号链接：", "Symbolic links are forbidden in dependency directories: "},
    {"候选目标", "Suggested targets"},
    {"元组元素是只读的；请构造新元组后给整个变量赋值", "Tuple elements are read-only; assign a new tuple to the whole variable"},
    {"内存不足", "Out of memory"},
    {"内存与系统", "Memory and system"},
    {"写入失败", "Write failed"},
    {"创建 magel.toml、src/main.mgl、tests/test_main.mgl、README.md 和 .gitignore。\n", "Create magel.toml, src/main.mgl, tests/test_main.mgl, README.md and .gitignore.\n"},
    {"协程异步入口不支持带捕获的 lambda", "Coroutine entry points do not support capturing lambdas"},
    {"参数", "parameter"},
    {"反射调用 API 与具体结构体或枚举类型不匹配", "Reflection API does not match the concrete struct or enum type"},
    {"发现并运行 test_*.mgl / *_test.mgl", "Discover and run test_*.mgl / *_test.mgl"},
    {"变量", "variable"},
    {"只读常数不能赋值：", "Cannot assign to a read-only constant: "},
    {"只读引用（`T const&`）不能写入元素", "A read-only reference (`T const&`) cannot modify elements"},
    {"只读引用（`T const&`）只能读，不能写入元素或成员", "A read-only reference (`T const&`) cannot modify elements or members"},
    {"只读引用（`T const&`）只能读，不能写字段（要写就用可写引用 `T mut&`）", "A read-only reference (`T const&`) cannot modify fields; use `T mut&`"},
    {"只读引用（`T const&`）只能读，不能通过引用写", "A read-only reference (`T const&`) cannot be written through"},
    {"只读引用（`T const&`）只能读，不能通过引用写（要写就用可写引用 `T mut&`）", "A read-only reference (`T const&`) cannot be written through; use `T mut&`"},
    {"只读接收者不能调用修改方法：", "A read-only receiver cannot call a mutating method: "},
    {"启动 stdio 语言服务器", "Start the stdio language server"},
    {"场景指令（运行方式也可用 --mode 指定）：\n", "Commands (execution mode can also be selected with --mode):\n"},
    {"声明", "declaration"},
    {"字段", "field"},
    {"字节", " bytes"},
    {"完整示例", "Complete example"},
    {"定时器异步入口不支持带捕获的 lambda", "Timer callbacks do not support capturing lambdas"},
    {"容器与迭代", "Collections and iteration"},
    {"已创建 Magel 工程 %s：%s\n", "Created Magel project %s: %s\n"},
    {"已生成 ", "Generated "},
    {"已生成 Magel 编译器工程：", "Generated Magel compiler project: "},
    {"常用配置词：out 路径、opt 级别、reflect、debug、release；new 使用 name 工程名。\n", "Settings: out PATH, opt LEVEL, reflect, debug, release; new accepts name NAME.\n"},
    {"常用配置词：out 路径、reflect、debug；new 使用 name 工程名。\n", "Settings: out PATH, reflect, debug; new accepts name NAME.\n"},
    {"并发与事件", "Concurrency and events"},
    {"开发场景：test tests backend interpret、fmt check src、deps lock project . offline。\n", "Development: test tests backend interpret, fmt check src, deps lock project . offline.\n"},
    {"引用声明必须初始化", "Reference declarations must be initialized"},
    {"引用声明必须初始化，缺省值请使用 Option<T mut&>", "References must be initialized; use Option<T mut&> for an absent value"},
    {"引用存储分配失败", "Reference storage allocation failed"},
    {"手册参数不能组合使用", "Incompatible manual arguments"},
    {"手册参数无效：", "Invalid manual argument: "},
    {"手册数据已生成；重新构建后进入可执行文件。\n", "Manual data generated; rebuild to embed it in the executable.\n"},
    {"手册示例执行失败", "Manual example failed"},
    {"打不开文件：%s\n", "Cannot open file: %s\n"},
    {"找不到变量：", "Variable not found: "},
    {"按值捕获外部变量的副本；原变量仍可继续使用。", "Capture copies of external values; the originals remain usable."},
    {"按值捕获并转移外部变量所有权；创建闭包后，被移出的源变量不能继续读取。", "Capture external values by moving ownership; moved sources cannot be read after closure creation."},
    {"按只读借用捕获外部变量；借用来源必须在闭包使用期间保持有效。", "Capture external values by read-only borrow; sources must remain valid while the closure is used."},
    {"按可写借用捕获外部变量；闭包读写当前值，借用来源必须保持有效。", "Capture external values by mutable borrow; the closure accesses current values and sources must remain valid."},
    {"数学与数值", "Mathematics and numerics"},
    {"文本与数据", "Text and data"},
    {"无法写入工程文件：", "Cannot write project file: "},
    {"无法创建开发工具临时目录", "Cannot create the development tool temporary directory"},
    {"无法创建输出目录：", "Cannot create output directory: "},
    {"无法删除产物：", "Cannot remove artifact: "},
    {"无法启动开发工具", "Cannot start the development tool"},
    {"无法定位 magel 可执行文件", "Cannot locate the magel executable"},
    {"无法检查产物：", "Cannot inspect artifact: "},
    {"无法等待开发工具", "Cannot wait for the development tool"},
    {"无法释放内嵌开发工具", "Cannot extract the embedded development tool"},
    {"映射路径不能覆盖输入或字节码输出\n", "The mapping path must not overwrite input or bytecode output\n"},
    {"未声明的变量：", "Undeclared variable: "},
    {"未找到手册目标：", "Manual target not found: "},
    {"未知类型：", "Unknown type: "},
    {"栈溢出", "Stack overflow"},
    {"核心与类型", "Core and types"},
    {"没有声明的类型：", "Undeclared type: "},
    {"泛型参数缺少 >", "Generic parameters require a closing >"},
    {"泛型声明名称后需要 <...>", "A generic declaration requires <...> after its name"},
    {"注解参数个数过多：", "Too many annotation arguments: "},
    {"注解参数类型不匹配：禁止隐式转换", "Annotation argument type mismatch; implicit conversion is forbidden"},
    {"清理完成，共删除 ", "Cleanup complete; removed "},
    {"清理完成：已删除 %zu 个生成的中间文件\n", "Cleanup complete: removed %zu generated intermediate files\n"},
    {"用法：", "Usage: "},
    {"用法：\n", "Usage:\n"},
    {"用法：magel deps lock|check [--project 目录] [--offline]\n", "Usage: magel deps lock|check [--project DIR] [--offline]\n"},
    {"用法：magel fmt 路径... [--check|--write]\n", "Usage: magel fmt PATH... [--check|--write]\n"},
    {"用法：magel lsp [--macros]\n", "Usage: magel lsp [--macros]\n"},
    {"用法：magel man [目标] [--search 关键词] [--exact] [--json] [--run] [--lang zh|en] [--update]\n", "Usage: magel man [TARGET] [--search KEYWORD] [--exact] [--json] [--run] [--lang zh|en] [--update]\n"},
    {"用法：magel new <目录> [name 工程名]\n", "Usage: magel new <DIR> [name NAME]\n"},
    {"用法：magel new <目录> [name 工程名]\n创建 magel.toml、src/main.mgl、tests/test_main.mgl、README.md 和 .gitignore。\n目标目录必须不存在；父目录必须存在。工程名由英文字母开头，后接字母、数字、- 或 _。\n", "Usage: magel new <DIR> [name NAME]\nCreate magel.toml, src/main.mgl, tests/test_main.mgl, README.md and .gitignore.\nThe destination must not exist and its parent must exist. Names start with a letter, followed by letters, digits, - or _.\n"},
    {"用法：magel test [路径...] [--filter 文本] [--backend interpret] [--timeout 秒] [--json|--list]\n", "Usage: magel test [paths...] [--filter TEXT] [--backend interpret] [--timeout SECONDS] [--json|--list]\n"},
    {"用法：magel 文件.mgl [程序参数...]\n", "Usage: magel FILE.mgl [program arguments...]\n"},
    {"目标已存在，拒绝覆盖", "The destination already exists; refusing to overwrite"},
    {"目标必须是新的工程目录", "The destination must be a new project directory"},
    {"目标目录必须不存在；父目录必须存在。工程名由英文字母开头，后接字母、数字、- 或 _。\n", "The destination must not exist and its parent must exist. Names start with a letter, followed by letters, digits, - or _.\n"},
    {"直接文件调用始终运行；文件后的所有内容传给程序。显式场景的高级选项：\n", "A direct file invocation always runs; all following arguments belong to the program. Advanced options:\n"},
    {"硬实时编译禁止启用反射", "Reflection is forbidden in hard real-time compilation"},
    {"示例输出", "Example output"},
    {"离线缓存缺失：", "Offline cache missing: "},
    {"类型不存在：", "Type does not exist: "},
    {"索引越界", "Index out of bounds"},
    {"线程异步入口不支持带捕获的 lambda", "Thread entry points do not support capturing lambdas"},
    {"统一工具链：VM 执行、导出四元式和 .svmb 字节码。\n", "One toolchain: execute VM programs and export quadruples or .svmb bytecode.\n"},
    {"编译器工程导出失败：", "Compiler project export failed: "},
    {"编译器模板渲染失败：", "Compiler template rendering failed: "},
    {"编译器版本不满足 ", "Compiler version does not satisfy "},
    {"规范缩进；保持字面量和宏不变", "Normalize indentation; preserve literals and macros"},
    {"解析失败", "Parsing failed"},
    {"解析失败：%s\n", "Parsing failed: %s\n"},
    {"该目标没有可运行示例", "No runnable example for this target"},
    {"语法错误：%s 第 %zu 行第 %u 列：%s\n", "Syntax error: %s:%zu:%u: %s\n"},
    {"语法错误：第 %u 行第 %u 列：%s\n", "Syntax error: line %u, column %u: %s\n"},
    {"说明", "Description"},
    {"说明原文语言：zh", "Documentation source language: zh"},
    {"读取失败", "Read failed"},
    {"读取编译器源文件失败\n", "Cannot read compiler source files\n"},
    {"调用形式", "Usage"},
    {"调用栈（外 → 内）：\n", "Call stack (outer to inner):\n"},
    {"输入输出与集成", "I/O and integration"},
    {"输出目录不能是符号链接：", "Output directory must not be a symbolic link: "},
    {"运行失败：", "Execution failed: "},
    {"运行时分配器：内存池容量不足\n", "Runtime allocator: memory pool capacity exceeded\n"},
    {"运行期错误：", "Runtime error: "},
    {"运行示例输出 `Hello, Magel!`。开发命令原生内建，无需 Python；程序通过 VM 执行。\n", "The example prints `Hello, Magel!`. Development commands are built in and need no Python; programs execute in the VM.\n"},
    {"返回值", "Return value"},
    {"返回类型", "return type"},
    {"进入工程目录后执行：magel run src/main.mgl\n", "From the project directory, run: magel run src/main.mgl\n"},
    {"选项 %s 后面要跟一个值\n", "Option %s requires a value\n"},
    {"锁定或离线校验依赖", "Lock dependencies or validate them offline"},
    {"错误与边界", "Errors and boundaries"},
    {"除零", "Division by zero"},
    {"静态成员", "static member"},
    {"非法依赖名称：", "Invalid dependency name: "},
    {"默认值", "Default value"},
    {"，字节数 ", ", byte count "},
    {"；请先运行 magel deps lock", "; run magel deps lock first"},
};
// MAGEL_CATALOG_END
/** @brief 查询编译期内嵌的消息，缺失时保留完整原文。 @param original 中文消息键。 @return 生命周期为进程的 UTF-8 文本。 */
inline const char *text(const char *original){
    const auto chosen=language();if(chosen==Language::Zh)return original;
    std::size_t low=0,high=sizeof(messages)/sizeof(messages[0]);
    while(low<high){const auto mid=low+(high-low)/2;const int order=std::strcmp(original,messages[mid].zh);if(order==0)return messages[mid].en;if(order<0)high=mid;else low=mid+1;}
    return original;
}
/** @brief 提取工具选项并将显式语言传递给开发子进程；-- 后的参数保持原样。
 * @param arguments 规范化后的 argv；成功时删除语言选项。 @param error 失败说明。 @return 选项合法时为 true。
 */
inline bool configure(std::vector<std::string>& arguments,std::string& error){
    for(std::size_t i=1;i<arguments.size();){
        if(arguments[i]=="--")break;
        const bool separate=arguments[i]=="--lang";
        if(!separate && arguments[i].compare(0,7,"--lang=")!=0){++i;continue;}
        if(separate && (i+1==arguments.size() || arguments[i+1]=="--")) {error=text("lang 后需要 zh、en 或 auto");return false;}
        const std::string value=separate?arguments[i+1]:arguments[i].substr(7);
        if(value!="zh" && value!="en" && value!="auto") {error=std::string(text("不支持的显示语言："))+value;return false;}
#ifdef _WIN32
        _putenv_s("MAGEL_LANG",value.c_str());
        _putenv_s("PYTHONIOENCODING","utf-8");
#else
        setenv("MAGEL_LANG",value.c_str(),1);
        setenv("PYTHONIOENCODING","utf-8",1);
#endif
        select(value=="auto"?Language::Auto:parse(value.c_str()));
        arguments.erase(arguments.begin()+i,arguments.begin()+i+(separate?2:1));
    }
    return true;
}
} // namespace magel_i18n
#define MAGEL_TEXT(original) (::magel_i18n::text(original))
#endif
