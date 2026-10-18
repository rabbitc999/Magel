"""软重启看门狗（PC 宿主）回归：`--watchdog` 打开前后的行为、输出回滚与重启上限。

判据：
  1. 不开 `--watchdog`（缺省）：程序里的 `requestRestart()` 照旧是普通运行错误（退出码非 0），
     行为与从前一致；
  2. 开 `--watchdog`：可恢复终止的那一轮**回滚输出**（`tail` 不该打出来），宿主复用缓冲区
     重跑，逐轮报"软重启 i/N（原因）"，用尽后报"软重启次数用尽"并非零退出；
  3. `--watchdog --watchdog-watermark N`：宿主量到超水位也重启。
  4. 时间维度（`enableTimeout*()`）默认不开：没开时 `kick()` 报 `Err(NotEnabled)`、非正超时
     报 `Err(InvalidTimeout)`；开了之后定时喂狗就一次都不触发（程序正常跑完）。
  5. 开了超时却不喂：整整一个超时周期没有 `kick()`，程序自己把这一轮重启掉 —— 走同一个
     软重启通道，被中止那一轮的输出同样回滚。
"""

from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""

PROBE = """import std::watchdog;

int64 main() {
    std::lang::sweep(); // 模块初始化也可能产生已释放临时块；先建立本轮清扫边界。
    match (std::watchdog::enable(0)) {
        Result<int64, std::watchdog::WatchdogError>::Ok(w) => { std::lang::println("on {}", w); }
        Result<int64, std::watchdog::WatchdogError>::Err(reason) => {
            match (reason) {
                std::watchdog::WatchdogError::InvalidWatermark => { std::lang::println("bad"); }
                std::watchdog::WatchdogError::InvalidTimeout => { std::lang::println("bad timeout"); }
                std::watchdog::WatchdogError::NotEnabled => { std::lang::println("off"); }
            }
        }
    }
    int64 mut& retired = heap int64;
    delete retired;
    std::watchdog::requestRestart();
    std::lang::println("tail");
    return 0;
}
"""

# 时间维度②：开了超时却一直不喂狗 —— 一整段没有 `kick()`，看门狗自己把这一轮重启掉。
TIMEOUT_PROBE = """import std::watchdog;

int64 main() {
    match (std::watchdog::enableTimeout(500)) {
        Result<int64, std::watchdog::WatchdogError>::Ok(units) => { std::lang::println("on {}", units); }
        Result<int64, std::watchdog::WatchdogError>::Err(reason) => {
            match (reason) {
                std::watchdog::WatchdogError::InvalidTimeout => { std::lang::println("bad timeout"); }
                std::watchdog::WatchdogError::InvalidWatermark => { std::lang::println("bad watermark"); }
                std::watchdog::WatchdogError::NotEnabled => { std::lang::println("off"); }
            }
        }
    }
    int64 i = 0;
    while (i < 100000) { i = i + 1; }
    std::lang::println("tail");
    return 0;
}
"""

# 时间维度①：默认关（kick 报 NotEnabled、非法超时被拒）；②：定时喂狗则一次都不触发。
KICK_PROBE = """import std::watchdog;

int64 main() {
    Result<bool, std::watchdog::WatchdogError> before = std::watchdog::kick();
    std::lang::println("kick-before-enable isErr={}", before.isErr());

    Result<int64, std::watchdog::WatchdogError> zero = std::watchdog::enableTimeout(0);
    std::lang::println("enableTimeout(0) isErr={}", zero.isErr());

    Result<int64, std::watchdog::WatchdogError> negative = std::watchdog::enableTimeoutMillis(-5);
    std::lang::println("enableTimeoutMillis(-5) isErr={}", negative.isErr());

    match (std::watchdog::enableTimeout(1000)) {
        Result<int64, std::watchdog::WatchdogError>::Ok(units) => { std::lang::println("on {}", units); }
        Result<int64, std::watchdog::WatchdogError>::Err(reason) => {
            match (reason) {
                std::watchdog::WatchdogError::InvalidTimeout => { std::lang::println("bad timeout"); }
                std::watchdog::WatchdogError::InvalidWatermark => { std::lang::println("bad watermark"); }
                std::watchdog::WatchdogError::NotEnabled => { std::lang::println("off"); }
            }
        }
    }
    int64 done = 0;
    while (done < 3000) {
        std::watchdog::kick();
        done = done + 1;
    }
    Result<bool, std::watchdog::WatchdogError> after = std::watchdog::kick();
    std::lang::println("kicked {} isOk={}", done, after.isOk());
    return 0;
}
"""


def run(args, timeout=300):
    """执行命令并保留诊断。"""
    command = [str(a) for a in args]
    command[2:2] = ["--out", str(ROOT / "build/magel-watchdog/program")]
    return subprocess.run(command, cwd=ROOT, capture_output=True,
                          encoding="utf-8", errors="replace", timeout=timeout)


def main():
    """跑 PC 宿主的软重启回归。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / ("magel" + EXE))
    args = parser.parse_args()
    compiler = args.magel.resolve()
    if not compiler.exists():
        raise SystemExit(f"找不到 magel 编译器：{compiler}")
    work = ROOT / "build/magel-watchdog"
    work.mkdir(parents=True, exist_ok=True)
    probe = work / "probe.mgl"
    probe.write_text(PROBE, encoding="utf-8")

    # 1. 缺省（看门狗关闭）：与从前一致 —— 普通运行错误、退出码非 0。
    off = run([compiler, "run", probe])
    assert off.returncode != 0, off.stdout + off.stderr
    assert "可恢复终止" in off.stderr and "软重启 " not in off.stderr, off.stderr
    print("[通过] 看门狗关闭（缺省）：可恢复终止按普通运行错误报，行为不变")

    # 2. 打开看门狗：回滚被中止那一轮的输出 + 逐轮重启 + 用尽后非零退出。
    on = run([compiler, "run", "--watchdog", "--max-restarts", "1", probe])
    assert "软重启 1/1（程序显式请求）" in on.stderr, on.stderr
    assert "软重启次数用尽" in on.stderr, on.stderr
    assert on.returncode != 0, "重启次数用尽时应当报失败"
    assert "tail" not in on.stdout, f"被中止那一轮的输出没有回滚：{on.stdout}"
    print("[通过] --watchdog：输出回滚 + 复用缓冲区重跑 + 重启次数用尽")

    # 3. 水位触发：宿主量到超水位也重启（这里水位设 1 字节，任何一轮都会超）。
    water = run([compiler, "run", "--watchdog", "--watchdog-watermark", "1",
                 "--max-restarts", "1", ROOT / "example/samples/hello.mgl"])
    assert "软重启 1/1（内存水位超限）" in water.stderr, water.stderr
    assert "软重启次数用尽" in water.stderr, water.stderr
    print("[通过] --watchdog-watermark：宿主量水位触发重启")

    # 4. 时间维度：默认不开 + 非法超时被拒 + 定时喂狗不触发。
    kick = work / "kick.mgl"
    kick.write_text(KICK_PROBE, encoding="utf-8")
    fed = run([compiler, "run", kick])
    assert fed.returncode == 0, fed.stdout + fed.stderr
    for expected_line in ("kick-before-enable isErr=1", "enableTimeout(0) isErr=1",
                          "enableTimeoutMillis(-5) isErr=1", "on 1000", "kicked 3000 isOk=1"):
        assert expected_line in fed.stdout, f"缺 {expected_line}：{fed.stdout}"
    assert "软重启" not in fed.stderr and "可恢复终止" not in fed.stderr, fed.stderr
    print("[通过] 时间维度默认关闭（kick 报 NotEnabled、非正超时被拒）；定时喂狗不触发")

    # 5. 开了超时却不喂：自己触发软重启（同一个通道），被中止那一轮输出回滚。
    timeout = work / "timeout.mgl"
    timeout.write_text(TIMEOUT_PROBE, encoding="utf-8")
    silent = run([compiler, "run", timeout])
    assert silent.returncode != 0, silent.stdout + silent.stderr
    assert "可恢复终止" in silent.stderr, silent.stderr
    assert "on 500" in silent.stdout, f"没开看门狗时输出不回滚：{silent.stdout}"
    armed = run([compiler, "run", "--watchdog", "--max-restarts", "1", timeout])
    assert "软重启 1/1（程序显式请求）" in armed.stderr, armed.stderr
    assert "软重启次数用尽" in armed.stderr, armed.stderr
    assert armed.returncode != 0, "重启次数用尽时应当报失败"
    assert "tail" not in armed.stdout, f"被中止那一轮的输出没有回滚：{armed.stdout}"
    print("[通过] 时间维度：超时未喂狗自己触发软重启（回滚输出 + 重启上限）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
