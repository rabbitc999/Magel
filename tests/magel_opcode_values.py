"""定向验证 opcode 值语义、红黑删除与字符串方法；其他库固定种子抽样。"""
import concurrent.futures
import json
import os
from pathlib import Path
import random
import subprocess

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('MAGEL_COMPILER', ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))).resolve()
WORK = ROOT / 'build' / 'opcode-values'


def execute(name, source, *, threads=False, failure=None):
    """在独立输出路径执行一个正例或诊断反例。"""
    path = WORK / (name + '.mgl')
    if isinstance(source, Path):
        path = source
    else:
        path.write_text(source, encoding='utf-8')
    command = [str(COMPILER), 'run', str(path), '--out', str(WORK / name)]
    if threads:
        command.append('--threads')
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=120,
                            env=dict(os.environ, MAGEL_LANG='zh'))
    for stream in ('stdout', 'stderr'):
        value = getattr(result, stream)
        try:
            text = value.decode('utf-8')
        except UnicodeDecodeError:
            text = value.decode('gb18030', errors='replace')
        setattr(result, stream, text)
    if failure is None:
        assert result.returncode == 0 and not result.stderr, (name, result.stdout, result.stderr)
    else:
        assert result.returncode != 0 and any(word in result.stderr for word in failure), (name, result.stdout, result.stderr)
    return name, result.stdout.strip()


def string_differential():
    """以 Python 字符串结果为独立参考生成固定种子差分。"""
    rng = random.Random(20261021)
    literal = lambda text: json.dumps(text, ensure_ascii=False)
    statements = []
    for _ in range(120):
        text = ''.join(rng.choices('ab cdXY09', k=rng.randrange(25)))
        needle = ''.join(rng.choices('abX', k=rng.randrange(1, 4)))
        replacement = ''.join(rng.choices('xy', k=rng.randrange(4)))
        first, count = rng.randrange(-3, 30), rng.randrange(-1, 30)
        start = max(0, first)
        sliced = text[start:] if count < 0 else text[start:start + count]
        statements.extend([
            f'assert({literal(text)}.substring({first}, {count}) == {literal(sliced)});',
            f'assert({literal(text)}.indexOf({literal(needle)}) == {text.find(needle)});',
            f'assert({literal(text)}.lastIndexOf({literal(needle)}) == {text.rfind(needle)});',
            f'assert({literal(text)}.replaceAll({literal(needle)}, {literal(replacement)}) == {literal(text.replace(needle, replacement))});',
            f'assert({literal(text)}.count({literal(needle)}) == {text.count(needle)});',
            f'assert({literal(text)}.trim() == {literal(text.strip())});',
            f'assert({literal(text)}.toUpper() == {literal(text.upper())});',
            f'assert("|".join({literal(text)}.split("a")) == {literal("|".join(text.split("a")))});',
        ])
    for _ in range(30):
        text = ''.join(rng.choices('你好🙂éa', k=rng.randrange(16)))
        statements.extend([
            f'assert({literal(text)}.charLength() == {len(text)});',
            f'assert({literal(text)}.reverse() == {literal(text[::-1])});',
            f'assert("".join({literal(text)}.unicodeChars()) == {literal(text)});',
        ])
    return 'int64 main() {\n' + '\n'.join(statements) + '\nreturn 0;\n}'


def main():
    """并行运行互不共享构建资产的定向案例，最后记录容量基准。"""
    WORK.mkdir(parents=True, exist_ok=True)
    cases = []
    for name in ('std_red_black_delete', 'string_methods', 'opcode_value_semantics', 'std_tree',
                 'std_linkedlist', 'std_graph', 'std_structured_data', 'std_math_clock_random',
                 'std_parse_random_path_memory', 'regex_limits', 'event_timer'):
        cases.append((name, ROOT / 'example' / 'samples' / (name + '.mgl'), {}))
    cases.append(('typed_messages_shared', ROOT / 'example/samples/typed_messages_shared.mgl', {'threads': True}))
    cases.append(('string_differential', string_differential(), {}))
    cases.append(('generic_function_comparisons', '''import std::math;
T identity<T>(T value) { return value; }
int64 main() {
    int64 round = 1; int64 powers = 7;
    while (round < powers) { round = round + 1; }
    assert(round == 7);
    if (round < 9 && powers > 2) { powers = powers + 1; }
    assert(powers == 8);
    let function = identity<int64>;
    assert(function(3) == 3);
    assert(std::math::round<float64>(1.5) == 2.0);
    return 0;
}''', {}))
    cases.append(('owner_move_and_scope', '''import std::sql;
import std::memory;
import std::event;
int64 main() {
    int64 index = 0;
    while (index < 1100) {
        Database connection = Database::sqlite(":memory:");
        Database moved = move(&connection);
        assert(moved.isOpen());
        assert(moved.exec("select 42").count() == 42);
        index = index + 1;
    }
    Arena original = Arena::create(); Arena moved = move(&original);
    assert(moved.allocate(8).len() == 8); moved.reset();
    Timer timer = Timer::afterMillis(60000); Timer timerMoved = move(&timer);
    assert(timerMoved.isValid()); timerMoved.stop(); timerMoved.stop();
    assert(!timerMoved.isValid()); return 0;
}''', {}))
    for name, module, declaration in (
        ('database_copy', 'std::sql', 'Database original = Database::sqlite(":memory:"); Database duplicate = original;'),
        ('arena_copy', 'std::memory', 'Arena original = Arena::create(); Arena duplicate = original;'),
        ('timer_copy', 'std::event', 'Timer original = Timer::afterMillis(10); Timer duplicate = original;'),
    ):
        cases.append((name, f'import {module};\nint64 main() {{ {declaration} return 0; }}',
                      {'failure': ('NonCopyable', '不可复制', '不能复制', 'move')}))
    for name, expression in (('negative_repeat', '"a".repeat(0 - 1)'), ('negative_padding', '"a".padStart(0 - 1)')):
        cases.append((name, f'int64 main() {{ String value = {expression}; return 0; }}', {'failure': ('String',)}))
    cases.append(('structured_closed', ROOT / 'example/samples-error/structured_closed.mgl', {'failure': ('JsonValue.kind',)}))
    cases.append(('invalid_utf8', 'int64 main() { String text = "你".substring(1); int64 count = text.charLength(); return 0; }', {'failure': ('UTF-8',)}))
    cases.append(('arena_reset_invalidates', '''import std::memory;
int64 main() {
    Arena arena = Arena::create(); let span = arena.allocate(8);
    char mut& first = span.iterator(0).unwrap(); first = 'x';
    arena.reset(); println("{}", first); return 0;
}''', {'failure': ('180143985', 'free', '越界')}))
    cases.append(('union_find_bounds', 'import std::graph;\nint64 main() { UnionFind sets = UnionFind::create(2); sets.find(2); return 0; }', {'failure': ('UnionFind.find',)}))
    cases.append(('graph_bounds', 'import std::graph;\nint64 main() { Graph graph = Graph::create(2, false); graph.addEdge(0, 2, 1); return 0; }', {'failure': ('Graph.addEdge',)}))
    cases.append(('random_range', 'import std::random;\nint64 main() { Random value = Random::withSeed((uint64)1); value.nextInt(3, 3); return 0; }', {'failure': ('Random.nextInt',)}))
    rng = random.Random(20261021)
    for name in rng.sample(['std_algorithms_extra', 'std_complex_fft', 'std_linalg_solve_glue',
                            'std_numerics_number_theory', 'std_binary', 'std_collections_algorithms'], 3):
        cases.append(('spot_' + name, ROOT / 'example/samples' / (name + '.mgl'), {}))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(execute, name, source, **options) for name, source, options in cases]
        for future in concurrent.futures.as_completed(futures):
            name, output = future.result()
            print(name, 'OK', output[:120])
    source = '''import std::graph;
import std::clock;
int64 measure(int64 count) {
    UnionFind sets = UnionFind::create(count);
    sets.unionSets(0, count - 1);
    int64 started = std::clock::nowNanos(); int64 sum = 0; int64 index = 0;
    while (index < 30000) { sum = sum + sets.find(count - 1); index = index + 1; }
    int64 elapsed = std::clock::nowNanos() - started; assert(sum == 0); return elapsed;
}
int64 main() { println("union-find query ns: small={} large={}", measure(1000), measure(1000000)); return 0; }
'''
    _, timing = execute('union_find_capacity', source)
    (WORK / 'capacity.txt').write_text(timing + '\n', encoding='utf-8')
    print(timing)
    print(f'{len(cases)} targeted/spot cases, 1050 string differential assertions, capacity benchmark OK')


if __name__ == '__main__':
    main()
