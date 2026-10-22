'use strict';

/**
 * `.chl` 折叠块的配对（纯计算，不依赖 VS Code，便于直接测）。
 *
 * 与 CHALI 编译器的"定义期配对检查"同口径：
 *   - `$macro … $end_macro`、`$for … $end_for`；
 *   - 配对用一个栈，交叉写错时不会把层次折乱；
 *   - 收尾还没写完（编辑到一半）时按文件末尾收，块不会突然消失。
 *
 * 指令名按**完整词面**判断：`$match_if` 不是 `$match`。
 * 行号是 0 基、闭区间，和 VS Code 的 `FoldingRange` 一致。
 */

/** 开标签 → 折叠后显示的提示文字。顺序就是判断顺序。 */
const OPENERS = [
  ['$macro', '宏定义'],
  ['$for', 'for 展开'],
  ['$match', '语法匹配'],
  ['$regex', '正则匹配'],
];

/** 收标签。 */
const CLOSERS = ['$end_macro', '$end_for', '$end_match', '$end_regex'];

/**
 * 这一行是不是完整的 `word` 指令（后面不能紧跟标识符字符）。
 *
 * @param {string} text 去掉首尾空白的整行。
 * @param {string} word 指令名，例如 `$end_macro`。
 * @returns {boolean} 是返回 true。
 */
function isDirective(text, word) {
  if (text.length < word.length || !text.startsWith(word)) return false;
  if (text.length === word.length) return true;
  const next = text[word.length];
  return !/[A-Za-z0-9_]/.test(next);
}

/**
 * 这一行是不是某个块的开标签。
 *
 * @param {string} text 去掉首尾空白的整行。
 * @returns {string|null} 折叠提示文字；不是开标签时返回 null。
 */
function openerName(text) {
  for (const [word, name] of OPENERS) {
    if (isDirective(text, word)) return name;
  }
  return null;
}

/**
 * 算出所有可折叠块。
 *
 * @param {string[]} lines 按行拆好的源码（不含换行符）。
 * @returns {{start: number, end: number, name: string}[]} 按起始行递增、只含跨行的块。
 */
function planFoldingRanges(lines) {
  const blocks = [];
  const open = [];
  for (let index = 0; index < lines.length; index++) {
    const text = String(lines[index]).trim();
    const name = openerName(text);
    if (name !== null) {
      const directive = OPENERS.find(([word]) => isDirective(text, word))[0];
      open.push({ line: index, name, closing: '$end_' + directive.slice(1) });
      continue;
    }
    const closing = CLOSERS.find((word) => isDirective(text, word));
    if (!closing || !open.length || open[open.length - 1].closing !== closing) continue;
    const start = open.pop();
    if (start === undefined) continue; // 多余的收尾：交给编译器诊断，这里不报
    if (index > start.line) blocks.push({ start: start.line, end: index, name: start.name });
  }

  // 收尾没写完：按到文件末尾收，编辑中的代码也能折。
  const last = lines.length - 1;
  while (open.length > 0) {
    const start = open.pop();
    if (last > start.line) blocks.push({ start: start.line, end: last, name: start.name });
  }

  blocks.sort((a, b) => (a.start !== b.start ? a.start - b.start : a.end - b.end));
  return blocks;
}

module.exports = { planFoldingRanges };
