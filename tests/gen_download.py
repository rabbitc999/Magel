"""验证生成器下载成功、网络失败及错误响应时的文件替换与清理。"""
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.error

ROOT = Path(__file__).resolve().parents[1]
DOWNLOAD = (ROOT / "gen.sh").read_text(encoding="utf-8").split("python - <<'PY'\n", 1)[1].split("\nPY\n", 1)[0]
# 与 gen.sh 里的 SOURCES 保持一致：先 Gitee，再 GitHub。
GITEE = "https://gitee.com/rabbitc999/ParseEngine/releases/download/latest/ParseEngine.exe"
GITHUB = "https://github.com/rabbitc999/ParseEngine/releases/download/latest/ParseEngine.exe"
SOURCES = [GITEE, GITHUB]


class DownloadTests(unittest.TestCase):
    """在隔离目录执行 gen.sh 中的实际下载代码。"""

    def run_download(self, response, expected=None, content=b"MZlatest", attempts=None):
        """模拟下载并检查只在内容有效时替换生成器。"""
        if attempts is None:
            attempts = 1 if expected is None else len(SOURCES)
        previous = Path.cwd()
        with tempfile.TemporaryDirectory(prefix="magel-generator-") as directory:
            try:
                os.chdir(directory)
                executable = Path("ParseEngine.exe")
                executable.write_bytes(b"MZprevious")
                if isinstance(response, list):
                    arguments = {"side_effect": response}
                elif isinstance(response, Exception):
                    arguments = {"side_effect": [response] * 2}
                else:
                    arguments = {"return_value": response}
                with patch("urllib.request.urlopen", **arguments) as request:
                    if expected is None:
                        exec(compile(DOWNLOAD, "gen.sh:download", "exec"), {})
                    else:
                        with self.assertRaisesRegex(SystemExit, expected):
                            exec(compile(DOWNLOAD, "gen.sh:download", "exec"), {})
                    attempted = [call.args[0] for call in request.call_args_list]
                    # 先试 Gitee；只有它失败时才继续试 GitHub。
                    self.assertEqual(attempted[0], GITEE)
                    self.assertEqual(attempted, SOURCES[:attempts])
                    self.assertTrue(all(call.kwargs == {"timeout": 60} for call in request.call_args_list))
                self.assertEqual(executable.read_bytes(), content if expected is None else b"MZprevious")
                self.assertEqual(sorted(path.name for path in Path.cwd().iterdir()), ["ParseEngine.exe"])
            finally:
                os.chdir(previous)

    def test_latest_release(self):
        """有效响应替换旧文件并删除临时文件。"""
        self.run_download(io.BytesIO(b"MZlatest"))

    def test_github_fallback(self):
        """Gitee 失败时继续从 GitHub 拿，成功同样替换生成器。"""
        self.run_download([urllib.error.URLError("gitee offline"), io.BytesIO(b"MZgithub")],
                          content=b"MZgithub", attempts=len(SOURCES))

    def test_network_failure(self):
        """两处都拿不到时保留旧文件并清理临时文件。"""
        self.run_download(urllib.error.URLError("offline"), "下载 ParseEngine 失败")

    def test_html_response(self):
        """成功状态下返回网页也不能覆盖生成器。"""
        self.run_download(io.BytesIO(b"<html>error</html>"), "不是 Windows 可执行文件")

    def test_empty_response(self):
        """空下载不能替换生成器。"""
        self.run_download(io.BytesIO(), "不是 Windows 可执行文件")


if __name__ == "__main__":
    unittest.main()
