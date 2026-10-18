"""仅补写映像内完全擦空的 H743 编程页；拒绝覆盖非空错误页，不执行擦除。"""
import argparse
from datetime import datetime
import hashlib
from pathlib import Path
import sys
from h7tool_connect import prepare_debug_domains


def main():
    """备份当前完整 Flash，验证差异形态，补写空页并整映像回读。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--board-tools', type=Path, default=Path(__file__).resolve().parent / 'h7tool',
                        help='板端烧录工具目录，默认使用示例内附的 h7tool')
    parser.add_argument('--backup-dir', type=Path, required=True)
    parser.add_argument('--uid')
    args = parser.parse_args()
    sys.path.insert(0, str(args.board_tools.resolve()))
    print(prepare_debug_domains(args.uid), flush=True)
    from pyocd.core.session import Session
    from pyocd.core.helpers import ConnectHelper
    bootstrap = Session(None, options={'cmsis_dap.prefer_v1': True})
    probes = ConnectHelper.get_all_connected_probes(unique_id=args.uid)
    if len(probes) != 1:
        raise RuntimeError('必须指定唯一的 H7-TOOL')
    expected = args.image.read_bytes()
    if not 8 <= len(expected) <= 2*1024*1024:
        raise ValueError('映像容量无效')
    options = {'cmsis_dap.prefer_v1': True, 'cmsis_dap.deferred_transfers': False,
               'cmsis_dap.limit_packets': True, 'cache.enable_memory': False,
               'resume_on_disconnect': False}
    base = 0x08000000
    with Session(probes[0], target_override='stm32h743xx', connect_mode='halt',
                 frequency=1000000, options=options) as session:
        target = session.target
        target.halt()
        actual = bytearray()
        for offset in range(0, 2*1024*1024, 16384):
            actual.extend(target.read_memory_block8(base+offset, 16384))
            if offset % 0x40000 == 0:
                print(f'backup {offset//1024}/2048 KiB', flush=True)
        args.backup_dir.mkdir(parents=True, exist_ok=True)
        backup = args.backup_dir / ('before-sector-repair-'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.bin')
        backup.write_bytes(actual)
        print('backup', str(backup.resolve()), hashlib.sha256(actual).hexdigest(), flush=True)
        page_size = 1024
        dirty = []
        for offset in range(0, len(expected), page_size):
            wanted = expected[offset:offset+page_size]
            found = actual[offset:offset+len(wanted)]
            if found == wanted:
                continue
            if len(wanted) != page_size or any(value != 255 for value in found):
                raise RuntimeError(f'差异页 {base+offset:#x} 并非映像内完全擦空的完整页，拒绝补写')
            dirty.append(offset)
        target.write32(0xE000ED94, 0)
        target.write32(0xE000E010, 0)
        active = None
        try:
            for index, offset in enumerate(dirty):
                flash = target.get_memory_map().get_region_for_address(base+offset).flash
                if flash is not active:
                    if active is not None:
                        active.uninit()
                    flash.init(flash.Operation.PROGRAM)
                    active = flash
                flash.program_page(base+offset, expected[offset:offset+page_size])
                if index % 16 == 0:
                    print(f'program {base+offset:#x} ({index+1}/{len(dirty)})', flush=True)
        finally:
            if active is not None:
                active.uninit()
        for offset in range(0, len(expected), 16384):
            count = min(16384, len(expected)-offset)
            if bytes(target.read_memory_block8(base+offset, count)) != expected[offset:offset+count]:
                raise RuntimeError(f'补写后整映像回读失败：{base+offset:#x}；保留暂停状态')
        print('full image verified', hashlib.sha256(expected).hexdigest(), flush=True)
        target.reset_and_halt()
        target.resume()
        print('target running', flush=True)


if __name__ == '__main__':
    main()
