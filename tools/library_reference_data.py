"""标准库参考手册的人工维护说明；声明与重载由 library_reference.py 从源码读取。"""

MODULES = {
    'std::cas': ('精确符号计算', 'Expr 从字符串解析为不可变 AST，使用共享大整数后端保存精确整数和有理数。支持四则运算、节点遍历、化简、求导、积分、替换、极限与 Taylor 展开；不支持的积分保留符号节点。调用串行保护 CAS 内部资源，AST 可以独立复制。'),
    'std::encoding': ('文本编码', 'RFC 4648 的 Base64、Base64URL 与十六进制编解码。输入字节使用 List<uint8>，解码返回 Result；拒绝非法字符、填充与非零尾部位，不忽略空白。'),
    'std::csv': ('CSV 读写', '支持自定义单字节分隔符、引号转义和字段内换行，保留空字段与空行。错误位置从一开始，列按字节计。输出记录以 LF 结束，空记录必须含至少一个空字段。'),
    'std::datetime': ('UTC 日期时间', 'Unix 秒与公历 UTC 日期换算，年份 1..9999，秒 0..59，不表示闰秒。ISO 8601 首版只接受 YYYY-MM-DDTHH:MM:SSZ。纯换算不依赖平台时区；nowUtc 要求宿主时钟。'),
    'std::cli': ('命令行参数', '长短选项、短选项组合、附着值、位置参数与 -- 分隔。重复选项采用最后一次值，required 必须显式出现。--help/-h 自动识别但不退出或打印，由应用处理。'),
    'std::utf8': ('UTF-8 与码点', 'RFC 3629 严格校验与码点解码，拒绝过长序列、代理项及超范围值。数量和截取按 Unicode 码点，不能代替字素簇处理。String 不适合保存内嵌零字节。'),
    'std::lang': ('核心库', '自动加载，无需 import。String 提供字节查找、切片、替换、修剪、填充、连接、分割、ASCII 大小写与分类，以及严格 UTF-8 码点操作；普通方法返回独立值，字节索引与码点索引明确区分，详情见字符串方法文档。Option/Result 通过 match 或 isValue/isOk 分支处理；unwrap/expect 在失败分支触发 panic。浮点转整数向零截断，范围检查见各重载；字节编码默认小端。拥有值按值复制，引用的生命周期不得超过来源。'),
    'std::collections': ('集合扩展', 'Deque、Queue、Channel 基于 List；Channel 是当前执行上下文的值队列，不是跨线程消息系统。FixedVector 与 RingBuffer 的容量固定，满时 push 返回 false；空时 pop/receive 返回 None。索引从 0 开始。'),
    'std::reflect': ('反射', '要求工具链构建时同时启用 CHALI_WITH_REFLECTION=1 与 STACKVM_WITH_REFLECTION=1，并在编译/运行时启用 --reflection。类型信息在具体类型和布局确定后生成；动态调用只支持 ReflectValue 的封闭载荷集合。Err 区分未找到、歧义、参数数量、类型、接收者和未暴露等错误。'),
    'std::algorithms': ('通用算法', '需 import std::algorithms; 并使用完整限定名。Value 族操作列表值；sortValue、shuffleValue 返回副本。Reference 族直接操作连续存储，调用者保证 count 对应有效元素数；返回的引用不能越过缓冲区生命周期。排序比较使用 >；默认稳定插入排序，构建时 SIMPLE_SORT 可切换到不稳定希尔排序。二分查找要求已升序排列。'),
    'std::image': ('图片处理', 'Image 是可深复制的 opcode，尺寸与连续 RGBA8888 像素存储由宿主记录拥有；width()/height() 返回尺寸，pixels() 返回独立列表快照。decode/open 支持无压缩 24/32 位 BMP 与二进制 P5/P6，按魔数自动识别；encodeBmp/encodePpm/encodePgm 与 saveBmp/savePpm/savePgm 生成对应文件字节或写盘。getPixel/setPixel 越界或分量超 0..255 返回错误，不 panic。卷积要求核边长为不超过 15 的奇数，边缘复制填充；池化窗口 0=最大 1=平均，输出尺寸向下取整。灰度采用 BT.601 亮度；ARGB8888 每像素一个 uint32，RGB565 每像素一个 uint16，还原时按高位复制扩展。大津法返回 0..255 阈值，二值化大于阈值给 255。重活由宿主胶水 glue/std_image.hpp 提供，裁剪宏 NO_CHALI_STD_IMAGE。'),
    'std::binary': ('二进制读取', 'BinaryReader 拥有输入的副本，默认小端。位置、长度均按字节；读取成功才推进游标。越界、负长度、元素数量乘法越界由宿主检查并报错。seek/skip 失败返回 false 且保留游标；文本字段在首个零字节停止显示但消费整个字段。'),
    'std::clock': ('单调时钟', '采用宿主 steady_clock。读数的起点由平台决定，只能用于差值与计时，不代表日期时间；单位分别为毫秒和纳秒，实际分辨率由平台决定。'),
    'std::complex': ('泛型复数', 'Complex<T> 保存 real/imag；T 应支持所用的四则运算。leftAdd/leftSub/leftMul/leftDiv 是运算符接口，支持 +、-、*、/。整数 T 的除法仍采用整数运算规则。'),
    'std::compress': ('无损压缩', 'compress 输入文本，输出二进制 List<uint8>；不能把压缩结果当 UTF-8 文本输出。decompress 对无效编码或损坏数据报告宿主运行错误，不返回 Result。'),
    'std::concurrency': ('线程通信与共享', 'Channel<T> 是线程组内的有界多生产者多消费者通道，容量 1..1048576；队列按值复制，关闭后排空再返回 CLOSED。句柄可复制，显式 close 负责关闭，运行结束统一释放；最多 1024 个未排空通道。Receive<T> 只有 READY 时 value 有效。状态为 READY=0、TIMEOUT=1、CLOSED=2、CANCELLED=3、FULL=4、FAILED=5、INVALID=6。millis=-1 无限、0 非阻塞、正数限时；超时不取消任务。Shared<T> 单次 get/set/swap 原子；AtomicInt64.fetchAdd 原子返回旧值，按 int64 位宽回绕。需 --threads；普通全局变量继续隔离。'),
    'std::event': ('显式事件与定时源', '事件只在 receive/receiveFor/tryReceive 显式接收时交付值，绝不自动执行用户函数。队列容量 256，post 在队列满时返回 FULL。定时源使用宿主单调纳秒时钟、创建后立即计时，最多 1024 个活动定时源；服务计算截止时间，不按定时源创建线程。周期通知合并，missed 表示错过的周期数。Timer.stop/drop 删除未接收通知；队列关闭后先排空，结束后的宿主弱引用投递器返回 CLOSED。事件操作不支持同步胶水重入。'),
    'std::io': ('文件与流接口', 'FileStream 返回 Result，失败载荷是 IoError；read/write/seek 之前检查打开结果。read 按底层文件读取一行。close 显式关闭句柄。Reader/Writer/Seek 是用户可实现的接口，接口本身不创建文件。路径与模式由宿主文件系统解释。'),
    'std::iter': ('惰性迭代器', 'next 消费游标，peek 缓存下一项而不丢弃。转换器惰性执行，collect/fold/count 等终结操作消费输入。sizeHint 返回下界和可选上界。iter/iterMut 引用源容器，intoIter 拥有值；借用有效性由借用检查器约束。repeat/cycle 可无限，须 take 等限制后才能收集。chars 当前按字节推进，不能把它当 Unicode 字素迭代器。'),
    'std::serialization': ('反射字节序列化', 'serialize<T> 使用反射生成精确 JSON 载荷，并封装版本、稳定类型 ID 和长度；deserialize<T> 严格检查后恢复独立拥有的值。编译器裁剪反射时 panic，模型未暴露时返回错误。'),
    'std::json': ('JSON 文档与反射编解码', 'parse 返回 Result<JsonValue,String>。节点由拥有型 opcode 保存独立子树；普通复制深复制，close 只关闭当前值，已取得的子值和副本继续有效，重复关闭返回 false。asXxx 返回 Option，不符合类型时为 None；asFloat64 可读取整数。含零字节的文本/键使用 asBytes/findBytes/keyBytesAt。kind：0 null、1 bool、2 int、3 float、4 string、5 array、6 object、7 date-time。'),
    'std::toml': ('TOML 文档与反射编解码', 'parse 返回 Result<TomlValue,String>。与 JSON 共用拥有型 opcode 协议；普通复制深复制子树，close 只影响当前值。表对应 isTable/isObject；时间值使用 isDateTime/asDateTime。asXxx 类型不符返回 None；含零字节的文本使用字节列表接口。kind 编码与 JSON 节点一致。解析文档根节点的 dump() 保留原文注释、空白与换行；反复解析保存不丢失或重复注释。底层 C++ 等形值编辑保留布局；结构编辑保留注释内容，但可调整位置。子节点只输出值。'),
    'std::linalg': ('向量与矩阵', 'Vector<T>、Matrix<T> 保留泛型聚合外观，values 使用 ScalarArray<T>，底层 ScalarStorage opcode 拥有连续数值缓冲，不再保存通用 List。支持内建有符号/无符号整数及 float32/float64，复制得到独立缓冲。下标从 0 开始；加减、点积和乘法必须维度兼容，非法维度触发断言或宿主错误。fromArray 复制，toArray 返回独立数组；裸引用重载须提供有效 count。normalized 不接受零向量。浮点分解和 solve 使用宿主胶水，返回分解结构/解向量；奇异、非法形状或不满足算法条件时报告运行错误，不返回 Result。'),
    'std::math': ('泛型数学', '三角函数使用弧度；普通整数返回 float64，float32 保持类型，float64 转发宿主数学库。BigInteger 转为 BigDecimal，BigDecimal 使用十进制高精度算法并沿用有效位数，Expr 组合符号 AST。pi(digits) 返回 1..100000 位圆周率。大十进制无 NaN/无穷，非法定义域报告错误；普通浮点沿用宿主特殊值。round 中点远离零，trunc 向零，fmod 余数与被除数同号。abs/min/max 保留既有泛型比较语义；clamp 检查 low<=high。'),
    'std::memory': ('内存视图与 arena', 'Span<T> 是引用加长度，不拥有缓冲区；iterator 返回元素 mut&。调用者保证底层连续存储有效。Arena 是不可复制的拥有型 opcode，显式 move 转移所有权；allocate 返回视图，reset/drop 后全部视图失效，不能继续解引用。Memory.stats 返回宿主分配器统计，不代表整个操作系统进程内存。'),
    'std::message': ('线程邮箱', '泛型线程消息按值复制，接收类型必须匹配；静态函数可传递，引用、独占资源、嵌套函数值和带捕获闭包不可传递。邮箱容量 256，sendThread 在满时等待；sendFor/trySend 和 receiveFor/tryReceive 返回统一操作状态。receiveThread 沿用阻塞和原始诊断；Receive<T> 仅在 READY 时读取 value。需 --threads。'),
    'std::net': ('TCP 与 HTTP', 'TcpSocket、TcpListener 是不可复制的 opcode，宿主记录独占连接，作用域退出自动关闭；所有权通过 move 转移，先检查 isOpen/lastError。receive/receiveLine 的空串需结合 status 区分关闭、超时和错误。端口 0 可让系统分配监听端口。HTTP 仅支持 http://，不支持 TLS；HTTP 状态码与传输错误分别检查。close 可提前关闭，重复调用安全；每条连接独立保存最近读状态与错误。connectAsync/pollConnect、acceptAsync、sendAsync、receiveAsync、receiveLineAsync 采用非阻塞轮询，未就绪返回 NET_PENDING；调用方可用 std::thread::sleepFor 有界暂停后重试。connectAsync 只接受数值 IPv4，默认五秒超时，域名使用同步 connect；sendAsync 返回本次部分发送字节数，调用方推进 offset；异步行缓冲上限 1 MiB，待完成数据保留至下次轮询。'),
    'std::numerics': ('数值分析', '回调应满足算法的连续性/可微性条件。实现已全部由 std::numerics 模块用 ChaLi 表达（原宿主胶水已删除），前置条件（导数步长、辛普森区间数、二分区间与容限、最大迭代次数、ODE 步数）在模块内校验并以 std::lang::panic 停机。二分需端点异号或端点为根；ODE 输出包含初值和后续 steps 个点。迭代次数按 int64 计数，不再收窄到宿主 int。数值结果是近似值，不保证任意模型收敛。'),
    'std::numtheory': ('整数数论', '实现已全部迁到 .chl，公开入口是模块级泛型自由函数（不再有 NumberTheory 静态方法包装，也没有给胶水用的宿主私有名）。gcd/lcm/addMod/mulMod/powMod/modInverse/isPrime 对任意整数类型 T 成立，含无符号与窄整型；gcd/lcm 接受任意数量实参，逐个折叠，结果非负。模运算要求 modulus>0；模乘通过逐位加法避免乘法中间溢出。isPrime 内部加宽到 uint64，使用七个底数的确定性 Miller-Rabin。modInverse 要求 modulus>1 且与 value 互素；不存在逆元时停机。lcm 溢出也停机。'),
    'std::parse': ('严格文本解析', '返回 Result<T,ParseError>，区分 Empty、Invalid、Overflow。不会静默返回零代替失败。整数的位宽与符号由具体函数确定；调用者处理 Err 后再取值。'),
    'std::path': ('路径字符串', '只进行路径字符串处理，不创建文件、不验证目标存在；分隔符与规范化遵循宿主实现。不要把字符串规范化当作文件权限或符号链接安全校验。'),
    'std::probe': ('最小宿主集成探针', '用于验证模块导入和调用链，twice 执行整数乘二，遵循 int64 运算语义。'),
    'std::process': ('宿主进程', '命令交给宿主 shell；processRun 创建读取管道，processRead 读取输出，processClose 关闭并等待。system 同步执行命令，返回宿主状态码。程序自行构造和信任 command 的内容；不将外部输入直接拼入 shell 命令。'),
    'std::random': ('随机数', '固定种子采用确定性 LCG，可复现；不用于密码学。nextInt/nextRange 区间为 [minimum,maximum)，要求 maximum>minimum；nextFloat64 为 [0,1)。Random 是拥有序列状态的 opcode，普通复制保留当前位置并独立推进，作用域退出自动释放。'),
    'std::regex': ('正则表达式', 'Regex 是 opcode；复制共享不可变编译模式，每次匹配使用独立状态，最后一个值释放时回收模式。compile 失败返回 RegexError::Invalid。find 返回首个匹配的字节下标或 None。capture 的 group=0 表示完整匹配。语法和 replacement 捕获组替换规则采用宿主 std::regex 的默认 ECMAScript 规则。'),
    'std::signal': ('复数信号变换', 'DFT 接受一般长度，FFT 要求长度为 2 的幂；非法长度由宿主检查。正变换采用负指数，逆变换归一化。输入输出为 Complex<float64> 列表，结果拥有独立存储。DFT 为 O(n²)，基 2 FFT 为 O(n log n)。'),
    'std::sql': ('数据库', 'SQLite 静态内置，无需 sqlite3.dll，忽略 SQLITE_LIBRARY。MySQL/MariaDB 使用 MYSQL_LIBRARY；PostgreSQL 使用 POSTGRES_LIBRARY；ODBC 需要相应驱动。mysql/maria 默认端口 3306，connect 已移除。exec 返回最后一条语句的结果，失败检查 ok/error；结果已复制，不借用客户端缓冲区。事务由 SQL BEGIN/COMMIT/ROLLBACK 控制。拼字符串字面量使用 db.quote；它不转义表名/列名。目标宿主必须提供相应数据库驱动。'),
    'std::tree': ('树与堆', 'BinarySearchTree 是集合语义的二叉搜索树，重复插入返回 false，中序即升序；删除双孩子节点取中序后继替换，被删节点槽回收复用。RedBlackTree 使用宿主持久红黑节点，insert/remove 原地旋转及变色修复；重复插入或删除不存在的键返回 false，validate 校验排序、父指针、根颜色、红节点约束及黑高，删除节点槽回收复用。MinHeap 是最小二叉堆，可当优先队列用，相等键的出堆顺序不保证插入序。GeneralTree 用"第一个孩子 / 下一个兄弟"存储，toBinary 与 fromBinary 按"左孩子 = 第一个孩子、右孩子 = 下一个兄弟"互转，转换本身不排序、不保证 BST 有序性。树高按节点数计（单节点 1，空树 0）；树与堆由 opcode 独立拥有持久状态，普通复制深复制；snapshot 显式导出独立节点或堆数组，遍历只导出结果。裁剪宏 NO_CHALI_STD_TREE。'),
    'std::graph': ('图论与并查集', 'Graph 使用链式前向星存储：head 是每顶点首条边，edgeTo/edgeNext/edgeWeight 是边终点、同起点下一条边与边权，顶点编号 0..vertexCount-1，重边与自环都允许。有向 / 无向由 create 的 directed 决定，无向图加边时自动镜像登记。bfs/dfs 输出可达顶点的访问序；topologicalSort 用 Kahn 算法，有环时输出空表；dijkstra 要求边权非负，不可达顶点距离为 -1；hasCycle 有向图用 Kahn 计数、无向图用并查集；minimumSpanningTreeWeight 用 Kruskal，图不连通返回 -1。Graph 与 UnionFind 使用独立拥有型 opcode，普通复制深复制；Graph.snapshot 和 UnionFind.parents/sizes 显式导出独立快照。UnionFind 路径压缩与按大小合并均原地执行，不随每次查询复制容量数组。全部算法由宿主胶水 glue/std_graph.hpp 提供，裁剪宏 NO_CHALI_STD_GRAPH。'),
    'std::linkedlist': ('双向链表', 'LinkedList 以私有拥有型 opcode 保存持久双向节点及游标索引，保留 Iterator<int64>。普通复制深复制节点并在副本内保留对应游标。首尾及按游标插删为平均 O(1)；按值查询和按序迭代 O(n)。游标不重复使用，删除或 clear 后永久失效；toArray 显式导出独立顺序快照。裁剪宏 NO_CHALI_STD_LINKEDLIST。'),
    'std::text': ('UTF-8 文本工具', '长度、位置和填充宽度按 UTF-8 字节，不按字符数或显示宽度。大小写转换仅 ASCII。除 charAt 返回字节外，变换返回新字符串。字符串下标/切片不允许从多字节编码中间开始；处理中文要保持 UTF-8 边界。kmpFind/kmpFindAll/kmpFailureTable 是字节级 KMP：kmpFind 返回首个匹配的字节下标，找不到为 -1，空针返回 0（与 find 口径一致）；kmpFindAll 返回升序命中下标、允许重叠匹配，空针给空列表；kmpFailureTable 返回长度等于针长的失配表。KMP 实现在宿主胶水 glue/std_kmp.hpp，裁剪宏 NO_CHALI_STD_KMP。'),
    'std::thread': ('隔离线程', '公开线程类型为 Thread，模块 std::thread；运行时需 --threads。create 使用无参静态入口，createWith 在创建时复制初始参数。wait/waitFor 传播普通失败原始诊断；waitResult/For 收集状态、error 与 trace，普通失败可被调用方处理；panic 始终终止本次 VM 运行及全部隔离线程。成功收集只允许一次，超时不消费句柄。requestStop 在 VM 安全点及可取消等待处终止任务，释放 VM 持有的宿主记录和堆；不在取消途中调用用户 Drop 代码，业务收尾应在请求取消前通过消息完成。阻塞宿主调用须由宿主自行支持超时/取消，VM 无法抢占正在执行的 C++ 函数。主程序结束请求停止并回收全部子任务。ThreadGroup 等待时监测失败并取消兄弟任务，离开作用域取消并回收。ThreadPool 使用固定线程和有界静态任务队列，closeAndWait 排空后回收，cancelAll 取消工作线程并丢弃未开始任务；长期阻塞 I/O 使用专用线程。ThreadGroup、ThreadPool 和 Timer 不克隆底层任务或定时源，使用引用或 move 维持单一收尾责任，避免复制拥有者。'),
    'std::watchdog': ('软重启看门狗', '默认关闭。enable 设置内存水位字节数；requestRestart 请求 VM 安全点结束本轮，由宿主重置 arena 与分配身份后重跑。enableTimeout 的超时由 VM 内部截止时间检查，已注入时钟时单位为纳秒，否则为逻辑指令数；kick 推迟截止时间，disableTimeout 关闭超时。不依赖事件回调；不能抢占正在执行的阻塞宿主函数。'),
}

# 精确名称优先，其后由生成器读取源码注释或按同族接口补全。
BEHAVIOR = {
    'rangeEq':'构造包含起点和终点的值区间；整数和 char 直接计算，其他类型通过 NextValue<T> 迭代。match 区间模式仅支持整数和 char。',
    'nextValue':'使用接收者的游标状态、end 与 inclusive 产生下一项，并推进游标；返回 None 结束区间。',

    'sin':'计算正弦。', 'cos':'计算余弦。', 'tan':'计算正切。', 'asin':'计算反正弦，结果为弧度。', 'acos':'计算反余弦，结果为弧度。', 'atan':'计算反正切，结果为弧度。',
    'exp':'计算 e 的 value 次幂。', 'ln':'计算自然对数。', 'log10':'计算以 10 为底的对数。', 'pow':'计算 value 的 exponent 次幂。', 'sqrt':'计算平方根。', 'abs':'返回绝对值；泛型入口，整数最小值取负按位宽回绕，NaN 与负数零原样返回。', 'floor':'向负无穷取整。', 'ceil':'向正无穷取整。', 'round':'舍入到最近整数，中点远离零。', 'trunc':'向零截断小数部分。', 'fmod':'计算浮点余数，结果符号与被除数一致。', 'isNaN':'判断是否为 NaN。', 'isInf':'判断是否为正无穷或负无穷。', 'min':'返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。', 'max':'返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。', 'clamp':'将 value 限制在闭区间 [low,high]；low>high 时报运行错误。',
    'gcd':'计算实参的最大公约数；泛型入口，接受任意数量实参，结果非负。', 'lcm':'计算实参的最小公倍数；泛型入口，接受任意数量实参，任一实参为零给零，乘积回绕时报运行错误。', 'addMod':'计算 (left+right) mod modulus；modulus 不为正时报运行错误。', 'mulMod':'计算 (left×right) mod modulus，避免直接乘法的中间溢出；modulus 不为正时报运行错误。', 'powMod':'通过快速幂计算 base^exponent mod modulus；modulus 不为正时报运行错误。', 'isPrime':'判断整数是否为素数；内部加宽到 uint64 做七个底数的确定性 Miller-Rabin。', 'modInverse':'计算模乘逆元；模数不大于 1 或不互素时报运行错误。',
    'nowMs':'读取单调时钟毫秒计数。', 'nowNanos':'读取单调时钟纳秒计数。', 'twice':'返回 x×2。',
    'withSeed':'用固定 seed 创建随机数状态。', 'fromEntropy':'用宿主熵源播种随机数状态。', 'nextUInt64':'推进生成器并返回 64 位随机值。', 'nextInt':'返回 [minimum,maximum) 内的随机整数。', 'nextFloat64':'返回 [0,1) 内的随机浮点数。', 'nextRange':'返回 [minimum,maximum) 内的随机浮点数。', 'nextBool':'推进生成器并返回随机布尔值。',
    'isValue':'判断 Option 是否含 Value。', 'isNone':'判断 Option 是否为 None。', 'isOk':'判断 Result 是否为 Ok。', 'isErr':'判断 Result 是否为 Err。', 'unwrap':'提取成功载荷；None/Err 时 panic。', 'unwrapOr':'提取成功载荷；None/Err 时返回 fallback。', 'expect':'提取成功载荷；None/Err 时以 message 触发 panic。', 'error':'提取 Err 载荷；Ok 时 panic。', 'transform':'成功时调用 mapper 转换载荷，失败分支保持不变。', 'andThen':'成功时调用返回同族 Option/Result 的 mapper，将结果展平。',
    'copy':'返回独立的值副本。', 'len':'返回当前元素数量。', 'isEmpty':'判断是否没有元素。', 'clear':'移除全部元素。', 'index':'读取 key 对应的元素，供下标语法调用。', 'iterator':'读取指定迭代位置；无元素时返回 None。', 'at':'读取从 0 开始的指定位置。', 'set':'将指定位置或共享槽替换为 value。', 'get':'读取共享槽当前值的副本。', 'swap':'原子替换共享槽并返回旧值。',
    'pushBack':'将 value 追加到尾部。', 'pushFront':'将 value 插入首部。', 'popBack':'移除尾部并返回 Option；空时 None。', 'popFront':'移除首部并返回 Option；空时 None。', 'peek':'查看下一项并保留该项供后续消费。', 'capacityValue':'返回固定容量。', 'available':'返回剩余可写容量。', 'withCapacity':'用指定 capacity 创建有界容器。',
    'fromBytes':'复制 bytes 为读取器自有存储。', 'position':'返回当前字节游标。', 'remaining':'返回游标后尚未消费的字节数。', 'size':'返回存储的总长度。', 'setBigEndian':'设置后续读取的大端/小端顺序。', 'skip':'前进 count 字节；失败返回 false 且游标不变。',
    'parseInt64':'严格解析有符号 64 位十进制整数。', 'parseUInt64':'严格解析无符号 64 位十进制整数。', 'parseFloat64':'解析 64 位浮点数并报告非法输入/溢出。', 'parseBool':'解析布尔文本并报告非法输入。',
    'find':'查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。', 'rfind':'返回子串最后出现的字节下标；找不到为 -1。', 'contains':'判断是否包含目标值或匹配文本。', 'startsWith':'判断文本是否以 prefix 开始。', 'endsWith':'判断文本是否以 suffix 结束。', 'compare':'按字典序比较，返回 -1、0 或 1。', 'toUpper':'将 ASCII 小写字母转换成大写。', 'toLower':'将 ASCII 大写字母转换成小写。', 'trim':'去掉两端 ASCII 空白。', 'ltrim':'去掉左端 ASCII 空白。', 'rtrim':'去掉右端 ASCII 空白。', 'replace':'将所有 from 子串替换为 to；空 from 保留副本。', 'repeat':'重复给定值或文本；文本次数非正时为空串，迭代器版本为无限源。', 'padStart':'在左侧以 fill 补到 width 字节。', 'padEnd':'在右侧以 fill 补到 width 字节。', 'charAt':'读取 index 位置的字节，需处于有效 UTF-8 边界。', 'split':'按 separator 分割，空分隔符把整串作为一项。', 'join':'用 separator 连接 parts。',
    'extension':'返回路径的扩展名。', 'fileName':'返回路径末尾文件名。', 'normalize':'返回宿主规则下的规范化路径字符串。',
    'fullMatch':'判断整个 text 是否匹配模式。', 'compile':'编译正则模式；失败返回 Err(Invalid)。', 'capture':'提取首次匹配的 group 捕获组。', 'replaceFirst':'替换首次正则匹配。', 'replaceAll':'替换全部正则匹配。',
    'isValid':'判断共享文档是否仍有效。', 'kind':'返回节点类型编号。', 'isNull':'判断是否为 null/NULL。', 'isBool':'判断是否为布尔节点。', 'isInt':'判断是否为整数节点。', 'isFloat':'判断是否为浮点节点。', 'isString':'判断是否为字符串节点。', 'isArray':'判断是否为数组节点。', 'isObject':'判断是否为对象/表节点。', 'isTable':'判断是否为表/对象节点。', 'isDateTime':'判断是否为日期时间节点。', 'asBool':'以 Option/Result 提取布尔值，类型不符走失败分支。', 'asInt':'提取整数值；具体转换规则见所属类型。', 'asFloat64':'提取浮点值，也接受整数节点。', 'asString':'提取不含零字节的字符串；否则 None。', 'asBytes':'提取字符串的原始字节，包括零字节。', 'asDateTime':'提取日期时间的文本表示。', 'findBytes':'按原始字节键查找成员，失败 None。', 'keyAt':'按成员位置读取文本键；不适用时 None。', 'keyBytesAt':'按成员位置读取原始字节键。', 'dump':'将节点序列化为文本。',
    'zeros':'创建指定大小并以 T 的默认值填充。', 'fromArray':'从连续数组复制元素；带 count 的重载由调用者保证有效范围。', 'toArray':'复制成由调用者作用域管理的独立运行时数组。', 'dot':'计算等长向量的点积。', 'norm':'返回向量欧氏范数。', 'normSquared':'返回实部平方加虚部平方。', 'normalized':'返回单位向量；零向量报错。', 'add':'计算逐项相加或追加元素，具体操作取决于所属类型。', 'subtract':'返回等维数据逐项相减的结果。', 'scale':'返回各分量乘以 factor 的新值。', 'leftAdd':'实现左操作数加法接口。', 'leftSub':'实现左操作数减法接口。', 'leftMul':'实现左操作数乘法接口。', 'leftDiv':'实现左操作数除法接口。', 'identity':'创建单位矩阵。', 'column':'复制指定列为向量。', 'transpose':'返回转置矩阵。', 'multiply':'进行矩阵乘法。', 'apply':'计算矩阵与向量的乘积。', 'solve':'解线性方程组，返回独立解向量；奇异或维度不符时报错。', 'conjugate':'返回虚部取反的共轭复数。',
    'luDecompose':'计算带主元置换的 LU 分解。', 'lurDecompose':'计算 LUR 分解结果。', 'symmetricEigen':'计算实对称矩阵的特征分解。', 'svd':'计算奇异值分解。', 'jordan2':'计算 2×2 矩阵的 Jordan 分解。', 'diagonalize2':'将可对角化的实数 2×2 矩阵对角化；不满足条件时报错。',
    'derivative':'用中心差分近似 function 在 x 的导数。', 'integrateSimpson':'用复合 Simpson 法近似定积分。', 'bisect':'在括根区间内二分求根。', 'rk4Step':'执行一次四阶 Runge-Kutta 更新。', 'newton':'从 initial 开始迭代 Newton 求根。', 'solveOdeEuler':'用 Euler 法求解标量初值问题并返回采样列表。', 'solveOdeRK4':'用四阶 RK 法求解标量初值问题并返回采样列表。',
    'dft':'计算离散傅里叶正变换。', 'idft':'计算归一化离散傅里叶逆变换。', 'fft':'计算基 2 快速傅里叶正变换。', 'ifft':'计算归一化基 2 快速傅里叶逆变换。',
    'stats':'返回当前分配器的活动字节、活动块及峰值统计。', 'allocate':'在 arena 中分配 bytes 字节，返回 Span<char>。',
    'processRun':'通过 shell 启动读取管道，返回进程句柄。', 'processRead':'读取进程管道的输出文本。', 'processClose':'关闭管道并等待进程，返回宿主状态码。', 'system':'同步执行 shell 命令并返回宿主状态码。',
    'waitFor':'最多等待 millis 毫秒，完成并回收目标返回 true，超时返回 false；超时不取消目标，目标失败传播原始诊断。', 'tryWait':'不阻塞地尝试等待并回收目标，尚未完成返回 false；目标失败传播原始诊断。', 'current':'返回当前线程的 Thread 句柄。', 'parent':'返回创建当前任务的父线程 Thread 句柄。', 'wait':'等待并回收目标线程，失败时保留原始诊断。', 'sendThread':'向指定线程投递 T 类型的值副本。', 'receiveThread':'等待当前线程的一条 T 类型消息。',
    'everyNanos':'创建纳秒周期的停止态定时器。', 'everyMillis':'创建毫秒周期的停止态定时器。', 'start':'启动定时器。', 'stop':'停止定时器，保留句柄。', 'enable':'启用软重启看门狗并设置水位。', 'requestRestart':'请求宿主在安全点结束本轮并处理软重启。',
    'mysql':'连接 MySQL，返回带打开状态及错误信息的 Database。', 'maria':'连接 MariaDB，驱动标识为 mariadb，客户端 ABI 与 MySQL 兼容。', 'local':'使用本机默认 MySQL 配置连接 database。', 'sqlite':'打开文件 SQLite 或 :memory: 内存库。', 'postgres':'使用 libpq 连接串/URI 打开 PostgreSQL。', 'sqlServer':'使用 ODBC 连接选项打开 SQL Server。', 'odbc':'使用 Unicode ODBC 连接选项打开数据源。', 'exec':'执行 SQL，复制最后一条语句的结果并返回 ResultSet。', 'execOk':'执行 SQL 并仅返回是否成功。', 'scalar':'读取结果首行首列的文本表示。', 'count':'读取结果首行首列的整数表示。', 'hasTable':'检查指定表是否存在。', 'affectedRows':'返回连接最近操作的影响行数。', 'backendName':'返回驱动标识字符串。', 'lastError':'返回连接/网络操作最近的错误文本。', 'isOpen':'判断连接或监听器是否已打开。', 'isNumber':'判断单元格是否为整数或实数。', 'asReal':'将单元格转换为实数。', 'text':'返回单元格的文本表示。', 'rowCount':'返回结果行数。', 'colCount':'返回结果列数。', 'columnName':'按列下标读取列名。', 'columnIndex':'按列名查找列下标。', 'intAt':'读取指定单元格的整数表示。', 'realAt':'读取指定单元格的实数表示。', 'textAt':'读取指定单元格的文本表示。', 'render':'将结果集渲染为文本表格。', 'pad':'按显示宽度补齐单元格文本。', 'displayWidth':'估算文本表格显示宽度；ASCII/双字节按 1，三/四字节编码按 2。', 'intLiteral':'生成整数 SQL 字面量。', 'realLiteral':'生成实数 SQL 字面量。', 'quote':'生成带单引号的 SQL 字符串字面量，按驱动处理反斜杠。',
    'listen':'绑定本机地址并开始监听，返回监听器。', 'accept':'接受一条 TCP 连接，可指定等待超时。', 'localPort':'返回实际绑定的本地端口。', 'status':'返回最近一次网络读取状态码。', 'receiveLine':'读取一行网络文本；空串时检查 status。', 'connect':'连接远端 TCP/HTTP 服务。', 'fetch':'发出 HTTP GET 并返回响应。', 'submit':'提交 HTTP 请求体并返回响应。', 'exchange':'发送构造后的 HTTP 请求并解析响应。', 'readAll':'持续读取 HTTP 响应原始文本。', 'header':'按名称查找响应头。', 'isSuccess':'判断 HTTP 状态码是否位于 200..299。', 'httpScheme':'返回 URL 的协议部分。', 'parseUrl':'拆分 HTTP URL 的主机、端口和请求路径。', 'buildRequest':'构造 HTTP 请求报文。', 'parseResponse':'解析原始 HTTP 响应报文。',
    'reset':'清空对象的历史状态或分配状态，具体资源规则见模块约定。', 'predict':'按运动模型执行预测更新。', 'correct':'根据观测执行校正更新。', 'pose':'读取当前二维位姿副本。', 'addPolar':'将测距/角度按 origin 位姿变换到世界坐标并追加。', 'centroid':'计算点集质心。', 'nearestDistance':'返回查询点到点集的最小距离。', 'transformed':'按 pose 刚体变换并返回新点云。', 'voxelDownsample':'按 cellSize 网格降采样并返回新点云。', 'radiusOutlierFilter':'保留 radius 内满足邻居数量要求的点，返回新点云。',
    'serialize':'按具体反射类型编码独立值，失败返回诊断。', 'deserialize':'按具体反射类型严格校验并恢复独立拥有的值。', 'requireReflection':'检查编译器是否编入反射；缺失时 panic。', 'getField':'按字段名读取精确类型的独立值。', 'setField':'按字段名写入精确类型的值。', 'query':'从模型注解与反射元数据建立 ipage 查询。', 'filter':'为动态 SQL 添加参数化单值条件。', 'filterMany':'为动态 SQL 添加多参数或空值条件。', 'build':'生成带参数列表的动态 SQL。', 'page':'设置查询分页条数及偏移。', 'update':'按反射模型值生成参数化更新。', 'typeId':'返回 ReflectValue 当前载荷的精确类型 ID。', 'getTypeId':'取得具体 T 的类型 ID。', 'getInterfaceId':'取得接口 I 的类型 ID。', 'getStructInfo':'读取具体结构体的布局和成员元数据。', 'getEnumInfo':'读取枚举变体及成员元数据。', 'getInterfaceInfo':'读取接口方法及实现关系元数据。', 'getFuncInfo':'按名称查找函数元数据，未找到时 None。', 'getAnnotationInfo':'按名称查找注解定义元数据。', 'implementsInterface':'判断 T 是否实现给定接口 ID。', 'getAnnotations':'取得类型上的注解列表。', 'getFieldAnnotations':'按字段名取得字段注解。', 'getFuncAnnotations':'按函数名取得函数注解。', 'getMethodAnnotations':'按方法名取得方法注解。', 'getStaticMemberAnnotations':'按静态成员名取得注解。', 'call':'按名称动态调用函数，返回封闭值或 ReflectError。', 'callMethod':'通过可写接收者动态调用实例方法。', 'callMethodConst':'通过只读接收者动态调用允许的实例方法。', 'callStatic':'动态调用 T 的静态方法。', 'callEnumMethod':'通过可写枚举接收者动态调用方法。', 'callEnumMethodConst':'通过只读枚举接收者动态调用方法。', 'callEnumStatic':'动态调用枚举的静态方法。', 'construct':'动态调用 T 的构造函数，返回 T 或错误。', 'callInterface':'通过可写接口接收者动态调用方法。', 'callInterfaceConst':'通过只读接口接收者动态调用方法。',
}

ITER = {
    'fromIterator':'复制实现 Iterator<T> 的来源值，建立可组合的独立惰性游标；不会消费来源变量。',

    'next':'消费并返回下一项，当前无项时 None。', 'sizeHint':'返回剩余数量下界及可选上界；未知上界为 None。', 'range':'构造半开整数区间 [start,stop) 的惰性迭代器，步长不能为零。', 'fromRange':'将核心整数 range 转为通用 Iter。', 'fromState':'复制实现 IterNext<T> 的状态值到专有存储，返回 Iter<T>。', 'fromList':'用列表值构造迭代器。', 'listIntoIter':'取得拥有列表值的迭代器。', 'listIter':'取得只读元素引用的迭代器。', 'listIterMut':'取得可写元素引用的迭代器。', 'setIntoIter':'取得集合值的迭代器。', 'setIter':'借用集合构造迭代器：把元素快照进迭代器状态，来源集合保持不变；迭代器本身仍是一次性消费。', 'mapIntoIter':'取得映射键值对的迭代器。', 'mapIter':'借用映射构造键值对元组迭代器：把键值对快照进迭代器状态，来源映射保持不变；迭代器本身仍是一次性消费。', 'fromSpan':'从连续存储视图构造迭代器。', 'map':'惰性调用 mapper，将 T 转成 U。', 'filter':'惰性保留 predicate 为真的项。', 'collect':'消费迭代器，收集到独立 List。', 'fold':'从 initial 开始按顺序累积。', 'tryFold':'累积到首个 Err 时短路返回，否则 Ok。', 'filterMap':'调用返回 Option 的 mapper，只输出 Value。', 'mapWhile':'转换元素，遇到首个 None 时结束。', 'scan':'以可变状态调用回调，产生 Option 输出。', 'inspect':'遍历时先执行观察回调，再输出原项。', 'enumerate':'给每项附加从 0 开始的序号。', 'zip':'将两个源配对，较短源结束时终止。', 'chain':'先输出第一源，再输出第二源。', 'rev':'产生逆序结果；实现会缓冲输入，不能用于无限源。', 'cycle':'循环输出源的缓冲副本；空源保持为空。', 'skip':'丢弃前 count 项。', 'take':'最多输出前 count 项。', 'skipWhile':'在前缀谓词为真期间丢弃，之后输出全部剩余项。', 'takeWhile':'输出谓词为真的前缀，消费使谓词失败的那一项。', 'peekingTakeWhile':'分离满足谓词的前缀与剩余迭代器，失败项保留在剩余部分。', 'stepBy':'按指定步长抽取元素，步长必须为正。', 'fuse':'一旦 next 返回 None，后续始终 None。', 'nth':'消费到从 0 开始的第 index 项，返回 Option。', 'last':'消费全部剩余项并返回最后一项。', 'find':'消费到首个 predicate 为真的项并返回 Option。', 'findMap':'转换并返回首个 Value。', 'all':'消费并判断是否全部满足谓词，空源为 true。', 'any':'消费并判断是否存在满足谓词的项，空源为 false。', 'count':'消费并返回项数。', 'countIter':'消费并返回项数。', 'forEach':'逐项调用 action 并消费迭代器。', 'sameItems':'逐项比较两个源的值及长度是否一致。', 'chars':'按字符串字节构造 char 迭代器，非 Unicode 字素切分。', 'repeat':'构造无限重复 value 的源，通常搭配 take。',
}

PARAMETERS = dict(line='用于诊断的源码行号', value='输入值或写入的新值', values='参与操作的元素序列', text='输入文本，位置与长度通常按字节', bytes='输入或输出字节存储', bigEndian='true 为大端，false 为小端', count='元素或字节数量，具体单位见行为说明', length='有效长度', index='从 0 开始的位置', position='目标绝对位置', key='查找键或下标', data='连续存储的首元素引用，调用者保证范围有效', condition='待验证的布尔条件', message='诊断文本或消息载荷', fallback='失败/空值分支的替代值', mapper='转换回调，签名决定输入和输出类型', predicate='判断回调，返回 bool', action='逐项执行的回调', source='输入游标、存储或事件来源值', state='初始状态值', left='左操作数或区间左端', right='右操作数或区间右端', low='闭区间下界', high='闭区间上界', start='起始值', stop='不包含的终止值', step='步长或时间增量', initial='迭代/累积初值', begin='积分起点', end='积分终点', seed='随机种子', handle='本模块创建且仍有效的宿主句柄', minimum='包含的下界', maximum='不包含的上界', base='幂底数', exponent='指数', modulus='模数', function='数值算法回调', x='输入横坐标/标量', y='输入纵坐标', tolerance='收敛误差容限', maxIterations='最大迭代次数', intervals='积分子区间数', derivative='导数或变化率回调', time='当前时间，秒', initialTime='初始时间', initialValue='初始状态值', rate='变化率回调', steps='更新步数', offset='字节偏移或 seek 相对位移', origin='定位基准或坐标原点', path='文件/数据库路径', mode='宿主文件打开模式', command='交给宿主 shell 的命令文本', process='进程管道句柄', parts='待拼接文本列表', separator='分隔文本', needle='待查找子串', prefix='前缀', suffix='后缀', width='目标字节宽度或定宽字段大小', fill='填充字节', pattern='正则模式', replacement='正则替换文本', group='捕获组编号，0 为完整匹配', host='主机名或 IP 地址', port='端口号', timeoutMs='超时时间，毫秒', backlog='监听待接受队列长度', maxBytes='最多接收字节数', url='HTTP URL，仅支持 http://', body='HTTP 正文', headers='HTTP 头部信息', method='HTTP 方法', raw='原始报文文本', name='名称', hostName='主机名', user='数据库用户名', password='数据库连接密码', database='数据库名', options='后端连接选项/连接串', sql='SQL 语句文本，可包含多条语句', table='表名', row='从 0 开始的行下标', col='从 0 开始的列下标', column='列下标或列名', escapeBackslash='是否对反斜杠进行 MySQL 风格转义', capacity='容器的最大元素数量', factor='缩放因子', other='另一操作数', n='向量长度或矩阵阶数', rows='矩阵行数', cols='矩阵列数', rhs='线性方程右端', enabled='是否启用该选项', watermarkBytes='内存水位字节数，0 不按水位判断', callback='事件或任务回调', handler='同步处理函数', period='定时器周期', timer='事件/定时器句柄', thread='线程句柄或归属线程', task='无参数任务函数', arguments='回调参数值', milliseconds='毫秒数', angle='角度，弧度', alpha='平滑系数', sample='新采样值', velocity='速度', dt='时间增量，秒', variance='方差', processVariance='过程噪声方差', measurementVariance='测量噪声方差', gyroWeight='陀螺仪融合权重', gyroRate='角速度，弧度/秒', observedAngle='观测角，弧度', kp='比例增益', ki='积分增益', kd='微分增益', outputLimit='输出限幅', integralLimit='积分限幅', target='目标值', measured='测量值', wheels='四轮位移', trackWidth='轮距，米', leverArm='底盘几何臂长，米', forwardSpeed='前向速度', steeringAngle='转向角，弧度', wheelbase='轴距，米', pose='二维位姿', motion='车体速度', yaw='航向角，弧度', forward='前向位移', lateral='侧向位移', deltaYaw='航向角增量，弧度', positionVariance='位置过程噪声方差', yawVariance='角度过程噪声方差', axis='观测轴：0=x，1=y，2=yaw', measurement='观测值', range='距离，米', cellSize='体素边长，米', radius='邻域半径，米', minimumNeighbors='保留点所需的最小邻居数量', interfaceId='接口类型 ID', args='与目标参数数量、位宽和类型匹配的封闭反射值列表', obj='实例接收者，其引用可写性须匹配方法', style='格式化风格', first='第一分量', second='第二分量')

BEHAVIOR.update({
    'assert':'condition 为真时继续，为假时以指定诊断文本和可选行号触发 panic。',
    'range':'创建半开整数区间；正步长小于 stop，负步长大于 stop，步长不能为零。',
    'push':'将 value 追加到列表尾部。', 'insert':'在 index 之前插入 value。',
    'pop':'移除列表尾元素并返回值，空列表时报错。', 'remove':'删除指定位置或键的元素。',
    'indexOf':'返回首次出现的下标；未找到返回 -1。', 'has':'检查是否包含指定值或键。',
    'put':'写入 key 对应的 value，已有键被替换。', 'keyAt':'按遍历位置读取键。',
    'valAt':'按遍历位置读取值。', 'str':'生成大数的十进制文本。',
    'setPrecision':'设置此 BigDecimal 的十进制精度；要求有效正精度，操作修改当前值。',
    'isEmpty':'判断 Any 是否为空盒子，不公开内部执行期标签。',
    'getValue':'按接收上下文要求的类型取出拥有值，类型不匹配时报错。',
    'getReference':'按上下文要求的类型借用盒子内的值，引用受盒子生命周期约束。',
    'getOptional':'按上下文要求的类型获取可选引用，类型不匹配为 None。',
    'decompress':'将有效 lztext 二进制数据还原为文本；损坏时报运行错误。',
    'toString':'由实现者将当前值转换为文本，Formattable 重载接收 style。',
    'fileRead':'从已打开文件读取文本行。', 'fileWrite':'向文件写入文本并返回宿主状态/计数。',
    'fileEof':'检查文件是否到达 EOF。', 'fileClose':'关闭原生文件句柄并返回宿主状态码。',
    'from':'用首元素引用和长度创建非拥有 Span 视图。',
})
PARAMETERS.update(dict(amount='需要跳过或取得的元素数量',contentType='HTTP 正文媒体类型',destination='可写目标存储首地址',digits='十进制有效数字精度',divisor='除数',folder='按签名接收累积值与当前元素的折叠回调',format='格式模板，使用 {} 占位符',from_='被替换子串',i='从 0 开始的元素下标',imaginary='复数虚部',input='输入矩阵、信号或数据值',items='参与处理的列表元素',millis='毫秒数',real='复数实部',rest='后续变长实参',tableName='数据库表名',to='替换文本',vector='输入向量'))
PARAMETERS['from']='被替换的子串'
PARAMETERS.update(dict(a='第一个元素或顶点下标', b='第二个元素或顶点下标', reply='胶水回执结构，调用后槽位被写回'))

BEHAVIOR.update(dict(fileOpen='按路径和模式打开文件，返回 Result<File,String>，失败为 Err。',hash='返回输入值的哈希值，由实现者保持与相等性的一致性。',explicitConvert='由用户类型提供显式目标类型转换。',implicitConvert='由用户类型提供隐式目标类型转换。'))

# 补充经宿主实现核对的边界，避免把错误误写为 Result/默认返回值。
for key,extra in {
 'std::numerics':' 二分要求 left<right、tolerance>0、maxIterations>0；导数与 rk4Step 要求 step>0；Simpson 要求 intervals 为正偶数；ODE 要求 steps>=0。',
 'std::signal':' FFT/IFFT 的长度必须为正的 2 的幂。',
 'std::sql':' DataCell.asInt/asReal 只转换数值类型，其他类型返回 0/0.0；text 对 NULL 返回 "NULL"。ResultSet.scalar/count 在空结果上返回空串/0，at/intAt/realAt/textAt 的行列索引必须有效。',
 'std::lang':' List.pop 空列表报错，列表/集合/映射的按位置访问需有效下标。Set/Map 的 has/contains 返回 int64 的 0/1；List 的同名接口返回 bool。Any.getReference/getOptional 的 const/mut 由接收上下文决定，返回引用受盒子寿命限制。BigDecimal 精度范围 1..100000。内存内建接受有效存储引用，字节计数不是元素数量。$assert(condition) 和 $assert(condition,message) 为宏调用，自动添加调用点行号，宏体自带分号。',
}.items():
 title,text=MODULES[key];MODULES[key]=(title,text+extra)
BEHAVIOR['zipValue']='按较短列表长度配对元素并返回元组列表，较长列表的多余元素忽略。'

PARAMETERS.update({'argument':'原始命令行参数。','days':'UTC 日期偏移天数。','delimiter':'单字节字段分隔符。','delta':'时间偏移秒数。','month':'月份，1 到 12。','option':'待登记的命令行选项。','padding':'是否输出 Base64 填充。','points':'Unicode 标量值列表。','seconds':'Unix 秒数。','uppercase':'是否输出大写十六进制。','year':'公历年份。'})
PARAMETERS.update({'whenTrue':'条件成立时取的分支值或无参回调',
                  'whenFalse':'条件不成立时取的分支值或无参回调'})

BEHAVIOR.update({
 'branch':'只读拆分传播值，Continue 产生成功载荷，Break 携带失败载荷。',
 'intoBranch':'在移动拥有值后拆分传播值，结果遵守成功与失败载荷的所有权规则。',
 'fromResidual':'静态地将匹配的传播载荷构造为当前函数的返回类型。',
 'ok':'成功时保留值，失败时明确丢弃错误信息并返回 None。',
 '__mapErrImpl':'只在失败时调用错误映射函数，成功值保持不变。',
 '__okOrImpl':'把 Option 的缺失转换为指定错误，错误参数按普通调用规则求值。',
 '__okOrElseImpl':'只在 Option 缺失时调用错误工厂。',
})

PARAMETERS['residual']='向当前函数返回类型传递的失败载荷。'

BEHAVIOR.update({'mapErr':'仅在失败时调用 mapper 转换错误类型，成功值保持不变。','okOr':'将 None 转为给定错误，error 按普通实参规则求值。','okOrElse':'只在 None 时调用错误工厂，成功分支不调用。'})

PARAMETERS.update({'error':'缺失时产生的错误值。','factory':'缺失时才调用的无参数错误工厂。'})

# 大整数/大数十进制方法说明：与 std::numerics 顶层函数重名，按 Owner::name 精确匹配。
BIG_BEHAVIOR = {
    'BigInteger::pow':'计算当前值的 exponent 次非负整数次幂（快速幂）；exponent 为负时报运行错误。',
    'BigInteger::powmod':'通过快速幂计算 this^exponent mod modulus；指数可以为大整数，modulus 为零或 exponent 为负时报运行错误。',
    'BigInteger::gcd':'返回两数绝对值的最大公约数。',
    'BigInteger::lcm':'返回两数绝对值的最小公倍数；任一操作数为零给零。',
    'BigInteger::mod':'非负取模：结果落在 [0, |modulus|)；modulus 为零时报运行错误。',
    'BigInteger::modInverse':'计算模乘逆元，结果落在 [0, |modulus|)；模数绝对值不大于 1 或两数不互素时报运行错误。',
    'BigInteger::isProbablePrime':'Miller-Rabin 概率素性测试，固定随机种子可复现；rounds 为测试轮数（非负）。',
    'BigInteger::nextProbablePrime':'返回严格大于当前值的下一个概率素数（25 轮测试）。',
    'BigInteger::bitLength':'返回当前值（绝对值）的二进制位长。',
    'BigInteger::bitAnd':'按位与；仅支持非负操作数，负数报运行错误。',
    'BigInteger::bitOr':'按位或；仅支持非负操作数，负数报运行错误。',
    'BigInteger::bitXor':'按位异或；仅支持非负操作数，负数报运行错误。',
    'BigInteger::strRadix':'生成 base（2..36）进制文本，数字用 0-9a-z，负数带前导 -；越界进制报运行错误。',
    'BigInteger::iroot':'整数 k 次方根，向下取整；k 小于 1 报运行错误。',
    'BigDecimal::pow':'计算当前值的 exponent 次非负整数次幂，保持当前精度；exponent 为负时报运行错误。',
    'BigDecimal::abs':'返回绝对值。',
    'BigDecimal::isInteger':'判断当前值是否为整数。',
    'BigDecimal::floor':'向负无穷取整，返回大整数。',
    'BigDecimal::ceil':'向正无穷取整，返回大整数。',
    'BigDecimal::trunc':'向零截断小数部分，返回大整数。',
    'BigDecimal::sqrt':'计算平方根，保留 digits 位有效数字。',
    'BigDecimal::root':'计算 k 次方根，保留 digits 位有效数字；k 小于 1 或 digits 越界时报运行错误。',
}
PARAMETERS.update({'rounds':'Miller-Rabin 素性测试轮数','k':'方根次数'})

PARAMETERS.update(workers='固定工作线程数量，1..1024', nanos='正纳秒间隔', delta='原子累加的 int64 增量', units='看门狗超时单位', jobs='跨线程静态任务通道', millis='等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数', task='静态任务函数，参数由签名决定')
CONCURRENCY_BEHAVIOR = {
    'createWith':'创建隔离线程并在创建时复制初始参数，拒绝借用闭包和不可传递载荷。',
    'state':'查询任务状态：0 运行、1 成功、2 失败、3 已取消；不消费句柄。',
    'requestStop':'请求指定任务在 VM 执行或等待安全点停止，不抢占阻塞宿主函数。',
    'stopRequested':'查询当前任务是否已收到协作停止请求。',
    'sleepFor':'可取消地睡眠非负毫秒数，成功返回 READY。',
    'waitResult':'收集完成结果和原始 error/trace，普通失败返回 FAILED，panic 继续传播。',
    'waitResultFor':'限时收集完成结果；超时不取消目标或消费句柄，-1 表示无限。',
    'receiveFor':'显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。',
    'tryReceive':'立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。',
    'sendFor':'在截止时间内复制发送载荷，返回统一操作状态。',
    'trySend':'立即尝试复制发送，队列满返回 FULL。',
    'isReady':'操作状态为 READY 时返回 true。',
    'isValid':'定时源句柄非零时返回 true。',
    'post':'向当前运行的有界事件队列投递 tag/source，满或关闭返回对应状态。',
    'afterNanos':'创建立即开始计时的一次性纳秒定时源。',
    'afterMillis':'创建立即开始计时的一次性毫秒定时源；无效间隔返回无效定时源。',
    'everyNanos':'创建立即开始计时的周期纳秒定时源，错过周期合并。',
    'everyMillis':'创建立即开始计时的周期毫秒定时源，错过周期合并。',
    'start':'在当前线程组中创建任务并登记统一回收责任。',
    'add':'登记已创建线程的统一等待责任，结束后的线程组拒绝新任务。',
    'waitAll':'监测全部任务，发现普通失败就取消兄弟任务，全部回收后返回是否均成功。',
    'cancelAll':'请求全部工作任务停止；线程池同时关闭并丢弃未开始的任务。',
    'closeAndWait':'关闭任务队列，排空已提交任务并回收固定工作线程。',
    'submit':'向线程池提交静态任务；有界队列满时等待并监测工作线程失败。',
    'submitFor':'限时提交静态任务，超时不取消已接收任务；拒绝已关闭或失败的线程池。',
    'fetchAdd':'原子累加 int64 并返回旧值，溢出按 64 位回绕。',
    'closeInbox':'关闭当前任务邮箱并唤醒等待者；已排队消息仍可排空。',
    'disableTimeout':'停止当前 VM 内部看门狗超时，保留内存水位和显式重启能力。',
}

CONCURRENCY_BEHAVIOR.update(enableTimeout='启用当前 VM 的内部超时检查，单位取决于宿主时钟配置；不注册事件回调。', enableTimeoutMillis='将毫秒转换为纳秒超时单位，拒绝非正值和溢出。')
CONCURRENCY_BEHAVIOR['kick']='推迟当前 VM 看门狗截止时间，未启用超时返回 NotEnabled。'
PARAMETERS['tag']='宿主事件的整数标签。'
PARAMETERS.update(lower='定积分下界表达式', upper='定积分上界表达式', number='需要精确转换的数值', operand='待取负的表达式', order='导数或展开阶数，范围 0..1000', point='极限或展开中心表达式', variable='单个自变量的名称')

PARAMETERS.update(before='待替换的精确匹配文本', after='替换文本', cursor='仍有效的节点游标', directed='是否创建有向图', keepEmpty='是否保留分割产生的空项', tree='独立转换的源二叉树', vertexCount='非负顶点数量', weight='边权值')

PARAMETERS.update(field="模型中声明的字段名或列名。", descending="为 true 时按降序排列。", limit="最大返回行数，零表示不限制。", operation="动态 SQL 操作名称或 ipage 操作符编号，按入口声明区分。", query="需要配置或生成语句的查询对象引用。", statement="包含 SQL 和绑定参数的语句值。")
