from pathlib import Path
'验证二进制解码、核心浮点转换、导出一致性及模块裁剪。'
import struct
from chali_glue_edges import ROOT, CHALI, EXE, run

def checkErrors():
    work = ROOT / 'build/chali-binary'
    work.mkdir(parents=True, exist_ok=True)
    samples = ROOT / 'example/samples-error'
    for source in sorted(samples.glob('binary_*.chl')) + sorted(samples.glob('float_int*.chl')):
        expected = (ROOT / 'example/expected-error' / (source.stem + '.txt')).read_text(encoding='utf-8')
        result = run(CHALI, 'run', source, '--out', work / 'error', succeeds=False)
        assert result.stderr.splitlines()[0] == expected.strip(), (source, result.stderr)

def main(names=('std_binary', 'lang_float_to_int', 'lang_float_bytes', 'lang_float64_bytes')):
    work = ROOT / 'build/chali-binary'
    work.mkdir(parents=True, exist_ok=True)
    for name in names:
        source = ROOT / f'example/samples/{name}.chl'
        expected = (ROOT / f'example/expected/{name}.txt').read_text(encoding='utf-8')
        result = run(CHALI, 'run', source, '--out', work / name)
        assert result.stdout == expected and (not result.stderr), (result.stdout, result.stderr)
        prefix = work / name
        run(CHALI, 'run', source, '--emit', '--out', prefix)
        args = ['--quad', prefix.with_suffix('.quad'), '--data', prefix.with_suffix('.data.bin'), '--layout', prefix.with_suffix('.layout')]
        asset_target = work / (name + '-assets' + EXE)
        trimmed = work / (name + '-trimmed' + EXE)
        macro = 'NO_CHALI_STD_BINARY' if name == 'std_binary' else 'NO_CHALI_STD'
        result = run(trimmed, succeeds=name != 'std_binary')
        if name == 'std_binary':
            assert 'std::binary::' in result.stdout + result.stderr
        else:
            assert result.stdout == expected and (not result.stderr)
    checkErrors()
    for value, label in [(float('nan'), 'nan'), (float('inf'), 'inf'), (-float('inf'), 'negative-inf'), (-0.0, 'negative-zero')]:
        for fmt, chali_type in [('f', 'Float32'), ('d', 'Float64')]:
            raw = struct.pack('<' + fmt, value)
            setup = ['import std::binary;', 'void main() {', f'uint8 bytes[{len(raw)}];']
            setup += [f'bytes[{i}] = (uint8){b};' for i, b in enumerate(raw)]
            setup += [f'std::binary::BinaryReader r = std::binary::BinaryReader::fromArray(bytes, {len(raw)});', f'{chali_type.lower()} value = r.read{chali_type}();']
            source = work / f'{label}-{fmt}.chl'
            for width in (32, 64):
                if width == 32:
                    check = 'std::lang::assert(std::lang::toInt32(value).unwrap() == (int32)0);' if label == 'negative-zero' else 'std::lang::assert(std::lang::toInt32(value).isNone());'
                else:
                    check = 'std::lang::toInt64(value);'
                source.write_text('\n'.join(setup + [check + ' }']), encoding='utf-8')
                result = run(CHALI, 'run', source, '--out', work / 'special', succeeds=width == 32 or label == 'negative-zero')
                if width == 64 and label != 'negative-zero':
                    assert 'std::lang::toInt64：浮点值不是有限数或超出 int64 范围' in result.stderr
                else:
                    assert not result.stderr, result.stderr
if __name__ == '__main__':
    main()
