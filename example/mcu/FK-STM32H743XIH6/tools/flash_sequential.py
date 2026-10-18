"""H7-TOOL 顺序烧录：完整备份、RAM 上传校验、逐页回读及按扇区重连重试。"""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import sys
import time

BASE = 0x08000000
SIZE = 2 * 1024 * 1024
SECTOR = 128 * 1024
PAGE = 1024


def main():
    """只操作核实为 2 MiB 的 H743；任何校验失败均保留暂停状态和备份。"""
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--uid')
    parser.add_argument('--board-tools', type=Path, default=Path(__file__).resolve().parent / 'h7tool')
    args = parser.parse_args()
    image = args.image.resolve().read_bytes()
    if not 8 <= len(image) <= SIZE:
        raise ValueError('映像大小须为 8 字节至 2 MiB')
    sys.path.insert(0, str(args.board_tools.resolve()))
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.session import Session
    from pyocd.probe.cmsis_dap_probe import CMSISDAPProbe
    from h7tool_connect import prepare_debug_domains
    # 禁用 DAP_TransferBlock，写入仍用普通 DAP_Transfer 组包；每次 RAM 上传均回读。
    original_multiple = CMSISDAPProbe.write_ap_multiple
    prepare_debug_domains(args.uid)
    CMSISDAPProbe.write_ap_multiple = original_multiple
    options = {'cmsis_dap.prefer_v1': True, 'cmsis_dap.limit_packets': True,
               'cmsis_dap.deferred_transfers': False, 'cache.enable_memory': False,
               'resume_on_disconnect': False}
    bootstrap = Session(None, options=options)

    def connection():
        """重新打开独占探针，并在每次重连时核对芯片身份与容量。"""
        probes = ConnectHelper.get_all_connected_probes(unique_id=args.uid)
        if len(probes) != 1:
            raise RuntimeError('必须连接唯一的 H7-TOOL，或用 --uid 指定探针')
        return Session(probes[0], target_override='stm32h743xx', frequency=1000000,
                       connect_mode='halt', options=options)

    def prepare(target):
        """暂停在复位入口，关闭影响 RAM 烧录算法的 MPU、时基及 Cache。"""
        target.reset_and_halt()
        if target.read32(0x5C001000) & 0xFFF != 0x450 or target.read16(0x1FF1E880) != 2048:
            raise RuntimeError('目标不是预期的 2 MiB STM32H743')
        target.write32(0x5C001054, target.read32(0x5C001054) | (1 << 18))
        target.write32(0xE000ED94, 0)
        target.write32(0xE000E010, 0)
        target.write32(0xE000ED14, target.read32(0xE000ED14) & ~((1 << 16) | (1 << 17)))

    def read(target, address, size):
        """分块读取真实目标内存，不使用主机内存缓存。"""
        result = bytearray()
        for offset in range(0, size, 16384):
            result.extend(target.read_memory_block8(address + offset, min(16384, size - offset)))
        return bytes(result)

    output = root / 'build-local/hardware-backups' / datetime.now().strftime('sequential_%Y%m%d_%H%M%S_%f')
    output.mkdir(parents=True)
    report = {'image': str(args.image.resolve()), 'imageSha256': hashlib.sha256(image).hexdigest(),
              'verified': False, 'sectors': []}
    try:
        with connection() as session:
            prepare(session.target)
            backup = read(session.target, BASE, SIZE)
        (output / 'original_flash.bin').write_bytes(backup)
        report.update(backup=str(output / 'original_flash.bin'),
                      backupSha256=hashlib.sha256(backup).hexdigest())
        print(f'完整 Flash 备份已保存：{output}', flush=True)
        # 仅擦写覆盖映像的扇区；最后一个扇区的映像之外字节按本次备份恢复。
        wanted = image + backup[len(image):]
        for sector in range((len(image) + SECTOR - 1) // SECTOR):
            address = BASE + sector * SECTOR
            payload = wanted[sector * SECTOR:(sector + 1) * SECTOR]
            for attempt in range(4):
                try:
                    with connection() as session:
                        target = session.target
                        prepare(target)
                        if read(target, address, SECTOR) == payload:
                            report['sectors'].append({'sector': sector, 'skipped': True, 'attempt': attempt})
                            break
                        original_write = target.write_memory_block8

                        def checked_write(destination, data):
                            """Flash 算法运行前验证 RAM 页缓冲；上传失败时禁止编程。"""
                            for _ in range(3):
                                original_write(destination, data)
                                if read(target, destination, len(data)) == bytes(data):
                                    return
                            raise RuntimeError(f'RAM 上传回读不一致：{destination:#x}')

                        target.write_memory_block8 = checked_write
                        flash = target.memory_map.get_region_for_address(address).flash
                        flash.init(flash.Operation.ERASE)
                        flash.erase_sector(address)
                        flash.uninit()
                        flash.init(flash.Operation.PROGRAM)
                        for offset in range(0, SECTOR, PAGE):
                            page = payload[offset:offset + PAGE]
                            if any(byte != 255 for byte in page):
                                flash.program_page(address + offset, page)
                            if read(target, address + offset, PAGE) != page:
                                raise RuntimeError(f'Flash 页校验失败：{address + offset:#x}')
                        flash.uninit()
                        report['sectors'].append({'sector': sector, 'skipped': False, 'attempt': attempt})
                        print(f'扇区 {sector} 写入及逐页校验通过', flush=True)
                        break
                except Exception as error:
                    print(f'扇区 {sector} 尝试 {attempt + 1} 失败：{error}', flush=True)
                    if attempt == 3:
                        raise
                    time.sleep(0.3)
            else:
                raise RuntimeError(f'扇区 {sector} 未完成')
        with connection() as session:
            prepare(session.target)
            if read(session.target, BASE, len(image)) != image:
                raise RuntimeError('最终完整映像校验失败，保留暂停状态')
            report['verified'] = True
            session.target.reset_and_halt()
            session.target.resume()
        print('完整映像回读通过，目标已复位运行', flush=True)
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n',
                                            encoding='utf-8')


if __name__ == '__main__':
    main()
