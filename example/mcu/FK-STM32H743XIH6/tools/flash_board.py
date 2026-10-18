"""使用本地 H7-TOOL 顺序烧录流程，完整备份并逐页校验固件。"""
import argparse
from pathlib import Path
import runpy
import sys


def main():
    """解析兼容参数，并调用支持扇区重试的本地顺序烧录工具。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board-tools', type=Path, default=Path(__file__).resolve().parent / 'h7tool',
                        help='板端烧录工具目录，默认使用示例内附的 h7tool')
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--uid')
    args = parser.parse_args()
    tools = args.board_tools.resolve()
    sys.argv = [str(Path(__file__).with_name('flash_sequential.py')),
                '--board-tools', str(tools), '--image', str(args.image.resolve())]
    if args.uid:
        sys.argv += ['--uid', args.uid]
    runpy.run_path(sys.argv[0], run_name='__main__')


if __name__ == '__main__':
    main()
