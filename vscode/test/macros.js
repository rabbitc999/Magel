'use strict';

const assert = require('node:assert/strict');
const { registerMacroPreview, renderPage, renderNode, parsePayload } = require('../src/macros');

/**
 * 构造与编译器 `magel/expandMacro` 返回结构一致的展开结果；两处调用各带一棵树。
 * @returns {string} 序列化后的 `text` 字段。
 */
function expansionText() {
  return JSON.stringify({
    format: 'hygienic-ast',
    expansions: [
      {
        file: 'file:///project/main.mgl', name: 'PRINT_ALL', line: 32, column: 5,
        tree: {
          kind: 'template', name: '', origin: 'definition', children: [{
            kind: 'call', name: '', origin: 'definition', children: [
              { kind: 'identifier', name: 'println', origin: 'definition', children: [] },
              { kind: 'integer', name: '1', origin: 'argument', value: 1, type: 'int64', children: [] },
            ],
          }],
        },
      },
      {
        file: 'file:///project/main.mgl', name: 'SUM_SQUARES', line: 34, column: 5,
        tree: { kind: 'template', name: '', origin: 'definition', children: [
          { kind: 'declaration', name: 'local', origin: 'definition', type: 'int64', children: [] },
        ] },
      },
    ],
  });
}

async function main() {
  const errors = [], infos = [], handlers = [];
  let command, created = [], opened = null, shown = null, flushed = false;
  const uri = (value) => ({
    scheme: value.split(':')[0], path: value.replace(/^file:\/\//, ''), toString: () => value,
  });
  const document = { languageId: 'magel', uri: uri('file:///project/main.mgl'), version: 3 };
  const vscode = {
    ViewColumn: { One: 1, Beside: 2 },
    Position: class { constructor(line, character) { Object.assign(this, { line, character }); } },
    Range: class { constructor(start, end) { Object.assign(this, { start, end }); } },
    Selection: class { constructor(anchor, active) { Object.assign(this, { anchor, active }); } },
    Uri: { parse: uri },
    workspace: { async openTextDocument(value) { opened = value; return { uri: value }; } },
    commands: {
      registerCommand(name, callback) {
        assert.equal(name, 'magel.previewMacroExpansion');
        command = callback;
        return { dispose() {} };
      },
    },
    window: {
      activeTextEditor: { document },
      async showInformationMessage(message) { infos.push(message); },
      async showErrorMessage(message) { errors.push(message); },
      async showTextDocument(value) {
        shown = {
          uri: value.uri, selection: null, revealRange(range) { this.revealed = range; },
        };
        return shown;
      },
      createWebviewPanel(viewType, title, column, options) {
        const panel = {
          viewType, title, column, options, disposed: 0, webview: {
            cspSource: 'vscode-webview://panel', html: '',
            onDidReceiveMessage(callback) {
              handlers.push(callback);
              return { dispose() {} };
            },
            postMessage() {},
          },
          onDidDispose(callback) { this.disposedHandlers = [callback]; },
          dispose() { this.disposed += 1; (this.disposedHandlers || []).forEach((item) => item()); },
        };
        created.push(panel);
        return panel;
      },
    },
  };
  let result = { text: expansionText(), format: 'json', diagnostics: [] };
  const context = { subscriptions: [] };
  registerMacroPreview(vscode, context, {
    flushDocument(value) { assert.equal(value, document); flushed = true; },
    getClient: () => ({
      running: true,
      async request(method, params) {
        assert.ok(flushed); assert.equal(method, 'magel/expandMacro');
        assert.equal(params.textDocument.uri, document.uri.toString());
        return result;
      },
    }),
  });

  await command();
  assert.deepEqual(errors, []);
  // 两处调用生成两张独立预览图，各自只包含自己的宏。
  assert.equal(created.length, 2);
  const [first, second] = created;
  assert.match(first.title, /PRINT_ALL（第 32 行）/);
  assert.match(second.title, /SUM_SQUARES（第 34 行）/);
  assert.equal(first.options.active, true);
  assert.equal(second.options.active, false);
  assert.match(first.webview.html, /^<!DOCTYPE html>/);
  assert.match(first.webview.html, /<meta name="viewport"/);
  assert.match(first.webview.html, /第 1 \/ 2 处/);
  assert.match(first.webview.html, /<span class="kind">identifier<\/span><span class="name">println<\/span>/);
  assert.match(first.webview.html, /class="leaf argument"/);
  assert.match(first.webview.html, /<span class="type">: int64<\/span>/);
  assert.doesNotMatch(first.webview.html, /SUM_SQUARES/);
  assert.doesNotMatch(second.webview.html, /PRINT_ALL/);
  // 预览是 AST 图而不是 JSON：不出现序列化字段名。
  assert.doesNotMatch(first.webview.html, /hygienic-ast/);
  assert.doesNotMatch(first.webview.html, /"children":/);

  // 双击图头回到源码调用处。
  await handlers[0]({ type: 'reveal', line: 32, column: 5 });
  assert.equal(opened.toString(), document.uri.toString());
  assert.equal(shown.selection.active.line, 31);
  assert.equal(shown.selection.active.character, 5);
  assert.equal(shown.revealed.start.line, 31);

  // 再次预览：旧图全部关闭，重新开一组。
  const previous = created.slice();
  await command();
  assert.equal(previous[0].disposed, 1);
  assert.equal(previous[1].disposed, 1);
  assert.equal(created.length, 4);

  result = { text: JSON.stringify({ format: 'hygienic-ast', expansions: [] }), format: 'json', diagnostics: [] };
  await command();
  assert.match(infos.at(-1), /没有可展开的宏调用/);

  result = { diagnostics: ['宏实参数量错误'], text: '' };
  await command();
  assert.match(errors[0], /宏实参数量错误/);

  result = { text: '{不是 JSON', diagnostics: [] };
  await command();
  assert.match(errors.at(-1), /合法/);

  for (const item of context.subscriptions) item.dispose();
  assert.equal(created.at(-1).disposed, 1);

  assert.equal(parsePayload({ expansions: [] }).expansions.length, 0);
  assert.match(renderNode({ kind: 'integer', name: '1', value: 1, origin: 'argument' }, 0), /class="leaf argument"/);
  assert.match(renderNode({ kind: 'call', name: '', children: [{ kind: 'identifier', name: 'f' }] }, 0),
    /<details class="branch definition" open>/);
  assert.match(renderNode({ kind: 'call', name: '', children: [null, { kind: 'identifier', name: 'f' }] }, 3),
    /class="branch definition"/);
  assert.equal(renderNode(null, 0), '');
  // 静态导出：不注入编辑器脚本时也能直接放进网页。
  const staticPage = renderPage({ name: 'ADD', line: 3, column: 0, tree: { kind: 'binary', name: '+' } }, 0,
    { total: 1, interactive: false });
  assert.doesNotMatch(staticPage, /acquireVsCodeApi/);
  assert.match(staticPage, /data-macro="ADD"/);
  console.log('宏展开插件验证通过：一宏一图、可直接嵌入网页、双击回源、面板复用与错误展示。');
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
