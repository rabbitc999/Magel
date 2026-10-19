"""验证不可复制资源的 Result 交接、借用和 VM/AOT 行为。"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    """在临时目录中检查资源成功、错误分支与复制拒绝诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', default='magel.exe')
    parser.add_argument('--aot', action='store_true')
    options = parser.parse_args()
    compiler = str(Path(options.magel).resolve())
    with tempfile.TemporaryDirectory(prefix='magel-opcode-resource-') as directory:
        root = Path(directory)
        source = root / 'resource.mgl'
        data = (root / 'roundtrip.txt').as_posix()
        source.write_text('''import std::io;
import std::process;
Result<FileStream,IoError> openStream(String path) {
    FileStream stream = FileStream::open(path,"w+")?;
    return Result<FileStream,IoError>::Ok(move(&stream));
}
Result<int64,File> failOwned(File owned) {
    return Result<int64,File>::Err(move(&owned));
}
Result<int64,File> propagateOwned(File owned) {
    int64 value = failOwned(move(&owned))?;
    return Result<int64,File>::Ok(value);
}
struct FileOwner { File handle; File other; };
int64 main() {
    String base = "''' + data + '''";
    {
        File handles[1];
        File original = File::new(base + ".array-a", "w");
        handles[0] = move(&original);
        assert(fileWrite(&handles[0], "first") == 5);
        handles[0] = File::new(base + ".array-b", "w");
        assert(readAllText(base + ".array-a").unwrap() == "first");
        assert(fileWrite(&handles[0], "second") == 6);
        Process processes[1];
        Process process = processRun("echo array-process");
        processes[0] = move(&process);
        assert(processRead(&processes[0]) == "array-process");
    }
    assert(readAllText(base + ".array-b").unwrap() == "second");
    {
        FileOwner owner;
        owner.other = File::new(base + ".field-other", "w");
        assert(fileWrite(&owner.other, "other") == 5);
        owner.handle = File::new(base + ".field-a", "w");
        assert(fileWrite(&owner.handle, "first") == 5);
        owner.handle = File::new(base + ".field-b", "w");
        assert(readAllText(base + ".field-a").unwrap() == "first");
        assert(fileWrite(&owner.handle, "second") == 6);
    }
    assert(readAllText(base + ".field-b").unwrap() == "second");
    assert(readAllText(base + ".field-other").unwrap() == "other");
    File failure = File::new(base + ".error", "w");
    File errorOwner = propagateOwned(move(&failure)).takeError();
    assert(fileWrite(&errorOwner, "error") == 5);
    fileClose(&errorOwner);
    File optional = File::new(base + ".option", "w");
    Option<File> pending = Option<File>::Value(move(&optional));
    File optionOwner = pending.take();
    assert(fileWrite(&optionOwner, "option") == 6);
    fileClose(&optionOwner);
    FileStream stream = openStream("''' + data + '''").take();
    assert(stream.write("resource ownership").isOk());
    assert(stream.seek(0,SeekOrigin::Begin).isOk());
    assert(stream.read().unwrap() == "resource ownership");
    stream.close();
    Result<File,String> missing = fileOpen("''' + data + '''.missing","r");
    String error = missing.takeError();
    assert(error != "");
    Process process = processRun("echo opaque-process");
    assert(processRead(&process) == "opaque-process");
    assert(processClose(&process) == 0);
    println("resource ownership OK");
    return 0;
}
''', encoding='utf-8')
        def run(command):
            """保留原始诊断，并在运行失败时停止测试。"""
            result = subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=300)
            assert result.returncode == 0 and "提示：程序结束时" not in result.stderr, result.stdout + result.stderr
            return result.stdout
        def vm(path):
            """让每个 VM 用例使用独立产物，允许与其他专项并行运行。"""
            return run([compiler,'run',str(path),'out',str(root/(path.stem+'.vm'))])
        assert 'resource ownership OK' in vm(source)
        fixture = Path(__file__).resolve().parent / 'fixtures/move_opcode.mgl'
        assert 'opcode move ok' in vm(fixture)
        borrowed = fixture.parent/'take_borrowed_payload.mgl'
        assert 'borrowed take ok' in vm(borrowed)
        numbers = fixture.parent/'opcode_number_temporaries.mgl'
        assert vm(numbers).splitlines() == ['55', '11']
        members = fixture.parent/'opcode_members.mgl'
        assert 'opcode members ok' in vm(members)
        if options.aot:
            executable = root / 'resource.exe'
            run([compiler,'aot',str(source),'out',str(executable),'checked'])
            assert 'resource ownership OK' in run([str(executable)])
            run([compiler,'aot',str(fixture),'out',str(executable),'checked'])
            assert 'opcode move ok' in run([str(executable)])
            run([compiler,'aot',str(numbers),'out',str(executable),'checked'])
            assert run([str(executable)]).splitlines() == ['55', '11']
            glue = Path(__file__).resolve().parents[1]/'source/glue/cpp_glue.cpp'
            run([compiler,'aot',str(members),'--glue',str(glue),'out',str(executable),'checked'])
            assert 'opcode members ok' in run([str(executable)])
        for expression in ['File value = fileOpen("unused","r").unwrap();',
                           'Process first; Process second = first;']:
            source.write_text('import std::io;\nimport std::process;\nint64 main() { '+expression+' return 0; }',encoding='utf-8')
            result = subprocess.run([compiler,'--check',str(source)],capture_output=True,encoding='utf-8',errors='replace',timeout=60)
            assert result.returncode != 0 and ('复制' in result.stdout+result.stderr), result.stdout+result.stderr
    print('opcode resources: ownership transfer, borrowed calls and copy rejection passed')


if __name__ == '__main__':
    main()
