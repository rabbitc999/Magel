"""通过H7TOOL CMSIS-DAP备份、烧录并校验STM32H743内部Flash。"""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x08000000
SIZE = 2 * 1024 * 1024


def read_flash(target, size):
    data = bytearray()
    for offset in range(0, size, 1024):
        data.extend(target.read_memory_block8(BASE + offset, min(1024, size - offset)))
        if offset % (256 * 1024) == 0:
            print(f"读取Flash：{offset // 1024}/{size // 1024} KiB", flush=True)
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build-local/board-sdk')
    parser.add_argument('--image', type=Path, help='烧录BIN文件，起始地址固定为0x08000000')
    parser.add_argument('--build', action='store_true', help='先编译已配置的CMake构建目录')
    parser.add_argument('--uid', help='H7TOOL探针UID；未指定时要求仅连接一个探针')
    parser.add_argument('--frequency', type=int, default=1000000, help='SWD频率，默认1MHz')
    args = parser.parse_args()
    if args.frequency <= 0:
        parser.error('--frequency必须大于0')
    if args.build:
        subprocess.run(['cmake', '--build', str(args.build_dir), '-j', '6'], check=True)
    image = (args.image or args.build_dir / 'stm32_lua_car.bin').resolve()
    expected = image.read_bytes()
    if not 8 <= len(expected) <= SIZE:
        raise ValueError('BIN文件大小必须为8字节至2MiB')

    # 延迟导入，使--help无需安装pyOCD或连接硬件即可使用。
    from h7tool_compat import disable_transfer_block
    disable_transfer_block()
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.session import Session
    from pyocd.flash.file_programmer import FileProgrammer

    options = {'cmsis_dap.prefer_v1': True, 'cmsis_dap.limit_packets': True,
               'cmsis_dap.deferred_transfers': False, 'cache.enable_memory': False,
               'resume_on_disconnect': False, 'chip_erase': 'sector'}
    bootstrap = Session(None, options=options)
    probes = ConnectHelper.get_all_connected_probes(unique_id=args.uid)
    if len(probes) != 1:
        raise RuntimeError(f'找到{len(probes)}个探针，请连接一个H7TOOL或用--uid指定')
    session = Session(probes[0], target_override='stm32h743xx', frequency=args.frequency,
                      connect_mode='halt', options=options)
    out = ROOT / 'build-local/hardware-backups' / datetime.now().strftime('auto_flash_%Y%m%d_%H%M%S_%f')
    out.mkdir(parents=True)
    result = {'image': str(image), 'imageSha256': hashlib.sha256(expected).hexdigest(),
              'verified': False}
    try:
        with session:
            target = session.target
            target.reset_and_halt()
            # 暂停时冻结看门狗，避免备份与烧录过程中复位。
            target.write32(0x5C001054, target.read32(0x5C001054) | (1 << 18))
            if target.read32(0x5C001000) & 0xFFF != 0x450 or target.read16(0x1FF1E880) != 2048:
                raise RuntimeError('目标不是预期的2MiB STM32H743，停止烧录')
            backup = read_flash(target, SIZE)
            backup_path = out / 'original_flash.bin'
            backup_path.write_bytes(backup)
            result.update(backup=str(backup_path), backupSha256=hashlib.sha256(backup).hexdigest())
            print(f'完整Flash备份已保存：{backup_path}', flush=True)
            # 清除旧固件MPU和SysTick配置，供RAM烧录算法运行。
            target.write32(0xE000ED94, 0)
            target.write32(0xE000E010, 0)
            # 本探针此前出现整段空白页，显式编程全部映像页并保留最终全量回读门禁。
            FileProgrammer(session, chip_erase='sector', keep_unwritten=True,
                           smart_flash=False).program(str(image), base_address=BASE)
            actual = read_flash(target, len(expected))
            if actual != expected:
                raise RuntimeError('Flash回读与BIN不一致；保留暂停状态，请检查备份和连接')
            result['verified'] = True
            target.reset_and_halt()
            target.resume()
            print('烧录和逐字节回读校验成功，目标已复位运行', flush=True)
    except Exception as exc:
        result['error'] = str(exc)
        raise
    finally:
        (out / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'烧录失败：{exc}', file=sys.stderr)
        sys.exit(1)
