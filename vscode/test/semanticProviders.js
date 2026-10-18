'use strict';
const assert = require('node:assert/strict');
const Module = require('node:module');
const { registerSemanticProviders } = require('../src/semanticProviders');

/** 验证编辑器 provider 的协议转换、版本拒绝和类型提示开关。 */
async function main() {
  const providers = {}, context = { subscriptions: [] };
  const uri = { toString: () => 'file:///project/main.mgl' }, document = { uri, version: 4 };
  let enabled = true, requests = 0, response;
  class WorkspaceEdit { constructor() { this.changes = []; } set(target, edits) { this.changes.push({ target, edits }); } }
  class TextEdit { constructor(range, newText) { Object.assign(this, { range, newText }); } }
  class Location { constructor(uri, range) { Object.assign(this, { uri, range }); } }
  class InlayHint { constructor(position, label, kind) { Object.assign(this, { position, label, kind }); } }
  class Diagnostic { constructor(range, message, severity) { Object.assign(this, { range, message, severity }); } }
  class DiagnosticRelatedInformation { constructor(location, message) { Object.assign(this, { location, message }); } }
  class Position { constructor(line, character) { Object.assign(this, { line, character }); } }
  class Range { constructor(start, end) { Object.assign(this, { start, end }); } }
  const vscode = {
    WorkspaceEdit, TextEdit, Location, InlayHint, Diagnostic, DiagnosticRelatedInformation, Position, Range,
    DiagnosticSeverity: { Error: 0, Warning: 1, Information: 2, Hint: 3 },
    MarkdownString: class { constructor(value) { this.value = value; } },
    InlayHintKind: { Type: 1 }, Uri: { parse: (value) => ({ toString: () => value }) },
    workspace: { textDocuments: [document], getConfiguration: () => ({ get: () => enabled }) },
    languages: {
      registerReferenceProvider(selector, provider) { providers.references = provider; return { dispose() {} }; },
      registerRenameProvider(selector, provider) { providers.rename = provider; return { dispose() {} }; },
      registerInlayHintsProvider(selector, provider) { providers.inlay = provider; return { dispose() {} }; },
    },
  };
  registerSemanticProviders(vscode, context, {
    selector: [], toRange: (range) => range, toPosition: (position) => position,
    async request(method, params, token, current, fail) {
      assert.equal(current, document); requests++;
      if (method.includes('Rename') || method.endsWith('/rename')) assert.equal(fail, true);
      return response;
    },
  });
  const range = { start: { line: 1, character: 3 }, end: { line: 1, character: 8 } };
  response = [{ uri: uri.toString(), range }];
  const refs = await providers.references.provideReferences(document, range.start, { includeDeclaration: true });
  assert.equal(refs[0].uri.toString(), uri.toString()); assert.deepEqual(refs[0].range, range);
  response = { range, placeholder: 'value' };
  assert.equal((await providers.rename.prepareRename(document, range.start)).placeholder, 'value');
  response = { documentChanges: [{ textDocument: { uri: uri.toString(), version: 4 }, edits: [{ range, newText: 'input' }] }] };
  assert.equal((await providers.rename.provideRenameEdits(document, range.start, 'input')).changes[0].edits[0].newText, 'input');
  document.version = 5;
  await assert.rejects(providers.rename.provideRenameEdits(document, range.start, 'input'), /文档已更新/);
  response = [{ position: range.end, label: ': int32', tooltip: { value: '推断类型' } }];
  const hints = await providers.inlay.provideInlayHints(document, range);
  assert.equal(hints[0].label, ': int32'); assert.equal(hints[0].tooltip.value, '推断类型');
  enabled = false;
  const before = requests;
  assert.deepEqual(await providers.inlay.provideInlayHints(document, range), []); assert.equal(requests, before);
  const original = Module._load;
  Module._load = function load(name, ...args) { return name === 'vscode' ? vscode : original.call(this, name, ...args); };
  try {
    const { toDiagnostics } = require('../src/providers');
    const diagnostic = toDiagnostics([{ range, message: '移动后读取', code: 'MGL1205', severity: 1,
      relatedInformation: [{ location: { uri: uri.toString(), range }, message: '此处捕获' }] }])[0];
    assert.equal(diagnostic.code, 'MGL1205'); assert.equal(diagnostic.relatedInformation[0].message, '此处捕获');
  } finally { Module._load = original; }
  for (const subscription of context.subscriptions) subscription.dispose();
  console.log('引用与重命名 provider、过期版本拒绝、内联提示开关及关联诊断转换通过。');
}
main().catch((error) => { console.error(error); process.exitCode = 1; });
