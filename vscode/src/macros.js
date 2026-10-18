'use strict';

/**
 * 注册只读宏展开预览，使用服务器的真实预处理器，支持未保存源码。
 * @param {object} vscode VSCode API。
 * @param {object} context 扩展上下文。
 * @param {object} deps 当前语言客户端与即时文档同步回调。
 * @returns {void} 无。
 */
function registerMacroPreview(vscode, context, deps) {
  const contents = new Map();
  let serial = 0;
  /**
   * 从只读展开文档中的编译器临时名找到原始整数槽。
   * @param {object} document 展开文档。
   * @param {object} position 查询位置。
   * @returns {object|undefined} 服务端提供的临时名元数据。
   */
  const temporaryAt = (document, position) => {
    const preview = contents.get(document.uri.toString());
    const range = document.getWordRangeAtPosition(position, /__magel_macro_[A-Za-z0-9_]+/);
    const name = range && document.getText(range);
    return preview && (preview.temporaries || []).find((item) => item.name === name);
  };
  context.subscriptions.push(vscode.workspace.registerTextDocumentContentProvider('magel-expanded', {
    provideTextDocumentContent(uri) { return (contents.get(uri.toString()) || {}).text || ''; },
  }));
  context.subscriptions.push(vscode.languages.registerDefinitionProvider({ scheme: 'magel-expanded', language: 'magel' }, {
    provideDefinition(document, position) {
      const temporary = temporaryAt(document, position);
      if (temporary) {
        const location = temporary.definition;
        return new vscode.Location(vscode.Uri.parse(location.uri), new vscode.Range(
          location.range.start.line, location.range.start.character, location.range.end.line, location.range.end.character));
      }
      const preview = contents.get(document.uri.toString());
      const mapping = preview && (preview.mappings || []).find((item) => item.expandedLine === position.line);
      if (!mapping) return [];
      // 映射来自编译器的 LineOrigin；列没有精确来源时回到该行开头，避免伪造精确定位。
      return new vscode.Location(vscode.Uri.parse(mapping.uri), new vscode.Position(mapping.sourceLine, 0));
    },
  }));
  context.subscriptions.push(vscode.languages.registerHoverProvider({ scheme: 'magel-expanded', language: 'magel' }, {
    provideHover(document, position) {
      const temporary = temporaryAt(document, position);
      if (!temporary) return null;
      return new vscode.Hover(new vscode.MarkdownString(
        `宏 \`${temporary.macro}\` 的临时槽 \`${temporary.slot}\`，展开编号 \`${temporary.expansionId}\`。\n\n生成名：\`${temporary.name}\`。使用“转到定义”查看原始整数槽。`));
    },
  }));
  context.subscriptions.push(vscode.workspace.onDidCloseTextDocument((document) => {
    if (document.uri.scheme === 'magel-expanded') contents.delete(document.uri.toString());
  }));
  context.subscriptions.push({ dispose() { contents.clear(); } });
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
    try {
      deps.flushDocument(editor.document);
      const version = editor.document.version;
      const result = await client.request('magel/expandMacro', {
        textDocument: { uri: editor.document.uri.toString() },
      });
      if (editor.document.version !== version) throw new Error('预览期间源码已更新，请重新展开。');
      if (!result || (result.diagnostics && result.diagnostics.length)) {
        throw new Error(result && result.diagnostics ? result.diagnostics.join('\n') : '服务器未返回展开结果');
      }
      const uri = vscode.Uri.parse(`magel-expanded:/宏展开-${++serial}.mgl`);
      contents.set(uri.toString(), result);
      const document = await vscode.workspace.openTextDocument(uri);
      await vscode.languages.setTextDocumentLanguage(document, 'magel');
      await vscode.window.showTextDocument(document, { viewColumn: vscode.ViewColumn.Beside, preview: true });
    } catch (error) {
      await vscode.window.showErrorMessage(`Magel：宏展开失败：${error.message}`);
    }
  }));
}

module.exports = { registerMacroPreview };
