#include "allocator/magel_allocator.hpp"
namespace toml
{
class Value;
}
#ifndef MAGEL_PACKAGE_HPP
#define MAGEL_PACKAGE_HPP

#include <cstdint>
#include <array>
#include <filesystem>

namespace magel_package
{
struct Project
{
    Memory::String file, contents, name, version, description, license;
    /// 编译器最低版本、依赖清单与锁文件签名。
    Memory::String compiler, lockSignature;
    Memory::Map<Memory::String, Memory::String> dependencies;
    Memory::Vector<Memory::String> authors, imports, searchPaths, includePaths, importPaths;
    /**
     * `[magel] debug`：写 `.svmb` 时是否带调试节，缺省 `false`（不带）。
     *  为 `true` 时等价于命令行加 `--debug`：保留"指令 → 文件/行"位置表，
     *  名字与类型表本来就在。
     */
    bool debug = false;
    /**
     * @brief [magel] compress_symbols：无名变长符号产物，默认关闭。
     */
    bool compressSymbols = false;
    /**
     * @brief [magel] symbol_map：PC 映射文件，空值使用 <前缀>.symbol_map.json。
     */
    Memory::String symbolMap;
    /**
     * `[magel] run`：项目默认运行方式，缺省 `"interpret"`。取值
     *  `interpret` / `svmb`（也接受 `smvb` 拼写）；命令行的子命令或
     *  `--mode` 覆盖它。统一入口 `magel` 据此决定解释 / 导出。
     */
    Memory::String run = "interpret";
    /**
     * `[magel] main`：工程主文件（入口）。指定后命令在不给源文件时默认用它，
     *  程序从该文件的 `main` 开始执行。
     */
    Memory::String main;
    /**
     * @brief 清单内容 + 锁文件内容的签名（锁文件把依赖的签名也拼进来）。
     * @return 清单文件为空（没有项目）时是空串。
     */
    Memory::String signature() const
    {
        if (file.empty()) return Memory::String();
        return file + "\n" + contents + "\n" + lockSignature;
    }
};
/**
 * @brief 去掉首尾空白（空格 / 制表 / 换行）。
 * @param s 输入文本。
 * @return 去掉首尾空白后的副本；整串都是空白时返回空串。
 */
Memory::String trim(const Memory::String &s);

/**
 * @brief 归一化运行方式：把 `smvb` 拼写折到 `svmb`。
 * @param mode 配置里写的运行方式。
 * @return 归一化后的名字；不认识时返回空串（调用方据此报错）。
 */
Memory::String normalizeRunMode(const Memory::String &mode);

/**
 * @brief 模块名是否合法：`a::b::c` 形式的点分标识符（`::` 分段，每段以字母或下划线开头）。
 * @param s 待判定的名字。
 * @return 合法返回 true。
 */
bool moduleName(const Memory::String &s);

/**
 * @brief 归一化工程配置路径：反斜杠转正斜杠，并拒绝空串 / 跨行 / 含 NUL。
 * @param s 配置里写的路径。
 * @return 归一化后的路径。
 * @throws std::runtime_error 路径为空、跨行或含 NUL 时。
 */
Memory::String normalizedPathText(const Memory::String &s);

/**
 * @brief 把配置里的相对路径按包清单所在目录展开成绝对路径。
 * @param s 配置里的路径。
 * @param base 包清单所在目录。
 * @return 规范化的绝对路径（正斜杠）。
 */
Memory::String path(const Memory::String &s, const std::filesystem::path &base);

/**
 * @brief 把文本写成 TOML 双引号字符串（转义 `\` 与 `"`）；`magel new` 生成清单时用。
 * @param s 文本。
 * @return 带引号的字面量。
 */
Memory::String quote(const Memory::String &s);

/*
 * `magel.toml` 的**格式校验**（与 TOML 语法本身分开）：
 *
 *   * 语法（字符串转义、数组、表头、注释、行尾 …）交给仓库自己的 TOML 库
 *     `magel/library/toml`（`library/toml` 的 Magel 内置副本，与 `std::toml` 同一份实现）；
 *   * 这里只回答"**这个格式允许哪些表和键、每个值怎么解释**"：未知表、未知键、
 *     类型不对、该非空却空了一律明确报错，不静默忽略。
 *
 * 报错位置统一写成 `表.键`（例如 `magel.symbol_map`），好让用户直接找到那行配置。
 */

/**
 * @brief 取一个字符串值。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 文本。
 * @throws std::runtime_error 值不是字符串时。
 */
Memory::String tomlText(const toml::Value &value, const Memory::String &where);

/**
 * @brief 取一个布尔值。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 布尔。
 * @throws std::runtime_error 值不是布尔时。
 */
bool tomlBool(const toml::Value &value, const Memory::String &where);

/**
 * @brief 取一个字符串数组（每一项都必须是字符串）。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 各项文本，保持文件里的顺序。
 * @throws std::runtime_error 值不是数组、或某一项不是字符串时。
 */
Memory::Vector<Memory::String> tomlArray(const toml::Value &value,
                                         const Memory::String &where);

/**
 * @brief 当前工具链版本，参与项目最低版本验证。
 */
inline constexpr const char *compilerVersion = "0.25.1";

/**
 * @brief 解析 `1.2.3` 形式的三段版本号。
 * @param text 版本串。
 * @return 三段整数（主 / 次 / 补丁）。
 * @throws std::runtime_error 段数不对、含非数字或结尾有多余内容时（**拒绝模糊范围**）。
 */
std::array<unsigned, 3> versionParts(const Memory::String &text);

/**
 * @brief 计算依赖源码与配置的**确定性内容指纹**（FNV-1a 64 位，16 位十六进制）。
 *
 * 口径（改这里等于让所有 `magel.lock` 失效，动之前先想清楚）：
 *   * 只收源码类后缀（`{mgl,inc,h,hpp,c,cpp,toml,json}`）与 `magel.lock`；
 *   * 跳过 `.git` / `.magel` / `build` / `tmp` / `node_modules` / `__pycache__`；
 *   * 文件名按字典序排序，`CRLF` 统一成 `LF`，名字与内容之间用一个 `\0` 分隔；
 *   * 这是**内容指纹**（防手改），不是安全签名。
 *
 * @param root 依赖根目录。
 * @return 十六进制指纹。
 * @throws std::runtime_error 路径里有符号链接、或源码读不出来时。
 */
Memory::String dependencyFingerprint(const std::filesystem::path &root);

/**
 * @brief `validateDependencies` 的前置声明（`read()` 里用得上）。
 */
bool validateDependencies(Project &project, Memory::String &error);
/**
 * @brief 读取并校验工程清单、目标配置及依赖声明。
 * @param file 工程清单文件的磁盘路径。
 * @param project 成功时接收规范化的工程设置。
 * @param error 失败时接收带清单位置的诊断。
 * @return 清单语法、目标配置和依赖均有效时返回 true。
 */
bool read(const std::filesystem::path &file, Project &project, Memory::String &error);
/**
 * @brief 校验锁定来源、包版本与源码指纹；编译过程绝不联网。
 *
 * 校验链（任何一步不符都直接报错，提示重跑 `magel deps lock`）：
 *   1. `magel.lock` 存在（有依赖才要求）且是本文认识的格式；
 *   2. `[lock] version` 必须是 `1`、`compiler` 非空；
 *   3. 依赖条数与 `magel.toml` 一致，每条的四元数组首项（来源）相同；
 *   4. 本地目录与锁定来源推导出的目录一致，且真在磁盘上；
 *   5. 递归校验依赖自己的 `magel.toml`（顺带检测依赖环）。
 *
 * @param project 项目与依赖搜索路径。
 * @param error 错误文本。
 * @return 全部校验通过时为 true。
 */
bool validateDependencies(Project &project, Memory::String &error);

/**
 * @brief 从入口文件所在目录向上找 `magel.toml`；找不到就用默认项目设置。
 * @param entry 入口 `.mgl` 路径（空 = 当前目录）。
 * @param project 输出：解析出的项目。
 * @param error 错误文本。
 * @return 成功（含"没找到清单"这一正常情况）返回 true。
 */
bool discover(const Memory::String &entry, Project &project, Memory::String &error);
}  // namespace magel_package
#endif
