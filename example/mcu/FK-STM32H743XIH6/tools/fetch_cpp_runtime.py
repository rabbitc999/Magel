"""从 Arm 官方工具链 ZIP 提取 Cortex-M7 硬浮点 C++ 运行库与头文件。"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import io
from pathlib import Path
import struct
import subprocess
import zipfile

URL = 'https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-mingw-w64-i686-arm-none-eabi.zip'
SIZE = 291670457


def fetch(start, end):
    """下载有界字节区间，拒绝服务器返回的非预期长度。"""
    result = bytearray()
    for begin in range(start, end + 1, 512 * 1024):
        finish = min(end, begin + 512 * 1024 - 1)
        for attempt in range(3):
            try:
                data = subprocess.check_output(['curl.exe', '--fail', '-sS', '--max-time',
                                                '120', '-r', f'{begin}-{finish}', URL])
                if len(data) != finish - begin + 1:
                    raise ValueError('服务器未按 Range 返回所需 ZIP 区间')
                result.extend(data)
                break
            except (subprocess.CalledProcessError, ValueError):
                if attempt == 2:
                    raise
    return bytes(result)


def main():
    """复用目录缓存、校验 ZIP CRC，并提取当前目标所需的完整 C/C++ 运行库。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    args.cache.mkdir(parents=True, exist_ok=True)
    tail_path = args.cache / 'zip-tail.bin'
    tail = tail_path.read_bytes() if tail_path.exists() else fetch(SIZE - 65536, SIZE - 1)
    tail_path.write_bytes(tail)
    index = tail.rfind(b'PK\x05\x06')
    directory_size, directory_at = struct.unpack_from('<4s4H2LH', tail, index)[5:7]
    directory_path = args.cache / 'zip-directory.bin'
    directory = directory_path.read_bytes() if directory_path.exists() else fetch(directory_at, directory_at + directory_size - 1)
    directory_path.write_bytes(directory)
    archive = args.cache / 'cpp-runtime-sparse.zip'
    with archive.open('wb') as stream:
        stream.truncate(SIZE)
        stream.seek(directory_at)
        stream.write(directory)
        stream.seek(SIZE - len(tail))
        stream.write(tail)
    with zipfile.ZipFile(archive) as package:
        entries = [entry for entry in package.infolist() if not entry.is_dir() and (
            entry.filename.startswith('arm-none-eabi/include/') or
            ('/thumb/v7e-m+dp/hard/' in entry.filename and entry.filename.endswith(
                ('/libstdc++.a', '/libsupc++.a', '/libc.a', '/libm.a', '/libnosys.a', '/libgcc.a'))) or
            entry.filename.endswith(('/Copying.html', '/Copying-lib.html')))]
        ranges = []
        for entry in sorted(entries, key=lambda value: value.header_offset):
            start = entry.header_offset
            end = min(SIZE - 1, start + 30 + len(entry.filename.encode()) + len(entry.extra) + entry.compress_size + 256)
            if ranges and start - ranges[-1][1] < 4096:
                ranges[-1][1] = max(ranges[-1][1], end)
            else:
                ranges.append([start, end])
        print(f'{len(entries)} entries, {len(ranges)} ranges, {sum(b-a+1 for a,b in ranges)} download bytes', flush=True)
        with ThreadPoolExecutor(max_workers=6) as executor, archive.open('r+b') as stream:
            for start, data in executor.map(lambda span: (span[0], fetch(*span)), ranges):
                stream.seek(start)
                stream.write(data)
        destination = args.destination.resolve()
        destination.mkdir(parents=True, exist_ok=True)
        for entry in entries:
            target = (destination / entry.filename).resolve()
            if not target.is_relative_to(destination):
                raise ValueError('ZIP 包含越界路径')
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(package.read(entry))
    print(f'C/C++ runtime ready: {destination}', flush=True)


if __name__ == '__main__':
    main()
