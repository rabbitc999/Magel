"""验证引用生命周期与泛型、枚举、回调、容器和构造链的组合边界。"""
from pathlib import Path
import argparse
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/magel-reference-combinations"


def execute(*args):
    """执行工具或产物，并保留失败诊断及运行时资源清理输出。"""
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                          encoding="utf-8", errors="replace", timeout=180)


def main():
    """检查组合拒绝路径及解释执行、AOT 的正常行为。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    compiler = parser.parse_args().magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    cases = {
        "generic_box": """
struct Box<T> { T value; };
T relay<T>(T value) { return value; }
Box<int64 mut&> bad() {
    int64 local = 1;
    Box<int64 mut&> box;
    box.value = &local;
    return relay(box);
}""",
        "generic_enum": """
enum Choice<T> { Empty, Found(T value); };
Choice<int64 mut&> bad() {
    int64 local = 1;
    return Choice<int64 mut&>::Found(&local);
}""",
        "nested_result": """
Result<Option<int64 mut&>, String> bad() {
    int64 local = 1;
    return Result<Option<int64 mut&>, String>::Ok(Option<int64 mut&>::Value(&local));
}""",
        "callback_argument": """

int64 mut& invoke(int64 mut& fn(int64 mut&) callback, int64 mut& value) { return callback(value); }
int64 mut& bad() {
    int64 local = 1;
    int64 mut& fn(int64 mut&) callback = int64 mut& fn(int64 mut& value) { return value; };
    return invoke(callback, &local);
}""",
        "lambda_local": """
int64 main() {
    auto callback = int64 mut& fn() {
        int64 local = 1;
        return &local;
    };
    return 0;
}""",
        "lambda_value_parameter": """
int64 main() {
    auto callback = int64 mut& fn(int64 value) { return &value; };
    return 0;
}""",
        "generic_tuple": """
T relay<T>(T value) { return value; }
(int64 const&, int64) bad() {
    int64 local = 1;
    (int64 const&, int64) pair = (&local, 2);
    return relay(pair);
}""",
        "cast_option": """
Option<int64 mut&> bad() {
    int64 local = 1;
    int64 const& read = &local;
    return Option<int64 mut&>::Value((int64 mut&)read);
}""",
        "constructor_chain": """
struct Entry {
    String name;
    static Entry create(String name) { self.name = name; }
    String const& borrow() { return &self.name; }
};
String const& bad() {
    return Entry::create("temporary").borrow();
}""",
        "static_factory_chain": """
struct Entry {
    String name;
    static Entry make() { Entry entry; entry.name = "temporary"; return entry; }
    String const& borrow() { return &self.name; }
};
String const& bad() { return Entry::make().borrow(); }
""",
        "enum_method_match": """
int64 global = 0;
enum Entry {
    Empty, Ref(int64 const& value);
    int64 const& borrow() {
        match (self) {
            Entry::Ref(p) => { return p; }
            Entry::Empty => { return &global; }
        }
        return &global;
    }
};
int64 const& bad() { int64 local = 1; return Entry::Ref(&local).borrow(); }
""",
        "generic_list": """
T relay<T>(T value) { return value; }
List<int64 mut&> bad() {
    int64 local = 1;
    List<int64 mut&> values;
    values.push(&local);
    return relay(values);
}""",
        "nested_alias": """
struct Box<T> { T value; };
void bad() {
    Box<Option<int64 mut&>> box;
    Box<Option<int64 mut&>> mut& alias = &box;
    {
        int64 local = 1;
        alias.value = Option<int64 mut&>::Value(&local);
    }
}""",
        "iterator_span": """
import std::iter;
std::iter::Iter<int64 const&> bad() {
    int64 local[2];
    local[0] = 1;
    return std::iter::fromSpan<int64>(&local[0], 2);
}""",
    }
    for name, source in cases.items():
        source = source.lstrip("\n")
        if "int64 main()" not in source:
            source += "\nint64 main() { return 0; }\n"
        path = WORK / (name + ".mgl")
        path.write_text(source, encoding="utf-8")
        result = execute(compiler, "--check", path)
        assert result.returncode != 0 and "MGL1204" in result.stderr, (name, result)
        result = execute(compiler, "--diagnostics=json", path)
        assert result.returncode != 0, (name, result)
        diagnostic = json.loads(result.stdout)["diagnostics"][0]
        assert diagnostic["code"] == "MGL1204", (name, diagnostic)
        span = diagnostic["span"]
        assert Path(span["file"]).name == path.name, (name, diagnostic)
        lines = source.splitlines()
        assert 1 <= span["line"] <= len(lines) and span["column"] >= 1, (name, diagnostic)
        assert diagnostic["source"].strip() == lines[span["line"] - 1].strip(), (name, diagnostic)
        print(f"[通过] 拒绝：{name}", flush=True)

    source = ROOT / "example/samples/reference_combinations.mgl"
    expected = (ROOT / "example/expected/reference_combinations.txt").read_text(encoding="utf-8")
    result = execute(compiler, "run", source, "--out", WORK / "normal-interpret")
    assert result.returncode == 0 and result.stdout == expected and not result.stderr, result
    output = WORK / ("normal-aot.exe" if os.name == "nt" else "normal-aot")
    result = execute(compiler, "aot", source, "--out", output, "--opt-level=0")
    assert result.returncode == 0, result
    result = execute(output)
    assert result.returncode == 0 and result.stdout == expected and not result.stderr, result

    module = WORK / "borrowed_module.mgl"
    module.write_text("module borrowed_module;\nint64 shared = 70;\n"
                      "T relay<T>(T value) { return value; }\n"
                      "int64 const& sharedReference() { return &borrowed_module::shared; }\n",
                      encoding="utf-8")
    source = WORK / "module_main.mgl"
    source.write_text("import borrowed_module;\nint64 main() {\n"
                      "int64 value = 3;\nint64 mut& p = borrowed_module::relay(&value);\n"
                      "p = 4;\nassert(value == 4 && borrowed_module::sharedReference() == 70);\n"
                      "println(\"module reference combinations OK\");\nreturn 0;\n}\n",
                      encoding="utf-8")
    result = execute(compiler, "run", source, "--out", WORK / "module-interpret")
    assert result.returncode == 0 and result.stdout == "module reference combinations OK\n" and not result.stderr, result
    output = WORK / ("module-aot.exe" if os.name == "nt" else "module-aot")
    result = execute(compiler, "aot", source, "--out", output, "--opt-level=0")
    assert result.returncode == 0, result
    result = execute(output)
    assert result.returncode == 0 and result.stdout == "module reference combinations OK\n" and not result.stderr, result

    # 错误发生在导入模块中，诊断必须指回该模块，而不是导入行或展开后的源码。
    module = WORK / "bad_borrowed_module.mgl"
    source = WORK / "bad_module_main.mgl"
    module.write_text("module bad_borrowed_module;\n" + cases["constructor_chain"].lstrip("\n"), encoding="utf-8")
    source.write_text("import bad_borrowed_module;\nint64 main() { return 0; }\n", encoding="utf-8")
    result = execute(compiler, "--check", source)
    assert result.returncode != 0 and "MGL1204" in result.stderr, result
    result = execute(compiler, "--diagnostics=json", source)
    assert result.returncode != 0, result
    diagnostic = json.loads(result.stdout)["diagnostics"][0]
    assert diagnostic["code"] == "MGL1204" and Path(diagnostic["span"]["file"]).name == module.name, diagnostic
    assert diagnostic["span"]["line"] == 8 and diagnostic["span"]["column"] >= 1, diagnostic
    assert diagnostic["source"].strip() == 'return Entry::create("temporary").borrow();', diagnostic
    print(f"跨功能组合验证通过：{len(cases) + 1} 个拒绝场景，综合及跨模块样例解释执行 / AOT")


if __name__ == "__main__":
    main()
