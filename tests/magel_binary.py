from pathlib import Path
"""验证二进制解码、核心浮点转换、导出一致性及模块裁剪。"""
import struct
from magel_glue_edges import ROOT, MAGEL, EMITTER, EXE, run


def checkErrors():
    work = ROOT / "build/magel-binary"
    work.mkdir(parents=True, exist_ok=True)
    samples = ROOT / "example/samples-error"
    for source in sorted(samples.glob("binary_*.mgl")) + sorted(samples.glob("float_int*.mgl")):
        expected = (ROOT / "example/expected-error" / (source.stem + ".txt")).read_text(encoding="utf-8")
        result = run(MAGEL, 'run', source, "--out", work / "error", succeeds=False)
        assert result.stderr.splitlines()[0] == expected.strip(), (source, result.stderr)


def main(names=("std_binary", "lang_float_to_int", "lang_float_bytes", "lang_float64_bytes")):
    work = ROOT / "build/magel-binary"
    work.mkdir(parents=True, exist_ok=True)
    for name in names:
        source = ROOT / f"example/samples/{name}.mgl"
        expected = (ROOT / f"example/expected/{name}.txt").read_text(encoding="utf-8")
        result = run(MAGEL, 'run', source, "--out", work / name)
        assert result.stdout == expected and not result.stderr, (result.stdout, result.stderr)
        prefix = work / name
        run(MAGEL, 'run', source, "--emit", "--out", prefix)
        args = ["--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
                "--layout", prefix.with_suffix(".layout")]
        asset_target = work / (name + "-assets" + EXE)
        run(EMITTER, *args, "--target", "cpp-aot", "--out", asset_target,
            "--cpp-output", "--magel-dir", work / "missing-runtime")
        cpp = Path(str(asset_target) + ".aot.cpp")
        for target in ("cpp-aot",):
            executable = work / (name + "-" + target + EXE)
            run(EMITTER, *args, "--target", target, "--out", executable,
                "--cpp-output-dir", work / (name + "-aot"))
            result = run(executable)
            assert result.stdout == expected and not result.stderr, (target, result.stdout, result.stderr)
        trimmed = work / (name + "-trimmed" + EXE)
        macro = "NO_MAGEL_STD_BINARY" if name == "std_binary" else "NO_MAGEL_STD"
        run("g++", "-std=c++17", "-O0", "-D" + macro, cpp, "-o", trimmed)
        result = run(trimmed, succeeds=name != "std_binary")
        if name == "std_binary":
            assert "std::binary::" in result.stdout + result.stderr
        else:
            assert result.stdout == expected and not result.stderr

    checkErrors()
    # 用 Python struct 的标准编码覆盖特殊浮点数，避免 MAGEL 常量解析影响输入。
    for value, label in [(float("nan"), "nan"), (float("inf"), "inf"),
                         (-float("inf"), "negative-inf"), (-0.0, "negative-zero")]:
        for fmt, magel_type in [("f", "Float32"), ("d", "Float64")]:
            raw = struct.pack("<" + fmt, value)
            setup = ["import std::binary;", "int64 main() {", f"uint8 bytes[{len(raw)}];"]
            setup += [f"bytes[{i}] = (uint8){b};" for i, b in enumerate(raw)]
            setup += [f"std::binary::BinaryReader r = std::binary::BinaryReader::fromArray(bytes, {len(raw)});",
                      f"{magel_type.lower()} value = r.read{magel_type}();"]
            source = work / f"{label}-{fmt}.mgl"
            for width in (32, 64):
                if width == 32:
                    check = "std::lang::assert(std::lang::toInt32(value).unwrap() == (int32)0);" if label == "negative-zero" else "std::lang::assert(std::lang::toInt32(value).isNone());"
                else:
                    check = "std::lang::toInt64(value);"
                source.write_text("\n".join(setup + [check + " return 0; }"]), encoding="utf-8")
                result = run(MAGEL, 'run', source, "--out", work / "special", succeeds=width == 32 or label == "negative-zero")
                if width == 64 and label != "negative-zero":
                    assert "std::lang::toInt64：浮点值不是有限数或超出 int64 范围" in result.stderr
                else:
                    assert not result.stderr, result.stderr
    print("二进制与浮点转换：VM、AOT、内嵌资源、裁剪和特殊浮点验证通过")


if __name__ == "__main__":
    main()
