"""使用 H7-TOOL 的物理 USART1 发送虚拟按键，并通过 SWD 验证已返回菜单。"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import time


def main():
    """复位板子验证默认测速启动，覆盖 MODE/ENTER 的四种虚拟退出事件。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--uart-tool', type=Path, required=True,
                        help='支持 --lua-file 的 Windows 原生 h7tool_mcp.exe')
    parser.add_argument('--uid', default='002B00223131510339333437')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(Path(__file__).resolve().parent / 'h7tool'))
    from h7tool_connect import prepare_debug_domains
    from h7tool_compat import disable_transfer_block
    from elftools.elf.elffile import ELFFile
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.session import Session
    debug = prepare_debug_domains(args.uid)
    disable_transfer_block()
    with args.elf.open('rb') as stream:
        table = ELFFile(stream).get_section_by_name('.symtab')
        snapshot = table.get_symbol_by_name('g_MagelBenchmark')[0]['st_value']
        trace = table.get_symbol_by_name('g_MagelUiTrace')[0]['st_value']
    options = {'cmsis_dap.prefer_v1': True, 'cmsis_dap.limit_packets': True,
               'cmsis_dap.deferred_transfers': False, 'cache.enable_memory': False,
               'resume_on_disconnect': True}
    def inspect(reset=False):
        """在独占 SWD 期间读取固件主动写回 Cache 的测试快照。"""
        # pyOCD 枚举 CMSIS-DAP 时读取当前 Session 的选项；每次枚举都重建选项上下文。
        options_session = Session(None, options=options)
        probes = ConnectHelper.get_all_connected_probes(unique_id=args.uid)
        assert len(probes) == 1, len(probes)
        with Session(probes[0], target_override='stm32h743xx', frequency=1000000,
                     connect_mode='attach', options=options) as session:
            target = session.target
            if reset:
                target.reset_and_halt()
                target.resume()
                return None
            bench = struct.unpack('<24I', bytes(target.read_memory_block8(snapshot, 96)))
            ui = struct.unpack('<8I', bytes(target.read_memory_block8(trace, 32)))
            return {'benchmark': bench, 'ui': ui,
                    'faults': [target.read32(0xE000ED28), target.read32(0xE000ED2C)]}

    args.out.parent.mkdir(parents=True, exist_ok=True)
    logs = args.out.parent / (args.out.stem + '-uart')
    logs.mkdir(exist_ok=True)
    transcript = []

    def send(command):
        """通过探针 UART 引脚发送真实串口字节，保存工具响应和串口回显。"""
        index = len(transcript)
        lua = r'''print("H7_TEST_BEGIN")
uart_cfg(1,115200,0,8,1)
local n,s=uart_recive(1,1024,20)
uart_send(1,"COMMAND\n")
local h=""
for j=1,4 do
 local n,s=uart_recive(1,512,250)
 for i=1,#s do h=h..string.format("%02X",string.byte(s,i)) end
end
print("rx_hex="..h)
print("H7_TEST_END")
'''.replace('COMMAND', command)
        path = logs / f'{index:02d}.lua'
        path.write_text(lua, encoding='ascii')
        completed = subprocess.run([str(args.uart_tool.resolve()), '--lua-file', str(path.resolve())],
                                   capture_output=True, encoding='utf-8', errors='replace', timeout=30)
        assert completed.returncode == 0, (completed.stdout, completed.stderr)
        response = json.loads(completed.stdout)
        match = re.search(r'rx_hex=([0-9A-F]*)', response.get('raw', ''))
        assert match, response
        received = bytes.fromhex(match[1]).decode('utf-8', 'replace')
        entry = {'command': command, 'uart': received, 'tool': response}
        transcript.append(entry)
        (logs / f'{index:02d}.json').write_text(json.dumps(entry, ensure_ascii=False, indent=2),
                                               encoding='utf-8')
        print(f'UART {command}: {received!r}', flush=True)
        return received

    inspect(reset=True)
    deadline = time.monotonic() + 20
    while True:
        boot = inspect()
        if boot['benchmark'][0] == 0x4D47454C and boot['benchmark'][2] == 5:
            break
        assert time.monotonic() < deadline, boot
        time.sleep(0.25)
    assert boot['ui'][0:2] == (0x55494D47, 0), boot
    results = []
    for key in ('mode', 'enter'):
        for mode in ('click', 'long', 'double', 'repeat'):
            before = inspect()
            if results:
                started = send('key enter')
                assert 'KEY_OK enter click' in started, started
                time.sleep(0.5)
                running = inspect()
                assert running['ui'][1] == before['ui'][1], running
                assert 'BENCH_END' in started, started
            if not results:
                other = send('key up')
                assert 'KEY_OK up click' in other, other
                assert inspect()['ui'][1] == 0
                invalid = send('key mode invalid')
                assert 'KEY_ERROR' in invalid and inspect()['ui'][1] == 0
            start = time.monotonic()
            received = send(f'key {key} {mode}')
            after = inspect()
            elapsed = time.monotonic() - start
            assert f'KEY_OK {key} {mode}' in received, received
            assert 'BENCH_EXIT key' in received, received
            assert after['ui'][1] == before['ui'][1] + 1, after
            index = before['ui'][1] % 3
            assert after['ui'][2 + index] == 0 and after['ui'][5 + index] == 0, after
            assert after['faults'] == [0, 0], after
            # 必须在三十秒演示超时之前由按键路径退出。
            assert elapsed < 10, elapsed
            time.sleep(0.15)
            assert inspect()['ui'][1] == after['ui'][1], '退出事件再次触发了菜单'
            results.append({'key': key, 'event': mode, 'seconds': elapsed, 'state': after})
    result = {'source': 'physical USART1 via H7-TOOL UART, selector snapshot via SWD',
              'capturedAt': datetime.now(timezone.utc).isoformat(), 'probeUid': args.uid,
              'firmwareSha256': hashlib.sha256(args.image.read_bytes()).hexdigest(),
              'defaultBootBenchmark': True, 'debugConnection': debug,
              'boot': boot, 'cases': results, 'uartLogDirectory': logs.name}
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('物理 USART1 虚拟按键：八种退出、方向键、非法命令、默认开机测速及菜单隔离全部通过。', flush=True)


if __name__ == '__main__':
    main()
