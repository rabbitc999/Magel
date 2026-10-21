# std::iter — 惰性迭代器

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::iter;
```

## 公共约定与错误边界

next 消费游标，peek 缓存下一项而不丢弃。转换器惰性执行，collect/fold/count 等终结操作消费输入。sizeHint 返回下界和可选上界。iter/iterMut 引用源容器，intoIter 拥有值；借用有效性由借用检查器约束。repeat/cycle 可无限，须 take 等限制后才能收集。chars 当前按字节推进，不能把它当 Unicode 字素迭代器。

## 参考代码

完整程序：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

```powershell
magel run example/samples/iter_pipeline.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::iter;
int64 observed = 0;
int64 addRefs((int64 const&,int64 const&) value) { return value.0 + value.1; }
bool smallRef(int64 const& value) { return value < 3; }
Iter<int64 const&> reversedLocal() { List<int64> values; values.push(2); values.push(4); return values.iter().rev(); }
Iter<int64 const&> prefixLocal() { List<int64> values; values.push(2); values.push(4); auto split = values.iter().peekingTakeWhile(smallRef); return split.0; }
struct IterRecord { String text; int64 number; };
void updateRecord(IterRecord mut& value) { value.text = value.text + "!"; value.number = value.number + 1; }
void replaceText(String mut& value) { value = "changed"; }
int64 textLength(String const& value) { return len(value); }
List<int64> madeList() { List<int64> result; result.push(2); result.push(3); return result; }
Iter<int64 const&> localBorrow() { List<int64> result; result.push(6); return result.iter(); }
int64 squared(int64 value) { return value * value; }
bool even(int64 value) { return value % 2 == 0; }
bool positive(int64 value) { return value > 0; }
bool belowFour(int64 value) { return value < 4; }
void observe(int64 value) { observed = observed + value; }
int64 sum(int64 acc, int64 value) { return acc + value; }
String label(int64 value) { return format("n{}", value); }
int64 read(int64 const& value) { return value; }
void bump(int64 mut& value) { value = value + 10; }
Option<int64> doubledEven(int64 value) {
    if (value % 2 == 0) { return Option<int64>::Value(value * 2); }
    return Option<int64>::None;
}
Option<int64> firstThree(int64 value) {
    if (value < 4) { return Option<int64>::Value(value); }
    return Option<int64>::None;
}
Option<int64> running(int64 mut& acc, int64 value) { acc = acc + value; return Option<int64>::Value(acc); }
Result<int64,String> checkedSum(int64 acc, int64 value) {
    if (value == 3) { return Result<int64,String>::Err("three"); }
    return Result<int64,String>::Ok(acc + value);
}
struct Flaky : IterNext<int64> {
    int64 calls;
    Option<int64> next() {
        self.calls = self.calls + 1;
        if (self.calls == 2) { return Option<int64>::Value(9); }
        return Option<int64>::None;
    }
    (int64,Option<int64>) sizeHint() { return (0,Option<int64>::None); }
};
int64 main() {
    auto values = std::iter::range(1, 5);
    auto hint = values.sizeHint();
    assert((hint.0) == (4));
    assert(((hint.1).unwrap()) == (4));
    assert((values.next().unwrap()) == (1));
    assert((values.nth(1).unwrap()) == (3));
    assert((values.count()) == (1));
    assert((values.next().isNone()) == (true));
    assert((std::iter::range(1,5).last().unwrap()) == (4));
    assert((std::iter::range(1,5).all(positive)) == (true));
    assert((std::iter::range(1,5).any(even)) == (true));
    assert((std::iter::range(1,5).find(even).unwrap()) == (2));
    auto lazy = std::iter::range(1,6).inspect(observe).map(squared).filter(even).take(2);
    assert((observed) == (0));
    auto selected = lazy.collect();
    assert((observed) == (10));
    assert((selected.len()) == (2));
    assert((selected[0]) == (4));
    assert((selected[1]) == (16));
    assert((std::iter::range(1,5).fold(0, sum)) == (10));
    assert((std::iter::range(1,7).filterMap(doubledEven).count()) == (3));
    assert((std::iter::range(1,7).findMap(doubledEven).unwrap()) == (4));
    assert((std::iter::range(1,7).mapWhile(firstThree).count()) == (3));
    assert((std::iter::range(1,7).takeWhile(belowFour).count()) == (3));
    assert((std::iter::range(1,7).skipWhile(belowFour).next().unwrap()) == (4));
    assert((std::iter::range(1,7).skip(2).take(2).fold(0,sum)) == (7));
    assert((std::iter::range(0,10).stepBy(2).last().unwrap()) == (8));
    assert((std::iter::range(1,4).rev().next().unwrap()) == (3));
    assert((std::iter::range(1,3).cycle().take(7).fold(0,sum)) == (10));
    assert((std::iter::repeat<int64>(3).take(4).fold(0,sum)) == (12));
    assert((std::iter::range(1,3).chain(std::iter::range(3,5)).fold(0,sum)) == (10));
    assert((std::iter::range(1,4).sameItems(std::iter::range(1,4))) == (true));
    assert((std::iter::range(1,4).sameItems(std::iter::range(1,5))) == (false));
    auto indexed = std::iter::range(5,7).enumerate().next().unwrap();
    assert((indexed.0) == (0));
    assert((indexed.1) == (5));
    auto zipped = std::iter::range(1,4).zip("ab".chars()).collect();
    assert((zipped.len()) == (2));
    assert(((zipped[1]).0) == (2));
    assert(((zipped[1]).1) == ('b'));
    auto split = std::iter::range(1,7).peekingTakeWhile(belowFour);
    auto first = split.0; auto rest = split.1;
    assert((first.count()) == (3));
    assert((rest.next().unwrap()) == (4));
    assert((rest.next().unwrap()) == (5));
    auto scanned = std::iter::range(1,5).scan(0,running).collect();
    assert((scanned[3]) == (10));
    auto failed = std::iter::range(1,5).tryFold(0,checkedSum);
    assert((failed.isErr()) == (true));
    assert((failed.error()) == ("three"));
    Flaky state;
    auto nonFused = std::iter::fromState<int64,Flaky>(state);
    assert((nonFused.next().isNone()) == (true));
    assert((nonFused.next().unwrap()) == (9));
    auto fused = std::iter::fromState<int64,Flaky>(state).fuse();
    assert((fused.next().isNone()) == (true));
    assert((fused.next().isNone()) == (true));
    List<int64> empty;
    assert((empty.intoIter().count()) == (0));
    {
        int64 round = 0;
        while (round < 4) {

        List<int64> source; source.push(1); source.push(2); source.push(3);
        {
            auto readonly = source.iter();
            assert((readonly.map(read).fold(0,sum)) == (6));
            auto collectedRefs = readonly.collect();
            assert((collectedRefs[0]) == (1));
            assert((readonly.zip(source.iter()).map(addRefs).fold(0,sum)) == (12));
            auto mutable = source.iterMut();
            mutable.forEach(bump);
            assert((source[0]) == (11));
            assert((source[2]) == (13));
            int64 const& saved = readonly.next().unwrap();
            assert((saved) == (11));
        }
        source.push(4);
        auto owned = source.intoIter();
        assert((source.len()) == (0));
        assert((owned.map(label).collect()[0]) == ("n11"));
        assert((owned.count()) == (4));
        auto set = {3,1,3};
        assert((set.intoIter().fold(0,sum)) == (4));
        assert((set.len()) == (0));
        auto map = {"a":"one","b":"two"};
        auto entries = map.intoIter().collect();
        assert((map.len()) == (0));
        for (Pair<String, String> entry in entries) {
            assert((entry.first == "a" && entry.second == "one")
                || (entry.first == "b" && entry.second == "two"));
        }
        List<String> words; words.push("one"); words.push("two");
        {
            auto textRefs = words.iterMut(); textRefs.forEach(replaceText);
            assert((words[0]) == ("changed"));
            assert((words.iter().map(textLength).fold(0,sum)) == (14));
        }
        List<IterRecord> records; IterRecord record; record.text = "record"; record.number = 4; records.push(record);
        { auto refs = records.iterMut(); refs.forEach(updateRecord); }
        assert(((records[0]).text) == ("record!"));
        assert(((records[0]).number) == (5));
        auto text = std::iter::range(1,4).map(label).collect();
        assert((text[2]) == ("n3"));
    
            round = round + 1;
        }
    }
    int64 raw[3]; raw[0] = 7; raw[1] = 8; raw[2] = 9;
    assert((std::iter::fromSpan<int64>(&raw[0],3).map(read).fold(0,sum)) == (24));
    int64 total = 0;
    for (int64 item in std::iter::range(1,4).map(squared)) { total = total + item; }
    assert((total) == (14));
    assert((std::lang::range(1,5).intoIter().fold(0,sum)) == (10));
    assert((std::iter::range(5,1,-1).fold(0,sum)) == (14));
    assert((madeList().intoIter().fold(0,sum)) == (5));
    assert((madeList().iter().map(read).fold(0,sum)) == (5));
    auto kept = localBorrow();
    assert((kept.map(read).fold(0,sum)) == (6));
    auto reversedRefs = reversedLocal();
    assert((reversedRefs.map(read).fold(0,sum)) == (6));
    auto prefixRefs = prefixLocal();
    assert((prefixRefs.map(read).fold(0,sum)) == (2));
    assert((std::iter::range(1,1).cycle().next().isNone()) == (true));
    assert((std::iter::range(1,5).take(0).count()) == (0));
    auto extreme = std::iter::range(-(int64)9223372036854775807 - (int64)1, 9223372036854775807);
    auto extremeHint = extreme.sizeHint();
    assert((extremeHint.0) == (0));
    assert(((extremeHint.1).isNone()) == (true));
    assert((std::iter::range(9223372036854775806, 9223372036854775807, 2).count()) == (1));
    assert((std::iter::range(-(int64)9223372036854775807, -(int64)9223372036854775807 - (int64)1, -2).count()) == (1));
    println("iterator pipelines OK");
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
iterator pipelines OK
```

</details>

## 点调用与自由函数对应

`Iter` 的适配器点调用由编译器转为同名自由函数，接收者作为第一个参数。例如 `cursor.map(mapper)` 等价于 `std::iter::map(cursor,mapper)`。下表中的方法同样适用各函数条目的参数、返回值与边界。

| 点调用 | 对应入口 |
| --- | --- |
| `cursor.map(...)` | `std::iter::map(cursor,...)` |
| `cursor.filter(...)` | `std::iter::filter(cursor,...)` |
| `cursor.filterMap(...)` | `std::iter::filterMap(cursor,...)` |
| `cursor.mapWhile(...)` | `std::iter::mapWhile(cursor,...)` |
| `cursor.takeWhile(...)` | `std::iter::takeWhile(cursor,...)` |
| `cursor.skipWhile(...)` | `std::iter::skipWhile(cursor,...)` |
| `cursor.findMap(...)` | `std::iter::findMap(cursor,...)` |
| `cursor.fold(...)` | `std::iter::fold(cursor,...)` |
| `cursor.tryFold(...)` | `std::iter::tryFold(cursor,...)` |
| `cursor.scan(...)` | `std::iter::scan(cursor,...)` |
| `cursor.take(...)` | `std::iter::take(cursor,...)` |
| `cursor.skip(...)` | `std::iter::skip(cursor,...)` |
| `cursor.zip(...)` | `std::iter::zip(cursor,...)` |
| `cursor.cycle(...)` | `std::iter::cycle(cursor,...)` |
| `cursor.chain(...)` | `std::iter::chain(cursor,...)` |
| `cursor.rev(...)` | `std::iter::rev(cursor,...)` |
| `cursor.enumerate(...)` | `std::iter::enumerate(cursor,...)` |
| `cursor.peekingTakeWhile(...)` | `std::iter::peekingTakeWhile(cursor,...)` |
| `cursor.stepBy(...)` | `std::iter::stepBy(cursor,...)` |
| `cursor.fuse(...)` | `std::iter::fuse(cursor,...)` |
| `cursor.inspect(...)` | `std::iter::inspect(cursor,...)` |
| `cursor.sameItems(...)` | `std::iter::sameItems(cursor,...)` |
| `cursor.collect(...)` | `std::iter::collect(cursor,...)` |
| `list.iter()` / `list.iterMut()` | `listIter(&list)` / `listIterMut(&list)` |
| `list.intoIter()` | `listIntoIter(&list)`，移动元素，源容器清空 |
| `set.iter()` / `map.iter()` | `setIter(&set)` / `mapIter(&map)`，借用快照，来源容器保持不变 |
| `set.intoIter()` / `map.intoIter()` | `setIntoIter(&set)` / `mapIntoIter(&map)`，移动元素，源容器清空 |
| `text.chars()` / `range.intoIter()` | `chars(text)` / `fromRange(range)` |

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-134"></a>`interface IterNext<T>` | 有状态迭代器接口；每次 next 消费一个元素。 |
| 模块 | <a id="symbol-135"></a>`struct Iter<T> : Iterator<T>` | 持有惰性游标状态，按 Magel 值语义复制时获得独立游标。 |
| Iter | <a id="symbol-136"></a>`std::lang::Any state` | 以 Any 持有的具体迭代游标状态。 |
| Iter | <a id="symbol-137"></a>`bool buffered` | 是否已缓存 peek 取得的下一项。 |
| Iter | <a id="symbol-138"></a>`Option<T> pending` | 等待消费的元素或队列。 |
| 模块 | <a id="symbol-139"></a>`struct IteratorValueState<T,S : Iterator<T>> : IterNext<T>` | 将语言 Iterator 值保存为独立的惰性游标。 |
| IteratorValueState | <a id="symbol-140"></a>`S source` | 被适配的源游标或连续存储。 |
| IteratorValueState | <a id="symbol-141"></a>`int64 position` | 下一项在源存储中的位置。 |
| 模块 | <a id="symbol-142"></a>`struct IterListState<T> : IterNext<T>` | 列表值迭代状态，保留未消费的元素。 |
| IterListState | <a id="symbol-143"></a>`std::lang::List<T> values` | 按声明顺序保存的元素存储。 |
| IterListState | <a id="symbol-144"></a>`int64 position` | 下一项在源存储中的位置。 |
| 模块 | <a id="symbol-145"></a>`struct IterRangeState : IterNext<int64>` | 创建半开整数区间的惰性迭代器。 |
| IterRangeState | <a id="symbol-146"></a>`int64 current` | 当前迭代位置或正在消费的游标。 |
| IterRangeState | <a id="symbol-147"></a>`int64 stop` | 区间结束边界。 |
| IterRangeState | <a id="symbol-148"></a>`int64 step` | 每次推进的步长。 |
| IterRangeState | <a id="symbol-149"></a>`bool done` | 游标是否已经结束。 |
| 模块 | <a id="symbol-150"></a>`struct IterMapState<T,U> : IterNext<U>` | IterMapState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterMapState | <a id="symbol-151"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterMapState | <a id="symbol-152"></a>`U fn(T) callback` | 宿主事件发生时执行的回调。 |
| 模块 | <a id="symbol-153"></a>`struct IterFilterState<T> : IterNext<T>` | IterFilterState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterFilterState | <a id="symbol-154"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterFilterState | <a id="symbol-155"></a>`bool fn(T) predicate` | 用于筛选或判断停止条件的回调。 |
| 模块 | <a id="symbol-156"></a>`struct IterFilterMapState<T,U> : IterNext<U>` | IterFilterMapState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterFilterMapState | <a id="symbol-157"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterFilterMapState | <a id="symbol-158"></a>`Option<U> fn(T) callback` | 宿主事件发生时执行的回调。 |
| 模块 | <a id="symbol-159"></a>`struct IterMapWhileState<T,U> : IterNext<U>` | IterMapWhileState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterMapWhileState | <a id="symbol-160"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterMapWhileState | <a id="symbol-161"></a>`bool done` | 游标是否已经结束。 |
| IterMapWhileState | <a id="symbol-162"></a>`Option<U> fn(T) callback` | 宿主事件发生时执行的回调。 |
| 模块 | <a id="symbol-163"></a>`struct IterTakeWhileState<T> : IterNext<T>` | IterTakeWhileState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterTakeWhileState | <a id="symbol-164"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterTakeWhileState | <a id="symbol-165"></a>`bool done` | 游标是否已经结束。 |
| IterTakeWhileState | <a id="symbol-166"></a>`bool fn(T) predicate` | 用于筛选或判断停止条件的回调。 |
| 模块 | <a id="symbol-167"></a>`struct IterSkipWhileState<T> : IterNext<T>` | IterSkipWhileState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterSkipWhileState | <a id="symbol-168"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterSkipWhileState | <a id="symbol-169"></a>`bool skipping` | 是否仍处于跳过前缀的阶段。 |
| IterSkipWhileState | <a id="symbol-170"></a>`bool fn(T) predicate` | 用于筛选或判断停止条件的回调。 |
| 模块 | <a id="symbol-171"></a>`struct IterTakeState<T> : IterNext<T>` | IterTakeState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterTakeState | <a id="symbol-172"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterTakeState | <a id="symbol-173"></a>`int64 remaining` | 尚需跳过或允许消费的元素数量。 |
| 模块 | <a id="symbol-174"></a>`struct IterSkipState<T> : IterNext<T>` | IterSkipState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterSkipState | <a id="symbol-175"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterSkipState | <a id="symbol-176"></a>`int64 remaining` | 尚需跳过或允许消费的元素数量。 |
| 模块 | <a id="symbol-177"></a>`struct IterChainState<T> : IterNext<T>` | IterChainState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterChainState | <a id="symbol-178"></a>`Iter<T> left` | 第一个输入游标。 |
| IterChainState | <a id="symbol-179"></a>`Iter<T> right` | 第二个输入游标。 |
| IterChainState | <a id="symbol-180"></a>`bool switched` | 是否已切换到第二个输入游标。 |
| 模块 | <a id="symbol-181"></a>`struct IterZipState<T,U> : IterNext<(T,U)>` | IterZipState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterZipState | <a id="symbol-182"></a>`Iter<T> left` | 第一个输入游标。 |
| IterZipState | <a id="symbol-183"></a>`Iter<U> right` | 第二个输入游标。 |
| 模块 | <a id="symbol-184"></a>`struct IterEnumerateState<T> : IterNext<(int64,T)>` | IterEnumerateState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterEnumerateState | <a id="symbol-185"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterEnumerateState | <a id="symbol-186"></a>`int64 index` | 当前元素序号，从 0 开始。 |
| 模块 | <a id="symbol-187"></a>`struct IterStepState<T> : IterNext<T>` | IterStepState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterStepState | <a id="symbol-188"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterStepState | <a id="symbol-189"></a>`int64 step` | 每次推进的步长。 |
| IterStepState | <a id="symbol-190"></a>`bool started` | 是否已消费首个元素。 |
| 模块 | <a id="symbol-191"></a>`struct IterFuseState<T> : IterNext<T>` | IterFuseState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterFuseState | <a id="symbol-192"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterFuseState | <a id="symbol-193"></a>`bool done` | 游标是否已经结束。 |
| 模块 | <a id="symbol-194"></a>`struct IterInspectState<T> : IterNext<T>` | IterInspectState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterInspectState | <a id="symbol-195"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterInspectState | <a id="symbol-196"></a>`void fn(T) action` | 逐项执行的观察或处理回调。 |
| 模块 | <a id="symbol-197"></a>`struct IterCycleState<T> : IterNext<T>` | IterCycleState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterCycleState | <a id="symbol-198"></a>`Iter<T> initial` | 用于重新开始遍历的初始游标。 |
| IterCycleState | <a id="symbol-199"></a>`Iter<T> current` | 当前迭代位置或正在消费的游标。 |
| IterCycleState | <a id="symbol-200"></a>`bool empty` | 输入是否已确认没有元素。 |
| 模块 | <a id="symbol-201"></a>`struct IterScanState<T,A,U> : IterNext<U>` | IterScanState 的惰性状态，仅在 next 时执行回调或消费来源。 |
| IterScanState | <a id="symbol-202"></a>`Iter<T> source` | 被适配的源游标或连续存储。 |
| IterScanState | <a id="symbol-203"></a>`A accumulator` | 扫描过程中保存的累积状态。 |
| IterScanState | <a id="symbol-204"></a>`Option<U> fn(A mut&, T) callback` | 宿主事件发生时执行的回调。 |
| 模块 | <a id="symbol-205"></a>`struct IterBufferState<T> : IterNext<T>` | 缓存有限来源，保留来源状态以延长元素引用的有效期。 |
| IterBufferState | <a id="symbol-206"></a>`std::lang::List<T> values` | 按声明顺序保存的元素存储。 |
| IterBufferState | <a id="symbol-207"></a>`std::lang::Any anchor` | 用于维持缓冲元素借用有效性的源状态。 |
| IterBufferState | <a id="symbol-208"></a>`int64 position` | 下一项在源存储中的位置。 |
| 模块 | <a id="symbol-209"></a>`struct IterConstListState<T> : IterNext<T const&>` | 持有列表只读租约的迭代状态，逐项返回 const&。 |
| IterConstListState | <a id="symbol-210"></a>`std::lang::Any lease` | 保持源列表存储有效的迭代租约。 |
| IterConstListState | <a id="symbol-211"></a>`int64 position` | 下一项在源存储中的位置。 |
| 模块 | <a id="symbol-212"></a>`struct IterMutListState<T> : IterNext<T mut&>` | 持有列表可写租约的迭代状态，逐项返回 mut&。 |
| IterMutListState | <a id="symbol-213"></a>`std::lang::Any lease` | 保持源列表存储有效的迭代租约。 |
| IterMutListState | <a id="symbol-214"></a>`int64 position` | 下一项在源存储中的位置。 |
| 模块 | <a id="symbol-215"></a>`struct IterSpanState<T> : IterNext<T const&>` | 读取数组或 Span 的只读引用，来源必须覆盖游标生命周期。 |
| IterSpanState | <a id="symbol-216"></a>`T const& source` | 被适配的源游标或连续存储。 |
| IterSpanState | <a id="symbol-217"></a>`int64 length` | 连续存储中可访问的元素数量。 |
| IterSpanState | <a id="symbol-218"></a>`int64 position` | 下一项在源存储中的位置。 |
| 模块 | <a id="symbol-219"></a>`struct IterCharsState : IterNext<char>` | 按 Magel 字符模型遍历字符串字节，持有独立字符串副本。 |
| IterCharsState | <a id="symbol-220"></a>`std::lang::String source` | 被适配的源游标或连续存储。 |
| IterCharsState | <a id="symbol-221"></a>`int64 position` | 下一项在源存储中的位置。 |
| 模块 | <a id="symbol-222"></a>`struct IterRepeatState<T> : IterNext<T>` | 惰性无限重复一个值，可配合 take 创建有限输出。 |
| IterRepeatState | <a id="symbol-223"></a>`T value` | 保存的单个值。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [IterNext::next](#api-0) | `Option<T> next()` |
| [IterNext::sizeHint](#api-1) | `(int64, Option<int64>) sizeHint()` |
| [Iter::peek](#api-2) | `Option<T> peek()` |
| [Iter::next](#api-3) | `Option<T> next()` |
| [Iter::sizeHint](#api-4) | `(int64, Option<int64>) sizeHint()` |
| [Iter::iterator](#api-5) | `Option<T> iterator(int64 index)` |
| [Iter::count](#api-6) | `int64 count()` |
| [Iter::nth](#api-7) | `Option<T> nth(int64 index)` |
| [Iter::last](#api-8) | `Option<T> last()` |
| [Iter::all](#api-9) | `bool all(bool fn(T) predicate)` |
| [Iter::any](#api-10) | `bool any(bool fn(T) predicate)` |
| [Iter::find](#api-11) | `Option<T> find(bool fn(T) predicate)` |
| [Iter::forEach](#api-12) | `void forEach(void fn(T) action)` |
| [fromState](#api-13) | `Iter<T> fromState<T,S>(S state)` |
| [IteratorValueState::next](#api-14) | `Option<T> next()` |
| [IteratorValueState::sizeHint](#api-15) | `(int64, Option<int64>) sizeHint()` |
| [fromIterator](#api-16) | `Iter<T> fromIterator<T,S : Iterator<T>>(S source)` |
| [IterListState::next](#api-17) | `Option<T> next()` |
| [IterListState::sizeHint](#api-18) | `(int64, Option<int64>) sizeHint()` |
| [fromList](#api-19) | `Iter<T> fromList<T>(std::lang::List<T> values)` |
| [IterRangeState::next](#api-20) | `Option<int64> next()` |
| [IterRangeState::sizeHint](#api-21) | `(int64, Option<int64>) sizeHint()` |
| [range](#api-22) | `Iter<int64> range(int64 start, int64 stop, int64 step)` |
| [range](#api-23) | `Iter<int64> range(int64 start, int64 stop)` |
| [IterMapState::next](#api-24) | `Option<U> next()` |
| [IterMapState::sizeHint](#api-25) | `(int64, Option<int64>) sizeHint()` |
| [map](#api-26) | `Iter<U> map<T,U>(Iter<T> source, U fn(T) callback)` |
| [IterFilterState::next](#api-27) | `Option<T> next()` |
| [IterFilterState::sizeHint](#api-28) | `(int64, Option<int64>) sizeHint()` |
| [filter](#api-29) | `Iter<T> filter<T>(Iter<T> source, bool fn(T) predicate)` |
| [IterFilterMapState::next](#api-30) | `Option<U> next()` |
| [IterFilterMapState::sizeHint](#api-31) | `(int64, Option<int64>) sizeHint()` |
| [filterMap](#api-32) | `Iter<U> filterMap<T,U>(Iter<T> source, Option<U> fn(T) callback)` |
| [IterMapWhileState::next](#api-33) | `Option<U> next()` |
| [IterMapWhileState::sizeHint](#api-34) | `(int64, Option<int64>) sizeHint()` |
| [mapWhile](#api-35) | `Iter<U> mapWhile<T,U>(Iter<T> source, Option<U> fn(T) callback)` |
| [IterTakeWhileState::next](#api-36) | `Option<T> next()` |
| [IterTakeWhileState::sizeHint](#api-37) | `(int64, Option<int64>) sizeHint()` |
| [takeWhile](#api-38) | `Iter<T> takeWhile<T>(Iter<T> source, bool fn(T) predicate)` |
| [IterSkipWhileState::next](#api-39) | `Option<T> next()` |
| [IterSkipWhileState::sizeHint](#api-40) | `(int64, Option<int64>) sizeHint()` |
| [skipWhile](#api-41) | `Iter<T> skipWhile<T>(Iter<T> source, bool fn(T) predicate)` |
| [IterTakeState::next](#api-42) | `Option<T> next()` |
| [IterTakeState::sizeHint](#api-43) | `(int64, Option<int64>) sizeHint()` |
| [take](#api-44) | `Iter<T> take<T>(Iter<T> source, int64 amount)` |
| [IterSkipState::next](#api-45) | `Option<T> next()` |
| [IterSkipState::sizeHint](#api-46) | `(int64, Option<int64>) sizeHint()` |
| [skip](#api-47) | `Iter<T> skip<T>(Iter<T> source, int64 amount)` |
| [IterChainState::next](#api-48) | `Option<T> next()` |
| [IterChainState::sizeHint](#api-49) | `(int64, Option<int64>) sizeHint()` |
| [chain](#api-50) | `Iter<T> chain<T>(Iter<T> left, Iter<T> right)` |
| [IterZipState::next](#api-51) | `Option<(T,U)> next()` |
| [IterZipState::sizeHint](#api-52) | `(int64, Option<int64>) sizeHint()` |
| [zip](#api-53) | `Iter<(T,U)> zip<T,U>(Iter<T> left, Iter<U> right)` |
| [IterEnumerateState::next](#api-54) | `Option<(int64,T)> next()` |
| [IterEnumerateState::sizeHint](#api-55) | `(int64, Option<int64>) sizeHint()` |
| [enumerate](#api-56) | `Iter<(int64,T)> enumerate<T>(Iter<T> source)` |
| [IterStepState::next](#api-57) | `Option<T> next()` |
| [IterStepState::sizeHint](#api-58) | `(int64, Option<int64>) sizeHint()` |
| [stepBy](#api-59) | `Iter<T> stepBy<T>(Iter<T> source, int64 step)` |
| [IterFuseState::next](#api-60) | `Option<T> next()` |
| [IterFuseState::sizeHint](#api-61) | `(int64, Option<int64>) sizeHint()` |
| [fuse](#api-62) | `Iter<T> fuse<T>(Iter<T> source)` |
| [IterInspectState::next](#api-63) | `Option<T> next()` |
| [IterInspectState::sizeHint](#api-64) | `(int64, Option<int64>) sizeHint()` |
| [inspect](#api-65) | `Iter<T> inspect<T>(Iter<T> source, void fn(T) action)` |
| [IterCycleState::next](#api-66) | `Option<T> next()` |
| [IterCycleState::sizeHint](#api-67) | `(int64, Option<int64>) sizeHint()` |
| [cycle](#api-68) | `Iter<T> cycle<T>(Iter<T> source)` |
| [IterScanState::next](#api-69) | `Option<U> next()` |
| [IterScanState::sizeHint](#api-70) | `(int64, Option<int64>) sizeHint()` |
| [scan](#api-71) | `Iter<U> scan<T,A,U>(Iter<T> source, A initial, Option<U> fn(A mut&, T) callback)` |
| [collect](#api-72) | `std::lang::List<T> collect<T>(Iter<T> source)` |
| [fold](#api-73) | `A fold<T,A>(Iter<T> source, A initial, A fn(A, T) callback)` |
| [tryFold](#api-74) | `Result<A,E> tryFold<T,A,E>(Iter<T> source, A initial, Result<A,E> fn(A, T) callback)` |
| [findMap](#api-75) | `Option<U> findMap<T,U>(Iter<T> source, Option<U> fn(T) callback)` |
| [sameItems](#api-76) | `bool sameItems<T>(Iter<T> left, Iter<T> right)` |
| [IterBufferState::next](#api-77) | `Option<T> next()` |
| [IterBufferState::sizeHint](#api-78) | `(int64,Option<int64>) sizeHint()` |
| [rev](#api-79) | `Iter<T> rev<T>(Iter<T> source)` |
| [peekingTakeWhile](#api-80) | `(Iter<T>,Iter<T>) peekingTakeWhile<T>(Iter<T> source, bool fn(T) predicate)` |
| [countIter](#api-81) | `int64 countIter<T>(Iter<T> mut& source)` |
| [nth](#api-82) | `Option<T> nth<T>(Iter<T> mut& source, int64 index)` |
| [last](#api-83) | `Option<T> last<T>(Iter<T> mut& source)` |
| [all](#api-84) | `bool all<T>(Iter<T> mut& source, bool fn(T) predicate)` |
| [any](#api-85) | `bool any<T>(Iter<T> mut& source, bool fn(T) predicate)` |
| [find](#api-86) | `Option<T> find<T>(Iter<T> mut& source, bool fn(T) predicate)` |
| [forEach](#api-87) | `void forEach<T>(Iter<T> mut& source, void fn(T) action)` |
| [listIntoIter](#api-88) | `Iter<T> listIntoIter<T>(std::lang::List<T> mut& source)` |
| [fromRange](#api-89) | `Iter<int64> fromRange(std::lang::__Range source)` |
| [IterConstListState::next](#api-90) | `Option<T const&> next()` |
| [IterConstListState::sizeHint](#api-91) | `(int64, Option<int64>) sizeHint()` |
| [IterMutListState::next](#api-92) | `Option<T mut&> next()` |
| [IterMutListState::sizeHint](#api-93) | `(int64, Option<int64>) sizeHint()` |
| [listIter](#api-94) | `Iter<T const&> listIter<T>(std::lang::List<T> const& source)` |
| [listIterMut](#api-95) | `Iter<T mut&> listIterMut<T>(std::lang::List<T> mut& source)` |
| [setIntoIter](#api-96) | `Iter<T> setIntoIter<T>(std::lang::Set<T> mut& source)` |
| [setIter](#api-97) | `Iter<T> setIter<T>(std::lang::Set<T> const& source)` |
| [mapIntoIter](#api-98) | `Iter<Pair<K,V>> mapIntoIter<K,V>(std::lang::Map<K,V> mut& source)` |
| [mapIter](#api-99) | `Iter<Pair<K,V>> mapIter<K,V>(std::lang::Map<K,V> const& source)` |
| [IterSpanState::next](#api-100) | `Option<T const&> next()` |
| [IterSpanState::sizeHint](#api-101) | `(int64, Option<int64>) sizeHint()` |
| [fromSpan](#api-102) | `Iter<T const&> fromSpan<T>(T const& source, int64 length)` |
| [IterCharsState::next](#api-103) | `Option<char> next()` |
| [IterCharsState::sizeHint](#api-104) | `(int64, Option<int64>) sizeHint()` |
| [chars](#api-105) | `Iter<char> chars(std::lang::String source)` |
| [IterRepeatState::next](#api-106) | `Option<T> next()` |
| [IterRepeatState::sizeHint](#api-107) | `(int64, Option<int64>) sizeHint()` |
| [repeat](#api-108) | `Iter<T> repeat<T>(T value)` |

<a id="api-0"></a>

### IterNext::next（1）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:9](../../source/std/iter.mgl#L9)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-1"></a>

### IterNext::sizeHint（2）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:14](../../source/std/iter.mgl#L14)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-2"></a>

### Iter::peek（3）

**声明**

```magel
Option<T> peek();
```

**行为**

查看下一项并保留该项供后续消费。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。查看下一项并保留该项供后续消费。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.peek();
```

**源码**：[source/std/iter.mgl:26](../../source/std/iter.mgl#L26)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-3"></a>

### Iter::next（4）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:38](../../source/std/iter.mgl#L38)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-4"></a>

### Iter::sizeHint（5）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:52](../../source/std/iter.mgl#L52)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-5"></a>

### Iter::iterator（6）

**声明**

```magel
Option<T> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从零开始的下标。 | 无 |

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/iter.mgl:78](../../source/std/iter.mgl#L78)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-6"></a>

### Iter::count（7）

**声明**

```magel
int64 count();
```

**行为**

消费并返回项数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。消费并返回项数。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.count();
```

**源码**：[source/std/iter.mgl:85](../../source/std/iter.mgl#L85)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-7"></a>

### Iter::nth（8）

**声明**

```magel
Option<T> nth(int64 index);
```

**行为**

消费到从 0 开始的第 index 项，返回 Option。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从零开始的下标。 | 无 |

**返回值**

`Option<T>`。消费到从 0 开始的第 index 项，返回 Option。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`index >= 0, "nth 下标不能为负数"`。

**调用形式**

```magel
obj.nth(index);
```

**源码**：[source/std/iter.mgl:101](../../source/std/iter.mgl#L101)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-8"></a>

### Iter::last（9）

**声明**

```magel
Option<T> last();
```

**行为**

消费全部剩余项并返回最后一项。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。消费全部剩余项并返回最后一项。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.last();
```

**源码**：[source/std/iter.mgl:121](../../source/std/iter.mgl#L121)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-9"></a>

### Iter::all（10）

**声明**

```magel
bool all(bool fn(T) predicate);
```

**行为**

消费并判断是否全部满足谓词，空源为 true。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`bool`。消费并判断是否全部满足谓词，空源为 true。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.all(predicate);
```

**源码**：[source/std/iter.mgl:137](../../source/std/iter.mgl#L137)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-10"></a>

### Iter::any（11）

**声明**

```magel
bool any(bool fn(T) predicate);
```

**行为**

消费并判断是否存在满足谓词的项，空源为 false。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`bool`。消费并判断是否存在满足谓词的项，空源为 false。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.any(predicate);
```

**源码**：[source/std/iter.mgl:154](../../source/std/iter.mgl#L154)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-11"></a>

### Iter::find（12）

**声明**

```magel
Option<T> find(bool fn(T) predicate);
```

**行为**

消费到首个 predicate 为真的项并返回 Option。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`Option<T>`。消费到首个 predicate 为真的项并返回 Option。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.find(predicate);
```

**源码**：[source/std/iter.mgl:171](../../source/std/iter.mgl#L171)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-12"></a>

### Iter::forEach（13）

**声明**

```magel
void forEach(void fn(T) action);
```

**行为**

逐项调用 action 并消费迭代器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `action` | `void fn(T) action` | 元素处理函数。 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.forEach(action);
```

**源码**：[source/std/iter.mgl:188](../../source/std/iter.mgl#L188)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-13"></a>

### fromState（14）

**声明**

```magel
Iter<T> fromState<T,S>(S state);
```

**行为**

复制实现 IterNext<T> 的状态值到专有存储，返回 Iter<T>。

将实现 IterNext 的值装入独立游标。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `state` | `S state` | 初始状态值。 | 无 |

**返回值**

`Iter<T>`。复制实现 IterNext<T> 的状态值到专有存储，返回 Iter<T>。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::fromState<T,S>(state);
```

**源码**：[source/std/iter.mgl:210](../../source/std/iter.mgl#L210)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-14"></a>

### IteratorValueState::next（15）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

从语言迭代协议读取下一项。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。本次元素，或表示迭代结束的 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:224](../../source/std/iter.mgl#L224)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-15"></a>

### IteratorValueState::sizeHint（16）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

提供未知长度的剩余项估计。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。下界为零，上界未知。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:235](../../source/std/iter.mgl#L235)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-16"></a>

### fromIterator（17）

**声明**

```magel
Iter<T> fromIterator<T,S : Iterator<T>>(S source);
```

**行为**

复制实现 Iterator<T> 的来源值，建立可组合的独立惰性游标；不会消费来源变量。

复制语言迭代器值，建立可组合的独立游标。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `S source` | 实现 Iterator<T> 的来源值。 | 无 |

**返回值**

`Iter<T>`。拥有来源副本的惰性迭代器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::fromIterator<T,S >>(source);
```

**源码**：[source/std/iter.mgl:245](../../source/std/iter.mgl#L245)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-17"></a>

### IterListState::next（18）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:260](../../source/std/iter.mgl#L260)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-18"></a>

### IterListState::sizeHint（19）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:272](../../source/std/iter.mgl#L272)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-19"></a>

### fromList（20）

**声明**

```magel
Iter<T> fromList<T>(std::lang::List<T> values);
```

**行为**

用列表值构造迭代器。

从列表创建独立值迭代器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `values` | `std::lang::List<T> values` | 操作参数。 | 无 |

**返回值**

`Iter<T>`。用列表值构造迭代器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::fromList<T>(values);
```

**源码**：[source/std/iter.mgl:284](../../source/std/iter.mgl#L284)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-20"></a>

### IterRangeState::next（21）

**声明**

```magel
Option<int64> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:300](../../source/std/iter.mgl#L300)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-21"></a>

### IterRangeState::sizeHint（22）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:317](../../source/std/iter.mgl#L317)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-22"></a>

### range（23）

**声明**

```magel
Iter<int64> range(int64 start, int64 stop, int64 step);
```

**行为**

构造半开整数区间 [start,stop) 的惰性迭代器，步长不能为零。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 区间起点。 | 无 |
| `stop` | `int64 stop` | 区间终点，不包含。 | 无 |
| `step` | `int64 step` | 步长。 | 无 |

**返回值**

`Iter<int64>`。构造半开整数区间 [start,stop) 的惰性迭代器，步长不能为零。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`step != 0, "迭代器区间步长不能为零"`。

**调用形式**

```magel
std::iter::range(start, stop, step);
```

**源码**：[source/std/iter.mgl:351](../../source/std/iter.mgl#L351)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-23"></a>

### range（24）

**声明**

```magel
Iter<int64> range(int64 start, int64 stop);
```

**行为**

构造半开整数区间 [start,stop) 的惰性迭代器，步长不能为零。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 区间起点。 | 无 |
| `stop` | `int64 stop` | 区间终点，不包含。 | 无 |

**返回值**

`Iter<int64>`。构造半开整数区间 [start,stop) 的惰性迭代器，步长不能为零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::range(start, stop);
```

**源码**：[source/std/iter.mgl:366](../../source/std/iter.mgl#L366)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-24"></a>

### IterMapState::next（25）

**声明**

```magel
Option<U> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<U>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:378](../../source/std/iter.mgl#L378)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-25"></a>

### IterMapState::sizeHint（26）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:389](../../source/std/iter.mgl#L389)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-26"></a>

### map（27）

**声明**

```magel
Iter<U> map<T,U>(Iter<T> source, U fn(T) callback);
```

**行为**

惰性调用 mapper，将 T 转成 U。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `callback` | `U fn(T) callback` | 处理元素或累加状态的函数。 | 无 |

**返回值**

`Iter<U>`。惰性调用 mapper，将 T 转成 U。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::map<T,U>(source, callback);
```

**源码**：[source/std/iter.mgl:401](../../source/std/iter.mgl#L401)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-27"></a>

### IterFilterState::next（28）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:416](../../source/std/iter.mgl#L416)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-28"></a>

### IterFilterState::sizeHint（29）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:432](../../source/std/iter.mgl#L432)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-29"></a>

### filter（30）

**声明**

```magel
Iter<T> filter<T>(Iter<T> source, bool fn(T) predicate);
```

**行为**

惰性保留 predicate 为真的项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`Iter<T>`。惰性保留 predicate 为真的项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::filter<T>(source, predicate);
```

**源码**：[source/std/iter.mgl:445](../../source/std/iter.mgl#L445)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-30"></a>

### IterFilterMapState::next（31）

**声明**

```magel
Option<U> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<U>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:460](../../source/std/iter.mgl#L460)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-31"></a>

### IterFilterMapState::sizeHint（32）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:477](../../source/std/iter.mgl#L477)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-32"></a>

### filterMap（33）

**声明**

```magel
Iter<U> filterMap<T,U>(Iter<T> source, Option<U> fn(T) callback);
```

**行为**

调用返回 Option 的 mapper，只输出 Value。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `callback` | `Option<U> fn(T) callback` | 处理元素或累加状态的函数。 | 无 |

**返回值**

`Iter<U>`。调用返回 Option 的 mapper，只输出 Value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::filterMap<T,U>(source, callback);
```

**源码**：[source/std/iter.mgl:490](../../source/std/iter.mgl#L490)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-33"></a>

### IterMapWhileState::next（34）

**声明**

```magel
Option<U> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<U>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:506](../../source/std/iter.mgl#L506)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-34"></a>

### IterMapWhileState::sizeHint（35）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:525](../../source/std/iter.mgl#L525)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-35"></a>

### mapWhile（36）

**声明**

```magel
Iter<U> mapWhile<T,U>(Iter<T> source, Option<U> fn(T) callback);
```

**行为**

转换元素，遇到首个 None 时结束。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `callback` | `Option<U> fn(T) callback` | 处理元素或累加状态的函数。 | 无 |

**返回值**

`Iter<U>`。转换元素，遇到首个 None 时结束。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::mapWhile<T,U>(source, callback);
```

**源码**：[source/std/iter.mgl:538](../../source/std/iter.mgl#L538)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-36"></a>

### IterTakeWhileState::next（37）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:554](../../source/std/iter.mgl#L554)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-37"></a>

### IterTakeWhileState::sizeHint（38）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:573](../../source/std/iter.mgl#L573)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-38"></a>

### takeWhile（39）

**声明**

```magel
Iter<T> takeWhile<T>(Iter<T> source, bool fn(T) predicate);
```

**行为**

输出谓词为真的前缀，消费使谓词失败的那一项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`Iter<T>`。输出谓词为真的前缀，消费使谓词失败的那一项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::takeWhile<T>(source, predicate);
```

**源码**：[source/std/iter.mgl:586](../../source/std/iter.mgl#L586)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-39"></a>

### IterSkipWhileState::next（40）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:602](../../source/std/iter.mgl#L602)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-40"></a>

### IterSkipWhileState::sizeHint（41）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:619](../../source/std/iter.mgl#L619)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-41"></a>

### skipWhile（42）

**声明**

```magel
Iter<T> skipWhile<T>(Iter<T> source, bool fn(T) predicate);
```

**行为**

在前缀谓词为真期间丢弃，之后输出全部剩余项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`Iter<T>`。在前缀谓词为真期间丢弃，之后输出全部剩余项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::skipWhile<T>(source, predicate);
```

**源码**：[source/std/iter.mgl:632](../../source/std/iter.mgl#L632)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-42"></a>

### IterTakeState::next（43）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:648](../../source/std/iter.mgl#L648)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-43"></a>

### IterTakeState::sizeHint（44）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:659](../../source/std/iter.mgl#L659)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-44"></a>

### take（45）

**声明**

```magel
Iter<T> take<T>(Iter<T> source, int64 amount);
```

**行为**

最多输出前 count 项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `amount` | `int64 amount` | 元素数量，不能为负数。 | 无 |

**返回值**

`Iter<T>`。最多输出前 count 项。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`amount >= 0, "take 数量不能为负数"`。

**调用形式**

```magel
std::iter::take<T>(source, amount);
```

**源码**：[source/std/iter.mgl:680](../../source/std/iter.mgl#L680)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-45"></a>

### IterSkipState::next（46）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:696](../../source/std/iter.mgl#L696)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-46"></a>

### IterSkipState::sizeHint（47）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:711](../../source/std/iter.mgl#L711)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-47"></a>

### skip（48）

**声明**

```magel
Iter<T> skip<T>(Iter<T> source, int64 amount);
```

**行为**

丢弃前 count 项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `amount` | `int64 amount` | 元素数量，不能为负数。 | 无 |

**返回值**

`Iter<T>`。丢弃前 count 项。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`amount >= 0, "skip 数量不能为负数"`。

**调用形式**

```magel
std::iter::skip<T>(source, amount);
```

**源码**：[source/std/iter.mgl:736](../../source/std/iter.mgl#L736)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-48"></a>

### IterChainState::next（49）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:753](../../source/std/iter.mgl#L753)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-49"></a>

### IterChainState::sizeHint（50）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:767](../../source/std/iter.mgl#L767)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-50"></a>

### chain（51）

**声明**

```magel
Iter<T> chain<T>(Iter<T> left, Iter<T> right);
```

**行为**

先输出第一源，再输出第二源。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `Iter<T> left` | 左侧来源。 | 无 |
| `right` | `Iter<T> right` | 右侧来源。 | 无 |

**返回值**

`Iter<T>`。先输出第一源，再输出第二源。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::chain<T>(left, right);
```

**源码**：[source/std/iter.mgl:779](../../source/std/iter.mgl#L779)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-51"></a>

### IterZipState::next（52）

**声明**

```magel
Option<(T,U)> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<(T,U)>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:794](../../source/std/iter.mgl#L794)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-52"></a>

### IterZipState::sizeHint（53）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:809](../../source/std/iter.mgl#L809)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-53"></a>

### zip（54）

**声明**

```magel
Iter<(T,U)> zip<T,U>(Iter<T> left, Iter<U> right);
```

**行为**

将两个源配对，较短源结束时终止。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `Iter<T> left` | 左侧来源。 | 无 |
| `right` | `Iter<U> right` | 右侧来源。 | 无 |

**返回值**

`Iter<(T,U)>`。将两个源配对，较短源结束时终止。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::zip<T,U>(left, right);
```

**源码**：[source/std/iter.mgl:821](../../source/std/iter.mgl#L821)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-54"></a>

### IterEnumerateState::next（55）

**声明**

```magel
Option<(int64,T)> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<(int64,T)>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:836](../../source/std/iter.mgl#L836)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-55"></a>

### IterEnumerateState::sizeHint（56）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:849](../../source/std/iter.mgl#L849)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-56"></a>

### enumerate（57）

**声明**

```magel
Iter<(int64,T)> enumerate<T>(Iter<T> source);
```

**行为**

给每项附加从 0 开始的序号。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<(int64,T)>`。给每项附加从 0 开始的序号。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::enumerate<T>(source);
```

**源码**：[source/std/iter.mgl:860](../../source/std/iter.mgl#L860)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-57"></a>

### IterStepState::next（58）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:875](../../source/std/iter.mgl#L875)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-58"></a>

### IterStepState::sizeHint（59）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:886](../../source/std/iter.mgl#L886)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-59"></a>

### stepBy（60）

**声明**

```magel
Iter<T> stepBy<T>(Iter<T> source, int64 step);
```

**行为**

按指定步长抽取元素，步长必须为正。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `step` | `int64 step` | 步长。 | 无 |

**返回值**

`Iter<T>`。按指定步长抽取元素，步长必须为正。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`step > 0, "stepBy 步长必须大于零"`。

**调用形式**

```magel
std::iter::stepBy<T>(source, step);
```

**源码**：[source/std/iter.mgl:898](../../source/std/iter.mgl#L898)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-60"></a>

### IterFuseState::next（61）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:914](../../source/std/iter.mgl#L914)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-61"></a>

### IterFuseState::sizeHint（62）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:928](../../source/std/iter.mgl#L928)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-62"></a>

### fuse（63）

**声明**

```magel
Iter<T> fuse<T>(Iter<T> source);
```

**行为**

一旦 next 返回 None，后续始终 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<T>`。一旦 next 返回 None，后续始终 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::fuse<T>(source);
```

**源码**：[source/std/iter.mgl:942](../../source/std/iter.mgl#L942)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-63"></a>

### IterInspectState::next（64）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:956](../../source/std/iter.mgl#L956)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-64"></a>

### IterInspectState::sizeHint（65）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:967](../../source/std/iter.mgl#L967)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-65"></a>

### inspect（66）

**声明**

```magel
Iter<T> inspect<T>(Iter<T> source, void fn(T) action);
```

**行为**

遍历时先执行观察回调，再输出原项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `action` | `void fn(T) action` | 元素处理函数。 | 无 |

**返回值**

`Iter<T>`。遍历时先执行观察回调，再输出原项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::inspect<T>(source, action);
```

**源码**：[source/std/iter.mgl:979](../../source/std/iter.mgl#L979)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-66"></a>

### IterCycleState::next（67）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:995](../../source/std/iter.mgl#L995)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-67"></a>

### IterCycleState::sizeHint（68）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:1014](../../source/std/iter.mgl#L1014)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-68"></a>

### cycle（69）

**声明**

```magel
Iter<T> cycle<T>(Iter<T> source);
```

**行为**

循环输出源的缓冲副本；空源保持为空。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<T>`。循环输出源的缓冲副本；空源保持为空。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::cycle<T>(source);
```

**源码**：[source/std/iter.mgl:1025](../../source/std/iter.mgl#L1025)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-69"></a>

### IterScanState::next（70）

**声明**

```magel
Option<U> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<U>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:1041](../../source/std/iter.mgl#L1041)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-70"></a>

### IterScanState::sizeHint（71）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:1052](../../source/std/iter.mgl#L1052)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-71"></a>

### scan（72）

**声明**

```magel
Iter<U> scan<T,A,U>(Iter<T> source, A initial, Option<U> fn(A mut&, T) callback);
```

**行为**

以可变状态调用回调，产生 Option 输出。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `initial` | `A initial` | 初始值或累加状态。 | 无 |
| `callback` | `Option<U> fn(A mut&, T) callback` | 处理元素或累加状态的函数。 | 无 |

**返回值**

`Iter<U>`。以可变状态调用回调，产生 Option 输出。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::scan<T,A,U>(source, initial, callback);
```

**源码**：[source/std/iter.mgl:1066](../../source/std/iter.mgl#L1066)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-72"></a>

### collect（73）

**声明**

```magel
std::lang::List<T> collect<T>(Iter<T> source);
```

**行为**

消费迭代器，收集到独立 List。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`std::lang::List<T>`。消费迭代器，收集到独立 List。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::iter::collect<T>(source);
```

**源码**：[source/std/iter.mgl:1080](../../source/std/iter.mgl#L1080)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-73"></a>

### fold（74）

**声明**

```magel
A fold<T,A>(Iter<T> source, A initial, A fn(A, T) callback);
```

**行为**

从 initial 开始按顺序累积。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `initial` | `A initial` | 初始值或累加状态。 | 无 |
| `callback` | `A fn(A, T) callback` | 处理元素或累加状态的函数。 | 无 |

**返回值**

`A`。从 initial 开始按顺序累积。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::iter::fold<T,A>(source, initial, callback);
```

**源码**：[source/std/iter.mgl:1100](../../source/std/iter.mgl#L1100)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-74"></a>

### tryFold（75）

**声明**

```magel
Result<A,E> tryFold<T,A,E>(Iter<T> source, A initial, Result<A,E> fn(A, T) callback);
```

**行为**

累积到首个 Err 时短路返回，否则 Ok。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `initial` | `A initial` | 初始值或累加状态。 | 无 |
| `callback` | `Result<A,E> fn(A, T) callback` | 处理元素或累加状态的函数。 | 无 |

**返回值**

`Result<A,E>`。累积到首个 Err 时短路返回，否则 Ok。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::iter::tryFold<T,A,E>(source, initial, callback);
```

**源码**：[source/std/iter.mgl:1120](../../source/std/iter.mgl#L1120)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-75"></a>

### findMap（76）

**声明**

```magel
Option<U> findMap<T,U>(Iter<T> source, Option<U> fn(T) callback);
```

**行为**

转换并返回首个 Value。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `callback` | `Option<U> fn(T) callback` | 处理元素或累加状态的函数。 | 无 |

**返回值**

`Option<U>`。转换并返回首个 Value。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::iter::findMap<T,U>(source, callback);
```

**源码**：[source/std/iter.mgl:1143](../../source/std/iter.mgl#L1143)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-76"></a>

### sameItems（77）

**声明**

```magel
bool sameItems<T>(Iter<T> left, Iter<T> right);
```

**行为**

逐项比较两个源的值及长度是否一致。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `Iter<T> left` | 左侧来源。 | 无 |
| `right` | `Iter<T> right` | 右侧来源。 | 无 |

**返回值**

`bool`。逐项比较两个源的值及长度是否一致。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::iter::sameItems<T>(left, right);
```

**源码**：[source/std/iter.mgl:1164](../../source/std/iter.mgl#L1164)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-77"></a>

### IterBufferState::next（78）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

读取下一项。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项，耗尽返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:1185](../../source/std/iter.mgl#L1185)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-78"></a>

### IterBufferState::sizeHint（79）

**声明**

```magel
(int64,Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

返回剩余数量。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64,Option<int64>)`。相等的下界与上界。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:1191](../../source/std/iter.mgl#L1191)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-79"></a>

### rev（80）

**声明**

```magel
Iter<T> rev<T>(Iter<T> source);
```

**行为**

产生逆序结果；实现会缓冲输入，不能用于无限源。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<T>`。产生逆序结果；实现会缓冲输入，不能用于无限源。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::rev<T>(source);
```

**源码**：[source/std/iter.mgl:1202](../../source/std/iter.mgl#L1202)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-80"></a>

### peekingTakeWhile（81）

**声明**

```magel
(Iter<T>,Iter<T>) peekingTakeWhile<T>(Iter<T> source, bool fn(T) predicate);
```

**行为**

分离满足谓词的前缀与剩余迭代器，失败项保留在剩余部分。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> source` | 来源游标、容器或连续存储。 | 无 |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`(Iter<T>,Iter<T>)`。分离满足谓词的前缀与剩余迭代器，失败项保留在剩余部分。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::peekingTakeWhile<T>(source, predicate);
```

**源码**：[source/std/iter.mgl:1225](../../source/std/iter.mgl#L1225)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-81"></a>

### countIter（82）

**声明**

```magel
int64 countIter<T>(Iter<T> mut& source);
```

**行为**

消费并返回项数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> mut& source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`int64`。消费并返回项数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::countIter<T>(source);
```

**源码**：[source/std/iter.mgl:1244](../../source/std/iter.mgl#L1244)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-82"></a>

### nth（83）

**声明**

```magel
Option<T> nth<T>(Iter<T> mut& source, int64 index);
```

**行为**

消费到从 0 开始的第 index 项，返回 Option。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> mut& source` | 来源游标、容器或连续存储。 | 无 |
| `index` | `int64 index` | 从零开始的下标。 | 无 |

**返回值**

`Option<T>`。消费到从 0 开始的第 index 项，返回 Option。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::nth<T>(source, index);
```

**源码**：[source/std/iter.mgl:1255](../../source/std/iter.mgl#L1255)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-83"></a>

### last（84）

**声明**

```magel
Option<T> last<T>(Iter<T> mut& source);
```

**行为**

消费全部剩余项并返回最后一项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> mut& source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Option<T>`。消费全部剩余项并返回最后一项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::last<T>(source);
```

**源码**：[source/std/iter.mgl:1265](../../source/std/iter.mgl#L1265)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-84"></a>

### all（85）

**声明**

```magel
bool all<T>(Iter<T> mut& source, bool fn(T) predicate);
```

**行为**

消费并判断是否全部满足谓词，空源为 true。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> mut& source` | 来源游标、容器或连续存储。 | 无 |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`bool`。消费并判断是否全部满足谓词，空源为 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::all<T>(source, predicate);
```

**源码**：[source/std/iter.mgl:1276](../../source/std/iter.mgl#L1276)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-85"></a>

### any（86）

**声明**

```magel
bool any<T>(Iter<T> mut& source, bool fn(T) predicate);
```

**行为**

消费并判断是否存在满足谓词的项，空源为 false。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> mut& source` | 来源游标、容器或连续存储。 | 无 |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`bool`。消费并判断是否存在满足谓词的项，空源为 false。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::any<T>(source, predicate);
```

**源码**：[source/std/iter.mgl:1287](../../source/std/iter.mgl#L1287)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-86"></a>

### find（87）

**声明**

```magel
Option<T> find<T>(Iter<T> mut& source, bool fn(T) predicate);
```

**行为**

消费到首个 predicate 为真的项并返回 Option。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> mut& source` | 来源游标、容器或连续存储。 | 无 |
| `predicate` | `bool fn(T) predicate` | 元素筛选条件。 | 无 |

**返回值**

`Option<T>`。消费到首个 predicate 为真的项并返回 Option。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::find<T>(source, predicate);
```

**源码**：[source/std/iter.mgl:1298](../../source/std/iter.mgl#L1298)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-87"></a>

### forEach（88）

**声明**

```magel
void forEach<T>(Iter<T> mut& source, void fn(T) action);
```

**行为**

逐项调用 action 并消费迭代器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `Iter<T> mut& source` | 来源游标、容器或连续存储。 | 无 |
| `action` | `void fn(T) action` | 元素处理函数。 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::forEach<T>(source, action);
```

**源码**：[source/std/iter.mgl:1309](../../source/std/iter.mgl#L1309)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-88"></a>

### listIntoIter（89）

**声明**

```magel
Iter<T> listIntoIter<T>(std::lang::List<T> mut& source);
```

**行为**

取得拥有列表值的迭代器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::List<T> mut& source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<T>`。取得拥有列表值的迭代器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::listIntoIter<T>(source);
```

**源码**：[source/std/iter.mgl:1319](../../source/std/iter.mgl#L1319)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-89"></a>

### fromRange（90）

**声明**

```magel
Iter<int64> fromRange(std::lang::__Range source);
```

**行为**

将核心整数 range 转为通用 Iter。

将 std::lang::range 转为独立有状态游标。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::__Range source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<int64>`。将核心整数 range 转为通用 Iter。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::fromRange(source);
```

**源码**：[source/std/iter.mgl:1331](../../source/std/iter.mgl#L1331)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-90"></a>

### IterConstListState::next（91）

**声明**

```magel
Option<T const&> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T const&>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:1372](../../source/std/iter.mgl#L1372)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-91"></a>

### IterConstListState::sizeHint（92）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:1384](../../source/std/iter.mgl#L1384)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-92"></a>

### IterMutListState::next（93）

**声明**

```magel
Option<T mut&> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T mut&>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:1396](../../source/std/iter.mgl#L1396)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-93"></a>

### IterMutListState::sizeHint（94）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:1408](../../source/std/iter.mgl#L1408)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-94"></a>

### listIter（95）

**声明**

```magel
Iter<T const&> listIter<T>(std::lang::List<T> const& source);
```

**行为**

取得只读元素引用的迭代器。

借用列表，不复制元素，next 返回只读引用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::List<T> const& source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<T const&>`。取得只读元素引用的迭代器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::listIter<T>(source);
```

**源码**：[source/std/iter.mgl:1420](../../source/std/iter.mgl#L1420)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-95"></a>

### listIterMut（96）

**声明**

```magel
Iter<T mut&> listIterMut<T>(std::lang::List<T> mut& source);
```

**行为**

取得可写元素引用的迭代器。

借用可写列表，next 返回可写引用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::List<T> mut& source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<T mut&>`。取得可写元素引用的迭代器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::listIterMut<T>(source);
```

**源码**：[source/std/iter.mgl:1432](../../source/std/iter.mgl#L1432)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-96"></a>

### setIntoIter（97）

**声明**

```magel
Iter<T> setIntoIter<T>(std::lang::Set<T> mut& source);
```

**行为**

取得集合值的迭代器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::Set<T> mut& source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<T>`。取得集合值的迭代器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::setIntoIter<T>(source);
```

**源码**：[source/std/iter.mgl:1465](../../source/std/iter.mgl#L1465)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-97"></a>

### setIter（98）

**声明**

```magel
Iter<T> setIter<T>(std::lang::Set<T> const& source);
```

**行为**

借用集合构造迭代器：把元素快照进迭代器状态，来源集合保持不变；迭代器本身仍是一次性消费。

借用集合建立迭代器：把元素快照进迭代器状态，来源集合保持不变。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::Set<T> const& source` | 来源集合，只读借用。 | 无 |

**返回值**

`Iter<T>`。独立有状态的元素迭代器；迭代器本身仍是一次性消费。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::setIter<T>(source);
```

**源码**：[source/std/iter.mgl:1485](../../source/std/iter.mgl#L1485)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-98"></a>

### mapIntoIter（99）

**声明**

```magel
Iter<Pair<K,V>> mapIntoIter<K,V>(std::lang::Map<K,V> mut& source);
```

**行为**

取得映射键值对的迭代器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::Map<K,V> mut& source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<Pair<K,V>>`。取得映射键值对的迭代器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::mapIntoIter<K,V>(source);
```

**源码**：[source/std/iter.mgl:1504](../../source/std/iter.mgl#L1504)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-99"></a>

### mapIter（100）

**声明**

```magel
Iter<Pair<K,V>> mapIter<K,V>(std::lang::Map<K,V> const& source);
```

**行为**

借用映射构造键值对元组迭代器：把键值对快照进迭代器状态，来源映射保持不变；迭代器本身仍是一次性消费。

借用映射建立迭代器：把 Pair<K,V> 键值对快照进迭代器状态，来源映射保持不变。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::Map<K,V> const& source` | 来源映射，只读借用。 | 无 |

**返回值**

`Iter<Pair<K,V>>`。独立有状态的 Pair<K,V> 迭代器；迭代器本身仍是一次性消费。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::mapIter<K,V>(source);
```

**源码**：[source/std/iter.mgl:1524](../../source/std/iter.mgl#L1524)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-100"></a>

### IterSpanState::next（101）

**声明**

```magel
Option<T const&> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T const&>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:1547](../../source/std/iter.mgl#L1547)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-101"></a>

### IterSpanState::sizeHint（102）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:1559](../../source/std/iter.mgl#L1559)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-102"></a>

### fromSpan（103）

**声明**

```magel
Iter<T const&> fromSpan<T>(T const& source, int64 length);
```

**行为**

从连续存储视图构造迭代器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `T const& source` | 来源游标、容器或连续存储。 | 无 |
| `length` | `int64 length` | 连续存储的有效元素数量。 | 无 |

**返回值**

`Iter<T const&>`。从连续存储视图构造迭代器。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`length >= 0, "迭代器长度不能为负数"`。

**调用形式**

```magel
std::iter::fromSpan<T>(source, length);
```

**源码**：[source/std/iter.mgl:1571](../../source/std/iter.mgl#L1571)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-103"></a>

### IterCharsState::next（104）

**声明**

```magel
Option<char> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<char>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:1587](../../source/std/iter.mgl#L1587)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-104"></a>

### IterCharsState::sizeHint（105）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:1599](../../source/std/iter.mgl#L1599)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-105"></a>

### chars（106）

**声明**

```magel
Iter<char> chars(std::lang::String source);
```

**行为**

按字符串字节构造 char 迭代器，非 Unicode 字素切分。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `std::lang::String source` | 来源游标、容器或连续存储。 | 无 |

**返回值**

`Iter<char>`。按字符串字节构造 char 迭代器，非 Unicode 字素切分。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::chars(source);
```

**源码**：[source/std/iter.mgl:1610](../../source/std/iter.mgl#L1610)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-106"></a>

### IterRepeatState::next（107）

**声明**

```magel
Option<T> next();
```

**行为**

消费并返回下一项，当前无项时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。下一项；耗尽时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.next();
```

**源码**：[source/std/iter.mgl:1623](../../source/std/iter.mgl#L1623)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-107"></a>

### IterRepeatState::sizeHint（108）

**声明**

```magel
(int64, Option<int64>) sizeHint();
```

**行为**

返回剩余数量下界及可选上界；未知上界为 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`(int64, Option<int64>)`。返回剩余数量下界及可选上界；未知上界为 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeHint();
```

**源码**：[source/std/iter.mgl:1630](../../source/std/iter.mgl#L1630)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。

<a id="api-108"></a>

### repeat（109）

**声明**

```magel
Iter<T> repeat<T>(T value);
```

**行为**

构造无限重复 value 的源，通常搭配 take。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 元素值。 | 无 |

**返回值**

`Iter<T>`。构造无限重复 value 的源，通常搭配 take。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::iter::repeat<T>(value);
```

**源码**：[source/std/iter.mgl:1640](../../source/std/iter.mgl#L1640)。

**完整示例**：[example/samples/iter_pipeline.mgl](../../example/samples/iter_pipeline.mgl)。
