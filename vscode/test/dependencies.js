'use strict';

/** 插件依赖命令集成测试：模拟编辑器 API，使用真实编译器与本地 HTTP 服务。 */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const http = require('node:http');
const crypto = require('node:crypto');
const { spawnSync } = require('node:child_process');
const { findProject, runDependencyProcess, registerDependencies } = require('../src/dependencies');

/** @brief 构造纯内存 ZIP 依赖包。 @returns {Buffer} 压缩包。 */
function zipPackage() {
  const code = "import io,sys,zipfile\nb=io.BytesIO()\nwith zipfile.ZipFile(b,'w') as z:\n z.writestr('magel.toml','[package]\\nname=\"values\"\\nversion=\"1.0.0\"\\n')\n z.writestr('values.mgl','module values;\\nint64 answer(){return 7;}\\n')\nsys.stdout.buffer.write(b.getvalue())";
  const result = spawnSync(process.env.MAGEL_PYTHON || (process.platform === 'win32' ? 'python' : 'python3'), ['-c', code]);
  assert.equal(result.status, 0, String(result.stderr));
  return result.stdout;
}

/** @brief 执行命令、共享缓存、失败处理、工程定位和语言服务刷新回归。 */
async function main() {
  const root = path.resolve(__dirname, '../..');
  const sourceCompiler = path.join(root, process.platform === 'win32' ? 'magel.exe' : 'magel');
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'magel vscode deps '));
  const compilerDir = path.join(directory, 'tool chain'); fs.mkdirSync(compilerDir);
  const compiler = path.join(compilerDir, path.basename(sourceCompiler)); fs.copyFileSync(sourceCompiler, compiler);
  const app = path.join(directory, 'my project'); fs.mkdirSync(app);
  const src = path.join(app, 'src'); fs.mkdirSync(src);
  const mainFile = path.join(src, 'main.mgl');
  fs.writeFileSync(mainFile, 'import values;\nint64 main(){std::lang::println("{}",values::answer());return 0;}\n');
  const archive = zipPackage(); let downloads = 0;
  const server = http.createServer((request, response) => {
    downloads += 1; response.writeHead(200, { 'Content-Length': archive.length }); response.end(archive);
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const digest = crypto.createHash('sha256').update(archive).digest('hex');
  const manifest = path.join(app, 'magel.toml');
  const text = `[package]\nname="app"\nversion="1.0.0"\n[dependencies]\nvalues="http://127.0.0.1:${server.address().port}/values.zip#sha256=${digest}"\n`;
  fs.writeFileSync(manifest, text);
  const commands = new Map(); const errors = []; const messages = []; const logs = []; const opened = [];
  let refreshed = 0; let saves = 0; let picks = 0;
  const disposable = { dispose() {} };
  const uri = (fsPath) => ({ scheme: 'file', fsPath });
  const context = { extensionPath: path.join(root, 'vscode'), subscriptions: [] };
  const vscode = {
    Uri: { file: uri }, ProgressLocation: { Notification: 1 },
    env: { openExternal: async (value) => { opened.push(value.fsPath); return true; } },
    commands: { registerCommand: (name, callback) => { commands.set(name, callback); return disposable; } },
    window: {
      activeTextEditor: { document: { uri: uri(mainFile) } },
      createOutputChannel: () => ({ ...disposable, append: (s) => logs.push(s), appendLine: (s) => logs.push(s), show() {} }),
      withProgress: async (_options, task) => task(),
      showInformationMessage: async (s) => messages.push(s), showWarningMessage: async (s) => messages.push(s),
      showErrorMessage: async (s) => errors.push(s),
      showQuickPick: async (choices) => { picks += 1; return choices[0]; },
    },
    workspace: {
      isTrusted: true, workspaceFolders: [{ uri: uri(app) }],
      textDocuments: [{ uri: uri(manifest), isDirty: true, async save() { saves += 1; this.isDirty = false; return true; } }],
      findFiles: async () => [],
      createFileSystemWatcher: () => ({ ...disposable, onDidCreate: () => disposable, onDidChange: () => disposable, onDidDelete: () => disposable }),
    },
  };
  try {
    assert.equal(findProject(mainFile), app);
    assert.equal(findProject(path.join(directory, 'absent')), null);
    registerDependencies(vscode, context, { readConfig: () => ({ magelPath: compiler }), refresh: async () => { refreshed += 1; } });
    assert.equal(commands.size, 3);
    await commands.get('magel.installDependencies')();
    assert.deepEqual(errors, []); assert.equal(saves, 1); assert.equal(downloads, 1);
    assert.ok(fs.existsSync(path.join(app, 'magel.lock')));
    assert.ok(fs.existsSync(path.join(compilerDir, 'libraries', `values-${digest}`, 'values.mgl')));
    await new Promise((resolve) => setTimeout(resolve, 650)); assert.equal(refreshed, 1);
    await commands.get('magel.checkDependencies')(uri(manifest)); assert.equal(downloads, 1); assert.deepEqual(errors, []);
    await commands.get('magel.openLibraries')(); assert.equal(opened[0], path.join(compilerDir, 'libraries'));
    await new Promise((resolve) => server.close(resolve));
    let output = '';
    await runDependencyProcess(compiler, ['run', mainFile, 'out', path.join(directory, 'output')], app, (chunk) => { output += chunk; });
    assert.equal(output.replace(/\r\n/g, '\n'), '7\n');
    // 失败必须显示错误及日志，不能发出成功通知。
    fs.appendFileSync(path.join(compilerDir, 'libraries', `values-${digest}`, 'values.mgl'), '\n// changed\n');
    const notifications = messages.length;
    await commands.get('magel.checkDependencies')(); assert.equal(errors.length, 1); assert.equal(messages.length, notifications);
    assert.ok(logs.some((line) => line.includes('libraries')));
    const other = path.join(directory, 'other'); fs.mkdirSync(other); fs.writeFileSync(path.join(other, 'magel.toml'), '[package]\nname="other"\n');
    vscode.window.activeTextEditor = null; vscode.workspace.workspaceFolders.push({ uri: uri(other) });
    await commands.get('magel.openLibraries')(); assert.equal(picks, 1);
    vscode.workspace.isTrusted = false;
    const before = downloads; await commands.get('magel.installDependencies')(); assert.equal(downloads, before);
    console.log('VS Code 依赖集成通过：真实 HTTP 下载、离线执行、含空格路径、保存清单、右键目标、多工程选择、刷新与错误处理。');
  } finally {
    for (const subscription of context.subscriptions) subscription.dispose();
    if (server.listening) await new Promise((resolve) => server.close(resolve));
    fs.rmSync(directory, { recursive: true, force: true });
  }
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
