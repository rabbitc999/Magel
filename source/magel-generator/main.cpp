#include "../runtime_allocator.hpp"
#include "../../library/template/engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

const char *const kManifestName = ".magel-generator-manifest";
const char *const kManifestHeader = "# Magel generator files v1";

bool readFile(const fs::path &path, Memory::String *content)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    Memory::OutputStringStream output;
    output << input.rdbuf();
    if (!input.eof() && input.fail()) return false;
    *content = output.str();
    return true;
}

bool safeRelative(const Memory::String &name, fs::path *relative)
{
    *relative = fs::path(name).lexically_normal();
    if (relative->empty() || *relative == "." || relative->is_absolute() ||
        relative->has_root_path() || relative->generic_string() == kManifestName)
        return false;
    return std::none_of(relative->begin(), relative->end(),
                        [](const fs::path &part) { return part == ".."; });
}

bool rejectSymlinkParents(const fs::path &root, const fs::path &relative,
                          Memory::String *error)
{
    fs::path current = root;
    for (const fs::path &part : relative.parent_path())
    {
        current /= part;
        std::error_code statusError;
        const fs::file_status status = fs::symlink_status(current, statusError);
        if (statusError && statusError != std::errc::no_such_file_or_directory)
        {
            *error = "无法检查编译器产物目录：" + current.string();
            return false;
        }
        if (!statusError && fs::is_symlink(status))
        {
            *error = "编译器产物目录不能经过符号链接：" + current.string();
            return false;
        }
    }
    return true;
}

bool readManifest(const fs::path &root, Memory::Set<Memory::String> *files,
                  Memory::String *error)
{
    files->clear();
    const fs::path manifest = root / kManifestName;
    std::error_code statusError;
    if (!fs::exists(manifest, statusError)) return !statusError;
    std::ifstream input(manifest, std::ios::binary);
    Memory::String line;
    if (!input || !std::getline(input, line))
    {
        *error = "无法读取编译器产物清单：" + manifest.string();
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != kManifestHeader)
    {
        *error = "编译器产物清单版本无法识别：" + manifest.string();
        return false;
    }
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        fs::path relative;
        if (!safeRelative(line, &relative))
        {
            *error = "编译器产物清单包含不安全路径：" + line;
            return false;
        }
        files->insert(relative.generic_string());
    }
    return true;
}

bool removeManaged(const fs::path &root, const Memory::Set<Memory::String> &files,
                   const Memory::Set<Memory::String> &kept, std::size_t *removed,
                   Memory::String *error)
{
    Memory::Vector<fs::path> parents;
    for (const Memory::String &name : files)
    {
        if (kept.count(name) != 0u) continue;
        fs::path relative;
        if (!safeRelative(name, &relative))
        {
            *error = "编译器产物清单包含不安全路径：" + name;
            return false;
        }
        if (!rejectSymlinkParents(root, relative, error)) return false;
        const fs::path target = root / relative;
        std::error_code statusError;
        const bool exists = fs::exists(target, statusError) || fs::is_symlink(target, statusError);
        if (statusError)
        {
            *error = "无法检查旧产物：" + target.string();
            return false;
        }
        if (exists)
        {
            std::error_code removeError;
            if (!fs::remove(target, removeError) || removeError)
            {
                *error = "无法删除旧产物：" + target.string();
                return false;
            }
            ++*removed;
        }
        for (fs::path parent = target.parent_path(); parent != root && !parent.empty();
             parent = parent.parent_path())
            parents.push_back(parent);
    }
    std::sort(parents.begin(), parents.end(), [](const fs::path &left, const fs::path &right) {
        return left.native().size() == right.native().size()
                   ? left.native() < right.native()
                   : left.native().size() > right.native().size();
    });
    parents.erase(std::unique(parents.begin(), parents.end()), parents.end());
    for (const fs::path &parent : parents)
    {
        std::error_code errorCode;
        if (fs::is_empty(parent, errorCode)) fs::remove(parent, errorCode);
    }
    return true;
}

bool writeProject(const fs::path &root,
                  const Memory::Map<Memory::String, Memory::String> &project,
                  Memory::String *error)
{
    std::error_code directoryError;
    fs::create_directories(root, directoryError);
    if (directoryError)
    {
        *error = MAGEL_TEXT("无法创建输出目录：") + directoryError.message();
        return false;
    }
    Memory::Set<Memory::String> current;
    for (const auto &[name, content] : project)
    {
        (void)content;
        fs::path relative;
        if (!safeRelative(name, &relative))
        {
            *error = "不安全的编译器产物路径：" + name;
            return false;
        }
        if (!rejectSymlinkParents(root, relative, error)) return false;
        current.insert(relative.generic_string());
    }
    Memory::Set<Memory::String> old;
    if (!readManifest(root, &old, error)) return false;
    for (const auto &[name, content] : project)
    {
        const fs::path target = root / fs::path(name);
        fs::create_directories(target.parent_path(), directoryError);
        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            *error = "无法写入编译器产物：" + target.string();
            return false;
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    std::size_t removed = 0u;
    if (!removeManaged(root, old, current, &removed, error)) return false;
    std::ofstream manifest(root / kManifestName, std::ios::binary | std::ios::trunc);
    if (!manifest)
    {
        *error = "无法写入编译器产物清单";
        return false;
    }
    manifest << kManifestHeader << '\n';
    for (const Memory::String &name : current) manifest << name << '\n';
    return static_cast<bool>(manifest);
}

Memory::String shellQuote(const Memory::String &text)
{
#if defined(_WIN32)
    Memory::String out = "\"";
    for (const char ch : text) out += ch == '"' ? "\\\"" : Memory::String(1u, ch);
    return out + "\"";
#else
    Memory::String out = "'";
    for (const char ch : text) out += ch == '\'' ? "'\\''" : Memory::String(1u, ch);
    return out + "'";
#endif
}

/* 把「文件在导出工程里的位置」换算成相对它的 `#include` 写法。
 * 例：文件在 `runtime/`、目标在 `runtime/magel_allocator.hpp` -> `magel_allocator.hpp`；
 *     文件在 `glue/`、目标在 `json/json.hpp`            -> `../json/json.hpp`。 */
Memory::String relativeInclude(const Memory::String &target, const char *destination)
{
    const std::string to(destination);
    std::string dir;
    const std::size_t slash = target.rfind('/');
    if (slash != Memory::String::npos) dir.assign(target.c_str(), slash);

    std::vector<std::string> from;
    std::vector<std::string> parts;
    if (!dir.empty())
        for (std::size_t at = 0;;)
        {
            const std::size_t next = dir.find('/', at);
            from.push_back(dir.substr(at, next == std::string::npos ? std::string::npos : next - at));
            if (next == std::string::npos) break;
            at = next + 1u;
        }
    for (std::size_t at = 0;;)
    {
        const std::size_t next = to.find('/', at);
        parts.push_back(to.substr(at, next == std::string::npos ? std::string::npos : next - at));
        if (next == std::string::npos) break;
        at = next + 1u;
    }
    std::size_t common = 0;
    while (common < from.size() && common + 1u < parts.size() && from[common] == parts[common]) ++common;
    std::string out;
    for (std::size_t i = common; i < from.size(); ++i) out += "../";
    for (std::size_t i = common; i < parts.size(); ++i)
    {
        out += parts[i];
        if (i + 1u < parts.size()) out += '/';
    }
    return Memory::String(out.c_str());
}

int run(const Memory::Vector<Memory::String> &arguments)
{
    Memory::String command;
#if defined(_WIN32)
    // cmd /c strips one outer quote pair; keep it separate from per-argument quoting.
    command = "\"";
#endif
    for (const Memory::String &argument : arguments)
    {
        if (!command.empty()) command += ' ';
        command += shellQuote(argument);
    }
#if defined(_WIN32)
    command += "\"";
#endif
    return std::system(command.c_str());
}

void usage(const Memory::String &program)
{
    std::cout << MAGEL_TEXT("用法：\n")
              << "  " << program << " compiler --out-dir DIR\n"
              << "  " << program << " clear --out-dir DIR\n";
}

} // namespace

namespace magel_generator
{

int runCli(int argc, char **argv)
{
    const fs::path self = fs::absolute(argc > 0 ? argv[0] : "magel-generator").lexically_normal();
    fs::path root = self.parent_path().parent_path().parent_path();
    int commandIndex = 1;
    /* 统一入口在进程内调用时会显式给仓库根：`runCli({"magel-generator","--root",ROOT,"compiler",...})`。
     * 独立二进制的 argv[0] 在 build/magel-generator/ 下，往上三级正好是仓库根，不用传。 */
    if (argc > 2 && Memory::String(argv[1]) == "--root")
    {
        std::error_code rootError;
        root = fs::absolute(fs::path(argv[2]), rootError).lexically_normal();
        commandIndex = 3;
    }
    // 独立仓库的 build/magel-generator 位于 Magel 自己的根下。
    if (fs::exists(root / "source/magel.gram")) root = root.parent_path();
    const fs::path mc = root / "magel";
    const fs::path templateDir = mc / "source/magel-generator";
    const fs::path parseEngine = root / "build/bin/ParseEngine.exe";
    const fs::path parseEnginePortable = root / "build/bin/ParseEngine";
    const fs::path emitter = root / "build/magel-emit/magel-emit.exe";
    const fs::path emitterPortable = root / "build/magel-emit/magel-emit";
    if (argc <= commandIndex)
    {
        usage(self.string());
        return 2;
    }
    const Memory::String command = argv[commandIndex];
    Memory::String outDir;
    Memory::Vector<Memory::String> forwarded;
    for (int index = commandIndex + 1; index < argc; ++index)
    {
        const Memory::String argument = argv[index];
        if (argument == "--out-dir" && index + 1 < argc)
        {
            outDir = argv[++index];
        }
        else
        {
            forwarded.push_back(argument);
        }
    }
    if (outDir.empty())
    {
        std::cerr << "必须指定 --out-dir DIR\n";
        return 2;
    }
    const fs::path output = fs::absolute(outDir).lexically_normal();
    const fs::path emitterExe = fs::exists(emitter) ? emitter : emitterPortable;
    if (command == "clear")
    {
        if (!forwarded.empty())
        {
            std::cerr << "clear 只接受 --out-dir\n";
            return 2;
        }
        Memory::String error;
        Memory::Set<Memory::String> files;
        if (!readManifest(output, &files, &error))
        {
            std::cerr << error << '\n';
            return 1;
        }
        std::size_t removed = 0u;
        if (!removeManaged(output, files, {}, &removed, &error))
        {
            std::cerr << error << '\n';
            return 1;
        }
        std::error_code removeError;
        fs::remove(output / kManifestName, removeError);
        if (fs::exists(emitterExe) && run({emitterExe.string(), "clear", "--out-dir", outDir}) != 0)
            return 1;
        std::cout << MAGEL_TEXT("清理完成，共删除 ") << removed << MAGEL_TEXT(" 个编译器文件\n");
        return 0;
    }
    if (command != "compiler")
    {
        usage(self.string());
        return 2;
    }
    if (!forwarded.empty()) { usage(self.string()); return 2; }

    fs::path parser = parseEnginePortable;
    for (const auto &candidate : {mc / "ParseEngine.exe", mc / "ParseEngine",
            root / "ParseEngine/build/bin/ParseEngine.exe", root / "ParseEngine/build/bin/ParseEngine",
            parseEngine, parseEnginePortable}) {
        if (fs::exists(candidate)) { parser = candidate; break; }
    }
    if (!fs::exists(parser))
    {
        std::cerr << "找不到 ParseEngine：" << parser.string() << '\n';
        return 2;
    }
    const fs::path stage = root / "tmp" /
        ("magel-generator-" + Memory::toString(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code stageError;
    fs::create_directories(stage, stageError);
    const int generated = run({parser.string(), (mc / "source/magel.gram").string(),
                               "--mode", "ielr", "--out-dir", stage.string()});
    if (generated != 0)
    {
        fs::remove_all(stage, stageError);
        return 1;
    }

    Memory::Map<Memory::String, Memory::String> project;
    /* 复制到导出工程会把源文件摊平：源文件里指向仓库原布局的相对 `#include` 必须
     * 改写成新布局下的相对路径。表给出「原写法 -> 该文件在导出工程里的位置」，
     * 匹配一律带引号，且长串排在前面。 */
    const auto rewriteIncludes = [](Memory::String *content, const Memory::String &target) {
        struct Fix { const char *from; const char *to; };
        static const Fix fixes[] = {
            {"../../library/allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"../../source/allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"../library/allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"../allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"../../source/magel_i18n.hpp", "magel_i18n.hpp"},
            {"../magel_i18n.hpp", "magel_i18n.hpp"},
            {"../../library/json/json.hpp", "json/json.hpp"},
            {"../json/json.hpp", "json/json.hpp"},
            {"../../library/json/json.cpp", "json/json.cpp"},
            {"../json/json.cpp", "json/json.cpp"},
            {"../../library/toml/toml.hpp", "toml/toml.hpp"},
            {"../toml/toml.hpp", "toml/toml.hpp"},
            {"../../library/toml/toml.cpp", "toml/toml.cpp"},
            {"../toml/toml.cpp", "toml/toml.cpp"},
        };
        for (const Fix &fix : fixes) {
            /* 只在 `#include "..."` 的引号里匹配：否则裸的 `allocator/magel_allocator.hpp`
             * 会连带命中 `"../stackvm_mcu/allocator/magel_allocator.hpp"` 的尾巴。 */
            const Memory::String needle = Memory::String("\"") + fix.from + "\"";
            const Memory::String value = Memory::String("\"") + relativeInclude(target, fix.to) + "\"";
            if (needle == value) continue;
            std::size_t at = 0;
            while ((at = content->find(needle, at)) != Memory::String::npos) {
                content->replace(at, needle.size(), value);
                at += value.size();
            }
        }
    };
    const auto copy = [&](const fs::path &source, const Memory::String &target) -> bool {
        Memory::String content;
        if (!readFile(source, &content)) return false;
        rewriteIncludes(&content, target);
        project[target] = content;
        return true;
    };
    bool ok = true;
    for (const char *name : {"magel_parser.cpp", "magel_parser.hpp",
                             "magel_parser_user.h", "magel_main.cpp", "RESULT.md"})
        ok = copy(stage / name, name) && ok;
    for (const char *name : {"magel_driver.cpp", "magel_i18n.hpp", "magel_diagnostics.hpp", "magel_borrow_checker.hpp", "magel_borrow_flow.hpp", "magel_macro.hpp", "magel_macro_controls.hpp", "magel_module.hpp",
                             "magel_incremental.hpp", "magel_package.hpp", "magel_project.hpp", "magel_cli.hpp",
                             "magel_lambda.hpp", "magel_generic.hpp", "magel_generic_name.hpp", "magel_glue.hpp",
                             "magel_stdlib_embedded.cpp", "runtime_allocator.hpp",
                             "magel_types.h", "magel_epilogue.cpp"})
        ok = copy(mc / "source" / name, name) && ok;
    /* 控制台 UTF-8 头来自 `library/console/`，导出工程里仍放 `console/` 的目录名：副本站 `console/utf8.hpp`，
     * `magel_i18n.hpp` 里也是 `#include "console/utf8.hpp"`，导出工程保持同一相对布局。 */
    ok = copy(mc / "library/console/utf8.hpp", "console/utf8.hpp") && ok;
    /* `magel_epilogue.cpp` 按职责拆成 `epilogue/*.cpp`，它用相对 `#include`
     * 拼回；生成工程必须把分片一起带过去（编译 `magel_parser.cpp` 时按相对路径找到）。 */
    for (const char *name : {"symbol_table.cpp", "type_ids.cpp", "type_ids_shared.cpp",
                             "lifetime.cpp", "aggregate_ownership.cpp", "scope.cpp",
                             "return_expression_ast.cpp", "named_function_ast.cpp", "derive_ast.cpp", "derive.cpp",
                             "types.cpp", "interfaces.cpp", "builtins.cpp", "generic_inference.cpp", "constants.cpp", "annotation_ast.cpp", "annotations.cpp", "reflection_metadata.cpp", "reflection_queries.cpp", "reflection_calls.cpp",
                             "lowering.cpp", "borrow_facts.cpp", "borrow_flow.cpp", "borrow_return_sources.cpp", "expression_effects.cpp", "interface_dispatch.cpp", "boxing.cpp",
                             "pass1_register.cpp", "pass2_lower.cpp", "c_exports.cpp", "artifacts.cpp"})
        ok = copy(mc / "source/epilogue" / name, Memory::String("epilogue/") + name) && ok;
    const Memory::Map<Memory::String, fs::path> runtime{
        {"magel_allocator.hpp", root / "magel/source/allocator/magel_allocator.hpp"},
        {"magel_pool.h", root / "magel/source/allocator/magel_pool.h"},
        {"magel_pool.c", root / "magel/source/allocator/magel_pool.c"},
        {"simple_ast.cpp", root / "magel/library/simple_ast/simple_ast.cpp"},
        {"simple_ast.hpp", root / "magel/library/simple_ast/simple_ast.hpp"},
        {"preprocess.cpp", root / "magel/library/preprocess/preprocess.cpp"},
        {"preprocess.hpp", root / "magel/library/preprocess/preprocess.hpp"},
        {"stackvm.cpp", root / "magel/library/stackvm/stackvm.cpp"},
        {"stackvm.hpp", root / "magel/library/stackvm/stackvm.hpp"},
        {"value_text.hpp", root / "magel/library/stackvm/value_text.hpp"},
        {"reflection.hpp", root / "magel/library/stackvm/reflection.hpp"},
        {"execution_plan.hpp", root / "magel/library/stackvm/execution_plan.hpp"},
        {"register_core.hpp", root / "magel/library/stackvm/register_core.hpp"},
        {"allocation_index.hpp", root / "magel/library/stackvm/allocation_index.hpp"},
        {"data_capacity_index.hpp", root / "magel/library/stackvm/data_capacity_index.hpp"},
        {"stackvm_glue.cpp", root / "magel/library/stackvm/stackvm_glue.cpp"},
        {"stackvm_glue.hpp", root / "magel/library/stackvm/stackvm_glue.hpp"},
        {"bigint.cpp", root / "magel/library/bignum/bigint.cpp"},
        {"bigint.hpp", root / "magel/library/bignum/bigint.hpp"},
        {"bigfloat.cpp", root / "magel/library/bignum/bigfloat.cpp"},
        {"bigfloat.hpp", root / "magel/library/bignum/bigfloat.hpp"},
    };
    for (const auto &[name, source] : runtime) ok = copy(source, "runtime/" + name) && ok;
    /* `stackvm.cpp` 同样拆成 `parts/` 下的 `.cpp`，随 runtime/stackvm.cpp 一起复制。 */
    for (const char *name : {"symbol_build.cpp", "symbol_container.cpp",
                             "machine_natives.cpp", "machine_value_text.cpp", "quads.cpp", "assembly.cpp",
                             "execution.cpp", "execution_fusion.inc", "register_execution.inc", "optimization_flow.inc", "aot_bridge.cpp"})
        ok = copy(root / "magel/library/stackvm/parts" / name,
                  Memory::String("runtime/parts/") + name) && ok;
    for (const char *format : {"json", "toml"}) {
        for (const char *extension : {"hpp", "cpp"}) {
            const Memory::String relative = Memory::String(format) + "/" + format + "." + extension;
            ok = copy(root / "magel/library" / relative, relative) && ok;
        }
    }
    for (const char *name : {"std_math.hpp", "std_random.hpp", "std_regex.hpp",
                             "std_signal.hpp",
                             "structured_data.hpp", "std_json.hpp", "std_toml.hpp", "lang_numeric.hpp", "std_binary.hpp", "std_linalg.hpp", "std_parse.hpp",
                             "std_process.hpp", "std_sql.hpp", "std_net.hpp", "std_filesystem.hpp", "std_registry.hpp"})
        ok = copy(root / "magel/source/glue" / name, Memory::String("glue/") + name) && ok;
    Memory::String langSource;
    Memory::String langTemplate;
    Memory::String cmakeTemplate;
    Memory::String readmeTemplate;
    if (!readFile(mc / "source/std/lang.mgl", &langSource) ||
        !readFile(templateDir / "compiler_lang.mgl.tmpl", &langTemplate) ||
        !readFile(templateDir / "compiler_CMakeLists.txt.tmpl", &cmakeTemplate) ||
        !readFile(templateDir / "compiler_README.md.tmpl", &readmeTemplate))
        ok = false;
    try
    {
        tmpl::SnippetTable snippets;
        snippets.emplace("magel_lang", langSource);
        project["std/lang.mgl"] = tmpl::renderValues(
            langTemplate, {}, snippets);
        project["CMakeLists.txt"] = tmpl::renderValues(
            cmakeTemplate);
        project["README.md"] = tmpl::renderValues(
            readmeTemplate);
    }
    catch (const std::exception &exception)
    {
        std::cerr << MAGEL_TEXT("编译器模板渲染失败：") << exception.what() << '\n';
        ok = false;
    }
    fs::remove_all(stage, stageError);
    if (!ok)
    {
        std::cerr << MAGEL_TEXT("读取编译器源文件失败\n");
        return 1;
    }
    Memory::String error;
    if (!writeProject(output, project, &error))
    {
        std::cerr << MAGEL_TEXT("编译器工程导出失败：") << error << '\n';
        return 1;
    }
    std::cout << MAGEL_TEXT("已生成 Magel 编译器工程：") << output.string() << "（"
              << project.size() << MAGEL_TEXT(" 个文件，lang.mgl 来自引用模板）\n");
    return 0;
}

} // namespace magel_generator

/* 独立 `magel-generator` 二进制自带 main；统一入口在进程内调用 `magel_generator::runCli`，
 * 构建时用 `-DMAGEL_GENERATOR_NO_MAIN` 关掉这里。 */
#ifndef MAGEL_GENERATOR_NO_MAIN
int main(int argc, char **argv) {
    auto arguments=magel_i18n::processArguments(argc,argv);
    std::string error;
    if(!magel_i18n::configure(arguments,error)){std::fprintf(stderr,"magel: %s\n",error.c_str());return 2;}
    std::vector<char*> values;
    for(auto &argument:arguments)values.push_back(argument.data());
    values.push_back(nullptr);
    return magel_generator::runCli(static_cast<int>(arguments.size()),values.data());
}
#endif
