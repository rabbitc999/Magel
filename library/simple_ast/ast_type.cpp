/**
 * @file ast_type.cpp
 * @brief 类型构造与四元式类型描述。
 */

#include "simple_ast.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace simple_ast
{
// ---- 类型 -----------------------------------------------------------------

/**
 * @brief 根据类型类别与位宽创建类型描述。
 * @param kind 待处理对象的类别。
 * @param bits 数值类型的位宽。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::make(Kind kind, int bits)
{
    auto type = Memory::makeShared<Type>();
    type->kind = kind;
    type->bits = bits;
    return type;
}

/**
 * @brief 构造有符号整数类型，位宽为 8。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::i8() { return make(Kind::Int, 8); }
/**
 * @brief 构造有符号整数类型，位宽为 16。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::i16() { return make(Kind::Int, 16); }
/**
 * @brief 构造有符号整数类型，位宽为 32。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::i32() { return make(Kind::Int, 32); }
/**
 * @brief 构造有符号整数类型，位宽为 64。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::i64() { return make(Kind::Int, 64); }
/**
 * @brief 构造无符号整数类型，位宽为 8。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::u8() { return make(Kind::Uint, 8); }
/**
 * @brief 构造无符号整数类型，位宽为 16。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::u16() { return make(Kind::Uint, 16); }
/**
 * @brief 构造无符号整数类型，位宽为 32。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::u32() { return make(Kind::Uint, 32); }
/**
 * @brief 构造无符号整数类型，位宽为 64。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::u64() { return make(Kind::Uint, 64); }
/**
 * @brief 构造浮点类型，位宽为 32。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::f32() { return make(Kind::Float, 32); }
/**
 * @brief 构造浮点类型，位宽为 64。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::f64() { return make(Kind::Float, 64); }
/**
 * @brief 构造布尔类型描述。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::boolean() { return make(Kind::Bool, 1); }
/**
 * @brief 构造字符串类型描述。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::stringType() { return make(Kind::String, 0); }

/**
 * @brief 根据元素类型及布局信息构造数组类型。
 * @param element 数组或容器的元素类型。
 * @param meta 类型布局的附加元数据。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::array(TypePtr element, Memory::String meta)
{
    auto type = make(Kind::Array);
    type->element = std::move(element);
    type->meta = std::move(meta);
    return type;
}

/**
 * @brief 根据目标类型构造引用类型。
 * @param target 待访问或修改的目标。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::ref(TypePtr target)
{
    auto type = make(Kind::Ref);
    type->element = std::move(target);
    type->meta = "nullable";
    return type;
}

/**
 * @brief 根据字段列表构造命名结构体类型。
 * @param name 名称文本。
 * @param fields 结构体字段定义或字段初始化值列表。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::structOf(Memory::String name, Memory::Vector<std::pair<Memory::String, TypePtr>> fields)
{
    auto type = make(Kind::Struct);
    type->name = std::move(name);
    type->fields = std::move(fields);
    type->meta = "value";
    return type;
}

/**
 * @brief 根据变体列表构造命名枚举类型。
 * @param name 名称文本。
 * @param variants 枚举变体声明列表。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::enumOf(Memory::String name, Memory::Vector<std::pair<Memory::String, TypePtr>> variants)
{
    auto type = make(Kind::Enum);
    type->name = std::move(name);
    type->variants = std::move(variants);
    type->meta = "tagged";
    return type;
}

/**
 * @brief 构造泛型类型参数的类型描述。
 * @param name 名称文本。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::generic(Memory::String name)
{
    auto type = make(Kind::Generic);
    type->name = std::move(name);
    type->meta = "generic";
    return type;
}

/**
 * @brief 取得类型对应的四元式类型名称。
 * @retval 生成或读取的结果文本。
 */
Memory::String Type::quadType() const
{
    switch (kind)
    {
    case Kind::Bool: return "i8";
    case Kind::Int: return "i" + Memory::toString(bits);
    case Kind::Uint: return "u" + Memory::toString(bits);
    case Kind::Float: return "f" + Memory::toString(bits);
    default: return "i64";  // 字符串 / 数组 / 引用 / 结构体 / 枚举 / 泛型：i64 句柄
    }
}

/**
 * @brief 生成当前类型的可读描述。
 * @retval 生成或读取的结果文本。
 */
Memory::String Type::describe() const
{
    Memory::String text;
    switch (kind)
    {
    case Kind::Bool: text = "bool"; break;
    case Kind::Int: text = "i" + Memory::toString(bits); break;
    case Kind::Uint: text = "u" + Memory::toString(bits); break;
    case Kind::Float: text = "f" + Memory::toString(bits); break;
    case Kind::String: text = "string"; break;
    case Kind::Generic: text = "generic<" + name + ">"; break;
    case Kind::Array: text = "array<" + (element ? element->describe() : "?") + ">"; break;
    case Kind::Ref: text = "ref<" + (element ? element->describe() : "?") + (meta == "nonnull" ? ">" : ">?"); break;
    case Kind::Struct:
        text = "struct " + name + " {";
        for (std::size_t index = 0; index < fields.size(); ++index)
        {
            if (index != 0u) text += ", ";
            text += fields[index].first + ": " + fields[index].second->describe();
        }
        if (!methods.empty())
        {
            text += " | methods:";
            for (const Memory::String &method : methods) text += " " + method;
        }
        if (!operators.empty())
        {
            text += " | operators:";
            for (const auto &item : operators) text += " " + item.first + "→" + item.second;
        }
        text += "}";
        break;
    case Kind::Enum:
        text = "enum " + name + " {";
        for (std::size_t index = 0; index < variants.size(); ++index)
        {
            if (index != 0u) text += ", ";
            text += variants[index].first;
            if (variants[index].second) text += "(" + variants[index].second->describe() + ")";
        }
        text += "}";
        break;
    default: text = "void"; break;
    }
    if (!meta.empty()) text += "{" + meta + "}";
    return text;
}

}  // namespace simple_ast
