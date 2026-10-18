# 无名变长符号表与空间对比

同样使用 control.mgl，对比普通字节码和 --compress-symbols 产物。程序行为不变；格式、名字保存位置和诊断隐私发生变化。

## 从同一源文件生成两种容器

```sh
mkdir -p build/doc-example
./magel svmb example/samples/control.mgl --no-compress-symbols --out build/doc-example/control-raw
./magel svmb example/samples/control.mgl --compress-symbols --symbol-map build/doc-example/control.symbols.json --out build/doc-example/control-compact
```

输入是[control.mgl](../example/samples/control.mgl)，行为基准是[control.txt](../example/expected/control.txt)。普通容器使用 PESVMB v5，压缩容器使用 MCVM0005，加载器识别魔数；无需改 Magel 语法。旧格式字节码须重新编译。

## 对比什么才能证明压缩正确

比较文件大小只验证空间收益；还要加载运行并比较 stdout，再用 PC 映射核对编号与原名。映射项包含 logical_id、physical_idx、original、kind、line、ref_count；logical_id 用十进制字符串，避免 JSON 浮点丢精度。映射用于 PC 管理，不应与隐藏名字的产物一并分发。

编译器默认为输出前缀追加 .symbol_map.json，命令中 --symbol-map 明确指定位置；magel 配置中的相对映射路径以配置目录为基准，CLI 则以启动目录为基准。增量缓存也跟踪映射内容，丢失或修改映射会使产物缓存失效。

压缩不隐藏字符串常量，也不加密机器码。panic 仅保留错误消息，隐藏位置与调用记录；具体诊断规则见 [panic 调用栈](18-panic-trace.md)。下文给出格式、工具、实测及严格 LEB128 校验。

## 数据流与接口

magel.gram 的语法不变；转换发生在组装后的 Program 与二进制序列化层。

~~~mermaid
flowchart LR
    A["源码或含名字的 raw.svmb"] --> B["收集导出、导入、内部、类型及形参"]
    B --> C["FNV-1a 64：稳定逻辑 ID，检查碰撞"]
    C --> D["统计指令 name / name2 引用次数"]
    D --> E["频率降序，逻辑 ID 升序打破平局"]
    E --> F["连续物理索引：表项与引用用 ULEB128"]
    F --> G["compressed.svmb：MCVM0005"]
    E --> H["PC symbol_map.json：原名与编号"]
    G --> I["VM 编号寻址：宿主与数据区用同一哈希绑定"]
~~~

核心结构与严格 LEB128 API 在 [stackvm.hpp](../library/stackvm/stackvm.hpp) 的 symbols 命名空间：

~~~cpp
enum class Error { Ok, Truncated, Overflow, Overlong, NonCanonical };
template<class Vector> void writeVarUInt(Vector& out, uint64_t value);
Error readVarUInt(const uint8_t* data, size_t size, size_t& pos, uint64_t& value);
struct SymbolRecord {
    uint64_t logicalId, physicalIndex, offset, size, refCount;
    uint8_t kind, flags;
    int32_t line;
    Memory::String original; // 只在编译端映射存在；加载器填编号键
};
~~~

读取失败不改变游标与输出。逻辑 ID 基于**名字本身的 UTF-8 字节**；模块名/版本只进入 PC 映射元数据。同名符号跨模块拥有相同 ID，模块身份须由调用方区分。哈希碰撞报错，不通过追加盐改变稳定编号。

saveProgramBinary(program, path, diagnostics, true) 写新格式，saveSymbolMap 输出匹配映射。无名文件不能再次转换或恢复原名。

## 构建与用法

在仓库根目录：

~~~sh
cmake -S library/stackvm -B build/symbols -DSTACKVM_WITH_REFLECTION=ON
cmake --build build/symbols --parallel

./magel svmb example/samples/control.mgl \
  --out build/symbols/control --compress-symbols \
  --symbol-map build/symbols/control.symbol_map.json

./magel svmb example/samples/control.mgl --no-reflection --no-compress-symbols --out build/symbols/python-raw
python tools/svmb_compress.py --input build/symbols/python-raw.svmb \
  --output compressed.svmb --map symbol_map.json --module NAME --version 1

build/symbols/svmb_symtool dump compressed.svmb
build/symbols/svmb_symtool stats compressed.svmb
build/symbols/svmb_symtool lookup 5 --map symbol_map.json
build/symbols/svmb_symtool lookup calculate --map symbol_map.json

build/symbols/magel-stackvm compile input.quad --out output.svmb \
  --compress-symbols --symbol-map output.symbol_map.json
~~~

Windows 可执行文件加 .exe；多配置生成器可能把文件放在 Release/。magel 默认映射为 <输出前缀>.symbol_map.json；独立 StackVM CLI 默认为 <输出文件>.symbol_map.json。
Python 转换器只接受不带特征位的普通 v5 输入，因此上述 Python 对照显式关闭反射。
需要保留反射时，直接使用编译器的 `--compress-symbols` 或支持反射的原生工具。

映射记录包含 logical_id、physical_idx、original、kind、line、ref_count。logical_id 用十进制字符串，防止 JSON 消费端浮点转换丢精度。line 优先保存源码行号，否则为四元式行号，未知为 0。

~~~toml
[package]
name = "symbols-demo"
version = "1"

[magel]
run = "svmb"
compress_symbols = true
symbol_map = "symbols.json"
~~~

--incremental 跟踪压缩选项、映射路径、配置及映射哈希。映射缺失或改变会使产物缓存失效。--no-compress-symbols 同时停用配置中的映射输出。

## 当前容器格式

现行加载器只接受对应的 v5 容器，拒绝旧 PESVMB v1/v2/v3/v4。紧凑容器使用独立魔数 MCVM0005，定长头部 44 字节，全部小端，按字节序列写入：

| 偏移 | 字段 | 编码 |
| ---: | --- | --- |
| 0 | magic | 8 字节 MCVM0005 |
| 8 | version | u32，5 |
| 12 | flags | u32，普通程序为 0，反射特征位为 0x0002 |
| 16 | export_count | u32 |
| 20 | import_count | u32 |
| 24 | code_size | u32，指令及配套元数据区总长度 |
| 28 | const_size | u32，0；常量仍随指令内联 |
| 32 | symbol_table_off | u32，文件绝对偏移 |
| 36 | symbol_table_size | u32 |
| 40 | crc32 | u32，CRC32/IEEE 覆盖头部后的全部内容 |

指令区先写模块名逻辑 ID 和指令数，再写指令及标签/类型/形参/导出表。保留原操作数存在位图，操作码与位图各为 u8；名字引用改为物理索引 ULEB128。计数、标签下标、大小、四元式序号用 ULEB128；有符号立即数和类型位宽用 ZigZag+ULEB128。浮点保持 IEEE 754 位，字符串常量保持长度与 UTF-8 数据；格式和标志保持 u8。

标签及表项 offset 是**指令序号**，不是指令字节偏移，故编码长度变化不改变跳转目标。size 保存符号类型大小，无相应数据时为 0。标签/类型/形参/导出关系只存编号；调试节条数为 0。

调试记录后保存内部执行序列（来源下标、操作、类别、跨度、立即数、位宽、标志与跳转目标）；
启用反射时再写元数据段。`code_size` 覆盖指令、各表、内部执行序列及可选元数据，随后才是符号表。

符号表由头部范围解码至末尾，每条：

~~~text
logical_id:varuint physical_idx:varuint kind:u8 flags:u8 offset:varuint size:varuint
~~~

kind 是可组合位：1 变量/指令名字、2 标签、4 类型名、8 外部调用、16 导出。flags 保留为 0；索引从 0 连续递增。加载器检查 CRC、范围、最短编码、编号重复、索引连续性、引用、标签偏移及导出数量。数量上限 1000 万，文件上限 512 MiB。

## 空间实测

2026-10-08 的历史格式测量：[test_symbols.cpp](../library/stackvm/test_symbols.cpp) 当时生成 N 个符号、5N 次引用，最热符号出现 4N+1 次，其余各一次，名字形如 calculate_symbol_12345，无调试节。以下是当时的**完整文件大小**，不代表现行 v5 的重新测量：

| 符号数 | 引用数 | 原始含名字 .svmb | 无名 LEB128 .svmb | 减少 |
| ---: | ---: | ---: | ---: | ---: |
| 10 | 50 | 1111 B | 360 B | 67.60% |
| 1000 | 5000 | 106951 B | 31715 B | 70.35% |
| 100000 | 500000 | 10888951 B | 3312958 B | 69.57% |

固定宽度模型保持同一无名符号表及其他字节，只替换指令引用宽度，计算为“压缩文件大小 − 变长引用字节数 + 引用数 × 固定宽度”。**这是空间估算，不是可加载的另一种格式**：

| 符号数 | 固定 16 位引用模型 | 固定 32 位引用模型 | LEB128 文件 |
| ---: | ---: | ---: | ---: |
| 10 | 410 B | 510 B | 360 B |
| 1000 | 35843 B | 45843 B | 31715 B |
| 100000 | 无法表示，索引溢出 | 4629470 B | 3312958 B |

10 符号的引用从 100 B 降至 50 B；相对同一无名表的固定 16 位完整文件模型只省 12.20%，不能承诺整文件一定省 30%。相对原始含名字格式则超过 30%。FNV-1a 哈希通常需要 9–10 字节 ULEB128，不一定比固定 u64 更省；收益主要来自移除重复名字和压缩索引、元数据。

仓库保留的 control-stripped.svmb 从 **249681 B → 166481 B，减少 33.32%**。真实统计：

~~~text
file_bytes=166481 symbols=5724 references=14081 reference_bytes=24803
1-byte: 3359 (23.8548%)
2-byte: 10722 (76.1452%)
3-byte: 0 (0%)
~~~

合成 1000/100000 符号样本的一字节引用占比为 82.56%/80.0256%。真实模块分布不同，不能普遍保证超过 80%。

当前工具链重新编译 magel/example/samples/control.mgl（按需核心标准库）：
**46147 B → 27125 B，减少 41.22%**。加载两个文件并配各自数据区执行，输出完全一致，
包括 sum 1..5 = 15、循环 classify 和布尔打印；共执行 357 条指令。

~~~text
file_bytes=27125 symbols=986 references=2097 reference_bytes=3471
1-byte: 723 (34.4778%)
2-byte: 1374 (65.5222%)
3-byte: 0 (0%)
~~~

## 逆向难度

| 产物 | 可直接得到的内容 | 分析成本 |
| --- | --- | --- |
| 原始含名字 .svmb | 函数/变量/类型名、标签、调用关系 | 格式解码后语义线索充足 |
| 无名紧凑 .svmb | 操作码、常量、类型数值、编号、调用关系 | 需额外推断名字及用途 |
| 无名文件 + symbol_map.json | 映射中的全部原名 | 原名恢复直接 |
| 无名文件 + 候选源码/标准库 | 可计算候选名字的 FNV-1a 并匹配 | 常见名字和已知 API 可识别 |

LEB128 与重排的解码成本有限，稳定编号还可用于跨版本关联；FNV-1a 不是密码学哈希，不能阻止字典匹配。符号隐藏增加语义恢复工作，不等于加密保护；没有攻击实验支持的“破解耗时倍数”不应给出。

.svmb 不存符号原名字段，但字符串常量、程序主动输出或反射文本可能与符号名相同；保持这些值才不改变行为。.data.bin、.layout、.quad、.abi.cpp 等 PC 中间产物仍可能含名字与函数指针字符串，本任务没有改其格式。宿主函数和数据区用原名计算同一 ID，兼容直接/间接调用及地址查找。

## 自动验证

~~~sh
ctest --test-dir build/symbols --output-on-failure
python magel/tests/test_svmb_symbols.py --build build/symbols --magel magel/magel
~~~

Windows 加 .exe。前一条生成 build/symbols/symbol-fixtures/，后一条依赖这些样本。本次独立 VM 构建关闭大数，完整 magel 保留大数。

| 用例 | 输入/预期 | 自动验证 |
| --- | --- | --- |
| LEB128 边界 | 0、127、128、16383、16384、2^32−1、2^63−1、2^64−1 往返 | C++ + Python |
| 非法编码 | 截断、非最短、溢出、超过十字节拒绝；C++ 游标不变 | C++ + Python |
| 小模块 | 10 符号、50 引用，相对含名字文件减少超过 30% | C++ 大小断言 |
| 大模块 | 100000 符号，连续编号及三字节引用有效 | C++ 加载 + stats |
| 稳定、增量 | 重复生成字节一致，新增符号后旧逻辑 ID 不变 | C++ + Python |
| 隐藏、反查 | 无 calculate_symbol_；编号与原名双向查询正确 | Python |
| 跨语言 | 合成及真实样本的文件、映射逐项一致 | Python |
| 元数据边界 | 错误头部/引用/索引/截断/尾部垃圾拒绝 | 重算 CRC 后调用 C++ |
| 执行等价 | 类型、浮点、乘除、递归、堆、宿主回调输出一致 | C++ 与七个 .quad |
| magel/CLI | 配置开启、CLI 覆盖、自定义映射、缺映射重建 | 带 --magel 的回归 |

启用符号压缩时，panic 只保留原始错误消息，不输出处理路径或任何函数调用记录；即使提供外部调试表亦然。直接解释运行和 VM/AOT 导出继承此策略，见 [panic 调用栈](18-panic-trace.md)。
