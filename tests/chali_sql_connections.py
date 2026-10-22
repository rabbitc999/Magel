"""用明确标注的客户端 ABI 夹具验证 MySQL/MariaDB 新入口、端口和转义。"""
import os
from pathlib import Path
from chali_glue_edges import ROOT, CHALI, EXE, run

def main():
    """验证连接入口的 VM 行为；不代表真实数据库服务验收。"""
    work = ROOT / 'build/sql-connections'
    work.mkdir(parents=True, exist_ok=True)
    library = work / ('fixture.dll' if os.name == 'nt' else 'fixture.so')
    run('gcc', '-shared', '-fPIC', ROOT / 'tests/fixtures/chali_sql_mysql.c', '-o', library)
    os.environ['MYSQL_LIBRARY'] = str(library)
    source = work / 'connections.chl'
    source.write_text('import std::sql;\nvoid main(){\n    Database a=Database::mysql("mysql-default","user","password","database");\n    Database b=Database::mysql("mysql-explicit","user","password","database",(int32)3307);\n    Database c=Database::maria("maria-default","user","password","database");\n    Database d=Database::maria("maria-explicit","user","password","database",(int32)3308);\n    std::lang::assert(a.isOpen() && b.isOpen() && c.isOpen() && d.isOpen());\n    std::lang::assert(a.backendName()=="mysql" && b.backendName()=="mysql");\n    std::lang::assert(c.backendName()=="mariadb" && d.backendName()=="mariadb");\n    std::lang::assert(a.quote("中文\'\\\\😀")=="\'中文\'\'\\\\\\\\😀\'");\n    std::lang::assert(c.quote("中文\'\\\\😀")==a.quote("中文\'\\\\😀"));\n    std::lang::assert(std::sql::quote("",false)=="\'\'");\n    a.close();b.close();c.close();d.close();\n    std::lang::println("mysql/maria connection ABI ok");return;\n}\n', encoding='utf-8')
    result = run(CHALI, 'run', source, '--out', work / 'vm')
    assert result.stdout == 'mysql/maria connection ABI ok\n' and (not result.stderr), result
    binary = work / ('connections' + EXE)
    old = work / 'old.chl'
    old.write_text('import std::sql;\nvoid main(){Database db=Database::connect("h","u","p","d");return;}', encoding='utf-8')
    result = run(CHALI, 'check', old, succeeds=False)
    assert 'connect' in result.stdout + result.stderr
if __name__ == '__main__':
    main()
