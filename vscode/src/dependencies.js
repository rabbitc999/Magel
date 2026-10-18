'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const locator = require('./locator');

/**
 * 从文件或目录向上定位最近的工程清单。
 * @param {string} candidate 文件或目录。
 * @returns {string|null} 工程目录。
 */
function findProject(candidate) {
  if (!candidate) return null;
  let directory = path.resolve(candidate);
  try { if (!fs.statSync(directory).isDirectory()) directory = path.dirname(directory); } catch { return null; }
  for (;;) {
    if (locator.isFile(path.join(directory, 'magel.toml'))) return directory;
    const parent = path.dirname(directory);
    if (parent === directory) return null;
    directory = parent;
  }
}

/**
 * 用独立 argv 运行依赖命令，将输出实时转交面板，不经过 shell。
 * @param {string} compiler 编译器路径。
 * @param {string[]} args 命令参数。
 * @param {string} cwd 工程目录。
 * @param {(text:string)=>void} append 输出回调。
 * @returns {Promise<void>} 成功退出时完成，失败时提供退出码。
 */
function runDependencyProcess(compiler, args, cwd, append) {
  return new Promise((resolve, reject) => {
    const child = spawn(compiler, args, { cwd, windowsHide: true, shell: false, stdio: ['ignore', 'pipe', 'pipe'] });
    child.stdout.setEncoding('utf8'); child.stderr.setEncoding('utf8');
    child.stdout.on('data', append); child.stderr.on('data', append);
    child.once('error', reject);
    child.once('close', (code) => {
      if (code === 0) resolve();
      else reject(new Error(`依赖命令退出码：${code}`));
    });
  });
}

/**
 * 注册在线依赖管理命令与锁文件变更监听。
 * @param {object} vscode 编辑器 API，可由测试替换。
 * @param {object} context 扩展上下文。
 * @param {object} deps 编译器配置及语言服务器刷新回调。
 * @returns {void} 无。
 */
function registerDependencies(vscode, context, deps) {
  const output = vscode.window.createOutputChannel('Magel 依赖');
  let running = false;
  let timer = null;
  let disposed = false;
  const refresh = () => {
    if (disposed) return;
    if (timer) clearTimeout(timer);
    timer = setTimeout(() => {
      timer = null;
      if (disposed) return;
      if (running) { refresh(); return; }
      Promise.resolve(deps.refresh()).catch((error) => output.appendLine(error.message));
    }, 500);
  };
  context.subscriptions.push(output, { dispose() { disposed = true; if (timer) clearTimeout(timer); } });

  /** @brief 优先使用显式右键目标，再使用当前文件，最后选择工作区工程。 */
  async function selectProject(uri) {
    const active = vscode.window.activeTextEditor;
    const selected = uri || (active && active.document.uri);
    if (selected && selected.scheme === 'file') {
      const nearest = findProject(selected.fsPath);
      if (nearest) return nearest;
      if (uri) throw new Error('所选位置没有 magel.toml');
    }
    const choices = new Set();
    for (const folder of vscode.workspace.workspaceFolders || []) {
      const nearest = findProject(folder.uri.fsPath);
      if (nearest) choices.add(nearest);
    }
    for (const file of await vscode.workspace.findFiles('**/magel.toml', '**/{.git,.magel,libraries,bin,build,node_modules}/**')) {
      choices.add(path.dirname(file.fsPath));
    }
    if (!choices.size) throw new Error('请打开包含 magel.toml 的 Magel 工程');
    if (choices.size === 1) return [...choices][0];
    const picked = await vscode.window.showQuickPick([...choices].sort().map((directory) => ({
      label: path.basename(directory), description: directory, directory,
    })), { placeHolder: '选择需要管理依赖的 Magel 工程' });
    return picked && picked.directory;
  }

  /** @brief 根据目标工程定位编译器，保持依赖缓存与该编译器的 libraries 一致。 */
  function compilerFor(project) {
    const resolved = locator.resolveMagel(project, deps.readConfig(), context.extensionPath);
    if (!resolved.magel) throw new Error('未找到 Magel 编译器，请设置 magel.path');
    return resolved.magel;
  }

  /** @brief 执行显式下载或离线校验，成功后刷新语言服务与打开文档的诊断。 */
  async function execute(action, uri) {
    if (vscode.workspace.isTrusted === false) {
      await vscode.window.showWarningMessage('Magel：依赖命令需要在受信任的工作区中执行。');
      return;
    }
    if (running) {
      await vscode.window.showInformationMessage('Magel：已有依赖操作正在运行，请等待完成。');
      return;
    }
    running = true;
    try {
      const project = await selectProject(uri);
      if (!project) return;
      const manifest = path.join(project, 'magel.toml');
      const dirty = vscode.workspace.textDocuments.find((document) => document.uri.scheme === 'file'
        && path.resolve(document.uri.fsPath) === manifest && document.isDirty);
      if (dirty && !(await dirty.save())) throw new Error('magel.toml 保存失败，依赖操作已取消');
      const compiler = compilerFor(project);
      const args = ['deps', action, '--project', project, ...(action === 'check' ? ['--offline'] : [])];
      output.show(true);
      output.appendLine(`编译器：${compiler}\n工程：${project}\n缓存：${path.join(path.dirname(compiler), 'libraries')}`);
      await vscode.window.withProgress({ location: vscode.ProgressLocation.Notification,
        title: action === 'lock' ? 'Magel：下载并锁定模块依赖' : 'Magel：离线校验模块依赖', cancellable: false },
      () => runDependencyProcess(compiler, args, project, (text) => output.append(text)));
      refresh();
      await vscode.window.showInformationMessage(action === 'lock' ? 'Magel：模块依赖已下载并锁定。' : 'Magel：模块依赖离线校验通过。');
    } catch (error) {
      output.appendLine(error.message); output.show(true);
      await vscode.window.showErrorMessage(`Magel：${error.message}，详细信息见“Magel 依赖”输出。`);
    } finally { running = false; }
  }

  context.subscriptions.push(
    vscode.commands.registerCommand('magel.installDependencies', (uri) => execute('lock', uri)),
    vscode.commands.registerCommand('magel.checkDependencies', (uri) => execute('check', uri)),
    vscode.commands.registerCommand('magel.openLibraries', async (uri) => {
      try {
        const project = await selectProject(uri); if (!project) return;
        const libraries = path.join(path.dirname(compilerFor(project)), 'libraries');
        if (!fs.existsSync(libraries)) {
          await vscode.window.showInformationMessage('Magel：缓存目录尚未创建，请先下载模块依赖。'); return;
        }
        await vscode.env.openExternal(vscode.Uri.file(libraries));
      } catch (error) { await vscode.window.showErrorMessage(`Magel：${error.message}`); }
    }),
  );
  const watcher = vscode.workspace.createFileSystemWatcher('**/{magel.toml,magel.lock}');
  context.subscriptions.push(watcher, watcher.onDidCreate(refresh), watcher.onDidChange(refresh), watcher.onDidDelete(refresh));
}

module.exports = { findProject, runDependencyProcess, registerDependencies };
