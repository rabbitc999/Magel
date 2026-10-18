"""核对内置板级库与快照清单，支持 LF/CRLF 工作区，无需原工程。"""
import hashlib
import json
from pathlib import Path


def main():
    """逐个验证导入文件的本地规范化哈希，并拒绝重复或越界路径。"""
    root = Path(__file__).resolve().parents[1]
    snapshot = json.loads((root / 'sdk-snapshot.json').read_text(encoding='utf-8'))
    seen = set()
    for entry in snapshot['files']:
        path = entry['path']
        assert path not in seen, f'清单重复路径：{path}'
        seen.add(path)
        target = (root / path).resolve()
        assert target.is_relative_to(root), f'清单路径越界：{path}'
        data = target.read_bytes().replace(b'\r\n', b'\n')
        actual = hashlib.sha256(data).hexdigest()
        assert actual == entry['localSha256Lf'], f'本地 SDK 内容已变更，需复核并更新快照：{path}'
    print(f'内置 SDK 快照通过：{len(seen)} 个导入文件；不依赖原工程路径。')


if __name__ == '__main__':
    main()
