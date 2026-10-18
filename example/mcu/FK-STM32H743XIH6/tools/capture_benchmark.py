"""通过 H7-TOOL SWD 读取板上发布的真实计时快照；可选备份后烧录。"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import statistics
import struct
import subprocess
import sys
import time


def main():
    """读取 ELF 符号位置，从正在运行的 MCU 采集五轮硬件周期计数。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--board-tools', type=Path, default=Path(__file__).resolve().parent / 'h7tool',
                        help='板端烧录工具目录，默认使用示例内附的 h7tool')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--uid')
    parser.add_argument('--flash', action='store_true', help='调用参考工程工具，完整备份 Flash 后烧录并回读验证')
    parser.add_argument('--timeout', type=float, default=90)
    parser.add_argument('--wait-ui', action='store_true', help='等待板端自动测试完成退出、panic、再次退出三轮')
    args = parser.parse_args()
    tools = args.board_tools.resolve()
    sys.path.insert(0, str(tools))
    from h7tool_connect import prepare_debug_domains
    debug = prepare_debug_domains(args.uid)
    print('Debug domains verified:', debug, flush=True)
    if args.flash:
        command = [sys.executable, str(Path(__file__).with_name('flash_board.py')),
                   '--board-tools', str(tools), '--image', str(args.image.resolve())]
        if args.uid:
            command += ['--uid', args.uid]
        subprocess.run(command, check=True)
    from elftools.elf.elffile import ELFFile
    with args.elf.open('rb') as stream:
        table = ELFFile(stream).get_section_by_name('.symtab')
        address = table.get_symbol_by_name('g_MagelBenchmark')[0]['st_value']
        ui_address = table.get_symbol_by_name('g_MagelUiTrace')[0]['st_value']
    from h7tool_compat import disable_transfer_block
    disable_transfer_block()
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.session import Session
    bootstrap = Session(None, options={'cmsis_dap.prefer_v1': True})
    probes = ConnectHelper.get_all_connected_probes(unique_id=args.uid)
    if len(probes) != 1:
        raise RuntimeError(f'需要唯一的 H7-TOOL，当前探针数量为 {len(probes)}')
    options = {'cmsis_dap.prefer_v1': True, 'cmsis_dap.limit_packets': True,
               'cmsis_dap.deferred_transfers': False, 'cache.enable_memory': False,
               'resume_on_disconnect': False}
    start = time.monotonic()
    with Session(probes[0], target_override='stm32h743xx', frequency=1000000,
                 connect_mode='attach', options=options) as session:
        while True:
            values = struct.unpack('<24I', bytes(session.target.read_memory_block8(address, 96)))
            magic, clock, count, status = values[:4]
            ui = struct.unpack('<8I', bytes(session.target.read_memory_block8(ui_address, 32)))
            if magic == 0x4d47454c and count == 5 and (not args.wait_ui or ui[1] >= 3):
                break
            if time.monotonic() - start > args.timeout:
                faults = {hex(reg): session.target.read32(reg) for reg in (0xE000ED28, 0xE000ED2C)}
                raise RuntimeError(f'等待实机样本超时：snapshot={values[:4]}, faults={faults}')
            time.sleep(1)
    samples = []
    for index in range(5):
        iterations, vm, native, checksum = values[4+index*4:8+index*4]
        expected = 123
        for _ in range(iterations):
            expected = (expected * 1664525 + 1013904223) & 2147483647
        if checksum != expected or min(iterations, vm, native) <= 0:
            raise RuntimeError('硬件样本校验值或计时无效')
        samples.append(dict(iterations=iterations, vmCycles=vm, nativeCycles=native, checksum=checksum,
                            vmMilliseconds=vm*1000/clock, nativeMilliseconds=native*1000/clock))
    vm = statistics.median(sample['vmCycles'] for sample in samples)
    native = statistics.median(sample['nativeCycles'] for sample in samples)
    if args.wait_ui and (ui[0] != 0x55494d47 or ui[2:5] != (0, 5, 0) or ui[5:8] != (0, 2, 1)):
        raise RuntimeError(f'程序退出与 panic 后的选择页回归失败：{ui}')
    result = dict(source='physical MCU DWT_CYCCNT via SWD', capturedAt=datetime.now(timezone.utc).isoformat(),
                  target='STM32H743XIH6', probeUid=probes[0].unique_id, clockHz=clock,
                  firmwareSha256=hashlib.sha256(args.image.read_bytes()).hexdigest(),
                  snapshotAddress=hex(address), programStatus=status, samples=samples,
                  medianVmMilliseconds=vm*1000/clock, medianNativeMilliseconds=native*1000/clock,
                  medianVmCyclesPerIteration=vm/samples[0]['iterations'], vmToNativeRatio=vm/native,
                  debugConnection=debug, selectorTrace=dict(completed=ui[1], statuses=ui[2:5], programs=ui[5:8]))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)


if __name__ == '__main__':
    main()
