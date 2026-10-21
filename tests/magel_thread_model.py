"""隔离线程、复制参数、取消、有界通道、线程组、线程池和显式事件的端到端回归。"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

CASES = {
    'initial_and_cancel': '''
import std::thread;
import std::message;
void initial(int64 value) { std::message::sendThread<int64>(std::thread::parent(), value + 1); }
void spin() { while (true) { } }
void blocked() { String value = std::thread::receive(); }
void sleeping() { std::thread::sleepFor(60000); }
int64 main() {
    Thread task = std::thread::createWith<int64>(initial, 41);
    auto message = std::message::receiveFor<int64>(5000);
    assert(message.isReady() && message.value == 42);
    assert(std::thread::waitResult(task).isReady());
    Thread a = std::thread::create(spin);
    Thread b = std::thread::create(blocked);
    Thread c = std::thread::create(sleeping);
    assert(!std::thread::waitFor(a, 10));
    std::thread::requestStop(a);
    std::thread::requestStop(b);
    std::thread::requestStop(c);
    assert(std::thread::waitResultFor(a, 5000).status == 3);
    assert(std::thread::waitResultFor(b, 5000).status == 3);
    assert(std::thread::waitResultFor(c, 5000).status == 3);
    assert(std::thread::state(a) == std::thread::CANCELLED);
    Thread joined = std::thread::create(spin);
    std::thread::requestStop(joined);
    assert(std::thread::waitFor(joined, 5000));
    assert(std::thread::state(joined) == std::thread::CANCELLED);
    assert(std::thread::tryReceive().status == 1);
    println("ok");
    return 0;
}
''',
    'channel_bounds': '''
import std::thread;
import std::concurrency;
import std::clock;
int64 main() {
    auto channel = std::concurrency::Channel<int64>::create(1);
    assert(channel.trySend(7) == 0);
    assert(channel.trySend(8) == 4);
    int64 started = std::clock::nowNanos();
    assert(channel.sendFor(9, 20) == 1);
    assert(std::clock::nowNanos() - started >= 20000000);
    channel.close();
    assert(channel.receive().value == 7);
    assert(channel.tryReceive().status == 2);
    assert(channel.trySend(1) == 2);
    for (int64 i in std::lang::range(0, 1100)) {
        auto next = std::concurrency::Channel<int64>::create(1);
        next.close();
    }
    std::thread::closeInbox();
    assert(std::thread::tryReceive().status == 2);
    println("ok");
    return 0;
}
''',
    'channel_parallel': '''
import std::thread;
import std::concurrency;
struct Work { std::concurrency::Channel<int64> jobs; std::concurrency::AtomicInt64 sum; };
void consume(Work work) {
    while (true) {
        auto item = work.jobs.receive();
        if (!item.isReady()) { return; }
        work.sum.fetchAdd(item.value);
    }
}
int64 main() {
    Work work;
    work.jobs = std::concurrency::Channel<int64>::create(3);
    work.sum = std::concurrency::AtomicInt64::create(0);
    ThreadGroup group;
    for (int64 i in std::lang::range(0, 4)) { group.add(std::thread::createWith<Work>(consume, work)); }
    for (int64 i in std::lang::range(1, 101)) { assert(work.jobs.send(i) == 0); }
    work.jobs.close();
    assert(group.waitAll());
    assert(work.sum.get() == 5050);
    println("ok");
    return 0;
}
''',
    'group_failure': '''
import std::thread;
void spin() { while (true) {} }
void bad() { List<int64> values; values.at(9); }
int64 main() {
    ThreadGroup group;
    Thread other = group.start(spin);
    group.start(bad);
    assert(!group.waitAll());
    assert(len(group.error) > 0);
    assert(std::thread::state(other) == std::thread::CANCELLED);
    assert(!group.waitAll());
    println("ok");
    return 0;
}
''',
    'pool_drain': '''
import std::thread;
void job() { std::thread::send(std::thread::parent(), "job"); }
int64 main() {
    ThreadPool pool = ThreadPool::create(3, 2);
    for (int64 i in std::lang::range(0, 12)) { assert(pool.submit(job) == 0); }
    assert(pool.closeAndWait());
    for (int64 i in std::lang::range(0, 12)) {
        auto result = std::thread::tryReceive();
        assert(result.isReady() && result.value == "job");
    }
    assert(pool.submit(job) == 2);
    println("ok");
    return 0;
}
''',
    'pool_failure': '''
import std::thread;
void bad() { List<int64> values; values.at(9); }
int64 main() {
    ThreadPool pool = ThreadPool::create(2, 2);
    assert(pool.submit(bad) == 0);
    assert(!pool.closeAndWait());
    println("ok");
    return 0;
}
''',
    'events': '''
import std::event;
int64 main() {
    assert(std::event::post(3, 8) == 0);
    auto next = std::event::tryReceive();
    assert(next.isReady() && next.value.tag == 3 && next.value.source == 8);
    Timer timer = Timer::afterMillis(2);
    auto tick = std::event::receiveFor(5000);
    assert(tick.isReady() && tick.value.timer == timer.id());
    timer.stop();
    assert(std::event::tryReceive().status == 1);
    std::event::close();
    assert(std::event::post(0, 1) == 2);
    println("ok");
    return 0;
}
''',
}

CASES['main_exit'] = '''import std::thread;
void spin() { while (true) {} }
int64 main() { Thread t = std::thread::create(spin); println("ok"); return 0; }'''

CASES['waiter_cancel'] = r'''
import std::thread;
import std::message;
void spin() { while (true) {} }
void waiter(Thread target) { std::thread::wait(target); }
int64 main() {
    for (int64 i in std::lang::range(0, 12)) {
        Thread target = std::thread::create(spin);
        Thread joining = std::thread::createWith<Thread>(waiter, target);
        std::thread::sleepFor(2);
        std::thread::requestStop(joining);
        assert(std::thread::waitResult(joining).status == 3);
        std::thread::requestStop(target);
        assert(std::thread::waitResult(target).status == 3);
    }
    println("ok");
    return 0;
}
'''
CASES['mailbox_capacity'] = r'''
import std::thread;
import std::message;
void spin() { while (true) {} }
int64 main() {
    Thread target = std::thread::create(spin);
    for (int64 i in std::lang::range(0, 256)) { assert(std::message::trySend<int64>(target, i) == 0); }
    assert(std::message::trySend<int64>(target, 257) == 4);
    assert(std::message::sendFor<int64>(target, 257, 5) == 1);
    std::thread::requestStop(target);
    assert(std::thread::waitResult(target).status == 3);
    assert(std::message::trySend<int64>(target, 0) == 2);
    println("ok");
    return 0;
}
'''
CASES['channel_close_waiters'] = r'''
import std::thread;
import std::message;
import std::concurrency;
void producer(std::concurrency::Channel<int64> queue) {
    std::message::sendThread<int64>(std::thread::parent(), queue.send(7));
}
void consumer(std::concurrency::Channel<int64> queue) {
    std::message::sendThread<int64>(std::thread::parent(), queue.receive().status);
}
int64 main() {
    for (int64 i in std::lang::range(0, 10)) {
        auto full = std::concurrency::Channel<int64>::create(1);
        full.send(1);
        Thread writer = std::thread::createWith<std::concurrency::Channel<int64>>(producer, full);
        std::thread::sleepFor(2);
        full.close();
        assert(full.receive().value == 1);
        assert(std::message::receiveFor<int64>(5000).value == 2);
        assert(std::thread::waitResult(writer).isReady());
        auto empty = std::concurrency::Channel<int64>::create(1);
        Thread reader = std::thread::createWith<std::concurrency::Channel<int64>>(consumer, empty);
        std::thread::sleepFor(2);
        empty.close();
        assert(std::message::receiveFor<int64>(5000).value == 2);
        assert(std::thread::waitResult(reader).isReady());
    }
    std::concurrency::Channel<int64> invalid;
    assert(invalid.tryReceive().status == 6);
    invalid.close();
    assert(std::thread::tryReceive().status == 1);
    println("ok");
    return 0;
}
'''

CASES['cancel_waits'] = r'''
import std::thread;
import std::message;
import std::event;
import std::concurrency;
void send(std::concurrency::Channel<int64> queue) {
    std::message::sendThread<int64>(std::thread::parent(), 1);
    queue.send(2);
}
void receive(std::concurrency::Channel<int64> queue) {
    std::message::sendThread<int64>(std::thread::parent(), 1);
    queue.receive();
}
void eventWait() {
    std::message::sendThread<int64>(std::thread::parent(), 1);
    std::event::receive();
}
int64 main() {
    auto full = std::concurrency::Channel<int64>::create(1);
    full.send(1);
    Thread writer = std::thread::createWith<std::concurrency::Channel<int64>>(send, full);
    assert(std::message::receiveFor<int64>(5000).isReady());
    std::thread::sleepFor(2);
    std::thread::requestStop(writer);
    assert(std::thread::waitResultFor(writer, 5000).status == 3);
    auto empty = std::concurrency::Channel<int64>::create(1);
    Thread reader = std::thread::createWith<std::concurrency::Channel<int64>>(receive, empty);
    assert(std::message::receiveFor<int64>(5000).isReady());
    std::thread::sleepFor(2);
    std::thread::requestStop(reader);
    assert(std::thread::waitResultFor(reader, 5000).status == 3);
    Thread events = std::thread::create(eventWait);
    assert(std::message::receiveFor<int64>(5000).isReady());
    std::thread::sleepFor(2);
    std::thread::requestStop(events);
    assert(std::thread::waitResultFor(events, 5000).status == 3);
    full.close();
    assert(full.receive().value == 1);
    empty.close();
    println("ok");
    return 0;
}
'''


def main():
    """在独立临时目录验证普通和压缩符号，所有进程均有超时。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', default='magel')
    parser.add_argument('--vm', type=Path)
    args = parser.parse_args()
    compiler = Path(args.magel).resolve()
    if not compiler.exists():
        compiler = compiler.with_suffix('.exe')
    environment = dict(os.environ, MAGEL_LANG='zh', PYTHONUTF8='1')
    with tempfile.TemporaryDirectory(prefix='magel-thread-model-') as folder:
        directory = Path(folder)
        for name, code in CASES.items():
            source = directory / (name + '.mgl')
            source.write_text(code, encoding='utf-8')
            for compact in (False, True):
                command = [str(compiler), 'run', str(source), '--threads',
                           '--compress-symbols' if compact else '--no-compress-symbols']
                result = subprocess.run(command, cwd=directory, capture_output=True,
                                        text=True, encoding='utf-8', env=environment, timeout=60)
                assert result.returncode == 0, (name, compact, result.stdout, result.stderr)
                assert result.stdout.strip().endswith('ok'), (name, result.stdout)
            print(name, 'passed', flush=True)
        if args.vm:
            vm = args.vm.resolve()
            for name in ('channel_parallel', 'pool_drain', 'events'):
                source = directory / (name + '.mgl')
                prefix = directory / (name + '-binary')
                built = subprocess.run([str(compiler), 'svmb', str(source), '--threads', '--out', str(prefix)],
                                       cwd=directory, capture_output=True, text=True, encoding='utf-8',
                                       env=environment, timeout=60)
                assert built.returncode == 0, built.stderr
                loaded = subprocess.run([str(vm), 'run', str(prefix.with_suffix('.svmb')), '--threads',
                                         '--data', str(prefix.with_suffix('.data.bin')),
                                         '--layout', str(prefix.with_suffix('.layout'))],
                                        cwd=directory, capture_output=True, text=True, encoding='utf-8',
                                        env=environment, timeout=60)
                assert loaded.returncode == 0 and 'ok' in loaded.stdout, (name, loaded.stdout, loaded.stderr)
        rejected = {
            'group_self': 'import std::thread; int64 main() { ThreadGroup g; g.add(std::thread::current()); return 0; }',
            'group_duplicate': 'import std::thread; void f() {} int64 main() { ThreadGroup g; Thread t = g.start(f); g.add(t); return 0; }',
            'nominal_handle': 'import std::thread; int64 main() { Thread task = 1; return 0; }',
            'nested_owner_transfer': 'import std::thread; import std::event; struct Box { Timer timer; }; void f(Box b) {} int64 main() { Box b; std::thread::createWith<Box>(f, b); return 0; }',
            'owner_transfer': 'import std::thread; void f(ThreadGroup g) {} int64 main() { ThreadGroup g; std::thread::createWith<ThreadGroup>(f, g); return 0; }',
            'resource_transfer': 'import std::thread; import std::net; void f(TcpSocket s) {} int64 main() { TcpSocket s; std::thread::createWith<TcpSocket>(f, s); return 0; }',
            'removed_module': 'import std::coroutine; int64 main() { return 0; }',
            'removed_callback': 'import std::event; int64 main() { Timer t; t.onTick(void fn(int64 value) {}); return 0; }',
            'borrowed_entry': 'import std::thread; int64 main() { int64 x=1; auto f=void fn(){println("{}",x);}; std::thread::create(f); return 0; }',
        }
        for name, code in rejected.items():
            source = directory / (name + '.mgl')
            code = code.replace('; ', ';\n')
            source.write_text(code, encoding='utf-8')
            result = subprocess.run([str(compiler), 'run', str(source), '--threads'], cwd=directory,
                                    capture_output=True, text=True, encoding='utf-8', env=environment, timeout=30)
            assert result.returncode != 0 and result.stderr, (name, result)
            expected = {
                'group_self': ('线程组不能接管',),
                'group_duplicate': ('重复登记',),
                'nominal_handle': ('类型不匹配', '不能', '隐式转换'),
                'owner_transfer': ('不能跨线程传递',),
                'nested_owner_transfer': ('不能跨线程传递',),
                'resource_transfer': ('NonCopyable', '不能跨线程传递'),
                'removed_module': ('std::coroutine',),
                'removed_callback': ('onTick',),
                'borrowed_entry': ('捕获',),
            }[name]
            assert any(marker in result.stderr for marker in expected), (name, result.stderr)
    print('thread model regression passed')


if __name__ == '__main__':
    main()
