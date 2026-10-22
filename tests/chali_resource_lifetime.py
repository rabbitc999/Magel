"""仅通过 VM 验证表达式临时值、严格释放和分支上的自动清理。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    """检查可静态识别的重复销毁，以及跨调用释放的运行时兜底。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', type=Path, default=Path('ChaLi.exe'))
    compiler = parser.parse_args().chali.resolve()
    with tempfile.TemporaryDirectory(prefix='chali-release-') as directory:
        work = Path(directory)
        sequence = 0

        def run(source, success=True, diagnostic=None):
            """执行独立源码并检查退出状态、泄漏与指定诊断。"""
            nonlocal sequence
            sequence += 1
            path = work / f'case_{sequence}.chl'
            path.write_text(source, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(path), '--out', str(work / f'out_{sequence}')],
                                    cwd=work, capture_output=True, encoding='utf-8', errors='replace',
                                    env=dict(os.environ, CHALI_LANG='zh'), timeout=120)
            assert (result.returncode == 0) == success, (sequence, source, result)
            if success:
                assert not result.stderr, (sequence, result.stderr)
            if diagnostic:
                assert diagnostic in result.stderr, (sequence, result.stderr)
            return result

        run('''int64 fn(int64) make(int64 mut& bias) {
            return fn(int64 value) { value + bias };
        }
        void main() {
            int64 bias = 85;
            int64 fn(int64) callback = make(&bias);
            List<(int64 fn(int64)) const&> references;
            references.push(&callback);
            assert(references[0](5) == 90);
            assert(callback(6) == 91);
        }''')
        run('''void main() {
            String owner = "hello";
            List<String const&> references;
            references.push(&owner);
            assert(references[0] == "hello");
            assert(owner == "hello");
        }''')
        result = run('''void main() {
            String a = "text";
            int64 i = 0;
            while (i < 50) {
                println("{}", a + "\\n");
                println("{1}:{1}", a + "!");
                String b = format("{}", a + "?");
                assert(b == "text?");
                i++;
            }
        }''')
        assert result.stdout == ('text\n\ntext!:text!\n' * 50), result.stdout
        for allocation, disposal in [
                ('int64 mut& value = heap int64;', 'value.drop();'),
                ('int64 value[] = new[4];', 'value.drop();'),
                ('List<int64> value = [];', 'value.drop();'),
                ('String value = \"owned\";', 'value.drop();')]:
            run('void main() { ' + allocation + disposal + disposal + ' }')
        for helper, allocation, argument in [
                ('void dispose(int64 mut& value) { value.drop(); }', 'int64 mut& value = heap int64;', 'value'),
                ('struct Item { int64 number; }; void dispose(Item mut& value) { value.drop(); }', 'Item mut& value = heap Item;', 'value'),
                ('void dispose(String[] mut& value) { value.drop(); }', 'String value[] = new[2]; value[0] = "owned";', '&value')]:
            run(helper + '\nvoid main() { ' + allocation + f'dispose({argument}); dispose({argument});' + ' }', False, 'CHL1205')
        for condition in ('true', 'false'):
            run('''void release(bool condition) {
                List<int64> value = [];
                if (condition) { value.drop(); }
            }
            void main() { release(''' + condition + '''); }''')
        for condition in ('true', 'false'):
            run('void release(bool condition) { int64 values[] = new[2]; if (condition) { values.drop(); } values.drop(); values = new[3]; values[2] = 7; assert(values[2] == 7); values.drop(); } void main() { release(' + condition + '); }')
        run('int64 calls = 0; struct Counter { int64 drop() { calls++; return calls; } }; void main() { Counter value; assert(value.drop() == 1); assert(value.drop() == 2); }')
        run('int64 calls = 0; int64 index() { calls++; return 0; } struct Holder { List<int64> values; }; void main() { Holder owner; owner.values.push(1); owner.values.drop(); owner.values = [2]; assert(owner.values[0] == 2); List<int64> values[1]; values[index()].drop(); assert(calls == 1); values[0] = [3]; }')
        run('void main() { int64 values[] = new[2]; values.drop(); println("{}", values[0]); }', False, 'CHL1205')
        run('int64 closes = 0; int64 drops = 0; struct CloseOnly : Closeable { void close() { closes++; } }; struct Resource : Closeable, Drop { void close() { closes++; } void drop() { drops++; } }; struct Parent : Drop { CloseOnly child; void drop() { drops++; } }; void closeValue(bool condition) { CloseOnly value; if (condition) { value.close(); } value.drop(); } void dropValue(bool condition) { Resource value; if (condition) { value.close(); } value.drop(); value.drop(); } void main() { closeValue(true); closeValue(false); dropValue(true); dropValue(false); { Parent value; value.drop(); } assert(closes == 5 && drops == 3); }')
        run('void main() { int64 mut& p = heap int64; delete p; }', False)
        run('void main() { int64 mut& p = heap int64; drop p; }', False)
        run('struct Resource : Closeable { void close() {} }; struct Owner { Resource child; }; void dispose(Owner const& owner) { owner.child.drop(); } void main() {}', False)
        run('struct Item : Drop { int64 value; void drop() {} }; void main() { Item mut& source = heap Item; source.value = 7; Item result = move(source); source.drop(); assert(result.value == 7); }')
        run('void main() { int64 mut& source = heap int64; let take = fn move[source]() { source }; source.drop(); }', False, 'CHL1205')
        run('void main() { std::lang::sweep(); }', False)
        print(f'[通过] {sequence} 项临时字符串、重复释放、跨调用检查和分支清理回归', flush=True)


if __name__ == '__main__':
    main()
