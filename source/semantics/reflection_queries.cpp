/**
 * @file reflection_queries.cpp
 * @brief 将只读元数据生成为普通 CHALI 值函数，复用 VM 执行与确定性清理。
 */
/**
 * @brief 判断本次输入是否加载 std::reflect 模块。
 * @return 加载时为真。
 */
bool HasReflectionQueries()
{
    for (int id : g_top)
        if (N(id).module == "std::reflect") return true;
    return false;
}
/**
 * @brief 查询函数源码生成器；静态文本不保存执行期地址。
 */
struct MReflectionQueries
{
    MReflectionBuilder &metadata;
    Memory::String helpers, functions;
    Memory::Set<Memory::String> replaced;
    unsigned serial = 0;
    /**
     * @brief 转义 CHALI 字符串，不改变原始字节。
     * @param value 字节文本。
     * @return 源码字面量。
     */
    static Memory::String quote(const Memory::String &value)
    {
        Memory::String out = "\"";
        const char *hex = "0123456789abcdef";
        for (unsigned char c : value)
        {
            if (c == '"' || c == '\\')
            {
                out += '\\';
                out += static_cast<char>(c);
            }
            else if (c < 32 || c == 127)
            {
                out += "\\x";
                out += hex[c >> 4];
                out += hex[c & 15];
            }
            else
                out += static_cast<char>(c);
        }
        return out + "\"";
    }
    /**
     * @brief 生成无符号精确位模式。
     * @param value 编号。
     * @return 类型明确的字面量。
     */
    static Memory::String number(std::uint64_t value)
    {
        Memory::OutputStringStream out;
        out << "(uint64)0x" << std::hex << value;
        return out.str();
    }
    /**
     * @brief 生成布尔字面量。
     * @param value 值。
     * @return 源码。
     */
    static Memory::String boolean(bool value) { return value ? "true" : "false"; }
    /**
     * @brief 生成拥有型记录工厂。
     * @param type CHALI 类型。
     * @param fields 字段表达式。
     * @return 工厂调用。
     */
    Memory::String record(const Memory::String &type,
                          std::initializer_list<std::pair<Memory::String, Memory::String>> fields)
    {
        auto name = "__reflect_record" + Memory::toString(serial++);
        const auto qualified = "std::reflect::" + type;
        helpers += qualified + " " + name + "(){" + qualified + " r;";
        for (const auto &field : fields) helpers += "r." + field.first + "=" + field.second + ";";
        helpers += "return r;}\n";
        return name + "()";
    }
    /**
     * @brief 生成独立拥有型列表。
     * @param type 元素类型。
     * @param values 元素表达式。
     * @return 工厂调用。
     */
    Memory::String list(const Memory::String &type, const Memory::Vector<Memory::String> &values)
    {
        auto name = "__reflect_record" + Memory::toString(serial++);
        const auto element = type == "uint64" ? type : "std::reflect::" + type;
        helpers += "List<" + element + "> " + name + "(){List<" + element + "> r;";
        for (const auto &value : values) helpers += "r.push(" + value + ");";
        helpers += "return r;}\n";
        return name + "()";
    }
    /**
     * @brief 转换编号列表。
     * @param values 编号。
     * @return CHALI 列表表达式。
     */
    Memory::String ids(const Memory::Vector<std::uint64_t> &values)
    {
        Memory::Vector<Memory::String> items;
        for (auto value : values) items.push_back(number(value));
        return list("uint64", items);
    }
    /**
     * @brief 递归生成记录列表。
     * @param type CHALI 元素类型。
     * @param values 元数据列表。
     * @return CHALI 表达式。
     */
    template <class T>
    Memory::String records(const char *type, const Memory::Vector<T> &values)
    {
        Memory::Vector<Memory::String> items;
        for (const auto &value : values) items.push_back(emit(value));
        return list(type, items);
    }
    /**
     * @brief 将常量转换为带精确类型的封闭值。
     * @param value 常量。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::Constant &value)
    {
        Memory::String type;
        for (const auto &id : metadata.table.identities)
            if (id.id == value.typeId)
            {
                // 类型身份域可为 type、host 或 struct，不能假设前缀长度一致。
                const auto separator = id.key.find(':');
                type = separator == Memory::String::npos ? id.key : id.key.substr(separator + 1u);
                break;
            }
        if (type == "std::lang::String") return "ReflectValue::Text(" + quote(value.text) + ")";
        if (type == "bool") return "ReflectValue::Bool(" + boolean(value.bits != 0) + ")";
        if (type == "char") return "ReflectValue::Char((char)" + number(value.bits) + ")";
        if (type == "float32" || type == "float64")
        {
            double floating;
            if (type == "float32")
            {
                auto bits = static_cast<std::uint32_t>(value.bits);
                float f;
                std::memcpy(&f, &bits, 4);
                floating = f;
            }
            else
                std::memcpy(&floating, &value.bits, 8);
            char decimal[96];
            std::snprintf(decimal, sizeof(decimal), "%.17e", floating);
            return "ReflectValue::" + Memory::String(type == "float32" ? "F32" : "F64") + "((" + type + ")" + Memory::String(decimal) + ")";
        }
        return "ReflectValue::" + Memory::String(type[0] == 'u' ? "U" : "I") + type.substr(type[0] == 'u' ? 4 : 3) + "((" + type + ")" + number(value.bits) + ")";
    }
    /**
     * @brief 生成具名参数记录。
     * @param v 参数。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::Parameter &v) { return record("ParameterInfo", { { "name", quote(v.name) }, { "typeId", number(v.typeId) } }); }
    /**
     * @brief 生成注解参数及默认值。
     * @param v 参数。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::AnnotationParameter &v) { return record("AnnotationParameter", { { "name", quote(v.name) }, { "typeId", number(v.typeId) }, { "defaultValue", v.defaultValue ? "Option<std::reflect::ReflectValue>::Value(" + emit(*v.defaultValue) + ")" : "Option<std::reflect::ReflectValue>::None" } }); }
    /**
     * @brief 生成注解定义。
     * @param v 定义。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::AnnotationInfo &v) { return record("AnnotationInfo", { { "name", quote(v.name) }, { "typeId", number(v.typeId) }, { "params", records("AnnotationParameter", v.params) } }); }
    /**
     * @brief 生成注解实例。
     * @param v 实例。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::Annotation &v) { return record("Annotation", { { "annotationId", number(v.annotationId) }, { "args", records("ReflectValue", v.args) } }); }
    /**
     * @brief 生成函数签名。
     * @param v 签名。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::FuncInfo &v) { return record("FuncInfo", { { "name", quote(v.name) }, { "typeId", number(v.typeId) }, { "params", records("ParameterInfo", v.params) }, { "returnTypeId", number(v.returnTypeId) }, { "annotations", records("Annotation", v.annotations) } }); }
    /**
     * @brief 生成方法签名及接收者契约。
     * @param v 方法。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::MethodInfo &v) { return record("MethodInfo", { { "function", emit(v.function) }, { "isStatic", boolean(v.isStatic) }, { "isConstructor", boolean(v.isConstructor) }, { "isConst", boolean(v.isConst) }, { "hasDefaultBody", boolean(v.hasDefaultBody) } }); }
    /**
     * @brief 生成字段记录。
     * @param v 字段。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::FieldInfo &v) { return record("FieldInfo", { { "name", quote(v.name) }, { "typeId", number(v.typeId) }, { "offset", "(uint32)" + number(v.offset) }, { "isOwned", boolean(v.isOwned) }, { "annotations", records("Annotation", v.annotations) } }); }
    /**
     * @brief 生成静态成员记录。
     * @param v 成员。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::StaticMemberInfo &v) { return record("StaticMemberInfo", { { "name", quote(v.name) }, { "typeId", number(v.typeId) }, { "isOwned", boolean(v.isOwned) }, { "annotations", records("Annotation", v.annotations) } }); }
    /**
     * @brief 生成枚举变体。
     * @param v 变体。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::VariantInfo &v) { return record("VariantInfo", { { "name", quote(v.name) }, { "tag", "(uint32)" + number(v.tag) }, { "payloads", records("ParameterInfo", v.payloads) } }); }
    /**
     * @brief 生成结构体完整记录。
     * @param v 类型。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::StructInfo &v) { return record("StructInfo", { { "name", quote(v.name) }, { "typeId", number(v.typeId) }, { "fields", records("FieldInfo", v.fields) }, { "methods", records("MethodInfo", v.methods) }, { "staticMembers", records("StaticMemberInfo", v.staticMembers) }, { "implementedInterfaces", ids(v.implementedInterfaces) }, { "annotations", records("Annotation", v.annotations) } }); }
    /**
     * @brief 生成枚举完整记录。
     * @param v 类型。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::EnumInfo &v) { return record("EnumInfo", { { "name", quote(v.name) }, { "typeId", number(v.typeId) }, { "variants", records("VariantInfo", v.variants) }, { "methods", records("MethodInfo", v.methods) }, { "staticMembers", records("StaticMemberInfo", v.staticMembers) }, { "implementedInterfaces", ids(v.implementedInterfaces) }, { "annotations", records("Annotation", v.annotations) } }); }
    /**
     * @brief 生成接口完整记录。
     * @param v 类型。
     * @return CHALI 表达式。
     */
    Memory::String emit(const mr::InterfaceInfo &v) { return record("InterfaceInfo", { { "name", quote(v.name) }, { "typeId", number(v.typeId) }, { "methods", records("MethodInfo", v.methods) }, { "implementedBy", ids(v.implementedBy) }, { "annotations", records("Annotation", v.annotations) } }); }
    /**
     * @brief 查询唯一记录，重名重载返回 None。
     * @param name 函数名。
     * @param type CHALI 结果类型。
     * @param values 元数据。
     * @param byName 按名称查询。
     */
    template <class T>
    void query(const char *name, const char *type, const Memory::Vector<T> &values, bool byName = false)
    {
        replaced.insert(name);
        Memory::String result = "Option<std::reflect::" + Memory::String(type) + ">";
        functions += result + " " + name + (byName ? "(std::lang::String name){" : "(uint64 id){");
        Memory::Map<Memory::String, int> counts;
        for (const auto &v : values) ++counts[v.name];
        for (const auto &v : values)
            if (!byName || counts[v.name] == 1)
                functions += "if(" + Memory::String(byName ? "name==" : "id==") + (byName ? quote(v.name) : number(v.typeId)) + "){return " + result + "::Value(" + emit(v) + ");}";
        functions += "return " + result + "::None;}\n";
    }
    /**
     * @brief 生成所有查询函数；仅开启特性且加载 std::reflect 时调用。
     * @return CHALI 源码。
     */
    Memory::String build()
    {
        replaced.insert("__reflect_typeId");
        functions = "uint64 __reflect_typeId(int64 id){";
        Memory::Set<std::int64_t> seen;
        auto types = g_types;
        for (const auto &entry : g_structs) types.push_back(NamedType(entry.first));
        for (const auto &entry : g_interfaces) types.push_back(NamedType(entry.first));
        for (const auto &type : types)
        {
            if (type->unresolvedName) continue;
            auto internal = TypeIdOf(type);
            if (!seen.insert(internal).second) continue;
            functions += "if(id==" + Memory::toString(internal) + "){return " + number(metadata.typeId(type)) + ";}";
        }
        functions += "return (uint64)0;}\n";
        query("__reflect_struct", "StructInfo", metadata.table.structs);
        query("__reflect_enum", "EnumInfo", metadata.table.enums);
        query("__reflect_interface", "InterfaceInfo", metadata.table.interfaces);
        query("getFuncInfo", "FuncInfo", metadata.table.functions, true);
        query("getAnnotationInfo", "AnnotationInfo", metadata.table.annotations, true);
        return "module std::reflect;\n" + helpers + functions;
    }
};
