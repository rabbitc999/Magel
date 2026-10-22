/* ---------- 生命周期：块作用域所有 ---------- */

/**
 * @brief 定长数组的元素总个数（各维连乘）。
 * @param dims 各维长度。
 * @return 连乘结果。
 *
 * 以前各处都写 `int count = 1; for (dim) count *= dim;`，维数一多就溢出成
 * 负数或 0，循环上界和槽偏移跟着变成错值。这里统一用 64 位累加，
 * 并在超过 int 上界时夹住，保证下游 `int` 转换不会拿到负数。
 * 真正的入口拒绝在声明处（`ReadDims` 之后的预算检查）完成，那个检查用的
 * 也是本函数，所以能通过声明的总个数一定在 int 范围内。
 */
long long FixedArrayElementCount(const Memory::Vector<int> &dims)
{
    long long count = 1;
    for (const int dim : dims)
    {
        count *= static_cast<long long>(dim);
        if (count > 0x7fffffffLL)
        {
            count = 0x7fffffffLL;
            break;
        }
    }
    return count;
}

/**
 * @brief 一个语句块里"自动释放"的变量（离开块时按**声明逆序**软释放）。
 *
 * 所有权分三档（README 的"生命周期"一节）：
 *
 * | 写法 | 归谁管 |
 * | --- | --- |
 * | `T a[] = new [n];`（运行时数组） | **块作用域所有**：离开声明它的块时自动 `drop` |
 * | `string s = 任意字符串值` | 同上（新值接管，借来的值先复制） |
 * | `T mut& p = new;` | **手动所有**：自己 `drop`，编译器不管 |
 * | `T const&` / 标量 | 不持有（借用 / 标量） |
 * | 含字符串 / 对象叶子的结构体值 | 出块递归释放归自身所有的叶子 |
 *
 * 拥有槽为零时不生成实际释放；非零堆身份必须仍然活跃，重复释放报错。
 * 释放后必须把所有者槽清零，防止循环重入时旧句柄误释放复用该地址的新分配。
 * 堆分配代次阻止旧引用命中新对象；显式 `drop` 与内部回收均检查有效身份。
 *
 * 跳出块的三条路都补齐了释放：`break` / `continue`（释放比循环那层更深的块）、
 * `return`（释放本函数里所有块，**但不含被 return 出去的那个变量** —— 它的所有权
 * 交给调用方）。
 */
struct MOwnedScope
{
    /// 这个块里要自动释放的变量：emitted 名 + 是不是运行时数组。
    struct Item
    {
        Memory::String name;
        bool runtimeArray = false;  ///< 运行时数组的句柄是"元素起始"，块首在它前面 8 字节
    };
    Memory::Vector<Item> names;
    /// 离开作用域前执行的借用清理（列表迭代元素写回并释放引用单元）。
    Memory::Vector<simple_ast::NodePtr> finalizers;
    bool hasDeferred = false;                   ///< 本作用域登记了延迟回调。
    Memory::Set<Memory::String> deferredReads;  ///< 清理回调仍要读取的原始存储身份。
    std::size_t deferredBorrowDepth = 0;        ///< 间接闭包或聚合借用的保守来源深度。
    struct Destructor
    {
        Memory::String name;
        Memory::String owner;
        bool drop = false;       ///< 还要跑 `Drop.drop()`
        bool close = false;      ///< 还要跑 `Closeable.close()`
        bool aggregate = false;  ///< 还要递归释放聚合槽里归它所有的字符串 / 对象
        bool temporary = false;  ///< 表达式中间值：未构造时跳过，析构后清空内部槽
    };
    Memory::Vector<Destructor> destructors;
};
/**
 * @brief 当前编译单元是否需要生成作用域退出回调栈。
 */
bool g_hasDefer = false;
Memory::Vector<MOwnedScope> g_ownedScopes;

/**
 * 变量 → 它在哪一层块里声明的（`g_ownedScopes` 的下标）。
 *
 * **所有权跟着声明处**：`string s;` 声明在外层块、在循环体里被反复赋值，所有权
 * 还是外层块的 —— 不然循环体每转一圈就把它释放掉，出循环再用就是"用了已经 free
 * 的内存"（踩过：`s = s + format(…)` 累加）。
 */
Memory::Map<Memory::String, std::size_t> g_ownedHome;

/**
 * @brief 停机清理用的"这个函数累计登记过哪些所有权变量"清单。
 *
 * 与 `g_ownedScopes` 的区别：那个只记**当前位置**还没释放的（出块 / `return` 就摘了），
 * 这份清单保存函数可能持有的所有者，用于生成显式帧清理块；失败和取消不执行任意用户析构。
 * 清单在 `LowerFunction` 与全局那一段开头清空。
 */
struct MCleanupItem
{
    Memory::String name;       /* 降级后的变量名 */
    bool runtimeArray = false; /* 运行时数组：还的是块首 `句柄 - 8` */
    bool drop = false;         /* 还要跑 `Drop.drop()` */
    bool close = false;        /* 还要跑 `Closeable.close()` */
    bool aggregate = false;    /* 还要递归释放聚合槽里归它所有的字符串 / 对象 */
    Memory::String owner;      /* 析构所属结构体 */
    bool outermost = false;    /* 登记在最外那层块（函数体 / 全局）—— 只有它敢跑析构 */
};
Memory::Vector<MCleanupItem> g_cleanupItems;
/**
 * 当前函数（或全局）最外那层块在 `g_ownedScopes` 里的下标。
 */
std::size_t g_cleanupBase = 0u;
/**
 * 全局（顶层变量）的停机清理项快照。
 *
 * 程序**正常停机**（`main` 返回）时由 `EmitGlobalCleanupStatements` 展开成语句，
 * 插在入口 `main` 的每条 `return` 之前。停机时**堆内存整体清空**，所以这里只对实现了
 * `Closeable` 的全局跑一次 `close()` 关掉宿主句柄 —— 不再跑 `drop()`，也不再逐个软释放
 * 堆对象。只在正常停机跑；`std::lang::panic` 之类的异常中止仍然不跑（与 `Drop` 的既有语义一致）。
 *
 * 快照必须在函数体全部降级完**之后**才展开成语句：函数体里对全局写的
 * `g.close()` / `drop g;` 会通过 关闭路径标记 改这些项，展开早了会双重关闭。
 */
Memory::Vector<MCleanupItem> g_globalCleanupItems;

/**
 * @brief 记一笔到停机清理清单（同名合并）。
 *
 * `outermost` 只对"最外那层块"为真：里层块的变量可能**已经**在出块时跑过析构了，
 * 停机清理再跑一次就是拿销毁过的值调用户代码，所以里层的只做软释放。
 */
void NoteCleanup(const Memory::String &name, std::size_t depth, bool runtimeArray,
                 bool drop, bool close, const Memory::String &owner,
                 bool aggregate = false)
{
    for (MCleanupItem &item : g_cleanupItems)
    {
        if (item.name != name) continue;
        item.runtimeArray = item.runtimeArray || runtimeArray;
        item.drop = item.drop || drop;
        item.close = item.close || close;
        item.aggregate = item.aggregate || aggregate;
        if (item.owner.empty()) item.owner = owner;
        return;
    }
    g_cleanupItems.push_back(
        MCleanupItem{ name, runtimeArray, drop, close, aggregate, owner, depth == g_cleanupBase });
}

/**
 * @brief 进一个语句块。
 */
void PushOwnedScope() { g_ownedScopes.emplace_back(); }

/**
 * @brief 这个降级名是不是**全局变量**。
 *
 * 全局不进"出块 / 出函数自动释放"名单：它的生命周期是整个程序，只有停机清理那一层
 * （`NoteCleanup(..., g_cleanupBase, ...)`）才管它。少了这道闸，函数里给全局字符串 /
 * 容器重新赋值就会把它挂到**当前函数的块**上，函数 `return` 时顺手 free —— 下一次
 * 读全局就是"用了已经 free 过的内存"。
 */
bool IsGlobalName(const Memory::String &emitted)
{
    if (emitted.empty() || g_scope.empty()) return false;
    for (const auto &entry : g_scope.front())
        if (entry.second.emitted == emitted) return true;
    return false;
}

/**
 * @brief 把一个变量登记成"某一块所有"（重名 / 无块可挂时忽略）。
 *
 * 登记到**声明它的那一层**（见 `g_ownedHome`）：赋值可能发生在更深的内层块里
 * （循环体、if 分支），但所有权不变。
 */
void MarkOwned(const Memory::String &emitted, bool runtimeArray = false)
{
    if (emitted.empty() || g_ownedScopes.empty()) return;
    if (IsGlobalName(emitted)) return; /* 全局只挂停机清理，不参加函数 / 出块自动释放。 */
    std::size_t index = g_ownedScopes.size() - 1u;
    const auto home = g_ownedHome.find(emitted);
    if (home != g_ownedHome.end())
    {
        if (home->second >= g_ownedScopes.size()) return; /* 声明的块已经关了 */
        index = home->second;
    }
    Memory::Vector<MOwnedScope::Item> &names = g_ownedScopes[index].names;
    for (const MOwnedScope::Item &item : names)
    {
        if (item.name == emitted) return; /* 已经登记过 */
    }
    names.push_back(MOwnedScope::Item{ emitted, runtimeArray });
    NoteCleanup(emitted, index, runtimeArray, false, false, Memory::String());
}

/**
 * @brief 登记一个数组所有者，并记住它的元素类型。
 *
 * 数组的元素和字段 / 容器元素一样是**值**：字符串、对象、带字符串 / 对象叶子的结构体
 * 都归数组所有。出块 / `drop` 时先按元素逐个释放，再还数组块本身
 * （见 `MakeArrayElementRelease`）。
 *
 * 运行时数组（`T[]`）与定长数组（`T[N]`）的差别只在"数多少个元素"：
 *
 *   - 运行时数组的长度跟着对象走（句柄前 8 字节），释放时从那里读；
 *   - 定长数组的长度是**编译期常量**（各维乘积），元素槽就在帧 / 数据区里，
 *     数组块本身归声明它的函数（`release` 对数据区地址是空操作）。
 */
Memory::Map<Memory::String, std::shared_ptr<MType>> g_arrayElementTypes;

/**
 * 定长数组的元素个数（运行时数组不记：长度从对象头里读）。
 */
Memory::Map<Memory::String, int> g_arrayFixedCounts;

/**
 * @brief 标记数组表达式拥有其元素资源。
 * @param emitted 已生成清理代码的聚合对象集合。
 * @param type 类型描述。
 */
void MarkOwnedArray(const Memory::String &emitted, const std::shared_ptr<MType> &type)
{
    if (!type || type->kind != MType::Array || emitted.empty()) return;
    g_arrayElementTypes[emitted] = type->elem;
    if (IsRuntimeArray(type))
    {
        MarkOwned(emitted, true);
        return;
    }
    g_arrayFixedCounts[emitted] = static_cast<int>(FixedArrayElementCount(type->dims));
    MarkOwned(emitted, true);
}

/**
 * @brief 结构体（含递归字段）是否需要"用户析构"：自身实现 Drop / Closeable，
 *        或任一字段实现了其中一条。
 *
 * 这是"嵌套析构"的判据：结构体本身没实现 Drop、但字段实现了，也该在出块时递归析构字段。
 */
bool StructNeedsDestruction(const Memory::String &structName)
{
    if (StructHasInterface(structName, "Drop") || StructHasInterface(structName, "Closeable")) return true;
    const auto definition = g_structs.find(structName);
    if (definition == g_structs.end()) return false;
    for (const MField &field : definition->second.fields)
        if (field.type && field.type->kind == MType::Struct && StructNeedsDestruction(field.type->name))
            return true;
    return false;
}
