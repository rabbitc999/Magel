'use strict';

/**
 * 找 `chali` 可执行文件（ChaLi 的语言服务器内嵌在编译器里：`chali lsp`）。
 *
 * 只依赖 Node 标准库，不引用 `vscode`，所以能在纯 Node 下单独测试。
 *
 * 顺序（先看设置，再看**扩展自带的编译器**，然后才是工作区与 PATH）：
 *   1. 设置里的 `chali.path`；
 *   2. 扩展包里内嵌的那份：`bin/chali[.exe]`（打 VSIX 时由
 *      `tools/embed-compiler.js` 放进去，装完即用）；
 *   3. 工作区里几处常见位置：`build/work/chali/chali`、`chali/chali`、
 *      `build/chali/chali`（Windows 上带 `.exe`）；
 *   4. PATH 里的 `chali`。
 */

const fs = require('node:fs');
const path = require('node:path');

/** Windows 上加后缀。 */
const EXE = process.platform === 'win32' ? '.exe' : '';

/** 扩展包里内嵌的编译器（相对扩展根目录）。 */
const BUNDLED_CANDIDATE = path.join('bin', `chali${EXE}`);

/** 工作区里按这个顺序找。 */
const WORKSPACE_CANDIDATES = [
  path.join('build', 'work', 'chali', `chali${EXE}`),
  path.join('build', 'chali', `chali${EXE}`),
  path.join('chali', `chali${EXE}`),
  `chali${EXE}`,
];

/**
 * 文件存在且是普通文件。
 *
 * @param {string} file 路径。
 * @returns {boolean} 存在返回 true。
 */
function isFile(file) {
  try {
    return !!file && fs.statSync(file).isFile();
  } catch {
    return false;
  }
}

/**
 * 在 PATH 里找可执行文件。
 *
 * @param {string} name 名字（不含后缀）。
 * @returns {string|null} 找到返回绝对路径。
 */
function findOnPath(name) {
  const dirs = (process.env.PATH || '').split(path.delimiter).filter(Boolean);
  for (const dir of dirs) {
    const candidate = path.join(dir, name);
    if (isFile(candidate)) return candidate;
    if (!name.endsWith(EXE) && isFile(candidate + EXE)) return candidate + EXE;
  }
  return null;
}

/**
 * 定位编译器。
 *
 * @param {string} workspaceDir 工作区目录（可为空）。
 * @param {{chaliPath?: string}} [configured] 设置里的路径。
 * @param {string} [extensionDir] 扩展根目录（内嵌编译器 `bin/chali` 在它下面）。
 * @returns {{chali: string|null, source: string}} 结果与来源说明（写日志用）。
 */
function resolveChaLi(workspaceDir, configured, extensionDir) {
  const fromSettings = configured && configured.chaliPath ? configured.chaliPath.trim() : '';
  if (fromSettings) {
    return isFile(fromSettings)
      ? { chali: fromSettings, source: '设置 chali.path' }
      : { chali: null, source: `设置里的 chali.path 不存在：${fromSettings}` };
  }
  if (extensionDir) {
    const bundled = path.join(extensionDir, BUNDLED_CANDIDATE);
    if (isFile(bundled)) return { chali: bundled, source: `内嵌 ${BUNDLED_CANDIDATE}` };
  }
  if (workspaceDir) {
    for (const relative of WORKSPACE_CANDIDATES) {
      const candidate = path.join(workspaceDir, relative);
      if (isFile(candidate)) return { chali: candidate, source: `工作区 ${relative}` };
    }
  }
  const onPath = findOnPath('chali');
  if (onPath) return { chali: onPath, source: 'PATH' };
  return { chali: null, source: '没找到（设置 chali.path，或在项目里先构建工具链）' };
}

module.exports = { resolveChaLi, isFile, findOnPath, EXE, BUNDLED_CANDIDATE };
