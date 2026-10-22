'use strict';

/**
 * 把展开结果里的 AST 序列化成可读的树状结构。
 * @param {string|object} payload 语言服务器返回的 `text`（JSON 文本）或已解析对象。
 * @returns {object} 含 `expansions` 数组的展开结果。
 */
function parsePayload(payload) {
  if (typeof payload !== 'string') return payload || {};
  try {
    return JSON.parse(payload);
  } catch (error) {
    throw new Error(`展开结果不是合法的 AST 描述：${error.message}`);
  }
}

/**
 * 转义 HTML 文本与属性。
 * @param {*} value 待转义的值。
 * @returns {string} 转义后的字符串。
 */
function escapeHtml(value) {
  return String(value === undefined || value === null ? '' : value)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

/**
 * 把 AST 节点渲染成可折叠的树。
 * @param {object|null} node 节点；编译器用 `null` 表示被裁剪的空槽。
 * @param {number} depth 当前深度，用于控制默认展开层级。
 * @returns {string} 节点对应的 HTML。
 */
function renderNode(node, depth) {
  if (!node || typeof node !== 'object') return '';
  const kind = escapeHtml(node.kind || 'node');
  const name = node.name === undefined || node.name === null ? '' : String(node.name);
  const details = [];
  if (node.member) details.push(`<span class="member">.${escapeHtml(node.member)}</span>`);
  if (node.value !== undefined && node.value !== null && String(node.value) !== name)
    details.push(`<span class="value">= ${escapeHtml(node.value)}</span>`);
  if (node.type) details.push(`<span class="type">: ${escapeHtml(node.type)}</span>`);
  const origin = node.origin === 'argument' ? 'argument' : 'definition';
  const label = `<span class="kind">${kind}</span>`
    + (name ? `<span class="name">${escapeHtml(name)}</span>` : '')
    + details.join('');
  const children = Array.isArray(node.children)
    ? node.children.filter((child) => child && typeof child === 'object') : [];
  if (!children.length) return `<div class="leaf ${origin}">${label}</div>`;
  const body = children.map((child) => renderNode(child, depth + 1)).join('');
  return `<details class="branch ${origin}"${depth < 3 ? ' open' : ''}>`
    + `<summary>${label}<span class="count">${children.length}</span></summary>`
    + `<div class="children">${body}</div></details>`;
}

/**
 * 单张预览图共用的样式：颜色全部带兜底色，脱离编辑器直接放进网页也能读。
 * @returns {string} `<style>` 片段。
 */
function pageStyle() {
  return `<style>
  :root { color-scheme: light dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: 20px 22px 26px;
    background: var(--vscode-editor-background, #ffffff);
    color: var(--vscode-editor-foreground, #1f2328);
    font-family: var(--vscode-font-family, -apple-system, "Segoe UI", "Microsoft YaHei", sans-serif);
    font-size: var(--vscode-font-size, 13px); line-height: 1.5;
  }
  figure.macro { margin: 0; }
  figcaption { display: flex; flex-wrap: wrap; align-items: baseline; gap: 8px; margin-bottom: 12px; }
  figcaption .macro { font-size: 1.35em; font-weight: 600; color: var(--vscode-symbolIcon-functionForeground, #8250df); }
  figcaption .site { color: var(--vscode-descriptionForeground, #57606a); font-size: .92em; }
  figcaption .toolbar { margin-left: auto; display: flex; gap: 6px; }
  figcaption button {
    font: inherit; color: inherit; cursor: pointer; padding: 2px 9px; border-radius: 4px;
    border: 1px solid var(--vscode-panel-border, #d0d7de);
    background: var(--vscode-button-secondaryBackground, #f6f8fa);
  }
  figcaption button:hover { background: var(--vscode-button-secondaryHoverBackground, #eaeef2); }
  div.tree { font-family: var(--vscode-editor-font-family, ui-monospace, Consolas, monospace); }
  details { margin: 1px 0; }
  summary { cursor: pointer; list-style: none; display: flex; align-items: baseline; gap: 6px; padding: 1px 2px; }
  summary::-webkit-details-marker { display: none; }
  summary::before { content: '▸'; color: var(--vscode-descriptionForeground, #8b949e); width: .9em; }
  details[open] > summary::before { content: '▾'; }
  div.children { margin-left: 9px; padding-left: 9px; border-left: 1px dashed var(--vscode-panel-border, #d0d7de); }
  div.leaf { display: flex; align-items: baseline; gap: 6px; padding: 1px 2px 1px 20px; }
  span.kind { color: var(--vscode-symbolIcon-keywordForeground, #0550ae); }
  span.member, span.value { color: var(--vscode-symbolIcon-propertyForeground, #953800); }
  span.type { color: var(--vscode-symbolIcon-classForeground, #116329); }
  span.count {
    color: var(--vscode-descriptionForeground, #57606a); font-size: .8em; padding: 0 5px;
    border-radius: 8px; background: var(--vscode-badge-background, #eaeef2);
  }
  .argument > summary > span.kind, .leaf.argument > span.kind {
    color: var(--vscode-symbolIcon-variableForeground, #9a6700);
  }
  @media (prefers-color-scheme: dark) {
    body { background: var(--vscode-editor-background, #1e1e1e); color: var(--vscode-editor-foreground, #d4d4d4); }
    span.kind { color: var(--vscode-symbolIcon-keywordForeground, #79c0ff); }
    span.member, span.value { color: var(--vscode-symbolIcon-propertyForeground, #ffa657); }
    span.type { color: var(--vscode-symbolIcon-classForeground, #7ee787); }
    figcaption .macro { color: var(--vscode-symbolIcon-functionForeground, #d2a8ff); }
    figcaption .site, span.count { color: var(--vscode-descriptionForeground, #9da7b3); }
  }
</style>`;
}

/**
 * 生成单张宏展开预览图（一个宏一张，自包含 HTML，可直接放进网页）。
 * @param {object} expansion 编译器给出的一次展开记录。
 * @param {number} index 展开序号，从 0 开始。
 * @param {object} [options] 可选设置；`total` 为同伴数量，`interactive` 为是否注入跳转脚本。
 * @returns {string} 完整 HTML 文档。
 */
function renderPage(expansion, index, options) {
  const settings = options || {};
  const total = Number(settings.total) || 1;
  const interactive = settings.interactive !== false;
  const line = Number(expansion.line) || 1;
  const column = Number(expansion.column) || 0;
  const name = escapeHtml(expansion.name || '宏');
  const file = escapeHtml(String(expansion.file || '').split(/[\\/]/).pop() || '当前文件');
  const heading = `宏展开 · ${name}`;
  const ordinal = total > 1 ? `<span class="site">第 ${index + 1} / ${total} 处</span>` : '';
  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${escapeHtml(heading)}（${file} 第 ${line} 行）</title>
${pageStyle()}
</head>
<body>
<figure class="macro" data-macro="${name}" data-line="${line}" data-column="${column}">
  <figcaption>
    <span class="macro">${name}</span>
    <span class="site">${file} · 第 ${line} 行 · 第 ${column + 1} 列</span>
    ${ordinal}
    <span class="toolbar">
      <button type="button" data-action="expand">全部展开</button>
      <button type="button" data-action="collapse">全部折叠</button>
    </span>
  </figcaption>
  <div class="tree">${renderNode(expansion.tree, 0) || '<div class="leaf">（空模板）</div>'}</div>
</figure>
${interactive ? `<script>
  document.querySelectorAll('button[data-action]').forEach((button) => {
    button.addEventListener('click', () => {
      const open = button.dataset.action === 'expand';
      document.querySelectorAll('details').forEach((details) => { details.open = open; });
    });
  });
  const figure = document.querySelector('figure.macro');
  if (typeof acquireVsCodeApi === 'function' && figure) {
    const api = acquireVsCodeApi();
    figure.addEventListener('dblclick', () => api.postMessage({
      type: 'reveal', line: Number(figure.dataset.line), column: Number(figure.dataset.column),
    }));
  }
</script>` : ''}
</body>
</html>`;
}

/**
 * 注册 AST 展开预览：每个宏调用单独一张预览图，支持未保存源码。
 * @param {object} vscode VSCode API。
 * @param {object} context 扩展上下文。
 * @param {object} deps 当前语言客户端与即时文档同步回调。
 * @returns {void} 无。
 */
function registerMacroPreview(vscode, context, deps) {
  let panels = [];
  let source = null;
  const close = () => {
    panels.forEach((panel) => panel.dispose());
    panels = [];
  };
  context.subscriptions.push({ dispose() { close(); source = null; } });
  context.subscriptions.push(vscode.commands.registerCommand('magel.previewMacroExpansion', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'magel' || editor.document.uri.scheme !== 'file') {
      await vscode.window.showInformationMessage('Magel：请先打开一个 .mgl 源文件。');
      return;
    }
    const client = deps.getClient();
    if (!client || !client.running) {
      await vscode.window.showErrorMessage('Magel：语言服务器尚未启动。');
      return;
    }
    const document = editor.document;
    try {
      deps.flushDocument(document);
      const version = document.version;
      const result = await client.request('magel/expandMacro', {
        textDocument: { uri: document.uri.toString() },
      });
      if (document.version !== version) throw new Error('预览期间源码已更新，请重新展开。');
      if (!result || (result.diagnostics && result.diagnostics.length)) {
        throw new Error(result && result.diagnostics ? result.diagnostics.join('\n') : '服务器未返回展开结果');
      }
      const payload = parsePayload(result.text);
      const expansions = Array.isArray(payload.expansions) ? payload.expansions : [];
      if (!expansions.length) {
        await vscode.window.showInformationMessage('Magel：当前文件没有可展开的宏调用。');
        return;
      }
      close();
      source = document.uri;
      expansions.forEach((expansion, index) => {
        const panel = vscode.window.createWebviewPanel(
          'magel.macroExpansion',
          `宏展开 · ${expansion.name || '宏'}（第 ${Number(expansion.line) || 1} 行）`,
          vscode.ViewColumn.Beside,
          { enableScripts: true, retainContextWhenHidden: true, localResourceRoots: [], active: index === 0 },
        );
        panel.webview.html = renderPage(expansion, index, { total: expansions.length });
        panel.webview.onDidReceiveMessage(async (message) => {
          if (!message || message.type !== 'reveal' || !source) return;
          const open = await vscode.workspace.openTextDocument(source);
          const position = new vscode.Position(
            Math.max(0, Number(message.line) - 1), Math.max(0, Number(message.column)));
          const shown = await vscode.window.showTextDocument(open, { viewColumn: vscode.ViewColumn.One, preview: true });
          if (shown && shown.selection !== undefined) shown.selection = new vscode.Selection(position, position);
          if (shown && shown.revealRange) shown.revealRange(new vscode.Range(position, position));
        }, undefined, context.subscriptions);
        panel.onDidDispose(() => { panels = panels.filter((item) => item !== panel); }, undefined, context.subscriptions);
        panels.push(panel);
      });
    } catch (error) {
      await vscode.window.showErrorMessage(`Magel：宏展开失败：${error.message}`);
    }
  }));
}

module.exports = { registerMacroPreview, renderPage, renderNode, parsePayload };
