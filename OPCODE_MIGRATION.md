# Opcode 与标准库类型迁移执行记录

本文件记录本次迁移的实际状态和验证证据，不作为已交付能力说明。
功能文档只在对应实现和回归通过后更新。

## 当前主线状态

- `83f9852` 已合入并推送 `origin/master`：包括连续 u8 内存访问、定长数组值复制、普通字面量合并及浮点/二进制数组胶水修复。下文隔离检出的记录是各阶段的验证历史，不再表示这些基础改动尚未合入。
- 主检出的编译器、独立导出器与 `vscode/dist/magel-editor-1.0.0.vsix` 已替换为修复后的验证快照；编译器 SHA256 为 `cb0f65cfbc952b91d30e9f50d09f8d81824afe3eecfc7fdf4bf5d471c36a2cd4`，版本保持 1.0.0。主检出运行定长数组值语义样例通过。
- 快速门禁原始结果为九步成功八步；其中样例步骤的八个失败用例修复后逐项 VM 复测通过，三个对应正常样例 checked AOT 通过。没有把原始失败的整轮日志改记为绿色。
- String 的语言字面量类型迁移、只读数据段、原生记录隔离，以及 opcode 关键字、复制/NonCopyable 检查、ABI 接入与反射/LSP 隐藏仍待实现；Map、Set、List 尚未隔离。

## 已确认的语义

- 字符串字面量采用带长度的 UTF-8 字节数组，不附加终止零字节。
  `"hello"` 有 5 个字节，`"你好"` 有 6 个字节。
- `T[] s` 按值传参，复制数组，元素沿用各自的值语义。
- `T[] & s` 与 `T[] const& s` 是只读引用，不复制，编译期禁止写入。
- `T[] mut& s` 是可变引用，不复制，修改作用于来源数组。
- 上述规则适用于全部数组元素类型，取消先前讨论的 `T mut[]` 方案。
- 按用户补充要求，不添加解引用运算；数组引用通过 `s[i]` 和 `s = value` 直接访问。
- 字面量可以复制给按值数组形参，也可以只读借用；不能直接可变借用。
- `char[]` 隐式转换为 `String` 采用复制，避免引入借用字符串的生命周期分支。
- 按用户最新要求，迁移顺序为 String → Opcode → Map → Set → List → 大数、File、Proc；三类容器优先于后续资源类型。
- Set、Map、List 的迭代顺序不作保证；不在胶水迭代路径中排序。

## 起点核对

- origin 基线与初始 HEAD：`8d9efd15e76ce26bff9964775535594fca68d6e4`。
- 初始工作区干净，只有 origin 远端；不涉及 GitHub 推送。
- `ObjectKind` 中的对应种类：String 无独立种类；BigInt、BigFloat、Vector（List）、
  Set、Map、File、Process（Proc）。
- `RuntimeObject` 语义字段：`number`、`decimal`、`decimalPrecision`、`items`、
  `entries`、`mapEqual`、`mapHash`、`mapComparing`、`file`、`filePath`、
  `fileError`、`fileSucceeded`、`process`、`command`。
  List 还持有 `elementViews`、`refreshingViews`、`iteratorLeases`、`pendingRelease`。
- String 在 VM 中采用带长度头的堆字节串，字面量由 `__vm_str_lit` 从只读源复制；
  `strLen` 读取长度头，必要时沿用旧 C 字符串读取路径。
- `LowerType` 已把 char 映射为 u8；但 `ElementBytes` 仍按槽分配，char 的引用读写与
  数组复制仍有专用分支。不能据此宣称字节数组布局已经完成。
- 基线的 `T[]` 语法存在，`T[] &` / `T[] const&` / `T[] mut&` 语法尚不支持。

## 阶段零：进行中

### 数组传参前置改动

- 先补 `example/samples/array_parameter_values.mgl` 与对应期望输出。
- 补 `tests/magel_array_parameters.py`，覆盖 VM/AOT、只读写入、元素取可变引用、
  只读引用转传与局部数组引用逃逸。
- 旧编译器执行 `magel --check example/samples/array_parameter_values.mgl`：
  在 `char[] &` 的 `&` 位置报语法错误，确认新增测试覆盖尚未实现的语法。
- 权威文法已增加三种数组引用；按值形参复用 `CopyRuntimeArrayValue`，
  并把副本登记为数组所有者，覆盖参数被取地址后的间接存储路径。
- 数组引用下标与元素取地址统一先解引用数组句柄，再按已有布局读取长度和元素。
- 可变数组引用整体赋值先物化新数组，再释放旧元素与旧块，支持自赋值。
- `float32ToBytes` / `float64ToBytes` 的运行时输出数组改为 `uint8[] mut&`，
  容量不足仍返回 false，检查通过后才取得首元素引用。
- 初轮 `tests/magel_array_parameters.py` 的 VM/AOT 与 6 个拒绝用例通过；
  随后增加整体替换、自赋值、空数组、泛型、按值返回、形参取地址及另外 3 个只读用例。
  更新后的 VM/AOT 与 9 个拒绝用例均通过；
  不添加解引用运算，测试沿用数组引用直接访问。撤回后的 9 个拒绝用例与
  VM/AOT 专项再次通过，日志为 `build/array-parameters-direct-targeted.log`。
  最终门禁与提交记录待完成后回填。
- MCU 默认宿主构建通过：`make -C library/stackvm_mcu -j4`，
  实际编译包含 `-fno-exceptions -fno-rtti`；新增样例已纳入 `MUST_PASS`。
- `python tests/mcu_vm.py --magel magel.exe` 通过：无异常符号，43 个样例等价性，
  文件后端、FileStream、移动所有权、流式输出与软重启附加检查通过。
  完整输出保存在 `build/array-parameters-mcu-regression.log`。
  第二轮相同门禁也通过（43 个样例，`build/array-parameters-final-mcu.log`）；
  该轮包含后来按用户要求撤回的解引用扩展，最终交付仍需对撤回后的构建复测。
- 最新完整门禁中的样例步骤通过：253 个正常样例与 307 个错误样例全部通过；
  图片与 StringBuilder 的 VM/AOT、模块裁剪和文件往返步骤也通过。
  后续专项发现两项既有门禁问题，尚不将整个 `ci.sh --full` 记为成功。
- 完整门禁第 32 项暴露 Python 符号转换器拒绝默认反射产物；已支持已知特性位、
  原字节保留反射段，并有界检查 schema 1 结构。原专项及追加的七类损坏输入均通过，
  日志为 `build/array-symbol-reflection-final.log`。
- 第 34 项源码行号夹具仍用 0.16.0 已删除的 `##`；改用已支持的单行 `$concat`，
  保留宏定义总行数和 `main.mgl:9` 断言，正在复测。
  改写时尝试多行 `$concat`，另发现参数名中的内部来源标记未清除这一既有问题；
  本次不扩展宏语义，该问题未修复并在此记录。
- 本步骤没有删除 VM 类型字段、ObjectKind 或操作码；尚未开始 String 数据下沉。

### String 依赖审计（尚未迁移）

- 前端：`types.cpp`、`builtins.cpp`、`boxing.cpp`、`lowering.cpp`、
  `aggregate_ownership.cpp` 等 15 个 epilogue 文件仍含 `MType::String` 分支。
  字面量推断在 `RawExprType`，字面量降级在 `LowerUncheckedExpr`，
  变量物化及复制在 `MaterializeStringValue` 等入口。
- `library/simple_ast/simple_ast.cpp` 的 `StringLit` 发出字符串数据项；
  普通字面量每次生成名称，只有具名字符串按名字复用。不能宣称普通字面量已经合并。
- `library/stackvm/parts/machine_natives.cpp` 的 `nativeReadCString` /
  `nativeAllocString`、`strLit`、`strLen`、`strJoin`、`strSub`、`strCmp`、
  `strPos`、`strAt`、`strSet`、`strRef` 及格式化、输入、容器、大数、文件路径等调用方
  都依赖现有表示；不能只把几个字符串函数换个文件就算剥离。
- `ObjectValue` / `ObjectKey` 仍有 `isText` 与 `text`，String 改成原生记录后，
  容器值复制、键比较以及 `NativeCallContext::readList` 等胶水接口也要同步处理。
- `machine_value_text.cpp` 仍通过旧字符串读写构造默认值文本；
  `aot_bridge.cpp` 在运行时诊断、协作执行和公开字符串桥中依赖相同入口。
- `DataArea::readCString` 还用于解析函数标签，不能把这类元数据读取和语言 String
  混为一谈后直接删除。字节数据区与只读范围、长度头、字面量合并需要独立实现。
- `NativeCallContext` 已有 `readString` / `makeString`，但尚无通用宿主记录编号与
  值复制回调接口；这部分要在 String 下沉时新增，并同时覆盖 VM/AOT 的复制与释放。

### 版本与意外发现

- 按用户明确要求，当前版本保持 1.0.0；数组形参与字节输出 API 的变更
  记入 1.0.0，根 VERSION、编译器、LSP、插件清单与锁文件保持一致。
- 初次验证可变数组引用整体替换时，旧路径复现局部数组离开后调用方越界读取，
  已增加回归并修复复制/释放顺序。
- 首轮全部 253 个正常样例通过。5 个旧类型拼写错误用例仍在原位置拒绝，
  但数组引用文法使期望 token 多出 `[`，因此只同步这 5 条候选列表：
  `legacy_ioption_type`、`legacy_iresult_type`、`legacy_map_type`、
  `legacy_set_type`、`legacy_vec_type`；同步后单独复测全部通过。
- 开启反射的独立 VM 测试通过：294 项断言、二进制中间产物损坏输入、闭包所有权、
  十万符号和 panic 栈；关闭 panic 跟踪的专项构建及测试也通过。
- 字节输出旧重载依赖隐式共享数组，已迁移为显式可变引用，保持原样例期望输出。
- `objdump -p magel.exe` 发现旧构建依赖 libgcc、libstdc++、libwinpthread 三个
  MinGW DLL，而 VSIX 只复制 EXE；Windows 工具链已改为静态链接运行库。
  `objdump -p` 确认主程序仅导入 KERNEL32、msvcrt、WINHTTP，独立导出器仅导入
  KERNEL32、msvcrt；两者在仅保留 Windows 系统 PATH 的环境中启动成功，
  主程序在仓库外目录运行数组形参样例，输出与期望一致。
- 编译还报告已有 `source/glue/std_image.hpp` 中 `ok` 可能未初始化的警告；
  本步骤未修改图片胶水，也不将该警告归因为本次数组改动。

### 环境记录

- 初始 `sh ci.sh` 停在环境检测：缺少 ParseEngine。
- `sh gen.sh` 已通过仓库自带下载逻辑获得指定的 ParseEngine v1.1.0。
- 首轮构建成功；Git Bash 运行样例返回 127，而 PowerShell 可启动编译器。
  将 `/d/MINGW64_YACC/bin` 放到 PATH 最前后，Git Bash 启动验证成功。
  首轮样例门禁结果无效，已终止，后续门禁使用修正后的 PATH。

## Opcode 起点补充核对（尚未迁移）

- 附件称 `Instruction::opcode` 是字符串，但当前基线实际已存在 `enum class Op`
  和 `Instruction::op`（`library/stackvm/stackvm.hpp`）；没有 `Instruction::opcode`。
  VM、融合和寄存器核心已按该枚举分派，不能把已有能力描述成本次新增。
- 现有 `Op` 未指定底层宽度，枚举项依赖声明顺序，也没有单一 `.def` 码表及稳定区间。
  后续需保留已有整数执行路径，补齐稳定编号、单源码名称映射和一致性校验。
- `.svmb` 执行程序也已经用整数保存指令：`stackvm.cpp::encodeProgramBinary`
  通过 `payload.u8(static_cast<std::uint8_t>(instruction.op))` 写码，解码还原 `Op`。
  目前格式版本 5 的该码只有一字节且依赖枚举顺序，不能容纳附件规定的 16 位分段。
  执行准备中的寄存器操作缓存同样序列化一字节，需要一起迁移并保留旧码映射。
- `parts/artifact_binary.cpp::encodeQuadBinary` 的 `MGQI\1` 仍把 `Quad::op`
  驻留进字符串表；`decodeQuadBinary` 从该表还原操作文本。
  这与运行时 `Instruction::op` 已是枚举并不矛盾：落盘层与执行层不同。
  后续必须明确两层的映射和旧二进制兼容方式，不能只更换执行枚举就声称完成整数序列化。
- 标量内存 `LoadAt` / `StoreAt` 在 VM 和寄存器服务中仍固定读写 8 字节；
  四元式出现 `load:u8` 并不等于物理只读取一个字节。
  char/uint8 合流必须同步 VM、执行准备、寄存器服务、AOT 及前端元素步长。

## String 隔离实现检出

- 临时工作树：`build/string-migration-worktree`，分支 `codex/string-literals-20261019`。
  以相同数组前置改动为基础，当前尚未合并回 master，也尚未提交。
- 普通字面量合并的 C++ 回归先复现重复内容与空串两项失败，实现后 86 项断言通过。
  VM/AOT 集成回归已证明生成数据去重、UTF-8 长度和运行时字符串副本独立。
- 另已复现基线接受直接改写字面量，隔离检出正在加入字面量写入与可变字节借用的拒绝检查。
  不添加解引用运算；数组引用继续直接访问。
- 隔离验证还发现常量字节引用错误跳过八字节长度头，以及工具入口/工程生成器
  缺少内嵌资产构建依赖的问题；修复与统一 VM/AOT 验证正在隔离检出中进行。
- 这些结果只覆盖合并这一步；char[] 类型、无终止零字节、只读数据段及宿主记录仍未完成。

## 后续阶段

String 的字面量表示、原生记录、复制回调、操作下沉与旧路径删除尚未完成。
Opcode 单一码表、整数序列化、VM/AOT 派发迁移尚未开始。
其余七类尚未开始剥离。未记录性能提升，也未改变现有历史性能基准。

## 完整门禁发现的旧验证口径

- `.svmb` 的 Python 转换器原先拒绝默认启用的反射标记；已保留并有界校验反射段，交叉加载与七类损坏数据拒绝测试通过。
- 源映射测试仍使用已删除的 `##`；改用当前 `$concat` 并保留原行号断言，导出场景复测通过。多行 `$concat` 的内部源标记问题另记，尚未修复。
- 生成器下载测试改为核对 `gen.sh` 实际固定的 ParseEngine 1.1.0，四项成功、失败和清理测试通过。
- 闭包样例已经使用 `fn` 输出；专项改为读取已有输出文件，并同步夹具的 `fn capture modes OK` 预期，VM/AOT 与 36 个拒绝场景复测通过。
- `destructuring_guards` 有意生成一条 MGL1601，专项严格匹配该警告全文，其余样例与 AOT 程序继续要求无诊断；该样例 VM/AOT 复测通过。
- 默认参数夹具原先以 `Box box;` 零初始化却期待构造结果，改成显式 `new Box()`，VM/AOT 与十二项非法默认参数测试通过。
- 移动后重新赋值恢复已被现行语义禁止，`redeclare_reset` 改为检查 MGL1205，解释执行、AOT、别名和嵌套清理专项复测通过。
- 该轮完整门禁已停止，原因和稳定快照复测结果见下文；未宣称 `ci.sh --full` 通过。

## 值语义不可变约束

字符串和数组的初始化、赋值、按值传参及返回保持独立值语义，不得以共享可写存储代替复制。常量去重只合并不可写字面量；显式引用参数按约定借用。本轮额外补充 String 参数内写入、赋值副本写入、返回值写入和 String 数组元素的内部字节写入验证，增强数组专项 VM/AOT 与十一项只读/生命周期拒绝测试通过；隔离检出的增强 String 专项 VM/AOT 通过。

- 补查默认反射时发现构造回归误以为类型仅有构造方法，忽略自动 `toString`；探针确认实际为构造方法 `new` 加 `toString`，已改为遍历并精确检查唯一构造方法。该测试默认启用反射，不再默认跳过，正在复测。

## u8 布局进一步核对

- 现行语言已删除 `sizeof`，样例和文法中也无 `alignof` 查询入口；不得为迁就附件直接添加新运算符。后续以布局元数据、地址步长与 VM/AOT/MCU 读写护栏证明一字节存储，宿主编译期断言只证明宿主字节条件，不能替代语言布局验证。
- `ElementBytes` 当前对标量仍返回 8，`lowering.cpp` 的数组元素寻址仍多处固定步长 8，固定数组默认初始化按槽数处理。只修改 `__vm_char_load` 不足以完成连续 u8 数组，需同时覆盖固定数组、运行时数组、聚合字段、复制、地址计算及析构路径。
- `char` 的 `LowerType` 已为 Uint/8，但读写另走 `__vm_char_*`，而普通 `load:u8` 仍落入八字节内存操作；后续必须合并完整路径，不能把四元式类型标记当成物理布局证明。

## 后续隔离优先顺序（用户最新确认）

保持 String 语义与 Opcode 的基础先后依赖，随后优先完成 Map、Set、List 三类隔离，按 Map → Set → List 推进，再处理大数、文件及进程。三类均须维持普通值复制和原生侧克隆回调，不得让 VM 壳或原生记录表引入共享可写值。当前尚未宣称这三类已经隔离。

三类逐项验收，不以移动文件或改类型名代替隔离：

- Map：键值记录、Equal/Hash 调用与比较期间重入保护迁到标准库胶水；键和值的插入、读取、容器初始化/赋值/传参/返回保持原有值语义。
- Set：元素记录与键协议迁到胶水，保留去重、结构体键及字符串副本的行为；遍历不承诺顺序，不在胶水中增加排序来维持旧输出。
- List：元素记录、可寻址元素视图、游标借用租约和延迟释放迁到胶水，验证嵌套容器、拥有元素、回调重入及迭代中借用生命周期。
- 共用原生记录接口须让 VM 在复制壳时取得宿主记录编号并调用克隆回调；销毁通知不能代替克隆接口。VM 保留通用句柄与存活管理，不解释上述记录布局或方法名。
- 每一类均检查 VM/AOT 同胶水一致性、MCU 裁剪构建，以及旧 VM 字段与语义分支的实际删除；沿用现有行为回归并补充原生克隆回调计数与副本独立性验证。

## 独立 u8 核心检出

后续单字节内存工作隔离于 `build/byte-layout-worktree/`（`codex/byte-layout-20261019`），不改变本检出正在进行的完整门禁。其核心测试 324 项、紧凑字节码往返和最小 MCU 程序已通过；前端数组布局与完整 String 迁移仍未完成，尚未合入。

## 门禁快照与值语义补查

- 根检出完整门禁已停止：此前在门禁期间重链相同编译器，导致第 65 项之后一批 Windows 启动错误，不能据此认定功能失败。保留原日志，后续使用稳定编译器快照复测。第 77 项另有旧 `build/work/magel` 路径缺失，不属于同一原因。
- 引用综合专项已单独通过 VM/AOT 共 27 个样例；内存护栏专项已通过自定义堆、五类真实非法访问、隐私裁剪，以及显式 unchecked 的直接/make/模块输出。
- 数组值参数中的显式 delete 只删除副本。墓碑测试已改用 `int64[] mut&` 显式删除调用者数组；前端修正被引用数组与引用槽的释放对象，并保留引用槽收尾。增强数组专项 VM/AOT 与十一项只读/生命周期拒绝检查已通过，完整墓碑专项仍在复测。
- 新增定长数组探针在旧实现复现 `int64 second[2] = first; second[0] = 9;` 会改变 first 的别名问题（`build/fixed-array-values-probe.log`）。根检出尚未合入该问题的修复；u8 隔离检出的新样例已通过 VM/AOT 与 MCU 45 项样例及附加检查。String 完整迁移与隔离检出全量门禁仍未完成，不能描述为该能力已交付。
- String 隔离检出的快速门禁中 254 个正常、307 个错误样例、图像、数组及字面量单测均通过；复制编译器的专项因 Windows `.exe` 路径漏补失败，修正后专项单独通过。该轮尚未作为整套门禁绿色结果；新数组删除代码也需重新构建后验证。

## 本轮稳定快照补验

- `build/array-owned-delete-compiler.exe` 是新增自动清理存活检查后的稳定快照。数组专项 VM/AOT 与十一项拒绝检查通过，新增可变借用删除 String 数组的场景；本轮墓碑专项通过，包含四类对象清扫前后重复删除、跨清扫存活及大小块合并后 300 次复用。
- 文档相对链接检查 111 页无缺失，文法 EBNF 同步与 CHANGELOG/VERSION 1.0.0 一致性检查通过。
- u8 隔离检出新增定长数据区数组长度头，符号地址仍指向首元素，长度头不计入载荷大小；更新后的 simple_ast 单测 112 项断言通过，包含空数组和结构体数组逻辑长度。前端构建及连续字节、跨槽写入、结构体副本和 char/uint8 回绕探针通过 VM/AOT；内联长度头地址计算改用显式 i64 偏移，避免默认 i32 运算截断地址。定长数组初始化/整体赋值/自赋值、按值传参/返回、String 和含 List 的结构体复制，以及内联数组字段的新样例已通过 VM/AOT；同一稳定快照通过 MCU 45 项等价性与文件、移动、输出和软重启检查（`build/byte-fixed-array-final-mcu.log`）。
- 裁剪验证改为默认编译器验证完整能力，再显式重编译裁剪 AOT；大数样例沿用仓库的 `.in` 输入。本轮专项全部通过，含三个功能样例、外部静态池运行及大数符号移除检查。
- String 隔离检出的稳定快速门禁九步全部通过（`build/literal-pool-stable-ci.log`）；随后同步新增 String 数组借用删除修复并重新构建，后续专项另记，不能将后续代码当作该轮九步门禁的输入。
- 当前根检出的稳定快照通过 MCU 43 项样例等价性，构建关闭异常和 RTTI；1.0.0 VSIX 已重新打包并核对包内编译器与根检出字节一致。四个本轮改动的前端 C++ 文件格式检查通过；全仓格式检查另报既有 bignum 等文件格式问题，未批量改写无关库。
- String 检出同步最新自动清理修复后的构建、数组 VM/AOT 十一项拒绝专项及 String 字面量 VM/AOT 专项均通过，分别记录于 `build/literal-owned-delete-*.log`。
- 根检出的快速门禁五个功能步骤通过：构建、253 个正常样例、307 个错误样例、图像/StringBuilder 与数组专项；最后的索引步骤因本轮新增 C++ 清理代码使源码行号漂移而失败。已重新生成三个索引产物并通过 `library_reference.py --check`，随后重新内嵌、重建驱动和独立导出器，数组专项 VM/AOT 十一项拒绝检查与导出目标十四项检查再次通过。未把原始返回非零的门禁日志改写成全绿。
- 最新 1.0.0 VSIX 已重新打包，包内编译器与根检出逐字节一致，SHA256 为 `3471ec42922283acfd07daf0b4e78514f2d09111cf63621f61f74ae8465295f2`；尚未交付 String/u8 隔离检出的编译器。
- 根检出的内嵌脚本重复执行已验证内容与文件时间戳均保持不变（`build/array-final-embed-stability.log`），后续无变化时避免重编译资产使用者。

## 已确认的 opcode 语言声明约定

- `opcode` 为区分大小写的小写语言关键字，类型声明采用 `opcode File { ... }`；它表示胶水内部对象的引用对象，不能当作普通整数句柄赋值复制。
- 默认可复制的 opcode 类型必须在声明体内声明复制胶水函数；语言层复制须调用该函数，取得独立的胶水记录，禁止仅复制内部指针。
- `#NonCopyable` 修饰 opcode 类型后，在编译期拒绝复制初始化、赋值复制、按值传参及按值返回；现有显式移动规则继续有效。File 等资源对象可据此禁止复制。
- 这是用户已经确认的语言约束，目前尚未实现为权威文法及类型检查能力。具体复制函数签名及胶水绑定随实现维护，不把这里的约定当作已支持 API。
- String、数组继续遵守值语义；opcode 的内部引用表示不能使它们的普通值复制产生共享可写状态。版本保持 1.0.0，不增加解引用运算符。
- ABI 与 C++ 胶水共用 opcode 声明及复制/移动限制；ABI 绑定的宿主对象也必须提供复制函数或显式标记 #NonCopyable，不能退回整数别名绕过所有权规则。
- opcode 在语言中的角色等同胶水侧 void* 不透明对象指针；语言不解释其目标内存布局，复制必须经声明的胶水函数。
- opcode 对象不进入反射结果及 LSP 提示；内部类型检查、绑定和所有权信息仍保留，不能靠退化为整数来实现隐藏。

## 字节常量与稳定门禁续验

- simple_ast 新增 byteArray 节点，与内部带尾零的 C 字符串分别合并；产物使用 u8 数组长度头和精确字节载荷，不添加尾零。单元测试 124 项断言通过，新增跨函数合并、UTF-8 六字节文本、内嵌零、空数组及相邻数组边界检查。
- 这是低层节点能力，语言层 MNode::Str 尚未切换，不能宣称字符串字面量已经变为 char[N]，只读数据段及 String 原生记录隔离仍未完成。
- VM 核心从 library/stackvm 工作目录复跑，324 项断言通过，包含样例文件；另一次从项目根运行的日志跳过样例，不作为完整证据。
- 定长数组样例加入 tests/magel_array_parameters.py，日常专项覆盖运行时及定长数组 VM/checked AOT；隔离检出快速门禁仍在运行，最终状态以后续退出码为准。
- 本轮隔离检出快速门禁的构建通过，样例步骤发现 8 项失败：lang_float_bytes、lang_float64_bytes、std_binary 以及五个 binary 错误样例。均定位为浮点数组胶水和二进制数组导入仍按八字节槽访问；原始日志 build/byte-foundation-ci-run.log 与 tmp/ci/logs/03.log 保留。
- 两处胶水改为连续 u8 访问，保留读取范围检查及浮点写入前的有效性检查。修复编译器单独输出至 build/byte-glue-fixed-compiler.exe，不覆盖门禁使用中的 magel.exe；8 项针对性 VM 复测全部通过（build/byte-glue-fixed-targeted.log）。checked AOT 仍在复测，尚未认定完整门禁全绿。
- 独立 AOT 导出器已重建并通过 14 项真实导出检查；该快照早于上述两处胶水修复，交付前仍须同步最新内嵌资产重建。
- 原始快速门禁最终退出 1：9 步成功 8 步，仅样例步骤失败；图像/StringBuilder、运行时及定长数组 VM/AOT 十一项拒绝专项、124 项字面量底层断言、字面量 VM/AOT 与索引检查均通过。
- 修复版 float32、float64 与 std::binary 三个 checked AOT 样例均通过，日志 build/byte-glue-fixed-aot.log；对应 8 项 VM 复测全通过。不能把修复前的整轮 CI 改记为绿色。
- 修复版 1.0.0 VSIX 完成 npm run package 及插件测试，ZIP 清单、编译器 LSP initialize 的 serverInfo.version 与 VERSION 一致，内嵌编译器与 build/byte-glue-fixed-compiler.exe 逐字节相同，SHA256 为 cb0f65cfbc952b91d30e9f50d09f8d81824afe3eecfc7fdf4bf5d471c36a2cd4。
- 使用两处胶水修复后的最新内嵌资产重建独立 AOT 导出器，十四项真实导出检查再次全部通过（build/byte-glue-fixed-emitter-test.log）。
