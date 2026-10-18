'use strict';
const assert = require('node:assert/strict');
const { registerMacroPreview } = require('../src/macros');

async function main() {
  let command, provider, definition, hover, closed, flushed = false, shown, result = { text: 'int64 answer=4;', diagnostics: [] };
  const errors = [];
  const uri = (value) => ({ scheme: value.split(':')[0], toString: () => value });
  const document = { languageId: 'magel', uri: uri('file:///project/main.mgl') };
  const vscode = {
    Location: class { constructor(uri, range) { Object.assign(this, { uri, range }); } },
    Position: class { constructor(line, character) { Object.assign(this, { line, character }); } },
    Range: class { constructor(...coordinates) { this.coordinates = coordinates; } },
    Hover: class { constructor(contents) { this.contents = contents; } },
    MarkdownString: class { constructor(value) { this.value = value; } },
    Uri: { parse: uri }, ViewColumn: { Beside: 2 },
    workspace: {
      registerTextDocumentContentProvider(scheme, value) { assert.equal(scheme, 'magel-expanded'); provider = value; return { dispose() {} }; },
      onDidCloseTextDocument(callback) { closed = callback; return { dispose() {} }; },
      async openTextDocument(value) { return { uri: value, text: provider.provideTextDocumentContent(value) }; },
    },
    commands: { registerCommand(name, callback) { assert.equal(name, 'magel.previewMacroExpansion'); command = callback; return { dispose() {} }; } },
    languages: { registerDefinitionProvider(selector, value) { definition = value; return { dispose() {} }; }, registerHoverProvider(selector, value) { hover = value; return { dispose() {} }; }, async setTextDocumentLanguage(value, language) { assert.equal(language, 'magel'); return value; } },
    window: {
      activeTextEditor: { document }, async showInformationMessage() {},
      async showErrorMessage(message) { errors.push(message); },
      async showTextDocument(value, options) { shown = value; assert.equal(options.viewColumn, 2); },
    },
  };
  const context = { subscriptions: [] };
  registerMacroPreview(vscode, context, {
    flushDocument(value) { assert.equal(value, document); flushed = true; },
    getClient: () => ({ running: true, async request(method, params) {
      assert.ok(flushed); assert.equal(method, 'magel/expandMacro');
      assert.equal(params.textDocument.uri, document.uri.toString()); return result;
    } }),
  });
  await command(); assert.equal(shown.text, result.text); assert.deepEqual(errors, []);
  result = { text: '__magel_macro_vec_1_10', diagnostics: [], mappings: [{ expandedLine: 0, uri: document.uri.toString(), sourceLine: 8 }],
    temporaries: [{ name: '__magel_macro_vec_1_10', slot: '$10', macro: 'vec', expansionId: '1',
      definition: { uri: document.uri.toString(), range: { start: { line: 2, character: 4 }, end: { line: 2, character: 7 } } } }] };
  await command();
  shown.getWordRangeAtPosition = () => ({}); shown.getText = () => result.text;
  assert.match(hover.provideHover(shown, { line: 0, character: 2 }).contents.value, /\$10/);
  assert.deepEqual(definition.provideDefinition(shown, { line: 0, character: 2 }).range.coordinates, [2, 4, 2, 7]);
  shown.getWordRangeAtPosition = () => undefined;
  assert.equal(definition.provideDefinition(shown, { line: 0, character: 0 }).range.line, 8);
  closed(shown); assert.equal(provider.provideTextDocumentContent(shown.uri), '');
  result = { diagnostics: ['宏实参数量错误'], text: '' };
  await command(); assert.match(errors[0], /宏实参数量错误/);
  for (const item of context.subscriptions) item.dispose();
  console.log('宏展开插件验证通过：同步未保存内容、只读预览、关闭清理与错误展示。');
}
main().catch((error) => { console.error(error); process.exitCode = 1; });
