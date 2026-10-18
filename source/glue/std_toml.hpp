#ifndef PARSE_ENGINE_STD_TOML_HPP
#define PARSE_ENGINE_STD_TOML_HPP
#include "../../library/toml/toml.hpp"
#include "../../library/toml/toml.cpp"
#include "structured_data.hpp"

/**
 * @brief TOML 库与 MAGEL 公共文档胶水的适配。
 */
namespace std_toml_glue
{
/**
 * @brief TOML 的解析、类型与序列化策略。
 */
struct Traits
{
    using Value = toml::Value;
    /**
     * @brief 解析文本。
     * @param text UTF-8 文本。
     * @return 根值及诊断。
     */
    static auto parse(std::string_view text) { return toml::parse(text); }
    /**
     * @brief 返回 MAGEL 类型编号。
     * @param v 节点。
     * @return 类型编号。
     */
    static std::int64_t kind(const Value &v)
    {
        if (v.isBool()) return 1;
        if (v.isInt()) return 2;
        if (v.isDouble()) return 3;
        if (v.isString()) return 4;
        if (v.isArray()) return 5;
        if (v.isObject()) return 6;
        return 7;
    }
    /**
     * @brief 提取日期时间文本。
     * @param v 节点。
     * @return 文本或空串。
     */
    static std::string dateTime(const Value &v)
    {
        const auto date = v.asDateTime();
        return date ? date->toString() : std::string();
    }
    /**
     * @brief 序列化节点。
     * @param v 节点。
     * @return 规范化文本。
     */
    static std::string dump(const Value &v) { return v.isTable() ? v.dump() : v.inlineDump(); }
};
/**
 * @brief 注册标准库模块。
 * @param registry 注册表。
 * @param error 错误输出。
 * @return 是否成功。
 */
inline bool bind(magel_glue::Registry &registry, Memory::String *error = nullptr)
{
    return structured_data_glue::bind<Traits>(registry, "std::toml::", error);
}
}  // namespace std_toml_glue
#endif
