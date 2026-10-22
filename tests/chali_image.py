from pathlib import Path
'验证 std::image 图片处理标准库：VM 执行、VM 导出一致性、文件读写往返与模块裁剪。\n\n先构建 ChaLi.exe 与 VM，再运行本文件。\n'
from chali_glue_edges import ROOT, CHALI, EXE, run

def checkFileRoundtrip(work):
    """BMP 保存 → 打开往返，以及坏路径错误路径。"""
    source = work / 'image_io.chl'
    source.write_text('import std::image;\nvoid main() {\n    Image image = Image::make(4, 2).unwrap();\n    image.setPixel(0, 0, 10, 20, 30, 255);\n    image.setPixel(3, 1, 200, 100, 50, 255);\n    Result<int64, String> saved = image.saveBmp("' + str((work / 'roundtrip.bmp').as_posix()) + '");\n    if (saved.isErr()) { print("save err={}", saved.error()); let __boom = 1; }\n    print("saved {}", saved.unwrap());\n    Image loaded = Image::open("' + str((work / 'roundtrip.bmp').as_posix()) + '").unwrap();\n    Pixel a = loaded.getPixel(0, 0).unwrap();\n    Pixel b = loaded.getPixel(3, 1).unwrap();\n    print("{}x{} {} {} {} {} {} {}", loaded.width(), loaded.height(),\n          a.r, a.g, a.b, b.r, b.g, b.b);\n    Result<int64, String> bad = image.saveBmp("Z:/no/such/dir/x.bmp");\n    print("bad={}", bad.isErr());\n    return;\n}\n', encoding='utf-8')
    result = run(CHALI, 'run', source, '--out', work / 'io-vm')
    assert 'saved 78' in result.stdout and '4x2 10 20 30 200 100 50' in result.stdout, result.stdout
    assert 'bad=1' in result.stdout, result.stdout

def main():
    work = ROOT / 'build/chali-image'
    work.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'example/samples/std_image.chl'
    expected = (ROOT / 'example/expected/std_image.txt').read_text(encoding='utf-8')
    result = run(CHALI, 'run', source, '--out', work / 'vm')
    assert result.stdout == expected and (not result.stderr), (result.stdout, result.stderr)
    checkFileRoundtrip(work)
    sbSource = ROOT / 'example/samples/std_string_builder.chl'
    sbExpected = (ROOT / 'example/expected/std_string_builder.txt').read_text(encoding='utf-8')
    result = run(CHALI, 'run', sbSource, '--out', work / 'sb-vm')
    assert result.stdout == sbExpected and (not result.stderr), (result.stdout, result.stderr)
    print('std::image 与 StringBuilder：VM 与文件读写往返通过')

main()
