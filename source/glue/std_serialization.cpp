#include "std_serialization.hpp"
#include "../chali_glue.hpp"
#include "../../source/json/json.hpp"
#include "../../source/toml/toml.hpp"
#include <stdexcept>
#include <limits>

namespace std_serialization_glue
{
/**
 * @brief 将精确 JSON 节点递归转换为 TOML，拒绝无对应值的 null。
 * @param value JSON 节点。
 * @return 拥有独立存储的 TOML 节点。
 */
toml::Value toToml(const json::Value &value)
{
    if (value.isBool()) return toml::Value(*value.asBool());
    if (value.isInt()) return toml::Value(*value.asInt());
    if (value.isDouble()) return toml::Value(*value.asDouble());
    if (value.isString()) return toml::Value(std::string(*value.asString()));
    if (const auto *array = value.asArray())
    {
        toml::Array result;
        for (const auto &child : *array) result.push_back(toToml(child));
        return toml::Value(std::move(result));
    }
    if (const auto *object = value.asObject())
    {
        toml::Object result;
        for (const auto &member : *object) result.insert(member.key, toToml(member.value));
        return toml::Value(std::move(result));
    }
    throw std::invalid_argument("TOML 不支持 null");
}
/**
 * @brief 将 TOML 标量和复合节点递归转成 JSON；日期保留文本表示。
 * @param value TOML 节点。
 * @return 拥有独立存储的 JSON 节点。
 */
json::Value toJson(const toml::Value &value)
{
    if (value.isBool()) return json::Value(*value.asBool());
    if (value.isInt()) return json::Value(*value.asInt());
    if (value.isDouble()) return json::Value(*value.asDouble());
    if (value.isString()) return json::Value(std::string(*value.asString()));
    if (const auto *array = value.asArray())
    {
        json::Array result;
        for (const auto &child : *array) result.push_back(toJson(child));
        return json::Value(std::move(result));
    }
    if (const auto *object = value.asTable())
    {
        json::Object result;
        for (const auto &member : *object) result.insert(member.key, toJson(member.value));
        return json::Value(std::move(result));
    }
    throw std::invalid_argument("反射模型不接受 TOML 日期节点，请使用字符串字段");
}
/**
 * @brief 在 JSON 和 TOML 文本之间转换并保留失败状态。
 * @param input 输入文本。
 * @param tomlOutput 为 true 时输出 TOML，否则输出 JSON。
 * @return 首字节为成功标记 1 或失败标记 0，后续为内容或诊断。
 */
Memory::String transcode(Memory::String input, bool tomlOutput)
{
    try
    {
        std::string output;
        if (tomlOutput)
        {
            auto parsed = json::parse(input.c_str());
            if (!parsed.ok() || !parsed.value->isObject()) throw std::invalid_argument("TOML 序列化要求对象根节点");
            output = toToml(*parsed.value).dump();
        }
        else
        {
            auto parsed = toml::parse(input.c_str());
            if (!parsed.ok()) throw std::invalid_argument("TOML 文本格式无效");
            output = toJson(*parsed.value).dump();
        }
        return Memory::String(("1" + output).c_str());
    } catch (const std::exception &failure) { return Memory::String((std::string("0") + failure.what()).c_str()); }
}
/**
 * @brief 将类型标识与 UTF-8 数据封装为固定小端的版本化字节流。
 * @param text JSON 载荷。
 * @param type 反射类型稳定标识。
 * @return 包含魔数、版本、类型、长度及载荷的独立字节列表。
 */
Memory::Vector<std::uint8_t> pack(Memory::String text, std::uint64_t type)
{
    Memory::Vector<std::uint8_t> bytes;
    for (auto value : { 'M', 'G', 'S', char(1) }) bytes.push_back(static_cast<std::uint8_t>(value));
    for (unsigned shift = 0; shift < 64; shift += 8) bytes.push_back(static_cast<std::uint8_t>(type >> shift));
    for (unsigned shift = 0; shift < 64; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint64_t>(text.size()) >> shift));
    for (std::size_t index = 0; index < text.size(); ++index) bytes.push_back(static_cast<std::uint8_t>(text[index]));
    return bytes;
}
/**
 * @brief 校验字节流版本、类型和完整长度后提取 JSON。
 * @param bytes 完整序列化字节流。
 * @param type 预期反射类型标识。
 * @return 首字节为 1 的 JSON 或首字节为 0 的诊断。
 */
Memory::String unpack(Memory::Vector<std::uint8_t> bytes, std::uint64_t type)
{
    if (bytes.size() < 20 || bytes[0] != 'M' || bytes[1] != 'G' || bytes[2] != 'S' || bytes[3] != 1) return "0无效序列化魔数或版本";
    std::uint64_t actual = 0, length = 0;
    for (unsigned index = 0; index < 8; ++index)
    {
        actual |= std::uint64_t(bytes[4 + index]) << (8 * index);
        length |= std::uint64_t(bytes[12 + index]) << (8 * index);
    }
    if (actual != type) return "0序列化类型不匹配";
    if (length != bytes.size() - 20) return "0序列化长度不匹配";
    std::string text("1");
    for (std::size_t index = 20; index < bytes.size(); ++index) text += static_cast<char>(bytes[index]);
    if (text.find('\0') != std::string::npos) return "0序列化载荷包含零字节";
    return Memory::String(text.c_str());
}
/**
 * @brief 注册格式转换及二进制封装函数。
 * @param registry 宿主注册表。
 * @param error 注册失败时的诊断。
 * @return 注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    using Memory::String;
    using Bytes = Memory::Vector<std::uint8_t>;
    return registry.bindTyped<String, String, bool>("std::serialization::__transcode", transcode, error)
           && registry.bindTyped<Bytes, String, std::uint64_t>("std::serialization::__pack", pack, error)
           && registry.bindTyped<String, Bytes, std::uint64_t>("std::serialization::__unpack", unpack, error);
}
}  // namespace std_serialization_glue
