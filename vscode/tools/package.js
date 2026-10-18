'use strict';

/**
 * 打 VSIX：内嵌编译器 → 清单自检 → 冒烟测试 → `vsce package`，产物落在 `dist/`。
 *
 * 用法（在 vscode/ 里跑）：
 *   npm run package                 # 全套
 *   npm run package -- --skip-tests # 只内嵌 + 打包
 *   node tools/package.js --dry-run # 只打印要跑的命令
 *   node tools/package.js --out other-dir
 *
 * `vsce` 优先用 PATH 上的那份；没有就退回 `npx --yes @vscode/vsce`（需要网络）。
 * 产物名与 `package.json` 的 name/version 对齐：`dist/magel-editor-<版本>.vsix`。
 */

const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const EXT_ROOT = path.join(__dirname, '..');
const manifest = require(path.join(EXT_ROOT, 'package.json'));

/** 取 `--xxx` 后面的值。 */
function option(name, fallback) {
  const index = process.argv.indexOf(name);
  if (index < 0) return fallback;
  const value = process.argv[index + 1];
  if (!value || value.startsWith('--')) {
    console.error(`${name} 后面要跟一个值`);
    process.exit(2);
  }
  return value;
}

const DRY_RUN = process.argv.includes('--dry-run');
const SKIP_TESTS = process.argv.includes('--skip-tests');
const outDir = path.resolve(EXT_ROOT, option('--out', 'dist'));
const vsix = path.join(outDir, `${manifest.name}-${manifest.version}.vsix`);

/**
 * 跑一条命令，失败就退出。
 *
 * @param {string} command 程序。
 * @param {string[]} args 参数。
 * @param {boolean} [shell] 是否经系统 shell（Windows 上的 vsce / npx 需要）。
 * @returns {void}
 */
function run(command, args, shell = false) {
  console.log(`\n$ ${command} ${args.join(' ')}`);
  if (DRY_RUN) return;
  // node 用绝对路径直连（`shell: true` 会把带空格的 "C:\\Program Files\\..." 拆坏）；
  // `vsce` / `npx` 在 Windows 上是 .cmd 垫片，才需要 shell。
  // 走 shell 时把命令与参数拼成一行（带空格的参数加引号）—— 给 spawnSync 同时传
  // args 数组和 shell:true 会触发 Node 的 DEP0190 警告，这样写就没有了。
  const quote = (value) => (/[\s"]/.test(value) ? `"${value.replace(/"/g, '\\"')}"` : value);
  const result = shell
    ? spawnSync([command, ...args.map(quote)].join(' '), {
        cwd: EXT_ROOT,
        stdio: 'inherit',
        shell: true,
      })
    : spawnSync(command, args, {
        cwd: EXT_ROOT,
        stdio: 'inherit',
      });
  if (result.status !== 0) {
    console.error(`\n失败（退出码 ${result.status ?? 'null'}）：${command}`);
    process.exit(result.status || 1);
  }
}

/** PATH 上有没有这个程序。 */
function onPath(command) {
  const probe = process.platform === 'win32' ? 'where' : 'which';
  return spawnSync(probe, [command], { stdio: 'ignore' }).status === 0;
}

/** 挑一个能用的 vsce。 */
function vsce() {
  if (onPath('vsce')) return ['vsce', []];
  console.log('（PATH 上没有 vsce，退回 npx --yes @vscode/vsce）');
  return ['npx', ['--yes', '@vscode/vsce']];
}

function main() {
  console.log(`== 打包 ${manifest.displayName || manifest.name} ${manifest.version} ==`);
  run(process.execPath, [path.join('tools', 'embed-compiler.js')]);
  run(process.execPath, [path.join('tools', 'check-manifest.js')]);
  if (!SKIP_TESTS) {
    run(process.execPath, [path.join('test', 'smoke.js')]);
    run(process.execPath, [path.join('test', 'dependencies.js')]);
    run(process.execPath, [path.join('test', 'macros.js')]);
    run(process.execPath, [path.join('test', 'lspCancellation.js')]);
    run(process.execPath, [path.join('test', 'semanticProviders.js')]);
    run(process.execPath, [path.join('test', 'macroHighlighting.js')]);
  }
  const [command, prefix] = vsce();
  fs.mkdirSync(outDir, { recursive: true });
  run(command, [...prefix, 'package', '--out', vsix, '--allow-missing-repository'],
      process.platform === 'win32');
  if (DRY_RUN) {
    console.log(`\n（dry-run：没有真的产出 ${path.relative(EXT_ROOT, vsix)}）`);
    return;
  }
  const size = fs.statSync(vsix).size;
  console.log(`\n已打包：${path.relative(EXT_ROOT, vsix)}（${(size / 1048576).toFixed(1)} MB）`);
}

main();
