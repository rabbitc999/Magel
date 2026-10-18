"""以 Python 标准库为独立参考，交叉验证编码、UTC 日期与 CSV。"""
from pathlib import Path
import argparse
import base64
import csv
import datetime as dt
import io
import json
import os
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--magel', type=Path, default=ROOT / ('magel.exe' if os.name == 'nt' else 'magel'))
    compiler = parser.parse_args().magel.resolve()
    rng = random.Random(220)
    quote = lambda s: json.dumps(s, ensure_ascii=False)
    blocks = ['import std::encoding;', 'import std::datetime;', 'import std::csv;', 'import std::utf8;', 'int64 main(){']
    checks = 0
    for size in (0, 1, 2, 3, 4, 5, 7, 16, 31, 32, 63, 64, 127):
        data = rng.randbytes(size)
        body = ['{List<uint8> data;'] + [f'data.push((uint8){b});' for b in data]
        body += [f'assert(encodeHex(data)=={quote(data.hex())});',
                 f'assert(encodeBase64(data)=={quote(base64.b64encode(data).decode())});',
                 f'assert(encodeBase64Url(data)=={quote(base64.urlsafe_b64encode(data).decode().rstrip("="))});',
                 'assert(encodeHex(decodeBase64(encodeBase64(data)).unwrap())==encodeHex(data));}']
        blocks.extend(body)
        checks += 4
    epoch = dt.datetime(1970, 1, 1, tzinfo=dt.timezone.utc)
    for seconds in [-62135596800, -2208988800, -1, 0, 2147483648, 253402300799] + [rng.randrange(-62135596800,253402300800) for _ in range(40)]:
        value = epoch + dt.timedelta(seconds=seconds)
        formatted = f'{value.year:04}-{value.month:02}-{value.day:02}T{value.hour:02}:{value.minute:02}:{value.second:02}Z'
        blocks.append(f'assert(formatUtc(fromUnixSeconds({seconds}).unwrap()).unwrap()=={quote(formatted)});')
        blocks.append(f'assert(toUnixSeconds(parseUtc({quote(formatted)}).unwrap()).unwrap()=={seconds});')
        checks += 2
    for delimiter in (',', ';', '\t'):
        rows = [[rng.choice(['', 'plain', '中文😀', 'a,b;c', 'quote"text', 'line\nnext', 'x\r\ny']) for _ in range(rng.randrange(1,5))] for _ in range(12)]
        stream = io.StringIO(newline='')
        # CR inside a field must be quoted even with LF record endings.
        csv.writer(stream, delimiter=delimiter, lineterminator='\r\n').writerows(rows)
        blocks.append('{auto rows=parseCsv('+quote(stream.getvalue())+','+quote(delimiter).replace('"', "'")+').unwrap();')
        blocks.append(f'assert(rows.len()=={len(rows)});')
        for i,row in enumerate(rows):
            blocks.append(f'assert(rows[{i}].len()=={len(row)});')
            for j,field in enumerate(row):
                blocks.append(f'assert(rows[{i}][{j}]=={quote(field)});')
                checks += 1
        blocks.append('assert(parseCsv(writeCsv(rows,'+quote(delimiter).replace('"', "'")+').unwrap(),'+quote(delimiter).replace('"', "'")+').unwrap().len()==rows.len());}')
    for value in ('A中文😀', '\u007f\u0080\u07ff\u0800\uffff\U00010000\U0010ffff', 'e\u0301'):
        blocks.append(f'assert(codePointCount({quote(value)}).unwrap()=={len(value)});')
        blocks.append(f'assert(encodeUtf8(decodeUtf8({quote(value)}).unwrap()).unwrap()=={quote(value)});')
        checks += 2
    blocks.append('return 0;}')
    with tempfile.TemporaryDirectory(prefix='std-data-', dir=ROOT/'build') as directory:
        source = Path(directory)/'crosscheck.mgl'
        source.write_text('\n'.join(blocks), encoding='utf-8')
        result = subprocess.run([str(compiler),'run',str(source),'out',str(Path(directory)/'program')],cwd=ROOT,
                                capture_output=True, encoding='utf-8', errors='replace', timeout=180)
        assert result.returncode == 0 and not result.stderr, result
    print(f'Python 标准库交叉验证通过：{checks} 项固定种子断言')


if __name__ == '__main__':
    main()
