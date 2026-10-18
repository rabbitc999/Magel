"""验证 PostgreSQL libpq ABI 夹具与真实 ODBC 数据源；不要求网络数据库服务。"""
from pathlib import Path
import json
import os
import tempfile
from magel_glue_edges import ROOT, MAGEL, EXE, run

POSTGRES_SOURCE = r'''
import std::sql;
int64 main() {
    Database failed = Database::postgres("fixture-fail");
    std::lang::assert(!failed.isOpen() && std::lang::len(failed.lastError()) > 0);
    Database setup = Database::postgres("fixture-setup-fail");
    std::lang::assert(!setup.isOpen() && std::lang::len(setup.lastError()) > 0);
    Database db = Database::postgres("fixture-ok");
    if (!db.isOpen()) { std::lang::panic(db.lastError()); }
    std::lang::assert(db.backendName() == "postgresql" && db.quote("O'Reilly\\path") == "'O''Reilly\\path'");
    ResultSet result = db.exec("SELECT fixtures");
    std::lang::assert(result.ok && result.rowCount() == 1 && result.colCount() == 6 && result.affected == 0);
    std::lang::assert(result.intAt(0, 0) == 9223372036854775807 && result.realAt(0, 1) == 1.25);
    DataCell text = result.at(0, 2);
    DataCell absent = result.at(0, 4);
    std::lang::assert(text.kind == CELL_TEXT && text.text() == "123" && absent.isNull());
    std::lang::assert(result.textAt(0, 3) == "中文" && result.intAt(0, 5) == 1);
    std::lang::assert(result.columnName(0) == "big");
    ResultSet written = db.exec("UPDATE fixtures");
    std::lang::assert(written.ok && written.affected == 7 && db.affectedRows() == 7);
    ResultSet huge = db.exec("UPDATE huge");
    std::lang::assert(huge.ok && huge.affected == 5000000000 && db.affectedRows() == 5000000000);
    std::lang::assert(!db.exec("SELECT fail").ok);
    for (int64 i = 0; i < 300; i = i + 1) { std::lang::assert(db.exec("SELECT fixtures").ok); }
    db.close(); db.close();
    std::lang::assert(!db.exec("SELECT fixtures").ok);
    Database copy = Database::postgres("fixture-ok");
    std::lang::assert(!copy.exec("COPY fixtures").ok);
    copy.close();
    std::lang::assert(fixture_live_connections() == 0 && fixture_live_results() == 0);
    std::lang::println("postgres ABI checks passed");
    return 0;
}
'''
ODBC_SOURCE = r'''
import std::sql;
int64 main(std::lang::List<String> arguments) {
    Database microsoft = Database::sqlServer("DRIVER={Magel Missing Driver};SERVER=invalid;");
    std::lang::assert(!microsoft.isOpen() && std::lang::len(microsoft.lastError()) > 0);
    std::lang::assert(microsoft.backendName() == "sqlserver" && microsoft.quote("中文") == "N'中文'");
    Database db = Database::odbc(arguments[0]);
    if (!db.isOpen()) { std::lang::panic(db.lastError()); }
    std::lang::assert(db.backendName() == "odbc");
    ResultSet created = db.exec("CREATE TABLE items(id BIGINT, name TEXT, amount DOUBLE, note TEXT)");
    if (!created.ok) { std::lang::panic(created.error); }
    String longText = "";
    for (int64 i = 0; i < 1400; i = i + 1) { longText = longText + "中文🙂"; }
    ResultSet inserted = db.exec("INSERT INTO items VALUES(9223372036854775807, " + db.quote(longText) + ", 1.25, NULL)");
    std::lang::assert(inserted.ok && inserted.affected == 1);
    ResultSet result = db.exec("SELECT id, name, amount, note, '123' AS numeric_text FROM items");
    std::lang::assert(result.ok && result.rowCount() == 1 && result.colCount() == 5);
    std::lang::assert(result.intAt(0, 0) == 9223372036854775807);
    std::lang::assert(result.textAt(0, 1) == longText && result.realAt(0, 2) == 1.25);
    DataCell absent = result.at(0, 3);
    DataCell text = result.at(0, 4);
    std::lang::assert(absent.isNull() && text.kind == CELL_TEXT && text.text() == "123");
    ResultSet empty = db.exec("SELECT id FROM items WHERE 0");
    std::lang::assert(empty.ok && empty.rowCount() == 0 && empty.colCount() == 1);
    std::lang::assert(db.execOk("BEGIN TRANSACTION"));
    std::lang::assert(db.execOk("INSERT INTO items(id) VALUES(2)"));
    std::lang::assert(db.execOk("ROLLBACK"));
    std::lang::assert(db.count("SELECT COUNT(*) FROM items") == 1);
    std::lang::assert(!db.exec("SELECT FROM items").ok);
    for (int64 i = 0; i < 300; i = i + 1) { std::lang::assert(db.count("SELECT COUNT(*) FROM items") == 1); }
    ResultSet deleted = db.exec("DELETE FROM items");
    std::lang::assert(deleted.ok && deleted.affected == 1);
    db.close(); db.close();
    std::lang::println("odbc database checks passed");
    return 0;
}
'''


def triplet(source, expected, work, arguments_factory=lambda mode: []):
    prefix = work / source.stem
    result = run(MAGEL, 'run', source, '--out', prefix, '--', *arguments_factory('interpreter'))
    assert result.stdout == expected and not result.stderr, (result.stdout, result.stderr)
    for target in ('cpp-aot',):
        executable = work / (source.stem + '-' + target + EXE)
        run(MAGEL, 'aot', source, '--out', executable, '--opt-level=0')
        result = run(executable, *arguments_factory(target))
        assert result.stdout == expected and not result.stderr, (target, result.stdout, result.stderr)


def main():
    work = ROOT / 'build/magel-sql-backends'
    work.mkdir(parents=True, exist_ok=True)
    fixture = work / ('libpq-fixture.dll' if os.name == 'nt' else 'libpq-fixture.so')
    run('gcc', '-shared', *([] if os.name == 'nt' else ['-fPIC']), ROOT / 'tests/fixtures/magel_sql_pg.c', '-o', fixture)
    old = os.environ.get('POSTGRES_LIBRARY')
    os.environ['POSTGRES_LIBRARY'] = str(fixture)
    try:
        source = work / 'postgres.mgl'
        declarations = 'abi ' + json.dumps(fixture.as_posix()) + ' { int32 fixture_live_connections() = "fixture_live_connections"; int32 fixture_live_results() = "fixture_live_results"; }\n'
        source.write_text(POSTGRES_SOURCE.replace('import std::sql;', 'import std::sql;\n' + declarations, 1), encoding='utf-8')
        triplet(source, 'postgres ABI checks passed\n', work)
        print('PostgreSQL：libpq ABI 夹具解释执行与 AOT 通过（非真实服务器）', flush=True)
    finally:
        if old is None: os.environ.pop('POSTGRES_LIBRARY', None)
        else: os.environ['POSTGRES_LIBRARY'] = old
    driver = os.environ.get('MAGEL_TEST_ODBC_DRIVER')
    if not driver and os.name == 'nt':
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers') as key:
            try:
                installed, _ = winreg.QueryValueEx(key, 'SQLite3 ODBC Driver')
                if installed == 'Installed': driver = 'SQLite3 ODBC Driver'
            except FileNotFoundError:
                pass
    if not driver:
        print('ODBC 实库测试未运行：可用 MAGEL_TEST_ODBC_DRIVER 指定 SQLite ODBC 驱动', flush=True)
        return
    with tempfile.TemporaryDirectory(prefix='database-', dir=work) as temporary:
        source = work / 'odbc.mgl'
        source.write_text(ODBC_SOURCE, encoding='utf-8')
        def arguments(mode):
            path = (Path(temporary) / (mode + '.db')).as_posix()
            return [f'DRIVER={{{driver}}};Database={path};BigInt=1;']
        triplet(source, 'odbc database checks passed\n', work, arguments)
    print('ODBC：SQLite 驱动真实Unicode、长文本、NULL、事务与错误恢复解释执行与 AOT 通过；SQL Server 连接诊断与转义通过')


if __name__ == '__main__':
    main()
