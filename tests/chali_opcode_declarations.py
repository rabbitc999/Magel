"""验证 opcode 声明完整性及任意数量的 opcode 值字段。"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """用当前编译器检查正常声明与必须拒绝的复制行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', default='ChaLi.exe')
    options = parser.parse_args()
    compiler = str(Path(options.chali).resolve())
    declarations = '''
opcode First { new = __std_bint_from_int; drop = __std_bint_drop; copy = __std_bint_copy; };
opcode Second { new = __std_bint_from_int; drop = __std_bint_drop; copy = __std_bint_copy; };
'''
    unique = '#NonCopyable\nopcode Unique { new = __std_bint_from_int; drop = __std_bint_drop; };\n'
    cases = [
        ('one', declarations + 'struct One { First value; int64 count; bool ready; };', None),
        ('nested', declarations + 'struct Left { First value; }; struct Right { Second value; }; struct Pair { Left left; Right right; };', None),
        ('generic_one', declarations + 'struct One<T> { T value; int64 count; }; One<First> value;', None),
        ('same_type', declarations + 'struct Invalid { First first; First second; };', None),
        ('different_types', declarations + 'struct Invalid { First first; Second second; };', None),
        ('builtin_types', 'struct Invalid { BigInteger first; BigDecimal second; };', None),
        ('generic_two', declarations + 'struct Invalid<T> { T first; T second; }; Invalid<First> value;', None),
        ('static_and_instance', declarations + 'struct Invalid { static First first; Second second; };', None),
        ('string_pair', 'struct Pair { String first; String second; };', None),
        ('no_string_alias', 'string value = "legacy";', 'string'),
        ('handle_readonly', declarations + 'void invalid() { First value; value.opcode = 0; }', 'const'),
        ('handle_created_readonly', declarations + 'void invalid() { First value = First::new(1); value.opcode = 0; }', 'const'),
        ('handle_compound_readonly', declarations + 'void invalid() { First value = First::new(1); value.opcode += 1; }', 'const'),
        ('handle_increment_readonly', declarations + 'void invalid() { First value = First::new(1); value.opcode++; }', 'const'),
        ('handle_const_borrow', declarations + 'void valid() { First value; int64 const& alias = &value.opcode; }', None),
        ('handle_mutable_borrow', declarations + 'void invalid() { First value; int64 mut& alias = &value.opcode; }', 'int64 const& -> int64 mut&'),
        ('missing_drop', 'opcode Invalid { new = __std_bint_from_int; copy = __std_bint_copy; };', '必须声明 new 和 drop'),
        ('missing_copy', 'opcode Invalid { new = __std_bint_from_int; drop = __std_bint_drop; };', '必须声明 copy'),
        ('noncopyable_move', unique + 'void consume(Unique value) {} void transfer() { Unique first; Unique second = move(&first); consume(move(&second)); }', None),
        ('noncopyable_move_return', unique + 'Unique relay(Unique value) { return move(&value); }', None),
        ('noncopyable_moved_read', unique + 'void transfer() { Unique first; Unique second = move(&first); Unique const& invalid = &first; }', 'CHL1205'),
        ('noncopyable_moved_twice', unique + 'void transfer() { Unique first; Unique second = move(&first); Unique third = move(&first); }', 'CHL1205'),
        ('noncopyable_container', unique + 'void invalid() { Unique value; List<Unique> items; items.push(value); }', '禁止复制不可复制的值'),
        ('noncopyable_const_move', unique + 'void invalid(Unique const& value) { Unique moved = move(value); }', '只接受可写引用'),
        ('noncopyable_value_argument', unique + 'void consume(Unique value) {} void invalid() { Unique value; consume(value); }', '禁止按值复制实参'),
        ('file_field_copy', 'struct Box { File handle; }; void invalid() { File original; Box box; box.handle = original; }', '禁止复制到字段或数组元素'),
        ('file_array_copy', 'void invalid() { File original; File handles[1]; handles[0] = original; }', '禁止复制到字段或数组元素'),
        ('noncopyable_copy', unique + 'Unique original; Unique copied = original;', '禁止复制初始化或赋值'),
        ('noncopyable_return', unique + 'Unique borrowed(Unique const& value) { return value; }', '禁止复制返回借用值'),
        ('noncopyable_aggregate', unique + 'struct Box { Unique value; }; Box original; Box copied = original;', '不可复制的值禁止复制：'),
        ('noncopyable_member_fields', '#NonCopyable opcode Unique {new=__std_bint_from_int;drop=__std_bint_drop;int64 count;String label;}; Unique original; Unique copied=original;', '不可复制的值禁止复制：'),
    ]
    with tempfile.TemporaryDirectory(prefix='chali-opcode-') as directory:
        for name, source, diagnostic in cases:
            path = Path(directory, name + '.chl')
            path.write_text(source + '\nint64 main() { return 0; }\n', encoding='utf-8')
            result = subprocess.run([compiler, '--check', str(path)], capture_output=True,
                                    encoding='utf-8', errors='replace', timeout=60)
            output = result.stdout + result.stderr
            if diagnostic is None:
                assert result.returncode == 0, f'{name}: {output}'
            else:
                assert result.returncode != 0 and diagnostic in output, f'{name}: {output}'
        module = Path(directory, 'vault.chl')
        module.write_text('module vault;\nopcode Secret {new=__std_bint_from_int;drop=__std_bint_drop;copy=__std_bint_copy;int64 count;}; int64 read(Secret const& value){return value.opcode;}', encoding='utf-8')
        source = Path(directory, 'private_handle.chl')
        source.write_text('import vault;\nint64 main(){vault::Secret value; return value.opcode;}', encoding='utf-8')
        result = subprocess.run([compiler, '--check', str(source)], capture_output=True,encoding='utf-8', errors='replace',timeout=60)
        assert result.returncode != 0 and '内部句柄仅允许在声明模块中访问' in result.stdout+result.stderr, result.stdout+result.stderr
    print(f'opcode declarations: {len(cases) + 1} checks passed')


if __name__ == '__main__':
    main()
