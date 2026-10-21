"""用同一份 C++ 胶水验证 VM 的同步函数调用、重入、所有权和错误传播。"""
import os
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/magel-native-callbacks'
COMPILER = Path(os.environ.get('MAGEL_COMPILER', ROOT / ('magel.exe' if os.name == 'nt' else 'magel')))
EXE = '.exe' if os.name == 'nt' else ''
EXAMPLE = ROOT / 'example/native-callbacks'
EXPECTED = 'callbacks: named inline copy move float32 bool void reference higher-order reentry OK\n'

def run(*arguments, success=True):
    """运行工具并在断言失败时保留命令、退出码和原始诊断。"""
    result = subprocess.run([str(value) for value in arguments], cwd=ROOT, env=dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1'), capture_output=True, encoding='utf-8', errors='replace', timeout=360)
    assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout, result.stderr)
    return result

def emit(source, name, level=2):
    """输出独立字节码、数据和布局，避免借用编译器内置演示胶水。"""
    prefix = WORK / name
    run(COMPILER, 'emit', source, '--svmb', '--out', prefix)
    return [prefix.with_suffix('.svmb'), prefix.with_suffix('.data.bin'), prefix.with_suffix('.layout')]

def main():
    """覆盖成功与失败路径；实际编译两种运行时并执行同一程序。"""
    WORK.mkdir(parents=True, exist_ok=True)
    glue_source = (EXAMPLE / 'glue.cpp').read_text(encoding='utf-8')
    glue_source = glue_source.replace('bool bindMagelGlue(', 'bool bindExampleGlue(') + '\n/**\n * @brief 注册示例及用于核对失败传播的测试入口。\n * @param registry 待安装的宿主注册表。\n * @param error 绑定失败时接收诊断。\n * @return 全部绑定成功返回 true。\n */\nbool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error) {\n    return bindExampleGlue(registry, error)\n        && registry.bindTypedWithContext<std::int64_t, IntegerFunction>("hostNull",\n            [](magel_glue::Context &context, IntegerFunction) {\n                magel_glue::Value result = magel_glue::Value::fromInt(99);\n                IntegerFunction empty(&context, 0);\n                if (empty.tryInvoke(result, 1) || result.asInt() != 0) context.fail("空函数检查错误");\n                return std::int64_t{};\n            }, error)\n        && registry.bindTypedWithContext<std::int64_t, IntegerFunction, std::int64_t>("hostArity",\n            [](magel_glue::Context &context, IntegerFunction function, std::int64_t count) {\n                magel_glue::Value result;\n                Memory::Vector<magel_glue::Value> arguments;\n                for (std::int64_t index = 0; index < count; ++index) arguments.push_back(magel_glue::Value::fromInt(index));\n                context.invoke(function.address(), arguments, result);\n                return std::int64_t{};\n            }, error)\n        && registry.bindTypedWithContext<void>("hostReject", [](magel_glue::Context &context) {\n                context.fail("native callback rejected");\n            }, error);\n}\n'
    glue = WORK / 'glue.cpp'
    glue.write_text(glue_source, encoding='utf-8')
    host_source = WORK / 'vm_host.cpp'
    host_source.write_text((EXAMPLE / 'vm_host.cpp').read_text(encoding='utf-8'), encoding='utf-8')
    host = WORK / ('vm-host' + EXE)
    run(os.environ.get('CXX', 'g++'), '-std=c++17', '-O2', '-pthread', '-DMAGEL_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1', '-DSTACKVM_WITH_BIGINT=0', '-DSTACKVM_WITH_BIGFLOAT=0', '-Isource', '-Ilibrary', '-Ilibrary/stackvm', host_source, ROOT / 'library/stackvm/stackvm.cpp', ROOT / 'library/stackvm/event_service.cpp', *[ROOT / 'source/glue' / name for name in ('std_collection.cpp', 'std_collection_storage.cpp', 'std_native_record.cpp')], '-o', host)
    for level in (2,):
        images = emit(EXAMPLE / 'demo.mgl', f'demo-O{level}', level)
        vm = run(host, *images)
        assert vm.stdout == EXPECTED and not vm.stderr, vm
    failures = WORK / 'failures.mgl'
    failures.write_text('\nint64 hostApply(int64 fn(int64) callback,int64 value);\nint64 hostNull(int64 fn(int64) callback);\nint64 hostArity(int64 fn(int64) callback,int64 count);\nvoid hostReject();\nint64 identity(int64 value) { return value; }\nint64 main(List<String> args) {\n    if (args[0] == "null") { hostNull(identity); }\n    if (args[0] == "arity") { hostArity(identity, 0); }\n    if (args[0] == "extra") { hostArity(identity, 2); }\n    if (args[0] == "panic") { hostApply(int64 fn(int64 x) { panic("Magel callback panic"); return x; }, 1); }\n    if (args[0] == "native") { hostApply(int64 fn(int64 x) { hostReject(); return x; }, 1); }\n    println("unexpected callback success");\n    return 0;\n}\n', encoding='utf-8')
    images = emit(failures, 'failures')
    for mode, marker in (('null', '空函数指针'), ('arity', '参数'), ('extra', '参数'), ('panic', 'Magel callback panic'), ('native', 'native callback rejected')):
        for backend, command in (('VM', [host, *images]),):
            result = run(*command, mode, success=False)
            assert marker in result.stderr and 'unexpected callback success' not in result.stdout, (backend, mode, result)
            assert '无法调用指定函数指针' not in result.stderr, (backend, mode, result.stderr)
if __name__ == '__main__':
    main()
