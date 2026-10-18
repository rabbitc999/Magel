#ifndef GLUE_NO_STD_FILESYSTEM
#ifndef MAGEL_STD_FILESYSTEM_HPP
#include "std_filesystem.hpp"
#endif
#endif
#ifndef PARSE_ENGINE_LANG_NUMERIC_HPP
#include "lang_numeric.hpp"
#endif
#ifndef GLUE_NO_STD_JSON
#ifndef PARSE_ENGINE_STD_JSON_HPP
#include "std_json.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_TOML
#ifndef PARSE_ENGINE_STD_TOML_HPP
#include "std_toml.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_BINARY
#ifndef PARSE_ENGINE_STD_BINARY_HPP
#include "std_binary.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_COMPRESS
#ifndef PARSE_ENGINE_STD_COMPRESS_HPP
#include "std_compress.hpp"
#endif
#endif
/* Standard host bindings share the interpreter implementation.
 * 每个模块用 `#ifndef GLUE_NO_XXX` 包住：定义宏 = 该模块胶水注册整段不编进来。
 * 解释器与导出程序共用同一套宏名（解释器走 glue/std_registry.hpp，导出走本文件）。 */
#ifndef GLUE_NO_STD_MATH
#ifndef PARSE_ENGINE_STD_MATH_HPP
#include "std_math.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_RANDOM
#ifndef PARSE_ENGINE_STD_RANDOM_HPP
#include "std_random.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_REGEX
#ifndef PARSE_ENGINE_STD_REGEX_HPP
#include "std_regex.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_SIGNAL
#ifndef PARSE_ENGINE_STD_SIGNAL_HPP
#include "std_signal.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_LINALG
#ifndef PARSE_ENGINE_STD_LINALG_HPP
#include "std_linalg.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_PARSE
#ifndef PARSE_ENGINE_STD_PARSE_HPP
#include "std_parse.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_PROCESS
#ifndef PARSE_ENGINE_STD_PROCESS_HPP
#include "std_process.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_SQL
#ifndef PARSE_ENGINE_STD_SQL_HPP
#include "std_sql.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_NET
#ifndef PARSE_ENGINE_STD_NET_HPP
#include "std_net.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_IMAGE
#ifndef PARSE_ENGINE_STD_IMAGE_HPP
#include "std_image.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_TREE
#ifndef PARSE_ENGINE_STD_TREE_HPP
#include "std_tree.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_GRAPH
#ifndef PARSE_ENGINE_STD_GRAPH_HPP
#include "std_graph.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_LINKEDLIST
#ifndef PARSE_ENGINE_STD_LINKEDLIST_HPP
#include "std_linkedlist.hpp"
#endif
#endif
#ifndef GLUE_NO_STD_KMP
#ifndef PARSE_ENGINE_STD_KMP_HPP
#include "std_kmp.hpp"
#endif
#endif
/**
 * @brief 为导出程序注册按模块裁剪后的标准库宿主函数。
 * @param registry 接收宿主函数绑定的注册表。
 * @param error 注册失败时写入中文错误信息。
 * @return 所有启用模块均注册成功时返回 true。
 */
bool bindMagelStandardGlue(magel_glue::Registry &registry, Memory::String *error)
{
    bool ok = lang_numeric_glue::bind(registry, error);
#ifndef GLUE_NO_STD_JSON
    ok = std_json_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_TOML
    ok = std_toml_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_BINARY
    ok = std_binary_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_COMPRESS
    ok = std_compress_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_MATH
    ok = std_math::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_RANDOM
    ok = std_random::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_REGEX
    ok = std_regex::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_SIGNAL
    ok = std_signal_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_LINALG
    ok = std_linalg_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_PARSE
    ok = std_parse_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_PROCESS
    ok = std_process_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_SQL
    ok = std_sql_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_NET
    ok = std_net_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_FILESYSTEM
    ok = std_filesystem_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_IMAGE
    ok = std_image_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_TREE
    ok = std_tree_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_GRAPH
    ok = std_graph_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_LINKEDLIST
    ok = std_linkedlist_glue::bind(registry, error) && ok;
#endif
#ifndef GLUE_NO_STD_KMP
    ok = std_kmp_glue::bind(registry, error) && ok;
#endif
    return ok;
}
