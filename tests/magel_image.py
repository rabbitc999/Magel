from pathlib import Path
"""验证 std::image 图片处理标准库：VM 执行、AOT 导出一致性、文件读写往返与模块裁剪。

先构建 magel.exe 与 magel-emit，再运行本文件。
"""
from magel_glue_edges import ROOT, MAGEL, EMITTER, EXE, run


def checkFileRoundtrip(work):
    """BMP 保存 → 打开往返，以及坏路径错误路径。"""
    source = work / "image_io.mgl"
    source.write_text(
        "import std::image;\n"
        "int64 main() {\n"
        "    Image image = Image::make(4, 2).unwrap();\n"
        "    image.setPixel(0, 0, 10, 20, 30, 255);\n"
        "    image.setPixel(3, 1, 200, 100, 50, 255);\n"
        "    Result<int64, String> saved = image.saveBmp(\"" + str((work / "roundtrip.bmp").as_posix()) + "\");\n"
        "    if (saved.isErr()) { print(\"save err={}\", saved.error()); return 1; }\n"
        "    print(\"saved {}\", saved.unwrap());\n"
        "    Image loaded = Image::open(\"" + str((work / "roundtrip.bmp").as_posix()) + "\").unwrap();\n"
        "    Pixel a = loaded.getPixel(0, 0).unwrap();\n"
        "    Pixel b = loaded.getPixel(3, 1).unwrap();\n"
        "    print(\"{}x{} {} {} {} {} {} {}\", loaded.width, loaded.height,\n"
        "          a.r, a.g, a.b, b.r, b.g, b.b);\n"
        "    Result<int64, String> bad = image.saveBmp(\"Z:/no/such/dir/x.bmp\");\n"
        "    print(\"bad={}\", bad.isErr());\n"
        "    return 0;\n"
        "}\n", encoding="utf-8")
    result = run(MAGEL, "run", source, "--out", work / "io-vm")
    assert "saved 78" in result.stdout and "4x2 10 20 30 200 100 50" in result.stdout, result.stdout
    assert "bad=1" in result.stdout, result.stdout


def main():
    work = ROOT / "build/magel-image"
    work.mkdir(parents=True, exist_ok=True)
    source = ROOT / "example/samples/std_image.mgl"
    expected = (ROOT / "example/expected/std_image.txt").read_text(encoding="utf-8")
    result = run(MAGEL, "run", source, "--out", work / "vm")
    assert result.stdout == expected and not result.stderr, (result.stdout, result.stderr)
    # 显式验证无护栏 AOT 导出的一致性；工具链默认配置仍为 checked。
    prefix = work / "image"
    run(MAGEL, "run", source, "--emit", "--out", prefix)
    common = ["--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
              "--layout", prefix.with_suffix(".layout")]
    executable = work / ("cpp-aot" + EXE)
    run(EMITTER, *common, "--target", "cpp-aot", "--out", executable,
        "--cpp-output-dir", work / "aot", "--unchecked")
    result = run(executable)
    assert result.stdout == expected, result.stdout
    # 模块裁剪：定义 NO_MAGEL_STD_IMAGE 后调用 std::image 应报错。
    trimmed = work / ("NO_MAGEL_STD_IMAGE" + EXE)
    run("g++", "-std=c++17", "-O0", "-DNO_MAGEL_STD_IMAGE",
        work / "aot" / ("cpp-aot" + EXE + ".aot.cpp"), "-o", trimmed)
    result = run(trimmed, succeeds=False)
    assert "std::image::" in result.stdout + result.stderr, result.stdout + result.stderr
    checkFileRoundtrip(work)
    # StringBuilder 同样过一遍 VM 与 AOT。
    sbSource = ROOT / "example/samples/std_string_builder.mgl"
    sbExpected = (ROOT / "example/expected/std_string_builder.txt").read_text(encoding="utf-8")
    result = run(MAGEL, "run", sbSource, "--out", work / "sb-vm")
    assert result.stdout == sbExpected and not result.stderr, (result.stdout, result.stderr)
    prefix = work / "sb"
    run(MAGEL, "run", sbSource, "--emit", "--out", prefix)
    common = ["--quad", prefix.with_suffix(".quad"), "--data", prefix.with_suffix(".data.bin"),
              "--layout", prefix.with_suffix(".layout")]
    sbExecutable = work / ("sb-aot" + EXE)
    run(EMITTER, *common, "--target", "cpp-aot", "--out", sbExecutable,
        "--cpp-output-dir", work / "sb-aot", "--unchecked")
    result = run(sbExecutable)
    assert result.stdout == sbExpected, result.stdout
    print("std::image 与 StringBuilder 检查通过：VM、AOT（无护栏）、模块裁剪、文件读写往返")


main()
