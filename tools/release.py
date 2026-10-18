"""构建、校验并打包 Magel；远端提交和标签推送必须显式启用。"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SAMPLES = ('return_expression', 'named_function', 'derive', 'macro_groups',
           'break_level', 'macro_gensym', 'foreach_interval', 'any_generics', 'value_text')
CHECKS = ('tests/magel_release.py', 'tools/changelog.py --check', 'tools/check_doc_links.py',
          'tools/export_magel_ebnf.py --check', 'tools/library_reference.py --check',
          'tools/embed_manual.py --check', 'tests/magel_book_updates.py',
          'tests/magel_man.py', 'tests/magel_macro_groups.py', 'tests/magel_any_generics.py',
          'tests/magel_value_text.py')


def run(command, cwd=ROOT, capture=False, env=None):
    """直接传递参数执行命令，失败立即停止，避免后续发布部分产物。"""
    print('+ ' + subprocess.list2cmdline([str(x) for x in command]), flush=True)
    result = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                            capture_output=capture, text=True, encoding='utf-8', errors='replace', check=True)
    return result.stdout.strip() if capture else ''


def version_of(root):
    """读取唯一版本并拒绝编译器与插件版本漂移。"""
    version = (root / 'VERSION').read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:\.\d+)?', version):
        raise ValueError('VERSION 格式无效：' + version)
    manifest = json.loads((root / 'vscode/package.json').read_text(encoding='utf-8'))
    if manifest['version'] != version:
        raise ValueError('vscode/package.json 的 version 必须与 VERSION 一致')
    return version


def compiler_version(binary):
    """通过标准 LSP 握手核对运行中编译器的版本。"""
    payload = b''
    for request in ({'id': 1, 'method': 'initialize', 'params': {'processId': None, 'capabilities': {}}},
                    {'id': 2, 'method': 'shutdown'}, {'method': 'exit'}):
        body = json.dumps(dict(jsonrpc='2.0', **request)).encode('utf-8')
        payload += f'Content-Length: {len(body)}\r\n\r\n'.encode() + body
    result = subprocess.run([str(binary), 'lsp'], input=payload, capture_output=True, timeout=60, check=True)
    data = result.stdout
    while data:
        header, separator, rest = data.partition(b'\r\n\r\n')
        if not separator:
            raise ValueError('编译器返回了不完整的 LSP 帧')
        lengths = re.findall(rb'(?im)^Content-Length:\s*(\d+)\s*$', header)
        if len(lengths) != 1:
            raise ValueError('编译器 LSP Content-Length 无效')
        size = int(lengths[0])
        message = json.loads(rest[:size])
        data = rest[size:]
        if message.get('id') == 1:
            return message['result']['serverInfo']['version']
    raise ValueError('编译器没有返回 initialize 响应')


def verify_assets(root, binary, vsix, version, windows, env=None):
    """核对版本、VSIX 内嵌字节、PDF、反射构建配置及 Windows 动态依赖。"""
    if compiler_version(binary) != version:
        raise ValueError('编译器版本与 VERSION 不一致')
    with zipfile.ZipFile(vsix) as package:
        manifest = json.loads(package.read('extension/package.json'))
        identity = ET.fromstring(package.read('extension.vsixmanifest')).find('.//{*}Identity')
        embedded = 'extension/bin/' + binary.name
        if manifest['version'] != version or identity is None or identity.attrib['Version'] != version:
            raise ValueError('VSIX 清单版本与 VERSION 不一致')
        if package.read(embedded) != binary.read_bytes():
            raise ValueError('VSIX 内嵌编译器与本次编译器字节不一致')
    pdf = root / 'doc/book/latex/book.pdf'
    if not pdf.read_bytes().startswith(b'%PDF-'):
        raise ValueError('书本 PDF 无效')
    config = (root / 'build/magel-runtime/buildConfig.json').read_text(encoding='utf-8')
    for flag in ('MAGEL_WITH_REFLECTION=1', 'STACKVM_WITH_REFLECTION=1'):
        if flag not in config:
            raise ValueError('发布构建必须同时启用前端与运行时反射：' + flag)
    if windows:
        imports = run(['objdump', '-p', binary], capture=True, env=env)
        unwanted = re.findall(r'DLL Name:\s*((?:libgcc_s[^\s]*|libstdc\+\+[^\s]*|libwinpthread[^\s]*))', imports, re.I)
        if unwanted:
            raise ValueError('编译器仍依赖未随包交付的 MinGW DLL：' + ', '.join(unwanted))


def package_assets(root, output, binary, vsix, version, target, revision, dirty):
    """复制独立产物并原子替换 ZIP，同时生成提交元数据与 SHA256 清单。"""
    output.mkdir(parents=True, exist_ok=True)
    files = [(binary, binary.name), (vsix, vsix.name),
             (root / 'doc/book/latex/book.pdf', 'Magel-book.pdf')]
    for source, name in files:
        destination = output / name
        if source.resolve() != destination.resolve():
            shutil.copy2(source, destination)
    metadata = dict(version=version, target=target, commit=revision, workingTreeDirty=dirty,
                    reflection=dict(frontend=True, runtime=True))
    (output / 'release.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    archive = output / f'magel-{version}-{target}.zip'
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output, suffix='.zip.tmp', delete=False) as stream:
            temporary = Path(stream.name)
        with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as package:
            for _, name in files:
                package.write(output / name, name)
            for name in ('VERSION', 'LICENSE', 'CHANGELOG.md'):
                package.write(root / name, name)
            package.write(output / 'release.json', 'release.json')
            for name in SAMPLES:
                package.write(root / f'example/samples/{name}.mgl', f'examples/{name}.mgl')
            package.writestr('README.txt', f'Magel {version} ({target})\n提交：{revision}\n工作区存在未提交改动：{dirty}\n\n'
                             f'运行：{binary.name} run examples/any_generics.mgl\n宏帮助：{binary.name} man macro\n'
                             f'插件：code --install-extension {vsix.name}\n书本：Magel-book.pdf\n'
                             '源码：https://github.com/rabbitc999/Magel\n')
        os.replace(temporary, archive)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    names = [name for _, name in files] + ['release.json', archive.name]
    (output / 'SHA256SUMS.txt').write_text(''.join(
        hashlib.sha256((output / name).read_bytes()).hexdigest() + '  ' + name + '\n' for name in names), encoding='utf-8')
    return archive


def publish_tag(version, replace):
    """仅推送干净 master，并按远端旧对象号租约更新显式允许替换的标签。"""
    if run(['git', 'status', '--porcelain'], capture=True):
        raise ValueError('推送标签前须提交源码和生成资产；可先完成打包，再提交并用 --package-only --push-tag 重跑')
    if run(['git', 'branch', '--show-current'], capture=True) != 'master':
        raise ValueError('发布推送要求当前分支为 master')
    tag = 'v' + version
    head = run(['git', 'rev-parse', 'HEAD'], capture=True)
    remote = run(['git', 'ls-remote', 'origin', 'refs/tags/' + tag, 'refs/tags/' + tag + '^{}'], capture=True)
    refs = dict(line.split()[::-1] for line in remote.splitlines())
    old = refs.get('refs/tags/' + tag, '')
    remote_commit = refs.get('refs/tags/' + tag + '^{}', old)
    if old and remote_commit != head and not replace:
        raise ValueError('远端标签已指向其他提交；确需覆盖时显式增加 --replace-tag')
    local = subprocess.run(['git', 'rev-parse', '--verify', tag + '^{}'], cwd=ROOT,
                           capture_output=True, text=True)
    if local.returncode == 0 and local.stdout.strip() != head and not replace:
        raise ValueError('本地标签已指向其他提交；确需覆盖时显式增加 --replace-tag')
    run(['git', 'push', 'origin', 'master'])
    if local.returncode or local.stdout.strip() != head:
        run(['git', 'tag', *(['-f'] if local.returncode == 0 else []), '-a', tag, '-m', '发布 Magel ' + version])
    run(['git', 'push', '--force-with-lease=refs/tags/' + tag + ':' + old, 'origin', 'refs/tags/' + tag])


def main(argv=None):
    """统一发布入口；默认构建和校验，仅显式选项允许远端写入。"""
    if sys.version_info < (3, 11):
        raise ValueError('发布脚本需要 Python 3.11 或更新版本')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package-only', action='store_true', help='复用已有编译器、VSIX 和 PDF，仍核对交付一致性')
    parser.add_argument('--build', action='store_true', help='兼容旧入口；默认已经构建')
    parser.add_argument('--out-dir', type=Path, default=ROOT / 'dist')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--toolchain-dir', type=Path, help='将指定编译工具目录加入 PATH')
    parser.add_argument('--ci', action='store_true', help='额外执行完整 ci.sh 门禁')
    parser.add_argument('--push-tag', action='store_true', help='交付完成后推送干净 master 和当前版本标签')
    parser.add_argument('--replace-tag', action='store_true', help='配合 --push-tag 显式允许更新已有版本标签')
    parser.add_argument('--dry-run', action='store_true', help='仅显示流程，不构建、打包或推送')
    args = parser.parse_args(argv)
    if args.jobs < 1 or (args.replace_tag and not args.push_tag) or (args.package_only and (args.build or args.ci)):
        parser.error('jobs 必须大于零；replace-tag 需要 push-tag；package-only 不能同时 build/ci')
    version = version_of(ROOT)
    windows = sys.platform == 'win32'
    target = {'win32': 'windows', 'darwin': 'macos'}.get(sys.platform, sys.platform) + '-' + {
        'amd64': 'x64', 'x86_64': 'x64', 'aarch64': 'arm64'}.get(platform.machine().lower(), platform.machine().lower())
    binary = ROOT / ('magel.exe' if windows else 'magel')
    vsix = ROOT / f'vscode/dist/magel-editor-{version}.vsix'
    if args.dry_run:
        print(f'版本 {version}，目标 {target}，输出 {args.out_dir.resolve()}')
        print('复用产物 → 版本/字节/反射/依赖验证 → ZIP/SHA256' if args.package_only else
              '生成书稿/手册 → XeLaTeX → 内嵌资产 → 默认优化与双反射构建 → 回归 → npm ci/package → 交付验证 → ZIP/SHA256')
        print('远端操作：' + ('推送 master 与标签' if args.push_tag else '无'))
        return 0
    env = dict(os.environ, MAGEL_LANG='zh', PARSEENGINE_LANG='zh', MAGEL_BIN=str(binary))
    env.pop('MAGEL_NO_OPT', None)
    if args.toolchain_dir:
        env['PATH'] = str(args.toolchain_dir.resolve(strict=True)) + os.pathsep + env.get('PATH', '')
    if not args.package_only:
        for script in ('tools/library_reference.py', 'doc/book/latex/tools/md2tex.py'):
            run([sys.executable, script], env=env)
        latex = ROOT / 'doc/book/latex'
        for _ in range(3):
            run(['xelatex', '-interaction=nonstopmode', '-halt-on-error', 'book.tex'], cwd=latex, env=env)
        for script in ('tools/embed_manual.py', 'tools/embed_stdlib.py'):
            run([sys.executable, script], env=env)
        if args.ci:
            run(['sh', 'ci.sh'], env=env)
        run(['sh', 'gen.sh'], env=env)
        command = ['make', '-j' + str(args.jobs), '-W', 'source/magel_main.cpp', 'TARGET=' + binary.name,
                   'MAGEL_WITH_REFLECTION=1', 'STACKVM_WITH_REFLECTION=1']
        if windows:
            command += ['LDFLAGS=-Wl,--gc-sections -s -static -static-libgcc -static-libstdc++']
        run(['sh', '-c', 'exec "$@"', 'magel-release', *command] if windows else command, env=env)
        for check in CHECKS:
            run([sys.executable, *check.split()], env=env)
        run(['npm.cmd' if windows else 'npm', 'ci', '--no-audit', '--no-fund'], cwd=ROOT / 'vscode', env=env)
        run(['npm.cmd' if windows else 'npm', 'run', 'package'], cwd=ROOT / 'vscode', env=env)
    verify_assets(ROOT, binary, vsix, version, windows, env)
    revision = run(['git', 'rev-parse', 'HEAD'], capture=True)
    dirty = bool(run(['git', 'status', '--porcelain'], capture=True))
    archive = package_assets(ROOT, args.out_dir.resolve(), binary, vsix, version, target, revision, dirty)
    print('已生成：' + str(archive))
    if args.push_tag:
        publish_tag(version, args.replace_tag)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, zipfile.BadZipFile, ET.ParseError) as error:
        print('发布失败：' + str(error), file=sys.stderr)
        raise SystemExit(1)
