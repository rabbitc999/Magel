#include "allocator/magel_allocator.hpp"
#ifndef MAGEL_MODULE_HPP
#define MAGEL_MODULE_HPP

#include "preprocess.hpp"
#include "magel_incremental.hpp"
#include "magel_package.hpp"
#include "magel_stdlib_embedded.cpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace magel_module
{

struct HeaderLine
{
    std::size_t index;
    Memory::String text, rest;
};

struct UnitInfo
{
    Memory::String name;
    Memory::Vector<Memory::String> imports;
    bool hasModule = false;
    Memory::Vector<HeaderLine> headers;
};

inline Memory::String trim(const Memory::String &text)
{
    const std::size_t begin = text.find_first_not_of(" \t\r");
    if (begin == Memory::String::npos) return {};
    const std::size_t end = text.find_last_not_of(" \t\r");
    return text.substr(begin, end - begin + 1u);
}

inline bool identifier(const Memory::String &text)
{
    if (text.empty() || !(std::isalpha(static_cast<unsigned char>(text[0])) || text[0] == '_'))
        return false;
    for (const unsigned char ch : text)
        if (!(std::isalnum(ch) || ch == '_')) return false;
    return true;
}

inline bool moduleName(const Memory::String &text)
{
    std::size_t begin = 0;
    while (begin < text.size())
    {
        const std::size_t end = text.find("::", begin);
        const Memory::String part = text.substr(begin, end == Memory::String::npos ? Memory::String::npos : end - begin);
        if (!identifier(part)) return false;
        if (end == Memory::String::npos) return true;
        begin = end + 2u;
    }
    return false;
}

inline bool directive(const Memory::String &line, const Memory::String &word, Memory::String &name)
{
    const Memory::String value = trim(line);
    if (value.compare(0, word.size(), word) != 0) return false;
    if (value.size() <= word.size() || !std::isspace(static_cast<unsigned char>(value[word.size()])))
        return false;
    if (value.back() != ';') return false;
    name = trim(value.substr(word.size(), value.size() - word.size() - 1u));
    return moduleName(name);
}

inline bool inspect(const preprocess::Result &input, UnitInfo &info, Memory::String &error)
{
    Memory::InputStringStream stream(input.text);
    Memory::String line;
    bool code = false;
    bool blockComment = false;
    std::size_t row = 0;
    while (std::getline(stream, line))
    {
        const auto lineIndex = row++;
        Memory::String visible;
        Memory::Vector<std::size_t> codePositions;
        for (std::size_t i = 0; i < line.size();)
        {
            if (blockComment)
            {
                const std::size_t end = line.find("*/", i);
                if (end == Memory::String::npos)
                {
                    i = line.size();
                    continue;
                }
                blockComment = false;
                i = end + 2u;
                continue;
            }
            if (line.compare(i, 2, "//") == 0) break;
            if (line.compare(i, 2, "/*") == 0)
            {
                blockComment = true;
                i += 2u;
                continue;
            }
            codePositions.push_back(i);
            visible += line[i++];
        }
        visible = trim(visible);
        if (visible.empty()) continue;
        Memory::String name;
        if (directive(visible, "module", name))
        {
            if (info.hasModule)
            {
                error = "同一文件只能声明一次 module";
                return false;
            }
            if (code)
            {
                error = "module 声明必须是文件开头的第一条有效语句";
                return false;
            }
            info.hasModule = true;
            info.name = name;
            Memory::String rest = line;
            for (const auto pos : codePositions) rest[pos] = ' ';
            info.headers.push_back({ lineIndex, visible, rest });
        }
        else if (directive(visible, "import", name))
        {
            if (code)
            {
                error = "import 必须写在模块文件头部、普通声明之前";
                return false;
            }
            info.imports.push_back(name);
            Memory::String rest = line;
            for (const auto pos : codePositions) rest[pos] = ' ';
            info.headers.push_back({ lineIndex, visible, rest });
        }
        else
        {
            code = true;
        }
    }
    return true;
}

inline bool findModule(const Memory::String &name, const Memory::String &fromDir,
                       const Memory::Vector<Memory::String> &includeDirs, Memory::String &path)
{
    namespace fs = std::filesystem;
    Memory::String relative = name;
    for (std::size_t at = 0; (at = relative.find("::", at)) != Memory::String::npos;)
        relative.replace(at, 2u, "/");
    Memory::Vector<fs::path> candidates;
    candidates.push_back(fs::path(fromDir) / (relative + ".mgl"));
    candidates.push_back(fs::path(fromDir) / "modules" / (relative + ".mgl"));
    for (const Memory::String &dir : includeDirs)
        candidates.push_back(fs::path(dir) / (relative + ".mgl"));
    for (const fs::path &candidate : candidates)
    {
        std::error_code ec;
        if (!fs::is_regular_file(candidate, ec)) continue;
        path = fs::weakly_canonical(candidate, ec).string();
        if (path.empty()) path = candidate.lexically_normal().string();
        return true;
    }
    return false;
}

/**
 * 把内嵌的标准库源码合成一份 preprocess::Result（行映射给合成来源，$line/$module 能对上）。
 */
inline preprocess::Result embeddedUnit(const Memory::String &name, const Memory::String &text)
{
    preprocess::Result result;
    result.text = text;
    std::size_t line = 1u;
    std::size_t at = 0u;
    /* 只给每个**内容行**（`\n` 之前的部分）记一条来源；末尾的 `\n` 不额外算一行，
     * 与 preprocess 的 emitLine 语义一致，否则 lineMap 会比 text 多一条、后续行号错位。 */
    while (at < text.size())
    {
        preprocess::LineOrigin origin;
        origin.file = "<内建 " + name + ">";
        origin.line = line++;
        result.lineMap.push_back(origin);
        const std::size_t newline = text.find('\n', at);
        if (newline == Memory::String::npos) break;
        at = newline + 1u;
    }
    return result;
}

class Loader
{
public:
    Loader(preprocess::Options options, Memory::Vector<Memory::String> includeDirs,
           magel_incremental::Cache *cache = nullptr,
           const magel_package::Project *project = nullptr) : options_(std::move(options)), includeDirs_(std::move(includeDirs)), cache_(cache), project_(project)
    {
#if defined(_WIN32)
        const char *os = "windows";
#elif defined(__APPLE__) && defined(__MACH__)
        const char *os = "macos";
#elif defined(__linux__)
        const char *os = "linux";
#else
        const char *os = "other";
#endif
        // Explicit -D definitions can select a deployment target when emitting.
        const auto addDefault = [&](const char *name, const Memory::String &value) {
            for (const auto &define : options_.defines)
                if (define.name == name) return;
            options_.defines.push_back({ name, value });
        };
        addDefault("MAGEL_OS", os);
        addDefault("MAGEL_BITS", Memory::toString(sizeof(void *) * 8));
    }

    bool rootFile(const Memory::String &path, preprocess::Result &output, Memory::String &error)
    {
        preprocessError_ = false;
        preprocess::Result root;
        if (!preprocessRoot(path, Memory::String(), root, error)) return false;
        std::error_code ec;
        const std::filesystem::path absolute = std::filesystem::weakly_canonical(path, ec);
        const Memory::String base = (ec ? std::filesystem::path(path) : absolute).parent_path().string();
        if (!loadStandard(base, output, error)) return false;
        return load(root, base, Memory::String(), true, output, error);
    }

    bool rootText(const Memory::String &text, const Memory::String &label, const Memory::String &base,
                  preprocess::Result &output, Memory::String &error)
    {
        preprocessError_ = false;
        preprocess::Result root;
        if (project_ && !project_->file.empty())
        {
            if (!preprocessRoot(label, text, root, error, base, true)) return false;
        }
        else
        {
            preprocess::Preprocessor preprocessor(options_);
            if (!preprocessor.runText(text, label, base, root)) return diagnostics(root, error);
        }
        if (!loadStandard(base, output, error)) return false;
        return load(root, base, Memory::String(), true, output, error);
    }

    bool preprocessError() const { return preprocessError_; }

    /**
     * @brief 源文件 → 模块名（`$module` 的取值来源）。
     *
     * 一个文件里 `%include` 进来的文件算在**宿主文件所在的那个模块**里，所以这里按
     * 「这个单元的行号映射里出现过的每个源文件」登记，而不只登记主文件。
     */
    const Memory::Map<Memory::String, Memory::String> &moduleByFile() const { return moduleByFile_; }

    /**
     * 某个模块（含经 import 传递加载的）是否真的进了本次编译。宿主胶水按它裁剪注册。
     */
    bool imported(const Memory::String &name) const { return loaded_.count(name) != 0u; }

private:
    bool preprocessRoot(const Memory::String &path, const Memory::String &text,
                        preprocess::Result &root, Memory::String &error,
                        const Memory::String &base = Memory::String(), bool inlineRoot = false)
    {
        if (!project_ || project_->file.empty())
        {
            if (!preprocessFile(path, root)) return diagnostics(root, error);
            return true;
        }
        const Memory::String marker = "// __magel_entry_boundary__";
        Memory::String wrapper;
        for (const auto &include : project_->includes)
        {
            namespace fs = std::filesystem;
            const auto name = Memory::u8path(include);
            const auto fromProject = Memory::u8path(project_->file).parent_path() / name;
            auto resolved = fromProject;
            std::error_code ec;
            if (!name.is_absolute() && !fs::is_regular_file(fromProject, ec))
            {
                for (const auto &directory : options_.includeDirs)
                {
                    const auto candidate = Memory::u8path(directory) / name;
                    if (fs::is_regular_file(candidate, ec))
                    {
                        resolved = candidate;
                        break;
                    }
                }
            }
            wrapper += "%include " + magel_package::quote(fs::absolute(resolved).lexically_normal().generic_u8string()) + "\n";
        }
        const auto boundaryLine = project_->includes.size() + 1u;
        wrapper += marker + "\n";
        if (inlineRoot)
            wrapper += text;
        else
            wrapper += "%include " + magel_package::quote(std::filesystem::absolute(Memory::u8path(path)).generic_u8string()) + "\n";
        preprocess::Result combined;
        const Memory::String label = inlineRoot ? path : Memory::String("<magel entry>");
        const Memory::String directory = inlineRoot ? base : Memory::String(Memory::u8path(project_->file).parent_path().generic_u8string());
        bool ok;
        if (cache_ && !inlineRoot)
            ok = cache_->preprocessTextUnit(path, options_, wrapper, label, directory, project_->file, combined);
        else
        {
            preprocess::Preprocessor processor(options_);
            ok = processor.runText(wrapper, label, directory, combined);
            combined.dependencies.push_back(project_->file);
        }
        if (!ok)
        {
            root = combined;
            return diagnostics(root, error);
        }
        preprocess::Result prefix, entry;
        bool reached = false;
        Memory::InputStringStream lines(combined.text);
        Memory::String line;
        std::size_t index = 0;
        while (std::getline(lines, line))
        {
            const auto origin = combined.lineMap.at(index++);
            if (!reached && origin.file == label && origin.line == boundaryLine && line == marker)
            {
                reached = true;
                continue;
            }
            auto &target = reached ? entry : prefix;
            target.text += line + "\n";
            auto mapped = origin;
            if (inlineRoot && reached && mapped.file == label) mapped.line -= boundaryLine;
            target.lineMap.push_back(mapped);
        }
        if (!reached)
        {
            error = "magel entry boundary was lost";
            return false;
        }
        UnitInfo entryInfo, prefixInfo;
        if (!inspect(entry, entryInfo, error) || !inspect(prefix, prefixInfo, error)) return false;
        if (prefixInfo.hasModule)
        {
            error = "magel includes must not declare module";
            return false;
        }
        // Keep the original entry's module/import header ahead of package values.
        preprocess::Result header;
        auto extractHeader = [&](preprocess::Result &unit, const UnitInfo &info) {
            Memory::InputStringStream stream(unit.text);
            Memory::String rebuilt, current;
            std::size_t row = 0, next = 0;
            while (std::getline(stream, current))
            {
                if (next < info.headers.size() && info.headers[next].index == row)
                {
                    const auto &declaration = info.headers[next++];
                    header.text += declaration.text + "\n";
                    header.lineMap.push_back(unit.lineMap[row]);
                    rebuilt += declaration.rest + "\n";
                }
                else
                    rebuilt += current + "\n";
                ++row;
            }
            unit.text = rebuilt;
        };
        extractHeader(entry, entryInfo);
        extractHeader(prefix, prefixInfo);
        for (const auto &name : project_->imports)
        {
            header.text += "import " + name + ";\n";
            header.lineMap.push_back({ project_->file, 1u });
        }
        root = header;
        append(root, prefix);
        append(root, entry);
        root.dependencies = combined.dependencies;
        return true;
    }

    bool preprocessFile(const Memory::String &path, preprocess::Result &result)
    {
        if (cache_ != nullptr) return cache_->preprocessFile(path, options_, result);
        preprocess::Preprocessor preprocessor(options_);
        return preprocessor.runFile(path, result);
    }

    bool loadStandard(const Memory::String &base, preprocess::Result &output,
                      Memory::String &error)
    {
        Memory::String embeddedText;
        if (embedded::find("std::lang", &embeddedText))
            return load(embeddedUnit("std::lang", embeddedText), base, "std::lang",
                        false, output, error);
        Memory::String path;
        if (!findModule("std::lang", base, includeDirs_, path))
        {
            error = "找不到 Magel 标准库 std/lang.mgl（可用 -I 指定其所在目录）";
            return false;
        }
        preprocess::Result unit;
        if (!preprocessFile(path, unit)) return diagnostics(unit, error);
        if (!load(unit, std::filesystem::path(path).parent_path().string(), "std::lang",
                  false, output, error)) return false;
        output.dependencies.push_back(path);
        return true;
    }

    bool diagnostics(const preprocess::Result &result, Memory::String &error)
    {
        preprocessError_ = true;
        if (result.diagnostics.empty())
            error = "模块前处理失败";
        else
            error = result.diagnostics.front().toString();
        return false;
    }

    void append(preprocess::Result &output, const preprocess::Result &input)
    {
        if (!output.text.empty() && output.text.back() != '\n')
        {
            output.text += '\n';
            /* 补的换行也算一行（parser 按 `\n` 数行），必须同步补一个 lineMap 占位，
             * 否则 text 的行数比 lineMap 条目多、后续行号整体偏一位。 */
            preprocess::LineOrigin pad;
            if (!input.lineMap.empty())
                pad = input.lineMap.front();
            else if (!output.lineMap.empty())
                pad = output.lineMap.back();
            output.lineMap.push_back(pad);
        }
        output.text += input.text;
        output.lineMap.insert(output.lineMap.end(), input.lineMap.begin(), input.lineMap.end());
        output.dependencies.insert(output.dependencies.end(), input.dependencies.begin(),
                                   input.dependencies.end());
    }

    bool load(const preprocess::Result &unit, const Memory::String &base,
              const Memory::String &expected, bool root, preprocess::Result &output,
              Memory::String &error)
    {
        UnitInfo info;
        if (!inspect(unit, info, error)) return false;
        /* 入口文件可以没有 `module` 声明。那样它在合并源码里会"继承"上一段的模块名
         * （通常是 lang.mgl 的 `lang`），而"当前是不是用户模块"直接决定保留名与 `std::lang::`
         * 的判定。所以给入口补一行合成的根模块声明（`__root` 映射回空模块名）。 */
        preprocess::Result effective = unit;
        if (root && !info.hasModule)
        {
            preprocess::Result header;
            header.text = "module __root;\n";
            header.lineMap.push_back(preprocess::LineOrigin{ "<入口>", 1u });
            effective.text = header.text + unit.text;
            effective.lineMap = header.lineMap;
            effective.lineMap.insert(effective.lineMap.end(), unit.lineMap.begin(), unit.lineMap.end());
            effective.dependencies = unit.dependencies;
            info.hasModule = true;
            info.name = "__root";
        }
        /* 模块根保留：`lang` 与 `std` 归标准库。导入方（root=false）本来就按 expected
         * 核对模块名，这里挡的是**入口文件**自称标准库模块。 */
        if (root && (info.name == "std::lang" || info.name == "std" || info.name.compare(0u, 5u, "std::") == 0u))
        {
            error = "模块名 " + info.name + " 保留给标准库（核心 prelude 是 std::lang，按需模块是 std::xxx）："
                                            "用户文件不能用这个名字";
            return false;
        }
        if (!root && !info.hasModule)
        {
            error = "导入文件必须以 `module " + expected + ";` 声明模块";
            return false;
        }
        if (!expected.empty() && info.name != expected)
        {
            error = "import " + expected + " 找到的文件声明的是 module " + info.name;
            return false;
        }
        const Memory::String key = info.hasModule ? info.name : Memory::String("__root");
        if (visiting_.count(key) != 0u)
        {
            error = "模块循环导入：" + key;
            return false;
        }
        if (!root && loaded_.count(key) != 0u) return true;
        visiting_.insert(key);
        /* 这个单元（连同它 `%include` 的文件）属于哪个模块 —— `$module` 就查这张表。 */
        for (const preprocess::LineOrigin &origin : effective.lineMap)
            if (!origin.file.empty()) moduleByFile_[origin.file] = key;
        for (const Memory::String &imported : info.imports)
        {
            if (imported == "std::lang") continue;
            /* 嵌入式模式（`magel.toml` 的 `[embedded] enabled` + `restrict_modules`）：
             * 用到被限制的模块就在这里**编译期**拦住 —— 这些模块在嵌入式运行时里没有
             * 后端，与其等到运行时报"函数入口不存在"，不如在编译期直接拒绝。 */
            if (project_ != nullptr && project_->embeddedEnabled && magel_package::embeddedDeniedModules(*project_).count(imported) != 0u)
            {
                error = "嵌入式模式不允许使用模块 " + imported + "：它在嵌入式运行时里没有后端；"
                                                                 "确需放开请改 magel.toml 的 [embedded] restrict_modules";
                return false;
            }
            /* `std::algorithms` 没有 MAGEL 文件，但仍在合并源码里保留模块边界。限定调用由
             * 编译器直接降级到 StackVM 胶水，不在这里注入函数体。 */
            if (imported == "std::algorithms")
            {
                if (loaded_.insert(imported).second)
                {
                    preprocess::Result intrinsic;
                    intrinsic.text = "module std::algorithms;\n";
                    intrinsic.lineMap.push_back(
                        preprocess::LineOrigin{ "<内建 std::algorithms>", 1u });
                    append(output, intrinsic);
                }
                continue;
            }
            Memory::String embeddedText;
            if (embedded::find(imported, &embeddedText))
            {
                preprocess::Result child = embeddedUnit(imported, embeddedText);
                if (!load(child, base, imported, false, output, error)) return false;
                continue;
            }
            Memory::String path;
            if (!findModule(imported, base, includeDirs_, path))
            {
                error = "找不到模块 " + imported + "（需要 " + imported + ".mgl）";
                return false;
            }
            preprocess::Result child;
            if (!preprocessFile(path, child)) return diagnostics(child, error);
            if (!load(child, std::filesystem::path(path).parent_path().string(), imported,
                      false, output, error)) return false;
            output.dependencies.push_back(path);
        }
        visiting_.erase(key);
        loaded_.insert(key);
        if (root && !info.hasModule && !output.text.empty())
        {
            preprocess::Result reset;
            reset.text = "module __root;\n";
            preprocess::LineOrigin origin;
            origin.file = "<module root>";
            origin.line = 1u;
            reset.lineMap.push_back(origin);
            append(output, reset);
        }
        append(output, effective);
        return true;
    }

    preprocess::Options options_;
    Memory::Vector<Memory::String> includeDirs_;
    Memory::Set<Memory::String> visiting_;
    Memory::Set<Memory::String> loaded_;
    Memory::Map<Memory::String, Memory::String> moduleByFile_;
    magel_incremental::Cache *cache_ = nullptr;
    bool preprocessError_ = false;
    const magel_package::Project *project_ = nullptr;
};

}  // namespace magel_module

#endif
