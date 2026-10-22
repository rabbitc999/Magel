"""验证不可复制资源的 Result 交接、借用和 VM 行为。"""
import argparse
from pathlib import Path
import subprocess
import tempfile

def main():
    """在临时目录中检查资源成功、错误分支与复制拒绝诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chali', default='ChaLi.exe')
    options = parser.parse_args()
    compiler = str(Path(options.chali).resolve())
    with tempfile.TemporaryDirectory(prefix='chali-opcode-resource-') as directory:
        root = Path(directory)
        source = root / 'resource.chl'
        data = (root / 'roundtrip.txt').as_posix()
        source.write_text('import std::io;\nimport std::process;\nResult<FileStream,IoError> openStream(String path) {\n    FileStream stream = FileStream::open(path,"w+")?;\n    return Result<FileStream,IoError>::Ok(move(&stream));\n}\nResult<int64,File> failOwned(File owned) {\n    return Result<int64,File>::Err(move(&owned));\n}\nResult<int64,File> propagateOwned(File owned) {\n    int64 value = failOwned(move(&owned))?;\n    return Result<int64,File>::Ok(value);\n}\nstruct FileOwner { File handle; File other; };\nint64 main() {\n    String base = "' + data + '";\n    {\n        File handles[1];\n        File original = File::new(base + ".array-a", "w");\n        handles[0] = move(&original);\n        assert(fileWrite(&handles[0], "first") == 5);\n        handles[0] = File::new(base + ".array-b", "w");\n        assert(readAllText(base + ".array-a").unwrap() == "first");\n        assert(fileWrite(&handles[0], "second") == 6);\n        Process processes[1];\n        Process process = processRun("echo array-process");\n        processes[0] = move(&process);\n        assert(processRead(&processes[0]) == "array-process");\n    }\n    assert(readAllText(base + ".array-b").unwrap() == "second");\n    {\n        FileOwner owner;\n        owner.other = File::new(base + ".field-other", "w");\n        assert(fileWrite(&owner.other, "other") == 5);\n        owner.handle = File::new(base + ".field-a", "w");\n        assert(fileWrite(&owner.handle, "first") == 5);\n        owner.handle = File::new(base + ".field-b", "w");\n        assert(readAllText(base + ".field-a").unwrap() == "first");\n        assert(fileWrite(&owner.handle, "second") == 6);\n    }\n    assert(readAllText(base + ".field-b").unwrap() == "second");\n    assert(readAllText(base + ".field-other").unwrap() == "other");\n    File failure = File::new(base + ".error", "w");\n    File errorOwner = propagateOwned(move(&failure)).takeError();\n    assert(fileWrite(&errorOwner, "error") == 5);\n    fileClose(&errorOwner);\n    File optional = File::new(base + ".option", "w");\n    Option<File> pending = Option<File>::Value(move(&optional));\n    File optionOwner = pending.take();\n    assert(fileWrite(&optionOwner, "option") == 6);\n    fileClose(&optionOwner);\n    FileStream stream = openStream("' + data + '").take();\n    assert(stream.write("resource ownership").isOk());\n    assert(stream.seek(0,SeekOrigin::Begin).isOk());\n    assert(stream.read().unwrap() == "resource ownership");\n    stream.close();\n    Result<File,String> missing = fileOpen("' + data + '.missing","r");\n    String error = missing.takeError();\n    assert(error != "");\n    Process process = processRun("echo opaque-process");\n    assert(processRead(&process) == "opaque-process");\n    assert(processClose(&process) == 0);\n    println("resource ownership OK");\n    return 0;\n}\n', encoding='utf-8')

        def run(command):
            """保留原始诊断，并在运行失败时停止测试。"""
            result = subprocess.run(command, capture_output=True, encoding='utf-8', errors='replace', timeout=300)
            assert result.returncode == 0 and '提示：程序结束时' not in result.stderr, result.stdout + result.stderr
            return result.stdout

        def vm(path):
            """让每个 VM 用例使用独立产物，允许与其他专项并行运行。"""
            return run([compiler, 'run', str(path), 'out', str(root / (path.stem + '.vm'))])
        assert 'resource ownership OK' in vm(source)
        fixture = Path(__file__).resolve().parent / 'fixtures/move_opcode.chl'
        assert 'opcode move ok' in vm(fixture)
        borrowed = fixture.parent / 'take_borrowed_payload.chl'
        assert 'borrowed take ok' in vm(borrowed)
        numbers = fixture.parent / 'opcode_number_temporaries.chl'
        assert vm(numbers).splitlines() == ['55', '11']
        members = fixture.parent / 'opcode_members.chl'
        assert 'opcode members ok' in vm(members)
        for expression in ['File value = fileOpen("unused","r").unwrap();', 'Process first; Process second = first;']:
            source.write_text('import std::io;\nimport std::process;\nint64 main() { ' + expression + ' return 0; }', encoding='utf-8')
            result = subprocess.run([compiler, '--check', str(source)], capture_output=True, encoding='utf-8', errors='replace', timeout=60)
            assert result.returncode != 0 and '复制' in result.stdout + result.stderr, result.stdout + result.stderr
    print('opcode resources: ownership transfer, borrowed calls and copy rejection passed')
if __name__ == '__main__':
    main()
