"""使用实际子进程验证并行、屏障、失败汇总和构建失败阻断。"""
import argparse
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from parallel_gates import independent, positive_jobs, run_tasks


class ParallelGatesTest(unittest.TestCase):
    """从子进程可观察行为检查门禁调度约束。"""

    def test_parallel_and_barrier(self):
        """两个任务互相等待以证明并发，独占屏障检查前组结束及后组尚未开始。"""
        with tempfile.TemporaryDirectory(prefix="gate tests ") as directory:
            work = Path(directory)
            script = work / "task.py"
            script.write_text('''import pathlib, sys, time
root = pathlib.Path(sys.argv[1])
name = sys.argv[2]
(root / (name + ".start")).touch()
if name in ("a", "b"):
    other = "b" if name == "a" else "a"
    deadline = time.monotonic() + 10
    while not (root / (other + ".start")).exists():
        assert time.monotonic() < deadline, "没有并行执行"
        time.sleep(.01)
if name == "barrier":
    assert all((root / (item + ".done")).exists() for item in ("a", "b"))
    assert not (root / "after.start").exists()
if name == "after":
    assert (root / "barrier.done").exists()
(root / (name + ".done")).touch()
''', encoding="utf-8")
            tasks = [dict(title=name, command=[sys.executable, str(script), str(work), name],
                          parallel=name != "barrier") for name in ("a", "b", "barrier", "after")]
            with redirect_stdout(io.StringIO()):
                results = run_tasks(tasks, work / "logs", 2, work)
            self.assertEqual([item["exit_code"] for item in results], [0] * 4)
            saved = json.loads((work / "logs/results.json").read_text(encoding="utf-8"))
            self.assertEqual([item["index"] for item in saved["results"]], [1, 2, 3, 4])
            self.assertEqual(len({item["log"] for item in results}), 4)

    def test_failure_and_required_build(self):
        """普通失败不阻断独立测试，必需构建失败则阻断后续旧产物测试。"""
        with tempfile.TemporaryDirectory() as directory:
            tasks = [dict(title="failed", command=[sys.executable, "-c", "raise SystemExit(7)"], parallel=True),
                     dict(title="passed", command=[sys.executable, "-c", "print('完整日志')"], parallel=True),
                     dict(title="build", command=["missing-magel-gate-command"], required=True, parallel=True),
                     dict(title="blocked", command=[sys.executable, "-c", "raise SystemExit(9)"])]
            with redirect_stdout(io.StringIO()):
                results = run_tasks(tasks, Path(directory), 1, directory)
            self.assertEqual([item["exit_code"] for item in results], [7, 0, 127, None])
            self.assertIn("完整日志", Path(results[1]["log"]).read_text(encoding="utf-8"))

    def test_command_arguments(self):
        """命令登记完整保留空参数、中文、空格与选项，不经过 shell 再解析。"""
        with tempfile.TemporaryDirectory(prefix="gate arguments ") as directory:
            work = Path(directory)
            runner = Path(__file__).resolve().parents[1] / "tools/parallel_gates.py"
            manifest = work / "commands.jsonl"
            command = [sys.executable, "-c", "import json,sys; print(json.dumps(sys.argv[1:]))",
                       "", "中文 空格", "--flag", "$literal"]
            subprocess.run([sys.executable, str(runner), "append", "--required", str(manifest), "参数", *command],
                           check=True, capture_output=True)
            self.assertTrue(json.loads(manifest.read_text(encoding="utf-8"))["required"])
            subprocess.run([sys.executable, str(runner), "run", str(manifest), "--output", str(work / "logs"),
                            "--jobs", "2", "--cleanup-manifest"], check=True, capture_output=True)
            self.assertEqual(json.loads((work / "logs/001.log").read_text()), command[3:])
            self.assertFalse(manifest.exists())

    def test_configuration(self):
        """无效并行数被拒绝，未知脚本不会被自动并行。"""
        for value in ("0", "-1", "four", ""):
            with self.assertRaises(argparse.ArgumentTypeError):
                positive_jobs(value)
        self.assertEqual(positive_jobs("3"), 3)
        self.assertTrue(independent([sys.executable, "tests/magel_borrow_flow.py"]))
        self.assertFalse(independent([sys.executable, "tests/magel_generator.py"]))
        self.assertFalse(independent(["make", "-j4"]))
        self.assertTrue(independent(["build/magel-symbol-tests.exe", "fixtures"]))


if __name__ == "__main__":
    unittest.main()
