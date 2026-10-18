"""用明确标注的客户端 ABI 夹具验证 MySQL/MariaDB 新入口、端口和转义。"""
import os
from pathlib import Path
from magel_glue_edges import ROOT, MAGEL, EXE, run


def main():
    """验证连接入口的 VM/AOT 行为；不代表真实数据库服务验收。"""
    work = ROOT / 'build/sql-connections'
    work.mkdir(parents=True, exist_ok=True)
    library = work / ('fixture.dll' if os.name == 'nt' else 'fixture.so')
    run('gcc', '-shared', '-fPIC', ROOT / 'tests/fixtures/magel_sql_mysql.c', '-o', library)
    os.environ['MYSQL_LIBRARY'] = str(library)
    source = work / 'connections.mgl'
    source.write_text(r'''import std::sql;
int64 main(){
    Database a=Database::mysql("mysql-default","user","password","database");
    Database b=Database::mysql("mysql-explicit","user","password","database",(int32)3307);
    Database c=Database::maria("maria-default","user","password","database");
    Database d=Database::maria("maria-explicit","user","password","database",(int32)3308);
    std::lang::assert(a.isOpen() && b.isOpen() && c.isOpen() && d.isOpen());
    std::lang::assert(a.backendName()=="mysql" && b.backendName()=="mysql");
    std::lang::assert(c.backendName()=="mariadb" && d.backendName()=="mariadb");
    std::lang::assert(a.quote("中文'\\😀")=="'中文''\\\\😀'");
    std::lang::assert(c.quote("中文'\\😀")==a.quote("中文'\\😀"));
    std::lang::assert(std::sql::quote("",false)=="''");
    a.close();b.close();c.close();d.close();
    std::lang::println("mysql/maria connection ABI ok");return 0;
}
''', encoding='utf-8')
    result = run(MAGEL, 'run', source, '--out', work / 'vm')
    assert result.stdout == 'mysql/maria connection ABI ok\n' and not result.stderr, result
    binary = work / ('connections' + EXE)
    run(MAGEL, 'aot', source, '--out', binary, '--opt-level=0')
    result = run(binary)
    assert result.stdout == 'mysql/maria connection ABI ok\n' and not result.stderr, result
    old = work / 'old.mgl'
    old.write_text('import std::sql;\nint64 main(){Database db=Database::connect("h","u","p","d");return 0;}',encoding='utf-8')
    result = run(MAGEL, 'check', old, succeeds=False)
    assert 'connect' in result.stdout + result.stderr
    print('MySQL/MariaDB：新入口、默认/显式端口、Unicode 转义、旧入口诊断 VM/AOT 通过（ABI 夹具）')


if __name__ == '__main__':
    main()
