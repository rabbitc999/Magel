"""以真实编译分析验证卫生宏的高阶闭包提示和 AST 预览。"""
import argparse
import json
from pathlib import Path
import tempfile
from native_lsp_transport import Transport, position

ROOT = Path(__file__).resolve().parents[1]


def main():
    """查询未保存源码的悬停、补全、行内提示和宏预览，保留原始文档位置。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    compiler = parser.parse_args().magel.resolve()
    cases = [
        ("fn() { fn() { fn(int32 x) { x } } }", [
            ("outer", "((int32 fn(int32)) fn()) fn()"),
            ("middle", "(int32 fn(int32)) fn()"),
            ("inner", "int32 fn(int32)")]),
        ("fn() { fn() { identity } }", [
            ("outer", "((int64 fn(int64)) fn()) fn()"),
            ("middle", "(int64 fn(int64)) fn()"),
            ("inner", "int64 fn(int64)")]),
    ]
    with tempfile.TemporaryDirectory(prefix="magel-macro-editor-") as folder:
        server = Transport(compiler, folder)
        server.query("initialize", dict(rootUri=Path(folder).as_uri()))
        try:
            for index, (body, expected) in enumerate(cases):
                text = "$macro MAKE\n" + body + "\n$end_macro\nint64 identity(int64 x) { return x; }\nint64 main() { auto outer = $MAKE; auto middle = outer(); auto inner = middle(); inner; return 0; }"
                uri = (Path(folder) / f"case_{index}.mgl").as_uri()
                server.send(dict(method="textDocument/didOpen", params=dict(textDocument=dict(uri=uri, text=text, version=1))))
                hints = server.query("textDocument/inlayHint", dict(textDocument=dict(uri=uri), range=dict(start=position(text, 0), end=position(text, len(text)))))
                assert {hint["label"] for hint in hints} == {": " + value for _, value in expected}, hints
                for name, value in expected:
                    hover = server.query("textDocument/hover", dict(textDocument=dict(uri=uri), position=position(text, text.index(name + " =") + 2)))
                    assert value + " " + name in hover["contents"]["value"], (name, hover)
                completion = server.query("textDocument/completion", dict(textDocument=dict(uri=uri), position=position(text, text.rindex("inner;"))))
                entries = completion.get("items", []) if isinstance(completion, dict) else completion
                assert any(item["label"] == "inner" and item.get("detail") == expected[-1][1] + " inner" for item in entries), entries
                preview = server.query("magel/expandMacro", dict(textDocument=dict(uri=uri)))
                assert not preview["diagnostics"] and preview["format"] == "json", preview
                tree = json.loads(preview["text"])
                assert tree["format"] == "hygienic-ast" and any(item["name"] == "MAKE" for item in tree["expansions"]), tree
                assert preview["mappings"] and "bindingIdentity" not in preview["text"] and "temporaries" not in preview, preview
            text = "$macro MAKE\nfn<T>(T value) { value }\n$end_macro\nint64 main() { auto closure = $MAKE; auto result = closure(42); return 0; }"
            uri = (Path(folder) / "generic.mgl").as_uri()
            server.send(dict(method="textDocument/didOpen", params=dict(textDocument=dict(uri=uri, text=text, version=1))))
            hints = server.query("textDocument/inlayHint", dict(textDocument=dict(uri=uri), range=dict(start=position(text, 0), end=position(text, len(text)))))
            assert {hint["label"] for hint in hints} == {": T fn<T>(T)", ": int64"}, hints
            hover = server.query("textDocument/hover", dict(textDocument=dict(uri=uri), position=position(text, text.index("closure =") + 2)))
            assert "T fn<T>(T) closure" in hover["contents"]["value"], hover
        finally:
            server.close()
    print("宏生成的多层闭包和函数返回：悬停、补全、行内类型与卫生 AST 预览通过。", flush=True)


if __name__ == "__main__":
    main()
