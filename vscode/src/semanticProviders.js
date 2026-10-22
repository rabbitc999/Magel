'use strict';

/**
 * 注册以服务端绑定身份为依据的引用、重命名和可选类型提示。
 * @param {object} vscode 编辑器 API。
 * @param {object} context 插件上下文。
 * @param {object} deps 协议请求、选择器和位置转换。
 * @returns {void} 无。
 */
function registerSemanticProviders(vscode, context, deps) {
  const { request, selector, toRange, toPosition } = deps;
  /**
   * 构造包含文档及 UTF-16 光标的协议参数。
   * @param {object} document 当前文档。
   * @param {object} position 当前光标。
   * @returns {object} 请求参数。
   */
  const params = (document, position) => ({
    textDocument: { uri: document.uri.toString() },
    position: { line: position.line, character: position.character },
  });
  context.subscriptions.push(
    vscode.languages.registerReferenceProvider(selector, {
      async provideReferences(document, position, options, token) {
        const result = await request('textDocument/references', {
          ...params(document, position), context: { includeDeclaration: options.includeDeclaration },
        }, token, document);
        return (result || []).map((item) => new vscode.Location(vscode.Uri.parse(item.uri), toRange(item.range)));
      },
    }),
    vscode.languages.registerRenameProvider(selector, {
      async prepareRename(document, position, token) {
        const result = await request('textDocument/prepareRename', params(document, position), token, document, true);
        return result ? { range: toRange(result.range), placeholder: result.placeholder } : null;
      },
      async provideRenameEdits(document, position, newName, token) {
        const result = await request('textDocument/rename', { ...params(document, position), newName }, token, document, true);
        if (!result) return null;
        const edit = new vscode.WorkspaceEdit();
        for (const change of result.documentChanges || []) {
          const target = vscode.Uri.parse(change.textDocument.uri);
          const open = vscode.workspace.textDocuments.find((item) => item.uri.toString() === target.toString());
          if (change.textDocument.version !== null && (!open || open.version !== change.textDocument.version)) {
            throw new Error('重命名期间文档已更新，请重新执行。');
          }
          edit.set(target, change.edits.map((item) => new vscode.TextEdit(toRange(item.range), item.newText)));
        }
        return edit;
      },
    }),
    vscode.languages.registerInlayHintsProvider(selector, {
      async provideInlayHints(document, range, token) {
        if (!vscode.workspace.getConfiguration('chali', document.uri).get('inlayHints.enabled', true)) return [];
        const result = await request('textDocument/inlayHint', {
          textDocument: { uri: document.uri.toString() },
          range: { start: { line: range.start.line, character: range.start.character },
            end: { line: range.end.line, character: range.end.character } },
        }, token, document);
        return (result || []).map((item) => {
          const hint = new vscode.InlayHint(toPosition(item.position), item.label, vscode.InlayHintKind.Type);
          hint.paddingLeft = !!item.paddingLeft;
          hint.paddingRight = !!item.paddingRight;
          if (item.tooltip) hint.tooltip = new vscode.MarkdownString(item.tooltip.value || item.tooltip);
          return hint;
        });
      },
    }),
  );
}

module.exports = { registerSemanticProviders };
