'use strict';

/**
 * 插件清单自检：确认 `magel`（`.mgl`）真的被注册齐了。
 *
 * 打包流程的第一步——注册不完整就不该产出安装包。它检查：
 *   1. `contributes.languages` 里有 id 为 `magel`、扩展名含 `.mgl` 的语言项；
 *   2. 指定的 `language-configuration.json` 存在且能解析（注释 / 折叠标记）；
 *   3. `contributes.grammars` 里有 scopeName 为 `source.magel` 的高亮文件，且文件存在、有规则；
 *   4. `activationEvents` 里有 `onLanguage:magel`；
 *   5. `main` 指向的入口文件存在；
 *   6. 语言级编辑器默认值 `[magel]` 已声明；
 *   7. 命令与设置齐（重启服务器 / 日志 / 文档 / **运行**，`magel.path` / `magel.traceServer`）；
 *   8. 宏块折叠由 `src/folding.js` 提供（`planFoldingRanges` 是函数）；
 *   9. 图标 `icon.png` 存在、是 PNG、且不小于 128×128（市场要求）；
 *  10. **只注册 `.mgl`**：`.gram` / `.metro` 归根目录的 GramEditor 插件，不该在这里重复声明。
 *  11. 插件版本与仓库根目录 VERSION 中的 Magel 程序版本一致。
 *
 * 用法（在仓库任意目录执行都可以）：
 *   node magel/vscode/tools/check-manifest.js
 *
 * 返回码 0 表示全部通过，1 表示有问题。
 */

const fs = require('node:fs');
const path = require('node:path');

/** 插件根目录（本文件在 <插件根>/tools/ 下）。 */
const EXTENSION_DIR = path.resolve(__dirname, '..');

/** 期望注册的语言 id。 */
const LANGUAGE_ID = 'magel';

/** 期望的语言扩展名。 */
const LANGUAGE_EXTENSION = '.mgl';

/** 期望的 TextMate scope 名。 */
const SCOPE_NAME = 'source.magel';

let failures = 0;

/**
 * 记一条断言。
 *
 * @param {string} title 断言描述。
 * @param {boolean} ok 是否通过。
 * @param {string} [detail] 失败时的补充说明。
 */
function check(title, ok, detail) {
  if (!ok) failures += 1;
  console.log(`   [${ok ? 'OK' : '失败'}] ${title}${!ok && detail ? `（${detail}）` : ''}`);
}

/**
 * 读一个 JSON 文件。
 *
 * @param {string} file 路径。
 * @returns {any} 解析结果；文件不存在或坏格式时返回 null。
 */
function readJson(file) {
  try {
    return JSON.parse(fs.readFileSync(file, 'utf8'));
  } catch {
    return null;
  }
}

/**
 * 读 PNG 的宽高（只解析头部 24 字节，不依赖任何图形库）。
 *
 * @param {string} file PNG 文件路径。
 * @returns {{width: number, height: number}|null} 尺寸；不是 PNG 时返回 null。
 */
function readPngSize(file) {
  const buffer = fs.readFileSync(file);
  if (buffer.length < 24) return null;
  if (buffer.toString('ascii', 1, 4) !== 'PNG') return null;
  return { width: buffer.readUInt32BE(16), height: buffer.readUInt32BE(20) };
}

/**
 * 自检主流程。
 */
function main() {
  console.log('== Magel 插件清单自检 ==');
  const pkg = readJson(path.join(EXTENSION_DIR, 'package.json'));
  check('package.json 可解析', !!pkg);
  if (!pkg) {
    console.log('\n清单检查失败 1 项');
    process.exit(1);
  }

  const version = fs.readFileSync(path.join(EXTENSION_DIR, '..', 'VERSION'), 'utf8').trim();
  check('插件版本与 Magel 程序版本一致', pkg.version === version,
    `插件 ${pkg.version}，Magel ${version}`);

  const contributes = pkg.contributes || {};
  const languages = contributes.languages || [];
  const grammars = contributes.grammars || [];
  const defaults = contributes.configurationDefaults || {};
  const commands = contributes.commands || [];
  const activationEvents = pkg.activationEvents || [];
  const properties = (contributes.configuration && contributes.configuration.properties) || {};

  // 1. 语言注册：id / 扩展名 / 配置文件。
  const language = languages.find((item) => item && item.id === LANGUAGE_ID);
  check(`注册了语言 ${LANGUAGE_ID}`, !!language, 'contributes.languages 里没有对应项');
  if (language) {
    const extensions = language.extensions || [];
    check(`语言 ${LANGUAGE_ID} 关联 ${LANGUAGE_EXTENSION} 扩展名`,
      extensions.includes(LANGUAGE_EXTENSION), `实际：${extensions.join('、') || '（空）'}`);
    const config = readJson(path.join(EXTENSION_DIR, language.configuration || ''));
    check('语言配置文件可解析', !!config);
    if (config) check('声明了注释与折叠标记',
      !!(config.comments && config.comments.lineComment === '//'));
  }

  // 2. 高亮语法注册：scopeName 与实际文件。
  const grammar = grammars.find((item) => item && item.language === LANGUAGE_ID);
  check(`语法高亮绑定到语言 ${LANGUAGE_ID}`, !!grammar);
  if (grammar) {
    check(`语法 scopeName 为 ${SCOPE_NAME}`, grammar.scopeName === SCOPE_NAME, `实际：${grammar.scopeName}`);
    const grammarJson = readJson(path.join(EXTENSION_DIR, grammar.path || ''));
    check('语法文件可解析且 scopeName 一致', !!(grammarJson && grammarJson.scopeName === SCOPE_NAME));
    check('语法文件包含规则',
      !!(grammarJson && Array.isArray(grammarJson.patterns) && grammarJson.patterns.length > 0));
  }

  // 3. 激活事件与入口。
  check(`activationEvents 包含 onLanguage:${LANGUAGE_ID}`,
    activationEvents.includes(`onLanguage:${LANGUAGE_ID}`));
  check('main 指向存在的入口文件',
    !!pkg.main && fs.existsSync(path.join(EXTENSION_DIR, pkg.main)), pkg.main);

  // 4. 编辑器默认值 / 命令 / 设置。
  check(`声明了 [${LANGUAGE_ID}] 编辑器默认值`, !!defaults[`[${LANGUAGE_ID}]`]);
  for (const command of ['magel.restartServer', 'magel.showServerLog', 'magel.openDoc', 'magel.run', 'magel.installDependencies', 'magel.checkDependencies', 'magel.openLibraries', 'magel.previewMacroExpansion']) {
    check(`注册了命令 ${command}`, commands.some((item) => item.command === command));
  }
  check('声明了 magel.path', !!properties['magel.path']);
  check('声明了 magel.traceServer', !!properties['magel.traceServer']);

  // 5. 折叠与图标。
  const menus = contributes.menus || {};
  const runMenu = ['editor/title/run', 'editor/context'];
  check('运行按钮挂在标题栏运行位与右键菜单',
    runMenu.every((slot) => (menus[slot] || []).some(
      (item) => item.command === 'magel.run' && item.when === `resourceLangId == ${LANGUAGE_ID}`)));
  check('标题栏只有一个 ▶（不再往 editor/title 里重复挂）',
    !(menus['editor/title'] || []).some((item) => item.command === 'magel.run'));
  check('「运行」由 src/runner.js 提供',
    typeof require(path.join(EXTENSION_DIR, 'src', 'runner.js')).buildRunCommand === 'function');

  check('有 LICENSE（MIT，vsce 打包会找它）',
    String(pkg.license || '').toUpperCase() === 'MIT'
      && fs.existsSync(path.join(EXTENSION_DIR, 'LICENSE')));
  check('宏块折叠由 src/folding.js 提供',
    typeof require(path.join(EXTENSION_DIR, 'src', 'folding.js')).planFoldingRanges === 'function');
  check('package.json 声明了 icon', typeof pkg.icon === 'string');
  if (typeof pkg.icon === 'string') {
    const iconPath = path.join(EXTENSION_DIR, pkg.icon);
    const exists = fs.existsSync(iconPath);
    check('图标文件存在', exists, iconPath);
    if (exists) {
      const size = readPngSize(iconPath);
      check('图标是 PNG 且不小于 128×128',
        !!size && size.width >= 128 && size.height >= 128,
        size ? `实际 ${size.width}×${size.height}` : '不是 PNG');
    }
  }

  // 6. 只注册 `.mgl`：`.gram` / `.metro` 归根目录的 GramEditor。
  check('没有重复声明 .gram（归根目录插件）',
    !languages.some((item) => item && (item.extensions || []).includes('.gram')));
  check('没有重复声明 .metro（归根目录插件）',
    !languages.some((item) => item && (item.extensions || []).includes('.metro')));

  console.log(failures === 0 ? '\n清单检查通过：magel（.mgl）注册完整' : `\n清单检查失败 ${failures} 项`);
  process.exit(failures === 0 ? 0 : 1);
}

main();
