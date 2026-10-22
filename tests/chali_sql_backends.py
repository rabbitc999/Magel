"""验证 PostgreSQL libpq ABI 夹具与真实 ODBC 数据源；不要求网络数据库服务。"""
from pathlib import Path
import json
import os
import tempfile
from chali_glue_edges import ROOT, CHALI, EXE, run
POSTGRES_SOURCE = '\nimport std::sql;\nvoid main() {\n    Database failed = Database::postgres("fixture-fail");\n    std::lang::assert(!failed.isOpen() && std::lang::len(failed.lastError()) > 0);\n    Database setup = Database::postgres("fixture-setup-fail");\n    std::lang::assert(!setup.isOpen() && std::lang::len(setup.lastError()) > 0);\n    Database db = Database::postgres("fixture-ok");\n    if (!db.isOpen()) { std::lang::panic(db.lastError()); }\n    std::lang::assert(db.backendName() == "postgresql" && db.quote("O\'Reilly\\\\path") == "\'O\'\'Reilly\\\\path\'");\n    ResultSet result = db.exec("SELECT fixtures");\n    std::lang::assert(result.ok && result.rowCount() == 1 && result.colCount() == 6 && result.affected == 0);\n    std::lang::assert(result.intAt(0, 0) == 9223372036854775807 && result.realAt(0, 1) == 1.25);\n    DataCell text = result.at(0, 2);\n    DataCell absent = result.at(0, 4);\n    std::lang::assert(text.kind == CELL_TEXT && text.text() == "123" && absent.isNull());\n    std::lang::assert(result.textAt(0, 3) == "中文" && result.intAt(0, 5) == 1);\n    std::lang::assert(result.columnName(0) == "big");\n    ResultSet written = db.exec("UPDATE fixtures");\n    std::lang::assert(written.ok && written.affected == 7 && db.affectedRows() == 7);\n    ResultSet huge = db.exec("UPDATE huge");\n    std::lang::assert(huge.ok && huge.affected == 5000000000 && db.affectedRows() == 5000000000);\n    std::lang::assert(!db.exec("SELECT fail").ok);\n    {\n        int64 i = 0;\n        while (i < 300) {\n std::lang::assert(db.exec("SELECT fixtures").ok); \n            i = i + 1;\n        }\n    }\n    db.close(); db.close();\n    std::lang::assert(!db.exec("SELECT fixtures").ok);\n    Database copy = Database::postgres("fixture-ok");\n    std::lang::assert(!copy.exec("COPY fixtures").ok);\n    copy.close();\n    std::lang::assert(fixture_live_connections() == 0 && fixture_live_results() == 0);\n    std::lang::println("postgres ABI checks passed");\n    return;\n}\n'
ODBC_SOURCE = '\nimport std::sql;\nvoid main(std::lang::List<String> arguments) {\n    Database microsoft = Database::sqlServer("DRIVER={ChaLi Missing Driver};SERVER=invalid;");\n    std::lang::assert(!microsoft.isOpen() && std::lang::len(microsoft.lastError()) > 0);\n    std::lang::assert(microsoft.backendName() == "sqlserver" && microsoft.quote("中文") == "N\'中文\'");\n    Database db = Database::odbc(arguments[0]);\n    if (!db.isOpen()) { std::lang::panic(db.lastError()); }\n    std::lang::assert(db.backendName() == "odbc");\n    ResultSet created = db.exec("CREATE TABLE items(id BIGINT, name TEXT, amount DOUBLE, note TEXT)");\n    if (!created.ok) { std::lang::panic(created.error); }\n    String longText = "";\n    {\n        int64 i = 0;\n        while (i < 1400) {\n longText = longText + "中文🙂"; \n            i = i + 1;\n        }\n    }\n    ResultSet inserted = db.exec("INSERT INTO items VALUES(9223372036854775807, " + db.quote(longText) + ", 1.25, NULL)");\n    std::lang::assert(inserted.ok && inserted.affected == 1);\n    ResultSet result = db.exec("SELECT id, name, amount, note, \'123\' AS numeric_text FROM items");\n    std::lang::assert(result.ok && result.rowCount() == 1 && result.colCount() == 5);\n    std::lang::assert(result.intAt(0, 0) == 9223372036854775807);\n    std::lang::assert(result.textAt(0, 1) == longText && result.realAt(0, 2) == 1.25);\n    DataCell absent = result.at(0, 3);\n    DataCell text = result.at(0, 4);\n    std::lang::assert(absent.isNull() && text.kind == CELL_TEXT && text.text() == "123");\n    ResultSet empty = db.exec("SELECT id FROM items WHERE 0");\n    std::lang::assert(empty.ok && empty.rowCount() == 0 && empty.colCount() == 1);\n    std::lang::assert(db.execOk("BEGIN TRANSACTION"));\n    std::lang::assert(db.execOk("INSERT INTO items(id) VALUES(2)"));\n    std::lang::assert(db.execOk("ROLLBACK"));\n    std::lang::assert(db.count("SELECT COUNT(*) FROM items") == 1);\n    std::lang::assert(!db.exec("SELECT FROM items").ok);\n    {\n        int64 i = 0;\n        while (i < 300) {\n std::lang::assert(db.count("SELECT COUNT(*) FROM items") == 1); \n            i = i + 1;\n        }\n    }\n    ResultSet deleted = db.exec("DELETE FROM items");\n    std::lang::assert(deleted.ok && deleted.affected == 1);\n    db.close(); db.close();\n    std::lang::println("odbc database checks passed");\n    return;\n}\n'

def triplet(source, expected, work, arguments_factory=lambda mode: []):
    prefix = work / source.stem
    result = run(CHALI, 'run', source, '--out', prefix, '--', *arguments_factory('interpreter'))
    assert result.stdout == expected and (not result.stderr), (result.stdout, result.stderr)

def main():
    work = ROOT / 'build/chali-sql-backends'
    work.mkdir(parents=True, exist_ok=True)
    fixture = work / ('libpq-fixture.dll' if os.name == 'nt' else 'libpq-fixture.so')
    run('gcc', '-shared', *([] if os.name == 'nt' else ['-fPIC']), ROOT / 'tests/fixtures/chali_sql_pg.c', '-o', fixture)
    old = os.environ.get('POSTGRES_LIBRARY')
    os.environ['POSTGRES_LIBRARY'] = str(fixture)
    try:
        source = work / 'postgres.chl'
        declarations = 'abi ' + json.dumps(fixture.as_posix()) + ' { int32 fixture_live_connections() = "fixture_live_connections"; int32 fixture_live_results() = "fixture_live_results"; }\n'
        source.write_text(POSTGRES_SOURCE.replace('import std::sql;', 'import std::sql;\n' + declarations, 1), encoding='utf-8')
        triplet(source, 'postgres ABI checks passed\n', work)
    finally:
        if old is None:
            os.environ.pop('POSTGRES_LIBRARY', None)
        else:
            os.environ['POSTGRES_LIBRARY'] = old
    driver = os.environ.get('CHALI_TEST_ODBC_DRIVER')
    if not driver and os.name == 'nt':
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, 'SOFTWARE\\ODBC\\ODBCINST.INI\\ODBC Drivers') as key:
            try:
                installed, _ = winreg.QueryValueEx(key, 'SQLite3 ODBC Driver')
                if installed == 'Installed':
                    driver = 'SQLite3 ODBC Driver'
            except FileNotFoundError:
                pass
    if not driver:
        print('ODBC 实库测试未运行：可用 CHALI_TEST_ODBC_DRIVER 指定 SQLite ODBC 驱动', flush=True)
        return
    with tempfile.TemporaryDirectory(prefix='database-', dir=work) as temporary:
        source = work / 'odbc.chl'
        source.write_text(ODBC_SOURCE, encoding='utf-8')

        def arguments(mode):
            path = (Path(temporary) / (mode + '.db')).as_posix()
            return [f'DRIVER={{{driver}}};Database={path};BigInt=1;']
        triplet(source, 'odbc database checks passed\n', work, arguments)
if __name__ == '__main__':
    main()
