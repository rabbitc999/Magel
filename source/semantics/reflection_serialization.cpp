/**
 * @file reflection_serialization.cpp
 * @brief 通过已校验的反射类型生成拥有型 JSON 编解码桥接。
 */
/**
 * @brief 将具体反射类型展开为普通语言编解码函数，复用类型检查与拥有关系。
 */
struct MReflectionSerialization
{
    MReflectionQueries &queries;
    MReflectionBuilder &metadata;
    Memory::Map<Memory::String, Memory::String> names;
    Memory::String definitions;
    unsigned serial = 0;
    /**
     * @brief 复用本次反射查询输出和元数据，统一生成入口及递归辅助函数。
     * @param queryOutput 当前编译单元的桥接输出。
     * @param typeMetadata 已完成布局校验的具体类型元数据。
     */
    MReflectionSerialization(MReflectionQueries &queryOutput, MReflectionBuilder &typeMetadata) : queries(queryOutput), metadata(typeMetadata)
    {
    }
    /**
     * @brief 生成编码失败的语句。
     * @param text 具体错误说明。
     * @return 设置诊断并返回空文本的源码。
     */
    Memory::String encodeError(const Memory::String &text)
    {
        return "error=" + queries.quote(text) + ";return \"\";";
    }
    /**
     * @brief 生成解码失败的语句。
     * @param text 具体错误说明。
     * @return 设置诊断并返回 false 的源码。
     */
    Memory::String decodeError(const Memory::String &text)
    {
        return "error=" + queries.quote(text) + ";return false;";
    }
    /**
     * @brief 检查结构体或枚举是否显式暴露给反射。
     * @param type 具体类型。
     * @return 显式暴露的用户类型或语言内建枚举时返回 true。
     */
    bool exposed(const std::shared_ptr<MType> &type)
    {
        const auto separator = type->name.rfind("::");
        const auto name = separator == Memory::String::npos ? type->name : type->name.substr(separator + 2);
        if (name.rfind("Option__", 0) == 0 || name.rfind("Result__", 0) == 0 || IsTupleType(type)) return true;
        const auto id = metadata.typeId(type);
        for (const auto &entry : metadata.table.structs)
            if (entry.typeId == id) return true;
        for (const auto &entry : metadata.table.enums)
            if (entry.typeId == id) return true;
        return false;
    }
    /**
     * @brief 为一个具体类型生成成对的递归编解码函数，复用已登记类型以支持递归容器。
     * @param type 完整具体类型。
     * @return 辅助函数的公共名称前缀。
     */
    Memory::String ensure(const std::shared_ptr<MType> &type)
    {
        const auto key = TypeKey(type);
        const auto old = names.find(key);
        if (old != names.end()) return old->second;
        const auto name = "__reflect_serde" + Memory::toString(serial++);
        names[key] = name;
        const auto text = InferenceTypeText(type);
        const auto encodeSignature = "String " + name + "Encode(" + text + " const& value,String mut& error,int64 depth)";
        const auto decodeSignature = "bool " + name + "Decode(std::json::JsonValue const& node," + text + " mut& value,String mut& error,int64 depth)";
        Memory::String encode, decode;
        if (IsStandardOpcode(type, "String"))
        {
            encode = "return __serdeQuote(value);";
            decode = "if(!node.isString() || node.asString().isNone()){" + decodeError("需要可表示的字符串") + "}value=node.asString().unwrap();return true;";
        }
        else if (type->kind == MType::Bool)
        {
            encode = "if(value){return \"true\";}return \"false\";";
            decode = "if(!node.isBool()){" + decodeError("需要布尔值") + "}value=node.asBool().unwrap();return true;";
        }
        else if (type->kind == MType::Int || type->kind == MType::Char)
        {
            encode = "return value.toString();";
            const bool unsignedValue = type->kind == MType::Char || type->isUnsigned;
            const int bits = type->kind == MType::Char ? 8 : type->bits;
            if (type->kind == MType::Char) encode = "return ((int64)value).toString();";
            if (unsignedValue && bits == 64)
            {
                encode = "if(value>(uint64)9223372036854775807){return __serdeQuote(value.toString());}" + encode;
                decode = "if(node.isString()){String digits=node.asString().unwrap();if(std::parse::Parse::parseUInt64(digits).isErr()){" + decodeError("无符号整数越界或格式无效") + "}value=std::parse::Parse::parseUInt64(digits).unwrap();return true;}";
            }
            decode += "if(!node.isInt()){" + decodeError("需要整数") + "}int64 number=node.asInt().unwrap();";
            if (unsignedValue)
            {
                decode += "if(number<0";
                if (bits < 64) decode += " || number>" + Memory::toString((std::uint64_t{ 1 } << bits) - 1);
                decode += "){" + decodeError("无符号整数超出目标范围") + "}";
            }
            else if (bits < 64)
            {
                const auto maximum = (std::int64_t{ 1 } << (bits - 1)) - 1;
                decode += "if(number<(0-" + Memory::toString(maximum) + "-1) || number>" + Memory::toString(maximum) + "){" + decodeError("整数超出目标范围") + "}";
            }
            decode += "value=(" + text + ")number;return true;";
        }
        else if (type->kind == MType::Double)
        {
            encode = "if(!__serdeFinite((float64)value)){" + encodeError("JSON 不支持非有限浮点数") + "}return __serdeFloat((float64)value);";
            decode = "if(!node.isInt() && !node.isFloat()){" + decodeError("需要数值") + "}float64 number=node.asFloat64().unwrap();value=(" + text + ")number;if(!__serdeFinite((float64)value)){" + decodeError("浮点数超出目标范围") + "}return true;";
        }
        else if (IsStandardOpcode(type, "BigInteger") || IsStandardOpcode(type, "BigDecimal"))
        {
            const bool decimal = IsStandardOpcode(type, "BigDecimal");
            encode = "return __serdeQuote(value.toString());";
            decode = "if(!node.isString()){" + decodeError("大数需要精确十进制字符串") + "}String digits=node.asString().unwrap();if(!__serdeNumber(digits," + Memory::String(decimal ? "true" : "false") + ")){" + decodeError("无效大数文本") + "}value=" + text + "(digits);return true;";
            if (decimal)
            {
                encode = "return \"{\\\"$decimal\\\":\"+__serdeQuote(value.toString())+\",\\\"$precision\\\":\"+__serdeBigPrecision(value).toString()+\"}\";";
                decode = "if(!node.isObject() || node.find(\"$decimal\").isNone() || node.find(\"$precision\").isNone()){" + decodeError("大数需要数值与精度对象") + "}auto digitsNode=node.find(\"$decimal\").unwrap();auto precisionNode=node.find(\"$precision\").unwrap();if(!digitsNode.isString() || !precisionNode.isInt()){" + decodeError("无效大数精度对象") + "}String digits=digitsNode.asString().unwrap();int64 precision=precisionNode.asInt().unwrap();if(!__serdeNumber(digits,true) || precision<0 || precision>100000){" + decodeError("无效大数数值或精度") + "}value=" + text + "(digits);if(precision>0){value.setPrecision(precision);}return true;";
            }
        }
        else if (IsContainer(type) && type->name != "map")
        {
            const auto child = ensure(type->elem);
            const auto item = InferenceTypeText(type->elem);
            encode = "String output=\"[\";int64 index=0;while(index<value.len()){if(index>0){output=output+\",\";}" + item + " entry=value.at(index);output=output+" + child + "Encode(&entry,&error,depth+1);if(error!=\"\"){return \"\";}index=index+1;}return output+\"]\";";
            decode = "if(!node.isArray()){" + decodeError("需要数组") + "}value.clear();int64 index=0;while(index<node.len()){auto part=node.at(index).unwrap();" + item + " entry;if(!" + child + "Decode(&part,&entry,&error,depth+1)){return false;}value." + Memory::String(type->name == "vec" ? "push" : "add") + "(std::lang::move(&entry));index=index+1;}return true;";
        }
        else if (IsContainer(type) && type->name == "map" && IsStandardOpcode(type->elem, "String"))
        {
            const auto child = ensure(type->elem2), item = InferenceTypeText(type->elem2);
            encode = "String output=\"{\";int64 index=0;while(index<value.len()){if(index>0){output=output+\",\";}String key=value.keyAt(index);" + item + " entry=value.valAt(index);output=output+__serdeQuote(key)+\":\"+" + child + "Encode(&entry,&error,depth+1);if(error!=\"\"){return \"\";}index=index+1;}return output+\"}\";";
            decode = "if(!node.isObject()){" + decodeError("需要对象") + "}value.clear();int64 index=0;while(index<node.len()){if(node.keyAt(index).isNone()){" + decodeError("对象键无法表示为 String") + "}String key=node.keyAt(index).unwrap();auto part=node.at(index).unwrap();" + item + " entry;if(!" + child + "Decode(&part,&entry,&error,depth+1)){return false;}value.put(key,std::lang::move(&entry));index=index+1;}return true;";
        }
        else if (type->kind == MType::Struct && exposed(type) && !HasNonCopyableValue(type))
        {
            const auto enumeration = g_enums.find(type->name);
            if (enumeration != g_enums.end())
            {
                encode = "match(value){";
                decode = "if(!node.isObject() || node.find(\"$variant\").isNone() || node.find(\"$values\").isNone()){" + decodeError("需要枚举变体对象") + "}auto variantNode=node.find(\"$variant\").unwrap();auto parts=node.find(\"$values\").unwrap();if(!variantNode.isString() || !parts.isArray()){" + decodeError("无效枚举变体") + "}String variant=variantNode.asString().unwrap();";
                for (const auto &variant : enumeration->second.variants)
                {
                    Memory::String pattern = variant.name, output = "String output=" + queries.quote("{\"$variant\":" + std::string(queries.quote(variant.name).c_str()) + ",\"$values\":[") + ";";
                    Memory::String construction = text + "::" + variant.name;
                    if (!variant.payloads.empty())
                    {
                        pattern += "(";
                        construction += "(";
                    }
                    decode += "if(variant==" + queries.quote(variant.name) + "){if(parts.len()!=" + Memory::toString(variant.payloads.size()) + "){" + decodeError("枚举载荷数量不匹配") + "}";
                    for (std::size_t index = 0; index < variant.payloads.size(); ++index)
                    {
                        const auto child = ensure(variant.payloads[index].type), variable = "item" + Memory::toString(index);
                        if (index)
                        {
                            pattern += ",";
                            construction += ",";
                            output += "output=output+\",\";";
                        }
                        pattern += variable;
                        construction += "std::lang::move(&" + variable + ")";
                        output += "output=output+" + child + "Encode(&" + variable + ",&error,depth+1);if(error!=\"\"){return \"\";}";
                        decode += InferenceTypeText(variant.payloads[index].type) + " " + variable + ";auto part" + Memory::toString(index) + "=parts.at(" + Memory::toString(index) + ").unwrap();if(!" + child + "Decode(&part" + Memory::toString(index) + ",&" + variable + ",&error,depth+1)){return false;}";
                    }
                    if (!variant.payloads.empty())
                    {
                        pattern += ")";
                        construction += ")";
                    }
                    encode += pattern + "=>{" + output + "return output+\"]}\";}";
                    decode += "value=" + construction + ";return true;}";
                }
                encode += "}" + encodeError("无效枚举标签");
                decode += decodeError("未知枚举变体");
            }
            else
            {
                encode = "String output=\"{\";";
                decode = "if(!node.isObject()){" + decodeError("需要结构体对象") + "}";
                bool first = true;
                const auto *definition = FindStruct(type->name);
                if (definition)
                    for (const auto &field : definition->fields)
                    {
                        const auto child = ensure(field.type), fieldName = queries.quote(field.name);
                        if (!first) encode += "output=output+\",\";";
                        first = false;
                        encode += "output=output+" + queries.quote(std::string(queries.quote(field.name).c_str()) + ":") + "+" + child + "Encode(&value." + field.name + ",&error,depth+1);if(error!=\"\"){return \"\";}";
                        decode += "{if(node.find(" + fieldName + ").isNone()){" + decodeError("缺少字段：" + field.name) + "}auto part=node.find(" + fieldName + ").unwrap();if(!" + child + "Decode(&part,&value." + field.name + ",&error,depth+1)){error=" + queries.quote(field.name + ": ") + "+error;return false;}}";
                    }
                encode += "return output+\"}\";";
                decode += "return true;";
            }
        }
        else
        {
            encode = encodeError("类型未暴露给反射或不支持序列化：" + text);
            decode = decodeError("类型未暴露给反射或不支持反序列化：" + text);
        }
        definitions += encodeSignature + "{if(depth>128){" + encodeError("序列化嵌套超过 128 层") + "}" + encode + "}\n";
        definitions += decodeSignature + "{if(depth>128){" + decodeError("反序列化嵌套超过 128 层") + "}" + decode + "}\n";
        return name;
    }
    /**
     * @brief 生成所有已使用的序列化入口及其依赖的具体类型桥接。
     */
    void build()
    {
        Memory::String entries;
        for (const auto &decl : g_functionDecls)
        {
            if (decl.module != "std::reflect" || !decl.external || decl.superseded) continue;
            for (const auto &entry : magel_generic::session.made)
            {
                const auto &expansion = entry.second;
                if (!expansion.generic || expansion.generic->module != "std::reflect" || expansion.name != decl.source) continue;
                const bool encoding = expansion.generic->name == "__serializeJson";
                if (!encoding && expansion.generic->name != "__deserializeJson") continue;
                auto type = encoding ? decl.signature.params.front().type->elem : std::shared_ptr<MType>{};
                if (!encoding)
                    for (const auto &variant : g_enums.at(decl.signature.result->name).variants)
                        if (variant.name == "Ok" && !variant.payloads.empty()) type = variant.payloads.front().type;
                if (!type)
                {
                    Fail("序列化需要完整具体类型");
                    return;
                }
                const auto helper = g_reflectionEnabled ? ensure(type) : Memory::String{}, result = InferenceTypeText(decl.signature.result);
                entries += result + " " + decl.source + "(" + InferenceTypeText(decl.signature.params.front().type) + " " + decl.signature.params.front().source + "){String error;";
                if (!g_reflectionEnabled)
                    entries += "std::lang::panic(\"编译器未启用反射支持\");return " + result + "::Err(\"编译器未启用反射支持\");}\n";
                else if (encoding)
                    entries += "String text=" + helper + "Encode(&value,&error,0);if(error!=\"\"){return " + result + "::Err(error);}return " + result + "::Ok(text);}\n";
                else
                    entries += "auto parsed=std::json::parse(text);if(parsed.isErr()){return " + result + "::Err(parsed.error());}auto node=parsed.unwrap();" + InferenceTypeText(type) + " value;if(!" + helper + "Decode(&node,&value,&error,0)){return " + result + "::Err(error);}return " + result + "::Ok(std::lang::move(&value));}\n";
                queries.replaced.insert(decl.source);
                break;
            }
        }
        queries.functions += definitions + entries;
    }
};
