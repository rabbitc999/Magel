# 标准库存储、复制与字符串方法

标准库按实际拥有关系管理宿主状态。连接、分配区和定时源只有一个所有者；容器和随机序列使用值复制；共享槽明确保留共享语义。相关接口见[API 索引](library/README.md)，回归入口为 [opcode 与字符串测试](../tests/chali_opcode_values.py)。

## 拥有与复制

| 类型 | 普通复制 | 生命周期及快照 |
| --- | --- | --- |
| `std::sql::Database` | 禁止，使用 `move value` 转移 | 宿主记录统一保存连接、驱动、错误及影响行数；退出作用域自动关闭，`close()` 可重复调用 |
| `std::memory::Arena` | 禁止，使用显式移动 | reset 或释放使所有分配出的 Span 失效；Span 本身不拥有存储 |
| `std::event::Timer` | 禁止，私有 opcode 独占定时源 | stop 及作用域退出取消定时源；`id()` 和 `Event.timer` 只是来源标识，不承担释放责任 |
| `std::random::Random` | 深复制当前序列状态 | 副本从相同位置开始，后续调用独立推进；固定种子序列保持可复现 |
| `Graph`、`UnionFind`、各类树及堆 | 深复制宿主状态 | 查询和修改不往返复制内部列表；仅显式快照或遍历生成结果列表 |
| `LinkedList` | 深复制节点并重建游标索引 | 删除或 clear 后旧游标永久失效；副本内保留原游标与对应节点的关系 |
| `JsonValue`、`TomlValue` | 深复制拥有的子树 | 提取子值产生独立值；close 只影响当前值，副本及已提取子值继续有效 |
| `Shared<T>` | 复制同一共享槽的引用 | 槽及槽内值由线程组拥有；get/set/swap 每次调用原子，组合调用不构成事务 |
| `Regex` | 共享不可变编译模式 | 每次匹配使用独立匹配状态；最后一个引用释放时回收编译结果 |

JSON 与 TOML 使用同一宿主拥有型协议。公开包装保留 `Iterator`，实际存储是私有 opcode。TOML 根值的 dump 保留原文注释、空白与换行，反复解析和保存不会增加或删除注释；子值独立输出自身的值。

`Graph.snapshot()` 返回独立邻接数组和标量属性；`UnionFind.parents()`、`sizes()` 显式复制对应数组。并查集 find 使用路径压缩，unionSets 按集合大小合并，单次查询不扫描或复制完整容量。`BinarySearchTree.snapshot()`、`GeneralTree.snapshot()` 包含有效槽标记和根下标；`MinHeap.snapshot()` 返回独立堆数组。`LinkedList.toArray()` 和各树遍历返回独立值序列。修改快照不会反向修改容器。

容量趋势的[原始测量](benchmarks/opcode-values.json)记录了版本、配置与固定种子：排除构造时间后，1,000 和 1,000,000 个元素各查询 30,000 次，约为 57.5 ms 和 56.9 ms。这是同一主机上的单次验收数据，不是跨机器的性能承诺。

## 红黑树删除

`RedBlackTree.remove(key)` 删除存在的键并返回 true，不存在时返回 false。删除黑节点后通过兄弟变色、旋转及向上修复恢复黑高；删除的节点槽回收复用。插入、删除和查询保持对数复杂度。双孩子节点使用后继键替换；内部节点槽不是公开引用。

`validate()` 检查严格排序、父子连接、根为黑色、红节点的孩子为黑色、各路径黑高一致及有效节点数。`inorder()` 显式导出独立升序快照。测试包含 12,000 次固定种子的混合插入删除，逐步检查红黑性质并与独立集合记录核对，还覆盖空树、删空重插、副本、快照及 int64 极值。

## String 方法

这些方法由自动加载的 `std::lang` 提供。转换方法返回新的 String，不修改接收者。String 不能保存内嵌零字节，需要任意字节时使用 `List<uint8>`。普通索引、长度及切片沿用 UTF-8 **字节**口径；显式 Unicode 方法按**码点**处理，不等同于用户可见字素簇，也不执行 Unicode 归一化。

| 分类 | 方法 |
| --- | --- |
| 长度与状态 | `length()`、`size()`、`byteLength()`、`isEmpty()`、`isBlank()` |
| ASCII 分类 | `isDigit()`、`isAlpha()`、`isAlnum()`、`isAscii()` |
| 查找与比较 | `indexOf(text, start=0)`、`lastIndexOf(text, start=-1)`、`contains(text)`、`startsWith(text)`、`endsWith(text)`、`count(text)`、`compare(text)` |
| 字节读取与切片 | `byteAt(index)`、`substring(start, count=-1)`、`slice(start, end)`、`left(count)`、`right(count)`、`bytes()` |
| 修剪与大小写 | `trim()`、`trimStart()`、`trimEnd()`、`toLower()`、`toUpper()` |
| 替换与编辑 | `replaceFirst(before, after)`、`replaceAll(before, after)`、`replace(before, after)`、`removePrefix(prefix)`、`removeSuffix(suffix)`、`insert(index, text)`、`remove(start, count=-1)` |
| 组合与填充 | `repeat(count)`、`padStart(length, fill=" ")`、`padEnd(length, fill=" ")`、`join(items)`、`concat(text)`、`prepend(text)` |
| 拆分 | `split(separator, keepEmpty=true)`、`splitWhitespace()`、`lines(keepEmpty=true)` |
| Unicode 码点 | `charLength()`、`charAt(index)`、`utf8Substring(start, count=-1)`、`unicodeChars()`、`reverse()` |

`indexOf` 和 `lastIndexOf` 返回字节下标，未找到为 -1。负的正向起点按零处理；lastIndexOf 的负起点表示从末尾搜索。空搜索串在当前有效边界匹配；count 统计不重叠匹配，空搜索串的计数是字节数加一。compare 只返回 -1、0 或 1。

substring 的负起点按零处理，超出末尾返回空串，负长度表示余下全部。slice 使用半开区间 `[start,end)`，边界截断后结束不大于起点返回空串。left/right 的负长度返回空串。字节切片、插入、移除和填充可能在多字节字符中间截断；需要保证码点完整时使用 utf8Substring 或 unicodeChars。

byteAt、charAt 越界返回 None。charAt 返回包含一个完整码点的 String。Unicode 方法严格验证 UTF-8，拒绝过长编码、代理区码点、非法延续字节及不完整字符。reverse 保留单个码点的编码完整性，组合附加符与 emoji 组合序列仍按各自码点反转。

大小写转换及字符分类只处理 ASCII；trim、isBlank、splitWhitespace 的空白是空格和 `\t` 至 `\r`，行为不随宿主区域设置改变。空串 isBlank/isAscii 为 true，isDigit/isAlpha/isAlnum 为 false。

replace 与 replaceAll 相同，均替换不重叠匹配；空搜索串保持原值。repeat 的负次数、padStart/padEnd 的负目标长度报错；重复和连接在分配前检查结果长度溢出。填充按目标字节长度循环使用 fill，fill 为空时保持原值，目标不长于原字符串时保持原值。

split 默认保留中间、起始和末尾空项，空 separator 按严格 UTF-8 码点拆分。splitWhitespace 忽略连续空白产生的空项。lines 将 CRLF 视为一次换行，也接受独立 CR 与 LF，默认保留末尾空项。join 把当前字符串作为分隔符，空列表生成空字符串。已有 `chars()` 惰性迭代入口仍由 `std::iter` 提供；unicodeChars 直接返回独立 `List<String>`。

完整示例见[字符串方法](../example/samples/string_methods.chl)、[值语义](../example/samples/opcode_value_semantics.chl)及[红黑树删除](../example/samples/std_red_black_delete.chl)。
