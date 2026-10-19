# std::reflect — 反射

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::reflect;
```

## 公共约定与错误边界

要求工具链构建时同时启用 MAGEL_WITH_REFLECTION=1 与 STACKVM_WITH_REFLECTION=1，并在编译/运行时启用 --reflection。类型信息在具体类型和布局确定后生成；动态调用只支持 ReflectValue 的封闭载荷集合。Err 区分未找到、歧义、参数数量、类型、接收者和未暴露等错误。

## 参考代码

完整程序：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

```powershell
magel run doc/examples/library/reflect.mgl --reflection
```

需先用 `make reflection` 构建支持反射的工具链；当前二进制未编入支持时，上述命令会明确报缺失宏。

<details>
<summary>展开完整参考代码</summary>

```magel
import std::reflect;

/** @brief 供启用反射的工具链演示类型元数据查询。 */
@Reflect
struct ReferenceRecord { int64 id; String name; };

int64 main() {
    uint64 id = std::reflect::getTypeId<ReferenceRecord>();
    Option<std::reflect::StructInfo> info = std::reflect::getStructInfo<ReferenceRecord>();
    assert(info.isValue());
    std::reflect::StructInfo record = info.unwrap();
    assert(record.typeId == id && record.fields.len() == 2);
    println("reflect API ok");
    return 0;
}
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-371"></a>`enum ReflectError` | 反射错误是普通枚举值，不保存目标地址或拥有资源。 |
| ReflectError | `NotFound, Ambiguous, ArgumentCount, TypeMismatch, ReceiverMismatch, UnsupportedSignature, NotExposed, InvalidTarget` | 反射错误是普通枚举值，不保存目标地址或拥有资源。 变体及载荷见声明。 |
| 模块 | <a id="symbol-372"></a>`enum ReflectValue` | 封闭值集合：精确保存位宽、符号及字符串所有权。 |
| ReflectValue | `Empty, Bool(bool value), Char(char value), I8(int8 value), I16(int16 value), I32(int32 value), I64(int64 value), U8(uint8 value), U16(uint16 value), U32(uint32 value), U64(uint64 value), F32(float32 value), F64(float64 value), Text(std::lang::String value)` | 封闭值集合：精确保存位宽、符号及字符串所有权。 变体及载荷见声明。 |
| 模块 | <a id="symbol-373"></a>`struct ParameterInfo` | 反射参数的名称与类型身份。 |
| ParameterInfo | <a id="symbol-374"></a>`std::lang::String name` | 声明或选项的名称。 |
| ParameterInfo | <a id="symbol-375"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| 模块 | <a id="symbol-376"></a>`struct AnnotationParameter` | 注解参数的名称、类型及可选默认值。 |
| AnnotationParameter | <a id="symbol-377"></a>`std::lang::String name` | 声明或选项的名称。 |
| AnnotationParameter | <a id="symbol-378"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| AnnotationParameter | <a id="symbol-379"></a>`Option<std::reflect::ReflectValue> defaultValue` | 可选的注解默认实参；None 表示没有默认值。 |
| 模块 | <a id="symbol-380"></a>`struct AnnotationInfo` | 注解定义的名称、类型身份与参数声明。 |
| AnnotationInfo | <a id="symbol-381"></a>`std::lang::String name` | 声明或选项的名称。 |
| AnnotationInfo | <a id="symbol-382"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| AnnotationInfo | <a id="symbol-383"></a>`List<std::reflect::AnnotationParameter> params` | 按声明顺序保存的参数信息。 |
| 模块 | <a id="symbol-384"></a>`struct Annotation` | 注解实例的定义身份与实际参数值。 |
| Annotation | <a id="symbol-385"></a>`uint64 annotationId` | 注解定义的身份。 |
| Annotation | <a id="symbol-386"></a>`List<std::reflect::ReflectValue> args` | 注解实例的实际参数值。 |
| 模块 | <a id="symbol-387"></a>`struct FuncInfo` | 函数反射信息，包含参数、返回类型和注解。 |
| FuncInfo | <a id="symbol-388"></a>`std::lang::String name` | 声明或选项的名称。 |
| FuncInfo | <a id="symbol-389"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| FuncInfo | <a id="symbol-390"></a>`List<std::reflect::ParameterInfo> params` | 按声明顺序保存的参数信息。 |
| FuncInfo | <a id="symbol-391"></a>`uint64 returnTypeId` | 函数返回类型的身份。 |
| FuncInfo | <a id="symbol-392"></a>`List<std::reflect::Annotation> annotations` | 附加在该声明上的注解实例。 |
| 模块 | <a id="symbol-393"></a>`struct MethodInfo` | 方法反射信息，包含函数签名及 static、const、构造器等标志。 |
| MethodInfo | <a id="symbol-394"></a>`FuncInfo function` | 方法的完整函数签名与注解信息。 |
| MethodInfo | <a id="symbol-395"></a>`bool isStatic` | 方法是否为静态方法。 |
| MethodInfo | <a id="symbol-396"></a>`bool isConstructor` | 方法是否为构造器。 |
| MethodInfo | <a id="symbol-397"></a>`bool isConst` | 方法是否只读访问接收者。 |
| MethodInfo | <a id="symbol-398"></a>`bool hasDefaultBody` | 接口方法是否提供默认实现。 |
| 模块 | <a id="symbol-399"></a>`struct FieldInfo` | 实例字段反射信息，包含字节偏移、类型身份和拥有属性。 |
| FieldInfo | <a id="symbol-400"></a>`std::lang::String name` | 声明或选项的名称。 |
| FieldInfo | <a id="symbol-401"></a>`uint32 offset` | 相对起始位置的字节偏移。 |
| FieldInfo | <a id="symbol-402"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| FieldInfo | <a id="symbol-403"></a>`bool isOwned` | 成员是否负责所存资源的所有权。 |
| FieldInfo | <a id="symbol-404"></a>`List<std::reflect::Annotation> annotations` | 附加在该声明上的注解实例。 |
| 模块 | <a id="symbol-405"></a>`struct StaticMemberInfo` | 静态成员反射信息，包含类型身份与拥有属性。 |
| StaticMemberInfo | <a id="symbol-406"></a>`std::lang::String name` | 声明或选项的名称。 |
| StaticMemberInfo | <a id="symbol-407"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| StaticMemberInfo | <a id="symbol-408"></a>`bool isOwned` | 成员是否负责所存资源的所有权。 |
| StaticMemberInfo | <a id="symbol-409"></a>`List<std::reflect::Annotation> annotations` | 附加在该声明上的注解实例。 |
| 模块 | <a id="symbol-410"></a>`struct VariantInfo` | 枚举变体的名称、标签与载荷参数。 |
| VariantInfo | <a id="symbol-411"></a>`std::lang::String name` | 声明或选项的名称。 |
| VariantInfo | <a id="symbol-412"></a>`uint32 tag` | 枚举变体的数值标签。 |
| VariantInfo | <a id="symbol-413"></a>`List<std::reflect::ParameterInfo> payloads` | 枚举变体的载荷参数声明。 |
| 模块 | <a id="symbol-414"></a>`struct StructInfo` | 结构体反射信息，包含字段、方法、静态成员、实现接口及注解。 |
| StructInfo | <a id="symbol-415"></a>`std::lang::String name` | 声明或选项的名称。 |
| StructInfo | <a id="symbol-416"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| StructInfo | <a id="symbol-417"></a>`List<std::reflect::FieldInfo> fields` | 结构体的实例字段信息。 |
| StructInfo | <a id="symbol-418"></a>`List<std::reflect::MethodInfo> methods` | 类型或接口的方法信息。 |
| StructInfo | <a id="symbol-419"></a>`List<std::reflect::StaticMemberInfo> staticMembers` | 类型的静态成员信息。 |
| StructInfo | <a id="symbol-420"></a>`List<uint64> implementedInterfaces` | 该类型实现的接口身份。 |
| StructInfo | <a id="symbol-421"></a>`List<std::reflect::Annotation> annotations` | 附加在该声明上的注解实例。 |
| 模块 | <a id="symbol-422"></a>`struct EnumInfo` | 枚举反射信息，包含变体、方法、静态成员、实现接口及注解。 |
| EnumInfo | <a id="symbol-423"></a>`std::lang::String name` | 声明或选项的名称。 |
| EnumInfo | <a id="symbol-424"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| EnumInfo | <a id="symbol-425"></a>`List<std::reflect::VariantInfo> variants` | 枚举的变体信息。 |
| EnumInfo | <a id="symbol-426"></a>`List<std::reflect::MethodInfo> methods` | 类型或接口的方法信息。 |
| EnumInfo | <a id="symbol-427"></a>`List<std::reflect::StaticMemberInfo> staticMembers` | 类型的静态成员信息。 |
| EnumInfo | <a id="symbol-428"></a>`List<uint64> implementedInterfaces` | 该类型实现的接口身份。 |
| EnumInfo | <a id="symbol-429"></a>`List<std::reflect::Annotation> annotations` | 附加在该声明上的注解实例。 |
| 模块 | <a id="symbol-430"></a>`struct InterfaceInfo` | 接口反射信息，包含方法、实现者类型及注解。 |
| InterfaceInfo | <a id="symbol-431"></a>`std::lang::String name` | 声明或选项的名称。 |
| InterfaceInfo | <a id="symbol-432"></a>`uint64 typeId` | 编译器登记的类型身份，不是对象地址。 |
| InterfaceInfo | <a id="symbol-433"></a>`List<std::reflect::MethodInfo> methods` | 类型或接口的方法信息。 |
| InterfaceInfo | <a id="symbol-434"></a>`List<uint64> implementedBy` | 实现该接口的具体类型身份。 |
| InterfaceInfo | <a id="symbol-435"></a>`List<std::reflect::Annotation> annotations` | 附加在该声明上的注解实例。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [ReflectValue::typeId](#api-0) | `uint64 typeId()` |
| [ReflectValue::asBool](#api-1) | `Result<bool,std::reflect::ReflectError> asBool()` |
| [ReflectValue::asChar](#api-2) | `Result<char,std::reflect::ReflectError> asChar()` |
| [ReflectValue::asI8](#api-3) | `Result<int8,std::reflect::ReflectError> asI8()` |
| [ReflectValue::asI16](#api-4) | `Result<int16,std::reflect::ReflectError> asI16()` |
| [ReflectValue::asI32](#api-5) | `Result<int32,std::reflect::ReflectError> asI32()` |
| [ReflectValue::asI64](#api-6) | `Result<int64,std::reflect::ReflectError> asI64()` |
| [ReflectValue::asU8](#api-7) | `Result<uint8,std::reflect::ReflectError> asU8()` |
| [ReflectValue::asU16](#api-8) | `Result<uint16,std::reflect::ReflectError> asU16()` |
| [ReflectValue::asU32](#api-9) | `Result<uint32,std::reflect::ReflectError> asU32()` |
| [ReflectValue::asU64](#api-10) | `Result<uint64,std::reflect::ReflectError> asU64()` |
| [ReflectValue::asF32](#api-11) | `Result<float32,std::reflect::ReflectError> asF32()` |
| [ReflectValue::asF64](#api-12) | `Result<float64,std::reflect::ReflectError> asF64()` |
| [ReflectValue::asText](#api-13) | `Result<std::lang::String,std::reflect::ReflectError> asText()` |
| [getFuncInfo](#api-14) | `Option<std::reflect::FuncInfo> getFuncInfo(std::lang::String name)` |
| [getAnnotationInfo](#api-15) | `Option<std::reflect::AnnotationInfo> getAnnotationInfo(std::lang::String name)` |
| [getTypeId](#api-16) | `uint64 getTypeId<T>()` |
| [getInterfaceId](#api-17) | `uint64 getInterfaceId<I>()` |
| [getStructInfo](#api-18) | `Option<std::reflect::StructInfo> getStructInfo<T>()` |
| [getEnumInfo](#api-19) | `Option<std::reflect::EnumInfo> getEnumInfo<T>()` |
| [getInterfaceInfo](#api-20) | `Option<std::reflect::InterfaceInfo> getInterfaceInfo<I>()` |
| [implementsInterface](#api-21) | `bool implementsInterface<T>(uint64 interfaceId)` |
| [getAnnotations](#api-22) | `List<std::reflect::Annotation> getAnnotations<T>()` |
| [getFieldAnnotations](#api-23) | `List<std::reflect::Annotation> getFieldAnnotations<T>(std::lang::String name)` |
| [getFuncAnnotations](#api-24) | `List<std::reflect::Annotation> getFuncAnnotations(std::lang::String name)` |
| [getMethodAnnotations](#api-25) | `List<std::reflect::Annotation> getMethodAnnotations<T>(std::lang::String name)` |
| [getStaticMemberAnnotations](#api-26) | `List<std::reflect::Annotation> getStaticMemberAnnotations<T>(std::lang::String name)` |
| [call](#api-27) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> call(std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [callMethod](#api-28) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callMethod<T>(T mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [callMethodConst](#api-29) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callMethodConst<T>(T const& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [callStatic](#api-30) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callStatic<T>(std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [callEnumMethod](#api-31) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumMethod<T>(T mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [callEnumMethodConst](#api-32) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumMethodConst<T>(T const& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [callEnumStatic](#api-33) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumStatic<T>(std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [construct](#api-34) | `Result<T,std::reflect::ReflectError> construct<T>(List<std::reflect::ReflectValue> args)` |
| [callInterface](#api-35) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callInterface<I>(I mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |
| [callInterfaceConst](#api-36) | `Result<std::reflect::ReflectValue,std::reflect::ReflectError> callInterfaceConst<I>(I const& obj, std::lang::String name, List<std::reflect::ReflectValue> args)` |

<a id="api-0"></a>

### ReflectValue::typeId（1）

**声明**

```magel
uint64 typeId();
```

**行为**

返回 ReflectValue 当前载荷的精确类型 ID。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint64`。返回 ReflectValue 当前载荷的精确类型 ID。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.typeId();
```

**源码**：[source/std/reflect.mgl:22](../../source/std/reflect.mgl#L22)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-1"></a>

### ReflectValue::asBool（2）

**声明**

```magel
Result<bool,std::reflect::ReflectError> asBool();
```

**行为**

精确提取 Bool 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<bool,std::reflect::ReflectError>`。精确提取 Bool 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asBool();
```

**源码**：[source/std/reflect.mgl:42](../../source/std/reflect.mgl#L42)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-2"></a>

### ReflectValue::asChar（3）

**声明**

```magel
Result<char,std::reflect::ReflectError> asChar();
```

**行为**

精确提取 Char 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<char,std::reflect::ReflectError>`。精确提取 Char 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asChar();
```

**源码**：[source/std/reflect.mgl:47](../../source/std/reflect.mgl#L47)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-3"></a>

### ReflectValue::asI8（4）

**声明**

```magel
Result<int8,std::reflect::ReflectError> asI8();
```

**行为**

精确提取 I8 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<int8,std::reflect::ReflectError>`。精确提取 I8 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asI8();
```

**源码**：[source/std/reflect.mgl:52](../../source/std/reflect.mgl#L52)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-4"></a>

### ReflectValue::asI16（5）

**声明**

```magel
Result<int16,std::reflect::ReflectError> asI16();
```

**行为**

精确提取 I16 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<int16,std::reflect::ReflectError>`。精确提取 I16 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asI16();
```

**源码**：[source/std/reflect.mgl:57](../../source/std/reflect.mgl#L57)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-5"></a>

### ReflectValue::asI32（6）

**声明**

```magel
Result<int32,std::reflect::ReflectError> asI32();
```

**行为**

精确提取 I32 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<int32,std::reflect::ReflectError>`。精确提取 I32 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asI32();
```

**源码**：[source/std/reflect.mgl:62](../../source/std/reflect.mgl#L62)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-6"></a>

### ReflectValue::asI64（7）

**声明**

```magel
Result<int64,std::reflect::ReflectError> asI64();
```

**行为**

精确提取 I64 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<int64,std::reflect::ReflectError>`。精确提取 I64 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asI64();
```

**源码**：[source/std/reflect.mgl:67](../../source/std/reflect.mgl#L67)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-7"></a>

### ReflectValue::asU8（8）

**声明**

```magel
Result<uint8,std::reflect::ReflectError> asU8();
```

**行为**

精确提取 U8 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<uint8,std::reflect::ReflectError>`。精确提取 U8 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asU8();
```

**源码**：[source/std/reflect.mgl:72](../../source/std/reflect.mgl#L72)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-8"></a>

### ReflectValue::asU16（9）

**声明**

```magel
Result<uint16,std::reflect::ReflectError> asU16();
```

**行为**

精确提取 U16 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<uint16,std::reflect::ReflectError>`。精确提取 U16 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asU16();
```

**源码**：[source/std/reflect.mgl:77](../../source/std/reflect.mgl#L77)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-9"></a>

### ReflectValue::asU32（10）

**声明**

```magel
Result<uint32,std::reflect::ReflectError> asU32();
```

**行为**

精确提取 U32 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<uint32,std::reflect::ReflectError>`。精确提取 U32 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asU32();
```

**源码**：[source/std/reflect.mgl:82](../../source/std/reflect.mgl#L82)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-10"></a>

### ReflectValue::asU64（11）

**声明**

```magel
Result<uint64,std::reflect::ReflectError> asU64();
```

**行为**

精确提取 U64 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<uint64,std::reflect::ReflectError>`。精确提取 U64 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asU64();
```

**源码**：[source/std/reflect.mgl:87](../../source/std/reflect.mgl#L87)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-11"></a>

### ReflectValue::asF32（12）

**声明**

```magel
Result<float32,std::reflect::ReflectError> asF32();
```

**行为**

精确提取 F32 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<float32,std::reflect::ReflectError>`。精确提取 F32 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asF32();
```

**源码**：[source/std/reflect.mgl:92](../../source/std/reflect.mgl#L92)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-12"></a>

### ReflectValue::asF64（13）

**声明**

```magel
Result<float64,std::reflect::ReflectError> asF64();
```

**行为**

精确提取 F64 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<float64,std::reflect::ReflectError>`。精确提取 F64 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asF64();
```

**源码**：[source/std/reflect.mgl:97](../../source/std/reflect.mgl#L97)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-13"></a>

### ReflectValue::asText（14）

**声明**

```magel
Result<std::lang::String,std::reflect::ReflectError> asText();
```

**行为**

精确提取 Text 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<std::lang::String,std::reflect::ReflectError>`。精确提取 Text 载荷；变体不匹配返回 Err(TypeMismatch)，不进行隐式数值转换。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asText();
```

**源码**：[source/std/reflect.mgl:102](../../source/std/reflect.mgl#L102)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-14"></a>

### getFuncInfo（15）

**声明**

```magel
Option<std::reflect::FuncInfo> getFuncInfo(std::lang::String name);
```

**行为**

按名称查找函数元数据，未找到时 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |

**返回值**

`Option<std::reflect::FuncInfo>`。按名称查找函数元数据，未找到时 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getFuncInfo(name);
```

**源码**：[source/std/reflect.mgl:127](../../source/std/reflect.mgl#L127)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-15"></a>

### getAnnotationInfo（16）

**声明**

```magel
Option<std::reflect::AnnotationInfo> getAnnotationInfo(std::lang::String name);
```

**行为**

按名称查找注解定义元数据。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |

**返回值**

`Option<std::reflect::AnnotationInfo>`。按名称查找注解定义元数据。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getAnnotationInfo(name);
```

**源码**：[source/std/reflect.mgl:128](../../source/std/reflect.mgl#L128)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-16"></a>

### getTypeId（17）

**声明**

```magel
uint64 getTypeId<T>();
```

**行为**

取得具体 T 的类型 ID。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint64`。取得具体 T 的类型 ID。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getTypeId<T>();
```

**源码**：[source/std/reflect.mgl:130](../../source/std/reflect.mgl#L130)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-17"></a>

### getInterfaceId（18）

**声明**

```magel
uint64 getInterfaceId<I>();
```

**行为**

取得接口 I 的类型 ID。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint64`。取得接口 I 的类型 ID。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getInterfaceId<I>();
```

**源码**：[source/std/reflect.mgl:131](../../source/std/reflect.mgl#L131)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-18"></a>

### getStructInfo（19）

**声明**

```magel
Option<std::reflect::StructInfo> getStructInfo<T>();
```

**行为**

读取具体结构体的布局和成员元数据。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<std::reflect::StructInfo>`。读取具体结构体的布局和成员元数据。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getStructInfo<T>();
```

**源码**：[source/std/reflect.mgl:132](../../source/std/reflect.mgl#L132)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-19"></a>

### getEnumInfo（20）

**声明**

```magel
Option<std::reflect::EnumInfo> getEnumInfo<T>();
```

**行为**

读取枚举变体及成员元数据。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<std::reflect::EnumInfo>`。读取枚举变体及成员元数据。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getEnumInfo<T>();
```

**源码**：[source/std/reflect.mgl:133](../../source/std/reflect.mgl#L133)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-20"></a>

### getInterfaceInfo（21）

**声明**

```magel
Option<std::reflect::InterfaceInfo> getInterfaceInfo<I>();
```

**行为**

读取接口方法及实现关系元数据。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<std::reflect::InterfaceInfo>`。读取接口方法及实现关系元数据。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getInterfaceInfo<I>();
```

**源码**：[source/std/reflect.mgl:134](../../source/std/reflect.mgl#L134)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-21"></a>

### implementsInterface（22）

**声明**

```magel
bool implementsInterface<T>(uint64 interfaceId);
```

**行为**

判断 T 是否实现给定接口 ID。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `interfaceId` | `uint64 interfaceId` | 接口类型 ID | 无 |

**返回值**

`bool`。判断 T 是否实现给定接口 ID。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::implementsInterface<T>(interfaceId);
```

**源码**：[source/std/reflect.mgl:135](../../source/std/reflect.mgl#L135)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-22"></a>

### getAnnotations（23）

**声明**

```magel
List<std::reflect::Annotation> getAnnotations<T>();
```

**行为**

取得类型上的注解列表。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<std::reflect::Annotation>`。取得类型上的注解列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getAnnotations<T>();
```

**源码**：[source/std/reflect.mgl:142](../../source/std/reflect.mgl#L142)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-23"></a>

### getFieldAnnotations（24）

**声明**

```magel
List<std::reflect::Annotation> getFieldAnnotations<T>(std::lang::String name);
```

**行为**

按字段名取得字段注解。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |

**返回值**

`List<std::reflect::Annotation>`。按字段名取得字段注解。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getFieldAnnotations<T>(name);
```

**源码**：[source/std/reflect.mgl:148](../../source/std/reflect.mgl#L148)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-24"></a>

### getFuncAnnotations（25）

**声明**

```magel
List<std::reflect::Annotation> getFuncAnnotations(std::lang::String name);
```

**行为**

按函数名取得函数注解。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |

**返回值**

`List<std::reflect::Annotation>`。按函数名取得函数注解。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getFuncAnnotations(name);
```

**源码**：[source/std/reflect.mgl:153](../../source/std/reflect.mgl#L153)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-25"></a>

### getMethodAnnotations（26）

**声明**

```magel
List<std::reflect::Annotation> getMethodAnnotations<T>(std::lang::String name);
```

**行为**

按方法名取得方法注解。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |

**返回值**

`List<std::reflect::Annotation>`。按方法名取得方法注解。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getMethodAnnotations<T>(name);
```

**源码**：[source/std/reflect.mgl:163](../../source/std/reflect.mgl#L163)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-26"></a>

### getStaticMemberAnnotations（27）

**声明**

```magel
List<std::reflect::Annotation> getStaticMemberAnnotations<T>(std::lang::String name);
```

**行为**

按静态成员名取得注解。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |

**返回值**

`List<std::reflect::Annotation>`。按静态成员名取得注解。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::getStaticMemberAnnotations<T>(name);
```

**源码**：[source/std/reflect.mgl:167](../../source/std/reflect.mgl#L167)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-27"></a>

### call（28）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> call(std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

按名称动态调用函数，返回封闭值或 ReflectError。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。按名称动态调用函数，返回封闭值或 ReflectError。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::call(name, args);
```

**源码**：[source/std/reflect.mgl:176](../../source/std/reflect.mgl#L176)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-28"></a>

### callMethod（29）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> callMethod<T>(T mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

通过可写接收者动态调用实例方法。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `obj` | `T mut& obj` | 实例接收者，其引用可写性须匹配方法 | 无 |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。通过可写接收者动态调用实例方法。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::callMethod<T>(obj, name, args);
```

**源码**：[source/std/reflect.mgl:177](../../source/std/reflect.mgl#L177)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-29"></a>

### callMethodConst（30）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> callMethodConst<T>(T const& obj, std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

通过只读接收者动态调用允许的实例方法。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `obj` | `T const& obj` | 实例接收者，其引用可写性须匹配方法 | 无 |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。通过只读接收者动态调用允许的实例方法。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::callMethodConst<T>(obj, name, args);
```

**源码**：[source/std/reflect.mgl:178](../../source/std/reflect.mgl#L178)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-30"></a>

### callStatic（31）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> callStatic<T>(std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

动态调用 T 的静态方法。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。动态调用 T 的静态方法。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::callStatic<T>(name, args);
```

**源码**：[source/std/reflect.mgl:179](../../source/std/reflect.mgl#L179)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-31"></a>

### callEnumMethod（32）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumMethod<T>(T mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

通过可写枚举接收者动态调用方法。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `obj` | `T mut& obj` | 实例接收者，其引用可写性须匹配方法 | 无 |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。通过可写枚举接收者动态调用方法。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::callEnumMethod<T>(obj, name, args);
```

**源码**：[source/std/reflect.mgl:180](../../source/std/reflect.mgl#L180)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-32"></a>

### callEnumMethodConst（33）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumMethodConst<T>(T const& obj, std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

通过只读枚举接收者动态调用方法。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `obj` | `T const& obj` | 实例接收者，其引用可写性须匹配方法 | 无 |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。通过只读枚举接收者动态调用方法。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::callEnumMethodConst<T>(obj, name, args);
```

**源码**：[source/std/reflect.mgl:181](../../source/std/reflect.mgl#L181)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-33"></a>

### callEnumStatic（34）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> callEnumStatic<T>(std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

动态调用枚举的静态方法。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。动态调用枚举的静态方法。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::callEnumStatic<T>(name, args);
```

**源码**：[source/std/reflect.mgl:182](../../source/std/reflect.mgl#L182)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-34"></a>

### construct（35）

**声明**

```magel
Result<T,std::reflect::ReflectError> construct<T>(List<std::reflect::ReflectValue> args);
```

**行为**

动态调用 T 的构造函数，返回 T 或错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<T,std::reflect::ReflectError>`。动态调用 T 的构造函数，返回 T 或错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::construct<T>(args);
```

**源码**：[source/std/reflect.mgl:183](../../source/std/reflect.mgl#L183)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-35"></a>

### callInterface（36）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> callInterface<I>(I mut& obj, std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

通过可写接口接收者动态调用方法。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `obj` | `I mut& obj` | 实例接收者，其引用可写性须匹配方法 | 无 |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。通过可写接口接收者动态调用方法。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::callInterface<I>(obj, name, args);
```

**源码**：[source/std/reflect.mgl:184](../../source/std/reflect.mgl#L184)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。

<a id="api-36"></a>

### callInterfaceConst（37）

**声明**

```magel
Result<std::reflect::ReflectValue,std::reflect::ReflectError> callInterfaceConst<I>(I const& obj, std::lang::String name, List<std::reflect::ReflectValue> args);
```

**行为**

通过只读接口接收者动态调用方法。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `obj` | `I const& obj` | 实例接收者，其引用可写性须匹配方法 | 无 |
| `name` | `std::lang::String name` | 名称 | 无 |
| `args` | `List<std::reflect::ReflectValue> args` | 与目标参数数量、位宽和类型匹配的封闭反射值列表 | 无 |

**返回值**

`Result<std::reflect::ReflectValue,std::reflect::ReflectError>`。通过只读接口接收者动态调用方法。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::reflect::callInterfaceConst<I>(obj, name, args);
```

**源码**：[source/std/reflect.mgl:185](../../source/std/reflect.mgl#L185)。

**完整示例**：[doc/examples/library/reflect.mgl](../../doc/examples/library/reflect.mgl)。
