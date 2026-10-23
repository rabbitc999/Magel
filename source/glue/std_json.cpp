#include "../chali_glue.hpp"
#include "std_json.hpp"
#include "../../source/json/json.hpp"
#include "std_structured_data.hpp"
#include "std_serialization.hpp"
#include <cmath>

/**
 * @brief JSON 库与 CHALI 公共文档胶水的适配。
 */
namespace std_json_glue
{
/**
 * @brief 线性校验十进制整数或小数，避免正则递归限制大数长度。
 * @param value 不含前后空白的数值文本。
 * @param decimal 是否允许小数部分与十进制指数。
 * @return 文本完整匹配 JSON 数值语法时返回 true。
 * @note 小数指数的绝对值沿用 BigDecimal 后端的一百万上限。
 */
bool numberText(Memory::String value, bool decimal)
{
    std::size_t at = 0;
    const auto digit = [&](std::size_t index) { return index < value.size() && value[index] >= '0' && value[index] <= '9'; };
    if (at < value.size() && value[at] == '-') ++at;
    if (!digit(at)) return false;
    if (value[at] == '0')
        ++at;
    else
        while (digit(at)) ++at;
    if (decimal && at < value.size() && value[at] == '.')
    {
        ++at;
        if (!digit(at)) return false;
        while (digit(at)) ++at;
    }
    if (decimal && at < value.size() && (value[at] == 'e' || value[at] == 'E'))
    {
        ++at;
        if (at < value.size() && (value[at] == '+' || value[at] == '-')) ++at;
        if (!digit(at)) return false;
        std::uint64_t exponent = 0;
        while (digit(at))
        {
            exponent = exponent * 10 + static_cast<unsigned>(value[at++] - '0');
            if (exponent > 1000000) return false;
        }
    }
    return at == value.size();
}

/**
 * @brief JSON 的解析、类型与序列化策略。
 */
struct Traits
{
    using Value = json::Value;
    static constexpr const char *recordName = "std::json::JsonValue";
    /**
     * @brief 解析文本。
     * @param text UTF-8 文本。
     * @return 根值及诊断。
     */
    static auto parse(std::string_view text) { return json::parse(text); }
    /**
     * @brief 返回 CHALI 类型编号。
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
        return 0;
    }
    /**
     * @brief 提取日期时间文本。
     * @param v 节点。
     * @return 文本或空串。
     */
    static std::string dateTime(const Value &v)
    {
        (void)v;
        return {};
    }
    /**
     * @brief 序列化节点。
     * @param v 节点。
     * @return 规范化文本。
     */
    static std::string dump(const Value &v) { return v.dump(); }
};
/**
 * @brief 注册标准库模块。
 * @param registry 注册表。
 * @param error 错误输出。
 * @return 是否成功。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    using Memory::String;
    return std_serialization_glue::bind(registry, error) && structured_data_glue::bind<Traits>(registry, "std::json::", error)
           && registry.bindTyped<bool>("std::reflect::__compiledReflection", []() {
#if CHALI_WITH_REFLECTION && STACKVM_WITH_REFLECTION
                  return true;
#else
                  return false;
#endif
              },
                                       error)
           && registry.bindTyped<String, String>("std::reflect::__serdeQuote", [](String value) { return String(json::Value(std::string(value.c_str(), value.size())).dump().c_str()); }, error) && registry.bindTyped<String, double>("std::reflect::__serdeFloat", [](double value) { return String(json::Value(value).dump().c_str()); }, error) && registry.bindTyped<bool, double>("std::reflect::__serdeFinite", [](double value) { return std::isfinite(value); }, error) && registry.bindTyped<bool, String, bool>("std::reflect::__serdeNumber", numberText, error);
}
}  // namespace std_json_glue
