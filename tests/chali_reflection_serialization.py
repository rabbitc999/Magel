#!/usr/bin/env python3
"""反射编解码、模型注解和参数化 SQL 的定向回归，不启动全量门禁。"""
import argparse
import concurrent.futures
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(compiler, source, directory, expected=True, flags=()):
    """编译并运行独立样例，核对进程状态并保留完整诊断。"""
    case = Path(tempfile.mkdtemp(prefix="case-", dir=directory))
    path = case / "case.chl"
    path.write_text(source, encoding="utf-8")
    result = subprocess.run([str(compiler), "run", str(path), "--out", str(case / "out"), "-I", str(directory), *flags], cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
    output = result.stdout + result.stderr
    if (result.returncode == 0) != expected:
        raise AssertionError(f"exit={result.returncode}\n{output}\n{source}")
    print(case.name, "OK", flush=True)
    return output


def main():
    """运行反射拥有关系、精确数字、格式错误及 SQL 集成验证。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--chali", default=os.environ.get("CHALI_COMPILER", ROOT / ("ChaLi.exe" if os.name == "nt" else "chali")))
    args = parser.parse_args()
    compiler = Path(args.chali).resolve()
    with tempfile.TemporaryDirectory(prefix="chali-serde-") as temporary, concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        directory = Path(temporary)
        futures = []
        def enqueue(*parameters, **options):
            """使用独立源码和输出目录并行运行互不依赖的回归。"""
            future = pool.submit(run, *parameters, **options)
            futures.append(future)
            return future
        (directory / "alpha.chl").write_text("module alpha;\nT echo<T>(T value) { return value; }\n", encoding="utf-8")
        (directory / "beta.chl").write_text("module beta;\nT echo<T>(T value) { return value + value; }\n", encoding="utf-8")
        enqueue(compiler, '''import alpha;
import beta;
int64 main() { assert(alpha::echo<int64>(3) == 3); assert(beta::echo<int64>(3) == 6); assert(beta::echo(4) == 8); return 0; }
''', directory)
        for sample in ("reflect_fields", "reflect_serialization", "sql_ipage"):
            enqueue(compiler, (ROOT / "example/samples" / f"{sample}.chl").read_text(encoding="utf-8"), directory)
        enqueue(compiler, r'''
import std::toml;
@Reflect
enum Choice { Empty, Count(int64 value), Text(String value) };
@Reflect
struct Record {
    uint64 maximum;
    BigInteger integer;
    BigDecimal decimal;
    List<int64> numbers;
    Map<String,String> names;
    Set<int64> unique;
    Option<String> optional;
    Choice choice;
};
int64 main() {
    Record value;
    value.maximum = 18446744073709551615u;
    value.integer = BigInteger("123456789012345678901234567890");
    value.decimal = BigDecimal("1.234567890123456789");
    value.decimal.setPrecision(40);
    value.numbers.push(0 - 9223372036854775807 - 1);
    value.names.put("中文", "quote\"\\\n");
    value.unique.add(7);
    value.optional = Option<String>::Value("owned");
    value.choice = Choice::Text("enum");
    let json = std::json::serialize<Record>(&value);
    if (json.isErr()) { println("{}", json.error()); }
    assert(json.isOk());
    let decoded = std::json::deserialize<Record>(json.unwrap());
    if (decoded.isErr()) { println("{}", decoded.error()); }
    assert(decoded.isOk());
    let copy = decoded.unwrap();
    assert(copy.maximum == value.maximum);
    assert(copy.integer == value.integer);
    assert(copy.decimal == value.decimal);
    assert(std::reflect::__serdeBigPrecision(copy.decimal) == 40);
    assert(copy.numbers.at(0) == value.numbers.at(0));
    assert(copy.names.valAt(0) == "quote\"\\\n");
    assert(copy.unique.has(7) != 0);
    assert(copy.optional.unwrap() == "owned");
    let toml = std::toml::serialize<Record>(&value);
    if (toml.isErr()) { println("{}", toml.error()); }
    assert(toml.isOk());
    let fromToml = std::toml::deserialize<Record>(toml.unwrap());
    if (fromToml.isErr()) { println("{}", fromToml.error()); }
    assert(fromToml.isOk());
    let bytes = std::serialization::serialize<Record>(&value).unwrap();
    assert(std::serialization::deserialize<Record>(bytes).isOk());
    assert(std::serialization::deserialize<int64>(bytes).isErr());
    bytes.push((uint8)1);
    assert(std::serialization::deserialize<Record>(bytes).isErr());
    assert(std::json::deserialize<int8>("128").isErr());
    assert(std::json::deserialize<uint8>("-1").isErr());
    assert(std::json::deserialize<bool>("1").isErr());
    assert(std::json::deserialize<float32>("1e100").isErr());
    assert(std::json::deserialize<Record>("{}").isErr());
    assert(std::json::deserialize<Record>("[").isErr());
    assert(std::toml::deserialize<Record>("broken = [").isErr());
    assert(std::json::deserialize<BigInteger>("\"01\"").isErr());
    assert(std::json::deserialize<BigDecimal>("{\"$decimal\":\"1e1000001\",\"$precision\":0}").isErr());
    println("precision, containers, enums, TOML and binary OK");
    return 0;
}
''', directory)
        enqueue(compiler, '''import std::json;
struct Hidden { int64 value; };
int64 main() { Hidden value; assert(std::json::serialize<Hidden>(&value).isErr()); return 0; }
''', directory)
        enqueue(compiler, r'''
import std::sql;
@Reflect
@std::sql::ModelClass("records")
struct Row {
    @std::sql::ModelField(id: true)
    int64 id;
    String name;
    bool active;
    float64 score;
};
int64 main() {
    let base = std::sql::query<Row>().unwrap();
    let db = std::sql::Database::sqlite(":memory:");
    assert(db.exec(base.build("create").unwrap()).ok);
    Row value;
    value.id = 1; value.name = "a'\\b\n中文"; value.active = true; value.score = 1.25;
    assert(db.exec(std::sql::insert<Row>(&base, &value).unwrap()).ok);
    value.id = 2; value.name = "second";
    assert(db.exec(std::sql::insert<Row>(&base, &value).unwrap()).ok);
    let filtered = base;
    assert(std::sql::filter<int64>(&filtered, "id", 1).isOk());
    assert(db.exec(filtered.build().unwrap()).rowCount() == 1);
    assert(db.exec(base.build().unwrap()).rowCount() == 2);
    assert(base.build("delete").isErr());
    assert(std::sql::update<Row>(&base, &value).isErr());
    value.name = "updated";
    assert(db.exec(std::sql::update<Row>(&filtered, &value).unwrap()).ok);
    assert(db.exec(filtered.build().unwrap()).textAt(0, 1) == "updated");
    let unknown = base;
    std::sql::filter<int64>(&unknown, "missing; DROP TABLE records", 1);
    assert(unknown.build().isErr());
    List<int64> ids; ids.push(1); ids.push(2);
    let inside = base;
    assert(std::sql::filterMany<int64>(&inside, "id", ids).isOk());
    assert(db.exec(inside.build("count").unwrap()).count() == 2);
    base.page(1, 1); base.orderBy("id");
    assert(db.exec(base.build().unwrap()).intAt(0, 0) == 2);
    base.page(0 - 1);
    assert(base.build().isErr());
    std::sql::SqlStatement manual;
    manual.sql = "SELECT ? AS n, ? AS t, ? AS z";
    manual.parameters = "[9223372036854775807,\"text\",null]";
    let result = db.exec(manual);
    assert(result.ok && result.intAt(0, 0) == 9223372036854775807);
    assert(result.textAt(0, 1) == "text" && result.at(0, 2).isNull());
    manual.parameters = "[]";
    assert(!db.exec(manual).ok);
    manual.parameters = "[{}]"; manual.sql = "SELECT ?";
    assert(!db.exec(manual).ok);
    manual.parameters = "[]"; manual.sql = "DELETE FROM records; SELECT 1";
    assert(!db.exec(manual).ok);
    manual.sql = "-- comment only\n";
    assert(!db.exec(manual).ok);
    assert(db.exec(filtered.build().unwrap()).rowCount() == 1);
    assert(db.exec(filtered.build("delete").unwrap()).ok);
    db.close();
    assert(!db.exec(filtered.build().unwrap()).ok);
    println("SQL CRUD, validation, copy isolation and parameters OK");
    return 0;
}
''', directory)
        output = enqueue(compiler, '''import std::json;
int64 main() { int64 value = 1; std::json::serialize<int64>(&value); return 0; }
''', directory, expected=False, flags=("--no-reflection",))
        output = output.result()
        for future in futures:
            future.result()
        if "反射" not in output:
            raise AssertionError(output)
    print("reflection serialization and SQL targeted regressions OK")


if __name__ == "__main__":
    main()
