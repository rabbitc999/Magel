'use strict';

/** @brief 检查客户端并行响应配对、请求取消、监听释放与诊断版本透传。 */
const assert = require('node:assert/strict');
const { LspClient } = require('../src/lspClient');

/** @brief 用内存帧代替传输，确定性模拟乱序和过期响应。 */
async function main() {
  const client = new LspClient('unused');
  client.process = { exitCode: null }; client.stopped = false;
  const sent = []; client._send = message => sent.push(message);
  const params = { textDocument: { uri: 'file:///sample.mgl' } };
  const first = client.request('textDocument/hover', params);
  const hoverId = sent.at(-1).id;
  const completion = client.request('textDocument/completion', params);
  const completionId = sent.at(-1).id;
  const newer = client.request('textDocument/hover', params);
  const newerId = sent.at(-1).id;
  assert.equal(await first, null);
  assert.ok(sent.some(frame => frame.method === '$/cancelRequest' && frame.params.id === hoverId));
  client._handle({ id: hoverId, result: 'stale' });
  client._handle({ id: completionId, result: ['completion'] });
  client._handle({ id: newerId, result: 'fresh' });
  assert.deepEqual(await completion, ['completion']); assert.equal(await newer, 'fresh');

  let cancel; let disposed = 0;
  const token = { isCancellationRequested: false,
    onCancellationRequested(callback) { cancel = callback; return { dispose() { disposed++; } }; } };
  const pending = client.request('textDocument/signatureHelp', params, token);
  cancel(); assert.equal(await pending, null); assert.equal(disposed, 1);
  assert.equal(client.pending.size, 0);
  const count = sent.length;
  assert.equal(await client.request('textDocument/hover', params, { isCancellationRequested: true }), null);
  assert.equal(sent.length, count);
  const normal = client.request('textDocument/hover', params, token);
  client._handle({ id: sent.at(-1).id, result: 'done' });
  assert.equal(await normal, 'done'); assert.equal(disposed, 2);
  let diagnostic;
  client.on('diagnostics', value => { diagnostic = value; });
  client._handle({ method: 'textDocument/publishDiagnostics', params: { uri: params.textDocument.uri, version: 7, diagnostics: [] } });
  assert.equal(diagnostic.version, 7);
  assert.equal(client.pending.size, 0);
  console.log('LSP 客户端乱序响应、请求替换、取消通知、监听释放和诊断版本透传通过。');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
