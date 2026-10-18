# 注解与反射

反射针对编译期已存在、显式暴露的具体声明，提供元数据、类型化查询和受限动态调用。
编译器生成普通 Magel 桥接函数，VM 与 AOT 复用精确类型检查、资源清理和实际接口分派。
它不创建新类型、函数或变体，不提供一般字段写入、引用签名调用或任意聚合装箱。
值语义、非空引用、确定性 Drop 和借用检查继续适用；分配使用现有 Arena，不承诺零分配或硬实时。

## 注解语法


注解声明仍用规格中的名称在前、类型在后的参数形式；被注解的函数、字段和构造函数遵循 Magel 现行 C 风格语法：

```mc
@annotation Component
@annotation http::Route(path: String, level: int32 = 2)

@Component
struct Service {
    int64 value;
    @http::Route("/read", level: 3)
    int64 read() { return self.value; }
};
```

注解声明只允许在模块顶层，没有结尾分号。可以连续使用多个注解，参数可用位置或名称绑定，位置参数须在命名参数之前；重复赋值、未知名称、缺少必填项或多余实参报错。默认参数可以位于必填参数前，此时后面的必填项可用命名实参提供。紧随注解名的 `(` 总是注解参数；标注元组返回函数时用 `@Tag()` 明确结束注解。

注解可附着于 struct、enum、interface、自由函数（含宿主函数声明）、module，以及结构体/枚举的实例方法、静态方法、构造函数、字段和静态字段；成员本身必须符合现有语言规则（例如枚举不因此新增实例字段或构造函数）。局部变量、表达式、语句块、import、宿主类型声明和全局变量不可标注。模块标注通过生成解析器/`--raw` 输入可用；普通模块加载器仍要求文件头第一条有效语句为 `module`，因此普通项目入口不支持在 module 之前放注解。

名称必须先声明后使用，不允许覆盖。无显式限定的声明属于当前模块；使用先找当前模块、根模块，再找直接导入模块中的唯一候选。模块仍各用独立文件和现有 import 文件头规则。注解不能与同一命名空间的类型重名，后续类型声明也参加冲突检查。

注解参数支持 bool、char、定宽有符号/无符号整数、float32/float64、String（核心限定可省略为 String）；拒绝 void、枚举、结构体、接口、宿主别名、容器、引用及函数指针。默认值和实参均限字面量、数字字面量的一元正负号以及类型完全匹配的显式 cast。整数与浮点字面量按目标类型检查范围；`2` 不能用于浮点参数，`2.0` 不能用于整数参数，`(int64)2` 不能用于 int32 参数。沿用既有十进制词法上限，`int64` 最小值用 `-0x8000000000000000`，`uint64` 最大值用 `0xFFFFFFFFFFFFFFFF`。字符串/字符复用现有转义检查。变量、函数调用和 `1+2` 等表达式均拒绝。

前置注解的泛型模板尚不支持，会在语法检查时报错；普通泛型声明、单态化类型作为普通函数签名，以及被注解的具体声明可以共存。具体函数签名中已单态化的类型会登记规范身份；目前不能用前置 `@Reflect` 标记泛型模板，也没有独立的具体泛型类型暴露语法。

AST 保留每个声明的 `annotations` 节点列表、每次使用的声明绑定，以及按参数声明顺序补齐的 `annotationValues`。常量保存编译器拥有的值和精确类型身份，不保存运行时句柄或引用。`M_ParsedNode` 是编译器内的只读 AST 接口，不能作为 Magel 反射查询接口使用；运行时元数据从这些 AST 绑定生成，执行语义与原声明一致。

语义错误指向注解起始 `@` 的文件、行、列；预处理包含文件映射同样有效。错误位置不是参数字符的单独定位，普通语义错误仍沿用原有整行范围。

## 默认配置与产物兼容


两个构建宏独立，只接受 `0` 或 `1`；根目录工具链通过 `features.mk` 默认将两项均设为 `1`：

- `MAGEL_WITH_REFLECTION`：编译器前端能力。
- `STACKVM_WITH_REFLECTION`：VM 运行时能力。

构建根目录工具链：

```sh
make -C magel
magel/magel run source.mgl --reflection --svmb --emit --out tmp/program
```

源码预处理的 `-D` 不能替代工具链构建宏。工具链编入两项能力后，单次编译默认启用反射并给产物写入要求；使用 `--no-reflection` 显式关闭。未编入所需能力时，显式 `--reflection` 报 `MGL1102`，`--diagnostics=json` 使用既有诊断 schema。检查模式仍然不生成产物。

| 前端宏 | VM 宏 | 普通编译 | `--reflection` |
| --- | --- | --- | --- |
| 0 | 0 | 允许 | 拒绝 |
| 0 | 1 | 允许 | 拒绝 |
| 1 | 0 | 允许 | 拒绝 |
| 1 | 1 | 允许 | 允许 |

前端查询实际链接的 `supportedProgramFeatures()`，不会仅凭自身头文件中的 VM 宏判断。Make 构建配置指纹包含两个宏，切换后会重编译；单次 `--reflection` 进入增量产物缓存签名。独立生成的编译器工程也提供两个同名 CMake 选项，均默认 `OFF`。

## 产物兼容规则

`QuadFile` 与 `Program` 使用 `requiredFeatures` 保存程序要求的能力。当前定义 `0x0002` 为反射位；未知能力位一律拒绝。四元式通过 `# magel-features: reflection` 保存要求，优化保留此字段。未知的 `magel-features` 值报错，不能悄悄降级为普通程序。

普通 `.svmb` 使用既有头部 `flags` 的 `0x0002` 位，`0x0001` 不再受支持，加载器会拒绝该标志。紧凑符号格式使用其 32 位 `flags` 的 `0x0002` 位。没有开启反射的产物保持原有字段值，不添加反射数据，不升级既有格式版本。旧版本格式不可附加新的能力位。

运行时按能力包含关系校验：开启反射支持的 VM 可以运行普通程序；关闭支持的 VM 不能加载、组装或直接运行要求反射的程序。加载失败保留调用者原 `Program`，写出拒绝发生在打开目标文件之前。能力位是兼容契约，不是防篡改签名。

VM/AOT 导出根据输入程序固化内嵌运行时的默认宏，反射产物若被外部宏关闭则编译报错。VM 导出保留 `Program.requiredFeatures`；AOT 源码和模块公共声明包含静态能力检查，模块缓存签名因源码变化自然区分配置。当前写出的反射产物在调试节后追加 schema=1 元数据段；普通和紧凑容器共用编码。无反射能力位时不写段，禁止未标记的非空表。只有能力位而没有段的旧格式产物仍按空表加载；已有段必须完整，不接受未知版本或尾部数据。

## 元数据模型与传输


C++ 宿主值模型与 Magel 类型化查询使用同一元数据来源。`StructInfo`、`EnumInfo`、`InterfaceInfo`、`FuncInfo`、`MethodInfo`、`AnnotationInfo` 记录布局、签名和注解；`AnnotationInfo` 保存参数声明与可选默认值，`Annotation` 保存某次使用已按参数顺序补齐的常量值。枚举通过多个具名 `payloads` 保留 Magel 原生语义。方法属性记录实例/静态、构造、只读接收者与接口默认体存在性，不含可调用地址。

身份由带类别前缀、完整模块名称及具体签名的规范键计算 FNV-1a 64 位编号，表校验拒绝重号或不一致的键。它独立于 Any 与接口的私有运行时分派编号；应用不能读取或持久化这些内部编号。规范键由 Magel 编译器统一生成。查询按编号匹配，按名称查询遇到重载返回空；结果为 `std::optional<std::reference_wrapper<const T>>`，生命周期受表约束，调用者不能通过查询修改记录。

独立段以 `MCRF` 魔数和小端 32 位 schema=1 开始，随后按模型字段顺序编码。编号及数字位模式使用小端 64 位，长度与偏移为小端 32 位，布尔/可选标志仅接受 0/1；字符串保存原始字节，支持内嵌零。段最多 16 MiB，每个列表最多 4096 项，每个字符串最多 1 MiB，读取分配前检查剩余长度。解码后检查身份、类型引用、注解参数精确类型、字段重名和枚举标签重复。编解码失败保留原输出，加载失败保留 Program，保存拒绝保留已有文件。

四元式注释 `# magel-reflection: <十六进制段>` 可携带表，须同时声明 `# magel-features: reflection`，重复段或非法编码报错。优化保留表。VM 与 AOT 导出把编码数据固化为 C++ 静态字节数组，并提供宿主拥有型表工厂 `magel_reflection_metadata()`；每次通过同一 codec 在调用方 Memory 分配域解码，表由 Program 或宿主局部变量持有并按作用域释放，静态只保存编码字节，运行时不读元数据文件。元数据解码不构造被描述的 Magel 对象。紧凑容器里的反射名称仍明确保留，符号压缩不等于隐藏显式暴露的名称。

## 显式暴露声明


内建零参数标记 `@Reflect`（也可写 `@std::reflect::Reflect`）仅暴露有实现的自由函数，以及具体结构体、枚举和接口。成员随类型暴露，不支持单独标记字段或方法；未标记类型只在已暴露签名或接口关系需要时登记身份，不生成其完整记录。用户已声明的同名注解优先，保留已有代码含义；需要明确内建标记时用限定名称。关闭开关时仍在注解前端报错，完全不生成反射段。

```mc
@annotation Route(path:String)
@Reflect
struct Service {
    int64 value;
    Service(int64 value) { self.value = value; }
    @Route("/read")
    int64 read() { return self.value; }
    static int64 answer() { return 42; }
};
@Reflect
int64 addOne(int64 value) { return value + 1; }
```

生成发生在所有实例化、名称解析、布局和方法降级完成之后，检查模式也校验表，但不写产物。类型身份保留模块、位宽、符号、引用权限、容器和具体泛型实参；函数/方法身份同时包含归属和完整具体签名，不改变内部编号。字段偏移使用编译器既有聚合存储布局的八字节槽位，不是宿主手写 C++ 类型的 `offsetof`。拥有标志复用既有资源分类；此模块仅描述布局，不实现反射字段写入。

类型的实例方法、静态方法和构造函数均进入表，显式参数不包含 self。构造函数显示为 `new`，记录所属类型为返回类型；方法只读性复用现有编译器方法体分析，不能据此绕过后续调用的接收者校验。接口表保留默认体存在性，具体类型表保留已解析的默认方法和原注解；接口双向关系含继承关系，但元数据本身不执行方法。函数声明中的引用或一般聚合可被描述，不表示调用桥接已支持这些签名。

注解定义独立保留参数和默认值；已暴露声明及成员保留绑定后的实例参数。内建标记使用独立身份域，避免与用户同名注解碰撞。schema=1 的模块注解列表仍是汇总列表，不额外保存所属模块。示例见 `example/reflection/declarations.mgl`。

## Magel 类型化查询


在默认启用反射的工具链中使用 `import std::reflect;`；`--reflection` 可显式要求能力。`getStructInfo<T>`、`getEnumInfo<T>`、`getInterfaceInfo<I>`、`getFuncInfo(name)` 和 `getAnnotationInfo(name)` 返回 `Option`，缺失或名称不能唯一选择重载时返回 `None`。具体类型的 `getTypeId<T>` 返回稳定规范身份，`getInterfaceId<I>` 使用同一身份机制；`implementsInterface<T>(id)` 查询已暴露类型的接口关系。

`getAnnotations<T>`、`getFieldAnnotations<T>(name)`、`getFuncAnnotations(name)`、`getMethodAnnotations<T>(name)` 和 `getStaticMemberAnnotations<T>(name)` 返回注解实例列表，缺失为空列表。`AnnotationInfo` 保存定义，`Annotation` 保存定义身份和按声明顺序补齐的实参；默认值与实参使用封闭 `ReflectValue`，其 `asI64/asText` 等读取方法返回精确类型 `Result`。整数位宽和符号必须精确匹配。

编译器在完整布局和单态化后生成普通 Magel 查询函数，再通过权威文法解析和正常降低。三条后端共用值复制与 Drop 规则；查询返回独立拥有型快照，修改快照不改变编译时表。内部编号不作为稳定公开身份。没有用户声明时小写 `string` 在类型位置报普通未知类型错误；字符串规范身份为 `String`。

## Magel 调用桥接


自由函数使用 `std::reflect::call(name, args)`；结构体实例使用 `callMethod<T>` / `callMethodConst<T>`，静态方法使用 `callStatic<T>`。枚举对应 `callEnumMethod<T>` / `callEnumMethodConst<T>` / `callEnumStatic<T>`。`static T create(...)` 这类具名构造入口登记为构造入口，包含静态标志及具体类型返回身份；构造使用 `construct<T>(args)`；接口使用 `callInterface<I>` / `callInterfaceConst<I>`。除构造返回 `Result<T, ReflectError>` 外，调用均返回 `Result<ReflectValue, ReflectError>`，void 结果包装为 `ReflectValue::Empty`。

`args` 是 `List<std::reflect::ReflectValue>`，用 `I64(value)`、`I32(value)`、`Text(value)` 等显式选择精确类型。名称、个数及每个参数的稳定身份全部检查后才执行目标。不同位宽、符号、浮点类型及字符串不能隐式转换。精确签名选择重载；多候选匹配返回 `Ambiguous`。只读入口拒绝会修改接收者的实际实现，可写入口可调用只读方法。未暴露类型返回 `NotExposed`；不存在、个数和类型分别返回 `NotFound`、`ArgumentCount`、`TypeMismatch`。

`@Reflect` 类型同时暴露其已经编译的接口契约，包括继承和具体运算符接口。接口目标仍由现有 `Any` 及接口引用规则提供，不能通过反射保存局部对象地址。桥接检查实际类型已暴露和实际方法的可变性，再走普通接口分派器，覆盖实现及继承的默认实现；不会直接跳到接口默认函数体。

`Add`、`RightAdd` 与一般方法相同。一元 `Negate`、`BitNot`、`Positive` 的源码签名保留操作数形参，但反射将唯一操作数归一为隐式接收者，元数据显式参数数为零。编译器分派桩按实际类型同时提供 self 与其值副本，不开放任意聚合装箱；返回仍须是封闭标量或字符串。内部实际身份和一元入口仅允许编译器生成的节点调用，用户源码直接调用报错。

引用、一般聚合、容器、函数指针及变长参数/返回值不在封闭签名范围，返回 `UnsupportedSignature`。显式实例生命周期方法 `drop` / `close` 的动态调用也拒绝（同名自由函数或静态方法仍按普通签名处理），生命周期仍由普通确定性清理负责。未显式声明构造函数的结构体保留普通零参数默认构造，其 new 签名同步进入元数据。构造在全部实参验证后创建实例并通过 Result 接管；参数校验失败不会构造对象。Magel `panic` 沿用普通执行失败语义，不会将任意运行时异常转成 ReflectError。

`--hard-realtime` 标记整个编译单元不得启用反射，与 `--reflection` 同时出现立即拒绝；这是一项反射使用限制，不表示编译器已经提供完整 WCET 分析或任务调度验证。

## 示例与验证

以下回归分别检查能力矩阵、注解、元数据、查询、动态调用和端到端行为；它们不是一次完整 CI 的替代。
根目录工具链保持前端和运行时反射同时开启，关闭能力配置仅用于交叉加载与裁剪验证。

```sh
python tests/magel_reflection_features.py
python tests/magel_annotations.py
python tests/magel_reflection_metadata.py
python tests/magel_reflection_declarations.py
python tests/magel_reflection_queries.py
python tests/magel_reflection_calls.py
python tests/magel_reflection_e2e.py
```

[声明示例](../example/reflection/declarations.mgl)展示标记和元数据；
[调用示例](../example/reflection/calls.mgl)覆盖函数、方法、静态方法与构造；
[JSON 示例](../example/reflection/json.mgl)用已知字段构造模型，仍不是通用反序列化器；
[接口示例](../example/reflection/dispatch.mgl)检查实际接收者、默认实现和只读限制。
调用与查询得到的拥有值遵守普通复制和清理规则，修改查询快照不会修改编译时表。

## 历史性能测量与限制

下列数据来自 2026-10-10 Windows / GCC 11.2 的历史构建，保留实际命令和样本；
调整文档或发布 tag 不会把这些数据变成当前编译器的重新测量。


命令：`python tests/magel_reflection_benchmark.py --magel magel/magel-reflection-release.exe --iterations 1000 --repeats 5`。脚本使用 AOT O2，同一可执行程序分别执行静态调用、反射调用及空基线；每种模式预热后交替运行五次，检查相同递推结果。反射路径包括参数更新、精确检查、Result 与 ReflectValue 解包。原始样本见 [性能测量 JSON](reflection-benchmark-2026-10-10.json)。

| 调用形式 | 空基线中位数 | 静态总时长中位数 | 反射总时长中位数 | 扣除基线后的反射估计/次 |
| --- | ---: | ---: | ---: | ---: |
| 自由函数 | 218.314 ms | 221.454 ms | 316.454 ms | 98.140 μs |
| 实例方法 | 321.221 ms | 324.750 ms | 385.776 ms | 64.555 μs |
| 接口方法 | 210.253 ms | 215.916 ms | 299.329 ms | 89.076 μs |

测量为 Python 计时的整个进程墙钟时间，包含启动；静态与空基线差距很小，环境波动明显，因此扣基线估计不适合据此推导可靠的倍数。包含启动的总时长比为 1.43/1.19/1.39，不能视为函数调用开销比。此前自由函数十万次反射预热曾在 180 秒超时；尚未确认原因，也不能假定线性扩展。该历史测量未证明最初规格的 5–10 倍目标，性能优化与更稳定的进程内计时仍需后续工作。
