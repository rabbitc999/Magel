'use strict';

/**
 * VS Code 插件入口：把编辑器接到 Magel 自带的语言服务器 `magel lsp`。
 *
 * 设计取舍：
 *   - 不依赖任何 npm 包（`vscode` 由宿主提供），不需要构建步骤，也不需要联网；
 *   - 语言服务器就是编译器本体（`magel lsp`，原生 C++ 实现），
 *     所以只用**一个**可执行文件：不需要另外编 lsp-server；开发命令不需要 Python；
 *   - 语言能力全在 `src/providers.js`，协议在 `src/lspClient.js`，定位在 `src/locator.js`；
 *     后两个不引用 `vscode`，可以在纯 Node 下单独测试（见 `test/smoke.js`）。
 *
 * 与根目录 `vscode/`（GramEditor，管 `.gram` / `.metro`）是**两个插件**：
 * `.mgl` 归这里，magel 目录搬家时插件跟着走。
 */

const vscode = require('vscode');
const path = require('node:path');

const { LspClient } = require('./src/lspClient');
const locator = require('./src/locator');
const runner = require('./src/runner');
const { registerDependencies } = require('./src/dependencies');
const { registerMacroPreview } = require('./src/macros');
const providers = require('./src/providers');
const { registerDebugger } = require('./src/debugger');

/** @type {LspClient|null} 当前的 `magel lsp` 客户端。 */
let client = null;

/** 上一次「运行」开的终端（同一时刻只留一个）。 */
let runTerminal = null;

/** @type {vscode.DiagnosticCollection|null} 诊断集合。 */
let diagnosticCollection = null;

/** @type {vscode.StatusBarItem|null} 状态栏上的服务器状态。 */
let statusItem = null;

/** @type {vscode.OutputChannel|null} 输出面板（协议日志与定位结果）。 */
let outputChannel = null;

/** @type {Map<string, number>} 文档 URI → 已同步的版本号。 */
const documentVersions = new Map();

/** @type {Map<string, NodeJS.Timeout>} 文档 URI → 防抖计时器。 */

/**
 * 插件激活：建面板、注册能力与命令、启动服务器。
 *
 * @param {vscode.ExtensionContext} context 扩展上下文。
 */
function activate(context) {
  outputChannel = vscode.window.createOutputChannel('Magel');
  diagnosticCollection = vscode.languages.createDiagnosticCollection(providers.LANGUAGE_ID);
  statusItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 90);
  statusItem.command = 'magel.restartServer';
  statusItem.tooltip = '点击重启 Magel 语言服务器';
  statusItem.show();
  setStatus('启动中…');

  context.subscriptions.push(outputChannel, diagnosticCollection, statusItem);

  // 1. 语言能力：诊断由服务器推送，其余按需请求。
  providers.registerProviders(context, { getClient: () => client, logLine });

  // 2. 文档同步。
  registerDocumentSync(context);

  // 3. 命令。
  registerCommands(context);
  registerDependencies(vscode, context, { readConfig, refresh: () => startServer(context) });
  registerMacroPreview(vscode, context, { getClient: () => client, flushDocument: notifyChange });

  // 3.5 断点调试：把编译器当 DAP 适配器起起来（`magel --dap <文件>`）。
  registerDebugger(context, { logLine, workspaceDir, readConfig });

  // 4. 设置变化时重启服务器（编译器路径或协议跟踪开关可能变了）。
  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((event) => {
      if (
        event.affectsConfiguration('magel.path') ||
        event.affectsConfiguration('magel.traceServer')
      ) {
        void startServer(context);
      }
    }),
  );

  // 5. 真正启动。
  void startServer(context);
}

/**
 * 插件停用：关掉服务器。
 */
function deactivate() {
  void stopServer();
}

// ---------------------------------------------------------------------------
//  设置与状态
// ---------------------------------------------------------------------------

/**
 * 读插件设置。
 *
 * @returns {{magelPath: string, traceServer: string}} 设置快照。
 */
function readConfig() {
  const config = vscode.workspace.getConfiguration('magel');
  return {
    magelPath: config.get('path', ''),
    traceServer: config.get('traceServer', 'off'),
  };
}

/**
 * 当前工作区目录（多根工作区取第一个）。
 *
 * @returns {string} 目录；没有工作区时返回空串。
 */
function workspaceDir() {
  const folders = vscode.workspace.workspaceFolders;
  return folders && folders.length > 0 ? folders[0].uri.fsPath : '';
}

/**
 * 写一行日志到输出面板。
 *
 * @param {string} text 文本。
 */
function logLine(text) {
  if (outputChannel) outputChannel.appendLine(text);
}

/**
 * 更新状态栏。
 *
 * @param {string} text 状态文字。
 */
function setStatus(text) {
  if (!statusItem) return;
  statusItem.text = `Magel: ${text}`;
}

// ---------------------------------------------------------------------------
//  语言服务器
// ---------------------------------------------------------------------------

/**
 * 运行当前 `.mgl`：先存盘（编译器读的是磁盘上的文件），再在集成终端里跑 `magel run <文件>`。
 *
 * @param {vscode.ExtensionContext} context 扩展上下文。
 * @returns {Promise<void>} 完成后返回。
 */
async function runCurrentFile(context) {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== providers.LANGUAGE_ID) {
    void vscode.window.showWarningMessage('Magel：请先打开一个 .mgl 文件再运行。');
    return;
  }
  const resolved = locator.resolveMagel(workspaceDir(), readConfig(), context.extensionPath);
  if (!resolved.magel) {
    logLine(`运行失败：没找到 magel 编译器（${resolved.source}）`);
    void vscode.window.showWarningMessage('Magel：没找到编译器，无法运行。', '打开设置')
      .then((choice) => {
        if (choice === '打开设置') {
          void vscode.commands.executeCommand('workbench.action.openSettings', 'magel.path');
        }
      });
    return;
  }
  if (editor.document.isDirty && !(await editor.document.save())) {
    logLine('运行取消：文件保存失败。');
    return;
  }

  const file = editor.document.uri.fsPath;
  const command = runner.buildRunCommand(resolved.magel, file);
  // 终端带着 cwd 建：命令里就不用 `cd`，Windows PowerShell / cmd / POSIX shell 都一样。
  if (runTerminal) {
    runTerminal.dispose();
    runTerminal = null;
  }
  runTerminal = vscode.window.createTerminal({ name: 'Magel 运行', cwd: path.dirname(file) });
  runTerminal.show(true);
  runTerminal.sendText(command, true);
  logLine(`运行：${command}（工作目录 ${path.dirname(file)}）`);
}

/**
 * 启动（或重启）`magel lsp`。
 *
 * @param {vscode.ExtensionContext} context 扩展上下文。
 * @returns {Promise<void>} 完成后返回。
 */
async function startServer(context) {
  await stopServer();
  setStatus('启动中…');

  // 1. 定位编译器。
  const config = readConfig();
  const resolved = locator.resolveMagel(workspaceDir(), config, context.extensionPath);
  if (!resolved.magel) {
    setStatus('未找到 magel');
    logLine(`没找到 magel 编译器：${resolved.source}`);
    logLine('（插件包自带 bin/magel；也可以在设置里指定 magel.path，或在项目里先 make）');
    const choice = await vscode.window.showWarningMessage(
      'Magel：没找到 magel 编译器，诊断 / 补全等功能暂不可用。',
      '打开设置',
    );
    if (choice === '打开设置') {
      void vscode.commands.executeCommand('workbench.action.openSettings', 'magel.path');
    }
    return;
  }
  logLine(`magel：${resolved.magel}（来源：${resolved.source}）`);

  // 2. 起进程并握手（`magel lsp` 走 stdio，不需要额外参数）。
  const instance = new LspClient(resolved.magel, ['lsp'], {
    cwd: workspaceDir() || path.dirname(resolved.magel),
    workspaceFolders: (vscode.workspace.workspaceFolders || []).map((folder) => ({ uri: folder.uri.toString(), name: folder.name })),
  });
  instance.on('diagnostics', (event) => publishDiagnostics(event));
  instance.on('log', (line) => {
    if (readConfig().traceServer !== 'off') logLine(`[server] ${line}`);
  });
  instance.on('exit', (code) => {
    setStatus(`已退出（${code}）`);
    client = null;
  });

  try {
    await instance.start();
    await instance.initialize();
    client = instance;
    setStatus('运行中');
  } catch (error) {
    setStatus('启动失败');
    logLine(`启动失败：${error.message}`);
    try {
      await instance.stop();
    } catch {
      // 起不来就没什么可关的。
    }
    return;
  }

  // 3. 已经打开的 `.mgl` 文档补一次 didOpen，让诊断立刻出来。
  for (const document of vscode.workspace.textDocuments) {
    if (document.languageId === providers.LANGUAGE_ID) notifyOpen(document);
  }
  void context;
}

/**
 * 关掉语言服务器并清掉文档版本记录。
 *
 * @returns {Promise<void>} 完成后返回。
 */
async function stopServer() {
  documentVersions.clear();

  const instance = client;
  client = null;
  if (!instance) return;
  try {
    await instance.stop();
  } catch {
    // 退出期的异常忽略。
  }
}

/**
 * 把服务器推来的诊断写进诊断集合。
 *
 * @param {{uri: string, diagnostics: Array<any>}} event 推送事件。
 */
function publishDiagnostics(event) {
  if (!diagnosticCollection) return;
  if (!documentVersions.has(event.uri)) return;
  if (event.version !== undefined && event.version !== documentVersions.get(event.uri)) return;
  diagnosticCollection.set(
    vscode.Uri.parse(event.uri),
    providers.toDiagnostics(event.diagnostics),
  );
}

// ---------------------------------------------------------------------------
//  文档同步
// ---------------------------------------------------------------------------

/**
 * 通知服务器：文档打开了。
 *
 * @param {vscode.TextDocument} document 文档。
 */
function notifyOpen(document) {
  if (!client || !client.running || document.uri.scheme === 'magel-expanded') return;
  const version = typeof document.version === 'number' ? document.version : 1;
  documentVersions.set(document.uri.toString(), version);
  client.notify('textDocument/didOpen', {
    textDocument: {
      uri: document.uri.toString(),
      languageId: document.languageId,
      version,
      text: document.getText(),
    },
  });
}

/**
 * 通知服务器：文档内容变了，按 UTF-16 范围增量同步。
 *
 * @param {vscode.TextDocument} document 文档。
 * @param {Array<vscode.TextDocumentContentChangeEvent>} changes 编辑范围；未提供时发送全文。
 */
function notifyChange(document, changes) {
  if (!client || !client.running) return;
  const key = document.uri.toString();
  const previous = documentVersions.get(key);
  const version = typeof document.version === 'number' ? document.version : (previous || 1) + 1;
  if (previous === version) return;
  documentVersions.set(key, version);
  client.notify('textDocument/didChange', {
    textDocument: { uri: key, version },
    contentChanges: changes && changes.length ? changes.map(change => ({
      range: { start: { line: change.range.start.line, character: change.range.start.character },
        end: { line: change.range.end.line, character: change.range.end.character } }, text: change.text,
    })) : [{ text: document.getText() }],
  });
}

/**
 * 注册文档事件：打开 / 增量修改 / 保存 / 关闭。
 *
 * @param {vscode.ExtensionContext} context 扩展上下文。
 */
function registerDocumentSync(context) {
  if (vscode.workspace.createFileSystemWatcher) {
    const watcher = vscode.workspace.createFileSystemWatcher('**/*.{mgl,inc,toml,lock}');
    const changed = (uri, type) => {
      if (client && client.running) client.notify('workspace/didChangeWatchedFiles', { changes: [{ uri: uri.toString(), type }] });
    };
    context.subscriptions.push(watcher, watcher.onDidCreate(uri => changed(uri, 1)),
      watcher.onDidChange(uri => changed(uri, 2)), watcher.onDidDelete(uri => changed(uri, 3)));
  }
  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument((document) => {
      if (document.languageId === providers.LANGUAGE_ID) notifyOpen(document);
    }),
  );

  context.subscriptions.push(
    vscode.workspace.onDidChangeTextDocument((event) => {
      const document = event.document;
      if (document.languageId !== providers.LANGUAGE_ID) return;
      if (event.contentChanges.length) notifyChange(document, event.contentChanges);
    }),
  );

  context.subscriptions.push(
    vscode.workspace.onDidSaveTextDocument((document) => {
      if (document.languageId === providers.LANGUAGE_ID) notifyChange(document);
      if (document.languageId === providers.LANGUAGE_ID && client && client.running)
        client.notify('textDocument/didSave', { textDocument: { uri: document.uri.toString() } });
    }),
  );

  context.subscriptions.push(
    vscode.workspace.onDidCloseTextDocument((document) => {
      if (document.languageId !== providers.LANGUAGE_ID) return;
      const key = document.uri.toString();
      documentVersions.delete(key);
      if (client && client.running) client.notify('textDocument/didClose', {
        textDocument: { uri: key },
      });
      if (diagnosticCollection) diagnosticCollection.delete(document.uri);
    }),
  );
}

// ---------------------------------------------------------------------------
//  命令
// ---------------------------------------------------------------------------

/**
 * 注册命令。
 *
 * @param {vscode.ExtensionContext} context 扩展上下文。
 */
function registerCommands(context) {
  context.subscriptions.push(
    vscode.commands.registerCommand('magel.restartServer', () => startServer(context)),
  );
  context.subscriptions.push(
    vscode.commands.registerCommand('magel.showServerLog', () => {
      if (outputChannel) outputChannel.show(true);
    }),
  );
  context.subscriptions.push(
    vscode.commands.registerCommand('magel.openDoc', () => openDoc()),
  );
  context.subscriptions.push(
    vscode.commands.registerCommand('magel.run', () => runCurrentFile(context)),
  );
}

/**
 * 打开随仓库的 Magel 语言文档。
 *
 * @returns {Promise<void>} 完成后返回。
 */
async function openDoc() {
  const dir = workspaceDir();
  const candidates = dir
    ? [path.join(dir, 'magel', 'doc', '02-language-tour.md'), path.join(dir, 'doc', '02-language-tour.md')]
    : [];
  for (const file of candidates) {
    if (locator.isFile(file)) {
      const document = await vscode.workspace.openTextDocument(vscode.Uri.file(file));
      await vscode.window.showTextDocument(document);
      return;
    }
  }
  void vscode.window.showInformationMessage(
    '没找到 Magel 语言文档（magel/doc/02-language-tour.md）。',
  );
}

module.exports = { activate, deactivate };
