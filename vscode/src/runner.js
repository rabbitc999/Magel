'use strict';

/**
 * 「运行当前文件」的命令行构造。
 *
 * 真正跑的是编译器自带的场景命令 `chali run <文件>`；只依赖 Node 标准库、不引用 `vscode`，
 * 所以能在纯 Node 下单独测试。工作目录由调用方设成文件所在目录 —— 这样程序里的相对路径
 * （数据区、输入文件）跟手工跑 `chali` 时一致。
 */

/**
 * 需要时给参数加引号（含空白或引号才加，cmd 与 POSIX shell 都认双引号）。
 *
 * @param {string} value 原始参数。
 * @returns {string} 可直接拼进命令行的参数。
 */
function quote(value) {
  const text = String(value);
  if (!/[\s"]/.test(text)) return text;
  return `"${text.replace(/"/g, '\\"')}"`;
}

/**
 * 拼出一行运行命令。
 *
 * @param {string} chaliPath 编译器路径。
 * @param {string} filePath 要运行的 `.chl` 文件。
 * @returns {string} 一行命令。
 */
function buildRunCommand(chaliPath, filePath) {
  return `${quote(chaliPath)} run ${quote(filePath)}`;
}

module.exports = { buildRunCommand, quote };
