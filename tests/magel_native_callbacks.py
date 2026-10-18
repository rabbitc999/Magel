"""用同一份 C++ 胶水验证 VM 与 AOT 的同步函数调用、重入、所有权和错误传播。"""
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
    result = subprocess.run([str(value) for value in arguments], cwd=ROOT,
                            env=dict(os.environ, MAGEL_LANG='zh', MAGEL_NO_STDIN_INPUT='1'),
                            capture_output=True, encoding='utf-8', errors='replace', timeout=360)
    assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout, result.stderr)
    return result


def emit(source, name, level=2):
    """输出独立字节码、数据和布局，避免借用编译器内置演示胶水。"""
    prefix = WORK / name
    run(COMPILER, 'emit', source, '--svmb', f'--opt-level={level}', '--out', prefix)
    return [prefix.with_suffix('.svmb'), prefix.with_suffix('.data.bin'), prefix.with_suffix('.layout')]


def main():
    """覆盖成功与失败路径；实际编译两种运行时并执行同一程序。"""
    WORK.mkdir(parents=True, exist_ok=True)
    glue_source = (EXAMPLE / 'glue.cpp').read_text(encoding='utf-8')
    # 失败绑定只供门禁使用，交付示例不需要暴露测试入口。
    glue_source = glue_source.replace('bool bindMagelGlue(', 'bool bindExampleGlue(') + r'''
/**
 * @brief 注册示例及用于核对失败传播的测试入口。
 * @param registry 待安装的宿主注册表。
 * @param error 绑定失败时接收诊断。
 * @return 全部绑定成功返回 true。
 */
bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error) {
    return bindExampleGlue(registry, error)
        && registry.bindTypedWithContext<std::int64_t, IntegerFunction>("hostNull",
            [](magel_glue::Context &context, IntegerFunction) {
                magel_glue::Value result = magel_glue::Value::fromInt(99);
                IntegerFunction empty(&context, 0);
                if (empty.tryInvoke(result, 1) || result.asInt() != 0) context.fail("空函数检查错误");
                return std::int64_t{};
            }, error)
        && registry.bindTypedWithContext<std::int64_t, IntegerFunction, std::int64_t>("hostArity",
            [](magel_glue::Context &context, IntegerFunction function, std::int64_t count) {
                magel_glue::Value result;
                Memory::Vector<magel_glue::Value> arguments;
                for (std::int64_t index = 0; index < count; ++index) arguments.push_back(magel_glue::Value::fromInt(index));
                context.invoke(function.address(), arguments, result);
                return std::int64_t{};
            }, error)
        && registry.bindTypedWithContext<void>("hostReject", [](magel_glue::Context &context) {
                context.fail("native callback rejected");
            }, error);
}
'''
    glue = WORK / 'glue.cpp'
    glue.write_text(glue_source, encoding='utf-8')
    host_source = WORK / 'vm_host.cpp'
    host_source.write_text((EXAMPLE / 'vm_host.cpp').read_text(encoding='utf-8'), encoding='utf-8')
    host = WORK / ('vm-host' + EXE)
    run(os.environ.get('CXX', 'g++'), '-std=c++17', '-O2', '-pthread',
        '-DMAGEL_WITH_REFLECTION=1', '-DSTACKVM_WITH_REFLECTION=1',
        '-DSTACKVM_WITH_BIGINT=0', '-DSTACKVM_WITH_BIGFLOAT=0',
        '-Isource', '-Ilibrary', '-Ilibrary/stackvm', host_source,
        ROOT / 'library/stackvm/stackvm.cpp', ROOT / 'library/stackvm/stackvm_glue.cpp', '-o', host)
    for level in (0, 2):
        images = emit(EXAMPLE / 'demo.mgl', f'demo-O{level}', level)
        vm = run(host, *images)
        executable = WORK / ('aot-O' + str(level) + EXE)
        run(COMPILER, 'aot', EXAMPLE / 'demo.mgl', '--glue', glue,
            f'--opt-level={level}', '--out', executable)
        aot = run(executable)
        assert vm.stdout == aot.stdout == EXPECTED and not vm.stderr and not aot.stderr, (vm, aot)
        print(f'[通过] VM/AOT O{level}：十种回调、资源清理及多层重入一致', flush=True)
    failures = WORK / 'failures.mgl'
    failures.write_text('''
int64 hostApply(int64 fn(int64) callback,int64 value);
int64 hostNull(int64 fn(int64) callback);
int64 hostArity(int64 fn(int64) callback,int64 count);
void hostReject();
int64 identity(int64 value) { return value; }
int64 main(List<String> args) {
    if (args[0] == "null") { hostNull(identity); }
    if (args[0] == "arity") { hostArity(identity, 0); }
    if (args[0] == "extra") { hostArity(identity, 2); }
    if (args[0] == "panic") { hostApply(int64 fn(int64 x) { panic("Magel callback panic"); return x; }, 1); }
    if (args[0] == "native") { hostApply(int64 fn(int64 x) { hostReject(); return x; }, 1); }
    println("unexpected callback success");
    return 0;
}
''', encoding='utf-8')
    images = emit(failures, 'failures')
    executable = WORK / ('failures-aot' + EXE)
    run(COMPILER, 'aot', failures, '--glue', glue, '--out', executable)
    for mode, marker in (('null', '空函数指针'), ('arity', '参数'), ('extra', '参数'),
                         ('panic', 'Magel callback panic'), ('native', 'native callback rejected')):
        for backend, command in (('VM', [host, *images]), ('AOT', [executable])):
            result = run(*command, mode, success=False)
            assert marker in result.stderr and 'unexpected callback success' not in result.stdout, (backend, mode, result)
            assert '无法调用指定函数指针' not in result.stderr, (backend, mode, result.stderr)
        print(f'[通过] VM/AOT 失败传播一致：{mode}', flush=True)


if __name__ == '__main__':
    main()
